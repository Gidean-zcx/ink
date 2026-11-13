/* Locked: a cover with the time, today's next thing, unread counts by kind and the family (D-135); or a chosen face. */
/* ui-scale-runtime: dimension date_y Keep the date below the emergency control using theme spacing. */
/* ui-scale-runtime: dimension ew The emergency call's width from its own words at the current text size. */
/* ui-scale-runtime: dimension y,x,w,row,top,bottom,cell,x0,inset Cover rows follow theme line heights; avatars share the width. */
/* ui-scale-runtime: font font Centered cover lines receive theme font tokens. */
#include <stdio.h>
#include <string.h>

#include "ink/osal.h"
#include "ink/sys/exec.h"
#include "ink/sys/i18n.h"
#include "ink/sys/members.h"
#include "ink/sys/notify.h"
#include "ink/sys/power.h"
#include "ink/sys/settings.h"
#include "ink/sys/state.h"
#include "ink/sys/time.h"
#include "ink/sys/ui_semantics.h"
#include "ink/ui_shell.h"
#include "shell/activity_card.h"
#include "shell/member_pages.h"
#include "lock_faces/faces.h"

static lv_obj_t *g_mark, *g_clock, *g_label, *g_date, *g_alarm, *g_notice, *g_next, *g_rows;
static uint32_t g_version, g_revision;
static uint64_t g_unlock_cause;
static int g_locked;
static bool g_minute_due; /* a minute turned while the chip slept or woke */
enum { LOCK_FIELDS = 6 + INK_LOCK_FACE_FIELDS, KINDS = 16, COVER_FIELDS = 5 + KINDS + INK_MEMBERS_MAX };
/* The cover's state: what each row and avatar shows, so a count that changes redraws only its number. */
static struct {
    char owner[16], pending[16], next[96], id[INK_MEMBERS_MAX][16], name[INK_MEMBERS_MAX][32], args[INK_MEMBERS_MAX][32];
    char value[KINDS][12];
    const char *label[KINDS];
    int count[KINDS], kind[KINDS], shown, rows_h;
    lv_obj_t *number[KINDS];
    ink_ui_field_spec_t fields[COVER_FIELDS];
    ink_ui_action_spec_t actions[INK_MEMBERS_MAX];
    size_t nactions;
} C INK_PSRAM_BSS;
static ink_exec_job_t g_job, g_minute_job;
static const ink_ui_action_spec_t ACTIONS[] = {
    {.id = "emergency", .label = TR_NOOP("打开紧急呼叫页面"), .capability = "phone.emergency_open", .enabled = true},
    {.id = "camera", .label = TR_NOOP("拍照"), .capability = "app.open", .args_json = "{\"id\":\"camera\"}", .enabled = true}
};
static const ink_ui_page_spec_t MODEL = {.id = "LockPage", .title = TR_NOOP("设备已锁定"), .object = {"LockPage", 0}, .actions = ACTIONS, .action_count = 2};
static void changed(void) { g_revision++; g_version = ink_ui_content_version(); }
static void pressed(lv_event_t *e) { (void)e; g_version = ink_ui_content_version(); }
static void act(lv_event_t *e, const ink_ui_action_spec_t *actions, size_t count) {
    lv_event_stop_processing(e);
    ink_ui_page_spec_t page = MODEL; page.object.revision = g_revision; page.actions = actions; page.action_count = count;
    ink_ui_action_target_t target = {g_version, page.object};
    ink_receipt_t rc;
    ink_ui_semantics_action(&page, lv_event_get_user_data(e), &target, INK_SRC_TOUCH, &rc);
}
static void camera_clicked(lv_event_t *e) { act(e, ACTIONS, 2); }
static void avatar_clicked(lv_event_t *e) { act(e, C.actions, C.nactions); }
static void camera_at(int x, int y) {
    if (!ink_cap_find("camera.capture")) return;
    lv_obj_t *b = ink_ui_circle(g_mark, "camera", x, y, INK_CIRCLE, INK_ICON_MEDIUM, INK_SURFACE_WHITE, camera_clicked, "camera");
    lv_obj_set_style_border_width(b, ink_ui_px(INK_STROKE), 0);
    ink_shell_bind(b, ACTIONS[1].capability, ACTIONS[1].args_json);
    lv_obj_add_event_cb(b, pressed, LV_EVENT_PRESSED, NULL);
}
static void camera_button(void) { camera_at(INK_CANVAS_W - INK_MARGIN - INK_CIRCLE, INK_STATUS_BAR + INK_GAP); }
static const char *mark_text(void) {
    if (ink_lock_face_why()) return ink_lock_face_why(); /* a drawn style that cannot show says why (D-138) */
    return ink_setting_get("display.lock_style") == 0 && ink_activity_listening() ? TR("正在听，按电源键或 AI 键唤醒") : TR("已锁定，按电源键或 AI 键唤醒");
}

static int cover_snapshot(ink_ui_page_spec_t *model, char *buf, size_t cap) {
    size_t n = 0;
    C.fields[n++] = (ink_ui_field_spec_t){.id = "status", .role = "text", .label = TR("锁屏"), .value = lv_label_get_text(g_label)};
    C.fields[n++] = (ink_ui_field_spec_t){.id = "time", .role = "text", .label = TR("时间"), .value = lv_label_get_text(g_clock)};
    C.fields[n++] = (ink_ui_field_spec_t){.id = "date", .role = "text", .label = TR("日期"), .value = lv_label_get_text(g_date)};
    if (lv_label_get_text(g_next)[0]) C.fields[n++] = (ink_ui_field_spec_t){.id = "next", .role = "text", .label = TR("今天"), .value = lv_label_get_text(g_next)};
    for (int r = 0; r < C.shown; r++)
        C.fields[n++] = (ink_ui_field_spec_t){.id = C.kind[r] < 0 ? "notify.more" : ink_notify_channel_at(C.kind[r])->id, .role = "text", .label = C.label[r], .value = C.value[r]};
    for (size_t i = 0; i < C.nactions; i++)
        C.fields[n++] = (ink_ui_field_spec_t){.id = C.id[i], .role = "button", .label = C.name[i], .value = ink_member_has_pin(C.id[i]) ? TR("有密码") : TR("无密码")};
    C.fields[n++] = (ink_ui_field_spec_t){.id = "face", .role = "text", .label = TR("表盘"), .value = TR(ink_lock_face_choices[ink_setting_get("display.lock_style")])};
    model->fields = C.fields; model->field_count = n; model->actions = C.actions; model->action_count = C.nactions;
    return ink_ui_semantics_snapshot(model, buf, cap);
}
static int semantic_snapshot(char *buf, size_t cap) {
    ink_ui_page_spec_t model = MODEL; model.title = TR(MODEL.title);
    model.object.revision = g_revision;
    if (g_next) return cover_snapshot(&model, buf, cap);
    ink_ui_field_spec_t fields[LOCK_FIELDS] = {
        {.id = "status", .role = "text", .label = TR("锁屏"), .value = g_label ? lv_label_get_text(g_label) : TR("设备已锁定；可拍照：app.open(id=camera)。电源键或 AI 键唤醒。")},
        {.id = "time", .role = "text", .label = TR("时间"), .value = g_clock ? lv_label_get_text(g_clock) : ""},
        {.id = "date", .role = "text", .label = TR("日期"), .value = g_date ? lv_label_get_text(g_date) : ""},
        {.id = "alarm", .role = "text", .label = TR("闹钟"), .value = g_alarm ? lv_label_get_text(g_alarm) : ""},
        {.id = "notifications", .role = "text", .label = TR("通知"), .value = g_notice ? lv_label_get_text(g_notice) : ""},
        {.id = "face", .role = "text", .label = TR("表盘"), .value = TR(ink_lock_face_choices[ink_setting_get("display.lock_style")])}
    };
    ink_ui_action_spec_t actions[2]; memcpy(actions, ACTIONS, sizeof actions);
    for (size_t i = 0; i < 2; i++) actions[i].label = TR(ACTIONS[i].label);
    model.actions = actions;
    size_t extra = ink_lock_face_fields(fields + 6);
    model.fields = extra ? fields + 5 : fields; model.field_count = extra ? 1 + extra : 6;
    model.action_count = ink_cap_find("camera.capture") ? 2 : 1;
    return ink_ui_semantics_snapshot(&model, buf, cap);
}
static const ink_ui_overlay_t OVERLAY = {.name = "LockPage", .level = 2, .snapshot = semantic_snapshot};

static void update_clock(void) {
    if (!g_clock) return;
    char s[24], date[96];
    ink_shell_time_strings(s, sizeof s, date, sizeof date);
    if (g_date && strcmp(lv_label_get_text(g_date), date)) {
        lv_label_set_text(g_date, date); changed(); ink_ui_hint_obj(g_date, INK_REFRESH_TEXT);
    }
    if (!strcmp(lv_label_get_text(g_clock), s)) return;
    lv_label_set_text(g_clock, s);
    changed();
    ink_ui_hint_obj(g_clock, INK_REFRESH_TEXT);
}

/* Today's next reminder or event, never while the member's space is closed: it is theirs. */
static void next_text(char *out, size_t cap) {
    out[0] = 0;
    if (!ink_time_valid() || !ink_member_space_open()) return;
    int64_t now = ink_time_now_s(), best = 0;
    ink_tm_t t; ink_time_to_local(now, &t);
    t.hour = t.min = t.sec = 0; t.day++;
    int64_t until = ink_time_from_local(&t);
    int cursor = 0; ink_alarm_t a;
    while (ink_alarm_next(&cursor, INK_REMINDER, &a) == 1) {
        int64_t at = a.silent || a.duration_s ? ink_alarm_occurrence(&a, now) : a.next_s;
        if (!a.enabled || !at || at >= until || (best && at >= best)) continue;
        best = at; ink_time_to_local(at, &t);
        if (a.all_day) snprintf(out, cap, "%s %.47s", TR("全天"), a.title);
        else snprintf(out, cap, "%02d:%02d %.47s", t.hour, t.min, a.title);
    }
}
static void update_next(void) {
    if (!g_next) return;
    next_text(C.next, sizeof C.next);
    if (!strcmp(lv_label_get_text(g_next), C.next)) return;
    lv_label_set_text(g_next, C.next); changed(); ink_ui_hint_obj(g_next, INK_REFRESH_TEXT);
}

/* One line of text alone in a box its size: a change in it refreshes that box, not the glyph margin LVGL adds around. */
static lv_obj_t *boxed(lv_obj_t *parent, const char *text, int font, ink_ui_weight_t weight, int x, int y, int w) {
    return ink_ui_line(ink_ui_box(parent, x, y, w, ink_ui_line_height(font), 0, INK_SURFACE_WHITE, false), text, font, weight, 0, 0, w);
}
/* Unread notifications per channel, counted only: titles and bodies stay in the inbox. Returns how many kinds. */
static int count_unread(void) {
    memset(C.count, 0, sizeof C.count);
    int left = ink_notify_unread(), kinds = 0;
    ink_notify_view_t *v = left > 0 ? osal_malloc(sizeof *v, OSAL_MEM_EXTERNAL) : NULL;
    ink_db_cursor_t cursor = {0};
    while (v && left > 0 && ink_notify_next(&cursor, v) == 1) {
        if (v->read) continue;
        left--;
        for (int i = 0; i < KINDS && ink_notify_channel_at(i); i++)
            if (!strcmp(ink_notify_channel_at(i)->id, v->channel)) { kinds += !C.count[i]++; break; }
    }
    osal_free(v);
    return kinds;
}
/* One row per kind, its number only; the system's own last; what does not fit folds into one row. */
static void fill_rows(bool fresh) {
    int kinds = count_unread(), line = ink_ui_line_height(INK_FONT_LIST), fit = C.rows_h / (line + INK_GAP_S);
    int room = kinds > fit ? fit - 1 : fit, kind[KINDS], n = 0, rest = 0;
    char value[KINDS][12];
    for (int k = 1; k <= KINDS; k++) {
        int i = k % KINDS;
        if (!C.count[i]) continue;
        if (n < room) { kind[n] = i; snprintf(value[n++], sizeof *value, "%d", C.count[i]); }
        else rest += C.count[i];
    }
    if (rest && fit > 0) { kind[n] = -1; snprintf(value[n++], sizeof *value, "%d", rest); }
    bool same = !fresh && n == C.shown && !memcmp(kind, C.kind, (size_t)n * sizeof *kind);
    if (!same) { lv_obj_clean(g_rows); C.shown = n; memcpy(C.kind, kind, (size_t)n * sizeof *kind); }
    int inset = INK_BLOCK_L - INK_MARGIN, w = INK_CANVAS_W - 2 * INK_BLOCK_L;
    for (int r = 0; r < n; r++) {
        if (same && !strcmp(C.value[r], value[r])) continue;
        snprintf(C.value[r], sizeof C.value[r], "%s", value[r]);
        changed();
        if (same) { lv_label_set_text(C.number[r], C.value[r]); ink_ui_hint_obj(C.number[r], INK_REFRESH_TEXT); continue; }
        const ink_notify_channel_t *ch = kind[r] < 0 ? NULL : ink_notify_channel_at(kind[r]);
        C.label[r] = !ch ? TR("通知") : !strcmp(ch->id, "call") ? TR("未接来电") : TR(ch->title);
        int y = r * (line + INK_GAP_S);
        ink_ui_icon(g_rows, !ch ? "bell" : !strcmp(ch->id, "message") ? "message" : !strcmp(ch->id, "call") ? "phone" : !strcmp(ch->id, "alarm") ? "clock" :
                    !strcmp(ch->id, "reminder") ? "calendar" : !strcmp(ch->id, "download") ? "download" : !strcmp(ch->id, "ai") ? "ai" : "bell",
                    INK_ICON_MEDIUM, INK_ICON_LINE, inset, y + (line - INK_ICON_MEDIUM) / 2, false);
        ink_ui_line(g_rows, C.label[r], INK_FONT_LIST, INK_WEIGHT_REGULAR, inset + INK_ICON_MEDIUM + INK_GAP_M, y, w - INK_ICON_MEDIUM - INK_GAP_M - INK_BLOCK_L);
        C.number[r] = boxed(g_rows, C.value[r], INK_FONT_LIST, INK_WEIGHT_BOLD, inset + w - INK_BLOCK_L, y, INK_BLOCK_L);
        lv_obj_set_style_text_align(C.number[r], LV_TEXT_ALIGN_RIGHT, 0);
    }
    if (!same && !fresh) { changed(); ink_ui_hint_obj(g_rows, INK_REFRESH_TEXT); }
}

static lv_obj_t *centered(const char *text, int font, ink_ui_weight_t weight, int y) {
    lv_obj_t *l = boxed(g_mark, text, font, weight, INK_MARGIN, y, INK_CANVAS_W - 2 * INK_MARGIN);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    return l;
}
/* Everyone in the family above bottom, the one whose space it is filled: a tap enters as that person. The counts get the room from y. */
static void family(int y, int bottom) {
    int n = 0, w = INK_CANVAS_W - 2 * INK_MARGIN, name = ink_ui_line_height(INK_FONT_NOTE);
    while (n < INK_MEMBERS_MAX && ink_member_at(n)) n++;
    int per = n <= 5 ? n : (n + 1) / 2, unit = INK_CIRCLE + INK_GAP_XS + name + INK_GAP;
    int rows = n > 5 ? 2 : 1;
    int cell = LV_MIN(w / LV_MAX(per, 1), INK_CIRCLE + INK_BLOCK_L), top = bottom - rows * unit, x0 = INK_MARGIN + (w - per * cell) / 2;
    C.rows_h = LV_MAX(top - INK_GAP_L - y, 0);
    g_rows = ink_ui_box(g_mark, INK_MARGIN, y, w, LV_MAX(top - INK_GAP_L - y, 0), 0, INK_SURFACE_WHITE, false);
    C.nactions = 0;
    for (int i = 0; i < n; i++) {
        const ink_member_t *m = ink_member_at(i);
        if (!m) break;
        int x = x0 + i % per * cell, row = top + i / per * unit;
        snprintf(C.id[i], sizeof C.id[i], "%s", m->id); snprintf(C.name[i], sizeof C.name[i], "%s", m->name);
        snprintf(C.args[i], sizeof C.args[i], "{\"id\":\"%.15s\"}", m->id);
        C.actions[C.nactions++] = (ink_ui_action_spec_t){.id = C.id[i], .label = C.name[i], .capability = "shell.lock_enter", .args_json = C.args[i], .enabled = true};
        lv_obj_t *b = ink_ui_pill(g_mark, "", x + (cell - INK_CIRCLE) / 2, row, INK_CIRCLE, INK_CIRCLE, !strcmp(m->id, C.owner), avatar_clicked, C.id[i]);
        lv_obj_add_event_cb(b, pressed, LV_EVENT_PRESSED, NULL);
        ink_ui_pill_text(b, INK_FONT_TITLE, INK_WEIGHT_MEDIUM);
        char first[8] = "";
        size_t len = 1;
        while (len < sizeof first - 1 && (m->name[len] & 0xc0) == 0x80) len++;
        memcpy(first, m->name, len);
        lv_label_set_text(lv_obj_get_child(b, 0), first);
        lv_obj_set_style_text_align(ink_ui_line(g_mark, C.name[i], INK_FONT_NOTE, INK_WEIGHT_REGULAR, x, row + INK_CIRCLE + INK_GAP_XS, cell), LV_TEXT_ALIGN_CENTER, 0);
    }
}
static void cover(void) {
    int note = ink_ui_line_height(INK_FONT_NOTE), y = INK_STATUS_BAR + INK_BLOCK;
    char time[24], date[96];
    ink_shell_time_strings(time, sizeof time, date, sizeof date);
    snprintf(C.owner, sizeof C.owner, "%s", ink_member_current()->id);
    lv_obj_remove_flag(ink_shell_statusbar(g_mark, false), LV_OBJ_FLAG_CLICKABLE);
    g_clock = centered(time, INK_FONT_CLOCK, INK_WEIGHT_REGULAR, y); y += ink_ui_line_height(INK_FONT_CLOCK);
    g_date = centered(date, INK_FONT_BODY, INK_WEIGHT_REGULAR, y); y += ink_ui_line_height(INK_FONT_BODY) + INK_GAP_S;
    next_text(C.next, sizeof C.next);
    g_next = centered(C.next, INK_FONT_LIST, INK_WEIGHT_MEDIUM, y); y += ink_ui_line_height(INK_FONT_LIST) + INK_BLOCK;
    g_label = centered(mark_text(), INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_CANVAS_H - INK_MARGIN - note);
    lv_obj_set_style_text_color(g_label, ink_ui_secondary(), 0);
    family(y, INK_CANVAS_H - INK_MARGIN - note - INK_GAP);
    fill_rows(true);
}

static void show(void) {
    if (g_mark) return;
    bool clock = ink_setting_get("display.lock_style") == 1;
    bool image = ink_setting_get("display.lock_style") == 2 && ink_wallpaper_ready();
    int style = (int)ink_setting_get("display.lock_style");
    bool face = style >= INK_LOCK_SIMPLE && ink_lock_face_ready(style); /* a drawn style not ready yet: the default first */
    g_mark = lv_obj_create(lv_layer_sys());
    lv_obj_remove_style_all(g_mark);
    lv_obj_set_size(g_mark, LV_PCT(100), LV_PCT(100));
    lv_obj_remove_flag(g_mark, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(g_mark, LV_OBJ_FLAG_CLICKABLE);
    if (face) {
        lv_obj_set_style_bg_color(g_mark, ink_ui_paper(), 0); lv_obj_set_style_bg_opa(g_mark, LV_OPA_COVER, 0);
        if (ink_setting_get("display.lock_background") && style < INK_LOCK_DRAWN) ink_wallpaper_draw(g_mark);
        ink_lock_face_draw(g_mark, style);
        ink_shell_statusbar(g_mark, false);
        if (style >= INK_LOCK_DRAWN) { /* a drawn style fills the page under the bar: emergency call and camera sit in the bar */
            int ew = ink_ui_text_width(TR("紧急呼叫"), INK_FONT_NOTE, INK_WEIGHT_MEDIUM) + 2 * INK_GAP;
            lv_obj_t *e = ink_ui_pill(g_mark, TR("紧急呼叫"), INK_MARGIN, 0, ew, INK_CONTROL, false, NULL, NULL);
            ink_ui_pill_text(e, INK_FONT_NOTE, INK_WEIGHT_MEDIUM); ink_shell_bind(e, ACTIONS[0].capability, NULL);
            camera_at(INK_MARGIN + ew + INK_GAP, 0);
            ink_ui_overlay_add(&OVERLAY, g_mark); changed(); ink_ui_hint_obj(g_mark, INK_REFRESH_UI); return;
        }
        camera_button(); ink_shell_emergency_button(g_mark, false);
        g_label = ink_ui_notice(g_mark, mark_text(), INK_CANVAS_H - INK_MARGIN - ink_ui_line_height(INK_FONT_NOTE));
        lv_obj_set_style_bg_color(g_label, ink_ui_paper(), 0); lv_obj_set_style_bg_opa(g_label, LV_OPA_COVER, 0);
        ink_ui_overlay_add(&OVERLAY, g_mark); changed(); ink_ui_hint_obj(g_mark, INK_REFRESH_UI); return;
    }
    if (image) {
        ink_wallpaper_draw(g_mark);
        camera_button();
        ink_shell_emergency_button(g_mark, false);
        ink_ui_overlay_add(&OVERLAY, g_mark); changed(); ink_ui_hint_obj(g_mark, INK_REFRESH_FLASH_UI);
        return;
    }
    lv_obj_set_style_bg_color(g_mark, ink_ui_paper(), 0);
    lv_obj_set_style_bg_opa(g_mark, LV_OPA_COVER, 0);
    if (!clock) { /* the default, and a wallpaper not ready: never the page under it */
        cover();
        ink_ui_overlay_add(&OVERLAY, g_mark); changed(); ink_ui_hint_obj(g_mark, INK_REFRESH_UI);
        return;
    }
    int text_w = INK_CANVAS_W - 2 * INK_MARGIN;
    int text_h = ink_ui_text_height(mark_text(), INK_FONT_NOTE, INK_WEIGHT_REGULAR, text_w) + INK_GAP_S;
    int strip_h = text_h > INK_CONTROL ? text_h : INK_CONTROL;
    if (ink_setting_get("display.lock_background")) ink_wallpaper_draw(g_mark);
    ink_shell_statusbar(g_mark, false);
    char time[24], date[96]; ink_shell_time_strings(time, sizeof time, date, sizeof date);
    int date_y = INK_STATUS_BAR + INK_GAP + INK_CONTROL + INK_GAP_M;
    lv_obj_t *date_label = ink_ui_label(g_mark, date, INK_FONT_LIST, INK_WEIGHT_MEDIUM,
                                       INK_MARGIN, date_y, INK_CANVAS_W - 2 * INK_MARGIN);
    lv_obj_set_style_text_align(date_label, LV_TEXT_ALIGN_CENTER, 0);
    g_date = date_label;
    g_clock = ink_ui_label(g_mark, "", INK_FONT_CLOCK, INK_WEIGHT_REGULAR, INK_MARGIN,
                           date_y + ink_ui_line_height(INK_FONT_LIST), INK_CANVAS_W - 2 * INK_MARGIN);
    lv_obj_set_style_text_align(g_clock, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_bg_color(g_date, ink_ui_paper(), 0); lv_obj_set_style_bg_opa(g_date, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(g_clock, ink_ui_paper(), 0); lv_obj_set_style_bg_opa(g_clock, LV_OPA_COVER, 0);
    update_clock();
    int cursor = 0; ink_alarm_t alarm;
    if (ink_alarm_next(&cursor, INK_ALARM, &alarm) == 1) {
        char next[64]; snprintf(next, sizeof next, TR("闹钟 %d:%02d"), alarm.hour, alarm.min);
        g_alarm = ink_ui_notice(g_mark, next, date_y + ink_ui_line_height(INK_FONT_LIST) + ink_ui_line_height(INK_FONT_CLOCK) + INK_GAP_M);
        lv_obj_set_style_bg_color(g_alarm, ink_ui_paper(), 0); lv_obj_set_style_bg_opa(g_alarm, LV_OPA_COVER, 0);
    }
    int h = ink_ui_row_double(), text_y = (h - ink_ui_line_height(INK_FONT_LIST) - ink_ui_line_height(INK_FONT_NOTE)) / 2;
    lv_obj_t *notice = ink_ui_card(g_mark, INK_MARGIN, INK_CANVAS_H - INK_CONTROL - INK_GAP_L - INK_MARGIN - h,
                                   INK_CANVAS_W - 2 * INK_MARGIN, h, INK_SURFACE_WHITE);
    ink_ui_icon(notice, "bell", INK_ICON_LIST, INK_ICON_LINE, INK_GAP, (h - INK_ICON_LIST) / 2, false);
    char count[64]; snprintf(count, sizeof count, TR("%d 条通知"), ink_notify_unread());
    g_notice = ink_ui_line(notice, count, INK_FONT_LIST, INK_WEIGHT_BOLD, INK_ROW_DOUBLE, text_y, INK_CANVAS_W - 2 * INK_ROW_DOUBLE);
    ink_ui_line(notice, TR("唤醒后查看"), INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_ROW_DOUBLE, text_y + ink_ui_line_height(INK_FONT_LIST), INK_CANVAS_W - 2 * INK_ROW_DOUBLE);
    camera_button();
    ink_shell_emergency_button(g_mark, false);
    lv_obj_t *strip = ink_ui_box(g_mark, INK_MARGIN, INK_CANVAS_H - strip_h - INK_GAP_S, INK_CANVAS_W - 2 * INK_MARGIN, strip_h, 0, INK_SURFACE_WHITE, false);
    lv_obj_t *l = ink_ui_label(strip, mark_text(), INK_FONT_NOTE, INK_WEIGHT_REGULAR, 0,
                               (strip_h - ink_ui_line_height(INK_FONT_NOTE)) / 2, text_w);
    lv_obj_align(l, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    g_label = l;
    ink_ui_overlay_add(&OVERLAY, g_mark);
    changed();
    ink_ui_hint_obj(g_mark, INK_REFRESH_UI);
}
static void hide(void) {
    if (!g_mark) return;
    ink_ui_hint_obj(g_mark, g_next ? INK_REFRESH_UI : INK_REFRESH_TEXT);
    ink_ui_cause(g_unlock_cause, "power.unlock_to_visible_ms");
    ink_ui_overlay_remove(&OVERLAY);
    ink_lock_face_clear();
    lv_obj_delete(g_mark); g_mark = g_clock = g_label = g_date = g_alarm = g_notice = g_next = g_rows = NULL;
    C.shown = 0; C.nactions = 0;
}
/* After the unlock a tapped picture goes on as that person: back to the page (D-95), or their PIN on its own page. */
static void enter_pending(void) {
    char id[16];
    snprintf(id, sizeof id, "%s", C.pending); C.pending[0] = 0;
    if (!id[0] || (!strcmp(id, ink_member_current()->id) && ink_member_space_open())) return;
    if (ink_member_has_pin(id)) ink_shell_member_pin_open(id);
    else if (ink_member_switch(id) == 0) ink_ui_home();
}
static void enter(ink_call_t *c) { /* UI task */
    char id[16] = "";
    ink_args_get_str(&c->args, "id", id, sizeof id);
    size_t i = 0;
    while (i < C.nactions && strcmp(C.id[i], id)) i++;
    if (!g_next || i == C.nactions) { c->rc.status = INK_RC_STALE; return; }
    snprintf(C.pending, sizeof C.pending, "%s", id);
    if (!ink_power_unlock(osal_now_us())) { C.pending[0] = 0; c->rc.status = INK_RC_STALE; }
}
static void sync_mark(void *arg) {
    (void)arg;
    if (__atomic_load_n(&g_locked, __ATOMIC_ACQUIRE)) { show(); return; }
    if (g_next) ink_ui_stage_next(); /* the whole page comes back: black and white first, then its grays, as a page turn */
    hide(); ink_lock_face_resume(); enter_pending();
}
static void on_power(void *ctx, ink_power_state_t from, ink_power_state_t to, uint64_t cause_us) {
    (void)ctx;
    if (to == INK_PWR_LOCKED) __atomic_store_n(&g_locked, 1, __ATOMIC_RELEASE);
    else if (from == INK_PWR_LOCKED) { g_unlock_cause = cause_us; __atomic_store_n(&g_locked, 0, __ATOMIC_RELEASE); }
    else return;
    ink_exec_post(INK_EXEC_UI, &g_job, sync_mark, NULL);
}
static void minute_now(void *arg) { (void)arg; update_clock(); update_next(); if (ink_lock_face_update()) changed(); }
static void on_minute(void *ctx, int h, const char *id) {
    (void)ctx; (void)h; (void)id;
    /* Woken from a slice: a wake key's unlock takes the cover off at once, and a clock drawn first holds the page back on the panel. */
    if (ink_power_waking()) { __atomic_store_n(&g_minute_due, true, __ATOMIC_RELEASE); return; }
    minute_now(NULL);
}
static bool minute_due(void) { /* power task, before the chip sleeps again: still locked, the clock catches up now */
    if (!__atomic_exchange_n(&g_minute_due, false, __ATOMIC_ACQ_REL)) return false;
    ink_exec_post(INK_EXEC_UI, &g_minute_job, minute_now, NULL);
    return true;
}
static void on_time(void *ctx, const char *id, const ink_args_t *args) { (void)args; on_minute(ctx, 0, id); }
static void on_member(void *ctx, int h, const char *id) {
    (void)ctx; (void)h;
    if (g_next && !strcmp(id, "member.authed") && !strcmp(C.owner, ink_member_current()->id)) update_next(); /* only the next thing hides */
    else if (g_mark) { hide(); show(); }
    ink_lock_face_resume();
}
/* A drawn style finished: in place (0), instead of the default lock (1), or only the reason under it changed (2). */
static void face_changed(int how) {
    if (!g_mark) return;
    if (how == 0) { changed(); return; }
    if (how == 2) {
        if (!g_label || !strcmp(lv_label_get_text(g_label), mark_text())) return;
        lv_label_set_text(g_label, mark_text()); changed(); ink_ui_hint_obj(g_label, INK_REFRESH_TEXT); return;
    }
    ink_ui_overlay_remove(&OVERLAY); ink_lock_face_clear(); /* not hide(): no unlock is being timed */
    lv_obj_delete(g_mark); g_mark = g_clock = g_label = g_date = g_alarm = g_notice = g_next = g_rows = NULL;
    C.shown = 0; C.nactions = 0;
    show();
}
static void on_style(void *ctx, int h, const char *id) { (void)ctx; (void)h; (void)id; if (g_mark) { hide(); show(); } }
static void on_unread(void *ctx, int h, const char *id) { (void)ctx; (void)h; (void)id; if (g_next) fill_rows(false); }
static void on_audio(void *ctx, int h, const char *id) {
    (void)ctx; (void)h; (void)id;
    if (!g_next || ink_setting_get("display.lock_style") != 0) return;
    if (!strcmp(lv_label_get_text(g_label), mark_text())) return;
    lv_label_set_text(g_label, mark_text());
    changed();
    ink_ui_hint_obj(g_label, INK_REFRESH_FAST);
}
int ink_ui_lock_mark_init(void) {
    static const ink_cap_t enter_cap = {.id = "shell.lock_enter", .title = "在锁屏上点头像进入（仅本人操作，密码仍需验证）", .expose = INK_EXPOSE_OWNER,
        .input = "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\",\"maxLength\":15}},\"required\":[\"id\"]}",
        .effect = INK_FX_NAVIGATE, .child_ok = true, .while_locked = true, .run = enter};
    if (ink_cap_register(&enter_cap, 1) || ink_state_watch("notify.unread", on_unread, NULL, INK_EXEC_UI)) return -1;
    if (ink_state_watch("member.", on_member, NULL, INK_EXEC_UI) || ink_settings_watch("display.lock_", on_style, NULL, INK_EXEC_UI)) return -1;
    if (ink_state_watch("media.state", on_audio, NULL, INK_EXEC_UI) || ink_state_watch("audio.now_playing", on_audio, NULL, INK_EXEC_UI)) return -1;
    if (ink_power_add_busy_check(ink_ui_busy) != 0 || ink_power_add_busy_check(minute_due) != 0 || ink_state_watch("time.minute", on_minute, NULL, INK_EXEC_UI) != 0) return -1;
    if (ink_power_add_busy_check(ink_lock_face_busy) != 0) return -1; /* after ink_ui_busy: a render the UI starts is seen */
    if (ink_cap_subscribe("time.changed", on_time, NULL, INK_EXEC_UI)) return -1;
    ink_lock_face_hook(face_changed);
    return ink_power_subscribe(on_power, NULL);
}
