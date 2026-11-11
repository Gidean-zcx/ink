/* Bounded member conversations, using the existing collection and executors. */
#include <stdio.h>
#include <string.h>
#include "ink/osal.h"
#include "ink/probe.h"
#include "ink/sys/ai.h"
#include "ink/sys/cap.h"
#include "ink/sys/data.h"
#include "ink/sys/json.h"
#include "ink/sys/layout.h"
#include "ink/sys/members.h"
#include "ink/sys/time.h"

static const ink_db_def_t HISTORY = {.name = "ai.history", .title = "对话记录", .schema = 1,
    .scope = INK_DB_MEMBER, .json = true, .ai_read = true, .max_records = 50,
    .max_bytes = 128 * 1024, .drop_oldest = true};
typedef struct { ink_exec_job_t job; char member[INK_MEMBER_ID_MAX]; int64_t at; char value[INK_DB_RECORD_MAX + 1]; } saved_t;
static void save(void *arg) {
    saved_t *s = arg; ink_db_t *db = ink_db_open_member(&HISTORY, s->member);
    if (db) {
        ink_db_cursor_t cur = {.desc = true}; ink_rec_t rec;
        int64_t sort = ink_db_next(db, &cur, &rec, NULL, 0) == 1 ? rec.sort + 1 : 1;
        ink_db_write_t w = {.sort = sort, .data = s->value, .len = strlen(s->value), .src = INK_SRC_SYSTEM};
        if (ink_db_put(db, &w, NULL)) ink_probe_count("ai.history.failed");
        else ink_cap_emit_for("ai.history.changed", NULL, s->member);
        ink_db_close(db);
    }
    osal_free(s);
}
void ink_ai_history_save(const char *member, const char *question, const char *answer, const char *page) {
    if (!answer[0]) return;
    saved_t *s = osal_calloc(1, sizeof *s, OSAL_MEM_EXTERNAL);
    if (!s) return;
    snprintf(s->member, sizeof s->member, "%s", member); s->at = ink_time_now_s();
    ink_jw_t w; ink_jw_init(&w, s->value, sizeof s->value);
    ink_jw_raw(&w, "{\"at\":"); ink_jw_int(&w, s->at);
    ink_jw_raw(&w, ",\"question\":"); ink_jw_str(&w, question);
    ink_jw_raw(&w, ",\"page\":"); ink_jw_str(&w, page);
    ink_jw_raw(&w, ",\"answer\":");
    size_t n = 0, bytes = 0, room = w.cap > w.len + 128 ? w.cap - w.len - 128 : 0;
    while (answer[n]) {
        unsigned char ch = (unsigned char)answer[n];
        size_t cost = ch < 32 ? 6 : ch == '"' || ch == '\\' ? 2 : 1;
        if (bytes + cost > room) break;
        bytes += cost; n++;
    }
    while (n && ((unsigned char)answer[n] & 0xc0) == 0x80) n--;
    ink_jw_strn(&w, answer, n);
    if (answer[n] && !w.overflow) { w.len--; s->value[w.len] = 0; ink_jw_raw(&w, "\\n记录太长，只保存了前面部分。\""); }
    ink_jw_raw(&w, "}");
    if (w.overflow || ink_exec_post(INK_EXEC_BG, &s->job, save, s)) osal_free(s);
}
typedef struct { ink_exec_job_t job; ink_call_t *call; uint32_t epoch; char raw[INK_DB_RECORD_MAX + 1]; } opened_t;
static void show(void *arg) {
    opened_t *p = arg; ink_call_t *c = p->call;
    ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL);
    char *answer = osal_malloc(INK_DB_RECORD_MAX + 1, OSAL_MEM_EXTERNAL);
    if (p->epoch != ink_member_epoch() || strcmp(ink_cap_call_member(c), ink_member_current()->id) || !ink_member_space_open()) c->rc.status = INK_RC_STALE;
    else if (!j || !answer || ink_json_parse(j, p->raw, strlen(p->raw)) || ink_json_str(j, ink_json_find(j, 0, "answer"), answer, INK_DB_RECORD_MAX + 1) < 0) c->rc.status = INK_RC_FAILED;
    else ink_ai_show_answer(answer);
    osal_free(answer); osal_free(j); osal_free(p); ink_cap_done(c);
}
static void history(ink_call_t *c) {
    ink_db_t *db = ink_db_open_member(&HISTORY, ink_cap_call_member(c));
    if (!db) { c->rc.status = INK_RC_FAILED; return; }
    int64_t id = 0, offset = 0; ink_args_get_int(&c->args, "id", &id); ink_args_get_int(&c->args, "offset", &offset);
    ink_rec_t rec; ink_db_cursor_t cur = {.desc = true};
    if (!strcmp(c->cap->id, "ai.history.open")) {
        opened_t *p = osal_calloc(1, sizeof *p, OSAL_MEM_EXTERNAL);
        if (!p || ink_db_get(db, (uint32_t)id, &rec, p->raw, INK_DB_RECORD_MAX)) { osal_free(p); c->rc.status = INK_RC_INVALID; }
        else { p->call = c; p->epoch = ink_member_epoch(); ink_cap_defer(c); if (ink_exec_post(INK_EXEC_UI, &p->job, show, p)) { osal_free(p); c->rc.status = INK_RC_FAILED; ink_cap_done(c); } }
    } else if (!strcmp(c->cap->id, "ai.history.list")) {
        char *out = osal_malloc(4 * INK_DB_RECORD_MAX + 256, OSAL_MEM_EXTERNAL);
        char *raw = osal_malloc(INK_DB_RECORD_MAX + 1, OSAL_MEM_EXTERNAL);
        if (!out || !raw) c->rc.status = INK_RC_FAILED;
        else {
            ink_jw_t w; ink_jw_init(&w, out, 4 * INK_DB_RECORD_MAX + 256); ink_jw_raw(&w, "{\"total\":"); ink_jw_int(&w, ink_db_count(db)); ink_jw_raw(&w, ",\"entries\":[");
            int count = 0;
            while (ink_db_next(db, &cur, &rec, raw, INK_DB_RECORD_MAX) == 1) {
                if (offset-- > 0) continue;
                if (count == 4) break;
                if (count++) ink_jw_raw(&w, ",");
                raw[rec.len] = 0; ink_jw_raw(&w, "{\"id\":"); ink_jw_int(&w, rec.id); ink_jw_raw(&w, ",\"value\":"); ink_jw_raw(&w, raw); ink_jw_raw(&w, "}");
            }
            ink_jw_raw(&w, "]}"); if (!w.overflow) ink_cap_result(c, out); else c->rc.status = INK_RC_FAILED;
        }
        osal_free(raw); osal_free(out);
    } else if (!strcmp(c->cap->id, "ai.history.clear")) {
        while (ink_db_next(db, &cur, &rec, NULL, 0) == 1) if (ink_db_del(db, rec.id, c->src)) { c->rc.status = INK_RC_FAILED; break; }
    } else if (ink_db_del(db, (uint32_t)id, c->src)) c->rc.status = INK_RC_INVALID;
    ink_db_close(db);
    if (c->rc.status == INK_RC_DONE && (strstr(c->cap->id, "delete") || strstr(c->cap->id, "clear"))) ink_cap_emit_for("ai.history.changed", NULL, ink_cap_call_member(c));
}
#define HISTORY_ID "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"integer\",\"minimum\":1,\"maximum\":4294967295}},\"required\":[\"id\"]}"
static const ink_cap_t CAPS[] = {
    {.id = "ai.history.list", .app = "ai", .title = "列出自己的对话记录，每页四条", .input = "{\"type\":\"object\",\"properties\":{\"offset\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":50}}}", .output = "{\"type\":\"object\"}", .effect = INK_FX_READ, .child_ok = true, .exec = INK_EXEC_BG, .run = history},
    {.id = "ai.history.open", .app = "ai", .title = "打开自己的一条对话记录", .input = HISTORY_ID, .effect = INK_FX_NAVIGATE, .child_ok = true, .exec = INK_EXEC_BG, .run = history},
    {.id = "ai.history.delete", .app = "ai", .title = "删除自己的一条对话记录", .input = HISTORY_ID, .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .exec = INK_EXEC_BG, .run = history},
    {.id = "ai.history.clear", .app = "ai", .title = "清空自己的全部对话记录", .ask = "清空全部对话记录，删了找不回。", .ask_by_hand = true, .effect = INK_FX_LOCAL_DESTRUCTIVE, .child_ok = true, .exec = INK_EXEC_BG, .run = history},
    {.id = "ai.history.changed", .app = "ai", .title = "自己的对话记录已改变", .form = INK_FORM_EVENT, .effect = INK_FX_READ, .child_ok = true}
};
int ink_ai_history_init(void) { return ink_db_register(&HISTORY) || ink_cap_register(CAPS, sizeof CAPS / sizeof *CAPS); }
