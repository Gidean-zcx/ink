/* Member memos after Fossify Notes (editor, checklist), Joplin (search, due dates) and ZSWatch (the recording screen, memo_voice.c). */
/* ui-scale-runtime: dimension w,x,y,top,bottom,field.y2,fit,bx,ideograph,list_w,voice_w,h,text_w,cell Theme tokens measured against the current text size and the editor's own layout. */
#include <stdio.h>
#include <string.h>
#include "ink/sys/app.h"
#include "ink/sys/appreg.h"
#include "ink/sys/crc.h"
#include "ink/sys/i18n.h"
#include "ink/sys/file.h"
#include "ink/sys/hub.h"
#include "ink/sys/json.h"
#include "ink/sys/media.h"
#include "ink/sys/members.h"
#include "ink/sys/power.h"
#include "ink/sys/state.h"
#include "ink/sys/time.h"
#include "ink/sys/textfield.h"
#include "ink/sys/ui_list.h"
#include "ink/ui_shell.h"

static const ink_db_def_t MEMOS = {.name = "memo.notes", .title = "备忘", .scope = INK_DB_MEMBER,
    .schema = 2, .json = true, .ai_read = true, .max_records = 200};
/* due/rid: a text memo's in [0], a checklist item's at its line; rid is the reminder the time service holds for it */
typedef struct { char body[4096], page[24], path[INK_FS_PATH_MAX]; uint32_t checked, draft, audio, ms, rid[32]; int64_t due[32], updated; bool checklist, deleted, pending; } memo_t;
static const ink_activity_t PAGES[10];
enum { LIST, DETAIL, EDIT, CHECKLIST, UNDO, FAIL, RECORD, PENDING, ITEM, DUE };
#define VOICE_DIR "/me/memos" /* a voice memo's recording: Ogg Opus, in the member's own folder */
#define HELD_MAX 8
#define FOUND_MAX 64
static struct {
    bool busy; uint32_t id; char member[16], text[1024]; ink_exec_job_t sweep;
    struct { char member[16]; uint32_t id, fails; uint64_t until; } held[HELD_MAX]; /* failed this boot: not sent again before `until` */
} T INK_PSRAM_BSS; /* the one being made into words: BG */
static struct {
    ink_db_t *db;
    ink_ui_list_t *list;
    ink_ui_list_position_t position, listed; /* listed: the memo list's own page, kept while a memo is open */
    lv_obj_t *field, *notice, *retry_button, *delete_button, *tools[4], *find, *adder, *done_row, *undo_row;
    memo_t note;
    char member[16], sent[4096], ids[4][32], args[4][96], labels[4][320], meta[4][96], object[32], target[64], save_args[160], args2[8][96];
    uint32_t id, fresh, rows[200]; /* fresh: the id a new memo was opened with, kept when its page is drawn again */
    int mode, keyboard_top, index, jump, parts[5];
    unsigned cap;
    uint32_t opened_crc; /* the text the editor opened with: no save status until the person changes it */
    ink_exec_job_t recorded, found;
    bool saving, retry, expanded, adding, blank; /* blank: no record yet, the editor's first save makes it */
    uint8_t order[33]; /* the checklist as shown: open items, the 已完成 row (32), then the done ones when expanded */
    size_t total;
    lv_obj_t *part_labels[5];
    ink_ui_action_spec_t actions[8];
    ink_ui_page_spec_t model;
    ink_exec_job_t refresh;
} M INK_PSRAM_BSS;
static struct { char query[64], member[16]; uint32_t ids[200]; size_t count; bool on; } S INK_PSRAM_BSS; /* memo.search */
static struct { char query[64], label[64]; size_t at[FOUND_MAX]; int count, cur; bool open; } F INK_PSRAM_BSS; /* memo.find */
static struct { char member[16]; uint32_t id; memo_t before; } U INK_PSRAM_BSS; /* 删掉已完成, until undone or the next change */
static struct { char member[16], page[24], body[4096]; uint32_t id, draft; } K INK_PSRAM_BSS; /* the editor's words its page could not store */
static struct { uint16_t at[4097]; char text[16][100], ids[16][24]; unsigned cpr; size_t lines; } R INK_PSRAM_BSS; /* the reading page's lines */
static void settle(void);
static void check_refresh(void);
static void find_now(void *arg);
static void adjust(lv_event_t *e);
void ink_memo_voice_create(lv_obj_t *screen);
void ink_memo_voice_destroy(void);
void ink_memo_voice_stop(void);
void ink_memo_voice_status(char *out, size_t cap);
bool ink_memo_voice_guarded(void);
bool ink_memo_voice_resume(void);

static bool read_note(ink_db_t *db, uint32_t id, memo_t *note) {
    char *raw = ink_app_alloc(INK_DB_RECORD_MAX + 1, true);
    ink_json_t *j = ink_app_alloc(sizeof *j, true);
    ink_rec_t rec = {0}; memset(note, 0, sizeof *note);
    bool ok = raw && j && ink_db_get(db, id, &rec, raw, INK_DB_RECORD_MAX) == 0;
    if (ok) {
        raw[rec.len] = 0; int64_t value = 0; note->updated = rec.updated_s;
        ok = ink_json_parse(j, raw, rec.len) == 0 && ink_json_str(j, ink_json_find(j, 0, "body"), note->body, sizeof note->body) >= 0;
        note->checklist = ink_json_eq(j, ink_json_find(j, 0, "checklist"), "true");
        note->deleted = ink_json_eq(j, ink_json_find(j, 0, "deleted"), "true");
        ink_json_int(j, ink_json_find(j, 0, "checked"), &value); note->checked = (uint32_t)value;
        value = 0; ink_json_int(j, ink_json_find(j, 0, "draft"), &value); note->draft = (uint32_t)value;
        ink_json_str(j, ink_json_find(j, 0, "page"), note->page, sizeof note->page);
        ink_json_str(j, ink_json_find(j, 0, "path"), note->path, sizeof note->path);
        value = 0; ink_json_int(j, ink_json_find(j, 0, "audio"), &value); note->audio = (uint32_t)value;
        value = 0; ink_json_int(j, ink_json_find(j, 0, "ms"), &value); note->ms = (uint32_t)value;
        note->pending = ink_json_eq(j, ink_json_find(j, 0, "pending"), "true");
        int due = ink_json_find(j, 0, "due"), rid = ink_json_find(j, 0, "rid"); /* schema 2; schema 1 records have none */
        for (int i = 0; ok && due > 0 && j->tok[due].type == 2 && i < j->tok[due].size && i < 32; i++) ink_json_int(j, due + 1 + i, &note->due[i]);
        for (int i = 0; ok && rid > 0 && j->tok[rid].type == 2 && i < j->tok[rid].size && i < 32; i++) {
            value = 0; ink_json_int(j, rid + 1 + i, &value); note->rid[i] = (uint32_t)value;
        }
    }
    ink_app_free(j); ink_app_free(raw); return ok;
}
static unsigned lines(const char *s) { unsigned n = *s ? 1 : 0; for (; *s; s++) n += *s == '\n'; return n; }
static const char *line_at(const char *s, unsigned i) { while (i-- && *s) { s += strcspn(s, "\n"); if (*s) s++; } return s; }
static size_t line_len(const char *p) { /* a reading line: to its end or R.cpr characters */ size_t len = strcspn(p, "\n"), cut = ink_text_byte_offset(p, R.cpr); return len < cut ? len : cut; }
/* After an edit of the text, each line keeps the check, due date and reminder of the same line before it. */
static uint32_t remap_checks(const char *old, const char *text, uint32_t checked, memo_t *n) {
    uint32_t used = 0, result = 0, rid[32] = {0}; int64_t due[32] = {0};
    for (unsigned i = 0; *text && i < 32; i++) {
        size_t len = strcspn(text, "\n"); const char *p = old;
        for (unsigned j = 0; *p && j < 32; j++) {
            size_t size = strcspn(p, "\n"); uint32_t bit = UINT32_C(1) << j;
            if (!(used & bit) && len == size && !memcmp(text, p, len)) {
                used |= bit; due[i] = n->due[j]; rid[i] = n->rid[j];
                if (checked & bit) result |= UINT32_C(1) << i;
                break;
            }
            p += size; if (*p) p++;
        }
        text += len; if (*text) text++;
    }
    memcpy(n->due, due, sizeof due); memcpy(n->rid, rid, sizeof rid);
    return result;
}
/* Line k of the new list is line from[k] of the old; its check, due date and reminder go with it. */
static void reorder(memo_t *n, const uint8_t *from, unsigned count, char *old) {
    uint32_t checked = n->checked, rid[32]; int64_t due[32]; size_t len = 0;
    memcpy(old, n->body, sizeof n->body); memcpy(rid, n->rid, sizeof rid); memcpy(due, n->due, sizeof due);
    memset(n->due, 0, sizeof n->due); memset(n->rid, 0, sizeof n->rid); n->checked = 0; n->body[0] = 0;
    for (unsigned k = 0; k < count; k++) {
        const char *p = line_at(old, from[k]);
        len += (size_t)snprintf(n->body + len, sizeof n->body - len, "%s%.*s", k ? "\n" : "", (int)strcspn(p, "\n"), p);
        n->checked |= (checked >> from[k] & 1u) << k; n->due[k] = due[from[k]]; n->rid[k] = rid[from[k]];
    }
}
static char fold(char c) { return c == '\n' || c == '\r' || c == '\t' ? ' ' : c >= 'A' && c <= 'Z' ? (char)(c + 32) : c; }
/* Joplin's basic search for text without spaces between words: the folded query anywhere. Byte offsets of the first max. */
static int hits(const char *text, const char *query, size_t *at, int max) {
    size_t n = strlen(query); int count = 0;
    for (size_t i = 0; n && text[i]; i++) {
        size_t k = 0;
        while (k < n && text[i + k] && fold(text[i + k]) == fold(query[k])) k++;
        if (k < n) continue;
        if (count < max && at) at[count] = i;
        count++; i += n - 1;
    }
    return count;
}
static void when(char *out, size_t cap, int64_t s) { /* 今天 9:00, 明天 9:00, 昨天 9:00 or 11月13日 9:00 */
    ink_tm_t t, now; ink_time_to_local(s, &t); ink_time_to_local(ink_time_now_s(), &now);
    int64_t days = (s + t.offset_min * 60) / 86400 - (ink_time_now_s() + now.offset_min * 60) / 86400;
    if (days >= -1 && days <= 1) snprintf(out, cap, "%s %d:%02d", days < 0 ? TR("昨天") : days ? TR("明天") : TR("今天"), t.hour, t.min);
    else snprintf(out, cap, TR("%d月%d日 %d:%02d"), t.month, t.day, t.hour, t.min);
}
static bool done_at(const memo_t *n, unsigned i) { return n->checklist && (n->checked >> i & 1u); }
static void due_text(char *out, size_t cap, const memo_t *n, unsigned i) { /* 明天 9:00 到期, 已过期, 提醒没设上 (the retry is a button) */
    char at[48]; out[0] = 0;
    if (!n->due[i] || done_at(n, i)) return;
    when(at, sizeof at, n->due[i]);
    if (n->due[i] <= ink_time_now_s()) snprintf(out, cap, "%s", TR("已过期"));
    else snprintf(out, cap, TR("%s 到期"), at);
    size_t len = strlen(out);
    if (!n->rid[i] && n->due[i] > ink_time_now_s()) snprintf(out + len, cap - len, " · %s", TR("提醒没设上"));
}
static void refresh(void *arg) {
    if (!M.db || !ink_ui_page_current() || strcmp(M.member, ink_member_current()->id)) return;
    bool was = M.note.checklist, known = M.id && M.mode != LIST && M.mode != UNDO && read_note(M.db, M.id, &M.note);
    if (known && M.note.deleted) ink_ui_push(&PAGES[UNDO], NULL);
    else if (known && !was && M.note.checklist && (M.mode == EDIT || M.mode == DETAIL)) /* just converted */ { ink_ui_pop(); ink_ui_push(&PAGES[CHECKLIST], (void *)(uintptr_t)M.id); }
    else if (M.field) return;
    else if ((M.mode == ITEM || M.mode == DUE) && (uintptr_t)arg == M.id) ink_ui_pop();
    else if (M.mode == PENDING && !M.note.pending) { ink_ui_pop(); ink_ui_push(&PAGES[DETAIL], (void *)(uintptr_t)M.id); }
    else if (M.mode == CHECKLIST && M.list) check_refresh();
    else ink_ui_redraw();
}
static uint32_t find_audio(ink_db_t *db, uint32_t audio, memo_t *n) { /* the memo holding this recording, or 0 */
    ink_db_cursor_t cur = {0}; ink_rec_t rec;
    while (audio && ink_db_next(db, &cur, &rec, NULL, 0) == 1) if (read_note(db, rec.id, n) && n->audio == audio) return rec.id;
    return 0;
}
static int held_at(const char *member, uint32_t id) {
    for (int i = 0; i < HELD_MAX; i++) if (T.held[i].id == id && !strcmp(T.held[i].member, member)) return i;
    return -1;
}
static void hold(const char *member, uint32_t id) { /* BG: its words did not go in; again after 1, 2, 4 … 32, then every 60 minutes */
    int at = held_at(member, id);
    if (at < 0) { at = 0; for (int i = 1; i < HELD_MAX; i++) if (T.held[i].until < T.held[at].until) at = i; T.held[at].fails = 0; }
    uint32_t fails = ++T.held[at].fails, minutes = fails > 6 ? 60 : UINT32_C(1) << (fails - 1);
    snprintf(T.held[at].member, sizeof T.held[at].member, "%s", member); T.held[at].id = id;
    T.held[at].until = ink_now_us() + (uint64_t)minutes * 60000000u;
}
static void sweep(void *arg);
static void voiced(void *ctx, const ink_receipt_t *rc) { /* BG: then the next one */
    (void)ctx; if (rc->status != INK_RC_DONE) hold(T.member, T.id);
    T.busy = false; sweep(NULL);
}
static void transcribed(void *ctx, const ink_receipt_t *rc) { /* BG: the words go in through memo.voice, the next one starts */
    (void)ctx; ink_json_t *j = ink_app_alloc(sizeof *j, true); int64_t ms = 0;
    bool ok = rc->status == INK_RC_DONE && rc->result && j && ink_json_parse(j, rc->result, strlen(rc->result)) == 0 &&
        ink_json_str(j, ink_json_find(j, 0, "text"), T.text, sizeof T.text) >= 0;
    if (j && ok) ink_json_int(j, ink_json_find(j, 0, "ms"), &ms);
    if (ok && !T.text[0]) snprintf(T.text, sizeof T.text, "%s", TR("这段录音里没听到说话")); /* done all the same: not sent again and again */
    ink_app_free(j);
    ink_args_t a; ink_args_init(&a); ink_args_int(&a, "id", T.id); ink_args_int(&a, "ms", ms);
    ink_call_opts_t o = {.src = INK_SRC_SYSTEM, .member = T.member, .reply_exec = INK_EXEC_BG, .done = voiced};
    if (ok && ink_cap_call("memo.voice", &a, &o) == 0) return; /* busy until the words are in: voiced, perhaps already */
    T.busy = false;
    if (ink_hub_online()) hold(T.member, T.id); /* offline: the next time online asks again */
}
static void sweep(void *arg) { /* BG: a recording a power loss cut off becomes a memo; the next one waiting gets its words */
    (void)arg; const char *member = ink_member_current()->id; uint32_t cursor = 0, count = 0, next = 0; ink_fs_entry_t e; ink_rec_t rec; ink_db_cursor_t cur = {0}; ink_fs_stat_t st;
    char path[INK_FS_PATH_MAX] = "";
    if (ink_fs_stat(VOICE_DIR, &st) != 0) cursor = UINT32_MAX; /* no voice memo yet: the index is not asked */
    ink_db_t *db = T.busy || !ink_member_space_open() ? NULL : ink_db_open_cached_member(&MEMOS, member); /* loaded by the app: no wait here */ memo_t *n = ink_app_alloc(sizeof *n, true);
    uint32_t *audio = ink_app_alloc(MEMOS.max_records * sizeof *audio, true); /* the recordings memos already hold: read once */
    while (db && n && audio && count < MEMOS.max_records && ink_db_next(db, &cur, &rec, NULL, 0) == 1) if (read_note(db, rec.id, n)) {
        int h = held_at(member, rec.id);
        if (n->audio) audio[count++] = n->audio;
        if (!next && n->pending && !n->deleted && (h < 0 || ink_now_us() >= T.held[h].until)) { next = rec.id; snprintf(path, sizeof path, "%s", n->path); }
    }
    while (db && audio && cursor != UINT32_MAX && !ink_audio_recording() && ink_fs_index(VOICE_DIR, INK_FS_AUDIO, &cursor, &e) == 1) {
        uint32_t i = 0; while (i < count && audio[i] != e.id) i++;
        if (i < count) continue;
        ink_args_t a; ink_args_init(&a); ink_args_str(&a, "path", e.path);
        ink_call_opts_t o = {.src = INK_SRC_SYSTEM, .member = member, .reply_exec = INK_EXEC_BG}; ink_cap_call("memo.voice", &a, &o);
    }
    if (next) {
        ink_args_t a; ink_args_init(&a); ink_args_str(&a, "path", path);
        ink_call_opts_t o = {.src = INK_SRC_SYSTEM, .member = member, .reply_exec = INK_EXEC_BG, .done = transcribed};
        T.id = next; snprintf(T.member, sizeof T.member, "%s", member); T.busy = true; /* before: the answer may come inside the call */
        if (ink_cap_call("audio.transcribe", &a, &o) != 0) T.busy = false;
    }
    ink_app_free(audio); ink_app_free(n); ink_db_close(db);
}
static void after_record(void *arg) { /* UI: the recording page gives way to what became of it */
    if (strcmp(ink_ui_page_name(), "MemoRecord") || !ink_ui_page_current()) return;
    ink_ui_pop(); if (arg) ink_ui_push(&PAGES[PENDING], arg);
}
static void kick(void *ctx, int h, const char *id) { (void)ctx; int64_t on = 0; if (!id || (ink_state_get_int(h, &on) && on)) ink_exec_post(INK_EXEC_BG, &T.sweep, sweep, NULL); }
static void kept(void *ctx, const ink_receipt_t *rc) { /* BG: not a memo yet (memos not read): the page goes, the sweep takes it later */
    (void)ctx; if (rc->status != INK_RC_DONE) ink_exec_post(INK_EXEC_UI, &M.recorded, after_record, NULL);
}
static void recorded(void *ctx, const char *ev, const ink_args_t *p) { /* BG: a memo's recording ended; kept, it becomes a memo */
    (void)ctx; (void)ev; char path[INK_FS_PATH_MAX] = "", member[16] = "", why[24] = ""; int64_t ms = 0;
    ink_args_get_str(p, "path", path, sizeof path); ink_args_get_str(p, "member", member, sizeof member); ink_args_get_str(p, "why", why, sizeof why); ink_args_get_int(p, "ms", &ms);
    if (strncmp(path, VOICE_DIR "/", sizeof VOICE_DIR)) return;
    if (!strcmp(why, "cancelled")) { ink_exec_post(INK_EXEC_UI, &M.recorded, after_record, NULL); return; }
    ink_args_t a; ink_args_init(&a); ink_args_str(&a, "path", path); ink_args_int(&a, "ms", ms);
    ink_call_opts_t o = {.src = INK_SRC_SYSTEM, .member = member, .reply_exec = INK_EXEC_BG, .done = kept}; ink_cap_call("memo.voice", &a, &o);
}
static void voice(ink_call_t *c, ink_db_t *db, uint32_t id, memo_t *n) { /* a recording becomes a memo; its words come from the hub */
    int64_t ms = 0; uint32_t cursor = 0; ink_fs_entry_t e; memo_t *other = ink_app_alloc(sizeof *other, true); const char *name;
    if (ink_args_get_str(&c->args, "path", n->path, sizeof n->path) > 0) {
        while ((name = strrchr(n->path, '/')) && ink_fs_index(VOICE_DIR, INK_FS_AUDIO, &cursor, &e) == 1) if (strrchr(e.path, '/') && !strcmp(strrchr(e.path, '/'), name)) n->audio = e.id;
        n->pending = true;
        if (!n->audio || !other || find_audio(db, n->audio, other)) c->rc.status = INK_RC_INVALID; /* not on the card, or a memo already */
    } else if (n->deleted || !n->path[0] || T.id != id || strcmp(T.member, ink_cap_call_member(c))) c->rc.status = INK_RC_INVALID;
    else if (n->body[0] && strlen(n->body) + 1 + strlen(T.text) >= sizeof n->body) c->rc.status = INK_RC_INVALID; /* what is written stays */
    else { size_t len = strlen(n->body); snprintf(n->body + len, sizeof n->body - len, "%s%s", len ? "\n" : "", T.text); n->pending = false; }
    if (ink_args_get_int(&c->args, "ms", &ms) && ms > 0) n->ms = (uint32_t)ms;
    ink_app_free(other);
}
static int store(ink_db_t *db, uint32_t id, const memo_t *n, uint8_t src, uint32_t *ver) { /* schema 2; body and deleted keep their names */
    char *raw = ink_app_alloc(INK_DB_RECORD_MAX + 1, true); unsigned last = 0; int result = -1;
    for (unsigned i = 0; i < 32; i++) if (n->due[i] || n->rid[i]) last = i + 1;
    if (raw) {
        ink_jw_t w; ink_jw_init(&w, raw, INK_DB_RECORD_MAX + 1);
        ink_jw_raw(&w, "{\"body\":"); ink_jw_str(&w, n->body);
        ink_jw_raw(&w, ",\"checklist\":"); ink_jw_raw(&w, n->checklist ? "true" : "false"); ink_jw_raw(&w, ",\"deleted\":"); ink_jw_raw(&w, n->deleted ? "true" : "false");
        ink_jw_raw(&w, ",\"checked\":"); ink_jw_int(&w, n->checked); ink_jw_raw(&w, ",\"draft\":"); ink_jw_int(&w, n->draft);
        ink_jw_raw(&w, ",\"page\":"); ink_jw_str(&w, n->page);
        if (n->path[0]) { ink_jw_raw(&w, ",\"path\":"); ink_jw_str(&w, n->path); ink_jw_raw(&w, ",\"audio\":"); ink_jw_int(&w, n->audio);
            ink_jw_raw(&w, ",\"ms\":"); ink_jw_int(&w, n->ms); ink_jw_raw(&w, n->pending ? ",\"pending\":true" : ",\"pending\":false"); }
        ink_jw_raw(&w, ",\"updated\":"); ink_jw_int(&w, ink_time_now_s());
        for (unsigned i = 0; i < last; i++) { ink_jw_raw(&w, i ? "," : ",\"due\":["); ink_jw_int(&w, n->due[i]); }
        for (unsigned i = 0; i < last; i++) { ink_jw_raw(&w, i ? "," : "],\"rid\":["); ink_jw_int(&w, n->rid[i]); }
        ink_jw_raw(&w, last ? "]}" : "}");
        ink_db_write_t wr = {.id = id, .sort = ink_time_now_s(), .data = raw, .len = w.len, .src = src, .written_ver = ver};
        result = w.overflow ? -1 : ink_db_put(db, &wr, NULL);
        if (!result) U.id = 0; /* the next change ends 删掉已完成's undo: its snapshot would take that change away */
    }
    ink_app_free(raw); return result;
}
static void got(void *ctx, const ink_receipt_t *rc) { *(ink_receipt_t *)ctx = *rc; }
/* Joplin's needAlarm and AlarmService: a reminder only for what is due later, not done and not deleted; the old one goes first. */
static void remind(ink_call_t *c, memo_t *n, unsigned i, bool renew) {
    bool want = !n->deleted && !done_at(n, i) && n->due[i] > ink_time_now_s();
    ink_receipt_t rc = {.status = INK_RC_FAILED}; ink_args_t a; ink_args_init(&a);
    ink_call_opts_t o = {.src = INK_SRC_SYSTEM, .member = ink_cap_call_member(c), .reply_exec = INK_EXEC_BG, .done = got, .ctx = &rc};
    if (n->rid[i] && (renew || !want)) { ink_args_int(&a, "id", n->rid[i]); ink_cap_call("reminder.cancel", &a, &o); n->rid[i] = 0; ink_args_init(&a); }
    if (!want || n->rid[i]) return;
    char title[64], at[64]; const char *p = line_at(n->body, n->checklist ? i : 0); ink_tm_t t; unsigned id = 0;
    size_t len = strcspn(p, "\n"), cut = ink_text_byte_offset(p, 15);
    ink_time_to_local(n->due[i], &t);
    snprintf(title, sizeof title, "%.*s", (int)(len < cut ? len : cut), p);
    snprintf(at, sizeof at, "%04d-%02d-%02d %02d:%02d", t.year, t.month, t.day, t.hour, t.min);
    ink_args_str(&a, "at", at); ink_args_str(&a, "title", title[0] ? title : TR("备忘")); rc.status = INK_RC_FAILED;
    if (ink_cap_call("reminder.add", &a, &o) == 0 && rc.status == INK_RC_DONE && sscanf(rc.object, "reminder:%u", &id) == 1) n->rid[i] = id;
}
static void items(ink_call_t *c, const char *op, memo_t *n, unsigned i, char *raw) { /* the checklist actions after Fossify's TasksFragment */
    char text[INK_ARGS_TEXT] = "", to[8] = ""; uint8_t from[32]; unsigned k = 0, total = lines(n->body);
    ink_args_get_str(&c->args, "text", text, sizeof text); ink_args_get_str(&c->args, "to", to, sizeof to);
    if (!strcmp(op, "add_item")) { /* several lines are several items, after the last */
        for (char *p = text, *end; *p; p = end + (*end != 0)) {
            end = p + strcspn(p, "\n"); size_t len = strlen(n->body);
            if (end > p && len + 1 + (size_t)(end - p) < sizeof n->body) snprintf(n->body + len, sizeof n->body - len, "%s%.*s", len ? "\n" : "", (int)(end - p), p);
        }
        if (lines(n->body) == total) c->rc.status = INK_RC_INVALID;
        return;
    }
    if (!strcmp(op, "uncheck_all")) { n->checked = 0; return; }
    if (!strcmp(op, "clear_done")) {
        U.before = *n; U.id = 0; snprintf(U.member, sizeof U.member, "%s", ink_cap_call_member(c));
        for (unsigned j = 0; j < total; j++) if (!done_at(n, j)) from[k++] = (uint8_t)j;
        reorder(n, from, k, raw); return;
    }
    if (i >= total) { c->rc.status = INK_RC_INVALID; return; }
    if (!strcmp(op, "check")) { n->checked ^= UINT32_C(1) << i; return; }
    if (!strcmp(op, "edit_item")) {
        const char *p = line_at(n->body, i);
        for (char *q = text; *q; q++) if (*q == '\n') *q = ' ';
        snprintf(raw, INK_DB_RECORD_MAX, "%.*s%s%s", (int)(p - n->body), n->body, text, p + strcspn(p, "\n"));
        if (!text[0] || strlen(raw) >= sizeof n->body) c->rc.status = INK_RC_INVALID; else snprintf(n->body, sizeof n->body, "%s", raw);
        return;
    }
    bool move = !strcmp(op, "move_item"), top = !strcmp(to, "top");
    if (move && top) from[k++] = (uint8_t)i;
    for (unsigned j = 0; j < total; j++) if (j != i) from[k++] = (uint8_t)j;
    if (move && !top) from[k++] = (uint8_t)i;
    if (!move) { n->due[i] = 0; remind(c, n, i, false); } /* delete_item: its reminder goes with it */
    reorder(n, from, k, raw);
}
static void mutate(ink_call_t *c) {
    int64_t id = 0, index = 0, draft = 0; ink_args_get_int(&c->args, "id", &id); ink_args_get_int(&c->args, "index", &index);
    ink_db_t *db = ink_db_open_member(&MEMOS, ink_cap_call_member(c));
    memo_t *n = ink_app_alloc(sizeof *n, true); char *raw = ink_app_alloc(INK_DB_RECORD_MAX + 1, true), at[32] = "";
    const char *op = c->cap->id + 5; /* after "memo." */
    bool save = !strcmp(op, "save"), is_voice = !strcmp(op, "voice"), fresh = is_voice && !id, add = !strcmp(op, "add_item"), renew = false, known = false;
    if (!db || !n || !raw) c->rc.status = INK_RC_FAILED;
    else if (fresh) id = ink_db_new_id(db);
    if (c->rc.status == INK_RC_DONE && !(known = read_note(db, (uint32_t)id, n)) && !save && !fresh && !add) c->rc.status = INK_RC_INVALID;
    if (c->rc.status == INK_RC_DONE) {
        if (save) {
            memcpy(raw, n->body, sizeof n->body); memcpy(at, n->page, sizeof n->page);
            ink_args_get_int(&c->args, "draft", &draft); ink_args_get_str(&c->args, "page", n->page, sizeof n->page);
            if (n->deleted || ink_text_draft_read(ink_cap_call_member(c), n->page, (uint32_t)draft, n->body, sizeof n->body) < 0) c->rc.status = INK_RC_INVALID;
            if (n->checklist) n->checked = remap_checks(raw, n->body, n->checked, n);
            known = known && n->draft == (uint32_t)draft && !strcmp(at, n->page) && !strcmp(raw, n->body); /* nothing new: not written, not moved up */
            n->draft = (uint32_t)draft; at[0] = 0;
        } else if (is_voice) voice(c, db, (uint32_t)id, n);
        else if (!strcmp(op, "undo") && !n->deleted && U.id == (uint32_t)id && !strcmp(U.member, ink_cap_call_member(c))) { *n = U.before; U.id = 0; }
        else if (!strcmp(op, "undo")) { if (n->deleted) n->deleted = false; else c->rc.status = INK_RC_INVALID; }
        else if (n->deleted) c->rc.status = INK_RC_INVALID;
        else if (!strcmp(op, "delete")) n->deleted = true;
        else if (!strcmp(op, "convert")) n->checklist = true;
        else if (!strcmp(op, "set_due")) {
            ink_tm_t t = {0}; ink_args_get_str(&c->args, "at", at, sizeof at); renew = true;
            if (index < 0 || index >= (n->checklist ? (int64_t)lines(n->body) : 1) ||
                (at[0] && sscanf(at, "%d-%d-%d %d:%d", &t.year, &t.month, &t.day, &t.hour, &t.min) != 5)) c->rc.status = INK_RC_INVALID;
            else n->due[index] = at[0] ? ink_time_from_local(&t) : 0;
        } else if (add && !n->body[0]) { n->checklist = true; items(c, op, n, 0, raw); }
        else if (!n->checklist || index < 0 || index >= 32) c->rc.status = INK_RC_INVALID;
        else items(c, op, n, (unsigned)index, raw);
        if (n->checklist && lines(n->body) > 32) { c->rc.status = INK_RC_INVALID; snprintf(at, sizeof at, "%s", TR("清单最多 32 项")); }
        if (c->rc.status == INK_RC_DONE && !save && !is_voice) {
            if (renew) remind(c, n, (unsigned)index, true);
            for (unsigned i = 0; i < 32; i++) remind(c, n, i, false);
        }
        if (c->rc.status == INK_RC_DONE && !(save && known) && (store(db, (uint32_t)id, n, c->src, &c->rc.revision) != 0 ||
            ink_db_wait(db, c->rc.revision, (int64_t)ink_cap_time_left_ms(c) * 1000) != 0)) c->rc.status = INK_RC_FAILED;
        else if (c->rc.status == INK_RC_DONE && !strcmp(op, "clear_done")) U.id = (uint32_t)id;
    }
    snprintf(c->rc.object, sizeof c->rc.object, "memo:%u", (unsigned)id);
    snprintf(c->rc.say, sizeof c->rc.say, "%s", c->rc.status == INK_RC_DONE ? TR("已记下") : at[0] && !strcmp(op, "add_item") ? at : TR("这条没存上，字还在这里。请腾出空间后重试"));
    if (c->rc.status == INK_RC_DONE) ink_exec_post(INK_EXEC_UI, &M.refresh, refresh, (void *)(uintptr_t)id);
    if (c->rc.status == INK_RC_DONE && fresh) { ink_exec_post(INK_EXEC_UI, &M.recorded, after_record, (void *)(uintptr_t)id); sweep(NULL); }
    ink_app_free(raw); ink_app_free(n); ink_db_close(db);
}
static void search(ink_call_t *c) { /* BG: the member's memos holding the folded query, newest change first */
    ink_db_t *db = ink_db_open_member(&MEMOS, ink_cap_call_member(c)); memo_t *n = ink_app_alloc(sizeof *n, true);
    char *out = ink_app_alloc(2048, true); ink_db_cursor_t cur = {.desc = true}; ink_rec_t rec; ink_jw_t w;
    S.count = 0; ink_args_get_str(&c->args, "query", S.query, sizeof S.query); S.on = S.query[0] != 0;
    snprintf(S.member, sizeof S.member, "%s", ink_cap_call_member(c));
    if (out) { ink_jw_init(&w, out, 2048); ink_jw_raw(&w, "["); }
    while (db && n && S.on && S.count < MEMOS.max_records && ink_db_next(db, &cur, &rec, NULL, 0) == 1) {
        int count = read_note(db, rec.id, n) && !n->deleted ? hits(n->body, S.query, NULL, 0) : 0;
        if (!count) continue;
        S.ids[S.count++] = rec.id;
        if (out) { ink_jw_raw(&w, S.count > 1 ? ",{\"id\":" : "{\"id\":"); ink_jw_int(&w, rec.id); ink_jw_raw(&w, ",\"title\":");
            ink_jw_strn(&w, n->body, LV_MIN(strcspn(n->body, "\n"), ink_text_byte_offset(n->body, 24))); ink_jw_raw(&w, ",\"hits\":"); ink_jw_int(&w, count); ink_jw_raw(&w, "}"); }
    }
    if (out) { ink_jw_raw(&w, "]"); if (!w.overflow) ink_cap_result(c, out); }
    if (!db) c->rc.status = INK_RC_FAILED;
    if (S.count) snprintf(c->rc.say, sizeof c->rc.say, TR("找到 %u 条"), (unsigned)S.count);
    else snprintf(c->rc.say, sizeof c->rc.say, "%s", S.on ? TR("没有找到") : TR("已清除搜索"));
    ink_exec_post(INK_EXEC_UI, &M.refresh, refresh, NULL);
    ink_app_free(out); ink_app_free(n); ink_db_close(db);
}
static void save_failed(bool failed) {
    M.mode = failed ? FAIL : EDIT;
    if (failed) lv_obj_remove_flag(M.retry_button, LV_OBJ_FLAG_HIDDEN); else lv_obj_add_flag(M.retry_button, LV_OBJ_FLAG_HIDDEN);
    if (failed) lv_obj_add_flag(M.delete_button, LV_OBJ_FLAG_HIDDEN); else lv_obj_remove_flag(M.delete_button, LV_OBJ_FLAG_HIDDEN);
}
static void reply(void *ctx, const ink_receipt_t *rc) {
    if ((uint32_t)(uintptr_t)ctx != ink_ui_generation() || !ink_ui_page_current() || strcmp(M.member, ink_member_current()->id)) return;
    M.saving = false;
    if (rc->status != INK_RC_DONE && M.field && M.mode == EDIT) save_failed(true);
    else if (rc->status == INK_RC_DONE && M.field && M.mode == FAIL) save_failed(false);
    if (M.notice && !M.field) lv_label_set_text(M.notice, rc->status == INK_RC_DONE && M.adder ? "" : rc->say);
    if (M.field) settle();
    if (rc->status != INK_RC_DONE && M.adder && M.sent[0]) ink_text_commit_utf8(M.sent); /* not added (32 items): the words come back */
    if (M.adder) M.sent[0] = 0;
    else if (rc->status == INK_RC_DONE && !M.field && M.mode != CHECKLIST && M.mode != ITEM && M.mode != DUE) ink_ui_redraw();
    ink_ui_content_changed();
}
static void request(const char *cap, const ink_args_t *args) {
    ink_call_opts_t opts = {.src = INK_SRC_TOUCH, .member = M.member, .reply_exec = INK_EXEC_UI, .done = reply, .ctx = (void *)(uintptr_t)ink_ui_generation()};
    if (ink_cap_call(cap, args, &opts) != 0) { M.saving = false; if (M.notice) lv_label_set_text(M.notice, TR("这条没存上，字还在这里。请重试")); }
}
static void navigate(ink_call_t *c) {
    const char *cap = c->cap->id + 5; int64_t id = 0, index = -1; bool due = false, list = false; char query[64] = "";
    ink_args_get_int(&c->args, "id", &id); ink_args_get_int(&c->args, "index", &index); ink_args_get_bool(&c->args, "due", &due); ink_args_get_bool(&c->args, "checklist", &list);
    bool asked = ink_args_get_str(&c->args, "query", query, sizeof query) > 0, find = !strcmp(cap, "find");
    if (!strcmp(cap, "next") || !strcmp(cap, "prev")) { if (M.list) ink_ui_list_turn(M.list, !strcmp(cap, "next") ? 1 : -1); }
    else if (!strcmp(cap, "retry")) { M.retry = true; if (!M.field && ink_ui_page_current()) ink_ui_redraw(); } /* the list: kept words again */
    else if (!strcmp(cap, "show_done")) { M.expanded = !M.expanded; if (M.mode == CHECKLIST) check_refresh(); }
    else if (!strcmp(cap, "find_close")) { F.open = false; F.query[0] = F.label[0] = 0; if (M.mode == DETAIL) ink_ui_redraw(); }
    else if (!strncmp(cap, "find_", 5)) {
        if (!F.open || !F.count) c->rc.status = INK_RC_INVALID;
        else { F.cur = (F.cur + (!strcmp(cap, "find_next") ? 1 : F.count - 1)) % F.count; ink_exec_post(INK_EXEC_UI, &M.found, find_now, NULL); }
    } else {
        int mode = !strcmp(cap, "edit") ? EDIT : DETAIL;
        memo_t *n = ink_app_alloc(sizeof *n, true); ink_db_t *db = ink_db_open_cached_member(&MEMOS, ink_cap_call_member(c));
        if (id && (!db || !n || !read_note(db, (uint32_t)id, n) || n->deleted || (find && (n->checklist || n->pending)))) c->rc.status = INK_RC_INVALID;
        else if (n) {
            if (mode == DETAIL && n->checklist) mode = index >= 0 ? (due ? DUE : ITEM) : CHECKLIST;
            else if (id && n->pending) mode = PENDING; /* its words would cover what is typed */
            else if (mode == DETAIL && due) mode = DUE; /* a text memo opens to read: the editor's keyboard leaves it a line */
            else if (!id && list) mode = CHECKLIST;
            M.index = index < 0 ? 0 : (int)index; M.jump = -1; M.adding = !id && list;
            if (asked && n->checklist) for (unsigned i = 0; i < lines(n->body) && M.jump < 0; i++) { const char *p = line_at(n->body, i); size_t h = 0; if (hits(p, query, &h, 1) && h < strcspn(p, "\n")) M.jump = (int)i; }
            if (mode == DETAIL && (find || asked)) { F.open = true; F.cur = F.count = 0; F.label[0] = 0; snprintf(F.query, sizeof F.query, "%s", query); }
            if ((mode == DETAIL || mode == EDIT) && M.mode == mode && M.id == (uint32_t)id && ink_ui_page_current()) ink_ui_redraw();
            else {
                if (!id && db) M.fresh = (uint32_t)(id = ink_db_new_id(db));
                if (mode != ITEM && mode != DUE) memset(&M.position, 0, sizeof M.position);
                ink_ui_push(&PAGES[mode], (void *)(uintptr_t)id);
            }
        }
        ink_app_free(n); ink_db_close(db);
    }
}
static void record_reply(void *ctx, const ink_receipt_t *rc) { /* UI: the microphone is on, or why not */
    ink_call_t *c = ctx; c->rc = *rc; c->rc.result = NULL;
    if (rc->status == INK_RC_DONE && ink_ui_page_current()) ink_ui_push(&PAGES[RECORD], NULL);
    else if (M.notice && rc->status != INK_RC_PENDING_CONFIRMATION) lv_label_set_text(M.notice, rc->say[0] ? rc->say : TR("现在录不了，请稍后再试"));
    ink_cap_done(c);
}
static void record(ink_call_t *c) { /* UI: a recording in the member's memo folder, asked for by whoever asked for this */
    if (c->src == INK_SRC_TOUCH && ink_memo_voice_guarded()) { c->rc.status = INK_RC_STALE; return; } /* a second tap on 记好了 */
    ink_tm_t t; ink_time_to_local(ink_time_now_s(), &t); char path[64]; ink_args_t a; ink_args_init(&a);
    snprintf(path, sizeof path, VOICE_DIR "/%04d%02d%02d-%02d%02d%02d.opus", t.year, t.month, t.day, t.hour, t.min, t.sec); ink_args_str(&a, "path", path);
    ink_call_opts_t o = {.src = c->src, .member = ink_cap_call_member(c), .reply_exec = INK_EXEC_UI, .done = record_reply, .ctx = c};
    ink_cap_defer(c);
    if (ink_cap_call("audio.record", &a, &o) != 0) { c->rc.status = INK_RC_FAILED; ink_cap_done(c); }
}
static void play_reply(void *ctx, const ink_receipt_t *rc) { ink_call_t *c = ctx; c->rc = *rc; c->rc.result = NULL; ink_cap_done(c); }
static void play(ink_call_t *c) { /* BG: by the file's number, which a rename or a move keeps */
    int64_t id = 0; ink_args_get_int(&c->args, "id", &id); ink_fs_entry_t e = {0}; const char *who = ink_cap_call_member(c);
    ink_db_t *db = ink_db_open_member(&MEMOS, who); memo_t *n = ink_app_alloc(sizeof *n, true);
    if (!db) { c->rc.status = INK_RC_FAILED; snprintf(c->rc.say, sizeof c->rc.say, TR("暂时读不了备忘，请稍后再试")); }
    else if (!n || !read_note(db, (uint32_t)id, n) || n->deleted || !n->path[0]) c->rc.status = INK_RC_INVALID;
    else {
        ink_args_t a; ink_args_init(&a); ink_args_str(&a, "path", ink_fs_lookup(n->audio, &e) == 0 ? e.path : n->path); ink_args_int(&a, "start_ms", 0);
        ink_call_opts_t o = {.src = c->src, .member = ink_cap_call_member(c), .reply_exec = INK_EXEC_BG, .done = play_reply, .ctx = c};
        ink_cap_defer(c);
        if (ink_cap_call("media.play", &a, &o) != 0) { c->rc.status = INK_RC_FAILED; ink_cap_done(c); }
    }
    ink_app_free(n); ink_db_close(db);
}
static void save_current(void) {
    ink_args_t a; ink_args_init(&a); ink_args_int(&a, "id", M.id); ink_args_int(&a, "draft", M.note.draft); ink_args_str(&a, "page", M.note.page);
    snprintf(M.sent, sizeof M.sent, "%s", ink_textfield_text(M.field)); M.retry = false; M.saving = true; request("memo.save", &a);
}
/* UI: the editor's words into the opener's memo, not through the bus (another member may be here already). True: none lost. */
static bool put_text(uint32_t id, const char *text, uint32_t draft, const char *page) {
    memo_t *n = ink_app_alloc(sizeof *n, true); char *old = n ? ink_app_alloc(sizeof n->body, true) : NULL;
    bool known = old && read_note(M.db, id, n), kept = old && (n->deleted || (!known && !text[0]));
    if (old && !kept) {
        memcpy(old, n->body, sizeof n->body); snprintf(n->body, sizeof n->body, "%s", text);
        if (n->checklist) n->checked = remap_checks(old, n->body, n->checked, n);
        n->draft = draft; snprintf(n->page, sizeof n->page, "%s", page);
        kept = store(M.db, id, n, INK_SRC_TOUCH, NULL) == 0;
    }
    ink_app_free(old); ink_app_free(n); return kept;
}
/* UI: leaving, locking, another member, or a draft that fell behind. After a failed save (force) it is tried again all the same. */
static bool flush_note(bool force) {
    const char *text = M.field ? ink_textfield_text(M.field) : "";
    if (!M.field || !M.db || (!force && !strcmp(text, M.sent))) return true;
    bool kept = put_text(M.id, text, M.note.draft, M.note.page);
    if (kept) snprintf(M.sent, sizeof M.sent, "%s", text);
    return kept;
}
static void settle(void) {
    if (!M.field || !ink_ui_page_current()) return;
    ink_textfield_long_refresh(M.field);
    const char *text = ink_textfield_text(M.field);
    int saved = ink_text_draft_saved();
    const char *status = M.mode == FAIL ? (saved > 0 ? TR("草稿已存，备忘没存上，请重试") :
        saved < 0 ? TR("草稿没存上，字还在，请重试") : TR("正在存草稿，备忘没存上")) :
        ink_crc32(0, text, strlen(text)) == M.opened_crc && !M.retry ? TR("备忘") :
        M.saving || strcmp(text, M.sent) ? TR("正在保存备忘") : TR("已记下");
    lv_area_t keyboard; ink_shell_keyboard_area(&keyboard);
    bool changed = strcmp(lv_label_get_text(M.notice), status) != 0, body = ink_text_draft_id() != 0;
    if (changed) lv_label_set_text(M.notice, status);
    if (changed || M.keyboard_top != keyboard.y1) {
        M.keyboard_top = keyboard.y1;
        ink_textfield_style(M.field, INK_FIELD_BODY, status);
    }
    for (int i = 0; i < 2 && body && M.tools[i]; i++) { /* 撤销 and 重做 only while there is something to undo or redo */
        bool can = i == 0 ? ink_text_can_undo() : ink_text_can_redo();
        if (lv_obj_has_flag(M.tools[i], LV_OBJ_FLAG_HIDDEN) == can) lv_obj_set_flag(M.tools[i], LV_OBJ_FLAG_HIDDEN, !can);
    }
    if (M.saving || ink_power_state() == INK_PWR_LOCKED) return; /* locked: the lock's own flush has it (background) */
    if (!M.retry && (!strcmp(text, M.sent) || M.mode == FAIL)) return;
    if (saved < 0) { M.retry = false; save_failed(!flush_note(true)); return; } /* memo.save reads the draft, which did not take the last keys */
    save_current();
}
static void retry(lv_event_t *e) { (void)e; ink_args_t a; ink_args_init(&a); request("memo.retry", &a); }
static void searched(void *ctx, const char *value) { /* the 搜索 key: the same memo.search the AI calls */
    (void)ctx; bool find = F.open && M.mode == DETAIL; ink_args_t a; ink_args_init(&a);
    if (find) ink_args_int(&a, "id", M.id);
    ink_args_str(&a, "query", value ? value : "");
    ink_call_opts_t o = {.src = INK_SRC_TOUCH, .member = M.member, .reply_exec = INK_EXEC_UI};
    ink_cap_call(find ? "memo.find" : "memo.search", &a, &o);
}
static void clear_search(lv_event_t *e) { (void)e; searched(NULL, ""); }
static void item_entered(void *ctx, const char *value) {
    (void)ctx; ink_args_t a; ink_args_init(&a); ink_args_int(&a, "id", M.id); ink_args_int(&a, "index", M.index);
    ink_args_str(&a, "text", value ? value : ""); request("memo.edit_item", &a);
}
static void cleared(void *arg) { (void)arg; if (M.adder) { ink_text_select(0, 1u << 20); ink_text_commit_utf8(""); } }
static void add_entered(void *ctx, const char *value) { /* the field empties at once for the next item, as Fossify's dialog does */
    (void)ctx; ink_args_t a; ink_args_init(&a); ink_args_int(&a, "id", M.id); ink_args_str(&a, "text", value ? value : "");
    snprintf(M.sent, sizeof M.sent, "%s", value ? value : ""); request("memo.add_item", &a); ink_exec_post(INK_EXEC_UI, &M.found, cleared, NULL);
}
static void ui_toggle(lv_event_t *e) { (void)e; M.adding = !M.adding; ink_ui_redraw(); } /* 加一项 and 完成: the field only, no data */
static int fit(const char *text) { return LV_MAX(INK_CONTROL, ink_ui_text_width(text, INK_FONT_LIST, INK_WEIGHT_MEDIUM) + 2 * INK_GAP); }
static lv_obj_t *button(lv_obj_t *parent, const char *text, int x, int y, int w, bool primary, const char *cap, const char *args) {
    lv_obj_t *b = ink_ui_pill(parent, text, x, y, w, INK_CONTROL, primary, NULL, NULL);
    if (cap) ink_shell_bind(b, cap, args);
    return b;
}
static void bind_note(lv_obj_t *slot, size_t index, ink_ui_field_spec_t *field, ink_ui_action_spec_t *action, void *ctx) {
    (void)ctx; unsigned at = index % 4; uint32_t id = M.rows[index]; memo_t *n = ink_app_alloc(sizeof *n, true);
    char preview[160] = ""; size_t hit = 0; int w = INK_CANVAS_W - 2 * INK_MARGIN - 2 * INK_GAP, y = INK_GAP_S + ink_ui_line_height(INK_FONT_LIST);
    if (!n || !read_note(M.db, id, n)) { ink_app_free(n); return; }
    bool waiting = n->pending && !n->body[0]; /* a recording waiting for its words: what it is and when they come */
    const char *body = waiting ? TR("录音已保存") : n->body, *rest = waiting ? TR("连上网后整理成文字") : line_at(body, 1);
    int count = S.on ? hits(n->body, S.query, &hit, 1) : 0;
    unsigned all = lines(n->body), open = 0;
    for (unsigned i = 0; i < all && n->checklist; i++) open += !done_at(n, i);
    snprintf(M.labels[at], sizeof M.labels[at], "%.*s", (int)strcspn(body, "\n"), body);
    if (count) { /* Joplin's result line: a few characters around the first hit, and how many */
        size_t from = ink_text_char_offset(n->body, hit), skip = ink_text_byte_offset(n->body, from > 6 ? from - 6 : 0);
        snprintf(preview, sizeof preview, "…%.*s…", (int)ink_text_byte_offset(n->body + skip, 16), n->body + skip);
        snprintf(M.meta[at], sizeof M.meta[at], TR("共 %d 处"), count);
    } else if (n->checklist) snprintf(M.meta[at], sizeof M.meta[at], TR("%u/%u 已完成"), all - open, all);
    else if (n->due[0]) due_text(M.meta[at], sizeof M.meta[at], n, 0);
    else if (n->updated) when(M.meta[at], sizeof M.meta[at], n->updated);
    else M.meta[at][0] = 0;
    if (!count) snprintf(preview, sizeof preview, "%.*s", (int)strcspn(rest, "\n"), rest);
    for (char *p = preview; *p; p++) if (*p == '\n') *p = ' ';
    size_t len = strlen(M.meta[at]); /* the second line: when, how far or how many hits, then how it goes on */
    if (preview[0]) snprintf(M.meta[at] + len, sizeof M.meta[at] - len, "%s%.60s", len ? " · " : "", preview);
    if (!lv_obj_get_child_count(slot)) {
        ink_ui_line(slot, "", INK_FONT_LIST, INK_WEIGHT_MEDIUM, INK_GAP, INK_GAP_S, w);
        ink_ui_line(slot, "", INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_GAP, y, w);
    }
    lv_label_set_text(lv_obj_get_child(slot, 0), M.labels[at]); lv_label_set_text(lv_obj_get_child(slot, 1), M.meta[at]);
    ink_jw_t w2; ink_jw_init(&w2, M.args[at], sizeof M.args[at]); ink_jw_raw(&w2, "{\"id\":"); ink_jw_int(&w2, id);
    if (count) { ink_jw_raw(&w2, ",\"query\":"); ink_jw_str(&w2, S.query); }
    ink_jw_raw(&w2, "}");
    if (w2.overflow) snprintf(M.args[at], sizeof M.args[at], "{\"id\":%u}", (unsigned)id);
    snprintf(M.ids[at], sizeof M.ids[at], "memo:%u:0", (unsigned)id);
    *field = (ink_ui_field_spec_t){.id = M.ids[at], .role = "listitem", .label = M.labels[at], .value = M.meta[at], .object = {M.ids[at], ink_db_last_ver(M.db)}};
    *action = (ink_ui_action_spec_t){.id = M.ids[at], .label = TR("查看备忘"), .capability = "memo.open", .args_json = M.args[at], .object = field->object, .enabled = true};
    ink_app_free(n);
}
static void bind_check(lv_obj_t *slot, size_t index, ink_ui_field_spec_t *field, ink_ui_action_spec_t *action, void *ctx) {
    (void)ctx; unsigned at = index % 4, k = M.order[index], done = 0, total = lines(M.note.body);
    int h = LV_MAX(2 * ink_ui_line_height(INK_FONT_LIST) + 2 * INK_GAP_S, INK_CONTROL), w = INK_CANVAS_W - 2 * INK_MARGIN, text_w = w - 2 * INK_CONTROL - INK_GAP;
    for (unsigned i = 0; i < total; i++) done += done_at(&M.note, i);
    lv_obj_clean(slot);
    snprintf(M.ids[at], sizeof M.ids[at], "memo:%u:%u", (unsigned)M.id, k);
    if (k == 32) { /* Fossify's collapsible header of the done items */
        char count[64]; snprintf(count, sizeof count, TR("已完成 %u 项"), done); /* not from M into M: gcc -Wrestrict */
        snprintf(M.labels[at], sizeof M.labels[at], "%s", count);
        snprintf(M.meta[at], sizeof M.meta[at], "%s · %s", count, M.expanded || done == total ? TR("收起") : TR("展开"));
        ink_ui_line(slot, M.meta[at], INK_FONT_LIST, INK_WEIGHT_MEDIUM, INK_GAP, (h - ink_ui_line_height(INK_FONT_LIST)) / 2, w - 2 * INK_GAP);
        snprintf(M.args[at], sizeof M.args[at], "{\"id\":%u}", (unsigned)M.id);
        *field = (ink_ui_field_spec_t){.id = M.ids[at], .role = "button", .label = M.labels[at], .value = M.expanded || done == total ? TR("收起") : TR("展开"), .object = {M.ids[at], ink_db_last_ver(M.db)}};
        *action = (ink_ui_action_spec_t){.id = M.ids[at], .label = M.meta[at], .capability = "memo.show_done", .args_json = M.args[at], .object = field->object, .enabled = true};
        return;
    }
    const char *text = line_at(M.note.body, k); bool checked = done_at(&M.note, k);
    char due[96]; due_text(due, sizeof due, &M.note, k);
    snprintf(M.labels[at], sizeof M.labels[at], "%.*s", (int)strcspn(text, "\n"), text);
    lv_obj_t *box = ink_ui_box(slot, INK_GAP, (h - INK_ICON_LIST) / 2, INK_ICON_LIST, INK_ICON_LIST, INK_RADIUS_KEY, checked ? INK_SURFACE_BLACK : INK_SURFACE_WHITE, true);
    if (checked) ink_ui_icon(box, "check", INK_ICON_MEDIUM, INK_ICON_LINE, INK_GAP_XS, INK_GAP_XS, true);
    lv_obj_t *label = ink_ui_label(slot, M.labels[at], INK_FONT_LIST, INK_WEIGHT_REGULAR, INK_CONTROL, INK_GAP_S, text_w);
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT); /* two lines at most; MemoItem shows it all */
    lv_obj_set_height(label, ink_ui_px((due[0] ? 1 : 2) * ink_ui_line_height(INK_FONT_LIST)));
    if (due[0]) ink_ui_line(slot, due, INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_CONTROL, INK_GAP_S + ink_ui_line_height(INK_FONT_LIST), text_w);
    snprintf(M.args2[at], sizeof M.args2[at], "{\"id\":%u,\"index\":%u}", (unsigned)M.id, k);
    button(slot, TR("改"), w - INK_CONTROL, (h - INK_CONTROL) / 2, INK_CONTROL, false, "memo.open", M.args2[at]);
    snprintf(M.meta[at], sizeof M.meta[at], "%s%s%.80s", checked ? TR("已完成") : TR("未完成"), due[0] ? " · " : "", due);
    *field = (ink_ui_field_spec_t){.id = M.ids[at], .role = "checkbox", .label = M.labels[at], .value = M.meta[at], .object = {M.ids[at], ink_db_last_ver(M.db)}};
    *action = (ink_ui_action_spec_t){.id = M.ids[at], .label = TR("勾选或取消"), .capability = "memo.check", .args_json = M.args2[at], .object = field->object, .enabled = true};
}
static void bind_text(lv_obj_t *slot, size_t index, ink_ui_field_spec_t *field, ink_ui_action_spec_t *action, void *ctx) { /* one line; the hit looked at inverted */
    (void)ctx; unsigned at = index % 16; size_t from = R.at[index], len = line_len(M.note.body + from), hit = F.count ? F.at[F.cur] : 0, end = hit + strlen(F.query);
    const char *text = M.note.body + from; bool on = F.open && F.count && hit < from + len && end > from;
    snprintf(R.text[at], sizeof R.text[at], "%.*s", (int)len, text);
    snprintf(R.ids[at], sizeof R.ids[at], "memo:%u:%zu", (unsigned)M.id, index);
    if (!lv_obj_get_child_count(slot)) {
        lv_obj_t *line = ink_ui_line(slot, "", INK_FONT_LIST, INK_WEIGHT_REGULAR, INK_GAP, 0, INK_CANVAS_W - 2 * INK_MARGIN - 2 * INK_GAP);
        lv_obj_set_style_bg_color(line, ink_ui_ink(), LV_PART_SELECTED); lv_obj_set_style_text_color(line, ink_ui_paper(), LV_PART_SELECTED);
    }
    lv_obj_t *line = lv_obj_get_child(slot, 0); lv_label_set_text(line, R.text[at]);
    lv_label_set_text_selection_start(line, on ? (uint32_t)ink_text_char_offset(text, hit > from ? hit - from : 0) : LV_LABEL_TEXT_SELECTION_OFF);
    lv_label_set_text_selection_end(line, on ? (uint32_t)ink_text_char_offset(text, LV_MIN(end, from + len) - from) : LV_LABEL_TEXT_SELECTION_OFF);
    *field = (ink_ui_field_spec_t){.id = R.ids[at], .role = "listitem", .label = R.text[at], .value = on ? F.label : "", .object = {R.ids[at], ink_db_last_ver(M.db)}};
    *action = (ink_ui_action_spec_t){.id = R.ids[at], .label = TR("编辑备忘"), .capability = "memo.edit", .args_json = M.target, .object = field->object};
}
static size_t check_order(void) { /* open items first, then the 已完成 row; the done ones while expanded or when all are done */
    unsigned total = lines(M.note.body), n = 0, done = 0;
    for (unsigned i = 0; i < total; i++) if (!done_at(&M.note, i)) M.order[n++] = (uint8_t)i;
    done = total - n;
    if (done) M.order[n++] = 32;
    for (unsigned i = 0; done && (M.expanded || done == total) && i < total; i++) if (done_at(&M.note, i)) M.order[n++] = (uint8_t)i;
    return n;
}
static void check_refresh(void) { /* UI: a check or a new item redraws the list area only */
    unsigned total = lines(M.note.body), done = 0;
    for (unsigned i = 0; i < total; i++) done += done_at(&M.note, i);
    bool row = M.expanded && done, undo = U.id == M.id && !strcmp(U.member, M.member);
    if (!M.list || !M.done_row != !row || !M.undo_row != !undo) { ink_ui_redraw(); return; }
    M.total = check_order(); ink_ui_list_refresh(M.list, M.total);
}
static void bind_part(lv_obj_t *b, int part, int delta) { /* the shared date picker: the line before the − button is its value */
    if (delta < 0) M.part_labels[part] = lv_obj_get_child(lv_obj_get_parent(b), (int32_t)lv_obj_get_index(b) - 1);
    lv_obj_add_event_cb(b, adjust, LV_EVENT_CLICKED, (void *)(intptr_t)(part * 2 + (delta > 0)));
}
static int month_days(int y, int m) { return m == 2 ? 28 + (y % 4 == 0 && (y % 100 || y % 400 == 0)) : 30 + ((m + (m > 7)) & 1); }
static void adjust(lv_event_t *e) { /* UI: picking a value is not an action; 设好了 is (memo.set_due) */
    static const char *const NAMES[] = {TR_NOOP("年"), TR_NOOP("月"), TR_NOOP("日"), TR_NOOP("时"), TR_NOOP("分")};
    int code = (int)(intptr_t)lv_event_get_user_data(e), part = code / 2, lo[5] = {2000, 1, 1, 0, 0}, hi[5] = {2100, 12, 31, 23, 59};
    hi[2] = month_days(M.parts[0], M.parts[1]);
    M.parts[part] += code & 1 ? 1 : -1;
    if (M.parts[part] < lo[part]) M.parts[part] = hi[part];
    if (M.parts[part] > hi[part]) M.parts[part] = lo[part];
    for (int i = 0; i < 5 && M.part_labels[i]; i++) {
        if (i == 2 && M.parts[2] > month_days(M.parts[0], M.parts[1])) M.parts[2] = month_days(M.parts[0], M.parts[1]);
        lv_label_set_text_fmt(M.part_labels[i], "%s %02d", TR(NAMES[i]), M.parts[i]);
    }
}
static void set_due(lv_event_t *e) { /* 设好了 / 清除到期 */
    char at[32] = ""; ink_args_t a; ink_args_init(&a);
    if (lv_event_get_user_data(e)) snprintf(at, sizeof at, "%04d-%02d-%02d %02d:%02d", M.parts[0] % 10000, M.parts[1] % 100, M.parts[2] % 100, M.parts[3] % 100, M.parts[4] % 100);
    ink_args_int(&a, "id", M.id); ink_args_int(&a, "index", M.index); ink_args_str(&a, "at", at); request("memo.set_due", &a);
}
static void action(int i, const char *id, const char *label, const char *cap, const char *args, bool enabled) {
    M.actions[i] = (ink_ui_action_spec_t){.id = id, .label = label, .capability = cap, .args_json = args, .enabled = enabled};
    if (M.model.action_count <= (size_t)i) M.model.action_count = (size_t)i + 1;
}
static ink_ui_list_t *rows(lv_obj_t *screen, int top, int bottom, int cell, ink_ui_list_bind_fn bind, const char *empty, int max) {
    const ink_ui_action_spec_t PAGING[] = {{.id = "prev", .label = TR("上一页"), .capability = "memo.prev"}, {.id = "next", .label = TR("下一页"), .capability = "memo.next"}};
    int count = LV_MAX(1, LV_MIN(max, (bottom - top - INK_CONTROL - INK_GAP) / (cell + INK_GAP_M)));
    M.cap = (unsigned)count;
    ink_ui_page_spec_t model = {.id = ink_ui_page_name(), .title = TR("备忘"), .object = {"memo.notes", ink_db_last_ver(M.db)}, .actions = PAGING, .action_count = 2};
    lv_obj_t *body = ink_ui_box(screen, 0, top, INK_CANVAS_W, bottom - top, 0, INK_SURFACE_WHITE, false);
    return ink_ui_list_create(body, &model, M.mode == LIST || M.mode == UNDO ? &M.listed : &M.position, (unsigned)count, 1, cell, empty, bind, NULL);
}
static void resume_record(void *arg) { (void)arg; if (!strcmp(ink_ui_page_name(), "Memo") && ink_ui_page_current()) ink_ui_push(&PAGES[RECORD], NULL); }
static void list_page(lv_obj_t *screen) { /* Joplin's list with its search box; Fossify's new note; our undo row */
    uint32_t undo = 0, undo_ver = 0; ink_db_cursor_t cur = {.desc = true}; ink_rec_t rec; memo_t *n = ink_app_alloc(sizeof *n, true);
    int top = INK_STATUS_BAR + INK_PRIMARY, bottom = INK_CANVAS_H - INK_MARGIN - INK_PRIMARY - INK_GAP_M;
    int list_w = fit(TR("新清单")), voice_w = fit(TR("说一句")), w = INK_CANVAS_W - 2 * INK_MARGIN - 2 * INK_GAP - list_w - voice_w;
    bool stacked = w < fit(TR("写一条")); /* a longer language or text size: 写一条 gets its own row, nothing is cut */
    if (stacked) { bottom -= INK_PRIMARY + INK_GAP_M; w = INK_CANVAS_W - 2 * INK_MARGIN; list_w = voice_w = (w - INK_GAP) / 2; }
    M.total = 0;
    while (n && !S.on && M.total < MEMOS.max_records && ink_db_next(M.db, &cur, &rec, NULL, 0) == 1) if (read_note(M.db, rec.id, n)) {
        if (!n->deleted) M.rows[M.total++] = rec.id;
        else if (rec.ver > undo_ver) { undo = rec.id; undo_ver = rec.ver; }
    }
    ink_app_free(n);
    if (S.on) { memcpy(M.rows, S.ids, S.count * sizeof *S.ids); M.total = S.count; }
    ink_text_cfg_t cfg = {.type = INK_TEXT_PLAIN, .enter = INK_ENTER_SEARCH, .initial = S.query, .hint = TR("搜索备忘"), .max = sizeof S.query - 1};
    lv_obj_t *box = ink_textfield_create(screen, &cfg, searched, NULL, false);
    lv_obj_set_y(box, ink_ui_px(top)); ink_textfield_style(box, INK_FIELD_SEARCH, TR("搜索"));
    top += ink_textfield_height(INK_FIELD_SEARCH) + INK_GAP_M;
    if (K.id && !strcmp(K.member, M.member)) { /* words an editor could not store are still here */
        bottom -= INK_CONTROL + INK_GAP_M; action((int)M.model.action_count, "retry", TR("有一条备忘没存上 · 重试"), "memo.retry", NULL, true);
        button(screen, TR("有一条备忘没存上 · 重试"), INK_MARGIN, bottom, INK_CANVAS_W - 2 * INK_MARGIN, false, "memo.retry", NULL);
    }
    if (undo) {
        snprintf(M.target, sizeof M.target, "{\"id\":%u}", (unsigned)undo);
        action((int)M.model.action_count, "undo", TR("撤销删除"), "memo.undo", M.target, true);
        bottom -= INK_CONTROL + INK_GAP_M;
        button(screen, TR("已删除一条备忘 · 撤销"), INK_MARGIN, bottom, INK_CANVAS_W - 2 * INK_MARGIN, false, "memo.undo", M.target);
    }
    int cell = ink_ui_line_height(INK_FONT_LIST) + ink_ui_line_height(INK_FONT_NOTE) + 2 * INK_GAP_S;
    M.list = rows(screen, top, bottom, cell, bind_note, S.on ? "" : TR("还没有备忘，写一条吧"), 4);
    if (M.list) ink_ui_list_refresh(M.list, M.total);
    if (S.on && !M.total) ink_ui_empty(screen, TR("没有找到"), TR("清除搜索"), clear_search, NULL);
    int y = INK_CANVAS_H - INK_MARGIN - INK_PRIMARY;
    button(screen, TR("写一条"), INK_MARGIN, stacked ? y - INK_PRIMARY - INK_GAP_M : y, w, true, "memo.edit", "{}");
    button(screen, TR("新清单"), stacked ? INK_MARGIN : INK_MARGIN + w + INK_GAP, y, list_w, false, "memo.edit", "{\"checklist\":true}");
    button(screen, TR("说一句"), INK_CANVAS_W - INK_MARGIN - voice_w, y, voice_w, false, "memo.record", NULL);
    action((int)M.model.action_count, "new", TR("写一条"), "memo.edit", NULL, true);
    action((int)M.model.action_count, "checklist", TR("新清单"), "memo.edit", "{\"checklist\":true}", true);
    action((int)M.model.action_count, "record", TR("说一句"), "memo.record", NULL, true);
    action((int)M.model.action_count, "search", TR("搜索备忘"), "memo.search", NULL, true);
    ink_exec_post(INK_EXEC_BG, &T.sweep, sweep, NULL);
    if (M.mode == LIST && ink_memo_voice_resume()) ink_exec_post(INK_EXEC_UI, &M.found, resume_record, NULL);
}
static int top_button(lv_obj_t *screen, int x, const char *text, const char *also, const char *cap, const char *args, lv_obj_t **out) { /* right to left */
    int w = LV_MAX(INK_CONTROL, ink_ui_text_width(text, INK_FONT_LIST, INK_WEIGHT_MEDIUM) + 2 * INK_GAP_M);
    if (also) w = LV_MAX(w, ink_ui_text_width(also, INK_FONT_LIST, INK_WEIGHT_MEDIUM) + 2 * INK_GAP_M);
    x -= w; lv_obj_t *b = ink_ui_text_button(screen, text, x, INK_STATUS_BAR, w, NULL, NULL);
    ink_shell_bind(b, cap, args); if (out) *out = b;
    return x;
}
static void edit_page(lv_obj_t *screen) { /* Fossify's TextFragment for writing only: no save button; undo, redo and delete written in the top bar */
    int x = INK_CANVAS_W - INK_MARGIN;
    x = top_button(screen, x, TR("删除"), TR("重试"), "memo.delete", M.target, &M.delete_button);
    int w = LV_MAX(INK_CONTROL, LV_MAX(ink_ui_text_width(TR("删除"), INK_FONT_LIST, INK_WEIGHT_MEDIUM), ink_ui_text_width(TR("重试"), INK_FONT_LIST, INK_WEIGHT_MEDIUM)) + 2 * INK_GAP_M);
    M.retry_button = ink_ui_text_button(screen, TR("重试"), INK_CANVAS_W - INK_MARGIN - w, INK_STATUS_BAR, w, retry, NULL); /* in 删除's place */
    lv_obj_add_flag(M.retry_button, LV_OBJ_FLAG_HIDDEN);
    x = top_button(screen, x, TR("重做"), NULL, "text.redo", "{}", &M.tools[1]); top_button(screen, x, TR("撤销"), NULL, "text.undo", "{}", &M.tools[0]);
    lv_obj_add_flag(M.tools[0], LV_OBJ_FLAG_HIDDEN); lv_obj_add_flag(M.tools[1], LV_OBJ_FLAG_HIDDEN); /* until there is something to undo or redo */
    bool kept = K.id == M.id && !strcmp(K.member, M.member); /* words its page could not store open again here, and are saved again */
    ink_text_cfg_t cfg = {.type = INK_TEXT_PLAIN, .max = 4095, .initial = kept ? K.body : M.note.body, .draft_page = ink_ui_page_name(),
        .draft_id = !kept && !strcmp(M.note.page, ink_ui_page_name()) ? M.note.draft : 0};
    M.field = ink_textfield_long_create(screen, &cfg);
    if (M.field) ink_textfield_style(M.field, INK_FIELD_BODY, TR("备忘"));
    if (M.field && kept) K.id = 0;
    lv_obj_add_flag(M.notice, LV_OBJ_FLAG_HIDDEN);
    for (uint32_t i = 0; M.field && i < lv_obj_get_child_count(M.field); i++) { /* the memo owns the save result; the shared footer keeps its pages */
        lv_obj_t *label = lv_obj_get_child(M.field, (int32_t)i);
        if (lv_obj_check_type(label, &lv_label_class) && lv_obj_get_style_align(label, 0) == LV_ALIGN_BOTTOM_LEFT)
            lv_obj_add_flag(label, LV_OBJ_FLAG_HIDDEN);
    }
    /* A new memo saves at once to get its record; an opened one only when its words change. That first save changes no label. */
    snprintf(M.sent, sizeof M.sent, "%s", M.mode == FAIL || (!M.blank && !kept) ? M.note.body : "\1");
    M.opened_crc = M.mode == FAIL || kept ? 0xffffffffu : ink_crc32(0, ink_textfield_text(M.field), strlen(ink_textfield_text(M.field)));
    M.note.draft = ink_text_draft_id(); snprintf(M.note.page, sizeof M.note.page, "%s", ink_ui_page_name());
}
static int reminder_retry(lv_obj_t *screen, int y, int index, int slot) { /* a reminder that did not take: a written button sets it again */
    if (M.note.due[index] <= ink_time_now_s() || M.note.rid[index] || done_at(&M.note, (unsigned)index)) return y;
    ink_tm_t t; ink_time_to_local(M.note.due[index], &t);
    snprintf(M.args2[5], sizeof M.args2[5], "{\"id\":%u,\"index\":%d,\"at\":\"%04d-%02d-%02d %02d:%02d\"}", (unsigned)M.id, index, t.year % 10000, t.month % 100, t.day % 100, t.hour % 100, t.min % 100);
    button(screen, TR("提醒没设上，点这里重试"), INK_MARGIN, y, INK_CANVAS_W - 2 * INK_MARGIN, false, "memo.set_due", M.args2[5]);
    action(slot, "retry_reminder", TR("提醒没设上，点这里重试"), "memo.set_due", M.args2[5], true);
    return y + INK_CONTROL + INK_GAP_M;
}
static void find_page(void) { /* the hit being looked at: where it is, and the page it is on */
    if (F.count) snprintf(F.label, sizeof F.label, TR("第 %d 处，共 %d 处"), F.cur + 1, F.count);
    else snprintf(F.label, sizeof F.label, "%s", TR("没有找到"));
    size_t row = 0; while (F.count && row + 1 < R.lines && R.at[row + 1] <= F.at[F.cur]) row++;
    M.position.page = row / LV_MAX(M.cap, 1u);
}
static void find_now(void *arg) { /* UI: 上一个 / 下一个: the count line and the lines, one refresh */
    (void)arg; if (M.mode != DETAIL || !M.list || !F.open) return;
    find_page(); lv_label_set_text(M.notice, F.label); ink_ui_list_refresh(M.list, R.lines);
}
static void read_page(lv_obj_t *screen) { /* a memo to read, a page of lines at a time; writing is the editor (编辑备忘), Fossify's find is here */
    const char *names[2][3] = {{TR("到期"), TR("查找"), NULL}, {TR("关闭"), TR("下一个"), TR("上一个")}}, *ids[2][3] = {{"due", "find", NULL}, {"find_close", "find_next", "find_prev"}};
    const char *caps[2][3] = {{"memo.open", "memo.find", NULL}, {"memo.find_close", "memo.find_next", "memo.find_prev"}};
    int x = INK_CANVAS_W - INK_MARGIN, set = F.open, w = (INK_CANVAS_W - 2 * INK_MARGIN - 2 * INK_GAP) / 3, y = INK_CANVAS_H - INK_PRIMARY - INK_MARGIN;
    int top = INK_STATUS_BAR + INK_PRIMARY + INK_GAP_S + (M.note.path[0] ? INK_CONTROL + INK_GAP_M : 0), text_w = INK_CANVAS_W - 2 * INK_MARGIN - 2 * INK_GAP;
    snprintf(M.args2[0], sizeof M.args2[0], "{\"id\":%u,\"due\":true}", (unsigned)M.id);
    for (int i = 0; i < 3 && names[set][i]; i++) {
        x = top_button(screen, x, names[set][i], NULL, caps[set][i], !set && !i ? M.args2[0] : set ? NULL : M.target, NULL);
        action(4 + i, ids[set][i], names[set][i], caps[set][i], !set && !i ? M.args2[0] : set ? NULL : M.target, !set || F.count || i == 0);
    }
    if (F.open && !F.query[0]) { /* the word is typed first; 搜索 shows the lines with the first hit */
        ink_text_cfg_t find = {.type = INK_TEXT_PLAIN, .enter = INK_ENTER_SEARCH, .hint = TR("查找"), .max = sizeof F.query - 1};
        M.find = ink_textfield_create(screen, &find, searched, NULL, false);
        lv_obj_set_y(M.find, ink_ui_px(top)); ink_textfield_style(M.find, INK_FIELD_SEARCH, TR("查找")); ink_textfield_short_focus(M.find);
        return;
    }
    char due[96]; due_text(due, sizeof due, &M.note, 0);
    if (F.open || due[0]) { lv_obj_set_y(M.notice, ink_ui_px(top)); lv_label_set_text(M.notice, F.open ? "" : due); top += ink_ui_line_height(INK_FONT_NOTE) + INK_GAP_M; }
    if (!F.open && !M.note.checklist) top = reminder_retry(screen, top, 0, 6);
    bool stacked = false; /* a larger text: 编辑备忘 gets its own row, nothing is cut */
    for (int i = 0; i < 3; i++) stacked |= ink_ui_text_width(M.actions[i].label, INK_FONT_LIST, INK_WEIGHT_MEDIUM) + 2 * INK_GAP_M > w; /* pad and frame */
    if (stacked) w = (INK_CANVAS_W - 2 * INK_MARGIN - INK_GAP) / 2;
    int ideograph = (int)lv_font_get_glyph_width(ink_ui_font_px(INK_FONT_LIST, INK_WEIGHT_REGULAR), 0x4E00, 0) * INK_CANVAS_W / ink_ui_px(INK_CANVAS_W) + 1; /* the widest character */
    R.cpr = (unsigned)LV_MAX(1, LV_MIN(24, text_w / ideograph)); R.lines = 0;
    for (const char *p = M.note.body; *p && R.lines < 4096;) { R.at[R.lines++] = (uint16_t)(p - M.note.body); p += line_len(p); if (*p == '\n') p++; }
    if (F.open) { F.count = LV_MIN(hits(M.note.body, F.query, F.at, FOUND_MAX), FOUND_MAX); if (F.cur >= F.count) F.cur = 0; }
    M.list = rows(screen, top, y - INK_GAP_M - (stacked ? INK_PRIMARY + INK_GAP_M : 0), ink_ui_line_height(INK_FONT_LIST), bind_text, TR("还没有备忘，写一条吧"), 16);
    if (F.open) { find_page(); lv_label_set_text(M.notice, F.label); }
    if (M.list) ink_ui_list_refresh(M.list, R.lines);
    for (int i = 0; i < 3; i++) {
        int bx = stacked ? (i ? INK_MARGIN + (i - 1) * (w + INK_GAP) : INK_MARGIN) : INK_MARGIN + i * (w + INK_GAP);
        button(screen, M.actions[i].label, bx, stacked && !i ? y - INK_PRIMARY - INK_GAP_M : y, stacked && !i ? INK_CANVAS_W - 2 * INK_MARGIN : w, i == 0, M.actions[i].capability, M.target);
    }
}
static void check_page(lv_obj_t *screen) { /* Fossify's TasksFragment with written buttons in place of long press and dragging */
    int top = INK_STATUS_BAR + INK_PRIMARY + INK_GAP_S, bottom = INK_CANVAS_H - INK_MARGIN - INK_PRIMARY - INK_GAP_M, half = (INK_CANVAS_W - 2 * INK_MARGIN - INK_GAP) / 2;
    unsigned total = lines(M.note.body), done = 0;
    for (unsigned i = 0; i < total; i++) done += done_at(&M.note, i);
    snprintf(M.args2[1], sizeof M.args2[1], "{\"id\":%u}", (unsigned)M.id);
    if (U.id == M.id && !strcmp(U.member, M.member)) {
        M.undo_row = button(screen, TR("已删掉已完成的项 · 撤销"), INK_MARGIN, top, INK_CANVAS_W - 2 * INK_MARGIN, false, "memo.undo", M.args2[1]);
        top += INK_CONTROL + INK_GAP_M; action(3, "undo", TR("撤销"), "memo.undo", M.args2[1], true);
    }
    if (M.expanded && done) {
        bottom -= INK_CONTROL + INK_GAP_M;
        bool stacked = fit(TR("全部取消勾选")) > half; /* the largest text: one per row rather than cut */
        if (stacked) { bottom -= INK_CONTROL + INK_GAP_M; half = INK_CANVAS_W - 2 * INK_MARGIN; }
        M.done_row = button(screen, TR("删掉已完成"), INK_MARGIN, bottom, half, false, "memo.clear_done", M.args2[1]);
        button(screen, TR("全部取消勾选"), stacked ? INK_MARGIN : INK_MARGIN + half + INK_GAP, stacked ? bottom + INK_CONTROL + INK_GAP_M : bottom, half, false, "memo.uncheck_all", M.args2[1]);
        action(4, "clear_done", TR("删掉已完成"), "memo.clear_done", M.args2[1], true);
        action(5, "uncheck_all", TR("全部取消勾选"), "memo.uncheck_all", M.args2[1], true);
    }
    action(6, "show_done", TR("展开"), "memo.show_done", M.args2[1], done > 0);
    action(7, "add_item", TR("加一项"), "memo.add_item", M.args2[1], true);
    M.total = check_order();
    if (!total && !M.adding) ink_ui_empty(screen, TR("清单是空的"), TR("加一项"), ui_toggle, NULL);
    else M.list = rows(screen, top, bottom, LV_MAX(2 * ink_ui_line_height(INK_FONT_LIST) + 2 * INK_GAP_S, INK_CONTROL), bind_check, "", 4);
    for (size_t i = 0; M.jump >= 0 && M.list && i < M.total; i++) if (M.order[i] == M.jump) { M.position.page = i / M.cap; M.jump = -1; }
    if (M.list) ink_ui_list_refresh(M.list, M.total);
    int y = INK_CANVAS_H - INK_MARGIN - INK_PRIMARY;
    if (!M.adding) { if (total) ink_ui_pill(screen, TR("加一项"), INK_MARGIN, y, INK_CANVAS_W - 2 * INK_MARGIN, INK_PRIMARY, true, ui_toggle, NULL); return; }
    ink_text_cfg_t cfg = {.type = INK_TEXT_PLAIN, .enter = INK_ENTER_NEXT, .hint = TR("加一项"), .max = INK_ARGS_TEXT - 40};
    M.adder = ink_textfield_create(screen, &cfg, add_entered, NULL, false);
    lv_obj_set_width(M.adder, ink_ui_px(INK_CANVAS_W - 2 * INK_MARGIN - INK_CONTROL - INK_GAP));
    lv_obj_set_y(M.adder, ink_ui_px(INK_CANVAS_H - INK_MARGIN - ink_textfield_height(INK_FIELD_SINGLE)));
    ink_textfield_style(M.adder, INK_FIELD_SINGLE, TR("加一项"));
    lv_obj_t *finish = ink_ui_pill(screen, TR("完成"), INK_CANVAS_W - INK_MARGIN - INK_CONTROL, y, INK_CONTROL, INK_PRIMARY, false, ui_toggle, NULL);
    ink_textfield_short_focus(M.adder); /* the field rises above the keyboard; 完成 goes up with it */
    lv_area_t field; lv_obj_update_layout(M.adder); lv_obj_get_coords(M.adder, &field);
    lv_obj_set_y(finish, field.y2 + 1 - lv_obj_get_height(finish));
}
static void item_page(lv_obj_t *screen) { /* Fossify's item menu as a page: one thing per button */
    int y = INK_STATUS_BAR + INK_PRIMARY + INK_GAP, half = (INK_CANVAS_W - 2 * INK_MARGIN - INK_GAP) / 2; char due[96];
    const char *text = line_at(M.note.body, (unsigned)M.index); size_t len = strcspn(text, "\n");
    snprintf(M.labels[0], sizeof M.labels[0], "%.*s", (int)len, text);
    ink_text_cfg_t cfg = {.type = INK_TEXT_PLAIN, .enter = INK_ENTER_DONE, .initial = M.labels[0], .hint = TR("这一项"), .max = INK_ARGS_TEXT - 40};
    lv_obj_t *field = ink_textfield_create(screen, &cfg, item_entered, NULL, false);
    lv_obj_set_y(field, ink_ui_px(y)); ink_textfield_style(field, INK_FIELD_SINGLE, TR("这一项"));
    y += ink_textfield_height(INK_FIELD_SINGLE) + INK_GAP_M;
    due_text(due, sizeof due, &M.note, (unsigned)M.index);
    lv_label_set_text(M.notice, due[0] ? due : TR("没有到期")); lv_obj_set_y(M.notice, ink_ui_px(y));
    for (int i = 0; i < 6; i++) snprintf(M.args2[i], sizeof M.args2[i], "{\"id\":%u,\"index\":%d%s}", (unsigned)M.id, M.index,
        (const char *[]){"", ",\"to\":\"top\"", ",\"to\":\"bottom\"", ",\"due\":true", "", ""}[i]);
    y += ink_ui_line_height(INK_FONT_NOTE) + INK_GAP_M;
    y = reminder_retry(screen, y, M.index, 5);
    button(screen, TR("移到最上"), INK_MARGIN, y, half, false, "memo.move_item", M.args2[1]);
    button(screen, TR("移到最下"), INK_MARGIN + half + INK_GAP, y, half, false, "memo.move_item", M.args2[2]);
    button(screen, TR("设到期时间"), INK_MARGIN, y + INK_CONTROL + INK_GAP_M, half, false, "memo.open", M.args2[3]);
    button(screen, TR("删除这一项"), INK_MARGIN + half + INK_GAP, y + INK_CONTROL + INK_GAP_M, half, false, "memo.delete_item", M.args2[0]);
    action(0, "edit_item", TR("改文字"), "memo.edit_item", M.args2[0], true);
    action(1, "top", TR("移到最上"), "memo.move_item", M.args2[1], true);
    action(2, "bottom", TR("移到最下"), "memo.move_item", M.args2[2], true);
    action(3, "due", TR("设到期时间"), "memo.open", M.args2[3], true);
    action(4, "delete_item", TR("删除这一项"), "memo.delete_item", M.args2[0], true);
}
static void due_page(lv_obj_t *screen) { /* the shared date picker; a reminder follows the due date (memo.set_due) */
    ink_tm_t t; int64_t at = M.note.due[M.index] ? M.note.due[M.index] : ink_time_now_s() + 86400;
    int w = (INK_CANVAS_W - 2 * INK_MARGIN - INK_GAP) / 2;
    ink_time_to_local(at, &t);
    if (!M.note.due[M.index]) { t.hour = 9; t.min = 0; } /* tomorrow 9:00 */
    int parts[5] = {t.year, t.month, t.day, t.hour, t.min};
    memcpy(M.parts, parts, sizeof parts); memset(M.part_labels, 0, sizeof M.part_labels);
    ink_ui_datetime_picker(screen, M.parts, bind_part);
    ink_ui_pill(screen, TR("清除到期"), INK_MARGIN, INK_CANVAS_H - INK_MARGIN - INK_PRIMARY, w, INK_PRIMARY, false, set_due, NULL);
    ink_ui_pill(screen, TR("设好了"), INK_MARGIN + w + INK_GAP, INK_CANVAS_H - INK_MARGIN - INK_PRIMARY, w, INK_PRIMARY, true, set_due, (void *)1);
    snprintf(M.args2[0], sizeof M.args2[0], "{\"id\":%u,\"index\":%d}", (unsigned)M.id, M.index);
    action(0, "set_due", TR("设好了"), "memo.set_due", M.args2[0], true);
}
static void voice_page(lv_obj_t *screen) { /* a memo with a recording: a written play button, no circles; MemoPending says when it was made */
    char text[96]; int y = INK_STATUS_BAR + INK_PRIMARY + INK_GAP_S, d[6] = {0};
    snprintf(text, sizeof text, M.note.ms ? "%s · %u:%02u" : "%s", TR("播放录音"), (unsigned)(M.note.ms / 60000), (unsigned)(M.note.ms / 1000 % 60));
    button(screen, text, INK_MARGIN, y, INK_CANVAS_W - 2 * INK_MARGIN, false, "memo.play", M.target);
    action(3, "play", TR("播放录音"), "memo.play", M.target, true);
    if (M.mode != PENDING) return;
    ink_shell_bind(ink_ui_text_button(screen, TR("删除"), INK_CANVAS_W - INK_MARGIN - fit(TR("删除")), INK_STATUS_BAR, fit(TR("删除")), NULL, NULL), "memo.delete", M.target);
    const char *name = strrchr(M.note.path, '/');
    if (name && sscanf(name, "/%4d%2d%2d-%2d%2d%2d", &d[0], &d[1], &d[2], &d[3], &d[4], &d[5]) >= 5)
        snprintf(text, sizeof text, TR("录于 %d月%d日 %02d:%02d · 连上网后整理成文字"), d[1], d[2], d[3], d[4]);
    else snprintf(text, sizeof text, "%s", TR("连上网后整理成文字"));
    ink_ui_label(screen, TR("录音已保存"), INK_FONT_SUBTITLE, INK_WEIGHT_BOLD, INK_MARGIN, y + INK_CONTROL + INK_GAP_XL, INK_CANVAS_W - 2 * INK_MARGIN);
    lv_obj_set_y(M.notice, ink_ui_px(y + INK_CONTROL + INK_GAP_XL + ink_ui_line_height(INK_FONT_SUBTITLE) + INK_GAP_S)); lv_label_set_text(M.notice, text);
    M.actions[1].enabled = true; M.actions[0].enabled = M.actions[2].enabled = false;
    ink_exec_post(INK_EXEC_BG, &T.sweep, sweep, NULL);
}
static void create(lv_obj_t *screen, void *arg) {
    int mode = 0; while (mode < 10 && strcmp(PAGES[mode].name, ink_ui_page_name())) mode++;
    M.mode = mode; M.id = (uint32_t)(uintptr_t)arg; M.saving = M.retry = M.blank = false; M.keyboard_top = -1;
    M.model = (ink_ui_page_spec_t){.id = ink_ui_page_name(), .title = TR("备忘"), .actions = M.actions};
    memset(M.actions, 0, sizeof M.actions); memset(M.tools, 0, sizeof M.tools);
    if (strcmp(M.member, ink_member_current()->id)) { memset(&M.position, 0, sizeof M.position); memset(&M.listed, 0, sizeof M.listed); memset(&M.note, 0, sizeof M.note); snprintf(M.member, sizeof M.member, "%s", ink_member_current()->id); }
    if (strcmp(S.member, M.member)) S.on = false;
    if (mode != DETAIL && mode != DUE) F.open = false; /* Fossify clears the search when the note changes */
    ink_shell_statusbar(screen, true);
    if (mode != RECORD) ink_shell_topbar(screen, mode == DUE ? TR("到期") : mode == EDIT || mode == FAIL || (mode == DETAIL && F.open) ? "" : TR("备忘")); /* room for written actions */
    M.notice = ink_ui_notice(screen, mode == FAIL ? TR("这条没存上。字还在这里。请腾出空间后重试") : "", INK_CANVAS_H - INK_PRIMARY - INK_CONTROL - INK_GAP_L);
    if (mode == RECORD) {
        ink_memo_voice_create(screen);
        action(0, "cancel", TR("不要了"), "audio.record_cancel", NULL, true); action(1, "stop", TR("记好了"), "audio.record_stop", NULL, true); return;
    }
    M.db = ink_db_open_cached_member(&MEMOS, M.member);
    if (!M.db) {
        lv_label_set_text(M.notice, TR("正在读取备忘")); ink_args_t a; ink_args_init(&a); ink_args_str(&a, "collection", MEMOS.name); request("data.list", &a); return;
    }
    bool list = mode == LIST || mode == UNDO;
    if (K.id && !strcmp(K.member, M.member) && (list || K.id != M.id) && put_text(K.id, K.body, K.draft, K.page)) K.id = 0; /* kept words: again */
    if (!list && mode != FAIL) {
        if (!M.id || (M.id == M.fresh && !read_note(M.db, M.id, &M.note))) {
            M.id = M.id ? M.id : ink_db_new_id(M.db); memset(&M.note, 0, sizeof M.note); M.note.checklist = mode == CHECKLIST; M.blank = true;
        }
        else if (!read_note(M.db, M.id, &M.note) || M.note.deleted) { lv_label_set_text(M.notice, TR("找不到这条备忘")); return; }
    }
    snprintf(M.object, sizeof M.object, "memo:%u", (unsigned)M.id);
    snprintf(M.target, sizeof M.target, "{\"id\":%u}", (unsigned)M.id);
    M.model.object = (ink_ui_object_ref_t){M.object, ink_db_last_ver(M.db)};
    if (mode == EDIT || mode == FAIL) action(0, "delete", TR("删除备忘"), "memo.delete", M.target, true);
    if (list) list_page(screen);
    else if (mode == EDIT || mode == FAIL) edit_page(screen);
    else if (mode == CHECKLIST) check_page(screen);
    else if (mode == ITEM) item_page(screen);
    else if (mode == DUE) due_page(screen);
    else {
        action(0, "edit", TR("编辑备忘"), "memo.edit", M.target, true); action(1, "delete", TR("删除备忘"), "memo.delete", M.target, true);
        action(2, "convert", TR("转成清单"), "memo.convert", M.target, true);
        if (M.note.path[0]) voice_page(screen);
        if (mode != PENDING) read_page(screen);
    }
}
static void destroy(void) {
    if (M.field && !flush_note(M.mode == FAIL)) { /* to the member who opened it, even when someone else is here now (they switch at once) */
        snprintf(K.member, sizeof K.member, "%s", M.member); snprintf(K.body, sizeof K.body, "%s", ink_textfield_text(M.field)); /* not stored: kept here, written when a memo page opens */
        snprintf(K.page, sizeof K.page, "%s", M.note.page); K.draft = M.note.draft; K.id = M.id;
    }
    if (M.mode == RECORD) ink_memo_voice_destroy();
    ink_db_close(M.db); M.db = NULL; M.list = NULL;
    M.field = M.notice = M.retry_button = M.delete_button = M.find = M.adder = M.done_row = M.undo_row = NULL;
}
/* the device locks over the editor; a save still on its way (the BG executor may be busy) is not stored yet: written now */
static void background(void) { if (M.field) flush_note(M.mode == FAIL || M.saving); }
static int snapshot(char *out, size_t cap) {
    char *body = ink_app_alloc(cap, true), *actions = ink_app_alloc(cap, true), status[160]; int result = -1;
    ink_ui_page_spec_t model = M.model;
    if (M.field) {
        snprintf(M.save_args, sizeof M.save_args, "{\"id\":%u,\"draft\":%u,\"page\":\"%s\"}", (unsigned)M.id, (unsigned)M.note.draft, M.note.page);
        action(1, "save", TR("记下当前草稿"), "memo.save", M.save_args, M.mode != FAIL);
        action(2, "retry", TR("重试"), "memo.retry", NULL, M.mode == FAIL);
        action(3, "undo_text", TR("撤销"), "text.undo", "{}", ink_text_can_undo());
        action(4, "redo_text", TR("重做"), "text.redo", "{}", ink_text_can_redo());
        model = M.model;
    }
    snprintf(status, sizeof status, "%s", M.notice ? lv_label_get_text(M.notice) : "");
    if (M.mode == RECORD) ink_memo_voice_status(status, sizeof status);
    if ((M.mode == LIST || M.mode == UNDO) && M.list && !M.total) snprintf(status, sizeof status, "%s", S.on ? TR("没有找到") : TR("还没有备忘，写一条吧"));
    if (body && actions && ink_ui_semantics_snapshot(&model, actions, cap) > 0) {
        int n = M.field ? ink_textfield_long_snapshot(M.field, body, cap) : M.list ? ink_ui_list_snapshot(M.list, body, cap) : 0;
        if (!n) snprintf(body, cap, "{}");
        if (n >= 0) {
            ink_jw_t w; ink_jw_init(&w, out, cap);
            ink_jw_raw(&w, "{\"view\":"); ink_jw_str(&w, PAGES[M.mode].name);
            ink_jw_raw(&w, ",\"status\":"); ink_jw_str(&w, status);
            if (M.mode == DETAIL && F.open) { ink_jw_raw(&w, ",\"find\":{\"query\":"); ink_jw_str(&w, F.query); ink_jw_raw(&w, ",\"at\":"); ink_jw_int(&w, F.count ? F.cur + 1 : 0); ink_jw_raw(&w, ",\"count\":"); ink_jw_int(&w, F.count); ink_jw_raw(&w, "}"); }
            if ((M.mode == LIST || M.mode == UNDO) && S.on) { ink_jw_raw(&w, ",\"search\":"); ink_jw_str(&w, S.query); }
            ink_jw_raw(&w, ",\"content\":"); ink_jw_raw(&w, body); ink_jw_raw(&w, ",\"controls\":"); ink_jw_raw(&w, actions); ink_jw_raw(&w, "}");
            result = w.overflow ? -1 : (int)w.len;
        }
    }
    ink_app_free(actions); ink_app_free(body); if (result < 0 && cap) out[0] = 0; return result;
}
static bool page_key(ink_key_t key, bool lng) {
    if (key == INK_KEY_BACK && !lng && M.mode == RECORD && ink_audio_recording()) { ink_memo_voice_stop(); return true; } /* ZSWatch: leaving saves */
    if (key == INK_KEY_BACK && !lng && M.mode == DETAIL && F.open) { F.open = false; F.query[0] = F.label[0] = 0; ink_ui_redraw(); return true; } /* Fossify: back closes the find bar first */
    return M.list && ink_ui_list_key(M.list, key, lng);
}
#define PAGE(name_) {.name = name_, .app = "memo", .create = create, .destroy = destroy, .snapshot = snapshot, .settle = settle, .on_key = page_key}
static const ink_activity_t PAGES[] = {PAGE("Memo"), PAGE("MemoDetail"), PAGE("MemoEdit"), PAGE("MemoChecklist"), PAGE("MemoUndo"), PAGE("MemoSaveFail"), PAGE("MemoRecord"), PAGE("MemoPending"), PAGE("MemoItem"), PAGE("MemoDue")};
static const ink_app_ops_t OPS = {.on_background = background};
static const ink_app_def_t APP = {.id = "memo", .title = TR_NOOP("备忘"), .icon = INK_UI_ICON(memo), .entry = &PAGES[LIST], .system_app = true, .child_ok = true, .ops = &OPS};
#define ID "\"id\":{\"type\":\"integer\",\"minimum\":1,\"maximum\":4294967295}"
#define ID_ARGS "{\"type\":\"object\",\"properties\":{" ID "},\"required\":[\"id\"]}"
#define INDEX "\"index\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":31}"
#define ITEM_ARGS(more, req) "{\"type\":\"object\",\"properties\":{" ID "," INDEX more "},\"required\":[\"id\",\"index\"" req "]}"
#define WRITE(id_, title_, input_) {.id = id_, .app = "memo", .title = title_, .input = input_, .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .exec = INK_EXEC_BG, .run = mutate}
static const ink_cap_t CAPS[] = {
    {.id = "memo.save", .app = "memo", .title = "记下当前成员的长文草稿", .input = "{\"type\":\"object\",\"properties\":{" ID ",\"draft\":{\"type\":\"integer\",\"minimum\":1},\"page\":{\"type\":\"string\",\"enum\":[\"MemoEdit\",\"MemoSaveFail\"]}},\"required\":[\"id\",\"draft\",\"page\"]}", .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .exec = INK_EXEC_BG, .run = mutate},
    WRITE("memo.delete", "删除备忘（可撤销，到期提醒一起删）", ID_ARGS),
    WRITE("memo.undo", "撤销删除备忘，或撤销刚才删掉已完成的清单项", ID_ARGS),
    WRITE("memo.convert", "逐行转成清单（最多 32 项）", ID_ARGS),
    WRITE("memo.check", "勾选或取消清单项", ITEM_ARGS("", "")),
    {.id = "memo.open", .app = "memo", .title = "查看备忘（query：停在这条里的第 1 处；index：清单的这一项；due：到期时间页）",
     .input = "{\"type\":\"object\",\"properties\":{" ID ",\"query\":{\"type\":\"string\",\"maxLength\":20}," INDEX ",\"due\":{\"type\":\"boolean\"}},\"required\":[\"id\"]}", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "memo.edit", .app = "memo", .title = "写一条或编辑备忘（checklist：新清单）", .input = "{\"type\":\"object\",\"properties\":{" ID ",\"checklist\":{\"type\":\"boolean\"}}}", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "memo.next", .app = "memo", .title = "备忘下一页", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "memo.prev", .app = "memo", .title = "备忘上一页", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "memo.retry", .app = "memo", .title = "重试记下备忘", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "memo.record", .app = "memo", .title = "说一句：录一条语音备忘（联网后整理成文字）", .output = "{\"type\":\"object\"}", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = record},
    {.id = "memo.play", .app = "memo", .title = "播放语音备忘的录音", .input = ID_ARGS, .effect = INK_FX_NAVIGATE, .child_ok = true, .exec = INK_EXEC_BG, .run = play},
    {.id = "memo.voice", .app = "memo", .title = "把录好的音存成语音备忘，或填上中枢整理的文字", .input = "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"integer\",\"minimum\":0},\"path\":{\"type\":\"string\",\"maxLength\":127},\"ms\":{\"type\":\"integer\",\"minimum\":0}}}",
     .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .expose = INK_EXPOSE_APP, .exec = INK_EXEC_BG, .run = mutate},
    {.id = "memo.search", .app = "memo", .title = "搜全部备忘（标题和正文，不分大小写；空串清除搜索）", .input = "{\"type\":\"object\",\"properties\":{\"query\":{\"type\":\"string\",\"maxLength\":20}},\"required\":[\"query\"]}",
     .output = "{\"type\":\"array\"}", .effect = INK_FX_READ, .child_ok = true, .exec = INK_EXEC_BG, .run = search},
    {.id = "memo.find", .app = "memo", .title = "在这条备忘里查找（打开查找条，停在第 1 处）", .input = "{\"type\":\"object\",\"properties\":{" ID ",\"query\":{\"type\":\"string\",\"maxLength\":20}},\"required\":[\"id\"]}", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "memo.find_next", .app = "memo", .title = "这条备忘里查找：下一处（到最后回到第 1 处）", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "memo.find_prev", .app = "memo", .title = "这条备忘里查找：上一处", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "memo.find_close", .app = "memo", .title = "关闭这条备忘里的查找", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    WRITE("memo.add_item", "清单加一项（多行就是多项，最多 32 项；id 是新的就建一条清单）", "{\"type\":\"object\",\"properties\":{" ID ",\"text\":{\"type\":\"string\",\"minLength\":1,\"maxLength\":300}},\"required\":[\"id\",\"text\"]}"),
    WRITE("memo.edit_item", "改清单这一项的文字", ITEM_ARGS(",\"text\":{\"type\":\"string\",\"minLength\":1,\"maxLength\":300}", ",\"text\"")),
    WRITE("memo.delete_item", "删除清单这一项", ITEM_ARGS("", "")),
    WRITE("memo.move_item", "把清单这一项移到最上或最下", ITEM_ARGS(",\"to\":{\"type\":\"string\",\"enum\":[\"top\",\"bottom\"]}", ",\"to\"")),
    WRITE("memo.clear_done", "删掉清单里已完成的项（可撤销）", ID_ARGS),
    WRITE("memo.uncheck_all", "清单全部取消勾选", ID_ARGS),
    WRITE("memo.set_due", "设到期时间（at 本地时间 YYYY-MM-DD HH:MM，空或不给就清除；到期前提醒；index 是清单的项，文本备忘不给）",
          "{\"type\":\"object\",\"properties\":{" ID "," INDEX ",\"at\":{\"type\":\"string\",\"maxLength\":16}},\"required\":[\"id\"]}"),
    {.id = "memo.show_done", .app = "memo", .title = "清单里已完成的项：展开或收起", .input = ID_ARGS, .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
};
int ink_memo_init(void) {
    return ink_db_register(&MEMOS) || ink_cap_register(CAPS, sizeof CAPS / sizeof *CAPS) || ink_app_register(&APP) ||
        ink_cap_subscribe("audio.record_done", recorded, NULL, INK_EXEC_BG) || ink_state_watch("net.online", kick, NULL, INK_EXEC_BG) ? -1 : 0;
}
