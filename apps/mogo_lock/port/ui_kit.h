/* apps/mogo/port/ui_kit.h plus Mogo's text, rule and icon calls the lock styles use. The lock files get their own
 * text calls (renamed below): every size goes through ink_mogo_scaled, so the words follow the system text size. */
#pragma once
#define ui_text ink_mogo_lock_text
#define ui_text_vc ink_mogo_lock_text_vc
#define ui_text_fixed ink_mogo_lock_text_fixed
#define ui_text_fixed_vc ink_mogo_lock_text_fixed_vc
#define ui_text_title_vc ink_mogo_lock_title_vc
#include_next <ui_kit.h>
#undef ui_text_fixed_width_px
#define ui_text_fixed_width_px ink_mogo_lock_width
#include <stdbool.h>
typedef enum { UI_ICON_BOOK_OPEN, UI_ICON_REFRESH_CW } ui_icon_t;
int ink_mogo_lock_width(int px, const char *text);
void ink_mogo_lock_text(uint8_t *fb, int x, int y_top, int px, const char *text, enum EpdFontFlags align, bool inverted);
void ink_mogo_lock_text_vc(uint8_t *fb, int x, int center_y, int px, const char *text, enum EpdFontFlags align, bool inverted);
void ink_mogo_lock_text_fixed(uint8_t *fb, int x, int y_top, int px, const char *text, enum EpdFontFlags align, bool inverted);
void ink_mogo_lock_text_fixed_vc(uint8_t *fb, int x, int center_y, int px, const char *text, enum EpdFontFlags align, bool inverted);
void ink_mogo_lock_title_vc(uint8_t *fb, int x, int center_y, int px, const char *text, const char *sample, enum EpdFontFlags align);
uint8_t ui_contrast_gray(uint8_t gray);
void ui_hairline(uint8_t *fb, int y, int x, int w, uint8_t color);
void ui_draw_separator(uint8_t *fb, int y, int x, int width, uint8_t color);
void ui_draw_icon(uint8_t *fb, int cx, int cy, int size, ui_icon_t icon, uint8_t gray);
void ui_nav_battery_draw(uint8_t *fb, int x, int cy);
