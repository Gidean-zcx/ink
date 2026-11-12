#pragma once
#include <stdint.h>
enum EpdFontFlags { EPD_DRAW_ALIGN_LEFT, EPD_DRAW_ALIGN_CENTER, EPD_DRAW_ALIGN_RIGHT };
void epd_draw_pixel(int x, int y, uint8_t gray, uint8_t *pixels);
