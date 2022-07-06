#ifndef __DBG_H__
#define __DBG_H__

extern Print &DBG, &ERR;

class Quiet: public Print {
public:
	size_t write(uint8_t) { return 0; }
};

#endif
