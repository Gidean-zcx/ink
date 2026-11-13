/* The timer tab and editor after Fossify Clock (对照表 G): up to six named timers per member, their names and durations
 * kept in clock.timers; running, paused and ringing are InfiniTime's (clock_infinitime.cpp) and end with a restart. UI task only. */
/* ui-scale-runtime: dimension w,x,y,h,bw,dw,sw,row,per,bar,add,ph,top Theme-derived coordinates supplied by this page. */
#include "clock_internal.h"
#include "ink/sys/interrupt.h"

static const ink_db_def_t TIMERS = {.name = "clock.timers", .title = TR_NOOP("计时器"), .scope = INK_DB_MEMBER,
    .schema = 1, .json = true, .ai_read = true, .max_records = CK_PER_MEMBER}; /* {"title":"泡面","seconds":65} */
static struct {
    struct { uint32_t rec; char title[48]; } t[CK_TIMERS]; /* by handle: member slot * 6 + index */
    ink_db_t *db; char owner[16];
    int order[CK_PER_MEMBER], n, member, edit, duration_field;
    struct { int t, field; lv_obj_t *digits; } row[CK_PER_MEMBER];
    unsigned rows;
    char error[64];
    ink_json_t json;
} T INK_PSRAM_BSS;
static const ink_activity_t EDIT;

static void shown(int t, char *out, size_t cap) { /* 1:05, 1:00:01 */
    unsigned s = ck_timer_shown(t);
    if (s >= 3600) snprintf(out, cap, "%u:%02u:%02u", s / 3600, s / 60 % 60, s % 60); else snprintf(out, cap, "%u:%02u", s / 60, s % 60);
}
static const char *state_text(int t) {
    int s = ck_timer_state(t);
    return s == CK_RUNNING ? TR("正在计时") : s == CK_PAUSED ? TR("已暂停") : s == CK_FINISHED ? TR("已到点") : "";
}
static const char *title_of(int t) { return T.t[t].title[0] ? T.t[t].title : TR("计时器"); }
static int put(int t, const char *title, uint32_t seconds) { /* the record's id, or 0 */
    char json[128]; ink_jw_t w; ink_jw_init(&w, json, sizeof json);
    ink_jw_raw(&w, "{\"title\":"); ink_jw_str(&w, title); ink_jw_raw(&w, ",\"seconds\":"); ink_jw_int(&w, seconds); ink_jw_raw(&w, "}");
    uint32_t id = t >= 0 ? T.t[t].rec : ink_db_new_id(T.db);
    ink_db_write_t rec = {.id = id, .sort = id, .data = json, .len = w.len, .src = INK_SRC_TOUCH};
    return !w.overflow && ink_db_put(T.db, &rec, NULL) == INK_DB_OK ? (int)id : 0;
}
static bool load(void) { /* the current member's timers, in the order they were made; handles follow the records */
    int m = T.member = ck_member(), base = m * CK_PER_MEMBER;
    if (m < 0) return false;
    if (!T.db || strcmp(T.owner, ink_member_current()->id)) {
        ink_db_close(T.db); snprintf(T.owner, sizeof T.owner, "%s", ink_member_current()->id); T.db = ink_db_open_member(&TIMERS, T.owner);
    }
    if (!T.db) return false;
    if (!ink_db_count(T.db) && !ink_db_last_ver(T.db)) put(-1, "", 300); /* never had one: a five-minute timer, once (G3) */
    bool seen[CK_PER_MEMBER] = {0}; ink_db_cursor_t c = {0}; ink_rec_t r; char buf[192], title[48];
    T.n = 0;
    while (ink_db_next(T.db, &c, &r, buf, sizeof buf - 1) == 1 && T.n < CK_PER_MEMBER) {
        int64_t seconds = 300; int k = -1, free = -1;
        buf[r.len < sizeof buf - 1 ? r.len : sizeof buf - 1] = 0; title[0] = 0;
        if (!ink_json_parse(&T.json, buf, strlen(buf))) {
            ink_json_str(&T.json, ink_json_find(&T.json, 0, "title"), title, sizeof title);
            ink_json_int(&T.json, ink_json_find(&T.json, 0, "seconds"), &seconds);
        }
        for (int i = CK_PER_MEMBER - 1; i >= 0; i--) {
            if (T.t[base + i].rec == r.id) k = i;
            if (!T.t[base + i].rec && !seen[i]) free = i;
        }
        if (k < 0 && (k = free) < 0) continue;
        int t = base + k; seen[k] = true; T.order[T.n++] = t;
        if (T.t[t].rec != r.id || (ck_timer_state(t) == CK_IDLE && ck_timer_duration(t) != (uint32_t)seconds)) ck_timer_set(t, (uint32_t)seconds);
        T.t[t].rec = r.id; snprintf(T.t[t].title, sizeof T.t[t].title, "%s", title);
    }
    for (int i = 0; i < CK_PER_MEMBER; i++) if (!seen[i] && T.t[base + i].rec) { ck_timer_set(base + i, 0); T.t[base + i].rec = 0; }
    return true;
}
static bool mine(int t) { return t >= T.member * CK_PER_MEMBER && t < (T.member + 1) * CK_PER_MEMBER && T.t[t].rec; }
static void noop(lv_event_t *e) { (void)e; }
static int pill_w(const char *label) { return LV_MAX(INK_CONTROL, ink_ui_text_width(label, INK_FONT_LIST, INK_WEIGHT_MEDIUM) + 2 * INK_GAP); }
static const char *main_label(int s) { return s == CK_RUNNING ? TR("暂停") : s == CK_PAUSED ? TR("继续") : s == CK_FINISHED ? TR("再计一次") : TR("开始"); }
void ck_timer_body(int h) {
    lv_obj_t *b = ck_body; int w = INK_CANVAS_W - 2 * INK_MARGIN, y = 0; T.rows = 0;
    if (!load()) { ink_ui_notice(b, TR("没能读取计时器，请返回重试"), y); ck_pager(b, 0, &ck_me->page[TAB_TIMER], 0, 1); return; }
    if (!T.n) { /* all deleted: the shared empty state (G4) */
        lv_obj_t *e = ink_ui_empty(b, TR("还没有计时器"), TR("加计时器"), noop, NULL);
        ck_tap(lv_obj_get_child(e, 1), TR("加计时器"), "clock.timer_new", NULL); ck_pager(b, 0, &ck_me->page[TAB_TIMER], 0, 1); return;
    }
    int line = ink_ui_line_height(INK_FONT_LIST), rw = pill_w(TR("重置")); bool wide = true;
    for (int k = 0; k < T.n; k++) { /* the buttons beside the digits when every row fits, else under them (largest text) */
        char digits[16]; int s = ck_timer_state(T.order[k]); shown(T.order[k], digits, sizeof digits);
        wide &= ink_ui_text_width(digits, INK_FONT_HEADING, INK_WEIGHT_REGULAR) + INK_GAP + pill_w(main_label(s)) + (s ? INK_GAP_M + rw : 0) <= w;
    }
    int band = wide ? LV_MAX(INK_LINE_HEADING, INK_CONTROL) : INK_LINE_HEADING, top = line + band, row = top + (wide ? 0 : INK_GAP_XS + INK_CONTROL) + INK_GAP_M;
    int add = h - INK_MARGIN - INK_PRIMARY, note = add - INK_GAP_S - ink_ui_line_height(INK_FONT_NOTE), floor = T.n >= CK_PER_MEMBER ? note : add;
    int per = (floor - INK_GAP - y) / row, bar = floor - INK_GAP_M - INK_CONTROL;
    if (T.n > per) per = LV_MAX(1, (bar - INK_GAP_M - y) / row);
    unsigned first = ck_pager(b, bar, &ck_me->page[TAB_TIMER], (unsigned)T.n, (unsigned)per);
    for (int k = (int)first; k < T.n && k < (int)first + per; k++) {
        int t = T.order[k], s = ck_timer_state(t), at = y + (k - (int)first) * row, mw = pill_w(main_label(s)); char digits[16];
        int bx = INK_MARGIN + w - mw - (s ? INK_GAP_M + rw : 0), by = wide ? at + line : at + top + INK_GAP_XS;
        shown(t, digits, sizeof digits);
        lv_obj_t *box = ink_ui_box(b, INK_MARGIN, at, wide ? bx - INK_MARGIN - INK_GAP_M : w, top, 0, INK_SURFACE_WHITE, false);
        ck_tap(box, title_of(t), "clock.timer_edit", ck_str("{\"id\":%d}", t)); /* the name or the time opens the editor */
        int sw = ink_ui_text_width(state_text(t), INK_FONT_NOTE, INK_WEIGHT_REGULAR);
        if (T.t[t].title[0]) ink_ui_line(box, T.t[t].title, INK_FONT_LIST, INK_WEIGHT_MEDIUM, 0, 0, w - sw - INK_GAP);
        lv_obj_t *st = ink_ui_line(b, state_text(t), INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_MARGIN + w - sw, at + line - ink_ui_line_height(INK_FONT_NOTE), sw);
        lv_obj_set_style_text_color(st, ink_ui_secondary(), 0);
        T.row[T.rows].digits = ink_ui_line(box, digits, INK_FONT_HEADING, INK_WEIGHT_REGULAR, 0, line + (band - INK_LINE_HEADING) / 2, ink_ui_text_width("00:00:00", INK_FONT_HEADING, INK_WEIGHT_REGULAR));
        const char *args = ck_str("{\"id\":%d}", t);
        ck_btn(b, main_label(s), s == CK_RUNNING ? "clock.timer_pause" : s == CK_FINISHED ? "clock.timer_restart" : "clock.timer_start", args, bx, by, mw, INK_CONTROL, false, true);
        if (s != CK_IDLE) ck_btn(b, TR("重置"), "clock.timer_cancel", args, bx + mw + INK_GAP_M, by, rw, INK_CONTROL, false, true);
        T.row[T.rows].t = t;
        T.row[T.rows++].field = ck_field(ck_str("timer:%d", t), title_of(t), ck_str("%s%s%s", digits, s ? " · " : "", state_text(t)));
    }
    if (T.n >= CK_PER_MEMBER) ink_ui_notice(b, TR("最多 6 个计时器"), note);
    ck_btn(b, TR("+ 加计时器"), "clock.timer_new", NULL, INK_MARGIN, add, w, INK_PRIMARY, true, T.n < CK_PER_MEMBER);
}
bool ck_timer_running(void) {
    for (int k = 0; k < T.n; k++) if (ck_timer_state(T.order[k]) == CK_RUNNING) return true;
    return false;
}
bool ck_timer_ticks(void) { /* once a second: only the digits of the running rows, only when their second changed */
    for (unsigned i = 0; i < T.rows; i++) {
        int t = T.row[i].t; char digits[16], value[96];
        if (!ck_timer_tick(t)) continue;
        shown(t, digits, sizeof digits); snprintf(value, sizeof value, "%s · %s", digits, state_text(t));
        ck_text(T.row[i].digits, digits, true); ck_value(T.row[i].field, value);
    }
    return ck_timer_running();
}
void ck_timer_extra(ink_jw_t *w) {
    char s[96];
    ink_jw_raw(w, ",\"timers\":[");
    for (int k = 0; k < T.n; k++) {
        int t = T.order[k]; static const char *const STATE[] = {"idle", "running", "paused", "finished"};
        snprintf(s, sizeof s, "%s{\"id\":%d,\"title\":", k ? "," : "", t); ink_jw_raw(w, s); ink_jw_str(w, T.t[t].title);
        snprintf(s, sizeof s, ",\"seconds\":%u,\"remaining_seconds\":%u,\"state\":\"%s\"}", (unsigned)ck_timer_duration(t),
                 (unsigned)((ck_timer_left_ms(t) + 999) / 1000), STATE[ck_timer_state(t)]);
        ink_jw_raw(w, s);
    }
    snprintf(s, sizeof s, "],\"current_timer\":%d", ck_me->timer); ink_jw_raw(w, s);
}
int ck_timer_ring(int t) { /* 计时结束, "泡面 · 14:05 到点"; again or close; the second button is the power key's */
    ink_interrupt_t it = {.kind = INK_INT_ALARM, .ref = (uint32_t)t, .ringing = true, .nactions = 2,
        .actions = {{.label = TR("再计一次"), .cap = "clock.timer_restart"}, {.label = TR("关闭"), .cap = "clock.timer_dismiss"}}};
    ink_tm_t now; ink_time_to_local(ink_time_now_s(), &now);
    snprintf(it.title, sizeof it.title, "%s", TR("计时结束"));
    snprintf(it.body, sizeof it.body, TR("%s · %02d:%02d 到点"), title_of(t), now.hour, now.min);
    for (int i = 0; i < 2; i++) snprintf(it.actions[i].args, sizeof it.actions[i].args, "{\"id\":%d}", t);
    return ink_member_exists(ck_member_id(t / CK_PER_MEMBER)) ? ink_interrupt_raise(&it) : -1;
}
void ck_timer_finished(int t) { if (ck_on("Clock") && ck_me && ck_me->tab == TAB_TIMER && mine(t)) ck_refresh(); }

/* ---- the editor: hours, minutes and seconds with InfiniTime's counters ---- */
static void duration_value(void) {
    char s[16]; snprintf(s, sizeof s, "%02d:%02d:%02d", ck_edit_get(CK_HOUR), ck_edit_get(CK_MINUTE), ck_edit_get(CK_SECOND));
    ck_value(T.duration_field, s);
}
void ck_timer_edit_label(int part) { ck_counter_label(part); duration_value(); }
static void set_counters(uint32_t seconds) { ck_edit_set(CK_HOUR, (int)(seconds / 3600)); ck_edit_set(CK_MINUTE, (int)(seconds / 60 % 60)); ck_edit_set(CK_SECOND, (int)(seconds % 60)); }
static void edit_create(lv_obj_t *s, void *arg) {
    (void)arg; bool old = T.edit >= 0; int w = INK_CANVAS_W - 2 * INK_MARGIN, y = INK_STATUS_BAR + INK_PRIMARY + INK_GAP;
    ck_begin("ClockTimerEdit", old ? TR("计时器") : TR("加计时器"), ""); CK.spec.object.id = ck_str("timer:%d", T.edit);
    ink_shell_statusbar(s, true); ink_shell_topbar(s, old ? TR("计时器") : TR("加计时器"));
    T.duration_field = ck_field("duration", TR("时长"), ""); duration_value();
    ck_counter_row(s, CK_HOUR, y, TR("时－"), TR("时＋"), TR("时")); y += INK_CONTROL + INK_GAP_XS;
    ck_counter_row(s, CK_MINUTE, y, TR("分－"), TR("分＋"), TR("分")); y += INK_CONTROL + INK_GAP_XS;
    ck_counter_row(s, CK_SECOND, y, TR("秒－"), TR("秒＋"), TR("秒")); y += INK_CONTROL + INK_GAP_XS;
    static const char *const QUICK[] = {TR_NOOP("1 分钟"), TR_NOOP("5 分钟"), TR_NOOP("15 分钟"), TR_NOOP("30 分钟")};
    static const int SECONDS[] = {60, 300, 900, 1800};
    int cols = 4, bw = (w - 3 * INK_GAP_M) / 4;
    for (int i = 0; i < 4; i++) if (ink_ui_text_width(TR(QUICK[i]), INK_FONT_LIST, INK_WEIGHT_MEDIUM) + 2 * INK_GAP > bw) cols = 2; /* 2 x 2 at the largest text */
    bw = (w - (cols - 1) * INK_GAP_M) / cols;
    for (int i = 0; i < 4; i++)
        ck_btn(s, TR(QUICK[i]), "clock.timer_set", ck_str("{\"seconds\":%d}", SECONDS[i]), INK_MARGIN + i % cols * (bw + INK_GAP_M), y + i / cols * (INK_CONTROL + INK_GAP_XS), bw, INK_CONTROL, false, true);
    y = ck_name(s, y + (4 / cols) * (INK_CONTROL + INK_GAP_XS), old ? T.t[T.edit].title : "", TR("计时器名称"), 45, "clock.timer_name") + INK_GAP_XS;
    if (T.error[0]) ink_ui_notice(s, T.error, y);
    ck_field("error", TR("提示"), T.error);
    int bottom = INK_CANVAS_H - INK_MARGIN - INK_CONTROL;
    if (old) ck_btn(s, TR("删除这个计时器"), "clock.timer_delete", ck_str("{\"id\":%d}", T.edit), INK_MARGIN, bottom, (w - INK_GAP) / 2, INK_CONTROL, false, true);
    ck_btn(s, TR("保存"), "clock.timer_save", NULL, INK_MARGIN + (old ? (w + INK_GAP) / 2 : 0), bottom, old ? (w - INK_GAP) / 2 : w, INK_CONTROL, true, true);
}
static void edit_destroy(void) { CK.name = NULL; }
static const ink_activity_t EDIT = {.name = "ClockTimerEdit", .app = "clock", .create = edit_create, .destroy = edit_destroy, .snapshot = ck_name_snapshot};
static void save(ink_call_t *c) {
    uint32_t seconds = (uint32_t)(ck_edit_get(CK_HOUR) * 3600 + ck_edit_get(CK_MINUTE) * 60 + ck_edit_get(CK_SECOND));
    if (!seconds) seconds = 10; /* Fossify: zero becomes ten seconds (G7) */
    int t = T.edit, id = load() && (t >= 0 || T.n < CK_PER_MEMBER) ? put(t, CK.name ? ink_textfield_text(CK.name) : "", seconds) : 0;
    if (!id) { snprintf(T.error, sizeof T.error, "%s", TR("没能保存，请重试")); c->rc.status = INK_RC_FAILED; ink_ui_redraw(); return; }
    bool longer = t >= 0 && seconds != ck_timer_duration(t);
    load();
    for (int k = 0; k < T.n; k++) if (T.t[T.order[k]].rec == (uint32_t)id) t = T.order[k];
    if (longer) ck_timer_set(t, seconds); /* a new duration: idle at it; a new name only: it keeps going (G12) */
    ck_me->timer = (int8_t)t;
    ink_ui_pop();
}
static int current(bool make) { /* the one last set or started; else the first idle; else a new one */
    if (ck_me->timer >= 0 && mine(ck_me->timer)) return ck_me->timer;
    for (int k = 0; k < T.n; k++) if (ck_timer_state(T.order[k]) == CK_IDLE) return T.order[k];
    if (!make || T.n >= CK_PER_MEMBER) return -1;
    int id = put(-1, "", 300); load();
    for (int k = 0; k < T.n; k++) if (T.t[T.order[k]].rec == (uint32_t)id) return T.order[k];
    return -1;
}
void ck_timer_cap(ink_call_t *c) {
    const char *id = c->cap->id + 12; int64_t v = -1, seconds = 0; bool has = ink_args_get_int(&c->args, "id", &v);
    if (!strcmp(id, "dismiss") || !strcmp(id, "restart")) { /* from the card: whoever is using the device, also locked */
        if (v < 0 || v >= CK_TIMERS || !T.t[v].rec) { c->rc.status = INK_RC_STALE; return; }
        if (*id == 'd') ck_timer_set((int)v, ck_timer_duration((int)v));
        else if (ck_timer_state((int)v) == CK_FINISHED) ck_timer_restart((int)v);
        else { c->rc.status = INK_RC_STALE; return; }
        ck_timer_finished((int)v); return;
    }
    if (!strcmp(id, "name")) { ck_focus(c); return; }
    if (!strcmp(id, "save")) { save(c); return; }
    ink_args_get_int(&c->args, "seconds", &seconds);
    if (!strcmp(id, "set") && ck_on("ClockTimerEdit")) { set_counters((uint32_t)seconds); return; } /* the editor's draft */
    if (!load()) { c->rc.status = INK_RC_FAILED; return; }
    bool editing = ck_on("ClockTimerEdit") && !strcmp(id, "delete");
    if (!editing && (!ck_on("Clock") || ck_me->tab != TAB_TIMER)) { c->rc.status = INK_RC_STALE; return; }
    int t = has ? (int)v : current(!strcmp(id, "set") || !strcmp(id, "start"));
    if (strcmp(id, "new") && (t < 0 || !mine(t))) { c->rc.status = INK_RC_STALE; return; }
    if (!strcmp(id, "new") || !strcmp(id, "edit")) {
        if (*id == 'n' && T.n >= CK_PER_MEMBER) { c->rc.status = INK_RC_REJECTED; return; }
        T.edit = *id == 'n' ? -1 : t; T.error[0] = 0; set_counters(*id == 'n' ? 300 : ck_timer_duration(t));
        ink_ui_push(&EDIT, NULL); return;
    }
    int s = ck_timer_state(t);
    if (!strcmp(id, "delete")) {
        ink_db_del(T.db, T.t[t].rec, c->src); ck_timer_set(t, 0); T.t[t].rec = 0;
        if (editing) { ink_ui_pop(); return; }
    } else if (!strcmp(id, "set")) {
        if (s == CK_RUNNING || !put(t, T.t[t].title, (uint32_t)seconds)) { c->rc.status = INK_RC_REJECTED; return; }
        ck_timer_set(t, (uint32_t)seconds);
    } else if (!strcmp(id, "cancel")) ck_timer_set(t, ck_timer_duration(t)); /* 重置 */
    else if ((!strcmp(id, "start") && (s == CK_IDLE || s == CK_PAUSED)) || (!strcmp(id, "pause") && s == CK_RUNNING)) ck_timer_toggle(t);
    else { c->rc.status = INK_RC_REJECTED; return; }
    ck_me->timer = (int8_t)t;
    ck_refresh();
}
int ck_timer_init(void) { T.edit = -1; return ink_db_register(&TIMERS); }
