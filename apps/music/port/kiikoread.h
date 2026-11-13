/* Force-included into the kiikoread page and library: the ESP, FreeRTOS and kiikoread UI names they use, mapped
 * onto this system. Drawing is renamed to ink_music_* so it never meets Mogo's ui_* and epd_* in ink_apps. */
#pragma once
#include <stdarg.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "ink/osal.h"
#include "ink/sys/file.h"
#include "ink/sys/i18n.h"
#include <ink/ui_theme.h>
#include "epdiy.h"
#include "music_page.h"

#define ui_clear_page ink_music_clear
#define ui_fill_rect_fast ink_music_fill
#define ui_fill_round_rect ink_music_fill_round
#define ui_draw_round_rect ink_music_frame
#define ui_text_fixed_vc ink_music_text
#define ui_text_fixed_ink_vc ink_music_text_ink
#define ui_text_fixed_width_px ink_music_text_width
#define ui_draw_icon ink_music_icon
#undef strlcpy
#define strlcpy ink_music_copy
#define app_settings_system_font_size ink_music_percent
#define ink_ui_px ink_music_px  /* the resolution scale cached on the UI task: the page runs on BG */

enum { UI_ICON_BOOKMARK, UI_ICON_BOOKMARK_CHECK, UI_ICON_FOLDER, UI_ICON_FOLDER_OPEN };
#define UI_GRAY_WHITE ink_music_gray(0)
#define UI_GRAY_BLACK ink_music_gray(1)
#define UI_GRAY_SECONDARY ink_music_gray(2)
#define UI_GRAY_SEPARATOR ink_music_gray(3)
#define UI_KEY_1 INK_KEY_PREV
#define UI_KEY_2 INK_KEY_BACK
#define UI_KEY_3 INK_KEY_NEXT
typedef struct app_ctx app_ctx_t;

/* The library's player internals that the media service owns here. */
#define ESP_LOGI(...) ((void)0)
#define bt_trace(...) ((void)0)
#define close_track() ((void)0)
#define time_freeze() ((void)0)
#define set_state(s) atomic_store(&s_state, (int)(s))
#define esp_random ink_music_random
#define heap_caps_malloc(n, caps) osal_malloc((n), OSAL_MEM_EXTERNAL)
#define MALLOC_CAP_SPIRAM 0
#define MALLOC_CAP_8BIT 0

static inline size_t ink_music_copy(char *out, const char *in, size_t n) {
    size_t len = strlen(in), copy = n && len >= n ? n - 1 : len;
    if (n) {
        memcpy(out, in, copy);
        out[copy] = 0;
    }
    return len;
}
int ink_music_percent(void);
int ink_music_px(int px);
uint8_t ink_music_gray(int which);
uint32_t ink_music_random(void);
void ink_music_clear(uint8_t *fb);
void ink_music_fill(EpdRect r, uint8_t gray, uint8_t *fb);
void ink_music_fill_round(uint8_t *fb, EpdRect r, int radius, uint8_t gray);
void ink_music_frame(uint8_t *fb, EpdRect r, int radius, uint8_t gray);
void ink_music_text(uint8_t *fb, int x, int y, int px, const char *text, enum EpdFontFlags align, bool inverted);
void ink_music_text_ink(uint8_t *fb, int x, int y, int px, const char *text, enum EpdFontFlags align, uint8_t gray);
int ink_music_text_width(int px, const char *text);
void ink_music_icon(uint8_t *fb, int cx, int cy, int size, int icon, uint8_t gray);
