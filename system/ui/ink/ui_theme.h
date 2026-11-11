/* System UI spec section 1. Values are logical pixels on the Plus panel (297 dpi: 4.125 px to the point). */
#ifndef INK_UI_THEME_H
#define INK_UI_THEME_H

#include "ink/sys/ui.h"
#include "ink/ui_icon_names.h"

/* The design's numbers x 1.2, all of them together, so that its proportions stay (docs/product/legibility.md).
 * Only the keys are not: ten have to fit across the panel. */
enum {
    INK_CANVAS_W = 684, INK_CANVAS_H = 1216, /* Panel geometry, not control sizes. */
    INK_STATUS_BAR = 62,
    INK_FONT_STATUS = 29, INK_FONT_NOTE = 31, INK_FONT_LIST = 36,
    INK_FONT_BODY = 38, INK_FONT_SUBTITLE = 43, INK_FONT_TITLE = 53,
    INK_FONT_HEADING = 72, INK_FONT_NUMBER = 106, INK_FONT_CLOCK = 211,
    INK_LINE_STATUS = 38, INK_LINE_NOTE = 43, INK_LINE_LIST = 53,
    INK_LINE_BODY = 62, INK_LINE_SUBTITLE = 58, INK_LINE_TITLE = 72,
    INK_LINE_HEADING = 86, INK_LINE_NUMBER = 120, INK_LINE_CLOCK = 223,
    INK_ICON_SMALL = 34, INK_ICON_MEDIUM = 43, INK_ICON_LIST = 53,
    INK_ICON_APP = 77, INK_ICON_STATE = 106,
    INK_CIRCLE_SMALL = 77, INK_CIRCLE = 101, INK_CIRCLE_APP = 134, INK_CIRCLE_LARGE = 192,
    INK_CONTROL = 101, INK_PRIMARY = 125, INK_ROW_DOUBLE = 134,
    INK_FIELD_HEIGHT = 101, INK_FIELD_LINES = 3,
    INK_FIELD_PAD = 19, INK_FIELD_LABEL_GAP = 10, INK_FIELD_RADIUS = 29,
    INK_FIELD_CLEAR = 106, INK_FIELD_ICON = 34, INK_FIELD_SEARCH_ICON = 53,
    INK_FONT_FIELD = 38, INK_FONT_FIELD_LABEL = 31,
    INK_LINE_FIELD = 62, INK_LINE_FIELD_LABEL = 43,
    INK_FIELD_STROKE = 3, INK_FIELD_FOCUS_GAP = 5,
    INK_SEGMENT = 115, INK_SLIDER = 115, INK_DOCK = 149,
    INK_KEY = 96, INK_DIGIT = 163, INK_MARGIN = 38, INK_READING_MARGIN = 58,
    INK_GAP_XS = 5, INK_GAP_S = 10, INK_GAP_M = 14, INK_GAP = 19,
    INK_GAP_L = 29, INK_GAP_XL = 38, INK_GAP_XXL = 58, INK_BLOCK = 77, INK_BLOCK_L = 115,
    INK_RADIUS_KEY = 29, INK_RADIUS_CARD = 48, INK_RADIUS_PANEL = 67,
    INK_STROKE = 3 /* 2.5 x 1.2 */
};

typedef enum { INK_WEIGHT_REGULAR, INK_WEIGHT_MEDIUM, INK_WEIGHT_BOLD } ink_ui_weight_t;
typedef enum { INK_ICON_LINE, INK_ICON_SOLID } ink_ui_icon_look_t;
typedef enum { INK_SURFACE_WHITE, INK_SURFACE_STATIC, INK_SURFACE_BLACK } ink_ui_surface_t;

int ink_ui_px(int px);
int ink_ui_width(void);
int ink_ui_height(void);
lv_color_t ink_ui_paper(void);
lv_color_t ink_ui_ink(void);
lv_color_t ink_ui_secondary(void);
/* Surface kinds survive the static surface's white fill. USER_2/3 are reserved for them. */
ink_ui_surface_t ink_ui_surface_kind(const lv_obj_t *obj);
void ink_ui_surface_style(lv_obj_t *obj, ink_ui_surface_t surface, bool border);
const lv_font_t *ink_ui_font_px(int px, ink_ui_weight_t weight);
int ink_ui_line_height(int px);
/** Height of a row with a title and a line under it, at the current text size. */
int ink_ui_row_double(void);
/** What this text takes at the current text size, in canvas pixels. */
int ink_ui_text_width(const char *text, int px, ink_ui_weight_t weight);
int ink_ui_text_height(const char *text, int px, ink_ui_weight_t weight, int w);
/** Another text size for a pill. */
void ink_ui_pill_text(lv_obj_t *pill, int px, ink_ui_weight_t weight);
/** One line of text in a box of w, cut with dots when it does not fit. */
lv_obj_t *ink_ui_line(lv_obj_t *parent, const char *text, int px, ink_ui_weight_t weight, int x, int y, int w);

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
void ink_ui_group_divider(lv_obj_t *parent, int y, int w);
lv_obj_t *ink_ui_panel(lv_obj_t *parent, int height, bool modal);
lv_obj_t *ink_ui_pagebar(lv_obj_t *parent, int page, int total, int y, lv_event_cb_t cb);
lv_obj_t *ink_ui_notice(lv_obj_t *parent, const char *text, int y);
enum { INK_UI_DATETIME_PARTS = 5 };
void ink_ui_datetime_picker(lv_obj_t *parent, const int parts[INK_UI_DATETIME_PARTS],
                            void (*bind)(lv_obj_t *button, int part, int delta));
void ink_ui_busy_control(lv_obj_t *obj, bool busy);

/* A control owns its whole gesture, including a press in an edge band. */
void ink_ui_capture_touch(lv_obj_t *obj);
bool ink_ui_touch_captured(int x, int y);
void ink_ui_capture_forget(lv_obj_t *root);

/* Keyboard: shared key units and dimensions, scaled with the text line height. */
enum {
    INK_KB_KEY = 96, INK_KB_DIGIT = 163, INK_KB_CAND = 101, INK_KB_GAP = 5, INK_KB_PAD_Y = 10,
    INK_KB_UNIT = 2, INK_KB_HALF = 1, INK_KB_FUNCTION = 3, INK_KB_SPACE = 5,
    INK_KB_FUNCTION_WIDTH = 91
};

#endif
