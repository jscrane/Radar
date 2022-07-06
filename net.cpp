#include <WiFi.h>
#include <TinyXML.h>

#include "net.h"
#include "dbg.h"

static const char *web_host = "m.met.ie";

static bool find_data(WiFiClient &client) {
	unsigned long now = millis();
	while (!client.available())
		if (millis() - now > 5000) {
			ERR.println(F("find_data: timeout!"));
			return false;
		}
	client.find("\r\n\r\n");
	return true;
}

bool update_index(TinyXML &xml) {
	WiFiClient client;
	
	DBG.println(F("update_index"));

	if (!client.connect(web_host, 80)) {
		ERR.println(F("update_index: failed to connect!"));
		return false;
	}

	client.print(F("GET /weathermaps/radar2/radar4_app.xml HTTP/1.1\r\n"));
	client.print(F("Host: "));
	client.print(web_host);
	client.print(F("\r\nConnection: close\r\n\r\n"));
	client.flush();

	if (!client.connected() || !find_data(client))
		return false;

	xml.reset();
	while (client.available()) {
		int c = client.read();
		if (c >= 0)
			xml.processChar(c);
	}
	return true;
}

bool update_image(const char *image, uint8_t *buf, unsigned buflen) {
	WiFiClient client;

	DBG.println(F("update_image"));

	if (!client.connect(web_host, 80)) {
		ERR.println(F("update_image: failed to connect!"));
		return false;
	}

	client.print(F("GET /weathermaps/radar2/"));
	client.print(image);
	client.print(F(" HTTP/1.1\r\n"));
	client.print(F("Host: "));
	client.print(web_host);
	client.print(F("\r\nConnection: close\r\n\r\n"));
	client.flush();

	if (!client.connected() || !find_data(client))
		return false;

	for (int i = 0; i < buflen && client.available(); i++)
		buf[i] = client.read();
	return true;
}
