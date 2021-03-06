#include <Arduino.h>
#include <pngle.h>
#include <TFT_eSPI.h>

#include "png.h"
#include "dbg.h"

extern TFT_eSPI tft;

#define BUFLEN 320
static uint16_t lbuf[BUFLEN];
static int pc, width, height;

void pngle_on_init(pngle_t *pngle, uint32_t w, uint32_t h)
{
	pc = 0;
	width = w > BUFLEN? BUFLEN: w;
	height = h;
	DBG.printf("width %d height %d\r\n", width, height);
}

void pngle_on_draw(pngle_t *pngle, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint8_t rgba[4])
{
	lbuf[pc++] = tft.color565(rgba[0], rgba[1], rgba[2]);
	if (pc >= width) {
		tft.pushImage(0, y, pc, 1, lbuf);
		pc = 0;
	}
}

void pngle_on_draw_alpha(pngle_t *pngle, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint8_t rgba[4])
{
	static int sx;

	if (x == 0)
		tft.readRect(0, y, width, 1, lbuf);

	if (rgba[3]) {
		uint16_t fgc = tft.color565(rgba[0], rgba[1], rgba[2]);
		uint16_t bgc = lbuf[x];
		if (pc == 0)
			sx = x;
		lbuf[pc++] = tft.alphaBlend(rgba[3], fgc, bgc);
	}
	if (pc > 0 && (x == width - 1 || !rgba[3])) {
		tft.pushImage(sx, y, pc, 1, lbuf);
		pc = 0;
	}
}

static void draw(uint8_t *buf, int len, pngle_draw_callback_t cb) {

	pngle_t *pngle = pngle_new();
	pngle_set_init_callback(pngle, pngle_on_init);
	pngle_set_draw_callback(pngle, cb);

	for (int n = len; n > 0; ) {
		int fed = pngle_feed(pngle, buf, n);
		if (fed < 0) {
			ERR.printf("draw: %s\r\n", pngle_error(pngle));
			break;
		}
		n -= fed;
	}
	pngle_destroy(pngle);
}

void draw_background(uint8_t *buf, int len) {
	draw(buf, len, pngle_on_draw);
}

void draw_foreground(uint8_t *buf, int len) {
	draw(buf, len, pngle_on_draw_alpha);
}
