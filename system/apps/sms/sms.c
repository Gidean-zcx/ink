/* SMS (sms-3): conversations, one conversation, a new message, search and a conversation's details after Fossify
 * Messages, the send states after Lineage Messaging, adapted to e-ink (docs/tasks/sms-3/对照表.md). The list rows,
 * bubbles, send key, deletes and stamps are LilyPhone's code (vendor/lilyphone/sms_view.c); this file is its glue:
 * the member's sms.messages as a PSRAM index, the pages, drafts, undo and the capabilities. */
/* ui-scale-runtime: dimension S.body_h,S.heights[n],h,w,x,y,top,bottom,row,used,width,cell,side,status_y,field_y,badge_w,lines,acts_h text and theme sizes measured at run time */
#include <stdio.h>
#include <string.h>
#include "ink/probe.h"
#include "ink/sys/app.h"
#include "ink/sys/appreg.h"
#include "ink/sys/confirm.h"
#include "ink/sys/data.h"
#include "ink/sys/json.h"
#include "ink/sys/members.h"
#include "ink/sys/state.h"
#include "ink/sys/notify.h"
#include "ink/sys/textfield.h"
#include "ink/sys/ui_list.h"
#include "ink/ui_shell.h"
#include "vendor/lilyphone/sms_lily.h"

enum { SMS_ROWS = 8, SMS_VISIBLE = 8, SMS_LIMIT = 512, SMS_FOUND = 50, SMS_ACTS = 4, UNDO_MAX = 65536 };
enum { K_IN, K_SENT, K_FAILED, K_PARTIAL, K_UNKNOWN, K_DRAFT, K_PENDING }; /* Lineage's states, as this service has them */
extern int ink_contacts_match(const char *query, bool exact, char *name, char *number);
extern ink_source_t ink_shell_keyboard_enter_source(void);
static const ink_activity_t LIST, THREAD, COMPOSE, LONG, INFO, SEARCH;
typedef struct { uint32_t id, at; uint16_t thread; uint8_t kind; } msg_t;
typedef struct { char number[24]; int last, draft, unread; } thread_t;
static struct { /* UI: the member's sms.messages, newest first, read once per refresh (LilyPhone's log and threads) */
    msg_t m[SMS_LIMIT + 1]; thread_t t[SMS_LIMIT + 1]; uint16_t order[SMS_LIMIT + 1]; int nm, nt, nrows, nfound;
    struct { char title[112], snippet[96], badge[16]; int thread; bool unread; } rows[SMS_LIMIT + 1];
    struct { uint32_t id; int thread; char snippet[96], badge[16]; } found[SMS_FOUND];
    sms_msg_t one;
    char pending_to[24], pending_text[INK_SMS_TEXT_MAX], raw[3 * INK_SMS_TEXT_MAX], name[97], number[25], filler[INK_SMS_TEXT_MAX + 200];
    uint32_t pending_after; bool pending;
    ink_json_t json;
} X INK_PSRAM_BSS;
static struct { uint32_t token; int n; char member[16]; size_t used; struct { uint32_t id, off, len; int64_t sort; } r[SMS_LIMIT]; char raw[UNDO_MAX]; } U INK_PSRAM_BSS;
static struct {
    ink_db_t *db;
    uint32_t shown_ver; /* sms.messages as the open page last read it; 0 when closed (atomic: read on BG) */
    const ink_activity_t *page;
    ink_ui_list_t *list;
    ink_ui_list_position_t position;
    lv_obj_t *field, *recipient, *suggestion, *notice, *send, *parts, *undo_btn, *info_btn, *thread_slot;
    bool selecting, failed, undoable, entered;
    ink_source_t src;
    char match_name[97], match_number[25], match_args[48], list_snapshot[6144], extra[1024];
    char member[16], peer[24], to[24], dpeer[24], query[64], draft[INK_SMS_TEXT_MAX], stored[INK_SMS_TEXT_MAX], result[160], say[160], hint[64], ask[160], undo[48], undo_args[80];
    char scratch[INK_SMS_TEXT_MAX], row[INK_SMS_TEXT_MAX], send_args[3 * INK_SMS_TEXT_MAX], send_label[128], info_args[4][64];
    struct { char to[24]; uint32_t id; } draft_ref;
    uint32_t rows[SMS_LIMIT], ats[SMS_LIMIT], selected, resend, resend_pending, seq, rc_id, confirm; size_t count;
    uint8_t kinds[SMS_LIMIT], resend_kind, pending_kind; uint16_t page_of[SMS_LIMIT];
    uint32_t pages[SMS_LIMIT + 1]; int16_t below[SMS_LIMIT]; int body_h, capacity;
    bool groups[SMS_LIMIT], stamps[SMS_VISIBLE], outgoing[SMS_VISIBLE], last[SMS_VISIBLE], rc_to;
    int heights[SMS_VISIBLE], rc_status; size_t thread_pages, visible, vrow[SMS_VISIBLE];
    char times[SMS_VISIBLE][32], states[SMS_VISIBLE][200], rc_say[160];
    ink_ui_field_spec_t message_fields[SMS_VISIBLE];
    ink_ui_action_spec_t message_actions[2 + SMS_VISIBLE * (SMS_ACTS + 1)];
    char act_ids[SMS_VISIBLE * (SMS_ACTS + 1)][24];
    ink_ui_page_spec_t messages;
    char peers[SMS_ROWS][64], ids[SMS_VISIBLE][32], args[SMS_VISIBLE][64], texts[SMS_VISIBLE][INK_SMS_TEXT_MAX], labels[SMS_VISIBLE][240];
    ink_ui_field_spec_t fields[3];
    ink_ui_action_spec_t actions[8];
    ink_ui_page_spec_t form;
    sms_msg_t msg;
    ink_exec_job_t refresh, settle, enter;
} S INK_PSRAM_BSS;

/* ---- the member's records: LilyPhone's log over sms.messages ---- */
static bool read_message(ink_db_t *db, uint32_t id, char *peer, char *text, char *status) { /* text: INK_SMS_TEXT_MAX */
    if (!id) { snprintf(peer, 24, "%s", X.pending_to); snprintf(text, INK_SMS_TEXT_MAX, "%s", X.pending_text); status[0] = 0; return true; } /* the one being sent */
    ink_rec_t r; bool ok = db && ink_db_get(db, id, &r, X.raw, sizeof X.raw - 1) == INK_DB_OK && ink_json_parse(&X.json, X.raw, LV_MIN(r.len, sizeof X.raw - 1)) == 0;
    if (ok) { ink_json_str(&X.json, ink_json_find(&X.json, 0, "peer"), peer, 24); ink_json_str(&X.json, ink_json_find(&X.json, 0, "text"), text, INK_SMS_TEXT_MAX); ink_json_str(&X.json, ink_json_find(&X.json, 0, "status"), status, 96); }
    return ok;
}
static int classify(const char *status) {
    if (!strcmp(status, "收到")) return K_IN;
    if (!strcmp(status, "草稿")) return K_DRAFT;
    if (strstr(status, "结果未知") || strstr(status, "unknown")) return K_UNKNOWN;
    if (!strcmp(status, "发送失败，未发出") || !strcmp(status, TR("发送失败，未发出"))) return K_FAILED;
    return strstr(status, "未发出") || strstr(status, "not sent") ? K_PARTIAL : K_SENT;
}
static bool same(const char *a, const char *b) { return phone_number_match(a, b) || !strcmp(a, b); } /* +86 and 0086 are one person */
static int thread_of(const char *peer, bool add) {
    for (int t = 0; t < X.nt; t++) if (same(X.t[t].number, peer)) return t;
    if (!add || X.nt > SMS_LIMIT) return -1;
    X.t[X.nt] = (thread_t){.last = -1, .draft = -1}; snprintf(X.t[X.nt].number, sizeof X.t[0].number, "%s", peer);
    return X.nt++;
}
static uint32_t thread_at(int t) { return X.m[X.t[t].last >= 0 ? X.t[t].last : X.t[t].draft].at; }
static void index_build(void) {
    ink_db_cursor_t cur = {.desc = true}; ink_rec_t r; char peer[24], status[96]; bool pending = X.pending;
    X.nm = X.nt = 0;
    while (S.db && X.nm < SMS_LIMIT && ink_db_next(S.db, &cur, &r, X.raw, sizeof X.raw - 1) == 1) {
        int64_t at = r.updated_s; peer[0] = status[0] = 0;
        if (ink_json_parse(&X.json, X.raw, LV_MIN(r.len, sizeof X.raw - 1))) continue;
        ink_json_str(&X.json, ink_json_find(&X.json, 0, "peer"), peer, sizeof peer); ink_json_str(&X.json, ink_json_find(&X.json, 0, "status"), status, sizeof status);
        ink_json_int(&X.json, ink_json_find(&X.json, 0, "at"), &at); /* a restored record keeps its time */
        int t = thread_of(peer, true), kind = classify(status);
        if (pending && kind == K_UNKNOWN && r.id > X.pending_after && same(peer, X.pending_to)) { kind = K_PENDING; pending = false; } /* stored before it went */
        if (t < 0 || (kind == K_DRAFT && X.t[t].draft >= 0)) continue;
        X.m[X.nm] = (msg_t){r.id, (uint32_t)at, (uint16_t)t, (uint8_t)kind};
        if (kind == K_DRAFT) X.t[t].draft = X.nm; else if (X.t[t].last < 0) X.t[t].last = X.nm;
        X.nm++;
    }
    int t = pending ? thread_of(X.pending_to, true) : -1;
    if (t >= 0) { /* LilyPhone's sms_add: the message with the modem, logged as pending before the service stored it */
        memmove(&X.m[1], &X.m[0], (size_t)X.nm++ * sizeof *X.m);
        for (int k = 0; k < X.nt; k++) { if (X.t[k].last >= 0) X.t[k].last++; if (X.t[k].draft >= 0) X.t[k].draft++; }
        X.m[0] = (msg_t){0, (uint32_t)ink_time_now_s(), (uint16_t)t, K_PENDING}; X.t[t].last = 0;
    }
    for (int k = 0; k < X.nt; k++) { /* newest conversation first; one with only a draft by the draft's time */
        int at = k; X.order[k] = (uint16_t)k;
        while (at && thread_at(X.order[at - 1]) < thread_at(k)) { X.order[at] = X.order[at - 1]; at--; }
        X.order[at] = (uint16_t)k;
    }
    ink_db_cursor_t nc = {0}; ink_notify_view_t n;
    while (ink_notify_next(&nc, &n) == 1) if (!n.read && !strcmp(n.channel, "message") && (t = thread_of(n.group, false)) >= 0) X.t[t].unread++;
}
const char *sms_active_number(void) { return S.page == &COMPOSE ? S.to : S.peer; }
int sms_add(const char *number, const char *text, uint32_t ts, int dir, int status, bool unread) {
    (void)ts; (void)dir; (void)status; (void)unread; X.pending_after = 0;
    for (int i = 0; i < X.nm; i++) X.pending_after = LV_MAX(X.pending_after, X.m[i].id);
    snprintf(X.pending_to, sizeof X.pending_to, "%s", number); snprintf(X.pending_text, sizeof X.pending_text, "%s", text);
    X.pending = true; index_build(); return 0;
}
int sms_thread_count(void) { return X.nt; }
const char *sms_thread_number(int i) { return X.t[X.order[i]].number; }
int sms_thread_unread(const char *number) { int t = thread_of(number, false); return t < 0 ? 0 : X.t[t].unread; }
int sms_thread_msg_count(const char *number) {
    int t = thread_of(number, false), n = 0;
    for (int i = 0; t >= 0 && i < X.nm; i++) n += X.m[i].thread == t && X.m[i].kind != K_DRAFT;
    return n;
}
static int status_of(int kind) { return kind == K_FAILED || kind == K_PARTIAL ? SMS_ST_FAILED : kind == K_UNKNOWN ? SMS_ST_UNKNOWN : kind == K_DRAFT ? SMS_ST_DRAFT : kind == K_PENDING ? SMS_ST_PENDING : SMS_ST_OK; }
const sms_msg_t *sms_thread_last(int i) { /* Fossify: a draft stands in for the last message */
    const thread_t *t = &X.t[X.order[i]]; const msg_t *m = &X.m[t->draft >= 0 ? t->draft : t->last]; char status[96];
    X.one = (sms_msg_t){.ts = thread_at(X.order[i]), .dir = m->kind == K_IN ? SMS_DIR_IN : SMS_DIR_OUT, .status = (uint8_t)status_of(m->kind)};
    if (!read_message(S.db, m->id, X.one.number, X.one.text, status)) X.one.text[0] = 0;
    return &X.one;
}
const char *sms_display_name(const char *number) { /* the contact, also when the number came with +86 or 0086 */
    const char *bare = !strncmp(number, "+86", 3) ? number + 3 : !strncmp(number, "0086", 4) ? number + 4 : NULL;
    return ink_contacts_match(number, true, X.name, X.number) > 0 || (bare && ink_contacts_match(bare, true, X.name, X.number) > 0) ? X.name : number;
}
static void whole_chars(char *s) { size_t n = strlen(s), j = n; while (j && ((unsigned char)s[j - 1] & 0xC0) == 0x80) j--; if (j-- && j + ((unsigned char)s[j] < 0xE0 ? 2 : (unsigned char)s[j] < 0xF0 ? 3 : 4) > n && (unsigned char)s[j] >= 0xC0) s[j] = 0; }
void scr_row_create(lv_obj_t *list, const char *title, const char *snippet, const char *badge, lv_event_cb_t cb, void *ctx) {
    (void)list; (void)cb; int i = (int)(intptr_t)ctx, t = X.order[i];
    if (i > SMS_LIMIT) return;
    snprintf(X.rows[i].title, sizeof X.rows[i].title, X.t[t].unread > 1 ? "%s (%d)" : "%s", title, X.t[t].unread); /* Fossify's unread count */
    snprintf(X.rows[i].snippet, sizeof X.rows[i].snippet, "%s", snippet); snprintf(X.rows[i].badge, sizeof X.rows[i].badge, "%s", badge);
    whole_chars(X.rows[i].snippet); X.rows[i].thread = t; X.rows[i].unread = X.t[t].unread > 0; X.nrows = i + 1;
}

/* ---- undo: what a delete took, in PSRAM, for system.undo (D-105) ---- */
static void undo_start(bool undoable) { U.token++; U.n = 0; U.used = 0; snprintf(U.member, sizeof U.member, "%s", S.member); S.undoable = undoable; }
static bool undo_keep(uint32_t id) { /* the record, its time written in so that the restored one keeps it */
    ink_rec_t r; char at[32] = "";
    if (U.n >= SMS_LIMIT || ink_db_get(S.db, id, &r, X.raw, sizeof X.raw - 1) != INK_DB_OK) return false;
    size_t n = LV_MIN(r.len, sizeof X.raw - 1), skip;
    if (ink_json_parse(&X.json, X.raw, n) == 0 && ink_json_find(&X.json, 0, "at") < 0) snprintf(at, sizeof at, "{\"at\":%lld,", (long long)r.updated_s);
    skip = at[0] ? 1 : 0;
    if (U.used + strlen(at) + n - skip > UNDO_MAX) return false;
    memcpy(U.raw + U.used, at, strlen(at)); memcpy(U.raw + U.used + strlen(at), X.raw + skip, n - skip);
    U.r[U.n++] = (typeof(U.r[0])){id, (uint32_t)U.used, (uint32_t)(strlen(at) + n - skip), r.sort}; U.used += strlen(at) + n - skip;
    return true;
}
bool sms_delete(int idx) {
    if (idx < 0 || idx >= X.nm || !X.m[idx].id || (S.undoable && !undo_keep(X.m[idx].id)) || ink_db_del(S.db, X.m[idx].id, (uint8_t)S.src) != INK_DB_OK) return false;
    index_build(); return true;
}
void sms_thread_delete(const char *number) {
    int t = thread_of(number, false);
    for (int i = 0; t >= 0 && i < X.nm; i++) if (X.m[i].thread == t && X.m[i].id && (!S.undoable || undo_keep(X.m[i].id))) ink_db_del(S.db, X.m[i].id, (uint8_t)S.src);
    index_build();
}

/* ---- pages ---- */
static void show_if(lv_obj_t *o, bool shown) { if (o && lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN) == shown) lv_obj_set_flag(o, LV_OBJ_FLAG_HIDDEN, !shown); }
static void set_text(lv_obj_t *l, const char *s) { if (l && strcmp(lv_label_get_text(l), s)) lv_label_set_text(l, s); }
static bool on(const ink_activity_t *page) { return S.page == page && ink_ui_page_current() && !strcmp(S.member, ink_member_current()->id); }
static void status_show(void) { /* what the last action said (until the next one), else the parts; 撤销 while it can */
    ink_jw_t w; ink_jw_init(&w, S.undo_args, sizeof S.undo_args); ink_jw_raw(&w, "{\"object\":"); ink_jw_str(&w, S.undo); ink_jw_raw(&w, "}");
    set_text(S.notice, S.say[0] ? S.say : S.hint); show_if(S.undo_btn, S.undo[0] && S.undoable); show_if(S.info_btn, !S.undo[0] || !S.undoable); /* 撤销 stands where 详情 is */
}
static void next_action(void) { S.say[0] = S.undo[0] = 0; }
static void show(const ink_activity_t *page) {
    ink_ui_drop_app("message");
    if (page != &LIST) ink_ui_push(&LIST, NULL);
    if (page == &THREAD) S.position.page = SIZE_MAX; /* the list below shared the position: the newest page */
    ink_ui_push(page, NULL);
}
void sms_pop(void) { show(&LIST); }
static void member_ready(void) {
    if (!strcmp(S.member, ink_member_current()->id)) return;
    memset(&S.position, 0, sizeof S.position); memset(&S.draft_ref, 0, sizeof S.draft_ref); S.selecting = false; U.n = 0; X.pending = false; ui_send_watch_id = 0;
    S.confirm = 0; S.to[0] = S.peer[0] = S.dpeer[0] = S.draft[0] = S.stored[0] = S.result[0] = S.say[0] = S.undo[0] = S.query[0] = 0; S.selected = S.resend = 0; X.nfound = 0;
    snprintf(S.member, sizeof S.member, "%s", ink_member_current()->id);
}
static void loaded(void *ctx, const ink_receipt_t *rc) { /* a collection came into memory: the page draws it */
    if ((uint32_t)(uintptr_t)ctx != ink_ui_generation() || strcmp(S.member, ink_member_current()->id)) return;
    S.failed = !S.db && rc->status != INK_RC_DONE; ink_ui_redraw();
}
static void load(const char *collection) {
    char json[64]; snprintf(json, sizeof json, "{\"collection\":\"%s\",\"limit\":1}", collection);
    ink_call_opts_t o = {.src = INK_SRC_TOUCH, .member = S.member, .args_json = json, .reply_exec = INK_EXEC_UI, .done = loaded, .ctx = (void *)(uintptr_t)ink_ui_generation()};
    ink_cap_call("data.list", NULL, &o);
}
static void request(const char *cap, const char *json) { /* touch: the same capability the AI calls */
    ink_call_opts_t o = {.src = INK_SRC_TOUCH, .member = S.member, .args_json = json};
    if (ink_cap_call(cap, NULL, &o) != 0) { snprintf(S.say, sizeof S.say, "%s", TR("没有完成，请重试")); status_show(); }
}
static void draft_save(void) { /* Fossify saveDraftMessage: the conversation's draft is a record, written as the page goes */
    const char *peer = S.page == &COMPOSE ? S.to : S.peer; int t = S.db && peer[0] ? thread_of(peer, false) : -1;
    if (!S.db || !peer[0] || !strcmp(S.draft, S.stored)) return;
    uint32_t id = t >= 0 && X.t[t].draft >= 0 ? X.m[X.t[t].draft].id : 0;
    snprintf(S.stored, sizeof S.stored, "%s", S.draft); snprintf(S.dpeer, sizeof S.dpeer, "%s", peer);
    if (!S.draft[0]) { if (id) ink_db_del(S.db, id, INK_SRC_TOUCH); index_build(); return; }
    ink_jw_t w; ink_jw_init(&w, X.raw, sizeof X.raw); ink_jw_raw(&w, "{\"peer\":"); ink_jw_str(&w, peer); ink_jw_raw(&w, ",\"text\":"); ink_jw_str(&w, S.draft); ink_jw_raw(&w, ",\"status\":\"草稿\"}");
    if (!id) id = ink_db_new_id(S.db);
    ink_db_write_t wr = {.id = id, .sort = id, .data = X.raw, .len = w.len, .src = INK_SRC_TOUCH};
    if (w.overflow || ink_db_put(S.db, &wr, NULL) != INK_DB_OK) { S.stored[0] = 0; snprintf(S.say, sizeof S.say, "%s", TR("草稿没存上：短信已满，删掉一些就能存")); }
    index_build();
}

/* the editor: the shared field, LilyPhone's send key, the parts line (Lineage's counter, always shown: each part is billed) */
static void parts_note(lv_event_t *e) {
    const char *t = S.field ? ink_textfield_text(S.field) : ""; int n = t[0] ? ink_comm_sms_parts(t) : 0;
    if (e) { S.resend = 0; next_action(); snprintf(S.draft, sizeof S.draft, "%s", t); } /* an edit is a new message (Fossify :890) */
    if (n > INK_SMS_PARTS_MAX) snprintf(S.hint, sizeof S.hint, TR("太长了，最多分 %d 条发送"), INK_SMS_PARTS_MAX);
    else if (n > 0 && S.recipient != S.field) {
        size_t len = strlen(t); int lo = 0, hi = 160; /* how many more 'a's still go as n parts: one unit in GSM 7-bit and in UCS2 */
        memcpy(X.filler, t, len);
        while (lo < hi) { int mid = (lo + hi + 1) / 2; memset(X.filler + len, 'a', (size_t)mid); X.filler[len + (size_t)mid] = 0; if (ink_comm_sms_parts(X.filler) == n) lo = mid; else hi = mid - 1; }
        if (n > 1) snprintf(S.hint, sizeof S.hint, TR("共 %d 条，第 %d 条还能打 %d 字"), n, n, lo); else snprintf(S.hint, sizeof S.hint, TR("第 1 条，还能打 %d 字"), lo);
    } else S.hint[0] = 0;
    status_show();
}
static void entered(void *ctx, const char *value);
static void reply_field(lv_obj_t *screen);
void sms_field_set(const char *text) {
    snprintf(S.draft, sizeof S.draft, "%s", text); S.draft_ref.id = 0;
    if (S.field && S.field != S.recipient) { lv_obj_t *screen = lv_obj_get_screen(S.field); lv_obj_delete(S.field); reply_field(screen); parts_note(NULL); }
}
static void refresh_thread(void);
void scr13_1_populate(void) { refresh_thread(); }
void lily_sent(void) { /* LilyPhone pops back to the conversation at once: the message is there as pending */
    draft_save();
    if (S.page == &COMPOSE) { snprintf(S.peer, sizeof S.peer, "%s", S.to); S.field = NULL; S.position.page = SIZE_MAX; S.selected = 0; show(&THREAD); }
    else scr13_1_populate();
}
static void settle(void *arg) { /* the send's receipt, after the dispatcher has started watching it */
    (void)arg;
    if (!S.rc_id || S.rc_id != ui_send_watch_id) return;
    ui_send_watch_id = 0; X.pending = false;
    if (S.resend_pending && S.rc_to && S.db && S.pending_kind == K_FAILED) ink_db_del(S.db, S.resend_pending, INK_SRC_TOUCH); /* Fossify updates the original */
    S.resend_pending = 0;
    snprintf(S.result, sizeof S.result, "%s", S.rc_say[0] ? S.rc_say : S.rc_status == INK_RC_DONE ? "" : TR("没有完成，请重试"));
    if (!S.rc_to) snprintf(S.say, sizeof S.say, "%s", S.result); /* nothing stored: why, until the next action */
    if (on(&THREAD) && S.list) scr13_1_populate(); else if (on(&LIST)) ink_ui_redraw();
}
static void sent(void *ctx, const ink_receipt_t *rc) { /* waiting for a person's yes: still in flight, the card's result settles it */
    unsigned token = 0; S.rc_id = (uint32_t)(uintptr_t)ctx; S.rc_status = rc->status; S.rc_to = !strncmp(rc->object, "message:to:", 11);
    snprintf(S.rc_say, sizeof S.rc_say, "%s", rc->say); ink_shell_airplane_card(rc);
    if (rc->status == INK_RC_PENDING_CONFIRMATION && sscanf(rc->object, "confirm:%u", &token) == 1) { S.confirm = token; if (on(&THREAD) && S.list) scr13_1_populate(); }
    else ink_exec_post(INK_EXEC_UI, &S.settle, settle, NULL);
}
static void send_confirmed(void *ctx, const char *event, const ink_args_t *a) {
    (void)ctx; (void)event; int64_t token = 0; char status[16] = "", object[48] = "";
    if (!S.confirm || !ink_args_get_int(a, "token", &token) || token != S.confirm) return;
    ink_args_get_str(a, "status", status, sizeof status); ink_args_get_str(a, "object", object, sizeof object); S.rc_say[0] = 0; ink_args_get_str(a, "say", S.rc_say, sizeof S.rc_say);
    S.confirm = 0; S.rc_id = ui_send_watch_id; S.rc_status = !strcmp(status, "done") ? INK_RC_DONE : INK_RC_FAILED; S.rc_to = !strncmp(object, "message:to:", 11);
    if (!S.rc_say[0] && !S.rc_to) snprintf(S.rc_say, sizeof S.rc_say, "%s", TR("没有发出"));
    settle(NULL);
}
uint32_t ui_sms_send(const char *number, const char *text) { /* message.send; a long text goes as the text service's draft */
    ink_args_t *a = ink_app_alloc(sizeof *a, true); uint32_t id = 0;
    if (!a) return 0;
    ink_args_init(a); ink_args_str(a, "to", number);
    if (ink_args_str(a, "text", text) != 0) {
        const char *page = S.draft_ref.id && !strcmp(number, S.draft_ref.to) && !strcmp(S.draft, text) ? LONG.name : COMPOSE.name;
        uint32_t draft = !strcmp(page, LONG.name) ? S.draft_ref.id : 0;
        if (!draft) {
            ink_text_cfg_t cfg = {.type = INK_TEXT_PLAIN, .max = INK_SMS_TEXT_MAX - 1, .initial = text, .draft_page = page};
            int session = ink_text_focus(&cfg, NULL, NULL); draft = session > 0 ? ink_text_draft_id() : 0;
            if (session > 0) ink_text_blur(session);
        }
        if (!draft) { ink_app_free(a); return 0; }
        ink_args_str(a, "page", page); ink_args_int(a, "draft", draft);
    }
    S.seq = S.seq + 1 ? S.seq + 1 : 1; S.resend_pending = S.resend; S.pending_kind = S.resend_kind; S.resend = 0;
    ink_call_opts_t o = {.src = S.src, .member = S.member, .reply_exec = INK_EXEC_UI, .done = sent, .ctx = (void *)(uintptr_t)S.seq};
    if (ink_cap_call("message.send", a, &o) == 0) id = S.seq; else S.resend_pending = 0;
    ink_app_free(a); return id;
}
const char *sms_field_text(lv_obj_t *field) { return S.entered ? S.draft : ink_textfield_text(field); } /* the keyboard's send: what was entered */
static void send_with(ink_source_t src) {
    if (S.page == &COMPOSE) snprintf(S.to, sizeof S.to, "%s", ink_textfield_text(S.recipient));
    S.src = src; next_action(); scr13_2_body_ta = S.field; scr13_2_status = S.notice; lily_send(); /* LilyPhone's send key */
    if (S.notice && strcmp(lv_label_get_text(S.notice), S.hint)) { snprintf(S.say, sizeof S.say, "%s", lv_label_get_text(S.notice)); }
}
static void send_clicked(lv_event_t *e) { (void)e; send_with(INK_SRC_TOUCH); }
static void enter_later(void *arg) { (void)arg; S.entered = S.field && (on(&THREAD) || on(&COMPOSE)); if (S.entered) send_with(ink_shell_keyboard_enter_source()); S.entered = false; }
static void entered(void *ctx, const char *value) { (void)ctx; S.draft_ref.id = 0; snprintf(S.draft, sizeof S.draft, "%s", value); ink_exec_post(INK_EXEC_UI, &S.enter, enter_later, NULL); } /* not inside the field's own event: sending rebuilds it */
static int button_w(const char *text) { return LV_MAX(INK_CONTROL, ink_ui_text_width(text, INK_FONT_LIST, INK_WEIGHT_MEDIUM) + 2 * (INK_GAP_XS + INK_STROKE)); }
static int reply_button_w(void) { return LV_MAX(button_w(TR("展开")), button_w(TR("发送"))); }
static void reply_field(lv_obj_t *screen) {
    lv_display_t *display = lv_obj_get_display(screen); lv_display_enable_invalidation(display, false); /* not dirty at its first origin */
    int bottom = INK_CANVAS_H - INK_MARGIN - ink_textfield_height(INK_FIELD_SINGLE), button = reply_button_w();
    ink_text_cfg_t cfg = {.type = INK_TEXT_PLAIN, .enter = INK_ENTER_SEND, .max = INK_SMS_TEXT_MAX - 1, .initial = S.draft, .hint = TR("内容")};
    S.field = ink_textfield_create(screen, &cfg, entered, NULL, false); lv_obj_set_pos(S.field, ink_ui_px(INK_MARGIN + button + INK_GAP), ink_ui_px(bottom));
    lv_obj_set_width(S.field, ink_ui_px(INK_CANVAS_W - 2 * INK_MARGIN - 2 * button - 2 * INK_GAP));
    ink_textfield_bind_short(S.field, &S.form, &S.fields[0], &S.actions[0]); lv_obj_add_event_cb(S.field, parts_note, LV_EVENT_VALUE_CHANGED, NULL);
    lv_display_enable_invalidation(display, true); lv_obj_invalidate(S.field);
}
static void undo_clicked(lv_event_t *e) { (void)e; request("system.undo", S.undo_args); }
static void status_row(lv_obj_t *screen, int y, bool field) { /* the notice and 撤销 beside it; above the field: one line in its label's place, 撤销 in the top bar */
    int undo_w = button_w(TR("撤销")), lines = field ? ink_ui_line_height(INK_FONT_NOTE) : LV_MIN(INK_CONTROL, 2 * ink_ui_line_height(INK_FONT_NOTE));
    S.notice = ink_ui_notice(screen, "", field ? y : y + (INK_CONTROL - lines) / 2); S.parts = S.notice;
    lv_obj_set_width(S.notice, ink_ui_px(INK_CANVAS_W - 2 * INK_MARGIN - (field ? 0 : undo_w + INK_GAP))); lv_label_set_long_mode(S.notice, LV_LABEL_LONG_DOT); lv_obj_set_height(S.notice, ink_ui_px(lines));
    if (S.page != &COMPOSE) S.undo_btn = ink_ui_pill(screen, TR("撤销"), INK_CANVAS_W - INK_MARGIN - undo_w, field ? INK_STATUS_BAR + INK_GAP_S : y, undo_w, INK_CONTROL, false, undo_clicked, NULL);
    if (S.undo_btn) lv_obj_add_flag(S.undo_btn, LV_OBJ_FLAG_HIDDEN);
    status_show();
}

/* ---- the list: LilyPhone's rows (scr13_populate) in ink_ui_list pages, Fossify's order and draft line ---- */
static void bind_item(lv_obj_t *slot, size_t index, ink_ui_field_spec_t *field, ink_ui_action_spec_t *action, void *ctx) {
    (void)ctx; unsigned at = index % SMS_ROWS; int w = INK_CANVAS_W - 2 * INK_MARGIN, badge_w = ink_ui_text_width(X.rows[index].badge, INK_FONT_NOTE, INK_WEIGHT_REGULAR) + INK_GAP_XS;
    lv_obj_set_style_radius(slot, 0, 0); lv_obj_set_style_border_side(slot, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_border_color(slot, ink_ui_separator(), 0); lv_obj_set_style_border_width(slot, ink_ui_px(index % (size_t)S.capacity ? INK_DIVIDER : 0), 0);
    if (!lv_obj_get_child_count(slot)) {
        ink_ui_line(slot, "", INK_FONT_LIST, INK_WEIGHT_MEDIUM, 0, INK_GAP_XS, w);
        lv_obj_t *badge = ink_ui_line(slot, "", INK_FONT_NOTE, INK_WEIGHT_REGULAR, 0, INK_GAP_XS, w); lv_obj_set_style_text_align(badge, LV_TEXT_ALIGN_RIGHT, 0); lv_obj_set_style_text_color(badge, ink_ui_secondary(), 0);
        lv_obj_set_style_text_color(ink_ui_line(slot, "", INK_FONT_NOTE, INK_WEIGHT_REGULAR, 0, INK_GAP_XS + ink_ui_line_height(INK_FONT_LIST), w), ink_ui_secondary(), 0);
    }
    lv_obj_t *title = lv_obj_get_child(slot, 0);
    lv_obj_set_style_text_font(title, ink_ui_font_px(INK_FONT_LIST, X.rows[index].unread ? INK_WEIGHT_BOLD : INK_WEIGHT_MEDIUM), 0); /* unread: ● and bold, not a coloured badge */
    lv_obj_set_width(title, ink_ui_px(w - badge_w - INK_GAP_S)); set_text(title, X.rows[index].title);
    set_text(lv_obj_get_child(slot, 1), X.rows[index].badge); set_text(lv_obj_get_child(slot, 2), X.rows[index].snippet);
    snprintf(S.labels[at], sizeof S.labels[at], "%s · %s", X.rows[index].snippet, X.rows[index].badge);
    snprintf(S.peers[at], sizeof S.peers[at], "{\"peer\":\"%.20s\"}", X.t[X.rows[index].thread].number);
    snprintf(S.ids[at], sizeof S.ids[at], "sms:%u", (unsigned)X.m[X.t[X.rows[index].thread].last >= 0 ? X.t[X.rows[index].thread].last : X.t[X.rows[index].thread].draft].id);
    *field = (ink_ui_field_spec_t){.id = S.ids[at], .role = "listitem", .label = X.rows[index].title, .value = S.labels[at], .object = {S.ids[at], ink_db_last_ver(S.db)}};
    *action = (ink_ui_action_spec_t){.id = S.ids[at], .label = TR("打开会话"), .capability = "sms.thread", .args_json = S.peers[at], .object = field->object, .enabled = true};
}
static void bind_found(lv_obj_t *slot, size_t index, ink_ui_field_spec_t *field, ink_ui_action_spec_t *action, void *ctx) { /* a search hit: who, when, where it was said */
    unsigned at = index % SMS_ROWS; int t = X.found[index].thread;
    snprintf(X.rows[index].title, sizeof X.rows[index].title, "%s", sms_display_name(X.t[t].number));
    snprintf(X.rows[index].snippet, sizeof X.rows[index].snippet, "%s", X.found[index].snippet); snprintf(X.rows[index].badge, sizeof X.rows[index].badge, "%s", X.found[index].badge);
    X.rows[index].thread = t; X.rows[index].unread = false; bind_item(slot, index, field, action, ctx);
    snprintf(S.peers[at], sizeof S.peers[at], "{\"peer\":\"%.20s\",\"id\":%u}", X.t[t].number, (unsigned)X.found[index].id);
}
static void search(void) { /* Fossify MainActivity :543-590: conversations whose name or number has it, then messages, newest first */
    char peer[24], status[96], *hit; X.nfound = 0; index_build();
    for (int i = 0; S.query[0] && i < X.nt && X.nfound < SMS_FOUND; i++) {
        const char *number = X.t[X.order[i]].number;
        if (strstr(sms_display_name(number), S.query) || strstr(number, S.query)) X.found[X.nfound++] = (typeof(X.found[0])){.thread = X.order[i]};
    }
    for (int i = 0; S.query[0] && i < X.nm && X.nfound < SMS_FOUND; i++) {
        if (X.m[i].kind == K_DRAFT || !read_message(S.db, X.m[i].id, peer, S.row, status) || !(hit = strstr(S.row, S.query))) continue;
        for (int k = 0; k < 6 && hit > S.row; k++) do hit--; while (hit > S.row && ((unsigned char)*hit & 0xC0) == 0x80); /* a little before it, no colour */
        typeof(X.found[0]) *f = &X.found[X.nfound++]; f->id = X.m[i].id; f->thread = X.m[i].thread;
        strcpy(f->snippet, hit > S.row ? "…" : ""); lily_snippet(f->snippet + strlen(f->snippet), (int)(sizeof f->snippet - strlen(f->snippet)), hit); whole_chars(f->snippet);
    }
    for (int i = 0; i < X.nfound; i++) { int t = X.found[i].thread; lily_stamp(X.found[i].badge, sizeof X.found[i].badge, X.found[i].id ? X.m[X.t[t].last].at : thread_at(t), true);
        for (int k = 0; X.found[i].id && k < X.nm; k++) if (X.m[k].id == X.found[i].id) lily_stamp(X.found[i].badge, sizeof X.found[i].badge, X.m[k].at, true); }
}

/* ---- the conversation: pages of bubbles from the newest back, a time row after five minutes (Fossify :1322) ---- */
static const char *const ACT_LABEL[] = {TR_NOOP("重新编辑"), TR_NOOP("重发"), TR_NOOP("再发一次（可能重复）"), TR_NOOP("复制"), TR_NOOP("删除")};
static const char *const ACT_CAP[] = {"sms.retry", "sms.resend", "sms.send_again", "sms.copy", "sms.delete"};
static int acts_of(size_t row, int *which) { /* not sent: Fossify's edit and Lineage's resend; unknown: only a person's resend */
    int k = S.kinds[row], n = 0;
    if (k == K_PENDING || (k != K_FAILED && k != K_PARTIAL && k != K_UNKNOWN && S.rows[row] != S.selected)) return 0;
    if (k == K_FAILED || k == K_PARTIAL) { which[n++] = 0; which[n++] = 1; } else if (k == K_UNKNOWN) which[n++] = 2;
    which[n++] = 3; which[n++] = 4; return n;
}
static int acts(lv_obj_t *box, size_t row, size_t vis, lv_coord_t top) { /* worded buttons under it: one row on its side, wrapped when they do not fit */
    int which[SMS_ACTS + 1], n = acts_of(row, which), total = -INK_GAP_S, x = 0, y = INK_GAP_XS;
    for (int i = 0; i < n; i++) total += button_w(TR(ACT_LABEL[which[i]])) + INK_GAP_S;
    int x0 = S.kinds[row] != K_IN && total <= SMS_CONTENT_W ? SMS_CONTENT_W - total : 0;
    for (int i = 0; i < n; i++) {
        int w = button_w(TR(ACT_LABEL[which[i]]));
        if (x && x + w > SMS_CONTENT_W) { x = 0; y += INK_CONTROL + INK_GAP_S; }
        if (box) { lv_obj_t *b = ink_ui_pill(box, TR(ACT_LABEL[which[i]]), x0 + x, 0, w, INK_CONTROL, false, NULL, NULL); lv_obj_set_y(b, top + ink_ui_px(y)); ink_shell_bind(b, ACT_CAP[which[i]], S.args[vis]); }
        x += w + INK_GAP_S;
    }
    return n ? y + INK_CONTROL : 0;
}
void scr13_1_bubble_event(lv_event_t *e) { char json[32]; snprintf(json, sizeof json, "{\"id\":%d}", (int)(intptr_t)lv_event_get_user_data(e)); request("sms.select", json); } /* LilyPhone opened a dialog */
static int stamp_h(void) { return ink_ui_line_height(INK_FONT_NOTE) + INK_GAP_S; }
static size_t text_end(const char *text, size_t start, size_t row) { /* a bubble that does not fit a page goes on over pages */
    size_t end = start, len = strlen(text); int room = LV_MAX(S.body_h - stamp_h() - 2 * INK_GAP_S - INK_GAP_M - S.below[row], ink_ui_line_height(INK_FONT_LIST));
    while (end < len) {
        size_t next = ink_text_cluster_end(text, end); memcpy(S.scratch, text + start, next - start); S.scratch[next - start] = 0;
        if (end > start && ink_ui_text_height(S.scratch, INK_FONT_LIST, INK_WEIGHT_REGULAR, SMS_BUBBLE_W - 2 * INK_GAP_S) > room) break;
        end = next;
    }
    return end;
}
static void state_of(size_t row, const char *status, char *out, size_t cap) { /* what is written under it: Lineage's words, 已发出 under the last one only (Fossify ThreadSent) */
    int k = S.kinds[row]; bool newest_out = true;
    for (size_t i = 0; i < row; i++) newest_out &= S.kinds[i] == K_IN;
    snprintf(out, cap, "%s%s%s", k == K_IN || (k == K_SENT && !newest_out) ? "" : k == K_PENDING ? (S.confirm ? TR("等你确认后发出") : TR("发送中")) : TR(status),
        k == K_UNKNOWN ? "\n" : "", k == K_UNKNOWN ? TR("可能已经发到，先问问对方") : "");
}
static void thread_layout(size_t wanted) { /* pack from the newest fragment back; page_of: where each message ends */
    size_t page = 0, n = 0; int used = 0, sh = stamp_h(); bool last_stamp = false; S.visible = 0;
    for (size_t row = 0; row < S.count; row++) {
        char peer[24], status[96], state[200], timestamp[32]; read_message(S.db, S.rows[row], peer, S.row, status);
        state_of(row, status, state, sizeof state); lily_stamp(timestamp, sizeof timestamp, S.ats[row], false);
        for (size_t parts = S.pages[row + 1] - S.pages[row], part = parts; part-- > 0;) {
            size_t start = 0; for (size_t i = 0; i < part; i++) start = text_end(S.row, start, row);
            size_t end = text_end(S.row, start, row); bool tail = part + 1 == parts; memcpy(S.scratch, S.row + start, end - start); S.scratch[end - start] = 0;
            int h = ink_ui_text_height(S.scratch, INK_FONT_LIST, INK_WEIGHT_REGULAR, SMS_BUBBLE_W - 2 * INK_GAP_S) + 2 * INK_GAP_S + INK_GAP_M + (tail ? S.below[row] : 0);
            int next = used + h + (!used || last_stamp ? sh : 0);
            if (used && (next > S.body_h || n == SMS_VISIBLE)) { page++; n = 0; used = 0; next = h + sh; }
            if (tail) S.page_of[row] = (uint16_t)page;
            if (page == wanted) {
                memcpy(S.texts[n], S.scratch, end - start + 1); snprintf(S.states[n], sizeof S.states[n], "%s", tail ? state : "");
                snprintf(S.times[n], sizeof S.times[n], "%s", timestamp);
                snprintf(S.labels[n], sizeof S.labels[n], "%s · %s", timestamp, S.kinds[row] == K_PENDING ? TR("发送中") : TR(status));
                snprintf(S.ids[n], sizeof S.ids[n], "sms:%u", (unsigned)S.rows[row]); snprintf(S.args[n], sizeof S.args[n], "{\"id\":%u}", (unsigned)S.rows[row]);
                S.heights[n] = h; S.stamps[n] = part == 0 && S.groups[row]; S.outgoing[n] = S.kinds[row] != K_IN; S.last[n] = tail; S.vrow[n] = row; S.visible = n + 1;
            }
            used = next; n++; last_stamp = part == 0 && S.groups[row];
        }
    }
    S.thread_pages = page + 1;
}
static void build_rows(void) {
    int t = thread_of(S.peer, false); S.count = 0; S.pages[0] = 0;
    for (int i = 0; t >= 0 && i < X.nm && S.count < SMS_LIMIT; i++) {
        char peer[24], status[96], state[200]; size_t row = S.count, end = 0;
        if (X.m[i].thread != t || X.m[i].kind == K_DRAFT || !read_message(S.db, X.m[i].id, peer, S.row, status)) continue;
        S.rows[row] = X.m[i].id; S.ats[row] = X.m[i].at; S.kinds[row] = X.m[i].kind; state_of(row, status, state, sizeof state);
        S.below[row] = (int16_t)((state[0] ? ink_ui_text_height(state, INK_FONT_NOTE, INK_WEIGHT_REGULAR, SMS_BUBBLE_W) + INK_GAP_XS : 0) + acts(NULL, row, 0, 0));
        S.pages[row + 1] = S.pages[row];
        do { end = text_end(S.row, end, row); S.pages[row + 1]++; } while (S.row[end]);
        S.count++;
    }
    for (size_t i = 0; i < S.count; i++) S.groups[i] = i + 1 == S.count || S.ats[i] >= S.ats[i + 1] + 300;
    thread_layout(SIZE_MAX);
}
static void thread_bar(void) {
    if (!S.thread_slot) return;
    lv_obj_t *bar = lv_obj_get_child(lv_obj_get_parent(S.thread_slot), -1);
    if (bar == S.thread_slot) return;
    lv_obj_set_y(bar, 0); lv_obj_set_flag(bar, LV_OBJ_FLAG_HIDDEN, S.count == 0);
}
static void bind_thread(lv_obj_t *slot, size_t index, ink_ui_field_spec_t *field, ink_ui_action_spec_t *action, void *ctx) {
    (void)ctx; thread_layout(S.thread_pages - 1 - index);
    if (S.thread_slot != slot) { S.thread_slot = slot; lv_obj_set_style_radius(slot, 0, 0); ink_ui_surface_style(slot, INK_SURFACE_WHITE, false); lv_obj_set_pos(slot, 0, ink_ui_px(INK_CONTROL + INK_GAP)); lv_obj_set_size(slot, ink_ui_px(INK_CANVAS_W), ink_ui_px(S.body_h)); lv_obj_remove_flag(slot, LV_OBJ_FLAG_CLICKABLE); }
    lv_obj_update_layout(slot); lv_area_t dirty; lv_obj_get_coords(slot, &dirty); int old_top = dirty.y2 + 1;
    lv_obj_t *old = lv_obj_get_child_count(slot) > 1 ? lv_obj_get_child(slot, 1) : NULL; /* the messages' box */
    for (uint32_t i = 0; old && i < lv_obj_get_child_count(old); i++) { lv_area_t a; lv_obj_get_coords(lv_obj_get_child(old, i), &a); if (a.y1 < old_top) old_top = a.y1; }
    lv_display_t *display = lv_obj_get_display(slot); lv_display_enable_invalidation(display, false);
    while (lv_obj_get_child_count(slot)) lv_obj_delete(lv_obj_get_child(slot, 0));
    lv_obj_t *away = ink_ui_box(slot, 0, 0, INK_CANVAS_W, S.body_h, 0, INK_SURFACE_WHITE, false), *box = ink_ui_box(slot, INK_MARGIN, 0, SMS_CONTENT_W, S.body_h, 0, INK_SURFACE_WHITE, false);
    if (S.selected) ink_shell_bind(away, "sms.select", "{\"id\":0}"); /* a tap beside the messages ends the selection */
    lv_obj_set_style_bg_opa(box, LV_OPA_TRANSP, 0);
    S.messages = (ink_ui_page_spec_t){.id = THREAD.name, .title = TR("短信"), .object = {"sms.messages", ink_db_last_ver(S.db)}, .fields = S.message_fields, .field_count = S.visible, .actions = S.message_actions, .action_count = 2};
    S.message_actions[0] = (ink_ui_action_spec_t){.id = "prev", .label = TR("上一页"), .capability = "sms.prev", .enabled = index > 0};
    S.message_actions[1] = (ink_ui_action_spec_t){.id = "next", .label = TR("下一页"), .capability = "sms.next", .enabled = index + 1 < S.thread_pages};
    lv_coord_t y = 0;
    for (size_t i = S.visible; i-- > 0;) { /* oldest at the top */
        size_t row = S.vrow[i], at = S.visible - 1 - i; int which[SMS_ACTS + 1], n = S.last[i] ? acts_of(row, which) : 0;
        if (S.stamps[i] || i + 1 == S.visible) { lv_obj_t *l = ink_ui_line(box, S.times[i], INK_FONT_NOTE, INK_WEIGHT_REGULAR, 0, 0, SMS_CONTENT_W); lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0); lv_obj_set_style_text_color(l, ink_ui_secondary(), 0); lv_obj_set_y(l, y); y += ink_ui_px(stamp_h()); }
        S.msg.dir = S.outgoing[i] ? SMS_DIR_OUT : SMS_DIR_IN; S.msg.status = (uint8_t)status_of(S.kinds[row]); snprintf(S.msg.text, sizeof S.msg.text, "%s", S.texts[i]);
        lv_coord_t bottom = lily_bubble(box, &S.msg, (int)S.rows[row], y) - ink_ui_px(INK_GAP_M); /* LilyPhone's bubble */
        if (S.states[i][0]) { lv_obj_t *l = ink_ui_label(box, S.states[i], INK_FONT_NOTE, INK_WEIGHT_REGULAR, S.outgoing[i] ? SMS_CONTENT_W - SMS_BUBBLE_W : 0, 0, SMS_BUBBLE_W); lv_obj_set_style_text_align(l, S.outgoing[i] ? LV_TEXT_ALIGN_RIGHT : LV_TEXT_ALIGN_LEFT, 0); lv_obj_set_y(l, bottom + ink_ui_px(INK_GAP_XS)); lv_obj_update_layout(l); bottom += ink_ui_px(INK_GAP_XS) + lv_obj_get_height(l); }
        if (S.last[i]) bottom += ink_ui_px(acts(box, row, i, bottom));
        y = bottom + ink_ui_px(INK_GAP_M);
        S.message_fields[at] = (ink_ui_field_spec_t){.id = S.ids[i], .role = S.outgoing[i] ? "outgoing" : "incoming", .label = S.labels[i], .value = S.texts[i], .object = {S.ids[i], ink_db_last_ver(S.db)}};
        for (int k = -1; S.last[i] && k < n && S.messages.action_count < sizeof S.message_actions / sizeof *S.message_actions; k++) { /* select, then the buttons it shows */
            char *aid = S.act_ids[S.messages.action_count - 2]; snprintf(aid, sizeof S.act_ids[0], "%s:%u", k < 0 ? "select" : ACT_CAP[which[k]] + 4, (unsigned)S.rows[row]);
            S.message_actions[S.messages.action_count++] = (ink_ui_action_spec_t){.id = aid, .label = k < 0 ? (S.rows[row] == S.selected ? TR("取消选中") : TR("选中")) : TR(ACT_LABEL[which[k]]), .capability = k < 0 ? "sms.select" : ACT_CAP[which[k]], .args_json = S.args[i], .object = S.message_fields[at].object, .enabled = true};
        }
    }
    lv_obj_set_height(box, y); lv_obj_set_y(box, ink_ui_px(S.body_h) - y);
    if (!S.visible) { ink_ui_notice(slot, TR("还没有短信"), INK_GAP_L); y = ink_ui_px(S.body_h - INK_GAP_L); }
    lv_obj_update_layout(slot); lv_display_enable_invalidation(display, true); dirty.y1 += ink_ui_px(S.body_h) - y; if (old_top < dirty.y1) dirty.y1 = old_top;
    lv_obj_invalidate_area(slot, &dirty);
    *field = (ink_ui_field_spec_t){.id = "thread", .role = "group", .label = TR("短信")}; *action = (ink_ui_action_spec_t){0};
    thread_bar();
}
static void place(uint32_t id) { /* the page that shows a message (a search hit, the selection) */
    for (size_t i = 0; id && i < S.count; i++) if (S.rows[i] == id) S.position.page = S.thread_pages - 1 - S.page_of[i];
}
static void refresh_thread(void) { /* in place: only the messages' area, and the pager when the count of pages changes */
    size_t pages = S.thread_pages, page = S.position.page; bool latest = page + 1 >= pages;
    index_build(); build_rows();
    if (latest) S.position.page = S.thread_pages - 1;
    place(S.selected);
    if (pages == S.thread_pages && page == S.position.page && S.thread_slot) { ink_ui_field_spec_t f; ink_ui_action_spec_t a; bind_thread(S.thread_slot, S.position.page, &f, &a, NULL); ink_ui_content_changed(); }
    else ink_ui_list_refresh(S.list, S.thread_pages);
    thread_bar(); status_show();
    __atomic_store_n(&S.shown_ver, ink_db_last_ver(S.db), __ATOMIC_RELEASE);
}
static void refresh_later(void *arg) { /* tap-loss-1: what the person did not do waits for the finger */
    (void)arg;
    if (on(&THREAD) && S.list) refresh_thread();
    else if (on(&LIST) && S.list) { index_build(); X.nrows = 0; lily_rows(); S.count = (size_t)X.nrows; ink_ui_list_refresh(S.list, (size_t)X.nrows); __atomic_store_n(&S.shown_ver, ink_db_last_ver(S.db), __ATOMIC_RELEASE); }
}
static void refresh(void *arg) { (void)arg; if (on(&THREAD) || on(&LIST)) ink_ui_after_press(refresh_later, NULL); }
/* BG: after the service's own subscriber stored it (it subscribed first). A page that shows this version gets no UI work. */
static void received(void *ctx, const char *event, const ink_args_t *a) {
    (void)ctx; (void)event; (void)a;
    uint32_t shown = __atomic_load_n(&S.shown_ver, __ATOMIC_ACQUIRE);
    ink_db_t *db = ink_db_open_cached_member(ink_db_find_def("sms.messages"), ink_member_current()->id);
    uint32_t stored = db ? ink_db_last_ver(db) : 0;
    ink_db_close(db);
    if (!shown || stored != shown) ink_exec_post(INK_EXEC_UI, &S.refresh, refresh, NULL);
}

/* ---- creating the pages ---- */
static void search_go(lv_event_t *e) { (void)e; char json[160]; ink_jw_t w; ink_jw_init(&w, json, sizeof json); ink_jw_raw(&w, "{\"query\":"); ink_jw_str(&w, ink_textfield_text(S.field)); ink_jw_raw(&w, "}"); if (!w.overflow) request("sms.search", json); }
static void search_entered(void *ctx, const char *value) { (void)ctx; (void)value; search_go(NULL); }
static void retry_read(lv_event_t *e) { (void)e; S.failed = false; ink_ui_redraw(); }
static void contact_selected(void *ctx, const char *event, const ink_args_t *a) {
    (void)ctx; (void)event; char member[16]; ink_args_get_str(a, "member", member, sizeof member);
    if (!S.selecting || strcmp(member, S.member) || strcmp(member, ink_member_current()->id) || strcmp(ink_ui_page_name(), "PhoneContacts")) return;
    ink_args_get_str(a, "number", S.to, sizeof S.to); S.selecting = false; ink_ui_pop();
}
static void recipient_edited(lv_event_t *e) {
    (void)e; snprintf(S.to, sizeof S.to, "%s", ink_textfield_text(S.recipient));
    int id = ink_contacts_match(S.to, false, S.match_name, S.match_number);
    S.actions[5] = (ink_ui_action_spec_t){.id = "match", .label = S.match_name, .capability = "sms.recipient", .args_json = S.match_args, .enabled = id > 0};
    if (id > 0) { snprintf(S.match_args, sizeof S.match_args, "{\"id\":%u}", (unsigned)id); lv_label_set_text(lv_obj_get_child(S.suggestion, 0), S.match_name); lv_obj_remove_flag(S.suggestion, LV_OBJ_FLAG_HIDDEN); ink_shell_bind(S.suggestion, "sms.recipient", S.match_args); }
    else lv_obj_add_flag(S.suggestion, LV_OBJ_FLAG_HIDDEN);
    ink_ui_content_changed();
}
static lv_obj_t *head_button(lv_obj_t *screen, const char *text, int right, const char *cap, const char *args) { /* worded buttons in the top bar (no FAB, no menu) */
    int w = button_w(text); lv_obj_t *b = ink_ui_pill(screen, text, INK_CANVAS_W - INK_MARGIN - right - w, INK_STATUS_BAR + INK_GAP_S, w, INK_CONTROL, false, NULL, NULL);
    ink_shell_bind(b, cap, args); return b;
}
static void create(lv_obj_t *screen, void *arg) {
    (void)arg; member_ready();
    const ink_activity_t *const pages[] = {&LIST, &THREAD, &COMPOSE, &LONG, &INFO, &SEARCH};
    for (unsigned i = 0; i < sizeof pages / sizeof *pages; i++) if (!strcmp(ink_ui_page_name(), pages[i]->name)) S.page = pages[i];
    int top = INK_STATUS_BAR + INK_PRIMARY + INK_GAP_L, foot = INK_CANVAS_H - INK_MARGIN - INK_CONTROL, w = INK_CANVAS_W - 2 * INK_MARGIN;
    int field_y = INK_CANVAS_H - INK_MARGIN - ink_textfield_height(INK_FIELD_SINGLE);
    S.db = ink_db_open_cached_member(ink_db_find_def("sms.messages"), S.member);
    __atomic_store_n(&S.shown_ver, S.db ? ink_db_last_ver(S.db) : 0, __ATOMIC_RELEASE);
    if (S.db) index_build();
    ink_shell_statusbar(screen, true);
    S.form = (ink_ui_page_spec_t){.id = S.page->name, .title = S.page == &COMPOSE || S.page == &LONG ? TR("写短信") : TR("短信"), .object = {"sms.messages", 1}, .fields = S.fields, .actions = S.actions};
    if (S.page == &LONG) {
        ink_text_cfg_t cfg = {.type = INK_TEXT_PLAIN, .max = INK_SMS_TEXT_MAX - 1, .initial = S.draft, .draft_page = LONG.name, .draft_id = !strcmp(S.to, S.draft_ref.to) ? S.draft_ref.id : 0}; S.field = ink_textfield_long_create(screen, &cfg);
        if (S.field) { S.draft_ref.id = ink_text_draft_id(); snprintf(S.draft_ref.to, sizeof S.draft_ref.to, "%s", S.to); ink_app_save_state("message"); }
        int back_w = button_w(TR("返回")); ink_shell_topbar(screen, TR("写短信"));
        ink_shell_bind(ink_ui_pill(screen, TR("返回"), INK_CANVAS_W - INK_MARGIN - back_w, INK_STATUS_BAR, back_w, INK_CONTROL, false, NULL, NULL), "shell.back", "{}"); return;
    }
    if (ink_contacts_match("", false, S.match_name, S.match_number) < 0) load("phone.contacts");
    if (!S.db && S.page != &COMPOSE) { /* cold: read with the first refresh; unreadable: say so, and 重试 (D-138) */
        ink_shell_topbar(screen, TR("短信"));
        if (S.failed) ink_ui_empty(screen, TR("短信没读出来"), TR("重试"), retry_read, NULL);
        else { load("sms.messages"); lv_timer_reset(lv_display_get_refr_timer(lv_obj_get_display(screen))); }
        S.fields[0] = (ink_ui_field_spec_t){.id = "status", .role = "status", .label = TR("短信"), .value = S.failed ? TR("短信没读出来") : ""}; S.form.field_count = 1;
        return;
    }
    if (S.page == &INFO) { /* Fossify's thread menu as a page of worded buttons */
        const char *name = sms_display_name(S.peer); int contact = ink_contacts_match(S.peer, true, S.match_name, S.match_number);
        ink_shell_topbar(screen, ""); ink_ui_line(screen, name, INK_FONT_TITLE, INK_WEIGHT_BOLD, INK_MARGIN + INK_CONTROL + INK_GAP, INK_STATUS_BAR + INK_GAP_S + (INK_CONTROL - ink_ui_line_height(INK_FONT_TITLE)) / 2, w - INK_CONTROL - INK_GAP);
        ink_ui_notice(screen, S.peer, top);
        snprintf(S.info_args[0], sizeof S.info_args[0], "{\"number\":\"%.20s\"}", S.peer); snprintf(S.info_args[1], sizeof S.info_args[1], "{\"id\":%d}", contact > 0 ? contact : 0);
        snprintf(S.info_args[2], sizeof S.info_args[2], "{\"peer\":\"%.20s\"}", S.peer);
        const char *labels[] = {TR("打电话"), contact > 0 ? TR("查看联系人") : TR("加为联系人"), TR("复制号码"), TR("删除整个会话")}, *caps[] = {"call.dial", contact > 0 ? "contacts.view" : "contacts.edit", "sms.copy", "sms.delete_thread"};
        const char *args[] = {S.info_args[0], contact > 0 ? S.info_args[1] : NULL, S.info_args[2], S.info_args[2]};
        for (int i = 0; i < 4; i++) {
            ink_shell_bind(ink_ui_pill(screen, labels[i], INK_MARGIN, top + ink_ui_line_height(INK_FONT_NOTE) + INK_GAP + i * (INK_CONTROL + INK_GAP_M), w, INK_CONTROL, false, NULL, NULL), caps[i], args[i]);
            S.actions[i] = (ink_ui_action_spec_t){.id = caps[i], .label = labels[i], .capability = caps[i], .args_json = args[i], .enabled = true};
        }
        status_row(screen, foot, false);
        S.form.title = TR("会话详情"); S.form.action_count = 4; S.form.field_count = 3;
        S.fields[0] = (ink_ui_field_spec_t){.id = "name", .role = "text", .label = TR("对方"), .value = name == S.peer ? S.peer : S.match_name};
        S.fields[1] = (ink_ui_field_spec_t){.id = "number", .role = "text", .label = TR("号码"), .value = S.peer};
        S.fields[2] = (ink_ui_field_spec_t){.id = "notice", .role = "status", .label = TR("提示"), .value = lv_label_get_text(S.notice)};
        return;
    }
    ink_ui_action_spec_t model_actions[] = {{.id = "prev", .label = TR("上一页"), .capability = "sms.prev"}, {.id = "next", .label = TR("下一页"), .capability = "sms.next"}, {.id = "compose", .label = TR("新建"), .capability = "sms.compose", .enabled = true}};
    ink_ui_page_spec_t model = S.form; model.actions = model_actions; model.action_count = 3;
    if (S.page == &LIST || S.page == &SEARCH) {
        int row = ink_ui_row_double(), body_y = top;
        ink_shell_topbar(screen, S.page == &LIST ? TR("短信") : TR("搜索短信"));
        if (S.page == &LIST) { head_button(screen, TR("新建"), 0, "sms.compose", "{}"); head_button(screen, TR("搜索"), button_w(TR("新建")) + INK_GAP_S, "sms.search", "{\"query\":\"\"}"); }
        else { /* Fossify searches as you type; here 搜索 (or the keyboard's search key) does, one refresh */
            int go = button_w(TR("搜索")); ink_text_cfg_t cfg = {.type = INK_TEXT_PLAIN, .enter = INK_ENTER_SEARCH, .max = 60, .initial = S.query, .hint = TR("内容、名字或号码")};
            S.field = ink_textfield_create(screen, &cfg, search_entered, NULL, false); ink_textfield_style(S.field, INK_FIELD_SEARCH, NULL);
            lv_obj_set_pos(S.field, ink_ui_px(INK_MARGIN), ink_ui_px(top)); lv_obj_set_width(S.field, ink_ui_px(w - go - INK_GAP));
            S.fields[0] = (ink_ui_field_spec_t){.id = "query", .role = "searchbox", .label = TR("搜索短信"), .value = S.query};
            S.actions[0] = (ink_ui_action_spec_t){.id = "text", .label = TR("输入要搜的字"), .capability = "sms.text", .enabled = true};
            ink_textfield_bind_short(S.field, &S.form, &S.fields[0], &S.actions[0]);
            S.send = ink_ui_pill(screen, TR("搜索"), INK_CANVAS_W - INK_MARGIN - go, top, go, INK_CONTROL, true, search_go, NULL);
            body_y = top + ink_textfield_height(INK_FIELD_SEARCH) + INK_GAP; model_actions[2] = (ink_ui_action_spec_t){.id = "search", .label = TR("搜索"), .capability = "sms.search", .enabled = true};
        }
        status_row(screen, foot, false);
        if (S.page == &LIST) { X.nrows = 0; lily_rows(); S.count = (size_t)X.nrows; }
        lv_obj_t *body = ink_ui_box(screen, 0, body_y, INK_CANVAS_W, foot - INK_GAP - body_y, 0, INK_SURFACE_WHITE, false);
        S.capacity = LV_CLAMP(1, (foot - 2 * INK_GAP - body_y - INK_CONTROL) / (row + INK_GAP_M), SMS_ROWS);
        S.list = ink_ui_list_create(body, &model, &S.position, (unsigned)S.capacity, 1, row, S.page == &LIST ? TR("还没有短信") : S.query[0] ? TR("没有找到") : "", S.page == &LIST ? bind_item : bind_found, NULL);
        if (S.list) ink_ui_list_refresh(S.list, S.page == &LIST ? (size_t)X.nrows : (size_t)X.nfound);
        S.actions[1] = (ink_ui_action_spec_t){.id = "search", .label = TR("搜索"), .capability = "sms.search", .args_json = "{\"query\":\"\"}", .enabled = S.page == &LIST};
        S.actions[2] = (ink_ui_action_spec_t){.id = "undo", .label = TR("撤销"), .capability = "system.undo", .args_json = S.undo_args, .enabled = S.undo[0] && S.undoable};
        S.fields[1] = (ink_ui_field_spec_t){.id = "notice", .role = "status", .label = TR("提示"), .value = lv_label_get_text(S.notice)};
        S.form.fields = S.fields + 1; S.form.field_count = 1; S.form.actions = S.actions + 1; S.form.action_count = 2;
        return;
    }
    /* the conversation and the new message share the editor: 展开, the field, 发送 (the one primary button) */
    int button = reply_button_w(), info_w = button_w(TR("详情"));
    S.actions[0] = (ink_ui_action_spec_t){.id = "text", .label = TR("输入短信"), .capability = "sms.text", .enabled = true};
    S.actions[1] = (ink_ui_action_spec_t){.id = "long", .label = TR("展开"), .capability = "sms.long", .enabled = true};
    S.actions[3] = (ink_ui_action_spec_t){.id = "to", .label = TR("输入收件人"), .capability = "sms.to", .enabled = S.page == &COMPOSE};
    S.actions[4] = (ink_ui_action_spec_t){.id = "pick", .label = TR("从联系人选择"), .capability = "sms.pick", .enabled = S.page == &COMPOSE};
    S.actions[6] = (ink_ui_action_spec_t){.id = "info", .label = TR("详情"), .capability = "sms.info", .args_json = S.page == &THREAD ? S.info_args[2] : "{}", .enabled = S.page == &THREAD};
    S.actions[7] = (ink_ui_action_spec_t){.id = "undo", .label = TR("撤销"), .capability = "system.undo", .args_json = S.undo_args, .enabled = S.undo[0] && S.undoable};
    S.fields[0] = (ink_ui_field_spec_t){.id = "text", .role = "textbox", .label = TR("短信内容")};
    S.form.field_count = S.page == &COMPOSE ? 2 : 1; S.form.action_count = 8;
    if (S.page == &THREAD && !same(S.dpeer, S.peer)) { /* Fossify :299: the conversation's draft back in the field */
        int t = thread_of(S.peer, false); char peer[24], status[96]; S.draft[0] = 0;
        if (t >= 0 && X.t[t].draft >= 0 && !read_message(S.db, X.m[X.t[t].draft].id, peer, S.draft, status)) S.draft[0] = 0;
        snprintf(S.stored, sizeof S.stored, "%s", S.draft); snprintf(S.dpeer, sizeof S.dpeer, "%s", S.peer);
    }
    reply_field(screen); status_row(screen, field_y, true);
    ink_shell_bind(ink_ui_pill(screen, TR("展开"), INK_MARGIN, INK_CANVAS_H - INK_MARGIN - INK_CONTROL, button, INK_CONTROL, false, NULL, NULL), "sms.long", "{}");
    S.send = ink_ui_pill(screen, TR("发送"), INK_CANVAS_W - INK_MARGIN - button, INK_CANVAS_H - INK_MARGIN - INK_CONTROL, button, INK_CONTROL, true, send_clicked, NULL); parts_note(NULL);
    if (S.page == &COMPOSE) {
        ink_shell_topbar(screen, TR("写短信"));
        ink_ui_line(screen, TR("收件人"), INK_FONT_NOTE, INK_WEIGHT_MEDIUM, INK_MARGIN, top, w);
        S.fields[1] = (ink_ui_field_spec_t){.id = "to", .role = "textbox", .label = TR("收件人")};
        ink_text_cfg_t to = {.type = INK_TEXT_PLAIN, .enter = INK_ENTER_DONE, .max = 20, .initial = S.to, .hint = TR("姓名或电话号码")};
        S.recipient = ink_textfield_create(screen, &to, NULL, NULL, false); lv_obj_set_pos(S.recipient, ink_ui_px(INK_MARGIN), ink_ui_px(top + INK_LINE_NOTE + INK_GAP));
        lv_obj_set_width(S.recipient, ink_ui_px(w - INK_CONTROL - INK_GAP));
        ink_textfield_bind_short(S.recipient, &S.form, &S.fields[1], &S.actions[3]); lv_obj_add_event_cb(S.recipient, recipient_edited, LV_EVENT_VALUE_CHANGED, NULL);
        ink_shell_bind(ink_ui_pill(screen, "+", INK_CANVAS_W - INK_MARGIN - INK_CONTROL, top + INK_LINE_NOTE + INK_GAP, INK_CONTROL, INK_CONTROL, false, NULL, NULL), "sms.pick", "{}");
        S.suggestion = ink_ui_pill(screen, "", INK_MARGIN, top + INK_LINE_NOTE + INK_CONTROL + 2 * INK_GAP, w, INK_CONTROL, false, NULL, NULL); recipient_edited(NULL);
        return;
    }
    ink_shell_topbar(screen, ""); snprintf(S.info_args[2], sizeof S.info_args[2], "{\"peer\":\"%.20s\"}", S.peer);
    ink_ui_line(screen, sms_display_name(S.peer), INK_FONT_TITLE, INK_WEIGHT_BOLD, INK_MARGIN + INK_CONTROL + INK_GAP, INK_STATUS_BAR + INK_GAP_S + (INK_CONTROL - ink_ui_line_height(INK_FONT_TITLE)) / 2, w - INK_CONTROL - 2 * INK_GAP - LV_MAX(info_w, button_w(TR("撤销"))));
    S.info_btn = head_button(screen, TR("详情"), 0, "sms.info", S.info_args[2]); status_show();
    S.body_h = field_y - INK_GAP - top - INK_CONTROL - INK_GAP; build_rows(); place(S.selected);
    lv_obj_t *body = ink_ui_box(screen, 0, top, INK_CANVAS_W, S.body_h + INK_CONTROL + INK_GAP, 0, INK_SURFACE_WHITE, false);
    S.list = ink_ui_list_create(body, &model, &S.position, 1, 1, S.body_h, TR("还没有短信"), bind_thread, NULL); if (S.list) ink_ui_list_refresh(S.list, S.thread_pages);
    thread_bar();
}
static void destroy(void) {
    if (S.field && S.page != &SEARCH) snprintf(S.draft, sizeof S.draft, "%s", ink_textfield_text(S.field)); /* the long editor's text comes back too */
    if (S.page == &THREAD || S.page == &COMPOSE) draft_save();
    __atomic_store_n(&S.shown_ver, 0, __ATOMIC_RELEASE);
    ink_db_close(S.db); S.db = NULL; S.list = NULL; S.thread_slot = NULL; S.field = S.recipient = S.suggestion = S.notice = S.send = S.parts = S.undo_btn = S.info_btn = NULL;
}

/* ---- what the AI sees: the same rows, messages and buttons as the person ---- */
static int with_more(int n, char *out, size_t cap) { /* a list's snapshot, then the page's own fields and actions under "more" */
    if (n < 1) return -1;
    int m = ink_ui_semantics_snapshot(&S.form, S.extra, sizeof S.extra);
    ink_jw_t w = {.buf = out, .cap = cap, .len = (size_t)n - 1};
    if (m < 1) return n;
    ink_jw_raw(&w, ",\"more\":"); ink_jw_raw(&w, S.extra); ink_jw_raw(&w, "}");
    return w.overflow ? -1 : (int)w.len;
}
static int snapshot(char *out, size_t cap) {
    if (strcmp(S.member, ink_member_current()->id)) { if (cap) out[0] = 0; return -1; }
    if (S.field && S.page == &LONG) return ink_textfield_long_snapshot(S.field, out, cap);
    if (S.field && (S.page == &THREAD || S.page == &COMPOSE)) {
        char *args = S.send_args; ink_jw_t w; ink_jw_init(&w, args, sizeof S.send_args); ink_jw_raw(&w, "{\"to\":"); ink_jw_str(&w, sms_active_number()); ink_jw_raw(&w, ",\"text\":"); ink_jw_str(&w, ink_textfield_text(S.field)); ink_jw_raw(&w, "}");
        snprintf(S.send_label, sizeof S.send_label, S.hint[0] ? TR("发送短信，%s") : TR("发送短信%s"), S.hint); /* the AI sees the parts too */
        S.actions[2] = (ink_ui_action_spec_t){.id = "send", .label = S.send_label, .capability = "message.send", .args_json = args, .enabled = !w.overflow && sms_active_number()[0] && ink_textfield_text(S.field)[0] && !ui_send_watch_id};
        int n = ink_textfield_short_snapshot(S.recipient && lv_obj_has_state(S.recipient, LV_STATE_FOCUSED) ? S.recipient : S.field, out, cap);
        if (n < 0) return -1;
        w = (ink_jw_t){.buf = out, .cap = cap, .len = (size_t)n - 1};
        ink_jw_raw(&w, ",\"send_status\":"); ink_jw_str(&w, S.notice ? lv_label_get_text(S.notice) : ""); ink_jw_raw(&w, ",\"parts\":"); ink_jw_str(&w, S.hint);
        if (S.list) {
            int m = ink_ui_semantics_snapshot(&S.messages, S.list_snapshot, sizeof S.list_snapshot);
            if (m < 0) return -1;
            int extra = snprintf(S.list_snapshot + m - 1, sizeof S.list_snapshot - (size_t)m + 1, ",\"selected\":%u,\"pagination\":{\"page\":%zu,\"pages\":%zu,\"total\":%zu,\"visible\":%zu}}", (unsigned)S.selected, S.position.page + 1, S.thread_pages, S.count, S.visible);
            if (extra < 0 || (size_t)extra >= sizeof S.list_snapshot - (size_t)m + 1) return -1;
            ink_jw_raw(&w, ",\"messages\":"); ink_jw_raw(&w, S.list_snapshot);
        }
        ink_jw_raw(&w, "}"); return w.overflow ? -1 : (int)w.len;
    }
    if (S.list) return with_more(ink_ui_list_snapshot(S.list, out, cap), out, cap);
    return ink_ui_semantics_snapshot(&S.form, out, cap);
}
static bool page_key(ink_key_t k, bool lng) { return S.list && ink_ui_list_key(S.list, k, lng); }
static const ink_activity_t LIST = {.name = "Sms", .app = "message", .create = create, .destroy = destroy, .snapshot = snapshot, .on_key = page_key};
static const ink_activity_t THREAD = {.name = "SmsThread", .app = "message", .create = create, .destroy = destroy, .snapshot = snapshot, .on_key = page_key};
static const ink_activity_t COMPOSE = {.name = "SmsCompose", .app = "message", .create = create, .destroy = destroy, .snapshot = snapshot, .on_key = page_key};
static const ink_activity_t LONG = {.name = "ImeLong", .app = "message", .create = create, .destroy = destroy, .snapshot = snapshot};
static const ink_activity_t INFO = {.name = "SmsInfo", .app = "message", .create = create, .destroy = destroy, .snapshot = snapshot};
static const ink_activity_t SEARCH = {.name = "SmsSearch", .app = "message", .create = create, .destroy = destroy, .snapshot = snapshot, .on_key = page_key};

/* ---- capabilities: buttons and the AI take the same entries ---- */
static void selected_contact(void *ctx, const ink_receipt_t *rc) {
    ink_call_t *c = ctx; c->rc.status = rc->status;
    if (strcmp(ink_cap_call_member(c), ink_member_current()->id) || strcmp(ink_ui_page_name(), COMPOSE.name)) c->rc.status = INK_RC_STALE;
    else if (rc->status == INK_RC_DONE && rc->result && ink_json_parse(&X.json, rc->result, strlen(rc->result)) == 0) { ink_json_str(&X.json, ink_json_find(&X.json, 0, "number"), S.to, sizeof S.to); ink_ui_redraw(); }
    ink_cap_done(c);
}
static void mark_read(const char *peer) {
    ink_db_cursor_t cur = {0}; ink_notify_view_t n;
    while (ink_notify_next(&cur, &n) == 1) {
        if (n.read || strcmp(n.channel, "message") || !same(n.group, peer)) continue;
        ink_args_t a; ink_args_init(&a); ink_args_int(&a, "id", n.id); ink_call_opts_t o = {.src = INK_SRC_SYSTEM, .member = S.member}; ink_cap_call("notify.read", &a, &o);
    }
}
static int find_row(uint32_t id) { for (size_t i = 0; id && i < S.count; i++) if (S.rows[i] == id) return (int)i; return -1; }
static ink_rc_t ask_thread(ink_args_t *a, ink_receipt_t *rc, ink_source_t src) { (void)a; (void)src; snprintf(rc->say, sizeof rc->say, "%s", S.ask); return INK_RC_DONE; }
static void undoable(ink_call_t *c) { /* D-105: undo, no card; the way back stays until the next action */
    ink_args_t u; ink_args_init(&u); ink_args_int(&u, "token", U.token); ink_cap_undoable(c, "sms.restore", &u, 60000);
    snprintf(S.undo, sizeof S.undo, "%s", c->rc.object);
}
static void erase(ink_call_t *c) { /* sms.delete, sms.delete_thread: LilyPhone's deletes, kept for system.undo when they fit */
    char peer[24] = ""; int64_t id = 0; ink_args_get_int(&c->args, "id", &id); ink_args_get_str(&c->args, "peer", peer, sizeof peer);
    bool one = !strcmp(c->cap->id, "sms.delete"), allowed = ink_member_time_allows("system.undo"); int row = find_row((uint32_t)id), t = -1, n = 0; size_t bytes = 0;
    if (S.db && !one && (index_build(), t = thread_of(peer, false)) >= 0)
        for (int i = 0; i < X.nm; i++) { ink_rec_t r; if (X.m[i].thread == t && X.m[i].id && ink_db_get(S.db, X.m[i].id, &r, NULL, 0) == INK_DB_OK) { n++; bytes += r.len + 24u; } }
    if (!S.db || (one ? row < 0 || !S.list || S.page != &THREAD : t < 0)) { c->rc.status = INK_RC_STALE; return; }
    bool keep = allowed && (one || bytes <= UNDO_MAX);
    if (!keep && c->src != INK_SRC_CONFIRMED) { /* it could not come back: asked first (child out of time, or too big for undo) */
        snprintf(S.ask, sizeof S.ask, TR("删除和 %s 的全部 %d 条短信？删除后找不回来"), sms_display_name(peer), n);
        ink_confirm_hold(c->cap, &c->args, c->src, c->page_gen, &c->rc); return;
    }
    S.src = c->src; undo_start(keep);
    if (keep) undoable(c);
    snprintf(S.say, sizeof S.say, "%s", !keep ? TR("已删除，找不回来") : one ? TR("已删除") : TR("已删除这个会话"));
    if (one) { int idx = -1; for (int i = 0; i < X.nm; i++) if (X.m[i].id == (uint32_t)id) idx = i; lily_delete(idx); }
    else { snprintf(S.peer, sizeof S.peer, "%s", X.t[t].number); lily_delete_thread(); }
    if (on(&THREAD)) status_show();
}
static void restore(ink_call_t *c) { /* system.undo of a delete: the records back with their ids, order and times */
    int64_t token = 0; ink_args_get_int(&c->args, "token", &token);
    ink_db_t *db = token == U.token && U.n && !strcmp(U.member, ink_cap_call_member(c)) ? ink_db_open_cached_member(ink_db_find_def("sms.messages"), U.member) : NULL;
    if (!db) { c->rc.status = INK_RC_STALE; return; }
    for (int i = 0; i < U.n; i++) {
        ink_db_write_t w = {.id = U.r[i].id, .sort = U.r[i].sort, .data = U.raw + U.r[i].off, .len = U.r[i].len, .src = (uint8_t)c->src, .flags = INK_DB_LAZY};
        if (ink_db_put(db, &w, NULL) != INK_DB_OK) c->rc.status = INK_RC_FAILED;
    }
    U.n = 0; ink_db_flush(db); ink_db_close(db); S.undo[0] = S.say[0] = 0;
    if (on(&THREAD) || on(&LIST)) refresh_later(NULL);
    if (S.notice && (on(&THREAD) || on(&LIST) || on(&INFO))) status_show();
}
static void send_again(ink_call_t *c, int row, bool resend) { /* Lineage "Not sent. Tap to try again."; unknown: a person's own second try */
    char peer[24], status[96]; int k = S.kinds[row];
    if (resend ? k != K_FAILED && k != K_PARTIAL : k != K_UNKNOWN) { c->rc.status = INK_RC_STALE; return; }
    if (!resend && (c->src == INK_SRC_AI || c->src == INK_SRC_REMOTE)) { c->rc.status = INK_RC_REJECTED; snprintf(c->rc.say, sizeof c->rc.say, "%s", TR("结果未知的短信只能由本人再发一次")); return; }
    S.src = c->src; S.resend = resend ? S.rows[row] : 0; S.resend_kind = (uint8_t)k;
    if (!read_message(S.db, S.rows[row], peer, S.row, status) || !lily_dispatch(S.peer, S.row)) {
        S.resend = 0; snprintf(S.say, sizeof S.say, "%s", ui_send_watch_id ? TR("上一条还在发") : TR("现在发不出去，请重试")); status_show(); c->rc.status = INK_RC_FAILED; return;
    }
    scr13_1_populate();
}
static void navigate(ink_call_t *c) {
    member_ready();
    const char *id = c->cap->id; int64_t num = 0; char peer[24] = "", status[96]; ink_args_get_int(&c->args, "id", &num); int row = find_row((uint32_t)num);
    if (strcmp(id, "sms.text") && strcmp(id, "sms.to")) next_action();
    if (!strcmp(id, "sms.recipient")) {
        ink_cap_defer(c); ink_call_opts_t o = {.src = c->src, .member = ink_cap_call_member(c), .reply_exec = INK_EXEC_UI, .done = selected_contact, .ctx = c};
        if (ink_cap_call("contacts.select", &c->args, &o) != 0) { c->rc.status = INK_RC_FAILED; ink_cap_done(c); }
    } else if (!strcmp(id, "sms.pick")) {
        S.selecting = true; ink_call_opts_t o = {.src = c->src, .member = S.member};
        if (ink_cap_call("contacts.pick", NULL, &o)) { S.selecting = false; c->rc.status = INK_RC_FAILED; }
    } else if (!strcmp(id, "sms.to")) { if (!S.recipient || ink_textfield_short_focus(S.recipient)) c->rc.status = INK_RC_STALE; }
    else if (!strcmp(id, "sms.retry")) { /* Fossify :690-698: the text back in this conversation's field, the original remembered */
        if (row < 0 || (S.kinds[row] != K_FAILED && S.kinds[row] != K_PARTIAL) || !read_message(S.db, (uint32_t)num, peer, S.row, status)) { c->rc.status = INK_RC_STALE; return; }
        sms_field_set(S.row); S.resend = (uint32_t)num; S.resend_kind = S.kinds[row]; snprintf(S.dpeer, sizeof S.dpeer, "%s", S.peer); ink_ui_content_changed();
    } else if (!strcmp(id, "sms.resend") || !strcmp(id, "sms.send_again")) {
        if (row < 0) c->rc.status = INK_RC_STALE; else send_again(c, row, !strcmp(id, "sms.resend"));
    } else if (!strcmp(id, "sms.select")) { /* a tap instead of Android's long press: its worded buttons appear under it */
        if (num && row < 0) { c->rc.status = INK_RC_STALE; return; }
        uint32_t was = S.selected; S.selected = !num || S.selected == (uint32_t)num ? 0 : (uint32_t)num;
        if (was != S.selected) refresh_thread(); else status_show();
    } else if (!strcmp(id, "sms.copy")) {
        bool number = ink_args_get_str(&c->args, "peer", peer, sizeof peer) > 0;
        if (!number && (row < 0 || !read_message(S.db, (uint32_t)num, peer, S.row, status))) { c->rc.status = INK_RC_STALE; return; }
        if (ink_clip_set(number ? peer : S.row, false, (uint8_t)c->src)) { c->rc.status = INK_RC_FAILED; return; }
        snprintf(S.say, sizeof S.say, "%s", number ? TR("号码已复制") : TR("已复制")); if (S.notice) { status_show(); ink_ui_content_changed(); }
    } else if (!strcmp(id, "sms.thread")) {
        ink_args_get_str(&c->args, "peer", S.peer, sizeof S.peer); snprintf(S.to, sizeof S.to, "%s", S.peer); S.result[0] = 0; S.position.page = SIZE_MAX; S.field = NULL; S.selected = (uint32_t)num; mark_read(S.peer); show(&THREAD);
    } else if (!strcmp(id, "sms.compose")) {
        char to[24] = ""; ink_args_get_str(&c->args, "number", to, sizeof to);
        snprintf(S.to, sizeof S.to, "%s", to[0] ? to : S.page == &THREAD ? S.peer : ""); S.selecting = false; S.draft[0] = S.stored[0] = S.dpeer[0] = S.result[0] = 0; S.position.page = 0; S.field = NULL; show(&COMPOSE);
    } else if (!strcmp(id, "sms.info")) { ink_args_get_str(&c->args, "peer", S.peer, sizeof S.peer); ink_ui_push(&INFO, NULL); }
    else if (!strcmp(id, "sms.search")) { /* Fossify MainActivity :521: one character is enough in Chinese */
        ink_args_get_str(&c->args, "query", S.query, sizeof S.query); memset(&S.position, 0, sizeof S.position); search();
        if (S.page == &SEARCH) ink_ui_redraw(); else ink_ui_push(&SEARCH, NULL);
        snprintf(c->rc.say, sizeof c->rc.say, TR("找到 %d 条"), X.nfound);
    } else if (!strcmp(id, "sms.next") || !strcmp(id, "sms.prev")) { if (S.list) { ink_ui_list_turn(S.list, !strcmp(id, "sms.next") ? 1 : -1); thread_bar(); status_show(); } }
    else if (!strcmp(id, "sms.long")) {
        if (!S.field || S.page == &SEARCH) c->rc.status = INK_RC_STALE;
        else { snprintf(S.draft, sizeof S.draft, "%s", ink_textfield_text(S.field)); ink_ui_push(&LONG, NULL); }
    } else if (!S.field || ink_textfield_short_focus(S.field) != 0) c->rc.status = INK_RC_STALE;
}
static int save_state(void *buf, size_t cap) { if (cap < sizeof S.draft_ref) return 0; memcpy(buf, &S.draft_ref, sizeof S.draft_ref); return sizeof S.draft_ref; }
static void restore_state(const void *buf, size_t len) { if (len == sizeof S.draft_ref) memcpy(&S.draft_ref, buf, len); }
static const ink_app_ops_t OPS = {.on_start = member_ready, .save_state = save_state, .restore_state = restore_state};
static const ink_app_def_t APP = {.id = "message", .title = TR_NOOP("短信"), .icon = INK_UI_ICON(message), .entry = &LIST, .system_app = true, .child_ok = true, .ops = &OPS};
#define ID_ARG(min) "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"integer\",\"minimum\":" min "}},\"required\":[\"id\"]}"
#define PEER_ARG "{\"type\":\"object\",\"properties\":{\"peer\":{\"type\":\"string\",\"maxLength\":20}},\"required\":[\"peer\"]}"
static const ink_cap_t CAPS[] = {
    {.id = "sms.to", .app = "message", .title = "输入短信收件人号码", .activity = "SmsCompose", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "sms.pick", .app = "message", .title = "打开短信联系人选择", .activity = "SmsCompose", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "sms.retry", .app = "message", .title = "重新编辑明确未发出的短信", .activity = "SmsThread", .input = ID_ARG("1"), .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "sms.thread", .app = "message", .title = "打开短信会话，可翻到并选中一条", .input = "{\"type\":\"object\",\"properties\":{\"peer\":{\"type\":\"string\",\"maxLength\":20},\"id\":{\"type\":\"integer\",\"minimum\":1}},\"required\":[\"peer\"]}", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "sms.compose", .app = "message", .title = "写短信或回复当前会话", .input = "{\"type\":\"object\",\"properties\":{\"number\":{\"type\":\"string\",\"maxLength\":20}}}", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "sms.recipient", .app = "message", .title = "从联系人选择收件人", .activity = "SmsCompose", .input = ID_ARG("1"), .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "sms.text", .app = "message", .title = "输入短信", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "sms.long", .app = "message", .title = "打开短信长文编辑页", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "sms.prev", .app = "message", .title = "短信上一页", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "sms.next", .app = "message", .title = "短信下一页", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "sms.select", .app = "message", .title = "选中或取消选中会话里的一条短信（id 为 0 取消）", .activity = "SmsThread", .input = ID_ARG("0"), .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "sms.copy", .app = "message", .title = "复制一条短信，或复制会话的号码", .input = "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"integer\",\"minimum\":1},\"peer\":{\"type\":\"string\",\"maxLength\":20}}}", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "sms.resend", .app = "message", .title = "把明确未发出的短信原样重发", .activity = "SmsThread", .input = ID_ARG("1"), .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "sms.send_again", .app = "message", .title = "结果未知的短信由本人再发一次（可能重复）", .activity = "SmsThread", .input = ID_ARG("1"), .effect = INK_FX_NAVIGATE, .child_ok = true, .expose = INK_EXPOSE_OWNER, .run = navigate},
    {.id = "sms.delete", .app = "message", .title = "删除会话里的一条短信（可撤销）", .ask = TR_NOOP("删除这条短信？删除后找不回来"), .activity = "SmsThread", .input = ID_ARG("1"), .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .run = erase},
    {.id = "sms.restore", .app = "message", .title = "恢复刚删除的短信", .input = "{\"type\":\"object\",\"properties\":{\"token\":{\"type\":\"integer\"}},\"required\":[\"token\"]}", .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .expose = INK_EXPOSE_OWNER, .run = restore},
    {.id = "sms.delete_thread", .app = "message", .title = "删除和一个号码的整个会话（装不下撤销时先确认）", .input = PEER_ARG, .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .prepare_confirm = ask_thread, .run = erase},
    {.id = "sms.info", .app = "message", .title = "打开会话详情", .input = PEER_ARG, .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "sms.search", .app = "message", .title = "搜索短信内容、名字或号码", .input = "{\"type\":\"object\",\"properties\":{\"query\":{\"type\":\"string\",\"maxLength\":60}},\"required\":[\"query\"]}", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
};
int ink_sms_init(void) {
    return ink_cap_register(CAPS, sizeof CAPS / sizeof *CAPS) || ink_app_register(&APP) || ink_cap_subscribe("message.received", received, NULL, INK_EXEC_BG) ||
        ink_cap_subscribe("confirm.result", received, NULL, INK_EXEC_BG) || ink_cap_subscribe("confirm.result", send_confirmed, NULL, INK_EXEC_UI) || ink_cap_subscribe("contacts.selected", contact_selected, NULL, INK_EXEC_UI) ? -1 : 0;
}
