/* Member memos share the capability bus, F4 lists and the long-input draft. */
#include <stdio.h>
#include <string.h>
#include "ink/sys/app.h"
#include "ink/sys/appreg.h"
#include "ink/sys/file.h"
#include "ink/sys/hub.h"
#include "ink/sys/json.h"
#include "ink/sys/media.h"
#include "ink/sys/members.h"
#include "ink/sys/state.h"
#include "ink/sys/time.h"
#include "ink/sys/textfield.h"
#include "ink/sys/ui_list.h"
#include "ink/ui_shell.h"

static const ink_db_def_t MEMOS = {.name = "memo.notes", .title = "备忘", .scope = INK_DB_MEMBER,
    .schema = 1, .json = true, .ai_read = true, .max_records = 200};
typedef struct { char body[4096], page[24], path[INK_FS_PATH_MAX]; uint32_t checked, draft, audio, ms; bool checklist, deleted, pending; } memo_t;
static const ink_activity_t PAGES[8];
enum { LIST, DETAIL, EDIT, CHECKLIST, UNDO, FAIL, RECORD, PENDING };
#define VOICE_DIR "/me/memos" /* a voice memo's recording: Ogg Opus, in the member's own folder */
#define HELD_MAX 8
static struct {
    bool busy; uint32_t id; char member[16], text[1024]; ink_exec_job_t sweep;
    struct { char member[16]; uint32_t id, fails; uint64_t until; } held[HELD_MAX]; /* failed this boot: not sent again before `until` */
} T INK_PSRAM_BSS; /* the one being made into words: BG */
static const char NOT_LOADED[] = "备忘还没读出来，请先打开备忘再试";
static struct {
    ink_db_t *db;
    ink_ui_list_t *list;
    ink_ui_list_position_t position;
    lv_obj_t *field, *notice, *retry_button, *delete_button;
    memo_t note;
    char member[16], sent[4096], ids[4][32], args[4][96], labels[4][128], object[32], target[64], save_args[160];
    uint32_t id, started;
    int mode;
    lv_timer_t *timer;
    ink_exec_job_t recorded;
    bool saving, retry;
    ink_ui_action_spec_t actions[4];
    ink_ui_page_spec_t model;
    ink_exec_job_t refresh;
} M INK_PSRAM_BSS;

static bool read_note(ink_db_t *db, uint32_t id, memo_t *note) {
    char *raw = ink_app_alloc(INK_DB_RECORD_MAX + 1, true);
    ink_json_t *j = ink_app_alloc(sizeof *j, true);
    ink_rec_t rec = {0}; memset(note, 0, sizeof *note);
    bool ok = raw && j && ink_db_get(db, id, &rec, raw, INK_DB_RECORD_MAX) == 0;
    if (ok) {
        raw[rec.len] = 0; int64_t value = 0;
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
    }
    ink_app_free(j); ink_app_free(raw); return ok;
}
static unsigned lines(const char *s) { unsigned n = *s ? 1 : 0; for (; *s; s++) n += *s == '\n'; return n; }
static uint32_t remap_checks(const char *old, const char *text, uint32_t checked) {
    uint32_t used = 0, result = 0;
    for (unsigned i = 0; *text && i < 32; i++) {
        size_t len = strcspn(text, "\n"); const char *p = old;
        for (unsigned j = 0; *p && j < 32; j++) {
            size_t size = strcspn(p, "\n"); uint32_t bit = UINT32_C(1) << j;
            if (!(used & bit) && len == size && !memcmp(text, p, len)) { used |= bit; if (checked & bit) result |= UINT32_C(1) << i; break; }
            p += size; if (*p) p++;
        }
        text += len; if (*text) text++;
    }
    return result;
}
static bool clipped(const char *text) {
    while (*text) { size_t len = strcspn(text, "\n"); if (len > ink_text_byte_offset(text, 24)) return true; text += len; if (*text) text++; }
    return false;
}
static void refresh(void *arg) {
    (void)arg;
    if (!M.db || !ink_ui_page_current() || strcmp(M.member, ink_member_current()->id)) return;
    if (M.id && M.mode != LIST && M.mode != UNDO && read_note(M.db, M.id, &M.note) && M.note.deleted) ink_ui_push(&PAGES[UNDO], NULL);
    else if (M.field) return;
    else if (M.mode == DETAIL && M.note.checklist) ink_ui_push(&PAGES[CHECKLIST], (void *)(uintptr_t)M.id);
    else if (M.mode == PENDING && !M.note.pending) { ink_ui_pop(); ink_ui_push(&PAGES[DETAIL], (void *)(uintptr_t)M.id); }
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
    if (ok && !T.text[0]) snprintf(T.text, sizeof T.text, "%s", "这段录音里没听到说话"); /* done all the same: not sent again and again */
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
static void mutate(ink_call_t *c) {
    int64_t id = 0, index = 0, draft = 0; ink_args_get_int(&c->args, "id", &id);
    ink_db_t *db = ink_db_open_cached_member(&MEMOS, ink_cap_call_member(c)); /* no wait on BG: the app's pages read it in */
    memo_t *n = ink_app_alloc(sizeof *n, true); char *raw = ink_app_alloc(INK_DB_RECORD_MAX + 1, true);
    bool save = !strcmp(c->cap->id, "memo.save"), is_voice = !strcmp(c->cap->id, "memo.voice"), fresh = is_voice && !id;
    if (!db || !n || !raw) c->rc.status = INK_RC_FAILED;
    else if (fresh) id = ink_db_new_id(db);
    if (c->rc.status == INK_RC_DONE && !read_note(db, (uint32_t)id, n) && !save && !fresh) c->rc.status = INK_RC_INVALID;
    if (c->rc.status == INK_RC_DONE) {
        if (save) {
            memcpy(raw, n->body, sizeof n->body);
            ink_args_get_int(&c->args, "draft", &draft); ink_args_get_str(&c->args, "page", n->page, sizeof n->page);
            if (n->deleted || ink_text_draft_read(ink_cap_call_member(c), n->page, (uint32_t)draft, n->body, sizeof n->body) < 0) c->rc.status = INK_RC_INVALID;
            if (n->checklist) n->checked = remap_checks(raw, n->body, n->checked);
            n->draft = (uint32_t)draft;
        } else if (is_voice) voice(c, db, (uint32_t)id, n);
        else if (!strcmp(c->cap->id, "memo.undo")) n->deleted = false;
        else if (n->deleted) c->rc.status = INK_RC_INVALID;
        else if (!strcmp(c->cap->id, "memo.delete")) n->deleted = true;
        else if (!strcmp(c->cap->id, "memo.convert")) n->checklist = true;
        else {
            ink_args_get_int(&c->args, "index", &index);
            if (!n->checklist || index < 0 || index >= 32 || (unsigned)index >= lines(n->body)) c->rc.status = INK_RC_INVALID;
            else n->checked ^= UINT32_C(1) << index;
        }
        if (n->checklist && lines(n->body) > 32) c->rc.status = INK_RC_INVALID;
        if (c->rc.status == INK_RC_DONE) {
            ink_jw_t w; ink_jw_init(&w, raw, INK_DB_RECORD_MAX + 1);
            ink_jw_raw(&w, "{\"body\":"); ink_jw_str(&w, n->body);
            ink_jw_raw(&w, ",\"checklist\":"); ink_jw_raw(&w, n->checklist ? "true" : "false"); ink_jw_raw(&w, ",\"deleted\":"); ink_jw_raw(&w, n->deleted ? "true" : "false");
            ink_jw_raw(&w, ",\"checked\":"); ink_jw_int(&w, n->checked); ink_jw_raw(&w, ",\"draft\":"); ink_jw_int(&w, n->draft);
            ink_jw_raw(&w, ",\"page\":"); ink_jw_str(&w, n->page);
            if (n->path[0]) { ink_jw_raw(&w, ",\"path\":"); ink_jw_str(&w, n->path); ink_jw_raw(&w, ",\"audio\":"); ink_jw_int(&w, n->audio);
                ink_jw_raw(&w, ",\"ms\":"); ink_jw_int(&w, n->ms); ink_jw_raw(&w, n->pending ? ",\"pending\":true" : ",\"pending\":false"); }
            ink_jw_raw(&w, "}");
            ink_db_write_t wr = {.id = (uint32_t)id, .data = raw, .len = w.len, .src = c->src, .written_ver = &c->rc.revision};
            if (w.overflow || ink_db_put(db, &wr, NULL) != 0 || ink_db_wait(db, c->rc.revision, (int64_t)ink_cap_time_left_ms(c) * 1000) != 0) c->rc.status = INK_RC_FAILED;
        }
    }
    snprintf(c->rc.object, sizeof c->rc.object, "memo:%u", (unsigned)id);
    snprintf(c->rc.say, sizeof c->rc.say, "%s", c->rc.status == INK_RC_DONE ? "已记下" : !db ? NOT_LOADED : "这条没存上，字还在这里。请腾出空间后重试");
    if (c->rc.status == INK_RC_DONE) ink_exec_post(INK_EXEC_UI, &M.refresh, refresh, NULL);
    if (c->rc.status == INK_RC_DONE && fresh) { ink_exec_post(INK_EXEC_UI, &M.recorded, after_record, (void *)(uintptr_t)id); sweep(NULL); }
    ink_app_free(raw); ink_app_free(n); ink_db_close(db);
}
static void save_failed(bool failed) {
    int delta = (failed - (M.mode == FAIL)) * ink_ui_px(INK_PRIMARY);
    lv_obj_set_y(M.field, lv_obj_get_y(M.field) + delta); lv_obj_set_height(M.field, lv_obj_get_height(M.field) - delta);
    lv_obj_t *label = lv_obj_get_child(M.field, 0); lv_obj_set_height(label, lv_obj_get_height(label) - delta);
    M.mode = failed ? FAIL : EDIT;
    lv_obj_set_y(M.notice, ink_ui_px(failed ? INK_STATUS_BAR + INK_PRIMARY + INK_GAP_S : INK_CANVAS_H - INK_PRIMARY - INK_CONTROL - INK_GAP_L));
    if (failed) lv_obj_remove_flag(M.retry_button, LV_OBJ_FLAG_HIDDEN); else lv_obj_add_flag(M.retry_button, LV_OBJ_FLAG_HIDDEN);
    if (failed) lv_obj_add_flag(M.delete_button, LV_OBJ_FLAG_HIDDEN); else lv_obj_remove_flag(M.delete_button, LV_OBJ_FLAG_HIDDEN);
}
static void reply(void *ctx, const ink_receipt_t *rc) {
    if ((uint32_t)(uintptr_t)ctx != ink_ui_generation() || !ink_ui_page_current() || strcmp(M.member, ink_member_current()->id)) return;
    M.saving = false;
    if (rc->status != INK_RC_DONE && M.field && M.mode == EDIT) save_failed(true);
    else if (rc->status == INK_RC_DONE && M.field && M.mode == FAIL) save_failed(false);
    if (M.notice) lv_label_set_text(M.notice, rc->say);
    if (rc->status == INK_RC_DONE && !M.field) ink_ui_redraw();
    ink_ui_content_changed();
}
static void request(const char *cap, const ink_args_t *args) {
    ink_call_opts_t opts = {.src = INK_SRC_TOUCH, .member = M.member, .reply_exec = INK_EXEC_UI, .done = reply, .ctx = (void *)(uintptr_t)ink_ui_generation()};
    if (ink_cap_call(cap, args, &opts) != 0) { M.saving = false; if (M.notice) lv_label_set_text(M.notice, "这条没存上，字还在这里。请重试"); }
}
static void navigate(ink_call_t *c) {
    const char *cap = c->cap->id; int64_t id = 0; ink_args_get_int(&c->args, "id", &id);
    if (!strcmp(cap, "memo.next") || !strcmp(cap, "memo.prev")) { if (M.list) ink_ui_list_turn(M.list, !strcmp(cap, "memo.next") ? 1 : -1); }
    else if (!strcmp(cap, "memo.retry")) M.retry = true;
    else {
        int mode = !strcmp(cap, "memo.edit") ? EDIT : DETAIL;
        memo_t *n = ink_app_alloc(sizeof *n, true);
        if (id && (!M.db || !n || !read_note(M.db, (uint32_t)id, n) || n->deleted)) c->rc.status = INK_RC_INVALID;
        else { if (mode == DETAIL && n && n->checklist) mode = CHECKLIST; else if (id && n && n->pending) mode = PENDING; /* its words would cover what is typed */ memset(&M.position, 0, sizeof M.position); ink_ui_push(&PAGES[mode], (void *)(uintptr_t)id); }
        ink_app_free(n);
    }
}
static void record_reply(void *ctx, const ink_receipt_t *rc) { /* UI: the microphone is on, or why not */
    ink_call_t *c = ctx; c->rc = *rc; c->rc.result = NULL;
    if (rc->status == INK_RC_DONE && ink_ui_page_current()) ink_ui_push(&PAGES[RECORD], NULL);
    else if (M.notice && rc->status != INK_RC_PENDING_CONFIRMATION) lv_label_set_text(M.notice, rc->say[0] ? rc->say : "现在录不了，请稍后再试");
    ink_cap_done(c);
}
static void record(ink_call_t *c) { /* UI: a recording in the member's memo folder, asked for by whoever asked for this */
    ink_tm_t t; ink_time_to_local(ink_time_now_s(), &t); char path[64]; ink_args_t a; ink_args_init(&a);
    snprintf(path, sizeof path, VOICE_DIR "/%04d%02d%02d-%02d%02d%02d.opus", t.year, t.month, t.day, t.hour, t.min, t.sec); ink_args_str(&a, "path", path);
    ink_call_opts_t o = {.src = c->src, .member = ink_cap_call_member(c), .reply_exec = INK_EXEC_UI, .done = record_reply, .ctx = c};
    ink_cap_defer(c);
    if (ink_cap_call("audio.record", &a, &o) != 0) { c->rc.status = INK_RC_FAILED; ink_cap_done(c); }
}
static void play_reply(void *ctx, const ink_receipt_t *rc) { ink_call_t *c = ctx; c->rc = *rc; c->rc.result = NULL; ink_cap_done(c); }
static void play(ink_call_t *c) { /* BG: by the file's number, which a rename or a move keeps */
    int64_t id = 0; ink_args_get_int(&c->args, "id", &id); ink_fs_entry_t e = {0}; const char *who = ink_cap_call_member(c);
    ink_db_t *db = ink_db_open_cached_member(&MEMOS, who); memo_t *n = ink_app_alloc(sizeof *n, true); /* no wait on BG */
    if (!db) { c->rc.status = INK_RC_FAILED; snprintf(c->rc.say, sizeof c->rc.say, "%s", NOT_LOADED); }
    else if (!n || !read_note(db, (uint32_t)id, n) || n->deleted || !n->path[0]) c->rc.status = INK_RC_INVALID;
    else {
        ink_args_t a; ink_args_init(&a); ink_args_str(&a, "path", ink_fs_lookup(n->audio, &e) == 0 ? e.path : n->path); ink_args_int(&a, "start_ms", 0);
        ink_call_opts_t o = {.src = c->src, .member = ink_cap_call_member(c), .reply_exec = INK_EXEC_BG, .done = play_reply, .ctx = c};
        ink_cap_defer(c);
        if (ink_cap_call("media.play", &a, &o) != 0) { c->rc.status = INK_RC_FAILED; ink_cap_done(c); }
    }
    ink_app_free(n); ink_db_close(db);
}
static void minutes(lv_timer_t *t) { /* UI: how long, a minute at a time: the panel is not redrawn every second */
    (void)t; char text[64]; uint32_t m = lv_tick_elaps(M.started) / 60000;
    if (m) snprintf(text, sizeof text, "%u 分钟 · 没网也能记", (unsigned)m); else snprintf(text, sizeof text, "不到 1 分钟 · 没网也能记");
    if (M.notice && strcmp(text, lv_label_get_text(M.notice))) lv_label_set_text(M.notice, text);
}
static lv_obj_t *centered(lv_obj_t *screen, const char *text, int px, ink_ui_weight_t weight, int y) {
    lv_obj_t *l = ink_ui_label(screen, text, px, weight, INK_MARGIN, y, INK_CANVAS_W - 2 * INK_MARGIN);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0); return l;
}
static void voice_pages(lv_obj_t *screen, int mode) { /* MemoRecord, and the player of a memo with a recording */
    int y = INK_STATUS_BAR + INK_PRIMARY + INK_GAP_S, mid = INK_CANVAS_H / 2 - INK_BLOCK;
    if (mode == RECORD) {
        int ring = INK_CIRCLE_LARGE + 2 * INK_GAP;
        ink_ui_box(screen, (INK_CANVAS_W - ring) / 2, mid - ring / 2, ring, ring, ring / 2, INK_SURFACE_WHITE, true);
        ink_ui_circle(screen, "wave", (INK_CANVAS_W - INK_CIRCLE_LARGE) / 2, mid - INK_CIRCLE_LARGE / 2, INK_CIRCLE_LARGE, INK_ICON_STATE, INK_SURFACE_BLACK, NULL, NULL);
        centered(screen, "正在记", INK_FONT_TITLE, INK_WEIGHT_BOLD, mid + ring / 2 + INK_GAP_XL);
        lv_obj_set_y(M.notice, ink_ui_px(mid + ring / 2 + INK_GAP_XL + INK_LINE_TITLE + INK_GAP_XS)); lv_obj_set_style_text_align(M.notice, LV_TEXT_ALIGN_CENTER, 0);
        M.started = lv_tick_get(); minutes(NULL); M.timer = lv_timer_create(minutes, 5000, NULL);
        int w = (INK_CANVAS_W - 2 * INK_MARGIN - INK_GAP) * 3 / 7, top = INK_CANVAS_H - INK_PRIMARY - INK_MARGIN;
        ink_shell_bind(ink_ui_pill(screen, "不要了", INK_MARGIN, top, w, INK_PRIMARY, false, NULL, NULL), "audio.record_cancel", NULL);
        ink_shell_bind(ink_ui_pill(screen, "记好了", INK_MARGIN + w + INK_GAP, top, INK_CANVAS_W - 2 * INK_MARGIN - INK_GAP - w, INK_PRIMARY, true, NULL, NULL), "audio.record_stop", NULL);
        M.actions[0] = (ink_ui_action_spec_t){.id = "cancel", .label = "不要了", .capability = "audio.record_cancel", .enabled = true};
        M.actions[1] = (ink_ui_action_spec_t){.id = "stop", .label = "记好了", .capability = "audio.record_stop", .enabled = true};
        M.model.action_count = 2; return;
    }
    char text[16]; snprintf(text, sizeof text, M.note.ms ? "%u:%02u" : "--:--", (unsigned)(M.note.ms / 60000), (unsigned)(M.note.ms / 1000 % 60));
    lv_obj_t *bar = ink_ui_box(screen, INK_MARGIN, y, INK_CANVAS_W - 2 * INK_MARGIN, INK_CONTROL + 2 * INK_GAP_S, (INK_CONTROL + 2 * INK_GAP_S) / 2, INK_SURFACE_STATIC, false);
    ink_shell_bind(ink_ui_circle(bar, "play", INK_GAP_S, INK_GAP_S, INK_CONTROL, INK_ICON_MEDIUM, INK_SURFACE_BLACK, NULL, NULL), "memo.play", M.target);
    int track = INK_CANVAS_W - 2 * INK_MARGIN - INK_CONTROL - 2 * INK_GAP - INK_BLOCK_L;
    ink_ui_box(bar, INK_CONTROL + INK_GAP, (INK_CONTROL + 2 * INK_GAP_S - INK_GAP_M) / 2, track, INK_GAP_M, INK_GAP_M / 2, INK_SURFACE_WHITE, true);
    ink_ui_line(bar, text, INK_FONT_LIST, INK_WEIGHT_MEDIUM, INK_CONTROL + 2 * INK_GAP + track, (INK_CONTROL + 2 * INK_GAP_S - INK_LINE_LIST) / 2, INK_BLOCK_L - INK_GAP);
    M.actions[3] = (ink_ui_action_spec_t){.id = "play", .label = "播放录音", .capability = "memo.play", .args_json = M.target, .enabled = true};
    if (mode != PENDING) return;
    ink_shell_bind(ink_ui_circle(screen, "trash", INK_CANVAS_W - INK_MARGIN - INK_CONTROL, INK_STATUS_BAR + INK_GAP_S, INK_CONTROL, INK_ICON_LIST, INK_SURFACE_STATIC, NULL, NULL), "memo.delete", M.target);
    ink_ui_circle(screen, "mic", (INK_CANVAS_W - INK_CIRCLE_LARGE) / 2, mid - INK_CIRCLE_LARGE / 2, INK_CIRCLE_LARGE, INK_ICON_STATE, INK_SURFACE_STATIC, NULL, NULL);
    centered(screen, "录音已保存", INK_FONT_SUBTITLE, INK_WEIGHT_BOLD, mid + INK_CIRCLE_LARGE / 2 + INK_GAP_XL);
    lv_obj_set_y(M.notice, ink_ui_px(mid + INK_CIRCLE_LARGE / 2 + INK_GAP_XL + INK_LINE_SUBTITLE + INK_GAP_S)); lv_obj_set_style_text_align(M.notice, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(M.notice, "连上网后整理成文字。\n不用等，先去做别的。");
    M.actions[1].enabled = true; M.actions[0].enabled = M.actions[2].enabled = false;
    ink_exec_post(INK_EXEC_BG, &T.sweep, sweep, NULL);
}
static void save_current(void) {
    ink_args_t a; ink_args_init(&a); ink_args_int(&a, "id", M.id); ink_args_int(&a, "draft", M.note.draft); ink_args_str(&a, "page", M.note.page);
    snprintf(M.sent, sizeof M.sent, "%s", ink_textfield_text(M.field)); M.retry = false; M.saving = true; request("memo.save", &a);
}
static void settle(void) {
    if (!M.field || M.saving || !ink_ui_page_current()) return;
    ink_textfield_long_refresh(M.field);
    const char *text = ink_textfield_text(M.field);
    if (!M.retry && (!strcmp(text, M.sent) || M.mode == FAIL)) return;
    save_current();
}
static void retry(lv_event_t *e) { (void)e; ink_args_t a; ink_args_init(&a); request("memo.retry", &a); }
static const ink_ui_action_spec_t PAGING[] = {
    {.id = "prev", .label = "上一页", .capability = "memo.prev"}, {.id = "next", .label = "下一页", .capability = "memo.next"},
    {.id = "new", .label = "写一条", .capability = "memo.edit", .enabled = true}
};
static bool nth(size_t index, uint32_t *id, memo_t *note) {
    ink_db_cursor_t cur = {0}; ink_rec_t rec;
    while (ink_db_next(M.db, &cur, &rec, NULL, 0) == 1) {
        if (read_note(M.db, rec.id, note) && !note->deleted && index-- == 0) { *id = rec.id; return true; }
    }
    return false;
}
static void bind_item(lv_obj_t *slot, size_t index, ink_ui_field_spec_t *field, ink_ui_action_spec_t *action, void *ctx) {
    (void)ctx; unsigned at = index % 4; uint32_t id = M.id; memo_t *n = ink_app_alloc(sizeof *n, true);
    if (!n) return;
    bool list = M.mode == LIST || M.mode == UNDO;
    if (list && !nth(index, &id, n)) { ink_app_free(n); return; }
    const char *text = list ? (n->pending ? "随口记 · 录音已保存，联网后整理成文字" : n->body) : M.note.body;
    if (!list && M.mode == CHECKLIST) for (size_t i = 0; i < index && *text; i++) { const char *p = strchr(text, '\n'); text = p ? p + 1 : text + strlen(text); }
    else if (!list) text += ink_text_byte_offset(text, index * 24);
    size_t len = ink_text_byte_offset(text, 24);
    if (M.mode == CHECKLIST && strcspn(text, "\n") < len) len = strcspn(text, "\n");
    bool truncated = M.mode == CHECKLIST && strcspn(text, "\n") > len;
    snprintf(M.labels[at], sizeof M.labels[at], "%.*s%s", (int)len, text, truncated ? "…" : "");
    if (list) for (char *p = M.labels[at]; *p; p++) if (*p == '\n') *p = ' ';
    snprintf(M.ids[at], sizeof M.ids[at], "memo:%u:%zu", (unsigned)id, list ? 0 : index);
    snprintf(M.args[at], sizeof M.args[at], "{\"id\":%u,\"index\":%zu}", (unsigned)id, index);
    int x = M.mode == CHECKLIST ? INK_CONTROL : INK_GAP;
    if (!lv_obj_get_child_count(slot)) ink_ui_label(slot, "", INK_FONT_LIST, INK_WEIGHT_REGULAR, x, INK_GAP_S, INK_CANVAS_W - 2 * INK_MARGIN - x - INK_GAP);
    lv_label_set_text(lv_obj_get_child(slot, 0), M.labels[at]);
    if (M.mode == CHECKLIST) {
        if (lv_obj_get_child_count(slot) > 1) lv_obj_delete(lv_obj_get_child(slot, 1));
        bool checked = (M.note.checked & (UINT32_C(1) << index)) != 0;
        lv_obj_t *box = ink_ui_box(slot, INK_GAP, INK_GAP_L, INK_ICON_LIST, INK_ICON_LIST, -1, checked ? INK_SURFACE_BLACK : INK_SURFACE_WHITE, true);
        if (checked) ink_ui_icon(box, "check", INK_ICON_MEDIUM, INK_ICON_LINE, INK_GAP_XS, INK_GAP_XS, true);
    }
    *field = (ink_ui_field_spec_t){.id = M.ids[at], .role = M.mode == CHECKLIST ? "checkbox" : "listitem", .label = M.labels[at], .value = M.mode == CHECKLIST ? (M.note.checked & (UINT32_C(1) << index) ? "已完成" : "未完成") : "", .object = {M.ids[at], ink_db_last_ver(M.db)}};
    *action = (ink_ui_action_spec_t){.id = M.ids[at], .label = M.mode == CHECKLIST ? "勾选或取消" : "查看备忘", .capability = M.mode == CHECKLIST ? "memo.check" : "memo.open", .args_json = M.args[at], .object = field->object, .enabled = list || M.mode == CHECKLIST};
    ink_app_free(n);
}
static void create(lv_obj_t *screen, void *arg) {
    int mode = 0; while (mode < 8 && strcmp(PAGES[mode].name, ink_ui_page_name())) mode++;
    M.mode = mode; M.id = (uint32_t)(uintptr_t)arg; M.saving = M.retry = false;
    M.model = (ink_ui_page_spec_t){.id = ink_ui_page_name(), .title = "备忘"};
    if (strcmp(M.member, ink_member_current()->id)) { memset(&M.position, 0, sizeof M.position); memset(&M.note, 0, sizeof M.note); snprintf(M.member, sizeof M.member, "%s", ink_member_current()->id); }
    ink_shell_statusbar(screen, true);
    if (mode != RECORD) ink_shell_topbar(screen, "备忘");
    M.notice = ink_ui_notice(screen, mode == FAIL ? "这条没存上。字还在这里。请腾出空间后重试" : "", INK_CANVAS_H - INK_PRIMARY - INK_CONTROL - INK_GAP_L);
    if (mode == RECORD) { M.model.actions = M.actions; voice_pages(screen, mode); return; }
    M.db = ink_db_open_cached_member(&MEMOS, M.member);
    if (!M.db) {
        lv_label_set_text(M.notice, "正在读取备忘"); ink_args_t a; ink_args_init(&a); ink_args_str(&a, "collection", MEMOS.name); request("data.list", &a); return;
    }
    bool list = mode == LIST || mode == UNDO;
    if (!list && mode != FAIL) {
        if (!M.id) { M.id = ink_db_new_id(M.db); memset(&M.note, 0, sizeof M.note); }
        else if (!read_note(M.db, M.id, &M.note) || M.note.deleted) { lv_label_set_text(M.notice, "找不到这条备忘"); return; }
    }
    snprintf(M.object, sizeof M.object, "memo:%u", (unsigned)M.id);
    snprintf(M.target, sizeof M.target, "{\"id\":%u}", (unsigned)M.id);
    M.model = (ink_ui_page_spec_t){.id = ink_ui_page_name(), .title = "备忘", .object = {M.object, ink_db_last_ver(M.db)}, .actions = M.actions};
    M.actions[0] = (ink_ui_action_spec_t){.id = "edit", .label = "编辑备忘", .capability = "memo.edit", .args_json = M.target, .enabled = !list};
    M.actions[1] = (ink_ui_action_spec_t){.id = "delete", .label = "删除备忘", .capability = "memo.delete", .args_json = M.target, .enabled = !list};
    M.actions[2] = (ink_ui_action_spec_t){.id = "convert", .label = "转成清单", .capability = "memo.convert", .args_json = M.target, .enabled = !list && !M.note.checklist};
    M.model.action_count = 3;
    if (M.note.path[0] && (mode == DETAIL || mode == PENDING)) { voice_pages(screen, mode); M.model.action_count = 4; }
    if (mode == PENDING) return;
    if (mode == EDIT || mode == FAIL) {
        ink_text_cfg_t cfg = {.type = INK_TEXT_PLAIN, .max = 4095, .initial = M.note.body, .draft_page = ink_ui_page_name(), .draft_id = !strcmp(M.note.page, ink_ui_page_name()) ? M.note.draft : 0};
        M.field = ink_textfield_long_create(screen, &cfg); snprintf(M.sent, sizeof M.sent, "%s", mode == FAIL ? M.note.body : "\1");
        M.note.draft = ink_text_draft_id(); snprintf(M.note.page, sizeof M.note.page, "%s", ink_ui_page_name());
        M.delete_button = ink_ui_circle(screen, "trash", INK_CANVAS_W - INK_MARGIN - INK_CONTROL, INK_STATUS_BAR + INK_GAP_S, INK_CONTROL, INK_ICON_LIST, INK_SURFACE_WHITE, NULL, NULL); ink_shell_bind(M.delete_button, "memo.delete", M.target);
        M.retry_button = ink_ui_pill(screen, "重试", INK_CANVAS_W - INK_MARGIN - INK_PRIMARY, INK_STATUS_BAR, INK_PRIMARY, INK_CONTROL, false, retry, NULL);
        lv_obj_add_flag(M.retry_button, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    size_t total = 0; uint32_t undo = 0, undo_ver = 0;
    if (list) {
        ink_db_cursor_t cur = {0}; ink_rec_t rec; memo_t *n = ink_app_alloc(sizeof *n, true);
        while (n && ink_db_next(M.db, &cur, &rec, NULL, 0) == 1) if (read_note(M.db, rec.id, n)) {
            if (!n->deleted) total++;
            else if (rec.ver > undo_ver) { undo = rec.id; undo_ver = rec.ver; }
        }
        ink_app_free(n);
    } else total = mode == CHECKLIST ? lines(M.note.body) : (ink_text_char_offset(M.note.body, strlen(M.note.body)) + 23) / 24;
    if (mode == CHECKLIST && clipped(M.note.body)) lv_label_set_text(M.notice, "长项已省略，编辑备忘可看全文");
    ink_ui_page_spec_t model = {.id = ink_ui_page_name(), .title = "备忘", .object = {"memo.notes", ink_db_last_ver(M.db)}, .actions = PAGING, .action_count = 3};
    int rows = M.note.path[0] && !list ? 3 : 4, top = rows < 4 ? INK_CONTROL + 2 * INK_GAP_S + INK_GAP_M : 0;
    lv_obj_t *body = ink_ui_box(screen, 0, INK_STATUS_BAR + INK_PRIMARY + INK_GAP_L + top, INK_CANVAS_W, rows * (INK_ROW_DOUBLE + INK_GAP_M) + INK_PRIMARY, 0, INK_SURFACE_WHITE, false);
    M.list = ink_ui_list_create(body, &model, &M.position, rows, 1, INK_ROW_DOUBLE, "还没有备忘，写一条吧", bind_item, NULL);
    if (M.list) ink_ui_list_refresh(M.list, total);
    if (list) {
        int w = INK_CANVAS_W - 2 * INK_MARGIN - INK_GAP - 2 * INK_CONTROL;
        ink_shell_bind(ink_ui_pill(screen, "说一句", INK_MARGIN, INK_CANVAS_H - INK_PRIMARY - INK_MARGIN, w, INK_PRIMARY, true, NULL, NULL), "memo.record", NULL);
        ink_shell_bind(ink_ui_pill(screen, "写一条", INK_MARGIN + w + INK_GAP, INK_CANVAS_H - INK_PRIMARY - INK_MARGIN, 2 * INK_CONTROL, INK_PRIMARY, false, NULL, NULL), "memo.edit", "{}");
        ink_exec_post(INK_EXEC_BG, &T.sweep, sweep, NULL);
        M.model.action_count = 0;
        if (undo) {
            snprintf(M.target, sizeof M.target, "{\"id\":%u}", (unsigned)undo);
            M.actions[0] = (ink_ui_action_spec_t){.id = "undo", .label = "撤销删除", .capability = "memo.undo", .args_json = M.target, .enabled = true}; M.model.action_count = 1;
            ink_shell_bind(ink_ui_pill(screen, "已删除一条备忘 · 撤销", INK_MARGIN, INK_CANVAS_H - INK_PRIMARY - INK_MARGIN - INK_CONTROL - INK_GAP_M, INK_CANVAS_W - 2 * INK_MARGIN, INK_CONTROL, false, NULL, NULL), "memo.undo", M.target);
        }
    } else for (int i = 0; i < 3; i++) if (M.actions[i].enabled) ink_shell_bind(ink_ui_pill(screen, M.actions[i].label, INK_MARGIN + i * (INK_CANVAS_W - 2 * INK_MARGIN) / 3, INK_CANVAS_H - INK_PRIMARY - INK_MARGIN, (INK_CANVAS_W - 2 * INK_MARGIN) / 3, INK_CONTROL, false, NULL, NULL), M.actions[i].capability, M.target);
}
static void destroy(void) {
    if (M.field && M.mode != FAIL && ink_ui_page_current() && !strcmp(M.member, ink_member_current()->id) && strcmp(ink_textfield_text(M.field), M.sent)) save_current();
    if (M.timer) lv_timer_delete(M.timer);
    M.timer = NULL; ink_db_close(M.db); M.db = NULL; M.list = NULL; M.field = M.notice = M.retry_button = M.delete_button = NULL;
}
static int snapshot(char *out, size_t cap) {
    char *body = ink_app_alloc(cap, true), *actions = ink_app_alloc(cap, true); int result = -1;
    if (M.field) {
        snprintf(M.save_args, sizeof M.save_args, "{\"id\":%u,\"draft\":%u,\"page\":\"%s\"}", (unsigned)M.id, (unsigned)M.note.draft, M.note.page);
        M.actions[3] = M.mode == FAIL ? (ink_ui_action_spec_t){.id = "retry", .label = "重试", .capability = "memo.retry", .enabled = true} : (ink_ui_action_spec_t){.id = "save", .label = "记下当前草稿", .capability = "memo.save", .args_json = M.save_args, .enabled = true};
        M.model.actions = M.mode == FAIL ? M.actions + 3 : M.actions; M.model.action_count = M.mode == FAIL ? 1 : 4;
    }
    ink_ui_page_spec_t model = M.model; ink_ui_action_spec_t listed[2] = {M.actions[0]}; /* the list also offers 说一句 */
    if (M.list && (M.mode == LIST || M.mode == UNDO) && M.model.action_count < 2) {
        listed[M.model.action_count] = (ink_ui_action_spec_t){.id = "record", .label = "说一句", .capability = "memo.record", .enabled = true};
        model.actions = listed; model.action_count++;
    }
    if (body && actions && ink_ui_semantics_snapshot(&model, actions, cap) > 0) {
        int n = M.field ? ink_textfield_long_snapshot(M.field, body, cap) : M.list ? ink_ui_list_snapshot(M.list, body, cap) : 0;
        if (!n) snprintf(body, cap, "{}");
        if (n >= 0) {
            ink_jw_t w; ink_jw_init(&w, out, cap);
            ink_jw_raw(&w, "{\"view\":"); ink_jw_str(&w, PAGES[M.mode].name);
            ink_jw_raw(&w, ",\"content_truncated\":"); ink_jw_raw(&w, M.mode == CHECKLIST && clipped(M.note.body) ? "true,\"read_full\":\"memo.edit\"" : "false");
            ink_jw_raw(&w, ",\"status\":"); ink_jw_str(&w, M.notice ? lv_label_get_text(M.notice) : "");
            ink_jw_raw(&w, ",\"content\":"); ink_jw_raw(&w, body); ink_jw_raw(&w, ",\"controls\":"); ink_jw_raw(&w, actions); ink_jw_raw(&w, "}");
            result = w.overflow ? -1 : (int)w.len;
        }
    }
    ink_app_free(actions); ink_app_free(body); if (result < 0 && cap) out[0] = 0; return result;
}
static bool page_key(ink_key_t key, bool lng) { return M.list && ink_ui_list_key(M.list, key, lng); }
#define PAGE(name_) {.name = name_, .app = "memo", .create = create, .destroy = destroy, .snapshot = snapshot, .settle = settle, .on_key = page_key}
static const ink_activity_t PAGES[] = {PAGE("Memo"), PAGE("MemoDetail"), PAGE("MemoEdit"), PAGE("MemoChecklist"), PAGE("MemoUndo"), PAGE("MemoSaveFail"), PAGE("MemoRecord"), PAGE("MemoPending")};
static const ink_app_def_t APP = {.id = "memo", .title = "备忘", .icon = INK_UI_ICON(memo), .entry = &PAGES[LIST], .system_app = true, .child_ok = true};
#define ID "\"id\":{\"type\":\"integer\",\"minimum\":1,\"maximum\":4294967295}"
#define ID_ARGS "{\"type\":\"object\",\"properties\":{" ID "},\"required\":[\"id\"]}"
static const ink_cap_t CAPS[] = {
    {.id = "memo.save", .title = "记下当前成员的长文草稿", .input = "{\"type\":\"object\",\"properties\":{" ID ",\"draft\":{\"type\":\"integer\",\"minimum\":1},\"page\":{\"type\":\"string\",\"enum\":[\"MemoEdit\",\"MemoSaveFail\"]}},\"required\":[\"id\",\"draft\",\"page\"]}", .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .exec = INK_EXEC_BG, .run = mutate},
    {.id = "memo.delete", .title = "删除备忘（可撤销）", .input = ID_ARGS, .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .exec = INK_EXEC_BG, .run = mutate},
    {.id = "memo.undo", .title = "撤销删除备忘", .input = ID_ARGS, .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .exec = INK_EXEC_BG, .run = mutate},
    {.id = "memo.convert", .title = "逐行转成清单（最多 32 项）", .input = ID_ARGS, .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .exec = INK_EXEC_BG, .run = mutate},
    {.id = "memo.check", .title = "勾选或取消清单项", .input = "{\"type\":\"object\",\"properties\":{" ID ",\"index\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":31}},\"required\":[\"id\",\"index\"]}", .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .exec = INK_EXEC_BG, .run = mutate},
    {.id = "memo.open", .title = "查看备忘", .input = ID_ARGS, .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "memo.edit", .title = "写一条或编辑备忘", .input = "{\"type\":\"object\",\"properties\":{" ID "}}", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "memo.next", .title = "备忘下一页", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "memo.prev", .title = "备忘上一页", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "memo.retry", .title = "重试记下备忘", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "memo.record", .title = "说一句：录一条语音备忘（联网后整理成文字）", .output = "{\"type\":\"object\"}", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = record},
    {.id = "memo.play", .title = "播放语音备忘的录音", .input = ID_ARGS, .effect = INK_FX_NAVIGATE, .child_ok = true, .exec = INK_EXEC_BG, .run = play},
    {.id = "memo.voice", .title = "把录好的音存成语音备忘，或填上中枢整理的文字", .input = "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"integer\",\"minimum\":0},\"path\":{\"type\":\"string\",\"maxLength\":127},\"ms\":{\"type\":\"integer\",\"minimum\":0}}}",
     .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .expose = INK_EXPOSE_APP, .exec = INK_EXEC_BG, .run = mutate}
};
int ink_memo_init(void) {
    return ink_db_register(&MEMOS) || ink_cap_register(CAPS, sizeof CAPS / sizeof *CAPS) || ink_app_register(&APP) ||
        ink_cap_subscribe("audio.record_done", recorded, NULL, INK_EXEC_BG) || ink_state_watch("net.online", kick, NULL, INK_EXEC_BG) ? -1 : 0;
}
