/* Compile-only contracts for ui_nav.c; these are not service implementations. */
#ifndef INK_ARCH_PROBE_COMPAT_H
#define INK_ARCH_PROBE_COMPAT_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct { int x, y, width, height; } EpdRect;
typedef struct app_desc_s app_desc_t;
typedef struct { const app_desc_t *request_app; } app_ctx_t;
typedef struct { uint8_t time_synced; uint32_t unix_sec; } pmu_snapshot_t;
typedef struct { bool network_ready; int mode; } read_pico_transfer_status_t;
typedef enum { UI_ICON_ANIMATE_CIRCLE_CHEVRON_LEFT, UI_ICON_WIFI, UI_ICON_HOUSE,
    UI_ICON_LIBRARY_BIG, UI_ICON_FOLDER_OPEN, UI_ICON_SLIDERS_HORIZONTAL,
    UI_ICON_BLUETOOTH, UI_ICON_ZAP } ui_icon_t;
typedef enum { UI_CLICK_NAV, UI_CLICK_BACK, UI_CLICK_BACK_SMALL } ui_click_kind_t;
enum EpdFontFlags { EPD_DRAW_ALIGN_LEFT, EPD_DRAW_ALIGN_CENTER, EPD_DRAW_ALIGN_RIGHT };
enum { READ_PICO_TRANSFER_MODE_STA = 1, APP_MAIN_REFRESH_NORMAL = 0 };
#define TTF_FONT_PATH_MAX 160
#define UI_LOCK_WIDTH 684
#define UI_LOCK_HEIGHT 1216
#define UI_GRAY_WHITE 0xff
#define UI_GRAY_LIGHT 0x90
#define UI_GRAY_BLACK 0x00

const app_desc_t *app_at(int index);
const pmu_snapshot_t *read_pico_pmu_get(void);
int pmu_battery_percent(const pmu_snapshot_t *pmu);
bool pmu_battery_charging(const pmu_snapshot_t *pmu);
#ifndef INK_ARCH_OMIT_NETWORK
void read_pico_transfer_get_status(read_pico_transfer_status_t *out);
#endif
const char *app_settings_status_signature(void);
const char *app_settings_system_font_path(void);
int app_settings_main_refresh_mode(void);
bool app_settings_ble_turner(void);
const char *ttf_font_path(void);
bool ui_font_has_text(const char *text);
int ui_font_text_width_px(int px, const char *text);
int ui_text_fixed_context_width_px(int px, const char *text, const char *sample);
int ui_text_fixed_width_px(int px, const char *text);
void ui_text(uint8_t *fb, int x, int y, int px, const char *text,
             enum EpdFontFlags align, bool inverted);
void ui_text_fixed_vc(uint8_t *fb, int x, int y, int px, const char *text,
                      enum EpdFontFlags align, bool inverted);
void ui_draw_icon(uint8_t *fb, int cx, int cy, int size, ui_icon_t icon, uint8_t gray);
void ui_draw_round_rect(uint8_t *fb, EpdRect rect, int radius, uint8_t color);
void ui_fill_round_rect(uint8_t *fb, EpdRect rect, int radius, uint8_t color);
void ui_hairline(uint8_t *fb, int y, int x, int w, uint8_t color);
void ui_fill_rect_fast(EpdRect rect, uint8_t gray, uint8_t *fb);
void epd_fill_rect(EpdRect rect, uint8_t gray, uint8_t *fb);
void ui_status_mark_visible(uint8_t *fb);
bool ui_status_visible(const uint8_t *fb);
void ui_click_feedback_register(uint8_t *fb, EpdRect rect, ui_click_kind_t kind, ui_icon_t icon);
#endif
