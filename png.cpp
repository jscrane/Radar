#include <PNGdec.h>
#include <TFT_eSPI.h>

#include "png.h"
#include "dbg.h"

PNG png;

void png_draw_fast(PNGDRAW *draw) {

	uint16_t pixels[320];
	png.getLineAsRGB565(draw, pixels, PNG_RGB565_BIG_ENDIAN, 0xffffffff);
	TFT_eSPI *display = (TFT_eSPI *)draw->pUser;

	display->setAddrWindow(0, draw->y, png.getWidth(), png.getHeight());
	display->pushPixels(pixels, png.getHeight());
}

void png_draw_transparent(PNGDRAW *draw) {

	uint16_t pixels[320];
	png.getLineAsRGB565(draw, pixels, PNG_RGB565_BIG_ENDIAN, 0xffffffff);
	TFT_eSPI *display = (TFT_eSPI *)draw->pUser;

	for (int i = 0; i < draw->iWidth; i++)
		if (pixels[i] != TFT_BLACK)
			display->drawPixel(i, draw->y, pixels[i]);
}

void draw_image(uint8_t *buf, int len, TFT_eSPI *display, PNG_DRAW_CALLBACK drawfn) {

	int rc = png.openRAM(buf, len, drawfn);
	if (rc == PNG_SUCCESS) {
		DBG(printf("image specs: (%d x %d), %d bpp, pixel type: %d\r\n", png.getWidth(), png.getHeight(), png.getBpp(), png.getPixelType()));

		display->startWrite();
		png.decode(display, 0);
		display->endWrite();

		png.close();
	}
}
