/* Shim for the copied ZSWatch voice memo UI only (see ../../SOURCE.md). ZSWatch draws light text on a dark round
 * watch face; here it is ink on paper, in grays only, with our CJK faces following the system text size. */
#pragma once
#include <lvgl.h>
#include "ink/sys/i18n.h"
#include "ink/ui_theme.h"

/* The list screen, its relative times and its delete dialog are not used: a voice memo is a row of the memo list. */
#define VOICE_MEMO_UI_LIST 0

static inline lv_color_t zsw_color_red(void) { return ink_ui_ink(); }
static inline lv_color_t zsw_color_blue(void) { return ink_ui_ink(); }
static inline lv_color_t zsw_color_gray(void) { return ink_ui_secondary(); }
static inline lv_color_t zsw_color_dark_gray(void) { return ink_ui_separator(); }
static inline lv_color_t zsw_color_bg(void) { return ink_ui_paper(); }

/* White text was for the dark face; on paper it is ink (lvgl.h is already included, so only this file sees it). */
#define lv_color_white() ink_ui_ink()

/* Montserrat sizes to the theme's text tokens. */
#define lv_font_montserrat_10 (*ink_ui_font_px(INK_FONT_NOTE, INK_WEIGHT_REGULAR))
#define lv_font_montserrat_12 (*ink_ui_font_px(INK_FONT_NOTE, INK_WEIGHT_REGULAR))
#define lv_font_montserrat_14 (*ink_ui_font_px(INK_FONT_LIST, INK_WEIGHT_REGULAR))
#define lv_font_montserrat_16 (*ink_ui_font_px(INK_FONT_SUBTITLE, INK_WEIGHT_MEDIUM))
#define lv_font_montserrat_20 (*ink_ui_font_px(INK_FONT_BODY, INK_WEIGHT_MEDIUM))
#define lv_font_montserrat_28 (*ink_ui_font_px(INK_FONT_TITLE, INK_WEIGHT_BOLD))
