/* apps/mogo/port/epdiy.h plus what the lock styles draw with. One portrait 684x1216 L8 canvas, no rotation. */
#pragma once
#include_next <epdiy.h>
enum EpdRotation { EPD_ROT_LANDSCAPE = 0, EPD_ROT_PORTRAIT = 1, EPD_ROT_INVERTED_LANDSCAPE = 2, EPD_ROT_INVERTED_PORTRAIT = 3 };
void epd_draw_rect(EpdRect r, uint8_t gray, uint8_t *fb);
enum EpdRotation epd_get_rotation(void);
int epd_width(void);
int epd_height(void);
int epd_rotated_display_width(void);
int epd_rotated_display_height(void);
