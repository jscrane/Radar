#ifndef __PNG_H__
#define __PNG_H__

void png_draw_fast(PNGDRAW *draw);
void png_draw_transparent(PNGDRAW *draw);
void draw_image(uint8_t *buf, int len, TFT_eSPI *display, PNG_DRAW_CALLBACK drawfn);

#endif
