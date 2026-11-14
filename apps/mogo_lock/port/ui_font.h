/* Mogo's built-in UI face is not on this device: book_cover_auto.c draws its words through these, which use the
 * card font (ttf) and write the cover template's own 4-bit canvas (see lock_port.c). */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <epdiy.h>
bool ui_font_has_text(const char *text);
int ui_font_text_width_px(int px, const char *text);
int ui_font_ascender_px(int px);
void ui_font_draw_text_px(uint8_t *fb, int x, int y, int px, const char *text, enum EpdFontFlags align, uint8_t fg, uint8_t bg, bool bold);
