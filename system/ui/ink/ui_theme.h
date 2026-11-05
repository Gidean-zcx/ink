/* System UI spec section 1. Values are logical pixels on the Plus panel. */
#ifndef INK_UI_THEME_H
#define INK_UI_THEME_H

#include "ink/sys/ui.h"

enum {
    INK_CANVAS_W = 684, INK_CANVAS_H = 1216, /* Panel geometry, not control sizes. */
    INK_FONT_STATUS = 24, INK_FONT_NOTE = 26, INK_FONT_LIST = 30,
    INK_FONT_BODY = 32, INK_FONT_SUBTITLE = 36, INK_FONT_TITLE = 44,
    INK_FONT_HEADING = 60, INK_FONT_NUMBER = 88, INK_FONT_CLOCK = 176,
    INK_LINE_STATUS = 32, INK_LINE_NOTE = 36, INK_LINE_LIST = 44,
    INK_LINE_BODY = 52, INK_LINE_SUBTITLE = 48, INK_LINE_TITLE = 60,
    INK_LINE_HEADING = 72, INK_LINE_NUMBER = 100, INK_LINE_CLOCK = 186,
    INK_ICON_SMALL = 28, INK_ICON_MEDIUM = 36, INK_ICON_LIST = 44,
    INK_ICON_APP = 64, INK_ICON_STATE = 88,
    INK_CIRCLE_SMALL = 64, INK_CIRCLE = 84, INK_CIRCLE_APP = 112, INK_CIRCLE_LARGE = 160,
    INK_CONTROL = 84, INK_PRIMARY = 104, INK_ROW_DOUBLE = 112,
    INK_SEGMENT = 96, INK_SLIDER = 96, INK_DOCK = 124,
    INK_KEY = 88, INK_DIGIT = 136, INK_MARGIN = 32, INK_READING_MARGIN = 48,
    INK_GAP_XS = 4, INK_GAP_S = 8, INK_GAP_M = 12, INK_GAP = 16,
    INK_GAP_L = 24, INK_GAP_XL = 32, INK_GAP_XXL = 48, INK_BLOCK = 64, INK_BLOCK_L = 96,
    INK_RADIUS_KEY = 24, INK_RADIUS_CARD = 40, INK_RADIUS_PANEL = 56,
    INK_STROKE = 3 /* 2.5 px rounded to LVGL's integer stroke. */
};

typedef enum { INK_WEIGHT_REGULAR, INK_WEIGHT_MEDIUM, INK_WEIGHT_BOLD } ink_ui_weight_t;
typedef enum { INK_ICON_LINE, INK_ICON_DUO, INK_ICON_SOLID } ink_ui_icon_look_t;
typedef enum { INK_SURFACE_WHITE, INK_SURFACE_STATIC, INK_SURFACE_BLACK } ink_ui_surface_t;

int ink_ui_px(int px);
int ink_ui_width(void);
int ink_ui_height(void);
lv_color_t ink_ui_paper(void);
lv_color_t ink_ui_ink(void);
lv_color_t ink_ui_gray(void);
lv_color_t ink_ui_secondary(void);
const lv_font_t *ink_ui_font_px(int px, ink_ui_weight_t weight);
int ink_ui_line_height(int px);

lv_obj_t *ink_ui_box(lv_obj_t *parent, int x, int y, int w, int h, int radius, ink_ui_surface_t surface, bool border);
lv_obj_t *ink_ui_label(lv_obj_t *parent, const char *text, int px, ink_ui_weight_t weight, int x, int y, int w);
lv_obj_t *ink_ui_icon(lv_obj_t *parent, const char *name, int px, ink_ui_icon_look_t look, int x, int y, bool white);
lv_obj_t *ink_ui_pill(lv_obj_t *parent, const char *text, int x, int y, int w, int h, bool primary,
                      lv_event_cb_t cb, void *ctx);
lv_obj_t *ink_ui_circle(lv_obj_t *parent, const char *icon, int x, int y, int diameter, int icon_px,
                        ink_ui_surface_t surface, lv_event_cb_t cb, void *ctx);
lv_obj_t *ink_ui_card(lv_obj_t *parent, int x, int y, int w, int h, ink_ui_surface_t surface);
lv_obj_t *ink_ui_toggle(lv_obj_t *parent, const char *title, bool on, int x, int y, int w, lv_event_cb_t cb, void *ctx);
lv_obj_t *ink_ui_slider(lv_obj_t *parent, const char *title, int value, int x, int y, int w, lv_event_cb_t cb, void *ctx);
lv_obj_t *ink_ui_segments(lv_obj_t *parent, const char *const *labels, int count, int selected,
                          int x, int y, int w, lv_event_cb_t cb, void *ctx);
lv_obj_t *ink_ui_list_row(lv_obj_t *parent, const char *icon, const char *title, const char *value,
                         int x, int y, int w, bool two_lines, lv_event_cb_t cb, void *ctx);
lv_obj_t *ink_ui_panel(lv_obj_t *parent, int height, bool modal);
lv_obj_t *ink_ui_pagebar(lv_obj_t *parent, int page, int total, int y, lv_event_cb_t cb);
lv_obj_t *ink_ui_notice(lv_obj_t *parent, const char *text, int y);
void ink_ui_busy_control(lv_obj_t *obj, bool busy);

/* A control owns its whole gesture, including a press in an edge band. */
void ink_ui_capture_touch(lv_obj_t *obj);
bool ink_ui_touch_captured(int x, int y);
void ink_ui_capture_forget(lv_obj_t *root);

#endif
