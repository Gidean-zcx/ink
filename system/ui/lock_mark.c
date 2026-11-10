/* Locked page or minute clock, with a capture-only camera entry. */
#include <stdio.h>
#include <string.h>

#include "ink/sys/exec.h"
#include "ink/sys/i18n.h"
#include "ink/sys/notify.h"
#include "ink/sys/power.h"
#include "ink/sys/settings.h"
#include "ink/sys/state.h"
#include "ink/sys/time.h"
#include "ink/sys/ui_semantics.h"
#include "ink/ui_shell.h"
#include "shell/activity_card.h"

static lv_obj_t *g_mark, *g_clock, *g_label, *g_date, *g_alarm, *g_notice;
static uint32_t g_version, g_revision;
static uint64_t g_unlock_cause;
static int g_locked;
static ink_exec_job_t g_job;
static const ink_ui_action_spec_t ACTIONS[] = {
    {.id = "emergency", .label = TR_NOOP("打开紧急呼叫页面"), .capability = "phone.emergency_open", .enabled = true},
    {.id = "camera", .label = TR_NOOP("拍照"), .capability = "app.open", .args_json = "{\"id\":\"camera\"}", .enabled = true}
};
static const ink_ui_page_spec_t MODEL = {.id = "LockPage", .title = TR_NOOP("设备已锁定"), .object = {"LockPage", 0}, .actions = ACTIONS, .action_count = 2};
static void changed(void) { g_revision++; g_version = ink_ui_content_version(); }
static void pressed(lv_event_t *e) { (void)e; g_version = ink_ui_content_version(); }
static void camera_clicked(lv_event_t *e) {
    lv_event_stop_processing(e);
    ink_ui_page_spec_t page = MODEL; page.object.revision = g_revision;
    ink_ui_action_target_t target = {g_version, page.object};
    ink_receipt_t rc;
    ink_ui_semantics_action(&page, "camera", &target, INK_SRC_TOUCH, &rc);
}
static void camera_button(bool strip) {
    if (!ink_cap_find("camera.capture")) return;
    lv_obj_t *b = ink_ui_circle(g_mark, "camera", INK_CANVAS_W - (strip ? 2 : 1) * INK_MARGIN - INK_CIRCLE, strip ? 0 : INK_STATUS_BAR + INK_GAP,
        INK_CIRCLE, INK_ICON_MEDIUM, INK_SURFACE_WHITE, camera_clicked, NULL);
    lv_obj_set_style_border_width(b, ink_ui_px(INK_STROKE), 0);
    ink_shell_bind(b, ACTIONS[1].capability, ACTIONS[1].args_json);
    lv_obj_add_event_cb(b, pressed, LV_EVENT_PRESSED, NULL);
}

static int semantic_snapshot(char *buf, size_t cap) {
    ink_ui_field_spec_t fields[] = {
        {.id = "status", .role = "text", .label = TR("锁屏"), .value = g_label ? lv_label_get_text(g_label) : TR("设备已锁定；可拍照：app.open(id=camera)。电源键或 AI 键唤醒。")},
        {.id = "time", .role = "text", .label = TR("时间"), .value = g_clock ? lv_label_get_text(g_clock) : ""},
        {.id = "date", .role = "text", .label = TR("日期"), .value = g_date ? lv_label_get_text(g_date) : ""},
        {.id = "alarm", .role = "text", .label = TR("闹钟"), .value = g_alarm ? lv_label_get_text(g_alarm) : ""},
        {.id = "notifications", .role = "text", .label = TR("通知"), .value = g_notice ? lv_label_get_text(g_notice) : ""}
    };
    ink_ui_page_spec_t model = MODEL; model.title = TR(MODEL.title);
    ink_ui_action_spec_t actions[2]; memcpy(actions, ACTIONS, sizeof actions);
    for (size_t i = 0; i < 2; i++) actions[i].label = TR(ACTIONS[i].label);
    model.actions = actions;
    model.object.revision = g_revision;
    model.fields = fields; model.field_count = g_clock ? sizeof fields / sizeof *fields : 1;
    model.action_count = ink_cap_find("camera.capture") ? 2 : 1;
    return ink_ui_semantics_snapshot(&model, buf, cap);
}
static const ink_ui_overlay_t OVERLAY = {.name = "LockPage", .level = 2, .snapshot = semantic_snapshot};
static const char *mark_text(void) {
    return ink_setting_get("display.lock_style") == 0 && ink_activity_listening() ? TR("正在听，按电源键或 AI 键唤醒") : TR("已锁定，按电源键或 AI 键唤醒");
}

static void update_clock(void) {
    if (!g_clock) return;
    char s[24], date[96];
    ink_shell_time_strings(s, sizeof s, date, sizeof date);
    if (!strcmp(lv_label_get_text(g_clock), s)) return;
    lv_label_set_text(g_clock, s);
    changed();
    ink_ui_hint_obj(g_clock, INK_REFRESH_TEXT);
}

static void show(void) {
    if (g_mark) return;
    bool clock = ink_setting_get("display.lock_style") == 1;
    bool image = ink_setting_get("display.lock_style") == 2 && ink_wallpaper_ready();
    int text_w = INK_CANVAS_W - 2 * INK_MARGIN - (clock ? 0 : INK_CIRCLE + 2 * INK_CONTROL + 4 * INK_GAP);
    int text_h = ink_ui_text_height(mark_text(), INK_FONT_NOTE, INK_WEIGHT_REGULAR, text_w) + INK_GAP_S;
    int strip_h = text_h > INK_CONTROL ? text_h : INK_CONTROL;
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
        camera_button(false);
        ink_shell_emergency_button(g_mark, false);
        ink_ui_overlay_add(&OVERLAY, g_mark); changed(); ink_ui_hint_obj(g_mark, INK_REFRESH_FLASH_UI);
        return;
    }
    if (clock) {
        lv_obj_set_style_bg_color(g_mark, ink_ui_paper(), 0);
        lv_obj_set_style_bg_opa(g_mark, LV_OPA_COVER, 0);
        ink_shell_statusbar(g_mark, false);
        char time[24], date[96]; ink_shell_time_strings(time, sizeof time, date, sizeof date);
        int date_y = LV_MAX(INK_STATUS_BAR + INK_BLOCK, INK_STATUS_BAR + INK_GAP + INK_CONTROL + INK_GAP_M);
        lv_obj_t *date_label = ink_ui_label(g_mark, date, INK_FONT_LIST, INK_WEIGHT_MEDIUM,
                                           INK_MARGIN, date_y, INK_CANVAS_W - 2 * INK_MARGIN);
        lv_obj_set_style_text_align(date_label, LV_TEXT_ALIGN_CENTER, 0);
        g_date = date_label;
        g_clock = ink_ui_label(g_mark, "", INK_FONT_CLOCK, INK_WEIGHT_REGULAR, INK_MARGIN,
                               date_y + ink_ui_line_height(INK_FONT_LIST), INK_CANVAS_W - 2 * INK_MARGIN);
        lv_obj_set_style_text_align(g_clock, LV_TEXT_ALIGN_CENTER, 0);
        update_clock();
        int cursor = 0; ink_alarm_t alarm;
        if (ink_alarm_next(&cursor, INK_ALARM, &alarm) == 1) {
            char next[64]; snprintf(next, sizeof next, TR("闹钟 %d:%02d"), alarm.hour, alarm.min);
            g_alarm = ink_ui_notice(g_mark, next, date_y + ink_ui_line_height(INK_FONT_LIST) + ink_ui_line_height(INK_FONT_CLOCK) + INK_GAP_M);
        }
        int h = ink_ui_row_double(), text_y = (h - ink_ui_line_height(INK_FONT_LIST) - ink_ui_line_height(INK_FONT_NOTE)) / 2;
        lv_obj_t *notice = ink_ui_card(g_mark, INK_MARGIN, INK_CANVAS_H - INK_CONTROL - INK_GAP_L - INK_MARGIN - h,
                                       INK_CANVAS_W - 2 * INK_MARGIN, h, INK_SURFACE_WHITE);
        ink_ui_circle(notice, "bell", INK_GAP, (h - INK_CIRCLE) / 2, INK_CIRCLE, INK_ICON_LIST, INK_SURFACE_STATIC, NULL, NULL);
        char count[64]; snprintf(count, sizeof count, TR("%d 条通知"), ink_notify_unread());
        g_notice = ink_ui_line(notice, count, INK_FONT_LIST, INK_WEIGHT_BOLD, INK_ROW_DOUBLE, text_y, INK_CANVAS_W - 2 * INK_ROW_DOUBLE);
        ink_ui_line(notice, TR("唤醒后查看"), INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_ROW_DOUBLE, text_y + ink_ui_line_height(INK_FONT_LIST), INK_CANVAS_W - 2 * INK_ROW_DOUBLE);
    }
    camera_button(!clock);
    ink_shell_emergency_button(g_mark, !clock);
    lv_obj_t *strip = clock ? ink_ui_box(g_mark, INK_MARGIN, INK_CANVAS_H - strip_h - INK_GAP_S,
                                  INK_CANVAS_W - 2 * INK_MARGIN, strip_h, 0, INK_SURFACE_WHITE, false) : g_mark;
    lv_obj_t *l = ink_ui_label(strip, mark_text(), INK_FONT_NOTE, INK_WEIGHT_REGULAR, 0,
                               (strip_h - ink_ui_line_height(INK_FONT_NOTE)) / 2, text_w);
    lv_obj_align(l, LV_ALIGN_LEFT_MID, clock ? 0 : ink_ui_px(2 * INK_CONTROL + 3 * INK_GAP), 0);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    g_label = l;
    ink_ui_overlay_add(&OVERLAY, g_mark);
    changed();
    ink_ui_hint_obj(clock ? g_mark : strip, clock ? INK_REFRESH_UI : INK_REFRESH_FAST);
}
static void hide(void) {
    if (!g_mark) return;
    ink_ui_hint_obj(g_mark, INK_REFRESH_TEXT);
    ink_ui_cause(g_unlock_cause, "power.unlock_to_visible_ms");
    ink_ui_overlay_remove(&OVERLAY);
    lv_obj_delete(g_mark); g_mark = g_clock = g_label = g_date = g_alarm = g_notice = NULL;
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
    if (!strcmp(lv_label_get_text(g_label), mark_text())) return;
    lv_label_set_text(g_label, mark_text());
    changed();
    ink_ui_hint_obj(g_mark, INK_REFRESH_FAST);
}
int ink_ui_lock_mark_init(void) {
    if (ink_state_watch("media.state", on_audio, NULL, INK_EXEC_UI) || ink_state_watch("audio.now_playing", on_audio, NULL, INK_EXEC_UI)) return -1;
    if (ink_power_add_busy_check(ink_ui_busy) != 0 || ink_state_watch("time.minute", on_minute, NULL, INK_EXEC_UI) != 0) return -1;
    return ink_power_subscribe(on_power, NULL);
}
