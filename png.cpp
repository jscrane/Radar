#include <Arduino.h>
#include <pngle.h>
#include <TFT_eSPI.h>

#include "png.h"
#include "dbg.h"

/*
PNG png;

static void png_draw_fast(PNGDRAW *draw) {

	uint16_t pixels[320];
	png.getLineAsRGB565(draw, pixels, PNG_RGB565_BIG_ENDIAN, 0xffffffff);
	TFT_eSPI *display = (TFT_eSPI *)draw->pUser;

	display->setAddrWindow(0, draw->y, png.getWidth(), png.getHeight());
	display->pushPixels(pixels, png.getHeight());
}

static void png_draw_transparent(PNGDRAW *draw) {

	uint16_t pixels[320];
	png.getLineAsRGB565(draw, pixels, PNG_RGB565_BIG_ENDIAN, 0xffffffff);
	TFT_eSPI *display = (TFT_eSPI *)draw->pUser;

	for (int i = 0; i < draw->iWidth; i++)
		if (pixels[i] != TFT_BLACK)
			display->drawPixel(i, draw->y, pixels[i]);
}

static void draw_image(uint8_t *buf, int len, TFT_eSPI *display, PNG_DRAW_CALLBACK drawfn) {

	int rc = png.openRAM(buf, len, drawfn);
	if (rc == PNG_SUCCESS) {
		DBG(printf("image specs: (%d x %d), %d bpp, pixel type: %d\r\n", png.getWidth(), png.getHeight(), png.getBpp(), png.getPixelType()));

		display->startWrite();
		png.decode(display, 0);
		display->endWrite();

		png.close();
	}
}
*/
static TFT_eSPI *tft;

void pngle_on_draw(pngle_t *pngle, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint8_t rgba[4])
{
	tft->drawPixel(x, y, tft->color565(rgba[0], rgba[1], rgba[2]));
}

void pngle_on_draw_alpha(pngle_t *pngle, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint8_t rgba[4])
{
	if (rgba[3]) {
		uint16_t fgc = tft->color565(rgba[0], rgba[1], rgba[2]);
		uint16_t bgc = tft->readPixel(x, y);
		tft->drawPixel(x, y, tft->alphaBlend(rgba[3], fgc, bgc));
	}
}

static void draw(uint8_t *buf, int len, TFT_eSPI *display, pngle_draw_callback_t cb) {

	tft = display;

	pngle_t *pngle = pngle_new();
	pngle_set_draw_callback(pngle, cb);

	for (int n = len; n > 0; ) {
		int fed = pngle_feed(pngle, buf, n);
		if (fed < 0) {
			ERR(printf("draw: %s\r\n", pngle_error(pngle)));
			break;
		}
		n -= fed;
	}
	pngle_destroy(pngle);
}

void draw_background(uint8_t *buf, int len, TFT_eSPI *display) {
	draw(buf, len, display, pngle_on_draw);
}

void draw_foreground(uint8_t *buf, int len, TFT_eSPI *display) {
	draw(buf, len, display, pngle_on_draw_alpha);
}
