/* Calls (phone-3): the dial pad, recent calls, call details and the call screen after Fossify Phone, adapted to
 * e-ink (docs/tasks/phone-3/对照表.md); number matching, caller names and the call card are LilyPhone's code
 * (vendor/lilyphone). The modem side is comm_service.c; the member data store keeps each member's history. */
/* ui-scale-runtime: dimension w,y,height,width,dw,lw,x,side,text,pager,name_w,nw theme sizes passed on and widths measured from text */
/* ui-scale-exception: 140 the emergency dialler (D-119) keeps its layout */
/* ui-scale-exception: 218 the emergency dialler (D-119) keeps its layout */
/* ui-scale-exception: 270 the emergency dialler (D-119) keeps its layout */
/* ui-scale-exception: 964 the emergency dialler (D-119) keeps its layout */
#include <stdio.h>
#include <string.h>
#include "ink/sys/app.h"
#include "ink/sys/comm.h"
#include "ink/sys/data.h"
#include "ink/sys/json.h"
#include "ink/sys/i18n.h"
#include "ink/sys/members.h"
#include "ink/sys/power.h"
#include "ink/sys/settings.h"
#include "ink/sys/state.h"
#include "ink/sys/textfield.h"
#include "ink/sys/time.h"
#include "ink/sys/ui_list.h"
#include "ink/ui_shell.h"
#include "vendor/lilyphone/lily_compat.h"

extern void ink_contacts_open(void);
void ink_phone_tabs(lv_obj_t *screen, int selected);
const ink_activity_t ink_phone_page;
static const ink_activity_t RECENTS, CALL, KEYPAD, DETAIL;
/* Schema 1 still, and no max_records: either would stop old backups from restoring (datastore.c ink_db_import refuses a
 * new schema or more records than the limit; old backups can hold more than 500). New records only add seconds, result
 * and sim. 500 records, the oldest going first like Android's call log: on_call trims after each new record. */
static const ink_db_def_t HISTORY = {.name = "phone.calls", .title = "通话记录", .scope = INK_DB_MEMBER,
    .schema = 1, .json = true, .ai_read = true};
/* TRASH: a group holds at most this many calls, so phone.delete can always keep the whole group for undo. */
enum { LOG_MAX = 500, CAND_MAX = CONTACTS_MAX + 32, SLOTS = 3, ROWS = 6, TRASH = 64, PAGER_W = 2 * INK_CONTROL + INK_GAP };
#define TRASH_TEXT "64" /* phone.delete's count limit in its schema */
_Static_assert(TRASH == 64, "TRASH_TEXT");
typedef struct { uint32_t id; int64_t at; int32_t seconds; char number[24], kind[12], result[12], sim[12]; } call_t;
typedef struct { uint16_t first, count; } group_t;
static struct { /* UI executor: this member's history and contacts, read once per page */
    call_t calls[LOG_MAX];
    group_t groups[LOG_MAX];
    contact_t book[CONTACTS_MAX];
    uint32_t book_id[CONTACTS_MAX];
    int16_t cand[CAND_MAX]; /* >= 0: a contact; < 0: -(group + 1), a number called but not saved */
    int ncalls, ngroups, nbook, ncand;
} X INK_PSRAM_BSS;
static struct {
    char member[16], number[24], sim[12];
    bool incoming, answered, active;
    uint64_t started;
    int64_t at;
} H INK_PSRAM_BSS; /* BG executor only. */
static struct { uint32_t token; int count; int64_t sort[TRASH]; char raw[TRASH][256]; char member[16]; } T INK_PSRAM_BSS; /* BG: the last deleted group */
static struct {
    char member[16], number[24], call_member[16], calling[24], status[32], end_text[64], reason[16], dial_args[64];
    char ids[ROWS][32], numbers[ROWS][24], titles[ROWS][128], details[ROWS][128], args[ROWS][64], detail_args[ROWS][32];
    char tones[65], tone_count[12], route[80], tone_args[12][16], card[48], call_sim[12];
    char duration_text[32], say[160], undo[48], undo_args[80], sms_args[64], view_args[32], delete_args[48];
    char cand_values[SLOTS][128], cand_args[SLOTS][64], pager[32], extra[3072];
    uint64_t started;
    int64_t minutes, seconds;
    uint32_t detail;
    int rows, cpage, pages, first, cand_y, side_w, pager_w, contacts; /* contacts: 1 read, 0 reading, -1 unreadable */



    bool selecting, active, muted, incoming, headset, log_failed;

    ink_exec_job_t refresh;
    ink_db_t *db;
    ink_ui_list_t *list;
    ink_ui_list_position_t position, detail_position;
    lv_obj_t *field, *notice, *duration, *status_line, *typed, *card_line, *bar, *ends, *pager_btn, *add_btn;
    lv_obj_t *slots[SLOTS], *controls[4];
    ink_ui_field_spec_t fields[12];
    ink_ui_action_spec_t actions[16];
    ink_ui_page_spec_t model;
} P INK_PSRAM_BSS; /* UI executor only. */

static struct { lv_obj_t *root, *number_label, *notice; char number[24]; bool active; } E;
static const ink_ui_overlay_t EMERGENCY;
static void emergency_draw(void);
static void emergency_close(void) {
    if (!E.root || E.active) return;
    ink_ui_overlay_remove(&EMERGENCY); lv_obj_delete(E.root); memset(&E, 0, sizeof E);
    ink_power_release(INK_NEED_AWAKE); ink_power_lock(); ink_ui_hint_frame(INK_REFRESH_FLASH_UI);
}
static bool emergency_key(ink_key_t key, bool lng) {
    if (!lng && (key == INK_KEY_HOME || key == INK_KEY_BACK)) emergency_close();
    return true;
}
static int emergency_snapshot(char *out, size_t cap) {
    return snprintf(out, cap, "%s；%s：%.20s；%s；110、120、119、112；%s",
        TR("紧急呼叫"), TR("号码"), E.number, lv_label_get_text(E.notice), TR("请在紧急呼叫页面亲自拨号"));
}
static const char *const EMERGENCY_ALLOWED[] = {"phone.emergency_open", "phone.emergency", "phone.emergency_key", "phone.emergency_close", "phone.emergency_hangup", "system.home", "shell.back", NULL};
static const ink_activity_t EMERGENCY_PAGE = {.name = "PhoneEmergency", .locked_caps = EMERGENCY_ALLOWED, .on_key = emergency_key};
static const ink_ui_overlay_t EMERGENCY = {.name = "PhoneEmergency", .activity = &EMERGENCY_PAGE, .level = 3, .snapshot = emergency_snapshot, .on_key = emergency_key};
static void emergency_open(void) {
    if (E.root) return;
    if (!E.root) {
        ink_power_hold(INK_NEED_AWAKE);
        E.number[0] = 0;
        E.root = ink_ui_box(lv_layer_sys(), 0, 0, INK_CANVAS_W, INK_CANVAS_H, 0, INK_SURFACE_WHITE, false);
        lv_obj_add_flag(E.root, LV_OBJ_FLAG_CLICKABLE);
        ink_ui_overlay_add(&EMERGENCY, E.root);
    }
    emergency_draw();
}
static void emergency_reply(void *ctx, const ink_receipt_t *rc) {
    (void)ctx;
    if (E.root && rc->status != INK_RC_DONE) { lv_label_set_text(E.notice, rc->say[0] ? rc->say : TR("没有完成，请重试")); ink_ui_hint_obj(E.notice, INK_REFRESH_UI); }
}
static void emergency_dial(lv_event_t *e) {
    const char *number = lv_event_get_user_data(e);
    char args[64]; snprintf(args, sizeof args, "{\"number\":\"%.20s\"}", number ? number : E.number);
    ink_call_opts_t o = {.src = INK_SRC_TOUCH, .args_json = args, .reply_exec = INK_EXEC_UI, .done = emergency_reply};
    ink_cap_call("phone.emergency", NULL, &o);
}
static void emergency_draw(void) {
    lv_obj_clean(E.root);
    ink_ui_line(E.root, TR("紧急呼叫"), INK_FONT_TITLE, INK_WEIGHT_BOLD, INK_MARGIN, INK_STATUS_BAR, INK_CANVAS_W - 2 * INK_MARGIN);
    E.number_label = ink_ui_line(E.root, E.number, INK_FONT_TITLE, INK_WEIGHT_MEDIUM, INK_MARGIN, 140, INK_CANVAS_W - 2 * INK_MARGIN);
    E.notice = ink_ui_notice(E.root, E.active ? TR("正在拨号") : TR("仅限紧急号码"), 218);
    if (E.active) {
        ink_shell_bind(ink_ui_pill(E.root, TR("挂断"), INK_MARGIN, INK_CANVAS_H - INK_MARGIN - INK_PRIMARY,
            INK_CANVAS_W - 2 * INK_MARGIN, INK_PRIMARY, true, NULL, NULL), "phone.emergency_hangup", NULL);
    } else {
        static const char *const keys[] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "*", "0", "#"};
        int cell = (INK_CANVAS_W - 2 * INK_MARGIN) / 3;
        for (unsigned i = 0; i < sizeof keys / sizeof *keys; i++) {
            char args[24]; snprintf(args, sizeof args, "{\"key\":\"%s\"}", keys[i]);
            lv_obj_t *b = ink_ui_pill(E.root, keys[i], INK_MARGIN + (i % 3) * cell + (cell - INK_DIGIT) / 2, 270 + (i / 3) * (INK_DIGIT + INK_GAP_S),
                INK_DIGIT, INK_DIGIT, false, NULL, NULL);
            ink_ui_pill_text(b, INK_FONT_HEADING, INK_WEIGHT_MEDIUM); ink_shell_bind(b, "phone.emergency_key", args);
        }
        ink_ui_pill(E.root, TR("拨号"), INK_MARGIN, 964, cell - INK_GAP, INK_CONTROL, true, emergency_dial, NULL);
        ink_shell_bind(ink_ui_pill(E.root, TR("删除"), INK_MARGIN + cell, 964, cell - INK_GAP, INK_CONTROL, false, NULL, NULL), "phone.emergency_key", "{\"key\":\"delete\"}");
        ink_shell_bind(ink_ui_pill(E.root, TR("返回"), INK_MARGIN + 2 * cell, 964, cell - INK_GAP, INK_CONTROL, false, NULL, NULL), "phone.emergency_close", NULL);
        static const char *const numbers[] = {"110", "120", "119", "112"};
        int w = (INK_CANVAS_W - 2 * INK_MARGIN - 3 * INK_GAP_S) / 4;
        for (unsigned i = 0; i < sizeof numbers / sizeof *numbers; i++)
            ink_ui_pill(E.root, numbers[i], INK_MARGIN + i * (w + INK_GAP_S), INK_CANVAS_H - INK_MARGIN - INK_CONTROL, w, INK_CONTROL, false, emergency_dial, (void *)numbers[i]);
    }
    ink_ui_hint_frame(INK_REFRESH_FLASH_UI);
}
static void emergency_action(ink_call_t *c) {
    if (!strcmp(c->cap->id, "phone.emergency_open")) { emergency_open(); return; }
    if (!E.root || strcmp(ink_ui_top_name(), "PhoneEmergency") || E.active) { c->rc.status = INK_RC_REJECTED; return; }
    if (!strcmp(c->cap->id, "phone.emergency_close")) { emergency_close(); return; }
    char key[8] = ""; ink_args_get_str(&c->args, "key", key, sizeof key);
    size_t n = strlen(E.number);
    if (!strcmp(key, "delete")) { if (n) E.number[n - 1] = 0; }
    else if (strlen(key) == 1 && strchr("0123456789*#", key[0]) && n < 20) { E.number[n] = key[0]; E.number[n + 1] = 0; }
    else { c->rc.status = INK_RC_INVALID; return; }
    lv_label_set_text(E.number_label, E.number); ink_ui_content_changed(); ink_ui_hint_obj(E.number_label, INK_REFRESH_TEXT);
}
static bool emergency_changed(const char *id, const ink_args_t *a) {
    bool emergency = false; ink_args_get_bool(a, "emergency", &emergency);
    if (!strcmp(id, "call.dialed") && emergency) {
        emergency_open(); ink_args_get_str(a, "number", E.number, sizeof E.number); E.active = true; emergency_draw(); return true;
    }
    if (!E.active) return false;
    if (!strcmp(id, "call.connected")) { lv_label_set_text(E.notice, TR("通话中")); ink_ui_content_changed(); ink_ui_hint_obj(E.notice, INK_REFRESH_TEXT); }
    if (!strcmp(id, "call.ended")) { E.active = false; emergency_close(); }
    return true;
}
void ink_shell_emergency_button(lv_obj_t *root, bool strip) {
    lv_obj_t *b = ink_ui_pill(root, TR("紧急呼叫"), strip ? 0 : INK_MARGIN,
        strip ? 0 : INK_STATUS_BAR + INK_GAP, 2 * INK_CONTROL + 2 * INK_GAP, INK_CONTROL, false, NULL, NULL);
    ink_ui_pill_text(b, INK_FONT_NOTE, INK_WEIGHT_MEDIUM); ink_shell_bind(b, "phone.emergency_open", NULL);
}

/* ---- this member's contacts and history ---- */
static int64_t local_day(int64_t at) { ink_tm_t t; ink_time_to_local(at, &t); return (at + t.offset_min * 60) / 86400; }
/* The one place that reads phone.contacts records (contacts-3 will change their shape): LilyPhone's table, by name. */
static int load_contacts(void) {
    const ink_db_def_t *def = ink_db_find_def("phone.contacts");
    ink_db_t *db = def ? ink_db_open_cached_member(def, P.member) : NULL;
    ink_json_t *j = db ? ink_app_alloc(sizeof *j, true) : NULL;
    ink_db_cursor_t cur = {0}; ink_rec_t rec; char raw[1024];
    X.nbook = 0;
    while (j && X.nbook < CONTACTS_MAX && ink_db_next(db, &cur, &rec, raw, sizeof raw - 1) == 1) {
        contact_t c = {0};
        if (ink_json_parse(j, raw, rec.len < sizeof raw - 1 ? rec.len : sizeof raw - 1) || ink_json_str(j, ink_json_find(j, 0, "name"), c.name, sizeof c.name) < 0 ||
            ink_json_str(j, ink_json_find(j, 0, "number"), c.number, sizeof c.number) <= 0) continue;
        int at = X.nbook++;
        while (at && strcmp(X.book[at - 1].name, c.name) > 0) { X.book[at] = X.book[at - 1]; X.book_id[at] = X.book_id[at - 1]; at--; }
        X.book[at] = c; X.book_id[at] = rec.id;
    }
    ink_app_free(j); ink_db_close(db); lily_contacts_bind(X.book, X.nbook);
    return db ? 1 : 0;
}
/* Fossify's groups (RecentsHelper.kt:90-125): consecutive, same card, same day, same number. */
static bool same_group(const call_t *head, const call_t *c) {
    return !strcmp(head->sim, c->sim) && local_day(head->at) == local_day(c->at) && phone_number_match(head->number, c->number);
}
/* The fields a group is made of, from one record's JSON. */
static bool parse_call(ink_json_t *j, const char *raw, size_t len, call_t *c) {
    if (ink_json_parse(j, raw, len)) return false;
    ink_json_str(j, ink_json_find(j, 0, "number"), c->number, sizeof c->number);
    ink_json_str(j, ink_json_find(j, 0, "sim"), c->sim, sizeof c->sim);
    ink_json_int(j, ink_json_find(j, 0, "at"), &c->at);
    return true;
}
/* Records newest first, then the groups. */
static void load_log(void) {
    ink_json_t *j = P.db ? ink_app_alloc(sizeof *j, true) : NULL;
    ink_db_cursor_t cur = {.desc = true}; ink_rec_t rec; char raw[256];
    X.ncalls = X.ngroups = 0;
    while (j && X.ncalls < LOG_MAX && ink_db_next(P.db, &cur, &rec, raw, sizeof raw - 1) == 1) {
        call_t *c = &X.calls[X.ncalls]; int64_t v = 0;
        memset(c, 0, sizeof *c); c->id = rec.id;
        if (!parse_call(j, raw, rec.len < sizeof raw - 1 ? rec.len : sizeof raw - 1, c)) continue;
        ink_json_str(j, ink_json_find(j, 0, "kind"), c->kind, sizeof c->kind);
        ink_json_str(j, ink_json_find(j, 0, "result"), c->result, sizeof c->result);
        if (ink_json_int(j, ink_json_find(j, 0, "seconds"), &v) || (ink_json_int(j, ink_json_find(j, 0, "minutes"), &v) && (v *= 60, true))) c->seconds = (int32_t)v;
        const call_t *head = X.ngroups ? &X.calls[X.groups[X.ngroups - 1].first] : NULL;
        if (head && X.groups[X.ngroups - 1].count < TRASH && same_group(head, c)) X.groups[X.ngroups - 1].count++;
        else X.groups[X.ngroups++] = (group_t){(uint16_t)X.ncalls, 1};
        X.ncalls++;
    }
    ink_app_free(j);
}
static const char *card_name(const char *sim) { return !strcmp(sim, "internal") ? TR("内置卡") : sim[0] ? TR("卡 1") : ""; }
static void sim_in_use(char *out, size_t cap) { /* cell.sim once the modem has said; until then the card the setting picks */
    out[0] = 0; ink_state_get_str(ink_state_find("cell.sim"), out, cap);
    if (!out[0]) snprintf(out, cap, "%s", ink_setting_get("cell.sim_slot") == 1 ? "internal" : "external");
}
static void when(int64_t at, char *out, size_t cap) { /* RecentCallsAdapter.kt:510-516, the day named */
    ink_tm_t t; ink_time_to_local(at, &t);
    int64_t days = local_day(ink_time_now_s()) - local_day(at);
    if (days == 0) snprintf(out, cap, TR("今天 %02d:%02d"), t.hour, t.min);
    else if (days == 1) snprintf(out, cap, TR("昨天 %02d:%02d"), t.hour, t.min);
    else snprintf(out, cap, TR("%d月%d日"), t.month, t.day);
}
static void span(int64_t s, char *out, size_t cap) {
    if (s >= 60) snprintf(out, cap, TR("%d 分 %02d 秒"), (int)(s / 60), (int)(s % 60));
    else snprintf(out, cap, TR("%d 秒"), (int)s);
}
static void call_line(const call_t *c, char *out, size_t cap) { /* 打出 · 今天 14:05 · 1 分 05 秒; nothing for a call not answered */
    char at[32], length[32] = ""; when(c->at, at, sizeof at);
    if (c->seconds && strcmp(c->kind, "未接")) span(c->seconds, length, sizeof length);
    snprintf(out, cap, "%s · %s%s%s", TR(c->kind), at, length[0] ? " · " : "", length);
}

/* ---- dial pad: Fossify DialpadActivity, Lineage SmartDialNameMatcher rules ---- */
static void digits_of(const char *s, char *out, size_t cap) {
    size_t n = 0;
    for (; *s && n + 1 < cap; s++) if (*s >= '0' && *s <= '9') out[n++] = *s;
    out[n] = 0;
}
/* Fossify: the number contains the keys. Lineage :117-181: from its start, or after +86 / 0086, comes first. */
static int number_rank(const char *number, const char *keys) {
    char d[25]; digits_of(number, d, sizeof d);
    const char *hit = strstr(d, keys);
    if (!hit) return -1;
    return hit == d || (number[0] == '+' && !strncmp(d, "86", 2) && hit == d + 2) || (!strncmp(d, "0086", 4) && hit == d + 4) ? 0 : 1;
}
/* Lineage :400-434: the keys spell a word's beginning ("846" finds Phillips Thomas, "466" does not); Fossify's Latin keys. */
static bool name_keys(const char *name, const char *keys) {
    static const char T9[] = "22233344455566677778889999";
    for (const char *w = name; *w; w++) {
        bool letter = (*w | 32) >= 'a' && (*w | 32) <= 'z';
        if (!letter || (w > name && (w[-1] | 32) >= 'a' && (w[-1] | 32) <= 'z')) continue;
        const char *p = w, *k = keys;
        for (; *k && *p; p++) {
            if ((*p | 32) < 'a' || (*p | 32) > 'z') continue;
            if (T9[(*p | 32) - 'a'] != *k) break;
            k++;
        }
        if (!*k) return true;
    }
    return false;
}
static void match(void) { /* Fossify dialpadValueChanged: number hits from the start, then inside, then names; then numbers not saved */
    char keys[24]; digits_of(P.number, keys, sizeof keys);
    X.ncand = 0;
    for (int rank = 0; rank < 3; rank++)
        for (int i = 0; i < X.nbook && X.ncand < CAND_MAX; i++) {
            int r = number_rank(X.book[i].number, keys);
            if (r < 0 && keys[0] && name_keys(X.book[i].name, keys)) r = 2;
            if (r == rank) X.cand[X.ncand++] = (int16_t)i;
        }
    for (int g = 0; keys[0] && g < X.ngroups && X.ncand < CAND_MAX; g++) {
        const char *number = X.calls[X.groups[g].first].number;
        bool seen = contacts_find_by_number(number) >= 0;
        for (int k = 0; k < X.ncand && !seen; k++) seen = X.cand[k] < 0 && phone_number_match(X.calls[X.groups[-X.cand[k] - 1].first].number, number);
        if (!seen && number_rank(number, keys) >= 0) X.cand[X.ncand++] = (int16_t)(-g - 1);
    }
}
static void number_args(char *out, size_t cap, const char *number) {
    ink_jw_t w; ink_jw_init(&w, out, cap); ink_jw_raw(&w, "{\"number\":"); ink_jw_str(&w, number); ink_jw_raw(&w, "}");
    if (w.overflow) snprintf(out, cap, "{}");
}
/* In-place changes touch an object only when it really changes: an unchanged one does not join the refresh. */
static void show_if(lv_obj_t *o, bool shown) { if (o && lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN) == shown) lv_obj_set_flag(o, LV_OBJ_FLAG_HIDDEN, !shown); }
static void set_text(lv_obj_t *l, const char *s) { if (l && strcmp(lv_label_get_text(l), s)) lv_label_set_text(l, s); }
static void set_w(lv_obj_t *o, int w) { if (lv_obj_get_style_width(o, 0) != ink_ui_px(w)) lv_obj_set_width(o, ink_ui_px(w)); }
static void set_y(lv_obj_t *o, int y) { if (lv_obj_get_y(o) != ink_ui_px(y)) lv_obj_set_y(o, ink_ui_px(y)); }
static void set_x(lv_obj_t *o, int x) { if (lv_obj_get_style_x(o, 0) != ink_ui_px(x)) lv_obj_set_x(o, ink_ui_px(x)); }

static void card_status(void) { /* the card in use and the network, from cell.* (no SIM choice when dialling: D-128) */
    char sim[12]; int64_t registered = 0, missing = 0;
    sim_in_use(sim, sizeof sim);
    ink_state_get_int(ink_state_find("cell.registered"), &registered); ink_state_get_int(ink_state_find("cell.sim_missing"), &missing);
    if (ink_setting_get("net.airplane")) snprintf(P.card, sizeof P.card, "%s", TR("飞行模式"));
    else if (missing) snprintf(P.card, sizeof P.card, "%s", TR("没有卡"));
    else snprintf(P.card, sizeof P.card, "%s%s%s", card_name(sim), registered ? "" : " · ", registered ? "" : TR("没有信号"));
    set_text(P.card_line, P.card);
}
/* The candidate rows, in place: the notice first (until the next action), then a page of candidates or why there are none. */
static void cands_show(void) {
    bool say = P.say[0], none = !X.ncand, add = none && P.number[0] && P.contacts > 0;
    int per = P.rows - say, w = INK_CANVAS_W - 2 * INK_MARGIN;
    P.pages = none || per < 1 ? 1 : (X.ncand + per - 1) / per;
    if (P.cpage >= P.pages) P.cpage = 0;
    P.model.field_count = 2; P.model.action_count = 5;
    set_text(P.notice, P.say); show_if(P.notice, say);
    number_args(P.dial_args, sizeof P.dial_args, P.number[0] || !X.ncalls ? P.number : X.calls[0].number);
    P.actions[1] = (ink_ui_action_spec_t){.id = "dial", .label = P.number[0] || !X.ncalls ? TR("拨打") : TR("填上最近一次的号码"),
        .capability = P.number[0] || !X.ncalls ? "call.dial" : "phone.open", .args_json = P.dial_args, .enabled = true};
    P.fields[1] = (ink_ui_field_spec_t){.id = "card", .role = "status", .label = TR("卡和网络"), .value = P.card};
    if (say) P.fields[P.model.field_count++] = (ink_ui_field_spec_t){.id = "notice", .role = "status", .label = TR("提示"), .value = P.say};
    for (int k = 0; k < P.rows; k++) {
        lv_obj_t *s = P.slots[k], *main = lv_obj_get_child(s, 0), *side = lv_obj_get_child(s, 1);
        int item = k - say, at = P.cpage * per + item;
        bool row = item >= 0 && (none ? item == 0 : at < X.ncand), narrow = item == 0 && (add || P.pages > 1);
        show_if(s, row);
        if (!row) continue;
        int room = narrow ? w - (add ? P.side_w : P.pager_w) - INK_GAP_M : w;
        const lv_font_t *font = ink_ui_font_px(none ? INK_FONT_NOTE : INK_FONT_LIST, none ? INK_WEIGHT_REGULAR : INK_WEIGHT_MEDIUM);
        if (lv_obj_get_style_text_font(main, 0) != font) lv_obj_set_style_text_font(main, font, 0); /* why there are none: a note */
        if (none) {
            const char *why = P.contacts < 0 ? TR("联系人暂时读不到") : !P.contacts ? TR("正在读取联系人") : P.number[0] ? TR("没有匹配的联系人") : TR("还没有联系人");
            set_text(main, why); set_text(side, ""); set_w(main, room);
            P.fields[P.model.field_count++] = (ink_ui_field_spec_t){.id = "candidates", .role = "status", .label = TR("候选"), .value = why};
            continue;
        }
        int16_t c = X.cand[at];
        const char *number = c >= 0 ? X.book[c].number : X.calls[X.groups[-c - 1].first].number;
        snprintf(P.cand_values[item], sizeof P.cand_values[item], "%s · %s", number, c >= 0 ? TR("联系人") : TR("最近通话"));
        number_args(P.cand_args[item], sizeof P.cand_args[item], number);
        set_text(main, c >= 0 ? X.book[c].name : number); set_text(side, c >= 0 ? number : TR("最近通话"));
        int name_w = LV_MIN(ink_ui_text_width(lv_label_get_text(main), INK_FONT_LIST, INK_WEIGHT_MEDIUM), room / 3);
        int nw = LV_MIN(ink_ui_text_width(lv_label_get_text(side), INK_FONT_NOTE, INK_WEIGHT_REGULAR) + INK_GAP_XS, room - name_w - INK_GAP_S);
        set_w(side, nw); set_w(main, room - nw - INK_GAP_S); /* the whole number, and a name that keeps a third at least */
        set_x(side, room - w); /* clear of the page button */
        P.fields[P.model.field_count++] = (ink_ui_field_spec_t){.id = P.cand_args[item], .role = "listitem", .label = c >= 0 ? X.book[c].name : number, .value = P.cand_values[item]};

        P.actions[P.model.action_count++] = (ink_ui_action_spec_t){.id = P.cand_args[item], .label = TR("填入这个号码"), .capability = "phone.open", .args_json = P.cand_args[item], .enabled = true};
    }
    show_if(P.add_btn, add); show_if(P.pager_btn, P.pages > 1);
    if (add) {
        set_y(P.add_btn, P.cand_y + say * INK_CONTROL);
        P.actions[P.model.action_count++] = (ink_ui_action_spec_t){.id = "add", .label = TR("加为联系人"), .capability = "contacts.edit", .enabled = true};
    }
    if (P.pages > 1) {
        snprintf(P.pager, sizeof P.pager, TR("%d/%d 翻页"), P.cpage + 1, P.pages); /* the last page turns back to the first */
        set_text(lv_obj_get_child(P.pager_btn, 0), P.pager); set_y(P.pager_btn, P.cand_y + say * INK_CONTROL);
        P.actions[P.model.action_count++] = (ink_ui_action_spec_t){.id = "next", .label = P.pager, .capability = "phone.next", .enabled = true};
    }
    ink_ui_content_changed();
}

/* ---- shared page plumbing ---- */
static void show(const ink_activity_t *page) {
    ink_ui_drop_app("phone");
    ink_ui_push(page, NULL);
}
static bool on_page(const ink_activity_t *page) { return !strcmp(ink_ui_page_name(), page->name); }
static bool mine(void) { return !strcmp(P.call_member, P.member); }
static void resume_call(void) {
    if (P.active && !strcmp(P.call_member, ink_member_current()->id) && ink_member_space_open()) show(&CALL);
}
static void own(void) { /* a page for another member starts from an empty dial pad */
    if (!strcmp(P.member, ink_member_current()->id)) return;
    P.number[0] = P.say[0] = P.undo[0] = 0; P.selecting = false; memset(&P.position, 0, sizeof P.position);
    snprintf(P.member, sizeof P.member, "%s", ink_member_current()->id);
}
static void call_show(void);
static void say_show(void) {
    if (on_page(&ink_phone_page) && P.field) cands_show();
    else if (P.notice && (P.say[0] || !on_page(&RECENTS))) set_text(P.notice, P.say);
}
static void reply(void *ctx, const ink_receipt_t *rc) {
    if ((uint32_t)(uintptr_t)ctx != ink_ui_generation() || !ink_ui_page_current()) return;
    if (rc->status != INK_RC_DONE && rc->status != INK_RC_PENDING_CONFIRMATION) snprintf(P.say, sizeof P.say, "%s", rc->say[0] ? rc->say : TR("没有完成，请重试"));
    ink_shell_airplane_card(rc);
    if (rc->undo_ms) { snprintf(P.undo, sizeof P.undo, "%s", rc->object); show(&RECENTS); return; } /* a group deleted from its details */
    say_show();
}
static void request(const char *cap, const char *json) { /* touch: the same capability the AI calls; the notice goes with the next action */
    P.say[0] = P.undo[0] = 0;
    ink_call_opts_t o = {.src = INK_SRC_TOUCH, .member = P.member, .args_json = json, .reply_exec = INK_EXEC_UI,
        .done = reply, .ctx = (void *)(uintptr_t)ink_ui_generation()};
    if (ink_cap_call(cap, NULL, &o) != 0) { snprintf(P.say, sizeof P.say, "%s", TR("没有完成，请重试")); say_show(); }
}
static void control(lv_event_t *e) {
    const ink_ui_action_spec_t *a = &P.actions[(intptr_t)lv_event_get_user_data(e)];
    request(a->capability, a->args_json);
}
static void refresh_history(void *db) { /* BG wrote or removed records: the open history shows them */
    (void)db;
    if (strcmp(P.member, ink_member_current()->id) || !ink_member_space_open()) return;
    if (on_page(&RECENTS)) ink_ui_redraw(); /* a details page goes back with its delete's reply */

}
static void loaded(void *ctx, const ink_receipt_t *rc) { /* data.list brought a collection into memory: show it in place */
    if ((uintptr_t)ctx >> 1 != (uintptr_t)ink_ui_generation() || !ink_ui_page_current()) return;
    if (on_page(&RECENTS)) { P.log_failed = rc->status != INK_RC_DONE; ink_ui_redraw(); return; }
    if (!on_page(&ink_phone_page) || !P.field) return;
    if (!P.db) P.db = ink_db_open_cached_member(&HISTORY, P.member);
    if ((uintptr_t)ctx & 1) P.contacts = load_contacts() ? 1 : rc->status == INK_RC_DONE ? 0 : -1;
    load_log(); match(); cands_show();
}
static void load(const char *collection) {
    char json[64]; snprintf(json, sizeof json, "{\"collection\":\"%s\",\"limit\":1}", collection);
    ink_call_opts_t o = {.src = INK_SRC_TOUCH, .member = P.member, .args_json = json, .reply_exec = INK_EXEC_UI, .done = loaded,
        .ctx = (void *)(((uintptr_t)ink_ui_generation() << 1) | (strcmp(collection, HISTORY.name) != 0))};
    ink_cap_call("data.list", NULL, &o);
}
static void head(lv_obj_t *screen, int tab) {
    ink_shell_statusbar(screen, true);
    ink_shell_topbar(screen, "");
    ink_phone_tabs(screen, tab);
}
void ink_phone_tabs(lv_obj_t *screen, int selected) {
    const char *const labels[] = {TR("拨号"), TR("最近"), TR("联系人")};
    lv_obj_t *seg = ink_ui_segments(screen, labels, 3, selected, INK_MARGIN + INK_CONTROL + INK_GAP,
        INK_STATUS_BAR + INK_GAP_S, INK_CANVAS_W - 2 * INK_MARGIN - INK_CONTROL - INK_GAP, NULL, NULL);
    if (strcmp(ink_lang_code(), "zh")) {
        int x = INK_GAP_XS - INK_STROKE, used = 0;
        for (int i = 0; i < 3; i++) used += ink_ui_text_width(labels[i], INK_FONT_LIST, INK_WEIGHT_MEDIUM) + 2 * INK_GAP;
        int extra = (INK_CANVAS_W - 2 * INK_MARGIN - INK_CONTROL - INK_GAP - 2 * INK_GAP_XS - used) / 3;
        for (int i = 0; i < 3; i++) { int tab_w = ink_ui_text_width(labels[i], INK_FONT_LIST, INK_WEIGHT_MEDIUM) + 2 * INK_GAP + extra; lv_obj_t *b = lv_obj_get_child(seg, i); lv_obj_set_x(b, ink_ui_px(x)); lv_obj_set_width(b, ink_ui_px(tab_w)); x += tab_w; }
    }
    const char *caps[] = {"phone.open", "phone.recents", "phone.contacts"};
    for (int i = 0; i < 3; i++) ink_shell_bind(lv_obj_get_child(seg, i), caps[i], "{}");
}

/* ---- the dial pad page ---- */
enum { FIELD_Y = INK_STATUS_BAR + INK_PRIMARY + INK_GAP_M, KEYS_Y = INK_CANVAS_H - INK_GAP_M - INK_PRIMARY - 4 * INK_CONTROL - 4 * INK_GAP_S };
static void entered(void *ctx, const char *value);
static void edited(lv_event_t *e);
static void make_field(lv_obj_t *screen) { /* the shared number field (its clear key empties it), rebuilt to show a new number in place */
    if (P.field) lv_obj_delete(P.field);
    ink_text_cfg_t cfg = {.type = INK_TEXT_PLAIN, .enter = INK_ENTER_DONE, .initial = P.number, .hint = TR("输入号码"), .max = 20};
    P.field = ink_textfield_create(screen, &cfg, entered, NULL, false);
    lv_obj_set_pos(P.field, ink_ui_px(INK_MARGIN), ink_ui_px(FIELD_Y));
    lv_obj_set_width(P.field, ink_ui_px(INK_CANVAS_W - 2 * INK_MARGIN));
    ink_textfield_bind_short(P.field, &P.model, &P.fields[0], &P.actions[0]);
    lv_obj_add_event_cb(P.field, edited, LV_EVENT_VALUE_CHANGED, NULL);
    if (P.card_line) lv_obj_move_foreground(P.card_line);
}
static void number_changed(bool rebuild) { /* the field and the candidates change in place: one partial refresh */
    if (!P.field || !on_page(&ink_phone_page)) return;
    if (rebuild) make_field(lv_obj_get_parent(P.field));
    P.cpage = 0; card_status(); match(); cands_show();
    ink_ui_hint_obj(P.field, INK_REFRESH_UI); /* the field asks for a fast refresh: one waveform for both, one refresh */
}
static void entered(void *ctx, const char *value) {
    (void)ctx; snprintf(P.number, sizeof P.number, "%s", value); P.say[0] = 0; number_changed(false);
}
static void edited(lv_event_t *e) { entered(NULL, ink_textfield_text(lv_event_get_target_obj(e))); }
static void digit(lv_event_t *e) {
    char json[24]; snprintf(json, sizeof json, "{\"key\":\"%s\"}", (const char *)lv_event_get_user_data(e));
    request("phone.key", json);
}
static void key_cap(ink_call_t *c) {
    char key[8] = ""; ink_args_get_str(&c->args, "key", key, sizeof key);
    size_t n = strlen(P.number);
    if (!strcmp(key, "delete")) { if (n) P.number[n - 1] = 0; }
    else if (strlen(key) == 1 && strchr("0123456789*#+", key[0]) && n < 20) { P.number[n] = key[0]; P.number[n + 1] = 0; }
    else { c->rc.status = INK_RC_INVALID; return; }
    P.say[0] = 0; number_changed(true);
}
static void pick(lv_event_t *e) { /* a candidate fills the number; only 拨打 dials (e-ink rows may change under the finger) */
    int item = (int)(intptr_t)lv_event_get_user_data(e) - (P.say[0] != 0);
    if (item >= 0 && X.ncand) request("phone.open", P.cand_args[item]);
}
static void dial(lv_event_t *e) { (void)e; request(P.actions[1].capability, P.dial_args); }
static void page_next(lv_event_t *e) { (void)e; request("phone.next", NULL); }
static void add_contact(lv_event_t *e) { (void)e; request("contacts.edit", NULL); } /* the number cannot go along yet (contacts-3) */
static void create_dial(lv_obj_t *screen, void *arg) {
    static const char *const keys[] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "*", "0", "#"};
    static const char *const letters[] = {"", "ABC", "DEF", "GHI", "JKL", "MNO", "PQRS", "TUV", "WXYZ", "", "", ""};
    (void)arg; own(); head(screen, 0);
    int w = INK_CANVAS_W - 2 * INK_MARGIN, cell = w / 3, cand_y = FIELD_Y + ink_textfield_height(INK_FIELD_SINGLE) + INK_GAP_S;
    P.cand_y = cand_y;
    P.model = (ink_ui_page_spec_t){.id = ink_phone_page.name, .title = TR("拨号"), .object = {"phone", 1}, .fields = P.fields, .field_count = 2, .actions = P.actions, .action_count = 5};
    P.fields[0] = (ink_ui_field_spec_t){.id = "number", .role = "textbox", .label = TR("号码")};
    P.actions[0] = (ink_ui_action_spec_t){.id = "number", .label = TR("输入号码"), .capability = "phone.number", .enabled = true};
    P.actions[2] = (ink_ui_action_spec_t){.id = "recents", .label = TR("最近"), .capability = "phone.recents", .enabled = true};
    P.actions[3] = (ink_ui_action_spec_t){.id = "contacts", .label = TR("联系人"), .capability = "phone.contacts", .enabled = true};
    P.actions[4] = (ink_ui_action_spec_t){.id = "delete", .label = TR("删除一位"), .capability = "phone.key", .args_json = "{\"key\":\"delete\"}", .enabled = true};
    make_field(screen);
    P.card_line = ink_ui_line(screen, "", INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_CANVAS_W / 2, FIELD_Y, w / 2);
    lv_obj_set_style_text_align(P.card_line, LV_TEXT_ALIGN_RIGHT, 0); lv_obj_set_style_text_color(P.card_line, ink_ui_secondary(), 0);
    P.rows = LV_CLAMP(1, (KEYS_Y - INK_GAP_S - cand_y) / INK_CONTROL, SLOTS);
    for (int k = 0; k < P.rows; k++) {
        P.slots[k] = ink_ui_box(screen, INK_MARGIN, cand_y + k * INK_CONTROL, w, INK_CONTROL, 0, INK_SURFACE_WHITE, false);
        lv_obj_add_flag(P.slots[k], LV_OBJ_FLAG_CLICKABLE); lv_obj_add_event_cb(P.slots[k], pick, LV_EVENT_CLICKED, (void *)(intptr_t)k);
        lv_obj_set_style_border_side(P.slots[k], LV_BORDER_SIDE_BOTTOM, 0); lv_obj_set_style_border_width(P.slots[k], ink_ui_px(INK_DIVIDER), 0);
        lv_obj_set_style_border_color(P.slots[k], ink_ui_separator(), 0);
        ink_ui_line(P.slots[k], "", INK_FONT_LIST, INK_WEIGHT_MEDIUM, 0, (INK_CONTROL - ink_ui_line_height(INK_FONT_LIST)) / 2, w / 2);
        lv_obj_t *side = ink_ui_line(P.slots[k], "", INK_FONT_NOTE, INK_WEIGHT_REGULAR, 0, 0, w / 2);
        lv_obj_align(side, LV_ALIGN_RIGHT_MID, 0, 0);
        lv_obj_set_style_text_align(side, LV_TEXT_ALIGN_RIGHT, 0); lv_obj_set_style_text_color(side, ink_ui_secondary(), 0);
    }
    /* a reply too long for two lines is cut here; the snapshot keeps all of it */
    P.notice = ink_ui_label(screen, "", INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_MARGIN, cand_y, w);
    lv_label_set_long_mode(P.notice, LV_LABEL_LONG_DOT);
    lv_obj_set_height(P.notice, ink_ui_px(LV_MIN(INK_CONTROL, 2 * ink_ui_line_height(INK_FONT_NOTE))));
    int side = LV_MAX(PAGER_W, ink_ui_text_width(TR("加为联系人"), INK_FONT_LIST, INK_WEIGHT_MEDIUM) + 2 * INK_GAP); /* its words fit at every size */
    char sample[32]; snprintf(sample, sizeof sample, TR("%d/%d 翻页"), 8, 8); /* pages up to 9; more still fit the word */
    int pager = LV_MAX(INK_CONTROL + INK_GAP_L, ink_ui_text_width(sample, INK_FONT_LIST, INK_WEIGHT_MEDIUM) + 2 * INK_GAP);

    P.side_w = side; P.pager_w = pager;
    P.pager_btn = ink_ui_pill(screen, "", INK_CANVAS_W - INK_MARGIN - pager, cand_y, pager, INK_CONTROL, false, page_next, NULL);
    P.add_btn = ink_ui_pill(screen, TR("加为联系人"), INK_CANVAS_W - INK_MARGIN - side, cand_y, side, INK_CONTROL, false, add_contact, NULL);
    for (int i = 0; i < 12; i++) {
        lv_obj_t *b = ink_ui_pill(screen, keys[i], INK_MARGIN + (i % 3) * cell + (cell - INK_DIGIT) / 2, KEYS_Y + (i / 3) * (INK_CONTROL + INK_GAP_S),
            INK_DIGIT, INK_CONTROL, false, digit, (void *)keys[i]);
        ink_ui_pill_text(b, INK_FONT_HEADING, INK_WEIGHT_MEDIUM);
        if (!letters[i][0]) continue; /* Fossify's letters beside the digit (the keys of name matching), the pair centred */
        int dw = ink_ui_text_width(keys[i], INK_FONT_HEADING, INK_WEIGHT_MEDIUM), lw = ink_ui_text_width(letters[i], INK_FONT_NOTE, INK_WEIGHT_REGULAR) + INK_GAP_XS;
        int x = (INK_DIGIT - 2 * INK_GAP_XS - dw - INK_GAP_XS - lw) / 2; /* ui-scale-runtime: dimension dw,lw,x measured text */
        lv_obj_t *d = lv_obj_get_child(b, 0), *l = ink_ui_line(b, letters[i], INK_FONT_NOTE, INK_WEIGHT_REGULAR, 0, 0, lw);
        lv_obj_set_width(d, LV_SIZE_CONTENT); lv_obj_align(d, LV_ALIGN_LEFT_MID, ink_ui_px(x), 0);
        lv_obj_set_style_text_color(l, ink_ui_secondary(), 0); lv_obj_align(l, LV_ALIGN_LEFT_MID, ink_ui_px(x + dw + INK_GAP_XS), 0);

    }
    ink_ui_pill(screen, TR("删除"), INK_MARGIN, INK_CANVAS_H - INK_GAP_M - INK_PRIMARY, cell - INK_GAP_M, INK_PRIMARY, false, digit, "delete");
    ink_ui_pill(screen, TR("拨打"), INK_MARGIN + cell, INK_CANVAS_H - INK_GAP_M - INK_PRIMARY, w - cell, INK_PRIMARY, true, dial, NULL);
    P.db = ink_db_open_cached_member(&HISTORY, P.member);
    P.contacts = load_contacts();
    if (!P.db) load(HISTORY.name);
    if (!P.contacts) load("phone.contacts");
    card_status(); load_log(); P.cpage = 0; match(); cands_show();
}
static bool dial_key(ink_key_t k, bool lng) { /* the cover's page keys turn the candidates */
    if (lng || (k != INK_KEY_NEXT && k != INK_KEY_PREV) || P.pages < 2) return false;
    ink_call_opts_t o = {.src = INK_SRC_KEY, .member = P.member};
    ink_cap_call(k == INK_KEY_NEXT ? "phone.next" : "phone.prev", NULL, &o);
    return true;
}

/* ---- the call screen: LilyPhone's card on top (vendor/lilyphone/call_card.c), our controls below ---- */
modem_call_state_t ui_phone_get_call_state(void) {
    if (!mine() || !P.active) return MODEM_CALL_IDLE;
    return P.started ? MODEM_CALL_ACTIVE : P.incoming ? MODEM_CALL_INCOMING : MODEM_CALL_DIALING;
}
void ui_phone_get_call_number(char *buf, size_t cap) { snprintf(buf, cap, "%s", mine() ? P.calling : ""); }
const char *ui_phone_end_text(void) { return mine() && P.end_text[0] ? P.end_text : TR("没有通话"); }
void ui_phone_answer(void) { request("call.answer", NULL); }
void ui_phone_hang_up(void) { request("call.hangup", NULL); }
lv_obj_t *scr_action_bar_create(lv_obj_t *parent, int height) { /* ui-scale-runtime: dimension height LilyPhone passes INK_PRIMARY */
    P.bar = ink_ui_box(parent, INK_MARGIN, INK_CANVAS_H - INK_MARGIN - height, INK_CANVAS_W - 2 * INK_MARGIN, height, 0, INK_SURFACE_WHITE, false);
    lv_obj_set_flex_flow(P.bar, LV_FLEX_FLOW_ROW); /* the gap is the first button's margin: LVGL keeps a gap after a hidden first child */
    return P.bar;
}
lv_obj_t *scr_bar_btn_create(lv_obj_t *bar, const char *text, int width, lv_event_cb_t cb, void *ctx) { /* ui-scale-runtime: dimension width LilyPhone passes INK_CONTROL */
    lv_obj_t *b = ink_ui_pill(bar, text, 0, 0, width, INK_PRIMARY, !strcmp(text, TR("挂断")), cb, ctx); /* 挂断: the page's one primary button */
    lv_obj_set_flex_grow(b, 1);
    if (!lv_obj_get_index(b)) lv_obj_set_style_margin_right(b, ink_ui_px(INK_GAP_M), 0);
    return b;

}
/* Lineage CallCardPresenter :394-446 (setDisconnectCause): say how it ended; the modem tells BUSY and NO ANSWER apart. */
static void ended(const char *reason) {
    const char *key = P.incoming && !P.started ? (!strcmp(reason, "cancelled") ? TR_NOOP("已拒接") : TR_NOOP("未接来电"))
        : P.started ? TR_NOOP("通话结束") : !strcmp(reason, "busy") ? TR_NOOP("对方忙或拒接") : !strcmp(reason, "no_answer") ? TR_NOOP("没人接")
        : !strcmp(reason, "cancelled") ? TR_NOOP("已取消") : TR_NOOP("没有接通");
    char length[32] = ""; span(P.seconds, length, sizeof length);
    snprintf(P.status, sizeof P.status, "%s", key); snprintf(P.reason, sizeof P.reason, "%s", reason);
    if (P.started) snprintf(P.end_text, sizeof P.end_text, "%s · %s", TR(key), length);
    else snprintf(P.end_text, sizeof P.end_text, "%s", TR(key));

}
static void pill_state(lv_obj_t *pill, const char *text, bool on) { /* state by words and solid / outline, not colour */
    lv_obj_t *l = lv_obj_get_child(pill, 0);
    if (!strcmp(lv_label_get_text(l), text) && (ink_ui_surface_kind(pill) == INK_SURFACE_BLACK) == on) return;
    ink_ui_surface_style(pill, on ? INK_SURFACE_BLACK : INK_SURFACE_WHITE, !on);
    lv_obj_set_style_text_color(l, on ? ink_ui_paper() : ink_ui_ink(), 0); lv_label_set_text(l, text);
}
static void call_model(bool keypad) {
    bool owned = mine(), live = owned && P.active, ending = owned && !P.active && P.end_text[0];
    const char *state = !owned ? TR("没有通话") : P.active ? TR(P.status) : P.end_text;
    P.model = (ink_ui_page_spec_t){.id = ink_ui_page_name(), .title = TR("通话"), .object = {"phone", 1}, .fields = P.fields, .field_count = 9, .actions = P.actions};
    P.fields[0] = (ink_ui_field_spec_t){.id = "number", .role = "text", .label = TR("号码"), .value = owned ? P.calling : ""};
    P.fields[1] = (ink_ui_field_spec_t){.id = "status", .role = "status", .label = TR("通话状态"), .value = state};
    P.fields[2] = (ink_ui_field_spec_t){.id = "mute", .role = "status", .label = TR("麦克风"), .value = !owned ? TR("没有通话") : P.muted ? TR("已静音") : TR("未静音")};
    P.fields[3] = (ink_ui_field_spec_t){.id = "route", .role = "status", .label = TR("声音去向"), .value = P.route};
    P.fields[4] = keypad ? (ink_ui_field_spec_t){.id = "tones", .role = "text", .label = TR("已发送的按键"), .value = owned ? P.tones : "", .secret = true}
        : (ink_ui_field_spec_t){.id = "recording", .role = "status", .label = TR("录音"), .value = TR("通话音频接线未核实，暂不能录音")};
    snprintf(P.tone_count, sizeof P.tone_count, "%u", owned ? (unsigned)strlen(P.tones) : 0);
    P.fields[5] = (ink_ui_field_spec_t){.id = "tone_count", .role = "text", .label = TR("已发送的按键位数"), .value = P.tone_count};
    P.fields[6] = (ink_ui_field_spec_t){.id = "duration", .role = "status", .label = TR("通话时长"), .value = live && P.started ? P.duration_text : ""};
    P.fields[7] = (ink_ui_field_spec_t){.id = "name", .role = "text", .label = TR("对方"), .value = owned && P.calling[0] ? contacts_display_name(P.calling) : ""};
    P.fields[8] = (ink_ui_field_spec_t){.id = "card", .role = "status", .label = TR("用的卡"), .value = owned ? card_name(P.call_sim) : ""};
    if (ending) P.fields[P.model.field_count++] = (ink_ui_field_spec_t){.id = "reason", .role = "status", .label = TR("结束原因"), .value = P.reason};
    P.actions[0] = (ink_ui_action_spec_t){.id = "hangup", .label = TR("挂断"), .capability = "call.hangup", .enabled = live};
    if (keypad) {
        P.actions[1] = (ink_ui_action_spec_t){.id = "keypad", .label = TR("收起键盘"), .capability = "phone.keypad", .enabled = live};
        for (int i = 0; i < 12; i++) P.actions[i + 2].enabled = live;
        P.model.action_count = 14;
        return;
    }
    int64_t connected = 0; ink_state_get_int(ink_state_find("bt.connected"), &connected);
    bool headset = P.headset;
    const char *labels[] = {P.muted ? TR("已静音") : TR("静音"), TR("键盘"), connected ? headset ? TR("耳机") : TR("喇叭") : TR("耳机未连"), TR("录音")};
    const char *ids[] = {"mute", "keypad", "headphones", "mic"}, *caps[] = {"call.mute", "phone.keypad", connected ? "call.route" : "shell.bluetooth", "call.record"};
    const char *args[] = {P.muted ? "{\"on\":false}" : "{\"on\":true}", "{}", headset ? "{\"headset\":false}" : "{\"headset\":true}", "{}"};
    for (int i = 0; i < 4; i++) {
        P.actions[i + 1] = (ink_ui_action_spec_t){.id = ids[i], .label = labels[i], .capability = caps[i], .args_json = args[i], .enabled = live};
        if (P.controls[i]) pill_state(P.controls[i], labels[i], (i == 0 && P.muted) || (i == 2 && connected && headset));
    }
    P.model.action_count = 5;
    if (live && P.incoming && !P.started) P.actions[P.model.action_count++] = (ink_ui_action_spec_t){.id = "answer", .label = TR("接听"), .capability = "call.answer", .enabled = true};
    if (!ending) return;
    number_args(P.dial_args, sizeof P.dial_args, P.calling); /* Fossify closes after 3 s; the result stays here with these two */
    P.actions[5] = (ink_ui_action_spec_t){.id = "again", .label = TR("再打一次"), .capability = "call.dial", .args_json = P.dial_args, .enabled = true};
    P.actions[6] = (ink_ui_action_spec_t){.id = "done", .label = TR("完成"), .capability = "phone.recents", .enabled = true};
    P.model.action_count = 7;
}
static void call_show(void) { /* the call screen follows the call in place: only the changed labels and buttons redraw */
    bool keypad = on_page(&KEYPAD), owned = mine(), live = owned && P.active, ending = owned && !P.active && P.end_text[0];
    if (!keypad && !on_page(&CALL)) return;
    char route[32] = ""; ink_state_get_str(ink_state_find("audio.route"), route, sizeof route);
    P.headset = !strcmp(route, "bt_headset");
    snprintf(P.route, sizeof P.route, "%s", P.headset ? TR("声音走耳机") : TR("声音走喇叭"));
    if (live && P.started) P.minutes = (int64_t)((ink_now_us() - P.started) / 60000000);
    snprintf(P.duration_text, sizeof P.duration_text, TR("%lld 分钟"), (long long)P.minutes);
    set_text(P.duration, live && P.started ? P.duration_text : "");
    if (keypad) { set_text(P.status_line, !owned ? "" : P.active ? TR(P.status) : P.end_text); set_text(P.typed, owned ? P.tones : ""); }
    else {
        lily_call_card_render();
        show_if(P.bar, !ending); show_if(P.ends, ending);
        for (int i = 0; i < 4; i++) show_if(P.controls[i], live);
    }
    call_model(keypad);
    ink_ui_content_changed();
}
static void create_call(lv_obj_t *screen, void *arg) {
    (void)arg; own(); ink_shell_statusbar(screen, true);
    bool keypad = on_page(&KEYPAD), owned = mine();
    int w = INK_CANVAS_W - 2 * INK_MARGIN;
    load_contacts(); /* this member's contacts as they are now: the table may hold another member's, or older ones */
    if (keypad) {
        static const char *const keys[] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "*", "0", "#"};
        int y = INK_STATUS_BAR + INK_GAP, line = ink_ui_line_height(INK_FONT_NOTE) + INK_GAP_XS, cell = w / 3;
        lv_obj_t *name = ink_ui_line(screen, owned ? contacts_display_name(P.calling) : TR("没有通话"), INK_FONT_TITLE, INK_WEIGHT_BOLD, INK_MARGIN, y, w);
        lv_obj_t **lines[] = {&P.status_line, &P.duration, &P.typed};
        lv_obj_set_style_text_align(name, LV_TEXT_ALIGN_CENTER, 0);
        y += ink_ui_line_height(INK_FONT_TITLE) + INK_GAP_XS;
        for (int i = 0; i < 3; i++) { *lines[i] = ink_ui_notice(screen, "", y + i * line); lv_obj_set_style_text_align(*lines[i], LV_TEXT_ALIGN_CENTER, 0); }
        y += 3 * line + INK_GAP;
        for (int i = 0; i < 12; i++) {
            snprintf(P.tone_args[i], sizeof P.tone_args[i], "{\"key\":\"%s\"}", keys[i]);
            lv_obj_t *b = ink_ui_pill(screen, keys[i], INK_MARGIN + (i % 3) * cell + (cell - INK_DIGIT) / 2, y + (i / 3) * (INK_CONTROL + INK_GAP_S), INK_DIGIT, INK_CONTROL, false, control, (void *)(intptr_t)(i + 2));
            ink_ui_pill_text(b, INK_FONT_HEADING, INK_WEIGHT_MEDIUM);
            P.actions[i + 2] = (ink_ui_action_spec_t){.id = keys[i], .label = keys[i], .capability = "call.dtmf", .args_json = P.tone_args[i]};
        }
        P.notice = ink_ui_notice(screen, "", y + 4 * INK_CONTROL + 3 * INK_GAP_S + INK_GAP_M);
        int half = (w - INK_GAP_M) / 2; /* Fossify keeps 挂断 under the call keypad too */
        ink_shell_bind(ink_ui_pill(screen, TR("收起键盘"), INK_MARGIN, INK_CANVAS_H - INK_MARGIN - INK_PRIMARY, half, INK_PRIMARY, false, NULL, NULL), "phone.keypad", "{}");
        ink_ui_pill(screen, TR("挂断"), INK_MARGIN + half + INK_GAP_M, INK_CANVAS_H - INK_MARGIN - INK_PRIMARY, half, INK_PRIMARY, true, control, (void *)(intptr_t)0);


    } else {
        P.status_line = lily_call_card(screen);
        P.duration = ink_ui_notice(screen, "", 0); P.card_line = ink_ui_notice(screen, owned ? card_name(P.call_sim) : "", 0);
        lv_obj_align_to(P.duration, P.status_line, LV_ALIGN_OUT_BOTTOM_MID, 0, ink_ui_px(INK_GAP_XS));
        lv_obj_align_to(P.card_line, P.duration, LV_ALIGN_OUT_BOTTOM_MID, 0, ink_ui_px(INK_GAP_XS));
        lv_obj_set_style_text_align(P.duration, LV_TEXT_ALIGN_CENTER, 0); lv_obj_set_style_text_align(P.card_line, LV_TEXT_ALIGN_CENTER, 0);
        int half = (w - INK_GAP_M) / 2, top = INK_CANVAS_H - INK_MARGIN - INK_PRIMARY - INK_GAP_L - 2 * INK_CONTROL - INK_GAP_M;
        for (int i = 0; i < 4; i++) /* Fossify's mute, keypad and audio route as worded buttons (and our 录音, B18) */
            P.controls[i] = ink_ui_pill(screen, "", INK_MARGIN + (i % 2) * (half + INK_GAP_M), top + (i / 2) * (INK_CONTROL + INK_GAP_M), half, INK_CONTROL, false, control, (void *)(intptr_t)(i + 1));
        P.notice = ink_ui_notice(screen, "", top - INK_GAP - 2 * ink_ui_line_height(INK_FONT_NOTE));
        P.ends = ink_ui_box(screen, INK_MARGIN, INK_CANVAS_H - INK_MARGIN - INK_PRIMARY, w, INK_PRIMARY, 0, INK_SURFACE_WHITE, false);
        ink_ui_pill(P.ends, TR("再打一次"), 0, 0, half, INK_PRIMARY, false, control, (void *)(intptr_t)5);
        ink_ui_pill(P.ends, TR("完成"), half + INK_GAP_M, 0, half, INK_PRIMARY, true, control, (void *)(intptr_t)6);
        lv_obj_add_flag(P.ends, LV_OBJ_FLAG_HIDDEN);
    }
    call_show();
}

/* ---- recent calls and a group's details: Fossify RecentsFragment, RecentCallsAdapter, ShowGroupedCallsDialog ---- */
/* Each row: the group, and its own 详情 button as wide as the word needs (English "Details"). */
static void open_detail(lv_event_t *e) { request("phone.detail", P.detail_args[(intptr_t)lv_obj_get_user_data(lv_event_get_current_target_obj(e))]); }
static void bind_group(lv_obj_t *slot, size_t index, ink_ui_field_spec_t *field, ink_ui_action_spec_t *action, void *ctx) {
    (void)ctx;
    const group_t *g = &X.groups[index]; const call_t *c = &X.calls[g->first];
    unsigned at = index % ROWS;
    int w = INK_CANVAS_W - 2 * INK_MARGIN, row = ink_ui_row_double(), side = LV_MAX(INK_CONTROL + INK_GAP_L, ink_ui_text_width(TR("详情"), INK_FONT_LIST, INK_WEIGHT_MEDIUM) + 2 * INK_GAP), text = w - side - INK_GAP;

    bool missed = !strcmp(c->kind, "未接");
    int n = snprintf(P.titles[at], sizeof P.titles[at], "%s", contacts_display_name(c->number));
    if (g->count > 1 && n > 0 && (size_t)n < sizeof P.titles[at]) snprintf(P.titles[at] + n, sizeof P.titles[at] - (size_t)n, " (%u)", (unsigned)g->count);
    call_line(c, P.details[at], sizeof P.details[at]);
    snprintf(P.ids[at], sizeof P.ids[at], "call:%u", (unsigned)c->id);
    number_args(P.args[at], sizeof P.args[at], c->number);
    snprintf(P.detail_args[at], sizeof P.detail_args[at], "{\"id\":%u}", (unsigned)c->id);
    lv_obj_set_style_radius(slot, 0, 0); lv_obj_set_style_border_side(slot, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_border_color(slot, ink_ui_separator(), 0); lv_obj_set_style_border_width(slot, ink_ui_px(index % P.rows ? INK_DIVIDER : 0), 0);
    if (!lv_obj_get_child_count(slot)) {
        /* RecentCallsAdapter.kt:460-540: name and count, then the type's icon before the type, time and length */
        ink_ui_line(slot, "", INK_FONT_LIST, INK_WEIGHT_MEDIUM, 0, INK_GAP_XS, text);
        ink_ui_line(slot, "", INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_ICON_SMALL + INK_GAP_XS, INK_GAP_XS + ink_ui_line_height(INK_FONT_LIST), text - INK_ICON_SMALL - INK_GAP_XS);
        ink_ui_pill(slot, TR("详情"), w - side, (row - INK_CONTROL) / 2, side, INK_CONTROL, false, open_detail, NULL);
    }
    if (lv_obj_get_child_count(slot) > 3) lv_obj_delete(lv_obj_get_child(slot, 3));
    ink_ui_icon(slot, missed ? "callEnd" : !strcmp(c->kind, "打来") ? "down" : "arrow", INK_ICON_SMALL, INK_ICON_LINE, 0,
        INK_GAP_XS + ink_ui_line_height(INK_FONT_LIST) + (ink_ui_line_height(INK_FONT_NOTE) - INK_ICON_SMALL) / 2, false);

    lv_obj_t *title = lv_obj_get_child(slot, 0);
    lv_obj_set_style_text_font(title, ink_ui_font_px(INK_FONT_LIST, missed ? INK_WEIGHT_BOLD : INK_WEIGHT_MEDIUM), 0); /* missed: bold, not red */
    lv_obj_set_user_data(lv_obj_get_child(slot, 2), (void *)(intptr_t)at);
    set_text(title, P.titles[at]); set_text(lv_obj_get_child(slot, 1), P.details[at]);
    *field = (ink_ui_field_spec_t){.id = P.ids[at], .role = "listitem", .label = P.titles[at], .value = P.details[at], .object = {P.ids[at], 1}};
    *action = (ink_ui_action_spec_t){.id = P.ids[at], .label = TR("用这个号码拨号"), .capability = "phone.open", .args_json = P.args[at], .object = field->object, .enabled = true};
}
static void undo(lv_event_t *e) { (void)e; request("system.undo", P.undo_args); }
static void create_recents(lv_obj_t *screen, void *arg) {
    const ink_ui_action_spec_t PAGES[] = {
        {.id = "prev", .label = TR("上一页"), .capability = "phone.prev"},
        {.id = "next", .label = TR("下一页"), .capability = "phone.next"},
        {.id = "dial", .label = TR("拨号"), .capability = "phone.open", .enabled = true}
    };
    const ink_ui_page_spec_t LIST_MODEL = {.id = "PhoneRecents", .title = TR("最近通话"), .object = {"phone.calls", 1}, .actions = PAGES, .action_count = 3};
    (void)arg; own(); head(screen, 1);
    int w = INK_CANVAS_W - 2 * INK_MARGIN, top = INK_STATUS_BAR + INK_PRIMARY + INK_GAP_L, foot = INK_CANVAS_H - INK_MARGIN - INK_CONTROL;
    P.model = (ink_ui_page_spec_t){.id = RECENTS.name, .title = TR("最近通话"), .object = {"phone.calls", 1}, .fields = P.fields, .actions = P.actions};
    P.notice = ink_ui_notice(screen, "", foot + (INK_CONTROL - ink_ui_line_height(INK_FONT_NOTE)) / 2);
    lv_obj_set_width(P.notice, ink_ui_px(w - PAGER_W - INK_GAP));
    P.db = ink_db_open_cached_member(&HISTORY, P.member);
    if (!P.db) {
        set_text(P.notice, P.log_failed ? TR("通话记录暂时读不到") : TR("正在读取通话记录"));
        if (P.log_failed) ink_shell_bind(ink_ui_pill(screen, TR("重试"), INK_CANVAS_W - INK_MARGIN - PAGER_W, foot, PAGER_W, INK_CONTROL, false, NULL, NULL), "phone.recents", "{}");
        else load(HISTORY.name);
        P.fields[0] = (ink_ui_field_spec_t){.id = "status", .role = "status", .label = TR("通话记录"), .value = lv_label_get_text(P.notice)};
        P.model.field_count = 1;
        return;
    }
    P.log_failed = false;
    load_contacts(); load_log();
    lv_obj_t *body = ink_ui_box(screen, 0, top, INK_CANVAS_W, foot - INK_GAP - top, 0, INK_SURFACE_WHITE, false);
    P.rows = LV_CLAMP(1, (foot - 2 * INK_GAP - top - INK_CONTROL) / (ink_ui_row_double() + INK_GAP_M), ROWS);
    P.list = ink_ui_list_create(body, &LIST_MODEL, &P.position, (unsigned)P.rows, 1, ink_ui_row_double(), TR("还没有通话记录"), bind_group, NULL);
    if (P.list) ink_ui_list_refresh(P.list, (size_t)X.ngroups);
    if (P.undo[0]) { /* D-105: an undoable delete asks nothing; the way back stays until the next action */
        set_text(P.notice, TR("已删除这一组通话记录"));
        ink_jw_t jw; ink_jw_init(&jw, P.undo_args, sizeof P.undo_args); ink_jw_raw(&jw, "{\"object\":"); ink_jw_str(&jw, P.undo); ink_jw_raw(&jw, "}");
        ink_ui_pill(screen, TR("撤销"), INK_CANVAS_W - INK_MARGIN - PAGER_W, foot, PAGER_W, INK_CONTROL, false, undo, NULL);
        P.actions[0] = (ink_ui_action_spec_t){.id = "undo", .label = TR("撤销"), .capability = "system.undo", .args_json = P.undo_args, .enabled = true};
    } else if (X.ngroups) { /* MainActivity.kt:276-285: clear all, asked first (it cannot be undone) */
        ink_ui_pill(screen, TR("清空"), INK_CANVAS_W - INK_MARGIN - PAGER_W, foot, PAGER_W, INK_CONTROL, false, control, (void *)(intptr_t)0);
        P.actions[0] = (ink_ui_action_spec_t){.id = "clear", .label = TR("清空通话记录"), .capability = "phone.clear", .enabled = true};
    }
    P.model.action_count = P.undo[0] || X.ngroups;
    P.fields[0] = (ink_ui_field_spec_t){.id = "notice", .role = "status", .label = TR("提示"), .value = lv_label_get_text(P.notice)};
    P.model.field_count = 1;
}
static void bind_entry(lv_obj_t *slot, size_t index, ink_ui_field_spec_t *field, ink_ui_action_spec_t *action, void *ctx) {
    (void)ctx; (void)action;
    const call_t *c = &X.calls[(size_t)P.first + index]; unsigned at = index % ROWS;
    call_line(c, P.details[at], sizeof P.details[at]);
    if (c->sim[0]) snprintf(P.details[at] + strlen(P.details[at]), sizeof P.details[at] - strlen(P.details[at]), " · %s", card_name(c->sim));
    snprintf(P.ids[at], sizeof P.ids[at], "call:%u", (unsigned)c->id);
    lv_obj_set_style_radius(slot, 0, 0); lv_obj_set_style_border_side(slot, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_border_color(slot, ink_ui_separator(), 0); lv_obj_set_style_border_width(slot, ink_ui_px(index % P.rows ? INK_DIVIDER : 0), 0);
    if (!lv_obj_get_child_count(slot)) ink_ui_line(slot, "", INK_FONT_LIST, INK_WEIGHT_REGULAR, 0, (INK_CONTROL - ink_ui_line_height(INK_FONT_LIST)) / 2, INK_CANVAS_W - 2 * INK_MARGIN);
    set_text(lv_obj_get_child(slot, 0), P.details[at]);
    *field = (ink_ui_field_spec_t){.id = P.ids[at], .role = "listitem", .label = TR("通话"), .value = P.details[at], .object = {P.ids[at], 1}};
}
static void create_detail(lv_obj_t *screen, void *arg) {
    const ink_ui_action_spec_t PAGES[] = {{.id = "prev", .label = TR("上一页"), .capability = "phone.prev"}, {.id = "next", .label = TR("下一页"), .capability = "phone.next"}};
    const ink_ui_page_spec_t LIST_MODEL = {.id = "PhoneDetail", .title = TR("通话详情"), .object = {"phone.calls", 1}, .actions = PAGES, .action_count = 2};
    (void)arg; own(); ink_shell_statusbar(screen, true);
    P.db = ink_db_open_cached_member(&HISTORY, P.member);
    load_contacts(); load_log();
    int g = 0, w = INK_CANVAS_W - 2 * INK_MARGIN, half = (w - INK_GAP_M) / 2, y = INK_STATUS_BAR + INK_PRIMARY + INK_GAP;
    while (g < X.ngroups && X.calls[X.groups[g].first].id != P.detail) g++;
    if (g == X.ngroups) { ink_shell_topbar(screen, TR("通话详情")); ink_ui_empty(screen, TR("这一组通话记录已经不在了"), NULL, NULL, NULL); return; }
    const call_t *c = &X.calls[X.groups[g].first];
    int contact = contacts_find_by_number(c->number);
    P.first = X.groups[g].first; /* bind_entry counts from the group's first record */
    ink_shell_topbar(screen, contacts_display_name(c->number));
    if (contact >= 0) { ink_ui_notice(screen, c->number, y); y += ink_ui_line_height(INK_FONT_NOTE) + INK_GAP_S; }
    number_args(P.dial_args, sizeof P.dial_args, c->number);
    snprintf(P.sms_args, sizeof P.sms_args, "%s", P.dial_args);
    snprintf(P.view_args, sizeof P.view_args, "{\"id\":%u}", contact >= 0 ? (unsigned)X.book_id[contact] : 0u);
    snprintf(P.delete_args, sizeof P.delete_args, "{\"id\":%u,\"count\":%u}", (unsigned)c->id, (unsigned)X.groups[g].count);
    /* the row menu of RecentCallsAdapter.kt:351-445 as worded buttons; 打电话 is the page's one primary button */
    P.actions[0] = (ink_ui_action_spec_t){.id = "call", .label = TR("打电话"), .capability = "call.dial", .args_json = P.dial_args, .enabled = true};
    P.actions[1] = (ink_ui_action_spec_t){.id = "sms", .label = TR("发短信"), .capability = "sms.compose", .args_json = P.sms_args, .enabled = true};
    P.actions[2] = contact >= 0 ? (ink_ui_action_spec_t){.id = "contact", .label = TR("查看联系人"), .capability = "contacts.view", .args_json = P.view_args, .enabled = true}
        : (ink_ui_action_spec_t){.id = "contact", .label = TR("加为联系人"), .capability = "contacts.edit", .enabled = true};
    P.actions[3] = (ink_ui_action_spec_t){.id = "delete", .label = TR("删除这一组"), .capability = "phone.delete", .args_json = P.delete_args, .enabled = true};
    for (int i = 0; i < 4; i++) ink_ui_pill(screen, P.actions[i].label, INK_MARGIN + (i % 2) * (half + INK_GAP_M), y + (i / 2) * (INK_CONTROL + INK_GAP_M), half, INK_CONTROL, i == 0, control, (void *)(intptr_t)i);
    y += 2 * INK_CONTROL + INK_GAP_M + INK_GAP;
    P.notice = ink_ui_notice(screen, "", INK_CANVAS_H - INK_MARGIN - ink_ui_line_height(INK_FONT_NOTE));
    P.model = (ink_ui_page_spec_t){.id = DETAIL.name, .title = TR("通话详情"), .object = {"phone.calls", 1}, .fields = P.fields, .field_count = 2, .actions = P.actions, .action_count = 4};
    P.fields[0] = (ink_ui_field_spec_t){.id = "name", .role = "text", .label = TR("对方"), .value = contacts_display_name(c->number)};
    P.fields[1] = (ink_ui_field_spec_t){.id = "number", .role = "text", .label = TR("号码"), .value = c->number};
    lv_obj_t *body = ink_ui_box(screen, 0, y, INK_CANVAS_W, INK_CANVAS_H - INK_MARGIN - ink_ui_line_height(INK_FONT_NOTE) - y, 0, INK_SURFACE_WHITE, false);
    P.rows = LV_CLAMP(1, (INK_CANVAS_H - INK_MARGIN - ink_ui_line_height(INK_FONT_NOTE) - y - 2 * INK_GAP - INK_CONTROL) / (INK_CONTROL + INK_GAP_M), ROWS);
    P.list = ink_ui_list_create(body, &LIST_MODEL, &P.detail_position, (unsigned)P.rows, 1, INK_CONTROL, "", bind_entry, NULL);
    if (P.list) ink_ui_list_refresh(P.list, X.groups[g].count);
}
static int with_page(int n, char *out, size_t cap) { /* a list's snapshot, then the page's own fields and actions under "more" */
    if (n < 1) return -1;
    int m = ink_ui_semantics_snapshot(&P.model, P.extra, sizeof P.extra);
    ink_jw_t w = {.buf = out, .cap = cap, .len = (size_t)n - 1};
    if (m < 1) return n;
    ink_jw_raw(&w, ",\"more\":"); ink_jw_raw(&w, P.extra); ink_jw_raw(&w, "}");
    return w.overflow ? -1 : (int)w.len;
}
static int recents_snapshot(char *out, size_t cap) {
    if (!P.list) return ink_ui_semantics_snapshot(&P.model, out, cap);
    size_t first = P.position.page * (size_t)P.rows, keep = P.model.action_count;
    for (size_t i = first; i < first + (size_t)P.rows && i < (size_t)X.ngroups && P.model.action_count < 16; i++) /* each row's 详情, beside its row */
        P.actions[P.model.action_count++] = (ink_ui_action_spec_t){.id = P.detail_args[i % ROWS], .label = TR("详情"), .capability = "phone.detail", .args_json = P.detail_args[i % ROWS], .enabled = true};
    int n = with_page(ink_ui_list_snapshot(P.list, out, cap), out, cap);
    P.model.action_count = keep;
    return n;
}
static int detail_snapshot(char *out, size_t cap) { return P.list ? with_page(ink_ui_list_snapshot(P.list, out, cap), out, cap) : ink_ui_semantics_snapshot(&P.model, out, cap); }
static int dial_snapshot(char *out, size_t cap) { return P.field ? ink_textfield_short_snapshot(P.field, out, cap) : -1; }
static int call_snapshot(char *out, size_t cap) { return ink_ui_semantics_snapshot(&P.model, out, cap); }
static void destroy(void) {
    ink_db_close(P.db); P.db = NULL; P.list = NULL; lily_call_card_gone();
    P.field = P.notice = P.duration = P.status_line = P.typed = P.card_line = P.bar = P.ends = P.pager_btn = P.add_btn = NULL;
    memset(P.slots, 0, sizeof P.slots); memset(P.controls, 0, sizeof P.controls);
}
static bool page_key(ink_key_t k, bool lng) { return P.list && ink_ui_list_key(P.list, k, lng); }
const ink_activity_t ink_phone_page = {.name = "Phone", .app = "phone", .create = create_dial, .destroy = destroy, .snapshot = dial_snapshot, .settle = resume_call, .on_key = dial_key};
static const ink_activity_t RECENTS = {.name = "PhoneRecents", .app = "phone", .create = create_recents, .destroy = destroy, .snapshot = recents_snapshot, .on_key = page_key};
static const ink_activity_t DETAIL = {.name = "PhoneDetail", .app = "phone", .create = create_detail, .destroy = destroy, .snapshot = detail_snapshot, .on_key = page_key};
static const ink_activity_t CALL = {.name = "PhoneCall", .app = "phone", .create = create_call, .destroy = destroy, .snapshot = call_snapshot};
static const ink_activity_t KEYPAD = {.name = "PhoneKeypad", .app = "phone", .create = create_call, .destroy = destroy, .snapshot = call_snapshot};

/* ---- events and capabilities ---- */
/* BG: a burst of writes can fill what waits to be written (INK_DB_ERR_FULL): wait for it once, then try again. */
static int put_waiting(ink_db_t *db, const ink_db_write_t *w) {
    int r = ink_db_put(db, w, NULL);
    return r == INK_DB_ERR_FULL && !ink_db_wait(db, ink_db_last_ver(db), 5000000) ? ink_db_put(db, w, NULL) : r;
}
static int del_waiting(ink_db_t *db, uint32_t id, uint8_t src) {
    int r = ink_db_del(db, id, src);
    return r == INK_DB_ERR_FULL && !ink_db_wait(db, ink_db_last_ver(db), 5000000) ? ink_db_del(db, id, src) : r;
}
static void on_call(void *ctx, const char *id, const ink_args_t *a) { /* BG: every call ends as one history record */
    (void)ctx;
    if (!strcmp(id, "call.incoming") || !strcmp(id, "call.dialed")) {
        H.incoming = !strcmp(id, "call.incoming"); H.answered = false; H.active = true;
        ink_args_get_str(a, "member", H.member, sizeof H.member);
        ink_args_get_str(a, H.incoming ? "from" : "number", H.number, sizeof H.number);
        sim_in_use(H.sim, sizeof H.sim);
        H.at = ink_time_now_s(); H.started = ink_now_us();
    } else if (!strcmp(id, "call.answered") || !strcmp(id, "call.connected")) {
        H.answered = true; H.started = ink_now_us();
    } else if (!strcmp(id, "call.ended") && H.active) {
        H.active = false;
        ink_db_t *db = ink_db_open_member(&HISTORY, H.member);
        if (!db) return;
        int64_t seconds = H.answered ? (int64_t)((ink_now_us() - H.started) / 1000000) : 0;
        char raw[256], result[16] = ""; ink_jw_t w; ink_jw_init(&w, raw, sizeof raw);
        ink_args_get_str(a, "reason", result, sizeof result);
        ink_jw_raw(&w, "{\"number\":"); ink_jw_str(&w, H.number);
        ink_jw_raw(&w, ",\"kind\":"); ink_jw_str(&w, H.incoming ? H.answered ? TR_NOOP("打来") : TR_NOOP("未接") : TR_NOOP("打出"));
        ink_jw_raw(&w, ",\"minutes\":"); ink_jw_int(&w, seconds / 60);
        ink_jw_raw(&w, ",\"seconds\":"); ink_jw_int(&w, seconds);
        ink_jw_raw(&w, ",\"result\":"); ink_jw_str(&w, result);
        ink_jw_raw(&w, ",\"sim\":"); ink_jw_str(&w, H.sim);
        ink_jw_raw(&w, ",\"at\":"); ink_jw_int(&w, H.at); ink_jw_raw(&w, "}");
        ink_db_write_t wr = {.sort = H.at, .data = raw, .len = w.len, .src = INK_SRC_SYSTEM};
        if (ink_db_put(db, &wr, NULL) == INK_DB_OK) {
            for (int n = ink_db_count(db); n > LOG_MAX; n--) { /* the oldest go (a restored backup may have had more) */
                ink_db_cursor_t cur = {0}; ink_rec_t rec;
                if (ink_db_next(db, &cur, &rec, NULL, 0) != 1 || del_waiting(db, rec.id, INK_SRC_SYSTEM) != INK_DB_OK) break;
            }
            ink_db_flush(db); ink_exec_post(INK_EXEC_UI, &P.refresh, refresh_history, db);
        }
        ink_db_close(db);
    }
}
static void erase(ink_call_t *c) { /* BG: phone.delete takes a group out (system.undo puts it back), phone.clear everything (asked first) */
    ink_db_t *db = ink_db_open_member(&HISTORY, ink_cap_call_member(c));
    if (!db) { c->rc.status = INK_RC_FAILED; return; }
    bool all = !strcmp(c->cap->id, "phone.clear"), found = all;
    int64_t id = 0, count = 0; ink_args_get_int(&c->args, "id", &id); ink_args_get_int(&c->args, "count", &count);
    ink_db_cursor_t cur = {.desc = true}; ink_rec_t rec; char raw[256]; int taken = 0;
    ink_json_t *j = all ? NULL : ink_app_alloc(sizeof *j, true);
    call_t head = {0}, one;
    T.count = 0; T.token++; snprintf(T.member, sizeof T.member, "%s", ink_cap_call_member(c));
    if (!all && !j) { ink_db_close(db); c->rc.status = INK_RC_FAILED; return; }
    /* A group only: from its first record, the consecutive ones of the same group, never more than undo keeps. */
    while ((all || (taken < count && T.count < TRASH)) && ink_db_next(db, &cur, &rec, raw, sizeof raw - 1) == 1) {
        if (!found && rec.id != (uint32_t)id) continue;
        size_t n = rec.len < sizeof raw - 1 ? rec.len : sizeof raw - 1;
        if (!all) {
            memset(&one, 0, sizeof one);
            if (!parse_call(j, raw, n, &one) || (found && !same_group(&head, &one))) break;
            if (!found) head = one;
            memcpy(T.raw[T.count], raw, n); T.raw[T.count][n] = 0; T.sort[T.count] = rec.sort;
        }
        found = true;
        if (del_waiting(db, rec.id, (uint8_t)c->src) == INK_DB_OK) { taken++; if (!all) T.count++; }
        else if (!all) break; /* a group is taken whole from its start, or as far as it went: no gaps */
    }
    ink_app_free(j);
    if (!taken && !all) c->rc.status = INK_RC_STALE;
    snprintf(c->rc.object, sizeof c->rc.object, "calls:%u", (unsigned)id);
    c->rc.revision = ink_db_last_ver(db);
    if (taken) { ink_db_flush(db); ink_exec_post(INK_EXEC_UI, &P.refresh, refresh_history, NULL); }
    if (taken && !all) {
        ink_args_t undo; ink_args_init(&undo); ink_args_int(&undo, "token", T.token);
        ink_cap_undoable(c, "phone.restore", &undo, 60000);
    }
    snprintf(c->rc.say, sizeof c->rc.say, "%s", !taken && !all ? TR("这一组通话记录已经不在了") : all ? TR("已清空通话记录") : TR("已删除这一组通话记录"));
    ink_db_close(db);
}
static void restore(ink_call_t *c) { /* BG: system.undo of phone.delete */
    int64_t token = 0; ink_args_get_int(&c->args, "token", &token);
    ink_db_t *db = token == T.token && T.count && !strcmp(T.member, ink_cap_call_member(c)) ? ink_db_open_member(&HISTORY, T.member) : NULL;
    if (!db) { c->rc.status = INK_RC_STALE; return; }
    for (int i = 0; i < T.count; i++) {
        /* one batch, written by the flush below: a write each would take seconds for a big group */
        ink_db_write_t wr = {.sort = T.sort[i], .data = T.raw[i], .len = strlen(T.raw[i]), .src = (uint8_t)c->src, .flags = INK_DB_LAZY};
        if (put_waiting(db, &wr) != INK_DB_OK) c->rc.status = INK_RC_FAILED;
    }
    T.count = 0; ink_db_flush(db); ink_db_close(db);
    ink_exec_post(INK_EXEC_UI, &P.refresh, refresh_history, NULL);
}
static void changed(void *ctx, const char *id, const ink_args_t *a) {
    (void)ctx;
    if (emergency_changed(id, a)) return;
    char member[16] = ""; ink_args_get_str(a, "member", member, sizeof member);
    if (!strcmp(id, "call.controls")) {
        if (!P.active || strcmp(member, P.call_member)) return;
        ink_args_get_bool(a, "muted", &P.muted);
        ink_comm_call_tones(P.tones, sizeof P.tones);
    }
    if (!strcmp(id, "contacts.selected")) {
        if (!P.selecting || strcmp(P.member, member) || strcmp(member, ink_member_current()->id)) return;
        ink_args_get_str(a, "number", P.number, sizeof P.number); P.selecting = false; show(&ink_phone_page); return;
    }
    if (!strcmp(id, "call.incoming") || !strcmp(id, "call.dialed")) {
        P.started = 0; P.minutes = P.seconds = 0; P.end_text[0] = P.reason[0] = 0;
        P.tones[0] = 0; P.muted = false;
        P.active = true; P.incoming = !strcmp(id, "call.incoming");
        snprintf(P.call_member, sizeof P.call_member, "%s", member);
        ink_args_get_str(a, P.incoming ? "from" : "number", P.calling, sizeof P.calling);
        sim_in_use(P.call_sim, sizeof P.call_sim);

        snprintf(P.status, sizeof P.status, "%s", P.incoming ? TR_NOOP("来电") : TR_NOOP("正在拨号"));
        if (!P.incoming && !strcmp(member, P.member)) P.number[0] = 0; /* Fossify clears the dial pad after a call: here with the page change */
    } else if (!strcmp(id, "call.answered") || !strcmp(id, "call.connected")) { P.started = ink_now_us(); snprintf(P.status, sizeof P.status, TR_NOOP("通话中")); }
    else if (!strcmp(id, "call.ended")) {
        char reason[16] = "failed"; ink_args_get_str(a, "reason", reason, sizeof reason);
        P.seconds = P.started ? (int64_t)((ink_now_us() - P.started) / 1000000) : 0;
        P.active = false; P.tones[0] = 0; P.muted = false; ended(reason);
    } else if (strcmp(id, "call.controls")) return;
    if (!strcmp(P.call_member, ink_member_current()->id) && ink_member_space_open()) {
        if (!strcmp(id, "call.dialed")) show(&CALL);
        else call_show();
    }
}
static void minute_changed(void *ctx, int h, const char *id) {
    (void)ctx; (void)h; (void)id;
    if (!P.duration || !P.active || !P.started || strcmp(P.call_member, ink_member_current()->id) || !ink_member_space_open()) return;
    int64_t minutes = (int64_t)((ink_now_us() - P.started) / 60000000);
    if (minutes == P.minutes) return;
    P.minutes = minutes;
    snprintf(P.duration_text, sizeof P.duration_text, TR("%lld 分钟"), (long long)minutes);
    lv_label_set_text(P.duration, P.duration_text); ink_ui_hint_obj(P.duration, INK_REFRESH_TEXT); ink_ui_content_changed();
}
static void call_route_changed(void *ctx, int h, const char *id) {
    (void)ctx; (void)h; (void)id;
    if (!strcmp(P.call_member, ink_member_current()->id) && ink_member_space_open()) call_show();
}
static void navigate(ink_call_t *c) {
    const char *id = c->cap->id;
    own();
    if (!strcmp(id, "phone.return")) { ink_shell_close(); resume_call(); }
    else if (!strcmp(id, "phone.keypad")) {
        if (!P.active || strcmp(P.call_member, P.member)) { c->rc.status = INK_RC_STALE; return; }
        show(on_page(&KEYPAD) ? &CALL : &KEYPAD);
    }
    else if (!strcmp(id, "phone.contacts") || !strcmp(id, "contacts.open")) { P.selecting = !strcmp(id, "phone.contacts"); ink_contacts_open(); }
    else if (!strcmp(id, "phone.recents")) { P.log_failed = false; show(&RECENTS); }
    else if (!strcmp(id, "phone.detail")) {
        int64_t record = 0; ink_args_get_int(&c->args, "id", &record);
        P.detail = (uint32_t)record; memset(&P.detail_position, 0, sizeof P.detail_position); ink_ui_push(&DETAIL, NULL);
    } else if (!strcmp(id, "phone.open")) {
        char number[24] = ""; ink_args_get_str(&c->args, "number", number, sizeof number);
        snprintf(P.number, sizeof P.number, "%s", number); P.say[0] = 0;
        if (on_page(&ink_phone_page) && P.field) number_changed(true); /* already open: fill it in place */
        else show(&ink_phone_page);
    } else if (!strcmp(id, "phone.number")) { if (!P.field || ink_textfield_short_focus(P.field)) c->rc.status = INK_RC_STALE; }
    else if (on_page(&ink_phone_page) && P.field && P.pages > 1) { /* candidates turn round: the page button only goes forward */
        P.cpage = (P.cpage + (!strcmp(id, "phone.next") ? 1 : P.pages - 1)) % P.pages; cands_show();
    } else if (P.list) ink_ui_list_turn(P.list, !strcmp(id, "phone.next") ? 1 : -1);
    else c->rc.status = INK_RC_STALE;
}
#define ID_ARG(extra) "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"integer\",\"minimum\":1}" extra "},\"required\":[\"id\"]}"
static const ink_cap_t CAPS[] = {
    {.id = "phone.emergency_open", .app = NULL, .title = "打开紧急呼叫页面", .effect = INK_FX_NAVIGATE, .child_ok = true, .while_locked = true, .locked_entry = true, .run = emergency_action},
    {.id = "phone.emergency_close", .app = NULL, .title = "返回锁屏", .effect = INK_FX_NAVIGATE, .child_ok = true, .while_locked = true, .expose = INK_EXPOSE_OWNER, .run = emergency_action},
    {.id = "phone.emergency_key", .app = NULL, .title = "按紧急拨号键", .input = "{\"type\":\"object\",\"properties\":{\"key\":{\"type\":\"string\",\"maxLength\":6}},\"required\":[\"key\"]}", .effect = INK_FX_NAVIGATE, .child_ok = true, .while_locked = true, .expose = INK_EXPOSE_OWNER, .run = emergency_action},
    {.id = "phone.keypad", .app = "phone", .locked_owner_only = true, .title = "打开或收起通话键盘", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "phone.return", .app = "phone", .locked_owner_only = true, .title = "回到通话", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "phone.open", .app = "phone", .locked_owner_only = true, .title = "打开拨号盘，可填入号码", .input = "{\"type\":\"object\",\"properties\":{\"number\":{\"type\":\"string\",\"maxLength\":20}}}", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "phone.contacts", .app = "phone", .locked_owner_only = true, .title = "从联系人选择号码", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "phone.recents", .app = "phone", .locked_owner_only = true, .title = "打开最近通话", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "phone.number", .app = "phone", .locked_owner_only = true, .title = "输入电话号码", .activity = "Phone", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "phone.key", .app = "phone", .locked_owner_only = true, .title = "按拨号键或删除一位", .input = "{\"type\":\"object\",\"properties\":{\"key\":{\"type\":\"string\",\"maxLength\":6}},\"required\":[\"key\"]}", .activity = "Phone", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = key_cap},
    {.id = "phone.prev", .app = "phone", .locked_owner_only = true, .title = "候选、通话记录或详情的上一页", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "phone.next", .app = "phone", .locked_owner_only = true, .title = "候选、通话记录或详情的下一页", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "contacts.selected", .app = "phone", .title = "联系人号码已选好", .form = INK_FORM_EVENT, .effect = INK_FX_READ, .child_ok = true},
    {.id = "phone.detail", .app = "phone", .locked_owner_only = true, .title = "打开一组通话记录的详情", .input = ID_ARG(""), .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "phone.delete", .app = "phone", .locked_owner_only = true, .title = "删除一组通话记录（可撤销）", .input = ID_ARG(",\"count\":{\"type\":\"integer\",\"minimum\":1,\"maximum\":" TRASH_TEXT "}"), .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .timeout_ms = 15000, .exec = INK_EXEC_BG, .run = erase},
    {.id = "phone.restore", .app = "phone", .locked_owner_only = true, .title = "恢复刚删除的通话记录", .input = "{\"type\":\"object\",\"properties\":{\"token\":{\"type\":\"integer\"}},\"required\":[\"token\"]}", .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .expose = INK_EXPOSE_OWNER, .timeout_ms = 15000, .exec = INK_EXEC_BG, .run = restore},
    {.id = "phone.clear", .app = "phone", .locked_owner_only = true, .title = "清空全部通话记录", .ask = TR_NOOP("清空全部通话记录？清空后找不回来"), .effect = INK_FX_LOCAL_DESTRUCTIVE, .ask_by_hand = true, .child_ok = true, .timeout_ms = 30000, .exec = INK_EXEC_BG, .run = erase},
};
int ink_phone_init(void) {
    return ink_db_register(&HISTORY) || ink_cap_register(CAPS, sizeof CAPS / sizeof *CAPS) ||
        ink_cap_subscribe("call.", on_call, NULL, INK_EXEC_BG) || ink_cap_subscribe("call.", changed, NULL, INK_EXEC_UI) ||
        ink_cap_subscribe("contacts.selected", changed, NULL, INK_EXEC_UI) || ink_state_watch("time.minute", minute_changed, NULL, INK_EXEC_UI) || ink_state_watch("audio.route", call_route_changed, NULL, INK_EXEC_UI) || ink_state_watch("bt.connected", call_route_changed, NULL, INK_EXEC_UI) ? -1 : 0;
}

bool ink_phone_activity(char *title, size_t cap) {
    if (!P.active || strcmp(P.status, "通话中") || strcmp(P.call_member, ink_member_current()->id) || !ink_member_space_open()) return false;
    snprintf(title, cap, TR("和%s通话"), P.calling);
    return true;
}
