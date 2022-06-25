#include <ArduinoJson.h>

#include <WiFi.h>
#include <ESPmDNS.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <Update.h>

#include <FS.h>
#include <SPIFFS.h>
#include <PNGdec.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <SimpleTimer.h>
#include <TinyXML.h>

#include "Configuration.h"
#include "dbg.h"

PNG png;
TFT_eSPI tft;
config cfg;
MDNSResponder mdns;
DNSServer dnsServer;
WebServer server(80);
SimpleTimer timers;
TinyXML xml;

bool connected, debug = true, new_image;
uint8_t xmlbuf[150];
const char *config_file = "/config.json";
const char *web_host = "m.met.ie";
const char *web_root = "http://m.met.ie/weathermaps/radar2/";
const char *index_file = "radar4_app.xml";

typedef struct radar_image {
	int day, hour, minute;
	char src[40];
} radar_image_t;

radar_image_t images[14];
int curr = 0;
uint8_t imgbuf[16384];

void config::configure(JsonDocument &o) {
	strlcpy(ssid, o[F("ssid")] | "", sizeof(ssid));
	strlcpy(password, o[F("password")] | "", sizeof(password));
	strlcpy(hostname, o[F("hostname")] | "", sizeof(hostname));
	rotate = o[F("rotate")];
}

void xml_callback(uint8_t flags, char *tag, uint16_t tagLen, char *data, uint16_t dataLen) {
	DBG(printf("flags: %0x tag: %s ", flags, tag));
	if (flags & STATUS_ATTR_TEXT)
		DBG(printf("data: %s", data));
	DBG(println());

	if (flags & STATUS_END_TAG) {
		curr++;
		if (curr == sizeof(images) / sizeof(images[0]))
			curr = 0;
		return;
	}

	if (flags & STATUS_ATTR_TEXT) {
		radar_image_t *r = &images[curr];
		if (!strncmp(tag, "day", tagLen))
			r->day = atoi(data);
		else if (!strncmp(tag, "hour", tagLen))
			r->hour = atoi(data);
		else if (!strncmp(tag, "min", tagLen))
			r->minute = atoi(data);
		else if (!strncmp(tag, "src", tagLen))
			strncpy(r->src, data, dataLen);
	}
}

bool find_data(WiFiClient &client) {
	unsigned long now = millis();
	while (!client.available())
		if (millis() - now > 5000) {
			ERR(print(F("update_index: timeout!")));
			return false;
		}
	client.find("\r\n\r\n");
	return true;
}

void update_index() {
	WiFiClient client;
	
	DBG(println(F("update_index")));

	if (!client.connect(web_host, 80)) {
		ERR(print(F("update_index: failed to connect!")));
		return;
	}

	client.print(F("GET /weathermaps/radar2/radar4_app.xml HTTP/1.1\r\n"));
	client.print(F("Host: "));
	client.print(web_host);
	client.print(F("\r\nConnection: close\r\n\r\n"));
	client.flush();
	if (client.connected() && find_data(client)) {
		xml.reset();
		while (client.available()) {
			int c = client.read();
			if (c >= 0)
				xml.processChar(c);
		}
		DBG(println(images[0].src));
		new_image = true;
	}
}

void update_image() {
	WiFiClient client;

	DBG(println(F("update_image")));

	if (!client.connect(web_host, 80)) {
		ERR(print(F("update_image: failed to connect!")));
		return;
	}

	client.print(F("GET /weathermaps/radar2/"));
	client.print(images[0].src);
	client.print(F(" HTTP/1.1\r\n"));
	client.print(F("Host: "));
	client.print(web_host);
	client.print(F("\r\nConnection: close\r\n\r\n"));
	client.flush();

	if (client.connected() && find_data(client)) {
		for (int i = 0; i < sizeof(imgbuf) && client.available(); i++)
			imgbuf[i] = client.read();
	}
}

void setup() {
	Serial.begin(115200);

	tft.init();
	tft.setTextColor(TFT_WHITE, TFT_BLACK);
	tft.setCursor(0, 0);

	tft.setRotation(3);
	tft.fillScreen(TFT_BLACK);

	if (!SPIFFS.begin()) {
		tft.println("SPIFFS failed");
		return;
	}

	if (!cfg.read_file(config_file)) {
		ERR(print(F("config!")));
		return;
        }
	tft.setRotation(cfg.rotate);
	tft.println(F("Radar (c) 2022"));
	tft.print(F("ssid: "));
	tft.println(cfg.ssid);
	tft.print(F("password: "));
	tft.println(cfg.password);
	tft.print(F("hostname: "));
	tft.println(cfg.hostname);

	xml.init(xmlbuf, sizeof(xmlbuf), xml_callback);

	WiFi.mode(WIFI_STA);
	WiFi.hostname(cfg.hostname);
	if (*cfg.ssid) {
		WiFi.setAutoReconnect(true);
		WiFi.begin(cfg.ssid, cfg.password);
       		const char busy[] = "|/-\\";
		int16_t y = tft.getCursorY();
		for (int i = 0; i < 60 && WiFi.status() != WL_CONNECTED; i++) {
			delay(500);
			char c = busy[i % 4];
			tft.print(c);
			tft.setCursor(0, y);
		}
       		connected = WiFi.status() == WL_CONNECTED;
	}

	if (mdns.begin(cfg.hostname)) {
		DBG(println(F("mDNS started")));
		mdns.addService("http", "tcp", 80);
	} else
		ERR(println(F("Error starting mDNS")));

	server.on("/config", HTTP_POST, []() {
		if (server.hasArg("plain")) {
			String body = server.arg("plain");
			File f = SPIFFS.open(config_file, "w");
			f.print(body);
			f.close();
			server.send(200);
			WiFi.setAutoConnect(false);
			ESP.restart();
		} else
			server.send(400, "text/plain", "No body!");
	});
	server.serveStatic("/", SPIFFS, "/index.html");
	server.serveStatic("/config", SPIFFS, config_file);
	server.serveStatic("/js/transparency.min.js", SPIFFS, "/transparency.min.js");
	server.serveStatic("/info.png", SPIFFS, "/info.png");
	server.begin();

	if (connected) {
		DBG(println());
		DBG(print(F("Connected to ")));
		DBG(println(cfg.ssid));
		DBG(println(WiFi.localIP()));

		tft.println();
		tft.print(F("http://"));
		tft.print(WiFi.localIP());
		tft.println('/');
	} else {
		WiFi.mode(WIFI_AP);
		WiFi.softAP(cfg.hostname);
		tft.println(F("Connect to SSID"));
		tft.println(cfg.hostname);
		tft.println(F("to configure WiFi"));
		dnsServer.start(53, "*", WiFi.softAPIP());
	}

	timers.setInterval(15 * 60 * 1000, update_index);

	update_index();
}

void png_draw(PNGDRAW *draw) {

	uint16_t pixels[320];
	png.getLineAsRGB565(draw, pixels, PNG_RGB565_LITTLE_ENDIAN, 0xffffffff);

	tft.setAddrWindow(0, draw->y, png.getWidth(), png.getHeight());
	tft.pushPixels(pixels, png.getHeight());
}

void draw_image() {

	int rc = png.openRAM(imgbuf, sizeof(imgbuf), png_draw);
	if (rc == PNG_SUCCESS) {
		DBG(printf("image specs: (%d x %d), %d bpp, pixel type: %d\r\n", png.getWidth(), png.getHeight(), png.getBpp(), png.getPixelType()));

		tft.startWrite();
		png.decode(NULL, 0);
		tft.endWrite();

		png.close();
	}
}

void loop() {

	server.handleClient();
	if (!connected) {
		dnsServer.processNextRequest();
		return;
	}

	if (new_image) {
		new_image = false;
		update_image();
		draw_image();
	}
}
