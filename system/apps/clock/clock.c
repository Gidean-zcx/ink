/* Clock pages use the existing schedule service; UI task only. */
/* ui-scale-runtime: dimension w,x,y,height Theme-derived button coordinates supplied by this page. */
#include "ink/osal.h"
#include "ink/sys/appreg.h"
#include "ink/sys/data.h"
#include "ink/sys/i18n.h"
#include "ink/sys/members.h"
#include "ink/sys/state.h"
#include "ink/sys/textfield.h"
#include "ink/sys/time.h"
#include "ink/sys/interrupt.h"
#include "ink/sys/power.h"
#include "ink/sys/svc.h"
#include "ink/sys/ui_list.h"
#include "ink/ui_shell.h"
#include <stdio.h>
#include <string.h>

static const ink_activity_t CLOCK, DETAIL, TIMER, EXTRAS[2];
static void timer_open(ink_call_t *c);
static struct {
    ink_ui_list_t *list;
    ink_ui_list_position_t position;
    ink_alarm_t draft, rows[3];
    lv_obj_t *name;
    char owner[16], ids[3][24], values[3][160], args[3][48];
    char time[16], repeat[64], next[32], object[24], edit_args[18][64], error[96];
    ink_ui_field_spec_t fields[5];
    ink_ui_action_spec_t actions[18];
} C INK_PSRAM_BSS;
static const char *const DAYS[] = {TR_NOOP("周日"), TR_NOOP("周一"), TR_NOOP("周二"), TR_NOOP("周三"), TR_NOOP("周四"), TR_NOOP("周五"), TR_NOOP("周六")};
static ink_ui_page_spec_t EDIT = {.id = "ClockAlarm", .title = TR_NOOP("闹钟"), .object = {"clock:draft", 1},
    .fields = C.fields, .field_count = 5, .actions = C.actions, .action_count = 18};

static void repeat_text(const ink_alarm_t *a, char *out, size_t cap) {
    unsigned days = a->repeat == INK_REPEAT_WEEKDAYS ? a->weekdays : 0;
    snprintf(out, cap, "%s", days == 127 ? TR("每天") : days == 62 ? TR("工作日") : !days ? TR("只响一次") : "");
    if (days && days != 127 && days != 62)
        for (int i = 0; i < 7; i++) if (days & (1u << i)) {
            size_t n = strlen(out); snprintf(out + n, cap - n, "%s%s", n ? " " : "", TR(DAYS[i]));
        }
}
static bool find_alarm(uint32_t id, ink_alarm_t *out) {
    int cursor = 0;
    while (ink_alarm_next(&cursor, INK_ALARM, out) == 1) if (out->id == id) return true;
    return false;
}
static void clicked(lv_event_t *e) {
    size_t i = (size_t)(uintptr_t)lv_event_get_user_data(e);
    ink_ui_action_target_t target = {ink_ui_content_version(), EDIT.object}; ink_receipt_t rc;
    ink_ui_semantics_action(&EDIT, C.actions[i].id, &target, INK_SRC_TOUCH, &rc);
}
static void action(int i, const char *label, const char *cap, const char *args, bool enabled) {
    snprintf(C.edit_args[i], sizeof C.edit_args[i], "%s", args ? args : "{}");
    C.actions[i] = (ink_ui_action_spec_t){.id = label, .label = TR(label), .capability = cap,
        .args_json = C.edit_args[i], .enabled = enabled};
}
static void button(lv_obj_t *root, int i, int x, int y, int w, bool primary) {
    ink_ui_pill(root, C.actions[i].label, x, y, w, INK_CONTROL, primary, clicked, (void *)(uintptr_t)i);
}
static void name_changed(lv_event_t *e) {
    (void)e; snprintf(C.draft.title, sizeof C.draft.title, "%s", ink_textfield_text(C.name));
}
static void toggled(lv_event_t *e) {
    ink_alarm_t *a = &C.rows[(uintptr_t)lv_event_get_user_data(e)]; char args[64]; ink_receipt_t rc;
    snprintf(args, sizeof args, "{\"id\":%u,\"enabled\":%s}", (unsigned)a->id, a->enabled ? "false" : "true");
    ink_cap_invoke_json("alarm.update", args, INK_SRC_TOUCH, ink_ui_content_version(), &rc);
}
static void bind_row(lv_obj_t *slot, size_t index, ink_ui_field_spec_t *field, ink_ui_action_spec_t *act, void *ctx) {
    (void)ctx;
    unsigned at = (unsigned)(index % 3); ink_alarm_t *a = &C.rows[at]; int cursor = 0;
    for (size_t i = 0; i <= index; i++) if (ink_alarm_next(&cursor, INK_ALARM, a) != 1) return;
    char time[16], repeat[64]; repeat_text(a, repeat, sizeof repeat);
    snprintf(time, sizeof time, "%u:%02u", a->hour, a->min);
    snprintf(C.ids[at], sizeof C.ids[at], "alarm:%u", (unsigned)a->id);
    snprintf(C.args[at], sizeof C.args[at], "{\"id\":%u}", (unsigned)a->id);
    char title[sizeof a->title]; /* the row lives in C too: copy it out before writing into C */
    snprintf(title, sizeof title, "%s", a->title);
    snprintf(C.values[at], sizeof C.values[at], "%s · %s · %s · %s", time, title, repeat, a->enabled ? TR("开启") : TR("关闭"));
    *field = (ink_ui_field_spec_t){.id = C.ids[at], .role = "listitem", .label = C.ids[at], .value = C.values[at], .object = {C.ids[at], 1}};
    *act = (ink_ui_action_spec_t){.id = C.ids[at], .label = TR("查看闹钟"), .capability = "clock.open", .args_json = C.args[at], .object = field->object, .enabled = true};
    int w = INK_CANVAS_W - 2 * INK_MARGIN;
    if (!lv_obj_get_child_count(slot)) {
        ink_ui_toggle(slot, "", a->enabled, w - INK_GAP - INK_PRIMARY, INK_GAP_L, INK_PRIMARY, toggled, (void *)(uintptr_t)at);
        ink_ui_line(slot, "", INK_FONT_HEADING, INK_WEIGHT_REGULAR, INK_GAP_L, INK_GAP_XS, w - INK_PRIMARY - 2 * INK_GAP_L);
        ink_ui_line(slot, "", INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_GAP_L, INK_LINE_HEADING + INK_GAP_XS, w - INK_PRIMARY - 2 * INK_GAP_L);
    }
    char note[128]; snprintf(note, sizeof note, "%s%s%s", a->title, a->title[0] ? " · " : "", repeat);
    lv_label_set_text(lv_obj_get_child(slot, 1), time); lv_label_set_text(lv_obj_get_child(slot, 2), note);
    lv_obj_t *track = lv_obj_get_child(lv_obj_get_child(slot, 0), 1), *knob = lv_obj_get_child(track, 0);
    lv_obj_set_style_bg_color(track, a->enabled ? ink_ui_ink() : ink_ui_paper(), 0);
    lv_obj_set_style_bg_color(knob, a->enabled ? ink_ui_paper() : ink_ui_ink(), 0);
    lv_obj_set_x(knob, ink_ui_px(a->enabled ? INK_PRIMARY - INK_RADIUS_PANEL + INK_GAP_XS : INK_GAP_XS));
}
static void create_list(lv_obj_t *screen, void *arg) {
    const ink_ui_action_spec_t LIST_ACTIONS[] = {
        {.id = "prev", .label = TR("上一页"), .capability = "clock.prev"},
        {.id = "next", .label = TR("下一页"), .capability = "clock.next"},
        {.id = "new", .label = TR("加闹钟"), .capability = "clock.new", .enabled = true}
    };
    const ink_ui_page_spec_t LIST = {.id = "Clock", .title = TR("时钟"), .object = {"clock:list", 1},
        .actions = LIST_ACTIONS, .action_count = 3};
    (void)arg;
    if (strcmp(C.owner, ink_member_current()->id)) { memset(&C.position, 0, sizeof C.position); snprintf(C.owner, sizeof C.owner, "%s", ink_member_current()->id); }
    ink_shell_statusbar(screen, true); ink_shell_topbar(screen, TR("时钟"));
    ink_alarm_t a; int cursor = 0; size_t total = 0; int64_t next = 0;
    while (ink_alarm_next(&cursor, INK_ALARM, &a) == 1) { total++; if (a.enabled && a.next_s && (!next || a.next_s < next)) next = a.next_s; }
    int y = INK_STATUS_BAR + INK_PRIMARY + INK_GAP;
    const char *tabs[] = {TR("闹钟"), TR("计时"), TR("秒表"), TR("世界时钟")};
    lv_obj_t *seg = ink_ui_segments(screen, tabs, 4, 0, INK_MARGIN, y, INK_CANVAS_W - 2 * INK_MARGIN, NULL, NULL);
    int used = 0, tab_x = INK_GAP_XS - INK_STROKE;
    for (int i = 0; i < 4; i++) used += ink_ui_text_width(tabs[i], INK_FONT_NOTE, INK_WEIGHT_REGULAR) + 2 * INK_GAP_S;
    for (int i = 0; i < 4; i++) {
        int width = ink_ui_text_width(tabs[i], INK_FONT_NOTE, INK_WEIGHT_REGULAR) + 2 * INK_GAP_S + (INK_CANVAS_W - 2 * INK_MARGIN - 2 * INK_GAP_XS - used) / 4;
        lv_obj_t *tab = lv_obj_get_child(seg, i); ink_ui_pill_text(tab, INK_FONT_NOTE, INK_WEIGHT_REGULAR);
        lv_obj_set_x(tab, ink_ui_px(tab_x)); lv_obj_set_width(tab, ink_ui_px(width)); tab_x += width;
    }
    ink_shell_bind(lv_obj_get_child(seg, 2), "clock.stopwatch", "{}"); ink_shell_bind(lv_obj_get_child(seg, 3), "clock.world", "{}");
    ink_shell_bind(lv_obj_get_child(seg, 1), "clock.timer", "{}"); y += INK_SEGMENT + INK_GAP;
    ink_tm_t t; ink_time_to_local(next, &t); char text[32];
    if (next) snprintf(text, sizeof text, "%d:%02d", t.hour, t.min); else snprintf(text, sizeof text, "%s", total ? TR("没有已开启的闹钟") : TR("还没有闹钟"));
    snprintf(C.next, sizeof C.next, "%s", text);
    char summary[80]; snprintf(summary, sizeof summary, TR("下一个闹钟：%s"), text);
    ink_ui_line(screen, summary, INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_MARGIN, y, INK_CANVAS_W - 2 * INK_MARGIN);
    y += INK_LINE_NOTE + INK_GAP;
    lv_obj_t *body = ink_ui_box(screen, 0, y, INK_CANVAS_W, INK_CANVAS_H - y - INK_MARGIN - INK_PRIMARY, 0, INK_SURFACE_WHITE, false);
    C.list = ink_ui_list_create(body, &LIST, &C.position, 3, 1, INK_DIGIT, TR("还没有闹钟，点下面加一个"), bind_row, NULL);
    if (C.list) ink_ui_list_refresh(C.list, total);
    lv_obj_t *add = ink_ui_pill(screen, TR("+ 加闹钟"), INK_MARGIN, INK_CANVAS_H - INK_MARGIN - INK_PRIMARY,
        INK_CANVAS_W - 2 * INK_MARGIN, INK_PRIMARY, true, NULL, NULL);
    ink_shell_bind(add, "clock.new", "{}");
}
static void create_detail(lv_obj_t *screen, void *arg) {
    (void)arg; memset(C.actions, 0, sizeof C.actions); EDIT.title = TR("闹钟");
    snprintf(C.object, sizeof C.object, "alarm:%u", (unsigned)C.draft.id); EDIT.object.id = C.object;
    EDIT.actions = C.actions; EDIT.action_count = C.draft.id ? 18 : 16;
    ink_shell_statusbar(screen, true); ink_shell_topbar(screen, C.draft.id ? TR("闹钟") : TR("加闹钟"));
    snprintf(C.time, sizeof C.time, "%02u:%02u", C.draft.hour, C.draft.min); repeat_text(&C.draft, C.repeat, sizeof C.repeat);
    C.fields[0] = (ink_ui_field_spec_t){.id = "time", .role = "text", .label = TR("时间"), .value = C.time};
    C.fields[1] = (ink_ui_field_spec_t){.id = "repeat", .role = "text", .label = TR("重复"), .value = C.repeat};
    C.fields[2] = (ink_ui_field_spec_t){.id = "name", .role = "textbox", .label = TR("名称"), .value = C.draft.title};
    C.fields[3] = (ink_ui_field_spec_t){.id = "enabled", .role = "text", .label = TR("开关"), .value = C.draft.enabled ? TR("开启") : TR("关闭")};
    C.fields[4] = (ink_ui_field_spec_t){.id = "error", .role = "status", .label = TR("提示"), .value = C.error};
    int y = INK_STATUS_BAR + INK_PRIMARY + INK_GAP, w = INK_CANVAS_W - 2 * INK_MARGIN;
    ink_ui_line(screen, C.time, INK_FONT_HEADING, INK_WEIGHT_REGULAR, INK_MARGIN + INK_CONTROL, y, w - 2 * INK_CONTROL);
    if (C.draft.id) {
        char args[48]; snprintf(args, sizeof args, "{\"id\":%u}", (unsigned)C.draft.id);
        action(16, TR_NOOP("删除闹钟"), "alarm.cancel", args, true);
        action(17, C.draft.enabled ? TR_NOOP("关闭") : TR_NOOP("开启"), "clock.draft", C.draft.enabled ? "{\"enabled\":false}" : "{\"enabled\":true}", true);
    }
    const char *labels[] = {TR_NOOP("时-"), TR_NOOP("时＋"), TR_NOOP("分-"), TR_NOOP("分＋")};
    for (int i = 0; i < 4; i++) {
        char args[48]; int value = i < 2 ? (C.draft.hour + (i ? 1 : 23)) % 24 : (C.draft.min + (i == 3 ? 1 : 59)) % 60;
        snprintf(args, sizeof args, "{\"%s\":%d}", i < 2 ? "hour" : "minute", value);
        action(i, labels[i], "clock.draft", args, true);
        button(screen, i, INK_MARGIN + i * (w + INK_GAP_M) / 4, y + INK_CONTROL, (w - 3 * INK_GAP_M) / 4, false);
    }
    y += 2 * INK_CONTROL + INK_GAP;
    action(4, TR_NOOP("输入名称"), "clock.focus", NULL, true);
    ink_text_cfg_t cfg = {.type = INK_TEXT_PLAIN, .enter = INK_ENTER_DONE, .initial = C.draft.title, .hint = TR("闹钟名称"), .max = sizeof C.draft.title - 1};
    C.name = ink_textfield_create(screen, &cfg, NULL, NULL, false);
    lv_obj_set_y(C.name, ink_ui_px(y));
    ink_textfield_style(C.name, INK_FIELD_SINGLE, TR("闹钟名称"));
    ink_textfield_bind_short(C.name, &EDIT, &C.fields[2], &C.actions[4]); lv_obj_add_event_cb(C.name, name_changed, LV_EVENT_VALUE_CHANGED, NULL);
    y += ink_textfield_height(INK_FIELD_SINGLE) + INK_GAP;
    const char *presets[] = {TR_NOOP("只响一次"), TR_NOOP("每天"), TR_NOOP("工作日")}; const int masks[] = {0, 127, 62};
    for (int i = 0; i < 3; i++) {
        char args[48]; snprintf(args, sizeof args, "{\"weekdays\":%d}", masks[i]); action(5 + i, presets[i], "clock.draft", args, true);
        button(screen, 5 + i, INK_MARGIN + i * (w + INK_GAP_M) / 3, y, (w - 2 * INK_GAP_M) / 3, C.draft.weekdays == masks[i]);
    }
    y += INK_CONTROL + INK_GAP;
    for (int i = 0; i < 7; i++) {
        char args[48]; snprintf(args, sizeof args, "{\"weekdays\":%u}", C.draft.weekdays ^ (1u << i)); action(8 + i, DAYS[i], "clock.draft", args, true);
        button(screen, 8 + i, INK_MARGIN + (i % 4) * (w + INK_GAP_M) / 4, y + (i / 4) * (INK_CONTROL + INK_GAP_M), (w - 3 * INK_GAP_M) / 4, C.draft.weekdays & (1u << i));
    }
    if (C.draft.id) {
        button(screen, 17, INK_MARGIN + 3 * (w + INK_GAP_M) / 4, y + INK_CONTROL + INK_GAP_M, (w - 3 * INK_GAP_M) / 4, false);
        button(screen, 16, INK_MARGIN, INK_CANVAS_H - INK_MARGIN - INK_CONTROL, (w - INK_GAP) / 2, false);
    }
    ink_ui_notice(screen, C.error, y + 2 * (INK_CONTROL + INK_GAP_M));
    action(15, TR_NOOP("保存闹钟"), "clock.save", NULL, true);
    button(screen, 15, INK_MARGIN + (C.draft.id ? (w + INK_GAP) / 2 : 0), INK_CANVAS_H - INK_MARGIN - INK_CONTROL, C.draft.id ? (w - INK_GAP) / 2 : w, true);
}
static void destroy_list(void) { C.list = NULL; }
static void destroy_detail(void) { C.name = NULL; }
static int list_snapshot(char *out, size_t cap) {
    int n = C.list ? ink_ui_list_snapshot(C.list, out, cap) : -1;
    if (n < 0) return -1;
    int more = snprintf(out + n - 1, cap - (size_t)n + 1, ",\"next_alarm\":\"%s\",\"timer_action\":\"clock.timer\",\"stopwatch_action\":\"clock.stopwatch\",\"world_action\":\"clock.world\"}", C.next);
    if (more < 0 || (size_t)more >= cap - (size_t)n + 1) { out[0] = 0; return -1; }
    return n - 1 + more;
}
static int detail_snapshot(char *out, size_t cap) {
    size_t actions = EDIT.action_count;
    if (C.name && ink_text_focused()) EDIT.action_count = 0; /* The keyboard owns input. */
    int n = C.name ? ink_textfield_short_snapshot(C.name, out, cap) : ink_ui_semantics_snapshot(&EDIT, out, cap);
    EDIT.action_count = actions; return n;
}
static bool list_key(ink_key_t key, bool lng) { return C.list && ink_ui_list_key(C.list, key, lng); }
static const ink_activity_t CLOCK = {.name = "Clock", .app = "clock", .create = create_list, .destroy = destroy_list, .snapshot = list_snapshot, .on_key = list_key};
static const ink_activity_t DETAIL = {.name = "ClockAlarm", .app = "clock", .create = create_detail, .destroy = destroy_detail, .snapshot = detail_snapshot};

/* Volatile, member-owned countdowns; the shared service timer also runs off-page. */
typedef struct {
    char owner[16];
    uint64_t left, end;
    bool running;
    int ringing;
    ink_svc_deadline_t due;
} countdown_t;
static countdown_t timers[INK_MEMBERS_MAX] INK_PSRAM_BSS;
static ink_exec_job_t timer_redraw;
static char timer_text[48] INK_PSRAM_BSS, timer_status[32] INK_PSRAM_BSS;
enum { TIMER_ADJUST = 4, TIMER_ACTIONS = 8 + TIMER_ADJUST };
static lv_obj_t *timer_digits;
static lv_timer_t *timer_ui_tick;
static char timer_adjust_args[TIMER_ADJUST][32] INK_PSRAM_BSS;
static ink_ui_field_spec_t timer_fields[3] INK_PSRAM_BSS;
static ink_ui_action_spec_t timer_actions[TIMER_ACTIONS] INK_PSRAM_BSS;
static ink_ui_page_spec_t timer_page = {.id = "ClockTimer", .title = TR_NOOP("计时"), .object = {"clock:timer", 1}, .fields = timer_fields, .field_count = 3, .actions = timer_actions, .action_count = TIMER_ACTIONS};
static countdown_t *my_timer(void) { /* UI task */
    for (int i = 0; i < INK_MEMBERS_MAX; i++) if (!strcmp(timers[i].owner, ink_member_current()->id)) return &timers[i];
    for (int i = 0; i < INK_MEMBERS_MAX; i++) if (!timers[i].owner[0]) {
        snprintf(timers[i].owner, sizeof timers[i].owner, "%s", ink_member_current()->id); timers[i].left = 300000000;
        return &timers[i];
    }
    return NULL;
}
static uint64_t remaining(countdown_t *t) {
    uint64_t now = osal_now_us();
    return t->running ? (t->end > now ? t->end - now : 0) : t->left;
}
static void timer_format(uint64_t left) {
    unsigned sec = (unsigned)((left + 999999) / 1000000);
    if (sec >= 3600) snprintf(timer_text, sizeof timer_text, "%u:%02u:%02u", sec / 3600, sec / 60 % 60, sec % 60);
    else snprintf(timer_text, sizeof timer_text, "%u:%02u", sec / 60, sec % 60);
}
static void timer_refresh(void *arg) {
    (void)arg;
    if (!ink_ui_page_current() || strcmp(ink_ui_page_name(), "ClockTimer") || ink_power_state() == INK_PWR_LOCKED) return;
    countdown_t *t = my_timer();
    ink_svc_lock(); uint64_t left = remaining(t); bool running = t->running; ink_svc_unlock();
    if (!running || !left) { ink_ui_redraw(); return; }
    timer_format(left);
    lv_label_set_text(timer_digits, timer_text);
    ink_ui_hint_obj(timer_digits, INK_REFRESH_FAST);
    ink_ui_content_changed();
}
static void timer_ui_refresh(lv_timer_t *tick) { (void)tick; timer_refresh(NULL); }
static void timer_destroy(void) {
    if (timer_ui_tick) lv_timer_delete(timer_ui_tick);
    timer_ui_tick = NULL; timer_digits = NULL;
}
static void timer_tick(void *arg) {
    countdown_t *t = arg;
    ink_svc_lock();
    bool active = t->running; uint64_t left = remaining(t), end = t->end;
    if (active && !left) { t->running = false; t->left = 0; }
    ink_svc_unlock();
    if (!active) return;
    if (left) ink_svc_at(&t->due, end, timer_tick, t);
    else {
        ink_interrupt_t it = {.kind = INK_INT_ALARM, .ringing = true, .nactions = 2,
            .actions = {{.label = TR("关闭"), .cap = "clock.timer_dismiss"}, {.label = TR("知道了"), .cap = "clock.timer_dismiss"}}};
        snprintf(it.title, sizeof it.title, "%s", TR("计时结束")); snprintf(it.body, sizeof it.body, "%s", TR("设定的时间到了"));
        for (int i = 0; i < 2; i++) snprintf(it.actions[i].args, sizeof it.actions[i].args, "{\"id\":%d}", (int)(t - timers));
        if (ink_member_exists(t->owner)) t->ringing = ink_interrupt_raise(&it);
        ink_power_release(INK_NEED_AWAKE);
    }
    if (!left) ink_exec_post(INK_EXEC_UI, &timer_redraw, timer_refresh, NULL);
}
static void timer_open(ink_call_t *c) { (void)c; ink_ui_push(&TIMER, NULL); }
static void timer_dismiss(ink_call_t *c) {
    int64_t id = 0; ink_args_get_int(&c->args, "id", &id);
    ink_interrupt_end(timers[id].ringing); timers[id].ringing = 0;
}
static void timer_change(ink_call_t *c) {
    countdown_t *t = my_timer(); int64_t seconds = 0;
    if (!t) { c->rc.status = INK_RC_FAILED; return; }
    ink_svc_lock();
    bool was = t->running; uint64_t left = remaining(t);
    if (!strcmp(c->cap->id, "clock.timer_set")) {
        if (was) c->rc.status = INK_RC_REJECTED;
        else { ink_args_get_int(&c->args, "seconds", &seconds); t->left = (uint64_t)seconds * 1000000; t->end = 0; }
    } else if (!strcmp(c->cap->id, "clock.timer_start")) {
        if (!was && t->left) { t->running = true; t->end = osal_now_us() + t->left; }
        else c->rc.status = INK_RC_REJECTED;
    } else { t->running = false; t->left = !strcmp(c->cap->id, "clock.timer_pause") ? left : 300000000; if (strcmp(c->cap->id, "clock.timer_pause")) t->end = 0; }
    bool running = t->running;
    ink_svc_unlock();
    if (!was && running) ink_power_hold(INK_NEED_AWAKE);
    if (was && !running) ink_power_release(INK_NEED_AWAKE);
    if (running) timer_tick(t); else ink_svc_cancel(&t->due);
    ink_ui_redraw();
}
static void create_timer(lv_obj_t *screen, void *arg) {
    (void)arg; timer_page.title = TR("计时"); countdown_t *t = my_timer(); if (!t) return;
    ink_svc_lock(); uint64_t left = remaining(t); bool running = t->running, started = t->end != 0; ink_svc_unlock();
    timer_format(left);
    snprintf(timer_status, sizeof timer_status, "%s", running ? TR("正在计时") : started ? (left ? TR("已暂停") : TR("计时结束")) : TR("设定时长"));
    timer_fields[0] = (ink_ui_field_spec_t){.id = "remaining", .label = TR("剩余时间"), .role = "text", .value = timer_text};
    timer_fields[1] = (ink_ui_field_spec_t){.id = "status", .label = TR("状态"), .role = "text", .value = timer_status};
    timer_fields[2] = (ink_ui_field_spec_t){.id = "restart", .label = TR("重新启动"), .role = "text", .value = started ? TR("重新启动会取消计时") : TR("重新启动后，之前的计时已取消")};
    ink_shell_statusbar(screen, true); ink_shell_topbar(screen, TR("时钟")); int w = INK_CANVAS_W - 2 * INK_MARGIN, y = INK_STATUS_BAR + INK_PRIMARY + INK_GAP;
    const char *tabs[] = {TR("闹钟"), TR("计时")}; lv_obj_t *seg = ink_ui_segments(screen, tabs, 2, 1, INK_MARGIN, y, w, NULL, NULL);
    ink_shell_bind(lv_obj_get_child(seg, 0), "shell.back", "{}"); y += INK_SEGMENT + INK_GAP_L;
    ink_ui_line(screen, timer_status, INK_FONT_SUBTITLE, INK_WEIGHT_MEDIUM, INK_MARGIN, y, w); y += INK_LINE_SUBTITLE + INK_GAP;
    timer_digits = ink_ui_line(screen, timer_text, INK_FONT_NUMBER, INK_WEIGHT_REGULAR, INK_MARGIN, y, w); y += INK_LINE_NUMBER + INK_GAP_L;
    if (running) timer_ui_tick = lv_timer_create(timer_ui_refresh, 1000, NULL);
    static const char *const adjust[] = {TR_NOOP("分－"), TR_NOOP("分＋"), TR_NOOP("秒－"), TR_NOOP("秒＋")};
    static const int delta[] = {-60, 60, -1, 1};
    _Static_assert(sizeof adjust / sizeof *adjust <= TIMER_ADJUST && sizeof delta / sizeof *delta <= TIMER_ADJUST, "timer controls fit");
    for (unsigned i = 0; i < TIMER_ADJUST; i++) {
        int sec = (int)((left + 999999) / 1000000) + delta[i];
        bool enabled = !running && sec >= 1 && sec <= 86400;
        snprintf(timer_adjust_args[i], sizeof timer_adjust_args[i], "{\"seconds\":%d}", enabled ? sec : 1);
        timer_actions[8 + i] = (ink_ui_action_spec_t){.id = adjust[i], .label = TR(adjust[i]), .capability = "clock.timer_set", .args_json = timer_adjust_args[i], .enabled = enabled};
        lv_obj_t *b = ink_ui_pill(screen, TR(adjust[i]), INK_MARGIN + (int)i * (w + INK_GAP_M) / TIMER_ADJUST, y, (w - (TIMER_ADJUST - 1) * INK_GAP_M) / TIMER_ADJUST, INK_CONTROL, false, NULL, NULL);
        ink_shell_bind(b, "clock.timer_set", timer_adjust_args[i]);
        if (!enabled) lv_obj_remove_flag(b, LV_OBJ_FLAG_CLICKABLE);
    }
    y += INK_CONTROL + INK_GAP;
    const char *labels[] = {TR("1 分钟"), TR("5 分钟"), TR("15 分钟"), TR("30 分钟"), started && !running && left ? TR("继续") : TR("开始"), TR("暂停"), TR("取消"), TR("闹钟")};
    const char *caps[] = {"clock.timer_set", "clock.timer_set", "clock.timer_set", "clock.timer_set", "clock.timer_start", "clock.timer_pause", "clock.timer_cancel", "shell.back"};
    const char *args[] = {"{\"seconds\":60}", "{\"seconds\":300}", "{\"seconds\":900}", "{\"seconds\":1800}", "{}", "{}", "{}", "{}"};
    int base = INK_CANVAS_H - INK_MARGIN - 3 * (INK_CONTROL + INK_GAP);
    for (int i = 0; i < 8; i++) {
        bool enabled = i < 4 ? !running : i == 4 ? !running && left : i == 5 ? running : true;
        timer_actions[i] = (ink_ui_action_spec_t){.id = labels[i], .label = labels[i], .capability = caps[i], .args_json = args[i], .enabled = enabled};
        if (i < 7) {
            int cols = i < 4 ? 2 : 3, col = i < 4 ? i % 2 : i - 4, row = i < 4 ? i / 2 : 2;
            lv_obj_t *b = ink_ui_pill(screen, labels[i], INK_MARGIN + col * (w + INK_GAP_M) / cols, base + row * (INK_CONTROL + INK_GAP), (w - (cols - 1) * INK_GAP_M) / cols, INK_CONTROL, i == (running ? 5 : 4), NULL, NULL);
            ink_shell_bind(b, caps[i], args[i]); if (!enabled) lv_obj_remove_flag(b, LV_OBJ_FLAG_CLICKABLE);
        }
    }
    ink_ui_notice(screen, timer_fields[2].value, y);
}
static int timer_snapshot(char *out, size_t cap) {
    int n = ink_ui_semantics_snapshot(&timer_page, out, cap); if (n < 0) return n;
    countdown_t *t = my_timer(); ink_svc_lock(); uint64_t left = remaining(t); ink_svc_unlock();
    int k = snprintf(out + n - 1, cap - (size_t)n + 1, ",\"remaining_seconds\":%llu}", (unsigned long long)((left + 999999) / 1000000));
    return k < 0 || (size_t)k >= cap - (size_t)n + 1 ? -1 : n - 1 + k;
}
static const ink_activity_t TIMER = {.name = "ClockTimer", .app = "clock", .create = create_timer, .destroy = timer_destroy, .snapshot = timer_snapshot};

/* Durations are volatile and member-owned, like countdowns; only visible digits tick. */
enum { EXTRA_ROWS = 3, EXTRA_FIELDS = EXTRA_ROWS + 1, EXTRA_ACTIONS = EXTRA_ROWS + 3, LAPS_MAX = 99 };
typedef struct { char owner[16]; uint64_t elapsed, since, laps[LAPS_MAX]; unsigned count; bool running; } stopwatch_t;
static stopwatch_t watches[INK_MEMBERS_MAX] INK_PSRAM_BSS;
static const ink_db_def_t WORLD = {.name = "clock.world", .title = TR_NOOP("世界时钟"), .scope = INK_DB_MEMBER,
    .schema = 1, .json = true, .ai_read = true, .max_records = 6};
static struct {
    bool world, picking; unsigned page, total, visible;
    ink_db_t *db; stopwatch_t *watch; lv_timer_t *tick;
    lv_obj_t *digits, *times[EXTRA_ROWS]; int zones[EXTRA_ROWS];
    char values[EXTRA_FIELDS][128], ids[EXTRA_FIELDS][24], args[EXTRA_ACTIONS][32], action_ids[EXTRA_ACTIONS][16];
    ink_ui_field_spec_t fields[EXTRA_FIELDS]; ink_ui_action_spec_t actions[EXTRA_ACTIONS]; ink_ui_page_spec_t model;
} X INK_PSRAM_BSS;
_Static_assert(EXTRA_ROWS + 1 <= EXTRA_FIELDS && EXTRA_ROWS + 3 <= EXTRA_ACTIONS, "extra page rows fit");
static stopwatch_t *my_watch(void) {
    for (int i = 0; i < INK_MEMBERS_MAX; i++) if (!strcmp(watches[i].owner, ink_member_current()->id)) return &watches[i];
    for (int i = 0; i < INK_MEMBERS_MAX; i++) if (!watches[i].owner[0]) {
        snprintf(watches[i].owner, sizeof watches[i].owner, "%s", ink_member_current()->id); return &watches[i];
    }
    return NULL;
}
static uint64_t elapsed(stopwatch_t *s) { return s->elapsed + (s->running ? osal_now_us() - s->since : 0); }
static void duration(char *out, size_t cap, uint64_t us, bool tenths) {
    uint64_t sec = us / 1000000; char fraction[8] = "";
    if (tenths) snprintf(fraction, sizeof fraction, ".%u", (unsigned)(us / 100000 % 10));
    snprintf(out, cap, "%02llu:%02u:%02u%s", (unsigned long long)(sec / 3600), (unsigned)(sec / 60 % 60), (unsigned)(sec % 60), fraction);
}
static int city_at(unsigned index) {
    for (int z = 0; z < ink_time_zone_count(); z++) if (X.picking || ink_db_get(X.db, (uint32_t)z + 1, NULL, NULL, 0) == 0) {
        if (!index--) return z;
    }
    return -1;
}
static void world_text(int z, char *out, size_t cap) {
    if (!ink_time_valid()) { snprintf(out, cap, "%s", TR("时间尚未设置")); return; }
    int64_t utc = ink_time_now_s(); ink_tm_t here, there; ink_time_to_local(utc, &here); ink_time_to_zone(z, utc, &there);
    int day = (there.year * 10000 + there.month * 100 + there.day) - (here.year * 10000 + here.month * 100 + here.day);
    snprintf(out, cap, TR("%02d:%02d · %+d 小时 · %s"), there.hour, there.min, (there.offset_min - here.offset_min) / 60,
        day > 0 ? TR("第二天") : day < 0 ? TR("前一天") : TR("同一天"));
}
static bool extra_current(void) { return ink_ui_page_current() && !strcmp(ink_ui_page_name(), X.world ? "ClockWorld" : "ClockStopwatch"); }
static void extra_update(void) {
    if (!extra_current() || ink_power_state() == INK_PWR_LOCKED) return;
    if (!X.world && X.watch && X.digits) {
        duration(X.values[0], sizeof X.values[0], elapsed(X.watch), !X.watch->running);
        lv_label_set_text(X.digits, X.values[0]); ink_ui_hint_obj(X.digits, INK_REFRESH_FAST);
    }
    if (X.world) for (unsigned i = 0; i < X.visible; i++) {
        world_text(X.zones[i], X.values[i + 1], sizeof X.values[i + 1]);
        lv_label_set_text(X.times[i], X.values[i + 1]); ink_ui_hint_obj(X.times[i], INK_REFRESH_FAST);
    }
    ink_ui_content_changed();
}
static void stopwatch_tick(lv_timer_t *t) { (void)t; extra_update(); }
static void extra_changed(void *ctx, int h, const char *id) { (void)ctx; (void)h; (void)id; if (X.world) extra_update(); }
static void extra_time_changed(void *ctx, const char *id, const ink_args_t *a) { (void)a; extra_changed(ctx, 0, id); }
static void extra_button(lv_obj_t *parent, const char *label, const char *cap, const char *args, bool enabled, int x, int y, int w, int height, bool primary) {
    unsigned i = (unsigned)X.model.action_count++;
    snprintf(X.args[i], sizeof X.args[i], "%s", args ? args : "{}");
    snprintf(X.action_ids[i], sizeof X.action_ids[i], "action:%u", i);
    X.actions[i] = (ink_ui_action_spec_t){.id = X.action_ids[i], .label = label, .capability = cap, .args_json = X.args[i], .enabled = enabled};
    lv_obj_t *b = ink_ui_pill(parent, label, x, y, w, height, primary, NULL, NULL);
    if (enabled) ink_shell_bind(b, cap, X.args[i]); else lv_obj_add_state(b, LV_STATE_DISABLED);
}
static void extra_create(lv_obj_t *screen, void *arg) {
    X.world = (intptr_t)arg != 0; X.digits = NULL; X.visible = 0; X.watch = X.world ? NULL : my_watch();
    X.model = (ink_ui_page_spec_t){.id = X.world ? "ClockWorld" : "ClockStopwatch", .title = X.world ? TR("世界时钟") : TR("秒表"), .object = {X.world ? "clock:world" : "clock:stopwatch", 1}, .fields = X.fields, .actions = X.actions};
    ink_shell_statusbar(screen, true); ink_shell_topbar(screen, X.model.title);
    int w = INK_CANVAS_W - 2 * INK_MARGIN, y = INK_STATUS_BAR + INK_PRIMARY + INK_GAP;
    if (X.world) { X.db = ink_db_open_member(&WORLD, ink_member_current()->id); X.total = X.picking ? (unsigned)ink_time_zone_count() : (unsigned)ink_db_count(X.db); }
    else {
        if (!X.watch) return;
        X.total = X.watch->count;
        snprintf(X.values[0], sizeof X.values[0], "%s", X.watch->running ? TR("正在计时") : X.watch->elapsed ? TR("已暂停") : TR("未开始"));
        ink_ui_line(screen, X.values[0], INK_FONT_SUBTITLE, INK_WEIGHT_MEDIUM, INK_MARGIN, y, w); y += INK_LINE_SUBTITLE + INK_GAP;
        duration(X.values[0], sizeof X.values[0], elapsed(X.watch), !X.watch->running);
        X.digits = ink_ui_line(screen, X.values[0], INK_FONT_NUMBER, INK_WEIGHT_REGULAR, INK_MARGIN, y, w); y += INK_LINE_NUMBER + INK_GAP;
        const char *labels[] = {X.watch->running ? TR("暂停") : X.watch->elapsed ? TR("继续") : TR("开始"), TR("计次"), TR("清零")};
        const char *caps[] = {X.watch->running ? "clock.stopwatch_pause" : "clock.stopwatch_start", "clock.stopwatch_lap", "clock.stopwatch_reset"};
        _Static_assert(sizeof labels / sizeof *labels == EXTRA_ROWS && sizeof caps / sizeof *caps == EXTRA_ROWS, "stopwatch controls fit");
        for (int i = 0; i < EXTRA_ROWS; i++) extra_button(screen, labels[i], caps[i], NULL, i != 1 || (X.watch->running && X.total < LAPS_MAX), INK_MARGIN + i * (w + INK_GAP_M) / EXTRA_ROWS, y, (w - 2 * INK_GAP_M) / EXTRA_ROWS, INK_CONTROL, i == 0);
        y += INK_CONTROL + INK_GAP;
        if (X.watch->running) X.tick = lv_timer_create(stopwatch_tick, 1000, NULL);
    }
    if (X.world && !X.db) { ink_ui_notice(screen, TR("没能读取城市，请返回重试"), y); return; }
    if (X.page * EXTRA_ROWS >= X.total) X.page = X.total ? (X.total - 1) / EXTRA_ROWS : 0;
    X.fields[0] = (ink_ui_field_spec_t){.id = "elapsed", .role = "text", .label = X.world ? TR("世界时钟") : TR("秒表"), .value = X.world ? "" : X.values[0]};
    for (unsigned i = 0, at = X.page * EXTRA_ROWS; i < EXTRA_ROWS && at + i < X.total; i++) {
        int height = X.world ? INK_DIGIT : INK_CIRCLE_SMALL;
        lv_obj_t *row = ink_ui_card(screen, INK_MARGIN, y + (int)i * (height + INK_GAP_M), w, height, INK_SURFACE_WHITE);
        if (!X.world) { lv_obj_set_style_radius(row, 0, 0); lv_obj_set_style_border_width(row, 0, 0); }
        const char *label; char args[32];
        if (X.world) {
            int z = city_at(at + i); X.zones[i] = z; label = TR(ink_time_zone_city(z));
            snprintf(X.ids[i + 1], sizeof X.ids[i + 1], "%s", ink_time_zone_name(z)); world_text(z, X.values[i + 1], sizeof X.values[i + 1]);
            snprintf(args, sizeof args, "{\"zone\":%d}", z);
            bool selected = ink_db_get(X.db, (uint32_t)z + 1, NULL, NULL, 0) == 0;
            extra_button(row, X.picking ? selected ? TR("已选择") : TR("加入") : TR("删除"), X.picking ? "clock.world_add" : "clock.world_remove", args,
                !X.picking || (!selected && ink_db_count(X.db) < 6), w - INK_DIGIT - INK_GAP_M, INK_GAP_M, INK_DIGIT, INK_CIRCLE_SMALL, false);
        } else {
            snprintf(X.ids[i + 1], sizeof X.ids[i + 1], TR("第 %u 次"), at + i + 1); label = X.ids[i + 1];
            duration(X.values[i + 1], sizeof X.values[i + 1], X.watch->laps[at + i], true);
        }
        X.fields[i + 1] = (ink_ui_field_spec_t){.id = X.ids[i + 1], .label = label, .role = "listitem", .value = X.values[i + 1]};
        ink_ui_line(row, label, X.world ? INK_FONT_LIST : INK_FONT_NOTE, INK_WEIGHT_MEDIUM, INK_GAP_M, INK_GAP_M, X.world ? w - INK_DIGIT - 3 * INK_GAP_M : INK_PRIMARY);
        X.times[i] = ink_ui_line(row, X.values[i + 1], INK_FONT_NOTE, INK_WEIGHT_REGULAR, X.world ? INK_GAP_M : INK_PRIMARY + INK_GAP_M, X.world ? INK_CIRCLE_SMALL + 2 * INK_GAP_M : INK_GAP_M, w - (X.world ? 2 * INK_GAP_M : INK_PRIMARY + 2 * INK_GAP_M)); X.visible++;
    }
    X.model.field_count = X.visible + 1;
    int bottom = INK_CANVAS_H - INK_MARGIN - INK_CONTROL;
    extra_button(screen, TR("上一页"), "clock.extra_prev", NULL, X.page > 0, INK_MARGIN, bottom - INK_CONTROL - INK_GAP, (w - INK_GAP) / 2, INK_CONTROL, false);
    extra_button(screen, TR("下一页"), "clock.extra_next", NULL, (X.page + 1) * EXTRA_ROWS < X.total, INK_MARGIN + (w + INK_GAP) / 2, bottom - INK_CONTROL - INK_GAP, (w - INK_GAP) / 2, INK_CONTROL, false);
    if (X.world) { ink_ui_notice(screen, TR("最多选6个城市"), bottom - 2 * (INK_CONTROL + INK_GAP)); extra_button(screen, X.picking ? TR("选好了") : TR("加城市"), "clock.world_pick", NULL, true, INK_MARGIN, bottom, w, INK_CONTROL, true); }
    else ink_ui_notice(screen, TR("重新启动后秒表会清零"), bottom);
    if (!X.total) ink_ui_notice(screen, X.world ? TR("还没有城市，点下面加一个") : TR("最多计次99条"), y);
}
static void extra_destroy(void) {
    if (X.tick) lv_timer_delete(X.tick);
    X.tick = NULL; X.digits = NULL; ink_db_close(X.db); X.db = NULL; X.visible = 0;
}
static int extra_snapshot(char *out, size_t cap) {
    int n = ink_ui_semantics_snapshot(&X.model, out, cap); if (n < 0) return n;
    int k = snprintf(out + n - 1, cap - (size_t)n + 1, ",\"page_number\":%u,\"total\":%u,\"picking\":%s,\"elapsed_ms\":%llu,\"elapsed_seconds\":%llu,\"running\":%s}", X.page + 1, X.total,
        X.world && X.picking ? "true" : "false", (unsigned long long)(!X.world && X.watch ? elapsed(X.watch) / 1000 : 0), (unsigned long long)(!X.world && X.watch ? elapsed(X.watch) / 1000000 : 0), !X.world && X.watch && X.watch->running ? "true" : "false");
    return k < 0 || (size_t)k >= cap - (size_t)n + 1 ? -1 : n - 1 + k;
}
static void extra_change(ink_call_t *c) {
    const char *id = c->cap->id;
    if (!strcmp(id, "clock.stopwatch") || !strcmp(id, "clock.world")) {
        X.page = 0; X.picking = false; bool world = !strcmp(id, "clock.world"); ink_ui_push(&EXTRAS[world], (void *)(intptr_t)world); return;
    }
    if (!extra_current()) { c->rc.status = INK_RC_STALE; return; }
    if (!strcmp(id, "clock.extra_prev")) { if (X.page) X.page--; }
    else if (!strcmp(id, "clock.extra_next")) { if ((X.page + 1) * EXTRA_ROWS < X.total) X.page++; }
    else if (!strcmp(id, "clock.world_pick")) { X.picking = !X.picking; X.page = 0; }
    else if (X.world) {
        int64_t z = 0; ink_args_get_int(&c->args, "zone", &z); char json[32]; snprintf(json, sizeof json, "{\"zone\":%d}", (int)z);
        ink_db_write_t w = {.id = (uint32_t)z + 1, .sort = z, .data = json, .len = strlen(json), .src = c->src};
        if (!X.db || (!strcmp(id, "clock.world_add") ? ink_db_put(X.db, &w, NULL) : ink_db_del(X.db, w.id, c->src))) { c->rc.status = INK_RC_FAILED; return; }
    } else {
        stopwatch_t *s = my_watch(); if (!s) { c->rc.status = INK_RC_FAILED; return; }
        uint64_t us = elapsed(s);
        if (!strcmp(id, "clock.stopwatch_start")) { if (!s->running) { s->since = osal_now_us(); s->running = true; } }
        else if (!strcmp(id, "clock.stopwatch_pause")) { s->elapsed = us; s->running = false; }
        else if (!strcmp(id, "clock.stopwatch_lap")) { if (s->running && s->count < LAPS_MAX) s->laps[s->count++] = us; else { c->rc.status = INK_RC_REJECTED; return; } }
        else { s->elapsed = 0; s->since = 0; s->running = false; s->count = 0; X.page = 0; }
    }
    ink_ui_redraw();
}
static const ink_activity_t EXTRAS[2] = {
    {.name = "ClockStopwatch", .app = "clock", .create = extra_create, .destroy = extra_destroy, .snapshot = extra_snapshot},
    {.name = "ClockWorld", .app = "clock", .create = extra_create, .destroy = extra_destroy, .snapshot = extra_snapshot}};
static void navigate(ink_call_t *c) {
    if (!strcmp(c->cap->id, "clock.prev") || !strcmp(c->cap->id, "clock.next")) { if (C.list) ink_ui_list_turn(C.list, !strcmp(c->cap->id, "clock.next") ? 1 : -1); return; }
    C.draft = (ink_alarm_t){.kind = INK_ALARM, .hour = 7, .enabled = true};
    C.error[0] = 0;
    if (!strcmp(c->cap->id, "clock.open")) { int64_t id = 0; ink_args_get_int(&c->args, "id", &id); if (!find_alarm((uint32_t)id, &C.draft)) { c->rc.status = INK_RC_STALE; return; } }
    ink_ui_push(&DETAIL, NULL);
}
static void draft(ink_call_t *c) {
    int64_t v;
    if (ink_args_get_int(&c->args, "hour", &v)) C.draft.hour = (uint8_t)v;
    if (ink_args_get_int(&c->args, "minute", &v)) C.draft.min = (uint8_t)v;
    if (ink_args_get_int(&c->args, "weekdays", &v)) { C.draft.weekdays = (uint8_t)v; C.draft.repeat = v ? INK_REPEAT_WEEKDAYS : INK_REPEAT_NONE; }
    ink_args_get_bool(&c->args, "enabled", &C.draft.enabled);
    ink_ui_redraw();
}
static void focus(ink_call_t *c) { if (!C.name || ink_textfield_short_focus(C.name)) c->rc.status = INK_RC_STALE; }
static void save(ink_call_t *c) {
    ink_args_t *args = osal_calloc(1, sizeof *args, OSAL_MEM_EXTERNAL);
    if (!args) { c->rc.status = INK_RC_FAILED; return; }
    ink_args_init(args); ink_args_int(args, "hour", C.draft.hour); ink_args_int(args, "minute", C.draft.min);
    ink_args_int(args, "weekdays", C.draft.weekdays); ink_args_str(args, "title", C.draft.title);
    if (C.draft.id) { ink_args_int(args, "id", C.draft.id); ink_args_bool(args, "enabled", C.draft.enabled); }
    ink_cap_invoke(C.draft.id ? "alarm.update" : "alarm.set", args, c->src, 0, &c->rc); osal_free(args);
    if (c->rc.status == INK_RC_DONE) ink_ui_pop();
    else { snprintf(C.error, sizeof C.error, "%s", c->rc.status == INK_RC_INVALID ? TR("名称最多15个字，请缩短后保存") : TR("没能保存，请重试")); ink_ui_redraw(); }
}
static const ink_cap_t CAPS[] = {
    {.id = "clock.stopwatch", .app = "clock", .title = TR_NOOP("秒表"), .effect = INK_FX_NAVIGATE, .child_ok = true, .run = extra_change},
    {.id = "clock.world", .app = "clock", .title = TR_NOOP("世界时钟"), .effect = INK_FX_NAVIGATE, .child_ok = true, .run = extra_change},
    {.id = "clock.stopwatch_start", .app = "clock", .title = TR_NOOP("开始"), .activity = "ClockStopwatch", .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .run = extra_change},
    {.id = "clock.stopwatch_pause", .app = "clock", .title = TR_NOOP("暂停"), .activity = "ClockStopwatch", .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .run = extra_change},
    {.id = "clock.stopwatch_lap", .app = "clock", .title = TR_NOOP("计次"), .activity = "ClockStopwatch", .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .run = extra_change},
    {.id = "clock.stopwatch_reset", .app = "clock", .title = TR_NOOP("清零"), .activity = "ClockStopwatch", .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .run = extra_change},
    {.id = "clock.world_pick", .app = "clock", .title = TR_NOOP("加城市"), .activity = "ClockWorld", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = extra_change},
    {.id = "clock.world_add", .app = "clock", .title = TR_NOOP("加城市"), .activity = "ClockWorld", .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .input = "{\"type\":\"object\",\"properties\":{\"zone\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":8}},\"required\":[\"zone\"]}", .run = extra_change},
    {.id = "clock.world_remove", .app = "clock", .title = TR_NOOP("删除"), .activity = "ClockWorld", .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .input = "{\"type\":\"object\",\"properties\":{\"zone\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":8}},\"required\":[\"zone\"]}", .run = extra_change},
    {.id = "clock.extra_prev", .app = "clock", .title = TR_NOOP("上一页"), .effect = INK_FX_NAVIGATE, .child_ok = true, .run = extra_change},
    {.id = "clock.extra_next", .app = "clock", .title = TR_NOOP("下一页"), .effect = INK_FX_NAVIGATE, .child_ok = true, .run = extra_change},
    {.id = "clock.timer_dismiss", .app = "clock", .title = "关闭计时响铃", .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .while_locked = true, .exec = INK_EXEC_DIRECT, .input = "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":7}},\"required\":[\"id\"]}", .run = timer_dismiss},
    {.id = "clock.timer", .app = "clock", .title = "计时", .activity = "Clock", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = timer_open},
    {.id = "clock.timer_set", .app = "clock", .title = "设定计时时长", .activity = "ClockTimer", .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .input = "{\"type\":\"object\",\"properties\":{\"seconds\":{\"type\":\"integer\",\"minimum\":1,\"maximum\":86400}},\"required\":[\"seconds\"]}", .run = timer_change},
    {.id = "clock.timer_start", .app = "clock", .title = "开始或继续计时", .activity = "ClockTimer", .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .run = timer_change},
    {.id = "clock.timer_pause", .app = "clock", .title = "暂停计时", .activity = "ClockTimer", .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .run = timer_change},
    {.id = "clock.timer_cancel", .app = "clock", .title = "取消计时", .activity = "ClockTimer", .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .run = timer_change},
    {.id = "clock.prev", .app = "clock", .title = "闹钟上一页", .activity = "Clock", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "clock.next", .app = "clock", .title = "闹钟下一页", .activity = "Clock", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "clock.new", .app = "clock", .title = "加闹钟", .activity = "Clock", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "clock.open", .app = "clock", .title = "查看闹钟", .activity = "Clock", .effect = INK_FX_NAVIGATE, .child_ok = true,
     .input = "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"integer\",\"minimum\":1}},\"required\":[\"id\"]}", .run = navigate},
    {.id = "clock.draft", .app = "clock", .title = "选择闹钟的时间、重复日期和开关", .activity = "ClockAlarm", .effect = INK_FX_NAVIGATE, .child_ok = true,
     .input = "{\"type\":\"object\",\"properties\":{\"hour\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":23},\"minute\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":59},\"weekdays\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":127},\"enabled\":{\"type\":\"boolean\"}}}", .run = draft},
    {.id = "clock.focus", .app = "clock", .title = "输入闹钟名称", .activity = "ClockAlarm", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = focus},
    {.id = "clock.save", .app = "clock", .title = "保存闹钟", .activity = "ClockAlarm", .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .run = save}
};
static void changed(void *ctx, int h, const char *id) {
    (void)ctx; (void)h; (void)id;
    if (!ink_ui_page_current()) return;
    if (!strcmp(ink_ui_page_name(), "Clock")) ink_ui_redraw();
    else if (!strcmp(ink_ui_page_name(), "ClockAlarm") && C.draft.id) {
        ink_alarm_t current; if (!find_alarm(C.draft.id, &current)) ink_ui_pop();
    }
}
int ink_clock_init(void) {
    return ink_db_register(&WORLD) || ink_cap_register(CAPS, sizeof CAPS / sizeof *CAPS) || ink_cap_subscribe("time.changed", extra_time_changed, NULL, INK_EXEC_UI) || ink_state_watch("time.", extra_changed, NULL, INK_EXEC_UI) || ink_state_watch("alarm.revision", changed, NULL, INK_EXEC_UI) ? -1 : 0;
}
const ink_app_def_t ink_appdef_clock = {.id = "clock", .title = TR_NOOP("时钟"), .icon = INK_UI_ICON(clock), .entry = &CLOCK, .system_app = true, .child_ok = true};
