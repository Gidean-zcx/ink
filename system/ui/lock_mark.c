/* Locked page or minute clock, with a capture-only camera entry. */
#include <stdio.h>

#include "ink/sys/exec.h"
#include "ink/sys/i18n.h"
#include "ink/sys/notify.h"
#include "ink/sys/power.h"
#include "ink/sys/settings.h"
#include "ink/sys/state.h"
#include "ink/sys/time.h"
#include "ink/ui_shell.h"
#include "shell/activity_card.h"

static lv_obj_t *g_mark, *g_clock, *g_label;
static uint64_t g_unlock_cause;
static int g_locked;
static ink_exec_job_t g_job;

static int snapshot(char *buf, size_t cap) { return snprintf(buf, cap, "%s", TR("设备已锁定；可拍照：app.open(id=camera)。电源键或 AI 键唤醒。")); }
static const ink_ui_overlay_t OVERLAY = {.name = "LockPage", .level = 2, .snapshot = snapshot};
static const char *mark_text(void) {
    return ink_setting_get("display.lock_style") == 0 && ink_activity_listening() ? TR("正在听，按电源键或 AI 键唤醒") : TR("已锁定，按电源键或 AI 键唤醒");
}

static void update_clock(void) {
    if (!g_clock) return;
    char s[24], date[96];
    ink_shell_time_strings(s, sizeof s, date, sizeof date);
    lv_label_set_text(g_clock, s);
    ink_ui_hint_obj(g_clock, INK_REFRESH_TEXT);
}

static void show(void) {
    if (g_mark) return;
    bool clock = ink_setting_get("display.lock_style") == 1;
    bool image = ink_setting_get("display.lock_style") == 2 && ink_wallpaper_ready();
    int strip_h = LV_MAX(INK_CONTROL, 2 * ink_ui_line_height(INK_FONT_NOTE));
    g_mark = clock || image ? lv_obj_create(lv_layer_sys()) : ink_ui_box(lv_layer_sys(), INK_MARGIN,
             INK_CANVAS_H - strip_h - INK_GAP_S, INK_CANVAS_W - 2 * INK_MARGIN, strip_h, 0, INK_SURFACE_WHITE, false);
    if (clock || image) {
        lv_obj_remove_style_all(g_mark);
        lv_obj_set_size(g_mark, LV_PCT(100), LV_PCT(100));
    }
    lv_obj_remove_flag(g_mark, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(g_mark, LV_OBJ_FLAG_CLICKABLE);
    if (image) {
        ink_wallpaper_draw(g_mark);
        ink_shell_camera_button(g_mark, false);
        ink_ui_overlay_add(&OVERLAY, g_mark); ink_ui_hint_obj(g_mark, INK_REFRESH_FLASH_UI);
        return;
    }
    if (clock) {
        lv_obj_set_style_bg_color(g_mark, ink_ui_paper(), 0);
        lv_obj_set_style_bg_opa(g_mark, LV_OPA_COVER, 0);
        ink_shell_statusbar(g_mark, false);
        char time[24], date[96]; ink_shell_time_strings(time, sizeof time, date, sizeof date);
        lv_obj_t *date_label = ink_ui_label(g_mark, date, INK_FONT_LIST, INK_WEIGHT_MEDIUM,
                                           INK_MARGIN, INK_STATUS_BAR + INK_BLOCK, INK_CANVAS_W - 2 * INK_MARGIN);
        lv_obj_set_style_text_align(date_label, LV_TEXT_ALIGN_CENTER, 0);
        g_clock = ink_ui_label(g_mark, "", INK_FONT_CLOCK, INK_WEIGHT_REGULAR, INK_MARGIN,
                               INK_STATUS_BAR + INK_BLOCK + ink_ui_line_height(INK_FONT_LIST), INK_CANVAS_W - 2 * INK_MARGIN);
        lv_obj_set_style_text_align(g_clock, LV_TEXT_ALIGN_CENTER, 0);
        update_clock();
        int cursor = 0; ink_alarm_t alarm;
        if (ink_alarm_next(&cursor, INK_ALARM, &alarm) == 1) {
            char next[64]; snprintf(next, sizeof next, "闹钟 %d:%02d", alarm.hour, alarm.min);
            ink_ui_notice(g_mark, next, INK_STATUS_BAR + INK_BLOCK + ink_ui_line_height(INK_FONT_LIST) + ink_ui_line_height(INK_FONT_CLOCK) + INK_GAP_M);
        }
        int h = ink_ui_row_double(), text_y = (h - ink_ui_line_height(INK_FONT_LIST) - ink_ui_line_height(INK_FONT_NOTE)) / 2;
        lv_obj_t *notice = ink_ui_card(g_mark, INK_MARGIN, INK_CANVAS_H - INK_CONTROL - INK_GAP_L - INK_MARGIN - h,
                                       INK_CANVAS_W - 2 * INK_MARGIN, h, INK_SURFACE_WHITE);
        ink_ui_circle(notice, "bell", INK_GAP, (h - INK_CIRCLE) / 2, INK_CIRCLE, INK_ICON_LIST, INK_SURFACE_STATIC, NULL, NULL);
        char count[64]; snprintf(count, sizeof count, "%d 条通知", ink_notify_unread());
        ink_ui_line(notice, count, INK_FONT_LIST, INK_WEIGHT_BOLD, INK_ROW_DOUBLE, text_y, INK_CANVAS_W - 2 * INK_ROW_DOUBLE);
        ink_ui_line(notice, "唤醒后查看", INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_ROW_DOUBLE, text_y + ink_ui_line_height(INK_FONT_LIST), INK_CANVAS_W - 2 * INK_ROW_DOUBLE);
    }
    ink_shell_camera_button(g_mark, !clock);
    lv_obj_t *strip = clock ? ink_ui_box(g_mark, INK_MARGIN, INK_CANVAS_H - strip_h - INK_GAP_S,
                                  INK_CANVAS_W - 2 * INK_MARGIN, strip_h, 0, INK_SURFACE_WHITE, false) : g_mark;
    lv_obj_t *l = ink_ui_label(strip, mark_text(), INK_FONT_NOTE, INK_WEIGHT_REGULAR, 0,
                               (strip_h - ink_ui_line_height(INK_FONT_NOTE)) / 2, INK_CANVAS_W - 2 * INK_MARGIN - (clock ? 0 : INK_CIRCLE + INK_GAP));
    lv_obj_align(l, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    g_label = l;
    ink_ui_overlay_add(&OVERLAY, g_mark);
    ink_ui_hint_obj(clock ? g_mark : strip, clock ? INK_REFRESH_UI : INK_REFRESH_FAST);
}
static void hide(void) {
    if (!g_mark) return;
    ink_ui_hint_obj(g_mark, INK_REFRESH_TEXT);
    ink_ui_cause(g_unlock_cause, "power.unlock_to_visible_ms");
    ink_ui_overlay_remove(&OVERLAY);
    lv_obj_delete(g_mark); g_mark = g_clock = g_label = NULL;
}
static void sync_mark(void *arg) { (void)arg; if (__atomic_load_n(&g_locked, __ATOMIC_ACQUIRE)) show(); else hide(); }
static void on_power(void *ctx, ink_power_state_t from, ink_power_state_t to, uint64_t cause_us) {
    (void)ctx;
    if (to == INK_PWR_LOCKED) __atomic_store_n(&g_locked, 1, __ATOMIC_RELEASE);
    else if (from == INK_PWR_LOCKED) { g_unlock_cause = cause_us; __atomic_store_n(&g_locked, 0, __ATOMIC_RELEASE); }
    else return;
    ink_exec_post(INK_EXEC_UI, &g_job, sync_mark, NULL);
}
static void on_minute(void *ctx, int h, const char *id) { (void)ctx; (void)h; (void)id; update_clock(); }
static void on_audio(void *ctx, int h, const char *id) {
    (void)ctx; (void)h; (void)id;
    if (!g_label || g_clock || ink_setting_get("display.lock_style") != 0) return;
    lv_label_set_text(g_label, mark_text());
    ink_ui_hint_obj(g_mark, INK_REFRESH_FAST);
}
int ink_ui_lock_mark_init(void) {
    if (ink_state_watch("media.state", on_audio, NULL, INK_EXEC_UI) || ink_state_watch("audio.now_playing", on_audio, NULL, INK_EXEC_UI)) return -1;
    if (ink_power_add_busy_check(ink_ui_busy) != 0 || ink_state_watch("time.minute", on_minute, NULL, INK_EXEC_UI) != 0) return -1;
    return ink_power_subscribe(on_power, NULL);
}
