/*
 * The data store on the soft bus: collections that say so are readable and
 * writable by the AI, the hub and other apps through capabilities, always those
 * of the current member. Credentials and the like never say so.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ink/osal.h"
#include "ink/sys/cap.h"
#include "ink/sys/data.h"
#include "ink/sys/json.h"

#define VALUE_CAP (INK_DB_RECORD_MAX + 1)
#define RESULT_CAP (2 * INK_DB_RECORD_MAX + 512) /* a record goes in as it is: a page has room for at least one, whole */
#define UNDO_MS 10000

static const char *const SRC_NAME[] = {"touch", "key", "ai", "system", "app", "remote", "confirmed"};

static const ink_db_def_t *open_def(ink_call_t *c, bool write, ink_db_t **out) {
    char name[INK_DB_NAME_MAX];
    ink_args_get_str(&c->args, "collection", name, sizeof name);
    const ink_db_def_t *d = ink_db_find_def(name);
    if (!d || !d->json || !(write ? d->ai_write : d->ai_read)) {
        c->rc.status = d ? INK_RC_REJECTED : INK_RC_INVALID;
        snprintf(c->rc.detail, sizeof c->rc.detail, "%s", d ? "this collection is not open to this" : "no such collection");
        return NULL;
    }
    *out = ink_db_open(d, NULL);
    if (!*out) {
        c->rc.status = INK_RC_FAILED;
        snprintf(c->rc.detail, sizeof c->rc.detail, "cannot open the collection");
        return NULL;
    }
    return d;
}

static void rec_json(ink_jw_t *w, const ink_rec_t *r, const char *value) {
    ink_jw_raw(w, "{\"id\":");
    ink_jw_int(w, r->id);
    ink_jw_raw(w, ",\"sort\":");
    ink_jw_int(w, r->sort);
    ink_jw_raw(w, ",\"updated_s\":");
    ink_jw_int(w, r->updated_s);
    ink_jw_raw(w, ",\"by\":");
    ink_jw_str(w, r->src < sizeof SRC_NAME / sizeof SRC_NAME[0] ? SRC_NAME[r->src] : "?");
    ink_jw_raw(w, ",\"value\":");
    ink_jw_raw(w, value[0] ? value : "null");
    ink_jw_raw(w, "}");
}

static void cap_collections(ink_call_t *c) { /* exec.bg: opening one may wait for the flash */
    char *out = osal_malloc(RESULT_CAP, OSAL_MEM_EXTERNAL);
    if (!out) {
        c->rc.status = INK_RC_FAILED;
        snprintf(c->rc.detail, sizeof c->rc.detail, "out of memory");
        return;
    }
    ink_jw_t w;
    ink_jw_init(&w, out, RESULT_CAP);
    ink_jw_raw(&w, "[");
    bool first = true;
    for (int i = 0;; i++) {
        const ink_db_def_t *d = ink_db_def_at(i);
        if (!d) break;
        if (!d->json || !d->ai_read) continue;
        ink_db_t *db = ink_db_open(d, NULL);
        ink_jw_raw(&w, first ? "{\"name\":" : ",{\"name\":");
        first = false;
        ink_jw_str(&w, d->name);
        ink_jw_raw(&w, ",\"title\":");
        ink_jw_str(&w, d->title ? d->title : d->name);
        ink_jw_raw(&w, ",\"count\":");
        ink_jw_int(&w, db ? ink_db_count(db) : 0);
        ink_jw_raw(&w, d->ai_write ? ",\"writable\":true}" : "}");
        ink_db_close(db);
    }
    ink_jw_raw(&w, "]");
    if (w.overflow) {
        c->rc.status = INK_RC_FAILED;
        snprintf(c->rc.detail, sizeof c->rc.detail, "too long");
    } else ink_cap_result(c, out);
    osal_free(out);
}

/* Cursor as text: "<sort>:<id>". */
static bool parse_cursor(const char *s, ink_db_cursor_t *cur) {
    char *end;
    long long sort = strtoll(s, &end, 10);
    if (end == s || *end != ':') return false;
    cur->sort = sort;
    cur->id = (uint32_t)strtoul(end + 1, &end, 10);
    cur->started = true;
    return !*end;
}

static void cap_list(ink_call_t *c) {
    ink_db_t *db;
    if (!open_def(c, false, &db)) return;
    char *out = osal_malloc(RESULT_CAP + VALUE_CAP, OSAL_MEM_EXTERNAL);
    if (!out) {
        ink_db_close(db);
        c->rc.status = INK_RC_FAILED;
        snprintf(c->rc.detail, sizeof c->rc.detail, "out of memory");
        return;
    }
    char *value = out + RESULT_CAP;
    int64_t limit = 20;
    ink_args_get_int(&c->args, "limit", &limit);
    bool oldest = false;
    ink_args_get_bool(&c->args, "oldest_first", &oldest);
    ink_db_cursor_t cur = {.desc = !oldest};
    char cursor[40];
    if (ink_args_get_str(&c->args, "cursor", cursor, sizeof cursor) > 0 && !parse_cursor(cursor, &cur)) {
        c->rc.status = INK_RC_INVALID;
        snprintf(c->rc.detail, sizeof c->rc.detail, "bad cursor");
        osal_free(out);
        ink_db_close(db);
        return;
    }
    ink_jw_t w;
    ink_jw_init(&w, out, RESULT_CAP - 64);
    ink_jw_raw(&w, "{\"records\":[");
    bool more = false, first = true;
    ink_db_cursor_t last = cur;
    for (int n = 0;; n++) {
        ink_rec_t r;
        ink_db_cursor_t try_cur = last;
        int got = ink_db_next(db, &try_cur, &r, value, VALUE_CAP - 1);
        if (got <= 0) break;
        if (n >= limit) {
            more = true;
            break;
        }
        size_t before = w.len;
        if (!first) ink_jw_raw(&w, ",");
        value[r.len < VALUE_CAP - 1 ? r.len : VALUE_CAP - 1] = 0;
        rec_json(&w, &r, value);
        if (w.overflow) { /* the next page starts with this one */
            w.overflow = false;
            w.len = before;
            out[w.len] = 0;
            more = true;
            break;
        }
        first = false;
        last = try_cur;
    }
    ink_jw_raw(&w, "]");
    if (more && last.started) snprintf(out + w.len, 64, ",\"next\":\"%lld:%u\"}", (long long)last.sort, (unsigned)last.id);
    else snprintf(out + w.len, 64, "}");
    ink_cap_result(c, out);
    osal_free(out);
    ink_db_close(db);
}

static void cap_get(ink_call_t *c) {
    ink_db_t *db;
    if (!open_def(c, false, &db)) return;
    int64_t id = 0;
    ink_args_get_int(&c->args, "id", &id);
    char *out = osal_malloc(RESULT_CAP, OSAL_MEM_EXTERNAL);
    char *value = out ? osal_malloc(VALUE_CAP, OSAL_MEM_EXTERNAL) : NULL;
    ink_rec_t r;
    if (!value) {
        c->rc.status = INK_RC_FAILED;
        snprintf(c->rc.detail, sizeof c->rc.detail, "out of memory");
    } else if (ink_db_get(db, (uint32_t)id, &r, value, VALUE_CAP - 1) != INK_DB_OK) {
        c->rc.status = INK_RC_FAILED;
        snprintf(c->rc.detail, sizeof c->rc.detail, "no such record");
    } else {
        value[r.len < VALUE_CAP - 1 ? r.len : VALUE_CAP - 1] = 0;
        ink_jw_t w;
        ink_jw_init(&w, out, RESULT_CAP);
        rec_json(&w, &r, value);
        if (!w.overflow) ink_cap_result(c, out);
        else {
            c->rc.status = INK_RC_FAILED;
            snprintf(c->rc.detail, sizeof c->rc.detail, "too long");
        }
    }
    osal_free(value);
    osal_free(out);
    ink_db_close(db);
}

static void put_result(ink_call_t *c, const ink_db_def_t *d, int r, uint32_t id) {
    if (r != INK_DB_OK) {
        c->rc.status = r == INK_DB_ERR_FULL ? INK_RC_REJECTED : INK_RC_FAILED;
        snprintf(c->rc.detail, sizeof c->rc.detail, "%s", r == INK_DB_ERR_FULL ? "the collection is full" : "not written");
        return;
    }
    snprintf(c->rc.object, sizeof c->rc.object, "%.32s#%u", d->name, (unsigned)id);
}

static void cap_put(ink_call_t *c) {
    ink_db_t *db;
    const ink_db_def_t *d = open_def(c, true, &db);
    if (!d) return;
    const ink_arg_t *v = ink_args_find(&c->args, "value");
    int64_t id = 0, sort = 0;
    ink_args_get_int(&c->args, "id", &id);
    bool has_sort = ink_args_get_int(&c->args, "sort", &sort);
    if (!has_sort) {
        int64_t wall = osal_wall_us();
        sort = wall > 0 ? wall / 1000000 : 0;
    }
    bool existed = id && ink_db_get(db, (uint32_t)id, NULL, NULL, 0) == INK_DB_OK;
    ink_db_write_t wr = {.id = (uint32_t)id, .sort = sort, .data = c->args.text + v->off, .len = v->len, .src = (uint8_t)c->src};
    uint32_t got = 0;
    int r = ink_db_put(db, &wr, &got);
    put_result(c, d, r, got);
    if (r == INK_DB_OK) {
        c->rc.revision = ink_db_last_ver(db);
        if (!existed) { /* a new record can be taken back; an overwrite could not be restored from here */
            ink_args_t undo;
            ink_args_init(&undo);
            ink_args_str(&undo, "collection", d->name);
            ink_args_int(&undo, "id", got);
            ink_cap_undoable(c, "data.delete", &undo, UNDO_MS);
        }
    }
    ink_db_close(db);
}

static void cap_delete(ink_call_t *c) {
    ink_db_t *db;
    const ink_db_def_t *d = open_def(c, true, &db);
    if (!d) return;
    int64_t id = 0;
    ink_args_get_int(&c->args, "id", &id);
    int r = ink_db_del(db, (uint32_t)id, (uint8_t)c->src);
    if (r == INK_DB_OK) snprintf(c->rc.object, sizeof c->rc.object, "%.32s#%u", d->name, (unsigned)id);
    else {
        c->rc.status = INK_RC_FAILED;
        snprintf(c->rc.detail, sizeof c->rc.detail, "no such record");
    }
    ink_db_close(db);
}

#define COLL "\"collection\":{\"type\":\"string\",\"maxLength\":39}"

static const ink_cap_t DATA_CAPS[] = {
    {.id = "data.collections",
     .title = "有哪些数据集合",
     .output = "{\"type\":\"array\"}",
     .effect = INK_FX_READ,
     .child_ok = true,
     .exec = INK_EXEC_BG,
     .run = cap_collections},
    {.id = "data.list",
     .title = "分页读一个数据集合（默认最新的在前）",
     .input = "{\"type\":\"object\",\"properties\":{" COLL ",\"limit\":{\"type\":\"integer\",\"minimum\":1,\"maximum\":50},"
              "\"cursor\":{\"type\":\"string\",\"maxLength\":39},\"oldest_first\":{\"type\":\"boolean\"}},\"required\":[\"collection\"]}",
     .output = "{\"type\":\"object\",\"properties\":{\"records\":{\"type\":\"array\"},\"next\":{\"type\":\"string\"}}}",
     .effect = INK_FX_READ,
     .child_ok = true,
     .exec = INK_EXEC_BG,
     .run = cap_list},
    {.id = "data.get",
     .title = "读一条数据",
     .input = "{\"type\":\"object\",\"properties\":{" COLL ",\"id\":{\"type\":\"integer\",\"minimum\":1}},\"required\":[\"collection\",\"id\"]}",
     .output = "{\"type\":\"object\"}",
     .effect = INK_FX_READ,
     .child_ok = true,
     .exec = INK_EXEC_BG,
     .run = cap_get},
    {.id = "data.put",
     .title = "写一条数据（新的可以撤销）",
     .input = "{\"type\":\"object\",\"properties\":{" COLL ",\"id\":{\"type\":\"integer\",\"minimum\":1},\"sort\":{\"type\":\"integer\"},"
              "\"value\":{\"type\":\"object\"}},\"required\":[\"collection\",\"value\"]}",
     .effect = INK_FX_LOCAL_WRITE,
     .child_ok = true,
     .exec = INK_EXEC_BG,
     .run = cap_put},
    {.id = "data.delete",
     .title = "删除一条数据",
     .ask = "从 {collection} 删除第 {id} 条",
     .input = "{\"type\":\"object\",\"properties\":{" COLL ",\"id\":{\"type\":\"integer\",\"minimum\":1}},\"required\":[\"collection\",\"id\"]}",
     .effect = INK_FX_LOCAL_DESTRUCTIVE,
     .child_ok = true,
     .exec = INK_EXEC_BG,
     .run = cap_delete},
    {.id = "data.write_failed", .title = "数据没能写进存储", .form = INK_FORM_EVENT, .effect = INK_FX_READ, .child_ok = true},
};

int ink_data_caps_init(void) { return ink_cap_register(DATA_CAPS, (int)(sizeof DATA_CAPS / sizeof DATA_CAPS[0])); }
