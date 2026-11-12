#pragma once
#include <epdiy.h>
#include <ttf_font.h>
#define UI_LOCK_WIDTH 684
#define UI_LOCK_HEIGHT 1216
#define UI_GRAY_WHITE 255
#define UI_GRAY_BLACK 0
#define UI_GRAY_LIGHT 144
#define UI_INK_BLACK 0
#define UI_INK_WHITE 15
void ui_clear_page(uint8_t *fb);
void ui_fill_round_rect(uint8_t *, EpdRect, int, uint8_t);
void ui_draw_round_rect(uint8_t *, EpdRect, int, uint8_t);
#define ui_draw_control_frame ui_draw_round_rect
void ui_fill_rect_fast(EpdRect, uint8_t, uint8_t *);
void ui_text_box(uint8_t *, EpdRect, int, const char *, enum EpdFontFlags, bool);
void ui_text_title_background_vc(uint8_t *, int, int, int, const char *, const char *, enum EpdFontFlags, uint8_t);
int ui_text_effective_px(int px);
#define ui_text_fixed_width_px ttf_text_width_px
int ui_text_title_fit(const char *, int, int, const char *);
EpdRect ui_inset_rect(EpdRect, int);
void ui_draw_button(uint8_t *, EpdRect, const char *, bool);
#define ui_nav_status(fb) ((void)0)
void ui_nav_back(uint8_t *, int, int);
void ui_text_fixed(uint8_t *, int, int, int, const char *, enum EpdFontFlags, bool);

static inline bool ui_rect_hit(EpdRect r,int x,int y) { return x>=r.x && y>=r.y && x<r.x+r.width && y<r.y+r.height; }
