/*
 * LVGL configuration for ink (all platforms). Only overrides are listed;
 * lv_conf_internal.h supplies LVGL defaults for everything else.
 *
 * - 8-bit grayscale rendering (L8): e-ink panels have 16 levels, LCD ports
 *   expand gray to RGB in their HAL.
 * - Memory comes from the device heaps via osal_malloc (system/ui/lv_mem_osal.c).
 * - No OS integration: exactly one task (the UI task) calls LVGL.
 * - Animations are off by default in the theme (e-ink).
 */
#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_COLOR_DEPTH 8

#define LV_USE_STDLIB_MALLOC LV_STDLIB_CUSTOM
#define LV_USE_STDLIB_STRING LV_STDLIB_BUILTIN
#define LV_USE_STDLIB_SPRINTF LV_STDLIB_BUILTIN

#define LV_USE_OS LV_OS_NONE
#define LV_DEF_REFR_PERIOD 30
#define LV_DPI_DEF 297

#define LV_DRAW_BUF_ALIGN 4
#define LV_DRAW_LAYER_SIMPLE_BUF_SIZE (24 * 1024)
#define LV_USE_DRAW_SW 1
#define LV_DRAW_SW_DRAW_UNIT_CNT 1
#define LV_DRAW_SW_SUPPORT_RGB565 0
#define LV_DRAW_SW_SUPPORT_RGB565_SWAPPED 0
#define LV_DRAW_SW_SUPPORT_RGB565A8 0
#define LV_DRAW_SW_SUPPORT_RGB888 0
#define LV_DRAW_SW_SUPPORT_XRGB8888 0
#define LV_DRAW_SW_SUPPORT_ARGB8888 1
#define LV_DRAW_SW_SUPPORT_ARGB8888_PREMULTIPLIED 0
#define LV_DRAW_SW_SUPPORT_L8 1
#define LV_DRAW_SW_SUPPORT_AL88 1
#define LV_DRAW_SW_SUPPORT_A8 1
#define LV_DRAW_SW_SUPPORT_I1 1
#define LV_DRAW_SW_COMPLEX 1

#define LV_USE_LOG 1
#define LV_LOG_LEVEL LV_LOG_LEVEL_WARN
#define LV_LOG_PRINTF 0

#define LV_USE_ASSERT_NULL 1
#define LV_USE_ASSERT_MALLOC 1

#define LV_USE_FLOAT 0
#define LV_USE_MATRIX 0

#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_28 1
#define LV_FONT_MONTSERRAT_40 1
#define LV_FONT_DEFAULT &lv_font_montserrat_28

#define LV_USE_THEME_DEFAULT 0
#define LV_USE_THEME_SIMPLE 1
#define LV_USE_THEME_MONO 0

#define LV_USE_VECTOR_GRAPHIC 0
#define LV_USE_THORVG_INTERNAL 0
#define LV_USE_THORVG_EXTERNAL 0
#define LV_USE_LOTTIE 0
#define LV_USE_SYSMON 0
#define LV_USE_PROFILER 0

#define LV_BUILD_EXAMPLES 0
#define LV_BUILD_DEMOS 0

#endif
