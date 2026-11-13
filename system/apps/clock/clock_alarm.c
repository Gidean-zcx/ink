/* The alarm tab and editor after Fossify Clock (对照表 C, D): alarms stay the time service's, through alarm.*. UI task only. */
/* ui-scale-runtime: dimension w,x,y,h,row,bw,ph,spare Theme-derived coordinates supplied by this page. */
#include "clock_internal.h"
#include <stdlib.h>

enum { ROWS = 4, ALARM_TABLE = 64 /* MAX_ALARMS in time_service.c: alarms and reminders of the whole family */ };
static const uint8_t PRESET[] = {0, 127, 62};
static const char *const PRESET_LABEL[] = {TR_NOOP("只响一次"), TR_NOOP("每天"), TR_NOOP("工作日")};
static struct {
    ink_alarm_t draft;
    char error[96], next[96];
    struct { uint32_t id; bool on; lv_obj_t *slot; int field; } row[ROWS];
    unsigned rows, first, per, count;
    lv_obj_t *next_line, *summary, *preset[3], *day[7];
    int next_field, time_field, repeat_field;
} A INK_PSRAM_BSS;
static const ink_activity_t EDITOR;

static bool find(uint32_t id, ink_alarm_t *out) {
    int cursor = 0;
    while (ink_alarm_next(&cursor, INK_ALARM, out) == 1) if (out->id == id) return true;
    return false;
}
/* 每天, 工作日, 周六、周日 (Monday first); one-off: 今天 or 明天 on the list (未设置 when off), 只响一次 · 明天 in the editor */
static void repeat_text(const ink_alarm_t *a, char *out, size_t cap, bool editor) {
    unsigned m = a->repeat == INK_REPEAT_WEEKDAYS ? a->weekdays : 0;
    out[0] = 0;
    if (m == 127 || m == 62) { snprintf(out, cap, "%s", m == 62 ? TR("工作日") : TR("每天")); return; }
    for (int i = 1; i <= 7; i++) if (m & (1u << (i % 7))) { size_t n = strlen(out); snprintf(out + n, cap - n, "%s%s", n ? TR("、") : "", ck_wday(i)); }
    if (m) return;
    int64_t at = a->next_s;
    if (editor) {
        ink_tm_t t; int64_t now = ink_time_now_s(); ink_time_to_local(now, &t);
        t.hour = ck_edit_get(CK_HOUR); t.min = ck_edit_get(CK_MINUTE); t.sec = 0; at = ink_time_from_local(&t);
        if (at <= now) at += 86400;
    }
    if (editor) snprintf(out, cap, TR("只响一次 · %s"), ck_day(at));
    else snprintf(out, cap, "%s", a->enabled && at ? ck_day(at) : TR("未设置"));
}
const char *ck_next_alarm(bool left) { /* "周一 7:00，还有 9 小时", "" when none is on; from the records' next_s */
    int cursor = 0; ink_alarm_t a; int64_t next = 0;
    while (ink_alarm_next(&cursor, INK_ALARM, &a) == 1) if (a.enabled && a.next_s && (!next || a.next_s < next)) next = a.next_s;
    A.next[0] = 0;
    if (!next || !ink_time_valid()) return A.next;
    ink_tm_t t; ink_time_to_local(next, &t);
    long long minutes = (next - ink_time_now_s() + 59) / 60; char rest[48] = "";
    if (left && minutes >= 60) snprintf(rest, sizeof rest, TR("，还有 %lld 小时"), minutes / 60); /* past an hour, hours only */
    else if (left) snprintf(rest, sizeof rest, TR("，还有 %lld 分钟"), minutes);
    snprintf(A.next, sizeof A.next, "%s %d:%02d%s", ck_day(next), t.hour, t.min, rest);
    return A.next;
}
static void toggled(lv_event_t *e) {
    unsigned r = (unsigned)(uintptr_t)lv_event_get_user_data(e); char args[64]; ink_receipt_t rc;
    snprintf(args, sizeof args, "{\"id\":%u,\"enabled\":%s}", (unsigned)A.row[r].id, A.row[r].on ? "false" : "true");
    ink_cap_invoke_json("alarm.update", args, INK_SRC_TOUCH, ink_ui_content_version(), &rc);
}
static void row_fill(unsigned r, const ink_alarm_t *a) { /* in place: the switch only when it changed, labels only when they changed */
    lv_obj_t *s = A.row[r].slot; int w = INK_CANVAS_W - 2 * INK_MARGIN;
    char time[8], repeat[64], note[128];
    snprintf(time, sizeof time, "%u:%02u", a->hour, a->min); repeat_text(a, repeat, sizeof repeat, false);
    snprintf(note, sizeof note, "%s%s%s", a->title, a->title[0] ? " · " : "", repeat);
    if (!lv_obj_get_child_count(s) || A.row[r].on != a->enabled) {
        if (lv_obj_get_child_count(s)) lv_obj_delete(lv_obj_get_child(s, 0));
        lv_obj_t *t = ink_ui_toggle(s, "", a->enabled, w - INK_GAP - INK_PRIMARY, INK_GAP_L, INK_PRIMARY, toggled, (void *)(uintptr_t)r);
        lv_obj_move_to_index(t, 0); ink_ui_hint_obj(t, INK_REFRESH_UI); ink_ui_content_changed();
    }
    if (lv_obj_get_child_count(s) == 1) {
        ink_ui_line(s, "", INK_FONT_HEADING, INK_WEIGHT_REGULAR, INK_GAP_L, INK_GAP_XS, w - INK_PRIMARY - 2 * INK_GAP_L);
        ink_ui_line(s, "", INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_GAP_L, INK_LINE_HEADING + INK_GAP_XS, w - INK_PRIMARY - 2 * INK_GAP_L);
    }
    ck_text(lv_obj_get_child(s, 1), time, false); ck_text(lv_obj_get_child(s, 2), note, false);
    A.row[r].id = a->id; A.row[r].on = a->enabled;
    CK.f[A.row[r].field].value = ck_str("%s · %s · %s · %s", time, a->title, repeat, a->enabled ? TR("开启") : TR("关闭"));
}
static const char *next_line(void) {
    const char *next = ck_next_alarm(true);
    return ck_str(TR("下一个闹钟：%s"), next[0] ? next : TR("没有开启的闹钟"));
}
void ck_alarm_body(int h) {
    lv_obj_t *b = ck_body; int w = INK_CANVAS_W - 2 * INK_MARGIN, y = 0, row = INK_DIGIT;
    ink_alarm_t a; int cursor = 0; A.count = 0; A.rows = 0;
    while (ink_alarm_next(&cursor, INK_ALARM, &a) == 1) A.count++;
    if (A.count) {
        const char *line = next_line();
        A.next_line = ink_ui_line(b, line, INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_MARGIN, y, w); A.next_field = ck_field("next", TR("下一个闹钟"), line);
        y += ink_ui_line_height(INK_FONT_NOTE) + INK_GAP;
    } else ink_ui_notice(b, TR("还没有闹钟"), y);
    int add = h - INK_MARGIN - INK_PRIMARY, bar = add - INK_GAP_M - INK_CONTROL;
    A.per = (unsigned)((add - INK_GAP_M - y + INK_GAP_M) / (row + INK_GAP_M));
    if (A.count > A.per) A.per = (unsigned)LV_MAX(1, (bar - INK_GAP_M - y + INK_GAP_M) / (row + INK_GAP_M));
    A.first = ck_pager(b, bar, &ck_me->page[TAB_ALARM], A.count, A.per);
    cursor = 0;
    for (unsigned i = 0; ink_alarm_next(&cursor, INK_ALARM, &a) == 1 && A.rows < A.per && A.rows < ROWS; i++) {
        if (i < A.first) continue;
        unsigned r = A.rows++;
        A.row[r].slot = ink_ui_card(b, INK_MARGIN, y + (int)r * (row + INK_GAP_M), w, row, INK_SURFACE_WHITE);
        char id[24]; snprintf(id, sizeof id, "alarm:%u", (unsigned)a.id);
        ck_tap(A.row[r].slot, TR("查看闹钟"), "clock.open", ck_str("{\"id\":%u}", (unsigned)a.id));
        A.row[r].field = ck_field(id, id, ""); A.row[r].on = !a.enabled;
        row_fill(r, &a);
    }
    ck_btn(b, TR("+ 加闹钟"), "clock.new", NULL, INK_MARGIN, add, w, INK_PRIMARY, true, true);
}
void ck_alarm_extra(ink_jw_t *w) { ink_jw_raw(w, ",\"next_alarm\":"); ink_jw_str(w, ck_next_alarm(true)); }
void ck_alarm_minute(void) {
    if (!A.count || !A.next_line) return;
    const char *line = next_line(); ck_text(A.next_line, line, false); ck_value(A.next_field, line);
}
static void update_rows(void *arg) { /* the same alarms on this page: in place; anything else: the tab again */
    (void)arg;
    if (!ck_on("Clock") || ck_me->tab != TAB_ALARM) return;
    ink_alarm_t a[ROWS], one; int cursor = 0; unsigned n = 0, i = 0;
    while (ink_alarm_next(&cursor, INK_ALARM, &one) == 1) { if (i >= A.first && i - A.first < A.rows) a[i - A.first] = one; i++; n++; }
    bool same = n == A.count;
    for (unsigned r = 0; same && r < A.rows; r++) same = a[r].id == A.row[r].id;
    if (!same) { ck_refresh(); return; }
    for (unsigned r = 0; r < A.rows; r++) row_fill(r, &a[r]);
    ck_alarm_minute();
}
static void leave(void *arg) { (void)arg; if (ck_on("ClockAlarm")) ink_ui_pop(); }
void ck_alarm_changed(void) {
    ink_alarm_t a;
    if (ck_on("ClockAlarm") && A.draft.id && !find(A.draft.id, &a)) ink_ui_after_press(leave, NULL); /* deleted elsewhere */
    else if (ck_on("Clock") && ck_me->tab == TAB_ALARM) ink_ui_after_press(update_rows, NULL);
    else if (ck_on("Clock") && ck_me->tab == TAB_WORLD) ck_refresh(); /* its next-alarm line */
}

/* ---- the editor ---- */
static void summary(void) {
    char s[96]; ink_alarm_t a = A.draft;
    if (A.error[0]) snprintf(s, sizeof s, "%s", A.error); else repeat_text(&a, s, sizeof s, true);
    ck_text(A.summary, s, false); ck_value(A.repeat_field, s);
}
void ck_alarm_edit_label(int part) {
    char t[8]; snprintf(t, sizeof t, "%02d:%02d", ck_edit_get(CK_HOUR), ck_edit_get(CK_MINUTE));
    ck_counter_label(part); ck_value(A.time_field, t); summary();
}
static void editor_create(lv_obj_t *s, void *arg) {
    (void)arg; bool old = A.draft.id != 0; int w = INK_CANVAS_W - 2 * INK_MARGIN, y = INK_STATUS_BAR + INK_PRIMARY + INK_GAP;
    ck_begin("ClockAlarm", TR("闹钟"), ""); CK.spec.object.id = ck_str("alarm:%u", (unsigned)A.draft.id);
    ink_shell_statusbar(s, true); ink_shell_topbar(s, old ? TR("闹钟") : TR("加闹钟"));
    A.time_field = ck_field("time", TR("时间"), ck_str("%02d:%02d", ck_edit_get(CK_HOUR), ck_edit_get(CK_MINUTE)));
    ck_counter_row(s, CK_HOUR, y, TR("时-"), TR("时＋"), TR("时")); y += INK_CONTROL + INK_GAP_XS;
    ck_counter_row(s, CK_MINUTE, y, TR("分-"), TR("分＋"), TR("分")); y += INK_CONTROL + INK_GAP_XS;
    A.repeat_field = ck_field("repeat", TR("重复"), "");
    A.summary = ink_ui_line(s, "", INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_MARGIN, y, w); summary();
    y += ink_ui_line_height(INK_FONT_NOTE) + INK_GAP_XS;
    ck_field("enabled", TR("开关"), A.draft.enabled ? TR("开启") : TR("关闭"));
    y = ck_name(s, y, A.draft.title, TR("闹钟名称"), sizeof A.draft.title - 1, "clock.focus") + INK_GAP_XS;
    int need[3], sum = 0, x = INK_MARGIN;
    for (int i = 0; i < 3; i++) sum += need[i] = LV_MAX(INK_CONTROL, ink_ui_text_width(TR(PRESET_LABEL[i]), INK_FONT_LIST, INK_WEIGHT_MEDIUM) + 2 * INK_GAP);
    int spare = w - 2 * INK_GAP_M - sum, ph = spare < 0 ? 2 * ink_ui_line_height(INK_FONT_LIST) + 2 * INK_GAP_XS : INK_CONTROL;
    for (int i = 0; i < 3; i++) { /* widths after their words, so 只响一次 stays on one line at the largest text */
        int bw = spare < 0 ? (w - 2 * INK_GAP_M) / 3 : need[i] + spare / 3;
        A.preset[i] = ck_btn(s, TR(PRESET_LABEL[i]), "clock.draft", ck_str("{\"weekdays\":%d}", PRESET[i]), x, y, bw, ph, A.draft.weekdays == PRESET[i], true);
        x += bw + INK_GAP_M;
    }
    y += ph + INK_GAP_XS;
    int bw = (w - 3 * INK_GAP_M) / 4;
    for (int k = 0; k < 7; k++) /* Monday first */
        A.day[k] = ck_btn(s, ck_wday(k + 1), "clock.draft", ck_str("{\"day\":%d}", (k + 1) % 7), INK_MARGIN + (k % 4) * (bw + INK_GAP_M),
                          y + (k / 4) * (INK_CONTROL + INK_GAP_XS), bw, INK_CONTROL, A.draft.weekdays & (1u << ((k + 1) % 7)), true);
    if (old) ck_btn(s, A.draft.enabled ? TR("关闭") : TR("开启"), "clock.draft", A.draft.enabled ? "{\"enabled\":false}" : "{\"enabled\":true}",
                    INK_MARGIN + 3 * (bw + INK_GAP_M), y + INK_CONTROL + INK_GAP_XS, bw, INK_CONTROL, false, true);
    ck_field("error", TR("提示"), A.error);
    int bottom = INK_CANVAS_H - INK_MARGIN - INK_CONTROL;
    if (old) ck_btn(s, TR("删除闹钟"), "alarm.cancel", ck_str("{\"id\":%u}", (unsigned)A.draft.id), INK_MARGIN, bottom, (w - INK_GAP) / 2, INK_CONTROL, false, true);
    ck_btn(s, TR("保存闹钟"), "clock.save", NULL, INK_MARGIN + (old ? (w + INK_GAP) / 2 : 0), bottom, old ? (w - INK_GAP) / 2 : w, INK_CONTROL, true, true);
}
static void editor_destroy(void) { CK.name = NULL; A.summary = NULL; memset(A.preset, 0, sizeof A.preset); memset(A.day, 0, sizeof A.day); }
static const ink_activity_t EDITOR = {.name = "ClockAlarm", .app = "clock", .create = editor_create, .destroy = editor_destroy, .snapshot = ck_name_snapshot};
static int title_max(void) { /* alarm.set's own limit, whatever the time service makes it */
    const ink_cap_t *cap = ink_cap_find("alarm.set");
    const char *p = cap && cap->input ? strstr(cap->input, "\"title\"") : NULL;
    p = p ? strstr(p, "\"maxLength\":") : NULL;
    return p ? atoi(p + 12) : (int)(sizeof A.draft.title / 3);
}
static void save(ink_call_t *c) {
    ink_args_t *args = osal_calloc(1, sizeof *args, OSAL_MEM_EXTERNAL);
    if (!args) { c->rc.status = INK_RC_FAILED; return; }
    if (CK.name) snprintf(A.draft.title, sizeof A.draft.title, "%s", ink_textfield_text(CK.name));
    ink_args_init(args); ink_args_int(args, "hour", ck_edit_get(CK_HOUR)); ink_args_int(args, "minute", ck_edit_get(CK_MINUTE));
    ink_args_int(args, "weekdays", A.draft.weekdays); ink_args_str(args, "title", A.draft.title);
    if (A.draft.id) { ink_args_int(args, "id", A.draft.id); ink_args_bool(args, "enabled", A.draft.enabled); }
    ink_cap_invoke(A.draft.id ? "alarm.update" : "alarm.set", args, c->src, 0, &c->rc); osal_free(args);
    if (c->rc.status == INK_RC_DONE) { ink_ui_pop(); return; }
    if (c->rc.status == INK_RC_INVALID) snprintf(A.error, sizeof A.error, TR("名称最多%d个字，请缩短后保存"), title_max());
    else if (strstr(c->rc.detail, "too many")) snprintf(A.error, sizeof A.error, TR("闹钟已满（全家最多 %d 个闹钟和提醒）"), ALARM_TABLE);
    else snprintf(A.error, sizeof A.error, "%s", TR("没能保存，请重试"));
    for (size_t i = 0; i < CK.spec.field_count; i++) if (!strcmp(CK.f[i].id, "error")) CK.f[i].value = ck_str("%s", A.error);
    summary();
}
void ck_alarm_cap(ink_call_t *c) {
    const char *id = c->cap->id + 6; int64_t v;
    if (!strcmp(id, "new") || !strcmp(id, "open")) {
        A.draft = (ink_alarm_t){.kind = INK_ALARM, .hour = 7, .enabled = true}; A.error[0] = 0;
        if (*id == 'o' && (!ink_args_get_int(&c->args, "id", &v) || !find((uint32_t)v, &A.draft))) { c->rc.status = INK_RC_STALE; return; }
        ck_edit_set(CK_HOUR, A.draft.hour); ck_edit_set(CK_MINUTE, A.draft.min);
        ink_ui_push(&EDITOR, NULL);
        return;
    }
    if (!strcmp(id, "focus")) { ck_focus(c); return; }
    if (!strcmp(id, "save")) { save(c); return; }
    bool was = A.draft.enabled; A.error[0] = 0;
    if (ink_args_get_int(&c->args, "hour", &v)) ck_edit_set(CK_HOUR, (int)v);
    if (ink_args_get_int(&c->args, "minute", &v)) ck_edit_set(CK_MINUTE, (int)v);
    if (ink_args_get_int(&c->args, "weekdays", &v)) A.draft.weekdays = (uint8_t)v;
    if (ink_args_get_int(&c->args, "day", &v)) A.draft.weekdays ^= (uint8_t)(1u << v);
    A.draft.repeat = A.draft.weekdays ? INK_REPEAT_WEEKDAYS : INK_REPEAT_NONE;
    if (ink_args_get_bool(&c->args, "enabled", &A.draft.enabled) && was != A.draft.enabled) { ink_ui_redraw(); return; } /* its button's words change */
    for (int i = 0; i < 3; i++) if (A.preset[i]) ck_mark(A.preset[i], A.draft.weekdays == PRESET[i]);
    for (int k = 0; k < 7; k++) if (A.day[k]) ck_mark(A.day[k], A.draft.weekdays & (1u << ((k + 1) % 7)));
    if (A.summary) summary();
}
