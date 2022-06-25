#ifndef __CONFIGURATION_H__
#define __CONFIGURATION_H__

class Configuration {
public:
	bool read_file(const char *filename);

protected:	
	virtual void configure(class JsonDocument &doc) = 0;
};

class config: public Configuration {
public:
	char ssid[33];
	char password[33];
	char hostname[17];
	uint8_t rotate;

	void configure(class JsonDocument &doc);
};

extern config cfg;

#endif
