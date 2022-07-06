#include <ArduinoJson.h>

#include <WiFi.h>
#include <ESPmDNS.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <Update.h>

#include <FS.h>
#include <SPIFFS.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <SimpleTimer.h>
#include <TinyXML.h>

#include "Configuration.h"
#include "dbg.h"
#include "png.h"
#include "net.h"

TFT_eSPI tft;
config cfg;
MDNSResponder mdns;
DNSServer dnsServer;
WebServer server(80);
SimpleTimer timers;
TinyXML xml;

Print &ERR = Serial;
#if defined(DEBUG)
Print &DBG = ERR;
#else
Print &DBG = Quiet();
#endif

bool connected, new_image;
uint8_t xmlbuf[150];
const char *config_file = "/config.json";
const char *ireland_file = "/ireland.png";

typedef struct radar_image {
	char day[3], hour[3], minute[3];
	char src[40];
} radar_image_t;

radar_image_t images[14];
uint8_t imgbuf[16384];
uint8_t ireland[8192];

void config::configure(JsonDocument &o) {
	strlcpy(ssid, o[F("ssid")] | "", sizeof(ssid));
	strlcpy(password, o[F("password")] | "", sizeof(password));
	strlcpy(hostname, o[F("hostname")] | "", sizeof(hostname));
	rotate = o[F("rotate")];
}

void xml_callback(uint8_t flags, char *tag, uint16_t tlen, char *data, uint16_t dlen) {
	static int curr = 0;

	DBG.printf("flags: %0x tag: %s ", flags, tag);
	if (flags & STATUS_ATTR_TEXT)
		DBG.printf("data: %s", data);
	DBG.println();

	if ((flags & STATUS_START_TAG) && !strncmp(tag, "/radar", tlen)) {
		curr = 0;
		return;
	}

	if (flags & STATUS_END_TAG) {
		curr++;
		return;
	}

	if (flags & STATUS_ATTR_TEXT) {
		radar_image_t &r = images[curr];
		if (!strncmp(tag, "day", tlen))
			strncpy(r.day, data, sizeof(r.day));
		else if (!strncmp(tag, "hour", tlen))
			strncpy(r.hour, data, sizeof(r.hour));
		else if (!strncmp(tag, "min", tlen))
			strncpy(r.minute, data, sizeof(r.minute));
		else if (!strncmp(tag, "src", tlen))
			strncpy(r.src, data, sizeof(r.src));
	}
}

void do_update_index() {

	new_image = update_index(xml);
	if (new_image)
		DBG.println(images[0].src);
}

void setup() {
	Serial.begin(115200);

	tft.init();
	tft.setTextColor(TFT_WHITE, TFT_BLACK);
	tft.setCursor(0, 0);

	tft.setRotation(3);
	tft.fillScreen(TFT_BLACK);
	tft.setSwapBytes(true);

	if (!SPIFFS.begin()) {
		ERR.println("SPIFFS failed");
		return;
	}

	if (!cfg.read_file(config_file)) {
		ERR.println(F("config!"));
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

	File f = SPIFFS.open(ireland_file, "r");
	if (!f) {
		ERR.print(F("failed to open: "));
		ERR.println(ireland_file);
		return;
	}
	f.read(ireland, sizeof(ireland));
	f.close();

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
		DBG.println(F("mDNS started"));
		mdns.addService("http", "tcp", 80);
	} else
		ERR.println(F("Error starting mDNS"));

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

	server.on("/update", HTTP_POST, []() {
		server.sendHeader("Connection", "close");
		if (Update.hasError())
			server.send(500, "text/plain", "FAIL");
		else
			server.send(200, "text/plain", "OK");
		ESP.restart();
	}, []() {
		HTTPUpload &upload = server.upload();
		if (upload.status == UPLOAD_FILE_START) {
			if (!Update.begin(UPDATE_SIZE_UNKNOWN))
				Update.printError(Serial);
		} else if (upload.status == UPLOAD_FILE_WRITE) {
			if (Update.write(upload.buf, upload.currentSize) != upload.currentSize)
				Update.printError(Serial);
		} else if (upload.status == UPLOAD_FILE_END) {
			if (!Update.end(true))
				Update.printError(Serial);
		}
	});

	server.serveStatic("/", SPIFFS, "/index.html");
	server.serveStatic("/config", SPIFFS, config_file);
	server.serveStatic("/js/transparency.min.js", SPIFFS, "/transparency.min.js");
	server.serveStatic("/info.png", SPIFFS, "/info.png");
	server.begin();

	if (connected) {
		DBG.println();
		DBG.print(F("Connected to "));
		DBG.println(cfg.ssid);
		DBG.println(WiFi.localIP());

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

	timers.setInterval(15 * 60 * 1000, do_update_index);

	do_update_index();
}

void loop() {

	server.handleClient();

	if (!connected) {
		dnsServer.processNextRequest();
		return;
	}

	timers.run();

	if (new_image) {
		new_image = false;
		if (update_image(images[0].src, imgbuf, sizeof(imgbuf))) {
			draw_background(ireland, sizeof(ireland));
			draw_foreground(imgbuf, sizeof(imgbuf));

			tft.setCursor(0, 0);
			tft.print(images[0].hour);
			tft.print(':');
			tft.print(images[0].minute);
		}
	}
}
