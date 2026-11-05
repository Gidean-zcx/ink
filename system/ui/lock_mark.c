/* Locked page or minute clock; touches remain disabled by the input service. */
#include <stdio.h>

#include "ink/sys/exec.h"
#include "ink/sys/notify.h"
#include "ink/sys/power.h"
#include "ink/sys/settings.h"
#include "ink/sys/state.h"
#include "ink/sys/time.h"
#include "ink/ui_shell.h"

static lv_obj_t *g_mark, *g_clock;
static uint64_t g_unlock_cause;
static int g_locked;
static ink_exec_job_t g_job;

static int snapshot(char *buf, size_t cap) { return snprintf(buf, cap, "设备已锁定，AI 无权读取。只能用电源键或 AI 键唤醒。"); }
static const ink_ui_overlay_t OVERLAY = {.name = "LockPage", .level = 2, .snapshot = snapshot};

static void update_clock(void) {
    if (!g_clock) return;
    char s[24] = "--:--";
    if (ink_time_valid()) { ink_tm_t t; ink_time_to_local(ink_time_now_s(), &t); snprintf(s, sizeof s, "%02d:%02d", t.hour, t.min); }
    lv_label_set_text(g_clock, s);
    ink_ui_hint_obj(g_clock, INK_REFRESH_TEXT);
}

static void show(void) {
    if (g_mark) return;
    bool clock = ink_setting_get("display.lock_style") == 1;
    g_mark = lv_obj_create(lv_layer_sys());
    lv_obj_remove_style_all(g_mark);
    lv_obj_set_size(g_mark, LV_PCT(100), LV_PCT(100));
    lv_obj_remove_flag(g_mark, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    if (clock) {
        lv_obj_set_style_bg_color(g_mark, ink_ui_paper(), 0);
        lv_obj_set_style_bg_opa(g_mark, LV_OPA_COVER, 0);
        g_clock = ink_ui_label(g_mark, "", INK_FONT_CLOCK, INK_WEIGHT_REGULAR, INK_MARGIN,
                               INK_LINE_BODY + INK_BLOCK + INK_LINE_LIST, INK_CANVAS_W - 2 * INK_MARGIN);
        lv_obj_set_style_text_align(g_clock, LV_TEXT_ALIGN_CENTER, 0);
        update_clock();
        char count[64]; snprintf(count, sizeof count, "%d 条通知，唤醒后查看", ink_notify_unread());
        ink_ui_notice(g_mark, count, INK_CANVAS_H - 2 * INK_PRIMARY - INK_MARGIN);
    }
    lv_obj_t *strip = ink_ui_box(g_mark, INK_MARGIN, INK_CANVAS_H - INK_CONTROL - INK_GAP_S,
                                  INK_CANVAS_W - 2 * INK_MARGIN, INK_CONTROL, 0, INK_SURFACE_WHITE, false);
    lv_obj_t *l = ink_ui_label(strip, "已锁定，按电源键或 AI 键唤醒", INK_FONT_NOTE, INK_WEIGHT_REGULAR, 0,
                               (INK_CONTROL - INK_LINE_NOTE) / 2, INK_CANVAS_W - 2 * INK_MARGIN);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    ink_ui_overlay_add(&OVERLAY, g_mark);
    ink_ui_hint_obj(clock ? g_mark : strip, clock ? INK_REFRESH_UI : INK_REFRESH_FAST);
}
static void hide(void) {
    if (!g_mark) return;
    ink_ui_hint_obj(g_mark, INK_REFRESH_TEXT);
    ink_ui_cause(g_unlock_cause, "power.unlock_to_visible_ms");
    ink_ui_overlay_remove(&OVERLAY);
    lv_obj_delete(g_mark); g_mark = g_clock = NULL;
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
int ink_ui_lock_mark_init(void) {
    if (ink_power_add_busy_check(ink_ui_busy) != 0 || ink_state_watch("time.minute", on_minute, NULL, INK_EXEC_UI) != 0) return -1;
    return ink_power_subscribe(on_power, NULL);
}
