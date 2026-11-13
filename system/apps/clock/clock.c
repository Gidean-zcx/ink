/* The clock: four tabs on one page after Fossify Clock (docs/tasks/clock-3/对照表.md), its page model, capabilities. UI task only. */
/* ui-scale-runtime: dimension w,x,y,h,height,top Theme-derived coordinates and heights supplied by the clock's pages. */
#include "clock_internal.h"
#include "ink/sys/state.h"
#include <stdarg.h>

ck_page_t CK INK_PSRAM_BSS;
static ck_member_t members[INK_MEMBERS_MAX] INK_PSRAM_BSS;
ck_member_t *ck_me;
lv_obj_t *ck_body;
static lv_obj_t *segments;
static lv_timer_t *tick;
static unsigned shown_page, pages, total; /* what ck_pager drew last */

void ck_begin(const char *id, const char *title, const char *object) {
    memset(&CK.spec, 0, sizeof CK.spec); CK.used = 0; CK.name = NULL;
    CK.spec = (ink_ui_page_spec_t){.id = id, .title = title, .object = {object, 1}, .fields = CK.f, .actions = CK.a};
}
const char *ck_str(const char *fmt, ...) {
    size_t room = sizeof CK.arena - CK.used;
    va_list ap; va_start(ap, fmt); int n = vsnprintf(CK.arena + CK.used, room, fmt, ap); va_end(ap);
    if (n < 0 || (size_t)n >= room) return "";
    const char *s = CK.arena + CK.used; CK.used += (size_t)n + 1; return s;
}
int ck_field(const char *id, const char *label, const char *value) {
    if (CK.spec.field_count >= sizeof CK.f / sizeof *CK.f) return 0;
    CK.f[CK.spec.field_count] = (ink_ui_field_spec_t){.id = ck_str("%s", id), .role = "text", .label = ck_str("%s", label), .value = ck_str("%s", value)};
    ink_ui_content_changed();
    return (int)CK.spec.field_count++;
}
void ck_text(lv_obj_t *label, const char *text, bool fast) {
    if (!label || !strcmp(lv_label_get_text(label), text)) return;
    lv_label_set_text(label, text); ink_ui_hint_obj(label, fast ? INK_REFRESH_FAST : INK_REFRESH_UI); ink_ui_content_changed();
}
void ck_value(int field, const char *value) { /* in place while it fits: values that change every second do not fill the arena */
    char *old = (char *)CK.f[field].value;
    if (old >= CK.arena && old < CK.arena + sizeof CK.arena && strlen(old) >= strlen(value)) strcpy(old, value);
    else CK.f[field].value = ck_str("%s", value);
}
void ck_declare(const char *label, const char *cap, const char *args, bool on) {
    if (CK.spec.action_count >= sizeof CK.a / sizeof *CK.a) return;
    CK.a[CK.spec.action_count] = (ink_ui_action_spec_t){.id = ck_str("a%u", (unsigned)CK.spec.action_count), .label = ck_str("%s", label),
        .capability = cap, .args_json = ck_str("%s", args ? args : "{}"), .enabled = on};
    CK.spec.action_count++;
}
void ck_tap(lv_obj_t *o, const char *label, const char *cap, const char *args) { ck_declare(label, cap, args, true); ink_shell_bind(o, cap, args ? args : "{}"); }
lv_obj_t *ck_btn(lv_obj_t *p, const char *label, const char *cap, const char *args, int x, int y, int w, int h, bool primary, bool on) {
    lv_obj_t *b = ink_ui_pill(p, label, x, y, w, h, primary, NULL, NULL);
    if (on) { ck_tap(b, label, cap, args); return b; }
    ck_declare(label, cap, args, false); lv_obj_add_state(b, LV_STATE_DISABLED);
    lv_obj_set_style_border_color(b, ink_ui_secondary(), 0); lv_obj_set_style_text_color(lv_obj_get_child(b, 0), ink_ui_secondary(), 0);
    return b;
}
void ck_mark(lv_obj_t *b, bool on) {
    if (on == (ink_ui_surface_kind(b) == INK_SURFACE_BLACK)) return;
    ink_ui_surface_style(b, on ? INK_SURFACE_BLACK : INK_SURFACE_WHITE, !on);
    lv_obj_set_style_text_color(lv_obj_get_child(b, 0), on ? ink_ui_paper() : ink_ui_ink(), 0);
    ink_ui_hint_obj(b, INK_REFRESH_UI); ink_ui_content_changed();
}
int ck_snapshot(char *out, size_t cap, void (*extra)(ink_jw_t *w)) {
    int n = ink_ui_semantics_snapshot(&CK.spec, out, cap);
    if (n < 2 || !extra) return n;
    ink_jw_t w = {.buf = out, .cap = cap, .len = (size_t)n - 1};
    extra(&w); ink_jw_raw(&w, "}");
    if (w.overflow) { out[0] = 0; return -1; }
    return (int)w.len;
}
int ck_member(void) {
    const char *id = ink_member_current()->id; int free = -1;
    for (int i = 0; i < INK_MEMBERS_MAX; i++) {
        if (!strcmp(members[i].id, id)) { ck_me = &members[i]; return i; }
        if (free < 0 && (!members[i].id[0] || !ink_member_exists(members[i].id))) free = i;
    }
    if (free < 0) return -1;
    for (int k = 0; k < CK_PER_MEMBER; k++) ck_timer_set(free * CK_PER_MEMBER + k, 0); /* a removed member's slot */
    ck_watch_clear(free);
    members[free] = (ck_member_t){.tab = TAB_ALARM, .timer = -1}; /* first open: the alarms (对照表 A3) */
    snprintf(members[free].id, sizeof members[free].id, "%s", id); ck_me = &members[free];
    return free;
}
const char *ck_member_id(int m) { return m >= 0 && m < INK_MEMBERS_MAX ? members[m].id : ""; }
bool ck_on(const char *page) { return ink_ui_page_current() && !strcmp(ink_ui_page_name(), page); }
static void page_turned(lv_event_t *e) {
    ink_receipt_t rc;
    ink_cap_invoke_json((intptr_t)lv_event_get_user_data(e) > 0 ? "clock.next" : "clock.prev", NULL, INK_SRC_TOUCH, ink_ui_content_version(), &rc);
}
unsigned ck_pager(lv_obj_t *p, int y, unsigned *page, unsigned count, unsigned per) {
    per = per ? per : 1; pages = count ? (count + per - 1) / per : 1; total = count;
    if (*page >= pages) *page = pages - 1;
    shown_page = *page;
    ck_declare(TR("上一页"), "clock.prev", NULL, *page > 0); ck_declare(TR("下一页"), "clock.next", NULL, *page + 1 < pages);
    if (pages > 1) ink_ui_pagebar(p, (int)*page + 1, (int)pages, y, page_turned);
    return *page * per;
}
static long daynum(int y, int m, int d) { /* days from 1970-01-01 (civil) */
    y -= m <= 2; long era = (y >= 0 ? y : y - 399) / 400, yoe = y - era * 400, doy = (153L * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    return era * 146097 + yoe * 365 + yoe / 4 - yoe / 100 + doy - 719468;
}
const char *ck_wday(int wday) {
    static const char *const W[] = {TR_NOOP("周日"), TR_NOOP("周一"), TR_NOOP("周二"), TR_NOOP("周三"), TR_NOOP("周四"), TR_NOOP("周五"), TR_NOOP("周六")};
    return TR(W[(wday % 7 + 7) % 7]);
}
const char *ck_day(int64_t utc_s) {
    static char out[24];
    ink_tm_t a, b; ink_time_to_local(ink_time_now_s(), &a); ink_time_to_local(utc_s, &b);
    long d = daynum(b.year, b.month, b.day) - daynum(a.year, a.month, a.day);
    if (d >= -1 && d <= 1) return d < 0 ? TR("昨天") : d ? TR("明天") : TR("今天");
    if (d > 1 && d < 7) return ck_wday(b.wday);
    snprintf(out, sizeof out, TR("%d月%d日"), b.month, b.day); return out;
}

/* ---- the Clock page: tabs and what is under them ---- */
static const char *const TAB_IDS[TABS] = {"world", "alarm", "stopwatch", "timer"};
static bool running(void) { return (ck_me->tab == TAB_WATCH && ck_watch_running()) || (ck_me->tab == TAB_TIMER && ck_timer_running()); }
static void ticked(lv_timer_t *t) {
    (void)t;
    if (!ck_on("Clock") || ink_power_state() == INK_PWR_LOCKED) return; /* off the page or locked: nothing redraws */
    if (ck_me->tab == TAB_WATCH) ck_watch_tick(); else if (ck_me->tab == TAB_TIMER) ck_timer_ticks();
}
static void build(void) {
    static const char *const CAPS[TABS] = {"clock.world", "clock.tab", "clock.stopwatch", "clock.timer"};
    if (tick) lv_timer_delete(tick);
    tick = NULL;
    lv_obj_clean(segments); lv_obj_clean(ck_body);
    ck_begin("Clock", TR("时钟"), "clock:list");
    const char *labels[TABS] = {TR("时钟"), TR("闹钟"), TR("秒表"), TR("计时")};
    lv_obj_t *row = ink_ui_segments(segments, labels, TABS, ck_me->tab, INK_MARGIN, 0, INK_CANVAS_W - 2 * INK_MARGIN, NULL, NULL);
    for (int i = 0; i < TABS; i++) ck_tap(lv_obj_get_child(row, i), labels[i], CAPS[i], i == TAB_ALARM ? "{\"tab\":\"alarm\"}" : NULL);
    int h = INK_CANVAS_H - (INK_STATUS_BAR + INK_PRIMARY + INK_GAP + INK_SEGMENT + INK_GAP);
    void (*const body[TABS])(int) = {ck_world_body, ck_alarm_body, ck_watch_body, ck_timer_body};
    body[ck_me->tab](h);
    if (running()) tick = lv_timer_create(ticked, 1000, NULL);
}
static void rebuild(void *arg) {
    (void)arg;
    if (!ck_on("Clock") || !segments) return;
    build();
    ink_ui_hint_obj(segments, INK_REFRESH_UI); ink_ui_hint_obj(ck_body, INK_REFRESH_UI); ink_ui_content_changed();
}
void ck_refresh(void) { ink_ui_after_press(rebuild, NULL); }
static void clock_create(lv_obj_t *screen, void *arg) {
    (void)arg; ck_member();
    ink_shell_statusbar(screen, true); ink_shell_topbar(screen, TR("时钟"));
    int y = INK_STATUS_BAR + INK_PRIMARY + INK_GAP;
    segments = ink_ui_box(screen, 0, y, INK_CANVAS_W, INK_SEGMENT, 0, INK_SURFACE_WHITE, false);
    y += INK_SEGMENT + INK_GAP;
    ck_body = ink_ui_box(screen, 0, y, INK_CANVAS_W, INK_CANVAS_H - y, 0, INK_SURFACE_WHITE, false);
    build();
}
static void clock_destroy(void) {
    if (tick) lv_timer_delete(tick);
    tick = NULL; segments = ck_body = NULL;
}
static void clock_extra(ink_jw_t *w) {
    char s[96]; snprintf(s, sizeof s, ",\"tab\":\"%s\",\"page\":%u,\"page_number\":%u,\"pages\":%u,\"total\":%u", TAB_IDS[ck_me->tab], shown_page + 1, shown_page + 1, pages, total);
    ink_jw_raw(w, s);
    void (*const extra[TABS])(ink_jw_t *) = {ck_world_extra, ck_alarm_extra, ck_watch_extra, ck_timer_extra};
    extra[ck_me->tab](w);
}
static int clock_snapshot(char *out, size_t cap) { return ck_snapshot(out, cap, clock_extra); }
static bool clock_key(ink_key_t key, bool lng) {
    if (lng || (key != INK_KEY_NEXT && key != INK_KEY_PREV)) return false;
    ink_receipt_t rc; ink_cap_invoke_json(key == INK_KEY_NEXT ? "clock.next" : "clock.prev", NULL, INK_SRC_KEY, ink_ui_content_version(), &rc);
    return true;
}
static const ink_activity_t CLOCK = {.name = "Clock", .app = "clock", .create = clock_create, .destroy = clock_destroy, .snapshot = clock_snapshot, .on_key = clock_key};

/* ---- the editors' counters (InfiniTime's Counter) ---- */
static lv_obj_t *numbers[3];
void ck_counter_row(lv_obj_t *p, int part, int y, const char *minus, const char *plus, const char *unit) {
    static const char *const PART[] = {"hour", "minute", "second"};
    int w = INK_CANVAS_W - 2 * INK_MARGIN, bw = LV_MAX(INK_CONTROL, ink_ui_text_width(minus, INK_FONT_LIST, INK_WEIGHT_MEDIUM) + 2 * INK_GAP);
    ck_btn(p, minus, "clock.step", ck_str("{\"part\":\"%s\",\"delta\":-1}", PART[part]), INK_MARGIN, y, bw, INK_CONTROL, false, true);
    ck_btn(p, plus, "clock.step", ck_str("{\"part\":\"%s\",\"delta\":1}", PART[part]), INK_MARGIN + w - bw, y, bw, INK_CONTROL, false, true);
    int mid = w - 2 * bw - 2 * INK_GAP, uw = ink_ui_text_width(unit, INK_FONT_LIST, INK_WEIGHT_MEDIUM), nw = ink_ui_text_width("00", INK_FONT_HEADING, INK_WEIGHT_REGULAR);
    int x = INK_MARGIN + bw + INK_GAP + (mid - nw - INK_GAP_S - uw) / 2; /* the number and its unit, centred between the buttons */
    numbers[part] = ink_ui_line(p, ck_str("%02d", ck_edit_get(part)), INK_FONT_HEADING, INK_WEIGHT_REGULAR, x, y + (INK_CONTROL - INK_LINE_HEADING) / 2, nw);
    ink_ui_line(p, unit, INK_FONT_LIST, INK_WEIGHT_MEDIUM, x + nw + INK_GAP_S, y + (INK_CONTROL - ink_ui_line_height(INK_FONT_LIST)) / 2, uw);
}
void ck_counter_label(int part) { /* only that number redraws */
    if (!numbers[part] || !(ck_on("ClockAlarm") || ck_on("ClockTimerEdit"))) return;
    char s[8]; snprintf(s, sizeof s, "%02d", ck_edit_get(part));
    lv_label_set_text(numbers[part], s); ink_ui_hint_obj(numbers[part], INK_REFRESH_UI); ink_ui_content_changed();
}
static void edit_label(int part) {
    if (ck_on("ClockAlarm")) ck_alarm_edit_label(part);
    else if (ck_on("ClockTimerEdit")) ck_timer_edit_label(part);
}
static void name_changed(lv_event_t *e) { int i = (int)(intptr_t)lv_event_get_user_data(e); CK.f[i].value = ink_textfield_text(CK.name); }
int ck_name(lv_obj_t *p, int y, const char *initial, const char *hint, size_t max, const char *focus_cap) {
    int field = ck_field("name", TR("名称"), initial);
    ck_declare(TR("输入名称"), focus_cap, NULL, true);
    ink_text_cfg_t cfg = {.type = INK_TEXT_PLAIN, .enter = INK_ENTER_DONE, .initial = initial, .hint = hint, .max = max};
    CK.name = ink_textfield_create(p, &cfg, NULL, NULL, false); /* the keyboard comes when the field is tapped */
    lv_obj_set_y(CK.name, ink_ui_px(y));
    ink_textfield_style(CK.name, INK_FIELD_SINGLE, hint);
    ink_textfield_bind_short(CK.name, &CK.spec, &CK.f[field], &CK.a[CK.spec.action_count - 1]);
    lv_obj_add_event_cb(CK.name, name_changed, LV_EVENT_VALUE_CHANGED, (void *)(intptr_t)field);
    return y + ink_textfield_height(INK_FIELD_SINGLE);
}
void ck_focus(ink_call_t *c) { if (!CK.name || ink_textfield_short_focus(CK.name)) c->rc.status = INK_RC_STALE; }
int ck_name_snapshot(char *out, size_t cap) {
    size_t actions = CK.spec.action_count;
    if (CK.name && ink_text_focused()) CK.spec.action_count = 0; /* the keyboard owns input */
    int n = CK.name ? ink_textfield_short_snapshot(CK.name, out, cap) : ink_ui_semantics_snapshot(&CK.spec, out, cap);
    CK.spec.action_count = actions; return n;
}

/* ---- navigation: tabs, pages ---- */
static void nav(ink_call_t *c) {
    const char *id = c->cap->id + 6; int tab = -1; char want[16] = "";
    if (!strcmp(id, "tab")) ink_args_get_str(&c->args, "tab", want, sizeof want);
    for (int i = 0; i < TABS; i++) if (!strcmp(id, TAB_IDS[i]) || !strcmp(want, TAB_IDS[i]) || (i == TAB_TIMER && !strcmp(id, "timer"))) tab = i;
    if (tab >= 0) {
        if (ck_member() < 0) { c->rc.status = INK_RC_FAILED; return; }
        ck_me->tab = (uint8_t)tab;
        if (ck_on("Clock")) { ck_refresh(); return; }
        if (!ink_ui_bring_up("clock")) { if (ink_app_open("clock", NULL, NULL)) c->rc.status = INK_RC_REJECTED; return; }
        while (!strncmp(ink_ui_page_name(), "Clock", 5) && strcmp(ink_ui_page_name(), "Clock") && ink_ui_depth() > 1) ink_ui_pop();
        return;
    }
    int delta = strstr(id, "next") ? 1 : -1;
    if (ck_on("ClockWorldPick")) ck_world_turn(delta);
    else if (!ck_on("Clock")) c->rc.status = INK_RC_STALE;
    else if (delta < 0 ? shown_page > 0 : shown_page + 1 < pages) { ck_me->page[ck_me->tab] = shown_page + (unsigned)delta; ck_refresh(); }
}
static void step(ink_call_t *c) {
    char part[8] = ""; int64_t delta = 1;
    ink_args_get_str(&c->args, "part", part, sizeof part); ink_args_get_int(&c->args, "delta", &delta);
    int p = !strcmp(part, "hour") ? CK_HOUR : !strcmp(part, "minute") ? CK_MINUTE : CK_SECOND;
    if (p == CK_SECOND && !ck_on("ClockTimerEdit")) { c->rc.status = INK_RC_INVALID; return; }
    ck_edit_step(p, (int)delta); /* copied UpBtnPressed / DownBtnPressed: wraps, only that number redraws */
}

/* One line each; tools/capabilities.py reads these macros (literal strings only). */
#define CAP(n, t, page, fx, in, fn) {.id = "clock." n, .app = "clock", .title = t, .activity = page, .effect = fx, .child_ok = true, .input = in, .run = fn}
#define ANY(n, t, fx, in, fn) {.id = "clock." n, .app = "clock", .title = t, .effect = fx, .child_ok = true, .input = in, .run = fn}
#define NAV INK_FX_NAVIGATE
#define WRITE INK_FX_LOCAL_WRITE
#define NONE "{\"type\":\"object\",\"properties\":{}}"
#define ID_OPT "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":47}}}"
#define ID_REQ "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":47}},\"required\":[\"id\"]}"
#define ZONE_REQ "{\"type\":\"object\",\"properties\":{\"zone\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":8}},\"required\":[\"zone\"]}"
#define TAB_IN "{\"type\":\"object\",\"properties\":{\"tab\":{\"type\":\"string\",\"enum\":[\"world\",\"alarm\",\"stopwatch\",\"timer\"]}},\"required\":[\"tab\"]}"
#define STEP_IN "{\"type\":\"object\",\"properties\":{\"part\":{\"type\":\"string\",\"enum\":[\"hour\",\"minute\",\"second\"]},\"delta\":{\"type\":\"integer\",\"enum\":[-1,1]}},\"required\":[\"part\",\"delta\"]}"
#define OPEN_IN "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"integer\",\"minimum\":1}},\"required\":[\"id\"]}"
#define DRAFT_IN "{\"type\":\"object\",\"properties\":{\"hour\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":23},\"minute\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":59},\"weekdays\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":127},\"day\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":6},\"enabled\":{\"type\":\"boolean\"}}}"
#define TITLE_IN "{\"type\":\"object\",\"properties\":{\"zone\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":8},\"title\":{\"type\":\"string\",\"maxLength\":15}},\"required\":[\"zone\",\"title\"]}"
#define SORT_IN "{\"type\":\"object\",\"properties\":{\"by\":{\"type\":\"string\",\"enum\":[\"lap\",\"lap_time\",\"total\"]}},\"required\":[\"by\"]}"
#define SET_IN "{\"type\":\"object\",\"properties\":{\"seconds\":{\"type\":\"integer\",\"minimum\":1,\"maximum\":86399},\"id\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":47}},\"required\":[\"seconds\"]}"
static const ink_cap_t CAPS[] = {
    ANY("tab", "打开时钟的页签", NAV, TAB_IN, nav),
    ANY("world", TR_NOOP("世界时钟"), NAV, NONE, nav),
    ANY("stopwatch", TR_NOOP("秒表"), NAV, NONE, nav),
    ANY("timer", "计时", NAV, NONE, nav),
    ANY("prev", "上一页（当前页签或加城市页）", NAV, NONE, nav),
    ANY("next", "下一页（当前页签或加城市页）", NAV, NONE, nav),
    ANY("extra_prev", TR_NOOP("上一页"), NAV, NONE, nav),
    ANY("extra_next", TR_NOOP("下一页"), NAV, NONE, nav),
    ANY("step", "时、分、秒加一或减一（到头回卷）", NAV, STEP_IN, step),
    CAP("new", "加闹钟", "Clock", NAV, NONE, ck_alarm_cap),
    CAP("open", "查看闹钟", "Clock", NAV, OPEN_IN, ck_alarm_cap),
    CAP("draft", "选择闹钟的时间、重复日期和开关", "ClockAlarm", NAV, DRAFT_IN, ck_alarm_cap),
    CAP("focus", "输入闹钟名称", "ClockAlarm", NAV, NONE, ck_alarm_cap),
    CAP("save", "保存闹钟", "ClockAlarm", WRITE, NONE, ck_alarm_cap),
    ANY("world_pick", TR_NOOP("加城市"), NAV, NONE, ck_world_cap),
    ANY("world_add", TR_NOOP("加城市"), WRITE, ZONE_REQ, ck_world_cap),
    ANY("world_remove", TR_NOOP("删除"), WRITE, ZONE_REQ, ck_world_cap),
    CAP("world_edit", "改城市的名字或删除城市", "Clock", NAV, ZONE_REQ, ck_world_cap),
    CAP("world_name", "输入城市名字", "ClockCity", NAV, NONE, ck_world_cap),
    ANY("world_title", "给城市改名（空的恢复默认）", WRITE, TITLE_IN, ck_world_cap),
    CAP("stopwatch_start", TR_NOOP("开始"), "Clock", WRITE, NONE, ck_watch_cap),
    CAP("stopwatch_pause", TR_NOOP("暂停"), "Clock", WRITE, NONE, ck_watch_cap),
    CAP("stopwatch_lap", TR_NOOP("计次"), "Clock", WRITE, NONE, ck_watch_cap),
    CAP("stopwatch_reset", TR_NOOP("清零"), "Clock", WRITE, NONE, ck_watch_cap),
    CAP("lap_sort", "计次按圈次、单圈或累计排序（再点同一列升降切换）", "Clock", NAV, SORT_IN, ck_watch_cap),
    ANY("timer_set", "设定计时时长", WRITE, SET_IN, ck_timer_cap),
    CAP("timer_start", "开始或继续计时", "Clock", WRITE, ID_OPT, ck_timer_cap),
    CAP("timer_pause", "暂停计时", "Clock", WRITE, ID_OPT, ck_timer_cap),
    CAP("timer_cancel", "重置计时", "Clock", WRITE, ID_OPT, ck_timer_cap),
    CAP("timer_new", "加计时器", "Clock", NAV, NONE, ck_timer_cap),
    CAP("timer_edit", "编辑计时器", "Clock", NAV, ID_REQ, ck_timer_cap),
    CAP("timer_name", "输入计时器名字", "ClockTimerEdit", NAV, NONE, ck_timer_cap),
    CAP("timer_save", "保存计时器", "ClockTimerEdit", WRITE, NONE, ck_timer_cap),
    ANY("timer_delete", "删除计时器", WRITE, ID_REQ, ck_timer_cap),
    {.id = "clock.timer_dismiss", .app = "clock", .title = "关闭计时响铃", .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .while_locked = true, .input = ID_REQ, .run = ck_timer_cap},
    {.id = "clock.timer_restart", .app = "clock", .title = "到点的计时器再计一次", .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .while_locked = true, .input = ID_REQ, .run = ck_timer_cap},
};
static void time_changed(void *ctx, int h, const char *id) {
    (void)ctx; (void)h; (void)id;
    if (!ck_on("Clock") || !ck_body || ink_power_state() == INK_PWR_LOCKED) return;
    if (ck_me->tab == TAB_WORLD) ck_world_minute(); else if (ck_me->tab == TAB_ALARM) ck_alarm_minute();
}
static void time_event(void *ctx, const char *id, const ink_args_t *a) { (void)a; time_changed(ctx, 0, id); }
static void alarms_changed(void *ctx, int h, const char *id) { (void)ctx; (void)h; (void)id; ck_alarm_changed(); }
int ink_clock_init(void) {
    ck_init(&(ck_hooks_t){.edit_label = edit_label, .ring = ck_timer_ring, .finished = ck_timer_finished});
    return ck_world_init() || ck_timer_init() || ink_cap_register(CAPS, sizeof CAPS / sizeof *CAPS) ||
        ink_cap_subscribe("time.changed", time_event, NULL, INK_EXEC_UI) || ink_state_watch("time.", time_changed, NULL, INK_EXEC_UI) ||
        ink_state_watch("alarm.revision", alarms_changed, NULL, INK_EXEC_UI) ? -1 : 0;
}
const ink_app_def_t ink_appdef_clock = {.id = "clock", .title = TR_NOOP("时钟"), .icon = INK_UI_ICON(clock), .entry = &CLOCK, .system_app = true, .child_ok = true};
