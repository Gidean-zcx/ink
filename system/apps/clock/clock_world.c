/* The clock tab after Fossify Clock (对照表 B): local time, the next alarm, world cities; pick and rename pages. UI task only. */
/* ui-scale-runtime: dimension w,x,y,h,row,per,bar,add,done Theme-derived coordinates supplied by this page. */
#include "clock_internal.h"

enum { CITIES = 6, ROWS = 6 };
static const ink_db_def_t WORLD = {.name = "clock.world", .title = TR_NOOP("世界时钟"), .scope = INK_DB_MEMBER,
    .schema = 1, .json = true, .ai_read = true, .max_records = CITIES}; /* {"zone":7} or {"zone":7,"title":"老妹家"} */
static struct {
    ink_db_t *db; char owner[16];
    struct { int zone, field, action; lv_obj_t *time, *note; } row[ROWS + 3];
    unsigned rows, pick_page, pick_pages;
    int zone, local_field, date_field;
    lv_obj_t *local, *date;
    ink_json_t json;
    char title[48];
} W INK_PSRAM_BSS;
static const ink_activity_t PICK, CITY;

static ink_db_t *world_db(void) {
    if (W.db && !strcmp(W.owner, ink_member_current()->id)) return W.db;
    ink_db_close(W.db);
    snprintf(W.owner, sizeof W.owner, "%s", ink_member_current()->id);
    return W.db = ink_db_open_member(&WORLD, W.owner);
}
static bool chosen(int zone, char *title, size_t cap) { /* its own name, "" for the table's */
    char buf[160]; ink_rec_t r; title[0] = 0;
    if (!world_db() || ink_db_get(W.db, (uint32_t)zone + 1, &r, buf, sizeof buf - 1)) return false;
    buf[r.len < sizeof buf - 1 ? r.len : sizeof buf - 1] = 0;
    if (!ink_json_parse(&W.json, buf, strlen(buf))) ink_json_str(&W.json, ink_json_find(&W.json, 0, "title"), title, cap);
    return true;
}
static const char *city_name(int zone) { return chosen(zone, W.title, sizeof W.title) && W.title[0] ? W.title : TR(ink_time_zone_city(zone)); }
static void city_text(int zone, char *time, char *note, size_t cap) { /* "19:00"; "昨天 · 比这里晚 13 小时" */
    if (!ink_time_valid()) { time[0] = 0; snprintf(note, cap, "%s", TR("时间尚未设置")); return; }
    int64_t utc = ink_time_now_s(); ink_tm_t here, there; ink_time_to_local(utc, &here); ink_time_to_zone(zone, utc, &there);
    snprintf(time, 8, "%02d:%02d", there.hour, there.min);
    int day = (there.year * 10000 + there.month * 100 + there.day) - (here.year * 10000 + here.month * 100 + here.day);
    int hours = (there.offset_min - here.offset_min) / 60; char offset[64];
    if (hours) snprintf(offset, sizeof offset, hours > 0 ? TR("比这里早 %d 小时") : TR("比这里晚 %d 小时"), hours > 0 ? hours : -hours);
    else snprintf(offset, sizeof offset, "%s", TR("和这里同一时间"));
    snprintf(note, cap, "%s%s%s", day > 0 ? TR("明天") : day < 0 ? TR("昨天") : "", day ? " · " : "", offset);
}
static void local_text(char *time, char *date) {
    ink_tm_t t; ink_time_to_local(ink_time_now_s(), &t);
    snprintf(time, 8, "%02d:%02d", t.hour, t.min);
    snprintf(date, 40, TR("%d月%d日 %s"), t.month, t.day, ck_wday(t.wday));
}
void ck_world_body(int h) {
    lv_obj_t *b = ck_body; int w = INK_CANVAS_W - 2 * INK_MARGIN, y = 0; bool valid = ink_time_valid();
    char time[8] = "", date[40] = "", note[96];
    if (valid) local_text(time, date);
    W.local = ink_ui_line(b, valid ? time : TR("时间尚未设置"), valid ? INK_FONT_NUMBER : INK_FONT_LIST, INK_WEIGHT_REGULAR, INK_MARGIN, y, w);
    W.local_field = ck_field("local_time", TR("本地时间"), valid ? time : TR("时间尚未设置"));
    y += valid ? INK_LINE_NUMBER : ink_ui_line_height(INK_FONT_LIST) + INK_GAP_XS;
    W.date = NULL;
    if (valid) {
        W.date = ink_ui_line(b, date, INK_FONT_LIST, INK_WEIGHT_REGULAR, INK_MARGIN, y, w); W.date_field = ck_field("date", TR("日期"), date);
        y += ink_ui_line_height(INK_FONT_LIST) + INK_GAP_XS;
    }
    const char *next = ck_next_alarm(false);
    if (valid && next[0]) {
        const char *line = ck_str(TR("下一个闹钟：%s"), next);
        ink_ui_line(b, line, INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_MARGIN, y, w); ck_field("next", TR("下一个闹钟"), line);
        y += ink_ui_line_height(INK_FONT_NOTE) + INK_GAP_XS;
    }
    y += INK_GAP;
    W.rows = 0;
    if (!world_db()) { ink_ui_notice(b, TR("没能读取城市，请返回重试"), y); ck_pager(b, 0, &ck_me->page[TAB_WORLD], 0, 1); return; }
    int add = h - INK_MARGIN - INK_PRIMARY, bar = add - INK_GAP_M - INK_CONTROL, row = ink_ui_row_double(), count = ink_db_count(W.db);
    int per = (add - INK_GAP_M - y + INK_GAP_M) / (row + INK_GAP_M);
    if (count > per) per = LV_MAX(1, (bar - INK_GAP_M - y + INK_GAP_M) / (row + INK_GAP_M));
    unsigned first = ck_pager(b, bar, &ck_me->page[TAB_WORLD], (unsigned)count, (unsigned)per);
    ink_db_cursor_t c = {0}; ink_rec_t r; char buf[160];
    for (unsigned i = 0; ink_db_next(W.db, &c, &r, buf, sizeof buf - 1) == 1 && W.rows < (unsigned)per && W.rows < ROWS; i++) {
        if (i < first) continue;
        int zone = (int)r.id - 1, at = y + (int)W.rows * (row + INK_GAP_M);
        const char *name = city_name(zone);
        lv_obj_t *box = ink_ui_box(b, INK_MARGIN, at, w, row, 0, INK_SURFACE_WHITE, false);
        ck_tap(box, name, "clock.world_edit", ck_str("{\"zone\":%d}", zone));
        city_text(zone, time, note, sizeof note);
        int tw = ink_ui_text_width("00:00", INK_FONT_LIST, INK_WEIGHT_MEDIUM), top = (row - ink_ui_line_height(INK_FONT_LIST) - ink_ui_line_height(INK_FONT_NOTE)) / 2;
        ink_ui_line(box, name, INK_FONT_LIST, INK_WEIGHT_MEDIUM, 0, top, w - tw - INK_GAP);
        W.row[W.rows].time = ink_ui_line(box, time, INK_FONT_LIST, INK_WEIGHT_MEDIUM, w - tw, top, tw);
        W.row[W.rows].note = ink_ui_line(box, note, INK_FONT_NOTE, INK_WEIGHT_REGULAR, 0, top + ink_ui_line_height(INK_FONT_LIST), w);
        lv_obj_set_style_text_color(W.row[W.rows].note, ink_ui_secondary(), 0);
        W.row[W.rows].zone = zone;
        W.row[W.rows++].field = ck_field(ink_time_zone_name(zone), name, ck_str("%s · %s", time, note));
    }
    ck_btn(b, TR("加城市"), "clock.world_pick", NULL, INK_MARGIN, add, w, INK_PRIMARY, true, true);
}
void ck_world_minute(void) { /* the time column, the local time, the date when the day turned: in place */
    if (!ink_time_valid() || !W.date) { ck_refresh(); return; }
    char time[8], date[40], note[96], value[128];
    local_text(time, date);
    ck_text(W.local, time, true); ck_value(W.local_field, time); ck_text(W.date, date, false); ck_value(W.date_field, date);
    for (unsigned i = 0; i < W.rows; i++) {
        city_text(W.row[i].zone, time, note, sizeof note);
        snprintf(value, sizeof value, "%s · %s", time, note);
        ck_text(W.row[i].time, time, true); ck_text(W.row[i].note, note, false); ck_value(W.row[i].field, value);
    }
}
void ck_world_extra(ink_jw_t *w) {
    char time[8], note[96];
    ink_jw_raw(w, ",\"cities\":[");
    for (unsigned i = 0; i < W.rows; i++) {
        city_text(W.row[i].zone, time, note, sizeof note);
        ink_jw_raw(w, i ? ",{\"zone\":" : "{\"zone\":"); ink_jw_str(w, ink_time_zone_name(W.row[i].zone));
        ink_jw_raw(w, ",\"name\":"); ink_jw_str(w, city_name(W.row[i].zone)); ink_jw_raw(w, ",\"time\":"); ink_jw_str(w, time);
        ink_jw_raw(w, ",\"note\":"); ink_jw_str(w, note); ink_jw_raw(w, "}");
    }
    ink_jw_raw(w, "]");
}

/* ---- adding cities: each tap writes at once and redraws its own row ---- */
static void pick_update(void) {
    int count = world_db() ? ink_db_count(W.db) : CITIES; char title[48];
    for (unsigned i = 0; i < W.rows; i++) {
        bool on = chosen(W.row[i].zone, title, sizeof title);
        const char *state = on ? TR("已选") : count >= CITIES ? TR("最多 6 个城市") : "";
        ck_text(W.row[i].note, state, false); ck_value(W.row[i].field, state);
        CK.a[W.row[i].action].capability = on ? "clock.world_remove" : "clock.world_add";
        CK.a[W.row[i].action].enabled = on || count < CITIES;
    }
}
static void picked(lv_event_t *e) {
    int zone = (int)(intptr_t)lv_event_get_user_data(e); char title[48], args[24]; ink_receipt_t rc;
    snprintf(args, sizeof args, "{\"zone\":%d}", zone);
    ink_cap_invoke_json(chosen(zone, title, sizeof title) ? "clock.world_remove" : "clock.world_add", args, INK_SRC_TOUCH, ink_ui_content_version(), &rc);
}
static void pick_create(lv_obj_t *s, void *arg) {
    (void)arg; int w = INK_CANVAS_W - 2 * INK_MARGIN, y = INK_STATUS_BAR + INK_PRIMARY + INK_GAP, n = ink_time_zone_count();
    ck_begin("ClockWorldPick", TR("加城市"), "clock:world_pick");
    ink_shell_statusbar(s, true); ink_shell_topbar(s, TR("加城市"));
    int done = INK_CANVAS_H - INK_MARGIN - INK_CONTROL, bar = done - INK_GAP_M - INK_CONTROL;
    int per = (done - INK_GAP - y + INK_GAP_M) / (INK_CONTROL + INK_GAP_M);
    if (n > per) per = (bar - INK_GAP_M - y + INK_GAP_M) / (INK_CONTROL + INK_GAP_M);
    W.pick_pages = (unsigned)((n + per - 1) / per);
    int first = (int)ck_pager(s, bar, &W.pick_page, (unsigned)n, (unsigned)per); W.rows = 0;
    for (int z = first; z < n && z < first + per; z++) {
        lv_obj_t *row = ink_ui_card(s, INK_MARGIN, y + (int)W.rows * (INK_CONTROL + INK_GAP_M), w, INK_CONTROL, INK_SURFACE_WHITE);
        lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE); lv_obj_add_event_cb(row, picked, LV_EVENT_CLICKED, (void *)(intptr_t)z);
        int sw = ink_ui_text_width(TR("最多 6 个城市"), INK_FONT_NOTE, INK_WEIGHT_REGULAR), ly = (INK_CONTROL - ink_ui_line_height(INK_FONT_LIST)) / 2;
        ink_ui_line(row, TR(ink_time_zone_city(z)), INK_FONT_LIST, INK_WEIGHT_MEDIUM, INK_GAP_M, ly, w - sw - 3 * INK_GAP_M);
        W.row[W.rows].note = ink_ui_line(row, "", INK_FONT_NOTE, INK_WEIGHT_REGULAR, w - sw - INK_GAP_M, (INK_CONTROL - ink_ui_line_height(INK_FONT_NOTE)) / 2, sw);
        lv_obj_set_style_text_align(W.row[W.rows].note, LV_TEXT_ALIGN_RIGHT, 0);
        W.row[W.rows].zone = z; W.row[W.rows].field = ck_field(ink_time_zone_name(z), TR(ink_time_zone_city(z)), "");
        W.row[W.rows].action = (int)CK.spec.action_count; ck_declare(TR(ink_time_zone_city(z)), "clock.world_add", ck_str("{\"zone\":%d}", z), true);
        W.rows++;
    }
    pick_update();
    ck_btn(s, TR("完成"), "clock.world_pick", NULL, INK_MARGIN, done, w, INK_CONTROL, true, true);
}
static void pick_extra(ink_jw_t *w) {
    char s[96]; snprintf(s, sizeof s, ",\"picking\":true,\"page_number\":%u,\"pages\":%u,\"total\":%d", W.pick_page + 1, W.pick_pages, ink_time_zone_count());
    ink_jw_raw(w, s);
}
static int pick_snapshot(char *out, size_t cap) { return ck_snapshot(out, cap, pick_extra); }
static void pick_destroy(void) { W.rows = 0; }
static bool pick_key(ink_key_t key, bool lng) {
    if (lng || (key != INK_KEY_NEXT && key != INK_KEY_PREV)) return false;
    ck_world_turn(key == INK_KEY_NEXT ? 1 : -1); return true;
}
void ck_world_turn(int delta) {
    if (delta < 0 ? !W.pick_page : W.pick_page + 1 >= W.pick_pages) return;
    W.pick_page = (unsigned)((int)W.pick_page + delta); ink_ui_redraw();
}
static const ink_activity_t PICK = {.name = "ClockWorldPick", .app = "clock", .create = pick_create, .destroy = pick_destroy, .snapshot = pick_snapshot, .on_key = pick_key};

/* ---- a city's own page: its name (empty: the table's), or remove it ---- */
static void city_saved(lv_event_t *e) {
    (void)e; char args[128]; ink_jw_t w; ink_receipt_t rc;
    ink_jw_init(&w, args, sizeof args); snprintf(args, sizeof args, "{\"zone\":%d,\"title\":", W.zone); w.len = strlen(args);
    ink_jw_str(&w, CK.name ? ink_textfield_text(CK.name) : ""); ink_jw_raw(&w, "}");
    if (!w.overflow) ink_cap_invoke_json("clock.world_title", args, INK_SRC_TOUCH, ink_ui_content_version(), &rc);
}
static void city_create(lv_obj_t *s, void *arg) {
    (void)arg; int w = INK_CANVAS_W - 2 * INK_MARGIN, y = INK_STATUS_BAR + INK_PRIMARY + INK_GAP; char title[48];
    chosen(W.zone, title, sizeof title);
    ck_begin("ClockCity", TR(ink_time_zone_city(W.zone)), ""); CK.spec.object.id = ck_str("city:%s", ink_time_zone_name(W.zone));
    ink_shell_statusbar(s, true); ink_shell_topbar(s, TR(ink_time_zone_city(W.zone)));
    ck_field("zone", TR("城市"), ink_time_zone_name(W.zone));
    ck_name(s, y, title, TR(ink_time_zone_city(W.zone)), 45, "clock.world_name");
    int bottom = INK_CANVAS_H - INK_MARGIN - INK_CONTROL;
    ck_btn(s, TR("删除这个城市"), "clock.world_remove", ck_str("{\"zone\":%d}", W.zone), INK_MARGIN, bottom, (w - INK_GAP) / 2, INK_CONTROL, false, true);
    ink_ui_pill(s, TR("保存"), INK_MARGIN + (w + INK_GAP) / 2, bottom, (w - INK_GAP) / 2, INK_CONTROL, true, city_saved, NULL);
    ck_declare(TR("保存"), "clock.world_title", ck_str("{\"zone\":%d}", W.zone), true);
}
static void city_destroy(void) { CK.name = NULL; }
static const ink_activity_t CITY = {.name = "ClockCity", .app = "clock", .create = city_create, .destroy = city_destroy, .snapshot = ck_name_snapshot};

static int put(int zone, const char *title, uint8_t src) {
    char json[128]; ink_jw_t w; ink_jw_init(&w, json, sizeof json);
    snprintf(json, sizeof json, "{\"zone\":%d", zone); w.len = strlen(json);
    if (title && title[0]) { ink_jw_raw(&w, ",\"title\":"); ink_jw_str(&w, title); }
    ink_jw_raw(&w, "}");
    ink_db_write_t rec = {.id = (uint32_t)zone + 1, .sort = zone, .data = json, .len = w.len, .src = src};
    return w.overflow ? -1 : ink_db_put(W.db, &rec, NULL);
}
void ck_world_cap(ink_call_t *c) {
    const char *id = c->cap->id + 6; int64_t zone = -1; char title[48] = "", old[48];
    if (!strcmp(id, "world_name")) { ck_focus(c); return; }
    if (!strcmp(id, "world_pick")) {
        if (ck_on("ClockWorldPick")) ink_ui_pop();
        else if (ck_on("Clock")) { W.pick_page = 0; ink_ui_push(&PICK, NULL); }
        else c->rc.status = INK_RC_STALE;
        return;
    }
    ink_args_get_int(&c->args, "zone", &zone);
    if (!world_db() || zone < 0 || zone >= ink_time_zone_count()) { c->rc.status = INK_RC_FAILED; return; }
    bool had = chosen((int)zone, old, sizeof old);
    if (!strcmp(id, "world_edit")) { if (!had) c->rc.status = INK_RC_STALE; else { W.zone = (int)zone; ink_ui_push(&CITY, NULL); } return; }
    if (!strcmp(id, "world_add") && !had && (ink_db_count(W.db) >= CITIES || put((int)zone, NULL, c->src))) {
        c->rc.status = INK_RC_FAILED; snprintf(c->rc.detail, sizeof c->rc.detail, "full: at most 6 cities"); return;
    }
    if (!strcmp(id, "world_remove") && had && ink_db_del(W.db, (uint32_t)zone + 1, c->src)) { c->rc.status = INK_RC_FAILED; return; }
    if (!strcmp(id, "world_title")) {
        ink_args_get_str(&c->args, "title", title, sizeof title);
        if (!had || put((int)zone, title, c->src)) { c->rc.status = had ? INK_RC_FAILED : INK_RC_STALE; return; }
    }
    if (ck_on("ClockWorldPick")) pick_update();
    else if (ck_on("ClockCity")) ink_ui_pop();
    else if (ck_on("Clock")) ck_refresh();
}
int ck_world_init(void) { return ink_db_register(&WORLD); }
