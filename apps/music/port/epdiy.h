/* The two epdiy types the page and Mogo's TrueType engine share (same layout as apps/mogo/port/epdiy.h). */
#pragma once
#include <stdint.h>
typedef struct { int x, y, width, height; } EpdRect;
enum EpdFontFlags { EPD_DRAW_ALIGN_LEFT, EPD_DRAW_ALIGN_CENTER, EPD_DRAW_ALIGN_RIGHT };
void epd_draw_pixel(int x, int y, uint8_t gray, uint8_t *fb);
