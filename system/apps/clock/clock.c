/* Clock pages use the existing schedule service; UI task only. */
/* ui-scale-runtime: dimension w,x,y Theme-derived button coordinates supplied by this page. */
#include "ink/osal.h"
#include "ink/sys/appreg.h"
#include "ink/sys/members.h"
#include "ink/sys/state.h"
#include "ink/sys/textfield.h"
#include "ink/sys/time.h"
#include "ink/sys/ui_list.h"
#include "ink/ui_shell.h"
#include <stdio.h>
#include <string.h>

static const ink_activity_t CLOCK, DETAIL;
static struct {
    ink_ui_list_t *list;
    ink_ui_list_position_t position;
    ink_alarm_t draft, rows[3];
    lv_obj_t *name;
    char owner[16], ids[3][24], values[3][160], args[3][48];
    char time[16], repeat[64], next[32], object[24], edit_args[17][64];
    ink_ui_field_spec_t fields[4];
    ink_ui_action_spec_t actions[17];
} C INK_PSRAM_BSS;
static const char *const DAYS[] = {"周日", "周一", "周二", "周三", "周四", "周五", "周六"};
static const ink_ui_action_spec_t LIST_ACTIONS[] = {
    {.id = "prev", .label = "上一页", .capability = "clock.prev"},
    {.id = "next", .label = "下一页", .capability = "clock.next"},
    {.id = "new", .label = "加闹钟", .capability = "clock.new", .enabled = true}
};
static const ink_ui_page_spec_t LIST = {.id = "Clock", .title = "时钟", .object = {"clock:list", 1},
    .actions = LIST_ACTIONS, .action_count = 3};
static ink_ui_page_spec_t EDIT = {.id = "ClockAlarm", .title = "闹钟", .object = {"clock:draft", 1},
    .fields = C.fields, .field_count = 4, .actions = C.actions, .action_count = 17};

static void repeat_text(const ink_alarm_t *a, char *out, size_t cap) {
    unsigned days = a->repeat == INK_REPEAT_WEEKDAYS ? a->weekdays : 0;
    snprintf(out, cap, "%s", days == 127 ? "每天" : days == 62 ? "工作日" : !days ? "只响一次" : "");
    if (days && days != 127 && days != 62)
        for (int i = 0; i < 7; i++) if (days & (1u << i)) {
            size_t n = strlen(out); snprintf(out + n, cap - n, "%s%s", n ? " " : "", DAYS[i]);
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
    C.actions[i] = (ink_ui_action_spec_t){.id = label, .label = label, .capability = cap,
        .args_json = C.edit_args[i], .enabled = enabled};
}
static void button(lv_obj_t *root, int i, int x, int y, int w, bool primary) {
    ink_ui_pill(root, C.actions[i].label, x, y, w, INK_CONTROL, primary, clicked, (void *)(uintptr_t)i);
}
static void name_changed(lv_event_t *e) {
    (void)e; snprintf(C.draft.title, sizeof C.draft.title, "%s", ink_textfield_text(C.name));
}
static void bind_row(lv_obj_t *slot, size_t index, ink_ui_field_spec_t *field, ink_ui_action_spec_t *act, void *ctx) {
    (void)ctx;
    unsigned at = (unsigned)(index % 3); ink_alarm_t *a = &C.rows[at]; int cursor = 0;
    for (size_t i = 0; i <= index; i++) if (ink_alarm_next(&cursor, INK_ALARM, a) != 1) return;
    char time[16], repeat[64]; repeat_text(a, repeat, sizeof repeat);
    snprintf(time, sizeof time, "%u:%02u", a->hour, a->min);
    snprintf(C.ids[at], sizeof C.ids[at], "alarm:%u", (unsigned)a->id);
    snprintf(C.args[at], sizeof C.args[at], "{\"id\":%u}", (unsigned)a->id);
    snprintf(C.values[at], sizeof C.values[at], "%s · %s · %s · %s", time, a->title, repeat, a->enabled ? "开启" : "关闭");
    *field = (ink_ui_field_spec_t){.id = C.ids[at], .role = "listitem", .label = C.ids[at], .value = C.values[at], .object = {C.ids[at], 1}};
    *act = (ink_ui_action_spec_t){.id = C.ids[at], .label = "查看闹钟", .capability = "clock.open", .args_json = C.args[at], .object = field->object, .enabled = true};
    int w = INK_CANVAS_W - 2 * INK_MARGIN;
    if (!lv_obj_get_child_count(slot)) {
        ink_ui_toggle(slot, "", a->enabled, INK_GAP, INK_GAP_L, w - 2 * INK_GAP, NULL, NULL);
        ink_ui_line(slot, "", INK_FONT_HEADING, INK_WEIGHT_REGULAR, INK_GAP_L, INK_GAP_XS, w - INK_PRIMARY - 2 * INK_GAP_L);
        ink_ui_line(slot, "", INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_GAP_L, INK_LINE_HEADING + INK_GAP_XS, w - INK_PRIMARY - 2 * INK_GAP_L);
    }
    char note[112]; snprintf(note, sizeof note, "%s%s%s", a->title, a->title[0] ? " · " : "", repeat);
    lv_label_set_text(lv_obj_get_child(slot, 1), time); lv_label_set_text(lv_obj_get_child(slot, 2), note);
    lv_obj_t *track = lv_obj_get_child(lv_obj_get_child(slot, 0), 1), *knob = lv_obj_get_child(track, 0);
    lv_obj_set_style_bg_color(track, a->enabled ? ink_ui_ink() : ink_ui_paper(), 0);
    lv_obj_set_style_bg_color(knob, a->enabled ? ink_ui_paper() : ink_ui_ink(), 0);
    lv_obj_set_x(knob, ink_ui_px(a->enabled ? INK_PRIMARY - INK_RADIUS_PANEL + INK_GAP_XS : INK_GAP_XS));
}
static void create_list(lv_obj_t *screen, void *arg) {
    (void)arg;
    if (strcmp(C.owner, ink_member_current()->id)) { memset(&C.position, 0, sizeof C.position); snprintf(C.owner, sizeof C.owner, "%s", ink_member_current()->id); }
    ink_shell_statusbar(screen, true); ink_shell_topbar(screen, "时钟");
    ink_alarm_t a; int cursor = 0; size_t total = 0; int64_t next = 0;
    while (ink_alarm_next(&cursor, INK_ALARM, &a) == 1) { total++; if (a.enabled && a.next_s && (!next || a.next_s < next)) next = a.next_s; }
    int y = INK_STATUS_BAR + INK_PRIMARY + INK_GAP;
    ink_ui_notice(screen, "下一个闹钟", y);
    ink_tm_t t; ink_time_to_local(next, &t); char text[32];
    if (next) snprintf(text, sizeof text, "%d:%02d", t.hour, t.min); else snprintf(text, sizeof text, "%s", total ? "没有已开启的闹钟" : "还没有闹钟");
    snprintf(C.next, sizeof C.next, "%s", text);
    ink_ui_line(screen, text, next ? INK_FONT_NUMBER : INK_FONT_SUBTITLE, INK_WEIGHT_REGULAR, INK_MARGIN, y + INK_LINE_NOTE, INK_CANVAS_W - 2 * INK_MARGIN);
    y += INK_LINE_NOTE + INK_LINE_NUMBER + INK_GAP;
    lv_obj_t *body = ink_ui_box(screen, 0, y, INK_CANVAS_W, INK_CANVAS_H - y - INK_MARGIN - INK_PRIMARY, 0, INK_SURFACE_WHITE, false);
    C.list = ink_ui_list_create(body, &LIST, &C.position, 3, 1, INK_DIGIT, "还没有闹钟，点下面加一个", bind_row, NULL);
    if (C.list) ink_ui_list_refresh(C.list, total);
    lv_obj_t *add = ink_ui_pill(screen, "+ 加闹钟", INK_MARGIN, INK_CANVAS_H - INK_MARGIN - INK_PRIMARY,
        INK_CANVAS_W - 2 * INK_MARGIN, INK_PRIMARY, true, NULL, NULL);
    ink_shell_bind(add, "clock.new", "{}");
}
static void create_detail(lv_obj_t *screen, void *arg) {
    (void)arg; memset(C.actions, 0, sizeof C.actions);
    snprintf(C.object, sizeof C.object, "alarm:%u", (unsigned)C.draft.id); EDIT.object.id = C.object;
    EDIT.actions = C.draft.id ? &C.actions[16] : C.actions; EDIT.action_count = C.draft.id ? 1 : 16;
    ink_shell_statusbar(screen, true); ink_shell_topbar(screen, C.draft.id ? "闹钟" : "加闹钟");
    snprintf(C.time, sizeof C.time, "%02u:%02u", C.draft.hour, C.draft.min); repeat_text(&C.draft, C.repeat, sizeof C.repeat);
    C.fields[0] = (ink_ui_field_spec_t){.id = "time", .role = "text", .label = "时间", .value = C.time};
    C.fields[1] = (ink_ui_field_spec_t){.id = "repeat", .role = "text", .label = "重复", .value = C.repeat};
    C.fields[2] = (ink_ui_field_spec_t){.id = "name", .role = "textbox", .label = "名称", .value = C.draft.title};
    C.fields[3] = (ink_ui_field_spec_t){.id = "enabled", .role = "text", .label = "开关", .value = C.draft.enabled ? "开启" : "关闭"};
    int y = INK_STATUS_BAR + INK_PRIMARY + INK_GAP, w = INK_CANVAS_W - 2 * INK_MARGIN;
    ink_ui_line(screen, C.time, INK_FONT_HEADING, INK_WEIGHT_REGULAR, INK_MARGIN + INK_CONTROL, y, w - 2 * INK_CONTROL);
    if (C.draft.id) {
        ink_ui_line(screen, C.draft.title, INK_FONT_SUBTITLE, INK_WEIGHT_MEDIUM, INK_MARGIN, y + INK_PRIMARY, w);
        ink_ui_line(screen, C.repeat, INK_FONT_LIST, INK_WEIGHT_REGULAR, INK_MARGIN, y + 2 * INK_PRIMARY, w);
        ink_ui_notice(screen, "暂不支持修改和开关闹钟", y + 3 * INK_PRIMARY);
        char args[48]; snprintf(args, sizeof args, "{\"id\":%u}", (unsigned)C.draft.id);
        action(16, "删除闹钟", "alarm.cancel", args, true);
        button(screen, 16, INK_MARGIN, INK_CANVAS_H - INK_MARGIN - INK_CONTROL, w, false); return;
    }
    const char *labels[] = {"时−", "时＋", "分−", "分＋"};
    for (int i = 0; i < 4; i++) {
        char args[48]; int value = i < 2 ? (C.draft.hour + (i ? 1 : 23)) % 24 : (C.draft.min + (i == 3 ? 1 : 59)) % 60;
        snprintf(args, sizeof args, "{\"%s\":%d}", i < 2 ? "hour" : "minute", value);
        action(i, labels[i], "clock.draft", args, true);
        button(screen, i, INK_MARGIN + i * (w + INK_GAP_M) / 4, y + INK_CONTROL, (w - 3 * INK_GAP_M) / 4, false);
    }
    y += 2 * INK_CONTROL + INK_GAP;
    action(4, "输入名称", "clock.focus", NULL, true);
    ink_text_cfg_t cfg = {.type = INK_TEXT_PLAIN, .enter = INK_ENTER_DONE, .initial = C.draft.title, .hint = "闹钟名称", .max = sizeof C.draft.title};
    C.name = ink_textfield_create(screen, &cfg, NULL, NULL, false);
    lv_obj_set_pos(C.name, ink_ui_px(INK_MARGIN), ink_ui_px(y)); lv_obj_set_width(C.name, ink_ui_px(w));
    ink_textfield_bind_short(C.name, &EDIT, &C.fields[2], &C.actions[4]); lv_obj_add_event_cb(C.name, name_changed, LV_EVENT_VALUE_CHANGED, NULL);
    y += INK_CONTROL + INK_GAP;
    const char *presets[] = {"只响一次", "每天", "工作日"}; const int masks[] = {0, 127, 62};
    for (int i = 0; i < 3; i++) {
        char args[48]; snprintf(args, sizeof args, "{\"weekdays\":%d}", masks[i]); action(5 + i, presets[i], "clock.draft", args, true);
        button(screen, 5 + i, INK_MARGIN + i * (w + INK_GAP_M) / 3, y, (w - 2 * INK_GAP_M) / 3, C.draft.weekdays == masks[i]);
    }
    y += INK_CONTROL + INK_GAP;
    for (int i = 0; i < 7; i++) {
        char args[48]; snprintf(args, sizeof args, "{\"weekdays\":%u}", C.draft.weekdays ^ (1u << i)); action(8 + i, DAYS[i], "clock.draft", args, true);
        button(screen, 8 + i, INK_MARGIN + (i % 4) * (w + INK_GAP_M) / 4, y + (i / 4) * (INK_CONTROL + INK_GAP_M), (w - 3 * INK_GAP_M) / 4, C.draft.weekdays & (1u << i));
    }
    action(15, "保存闹钟", "clock.save", NULL, true);
    button(screen, 15, INK_MARGIN, INK_CANVAS_H - INK_MARGIN - INK_CONTROL, w, true);
}
static void destroy_list(void) { C.list = NULL; }
static void destroy_detail(void) { C.name = NULL; }
static int list_snapshot(char *out, size_t cap) {
    int n = C.list ? ink_ui_list_snapshot(C.list, out, cap) : -1;
    if (n < 0) return -1;
    int more = snprintf(out + n - 1, cap - (size_t)n + 1, ",\"next_alarm\":\"%s\"}", C.next);
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
static void navigate(ink_call_t *c) {
    if (!strcmp(c->cap->id, "clock.prev") || !strcmp(c->cap->id, "clock.next")) { if (C.list) ink_ui_list_turn(C.list, !strcmp(c->cap->id, "clock.next") ? 1 : -1); return; }
    C.draft = (ink_alarm_t){.kind = INK_ALARM, .hour = 7, .enabled = true};
    if (!strcmp(c->cap->id, "clock.open")) { int64_t id = 0; ink_args_get_int(&c->args, "id", &id); if (!find_alarm((uint32_t)id, &C.draft)) { c->rc.status = INK_RC_STALE; return; } }
    ink_ui_push(&DETAIL, NULL);
}
static void draft(ink_call_t *c) {
    if (C.draft.id) { c->rc.status = INK_RC_REJECTED; return; }
    int64_t v;
    if (ink_args_get_int(&c->args, "hour", &v)) C.draft.hour = (uint8_t)v;
    if (ink_args_get_int(&c->args, "minute", &v)) C.draft.min = (uint8_t)v;
    if (ink_args_get_int(&c->args, "weekdays", &v)) { C.draft.weekdays = (uint8_t)v; C.draft.repeat = v ? INK_REPEAT_WEEKDAYS : INK_REPEAT_NONE; }
    ink_ui_redraw();
}
static void focus(ink_call_t *c) { if (!C.name || ink_textfield_short_focus(C.name)) c->rc.status = INK_RC_STALE; }
static void save(ink_call_t *c) {
    if (C.draft.id) { c->rc.status = INK_RC_REJECTED; return; }
    ink_args_t *args = osal_calloc(1, sizeof *args, OSAL_MEM_EXTERNAL);
    if (!args) { c->rc.status = INK_RC_FAILED; return; }
    ink_args_init(args); ink_args_int(args, "hour", C.draft.hour); ink_args_int(args, "minute", C.draft.min);
    ink_args_int(args, "weekdays", C.draft.weekdays); ink_args_str(args, "title", C.draft.title);
    ink_cap_invoke("alarm.set", args, c->src, 0, &c->rc); osal_free(args);
    if (c->rc.status == INK_RC_DONE) ink_ui_pop();
}
static const ink_cap_t CAPS[] = {
    {.id = "clock.prev", .title = "闹钟上一页", .activity = "Clock", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "clock.next", .title = "闹钟下一页", .activity = "Clock", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "clock.new", .title = "加闹钟", .activity = "Clock", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "clock.open", .title = "查看闹钟", .activity = "Clock", .effect = INK_FX_NAVIGATE, .child_ok = true,
     .input = "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"integer\",\"minimum\":1}},\"required\":[\"id\"]}", .run = navigate},
    {.id = "clock.draft", .title = "选择新闹钟的时间和重复日期", .activity = "ClockAlarm", .effect = INK_FX_NAVIGATE, .child_ok = true,
     .input = "{\"type\":\"object\",\"properties\":{\"hour\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":23},\"minute\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":59},\"weekdays\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":127}}}", .run = draft},
    {.id = "clock.focus", .title = "输入闹钟名称", .activity = "ClockAlarm", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = focus},
    {.id = "clock.save", .title = "保存新闹钟", .activity = "ClockAlarm", .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .run = save}
};
static void changed(void *ctx, int h, const char *id) {
    (void)ctx; (void)h; (void)id;
    if (!ink_ui_page_current()) return;
    if (!strcmp(ink_ui_page_name(), "Clock")) ink_ui_redraw();
    else if (!strcmp(ink_ui_page_name(), "ClockAlarm") && C.draft.id) {
        if (find_alarm(C.draft.id, &C.draft)) ink_ui_redraw(); else ink_ui_pop();
    }
}
int ink_clock_init(void) {
    return ink_cap_register(CAPS, sizeof CAPS / sizeof *CAPS) || ink_state_watch("alarm.revision", changed, NULL, INK_EXEC_UI) ? -1 : 0;
}
const ink_app_def_t ink_appdef_clock = {.id = "clock", .title = "时钟", .entry = &CLOCK, .system_app = true, .child_ok = true};
