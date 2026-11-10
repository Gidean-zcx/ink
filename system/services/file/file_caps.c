/*
 * The file service on the soft bus (R-052, R-061): what the file app's buttons do, and what the AI and
 * other apps can do. Card work stays off the UI task; thumbnails use media. Long copies answer
 * "queued" with the job as their object; file.job_done tells the end. An app's own files and the
 * caches are not on offer.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ink/osal.h"
#include "ink/sys/cap.h"
#include "ink/sys/file.h"
#include "ink/sys/json.h"
#include "ink/sys/state.h"
#include "ink/sys/textinput.h"
#include "ink/sys/i18n.h"
#include "ink/sys/task.h"
#include "file_internal.h"

#define RESULT_CAP 4096
#define READ_MAX 2048
#define COPY_NOW_BYTES (256u * 1024u) /* a copy this small is waited for */
#define UNDO_MS 10000
#define BATCH_FAILURE_MAX 20
#define BATCH_RESULT_CAP (BATCH_FAILURE_MAX * (INK_FS_NAME_MAX * 6 + 3) + 128)

static bool offered(const char *path) { return path && strncmp(path, "/app", 4) && strncmp(path, "/cache", 6); }

static void refuse(ink_call_t *c, int err) {
    c->rc.status = err == INK_FS_ERR_DENIED ? INK_RC_REJECTED : err == INK_FS_ERR_PATH ? INK_RC_INVALID : INK_RC_FAILED;
    snprintf(c->rc.detail, sizeof c->rc.detail, "%s", ink_fs_err_name(err));
}

static bool arg_path(ink_call_t *c, const char *key, char *out, size_t cap) {
    if (ink_args_get_str(&c->args, key, out, cap) < 0 || !offered(out)) {
        refuse(c, INK_FS_ERR_PATH);
        return false;
    }
    return true;
}

static void cap_list(ink_call_t *c) {
    file_context_t context;
    if (!fs_context_for_call(&context, c)) return;
    char path[INK_FS_PATH_MAX];
    if (!arg_path(c, "path", path, sizeof path)) return;
    int64_t skip = 0, limit = 50;
    char cur[16];
    if (ink_args_get_str(&c->args, "cursor", cur, sizeof cur) > 0) skip = atoi(cur);
    ink_args_get_int(&c->args, "limit", &limit);
    ink_fs_dir_t *d;
    int r = fs_dir_open_for(&context, path, &d);
    if (r) {
        refuse(c, r);
        return;
    }
    char *out = osal_malloc(RESULT_CAP, OSAL_MEM_EXTERNAL);
    if (!out) {
        ink_fs_dir_close(d);
        refuse(c, INK_FS_ERR_IO);
        return;
    }
    ink_jw_t w;
    ink_jw_init(&w, out, RESULT_CAP - 32);
    ink_jw_raw(&w, "{\"entries\":[");
    ink_fs_stat_t st;
    int64_t i = 0, shown = 0;
    bool more = false, first = true;
    while ((r = ink_fs_dir_next(d, &st)) == 1) {
        if (i++ < skip) continue;
        if (shown >= limit) {
            more = true;
            break;
        }
        size_t before = w.len;
        if (!first) ink_jw_raw(&w, ",");
        ink_jw_raw(&w, "{\"name\":");
        ink_jw_str(&w, st.name);
        ink_jw_raw(&w, st.is_dir ? ",\"dir\":true" : ",\"size\":");
        if (!st.is_dir) ink_jw_int(&w, st.size);
        ink_jw_raw(&w, "}");
        if (w.overflow) {
            w.overflow = false;
            w.len = before;
            out[w.len] = 0;
            more = true;
            i--;
            break;
        }
        first = false;
        shown++;
    }
    ink_fs_dir_close(d);
    ink_jw_raw(&w, "]");
    if (more) snprintf(out + w.len, 32, ",\"next\":\"%lld\"}", (long long)i - (shown >= limit ? 1 : 0));
    else snprintf(out + w.len, 32, "}");
    if (r < 0) refuse(c, r);
    else ink_cap_result(c, out);
    osal_free(out);
}

static void cap_stat(ink_call_t *c) {
    file_context_t context;
    if (!fs_context_for_call(&context, c)) return;
    char path[INK_FS_PATH_MAX];
    if (!arg_path(c, "path", path, sizeof path)) return;
    ink_fs_stat_t st;
    int r = fs_stat_for(&context, path, &st);
    if (r) {
        refuse(c, r);
        return;
    }
    char out[160];
    snprintf(out, sizeof out, "{\"dir\":%s,\"size\":%lld}", st.is_dir ? "true" : "false", (long long)st.size);
    ink_cap_result(c, out);
}

static bool utf8_valid(const char *s, size_t n, size_t *complete) {
    size_t i = 0;
    while (i < n) {
        unsigned char b = (unsigned char)s[i];
        size_t len = b < 0x80 ? 1 : (b >> 5) == 6 ? 2 : (b >> 4) == 14 ? 3 : (b >> 3) == 30 ? 4 : 0;
        if (!len) return false;
        if (i + len > n) break; /* cut at the end of the buffer */
        for (size_t k = 1; k < len; k++)
            if (((unsigned char)s[i + k] & 0xC0) != 0x80) return false;
        i += len;
    }
    *complete = i;
    return true;
}

static void cap_read(ink_call_t *c) {
    file_context_t context;
    if (!fs_context_for_call(&context, c)) return;
    char path[INK_FS_PATH_MAX];
    if (!arg_path(c, "path", path, sizeof path)) return;
    int64_t off = 0, max = READ_MAX;
    ink_args_get_int(&c->args, "offset", &off);
    ink_args_get_int(&c->args, "max_bytes", &max);
    if (max > READ_MAX) max = READ_MAX;
    ink_fs_file_t *f;
    int r = fs_open_for(&context, path, INK_FS_READ, &f);
    if (r) {
        refuse(c, r);
        return;
    }
    char *buf = osal_malloc((size_t)max + 1, OSAL_MEM_EXTERNAL), *out = osal_malloc(READ_MAX * 2 + 128, OSAL_MEM_EXTERNAL);
    int64_t n = buf && out && ink_fs_seek(f, off, 0) == 0 ? ink_fs_read(f, buf, (size_t)max) : -1;
    ink_fs_close(f);
    size_t good = 0;
    if (n < 0) refuse(c, INK_FS_ERR_IO);
    else if (!utf8_valid(buf, (size_t)n, &good)) {
        c->rc.status = INK_RC_FAILED;
        snprintf(c->rc.detail, sizeof c->rc.detail, "not text: a binary file");
    } else {
        ink_jw_t w;
        ink_jw_init(&w, out, READ_MAX * 2 + 128);
        ink_jw_raw(&w, "{\"text\":");
        ink_jw_strn(&w, buf, good);
        ink_jw_raw(&w, ",\"next_offset\":");
        ink_jw_int(&w, off + (int64_t)good);
        ink_jw_raw(&w, n < max ? ",\"end\":true}" : "}"); /* a page cut before a character that did not fit is not the end */
        if (!w.overflow) ink_cap_result(c, out);
    }
    osal_free(buf);
    osal_free(out);
}

static void cap_write(ink_call_t *c) {
    file_context_t context;
    if (!fs_context_for_call(&context, c)) return;
    char path[INK_FS_PATH_MAX], text[INK_ARGS_TEXT];
    if (!arg_path(c, "path", path, sizeof path)) return;
    bool replace = false; int64_t draft = 0; char page[48];
    ink_args_get_bool(&c->args, "replace", &replace); ink_args_get_int(&c->args, "draft", &draft);
    if (!draft && !ink_args_find(&c->args, "text")) { refuse(c, INK_FS_ERR_PATH); return; }
    if (replace || draft) {
        char *body = osal_malloc(4096, OSAL_MEM_EXTERNAL); ink_fs_stat_t st;
        bool txt = strlen(path) > 4 && !strcmp(path + strlen(path) - 4, ".txt");
        int r = !body || !txt || (draft && (ink_args_get_str(&c->args, "page", page, sizeof page) < 0 ||
            ink_text_draft_read(context.member, page, (uint32_t)draft, body, 4096) < 0)) ? INK_FS_ERR_PATH : 0;
        if (!draft && body) ink_args_get_str(&c->args, "text", body, 4096);
        if (!r && !replace && !fs_stat_for(&context, path, &st)) r = INK_FS_ERR_EXISTS;
        if (!r) r = fs_write_atomic_for(&context, path, body, strlen(body));
        osal_free(body); if (r) refuse(c, r); return;
    }
    ink_args_get_str(&c->args, "text", text, sizeof text);
    bool append = false;
    ink_args_get_bool(&c->args, "append", &append);
    ink_fs_file_t *f;
    int r = fs_open_for(&context, path, append ? (INK_FS_APPEND | INK_FS_CREATE) : (INK_FS_WRITE | INK_FS_CREATE | INK_FS_EXCL), &f);
    if (r) {
        refuse(c, r);
        return;
    }
    size_t n = strlen(text);
    int64_t w = ink_fs_write(f, text, n);
    uint32_t revision = ink_fs_file_revision(f);
    int cl = ink_fs_close(f);
    if (w != (int64_t)n || cl) {
        refuse(c, w < 0 ? (int)w : INK_FS_ERR_IO);
        return;
    }
    snprintf(c->rc.object, sizeof c->rc.object, "file:%.40s", path);
    c->rc.revision = revision;
    if (!append && strlen(path) <= 40) { /* a new file can be taken back; "object" must name it whole */
        ink_args_t undo;
        ink_args_init(&undo);
        ink_args_str(&undo, "path", path);
        ink_args_int(&undo, "if_revision", revision);
        ink_cap_undoable(c, "file.delete", &undo, UNDO_MS);
    }
}

/* One invocation, continued by the existing file job; no worker or retry queue. */
typedef struct {
    ink_call_t *call; file_context_t context; uint32_t ids[INK_FS_INDEX_MAX];
    size_t count, at, failed; int op, task; bool last_ok; char to[INK_FS_PATH_MAX], path[INK_FS_PATH_MAX], *out; ink_jw_t w;
} batch_t;
static void batch_next(batch_t *b);
static void batch_publish(batch_t *b, bool ok) {
    ink_args_t a; ink_args_init(&a); ink_args_str(&a, "path", b->path); ink_args_bool(&a, "ok", ok); ink_args_int(&a, "done", b->at); ink_args_int(&a, "total", b->count); ink_cap_emit_for("file.batch_progress", &a, b->context.member);
}
static void batch_result(batch_t *b, int err) {
    if (err && b->failed < BATCH_FAILURE_MAX) {
        if (b->failed) ink_jw_raw(&b->w, ",");
        const char *name = strrchr(b->path, '/');
        ink_jw_str(&b->w, name ? name + 1 : b->path);
    }
    b->failed += err != 0; b->last_ok = !err; b->at++;
    ink_task_progress(b->task, (int)(b->at * 100 / b->count), NULL);
    if (b->at < b->count) batch_publish(b, !err);
}
static void batch_ended(void *ctx, const ink_fs_job_status_t *st) {
    batch_t *b = ctx; batch_result(b, st->state == INK_FS_JOB_DONE ? 0 : st->err ? st->err : INK_FS_ERR_IO); batch_next(b);
}
static void batch_next(batch_t *b) {
    while (b->at < b->count) {
        ink_fs_entry_t e; uint32_t id = b->ids[b->at]; char to[INK_FS_PATH_MAX];
        int r = fs_index_for(&b->context, NULL, INK_FS_ANY, &id, &e);
        snprintf(b->path, sizeof b->path, "#%u", (unsigned)id);
        if (r == 1) {
            memcpy(b->path, e.path, sizeof b->path); const char *name = strrchr(e.path, '/');
            r = b->op != 1 && snprintf(to, sizeof to, "%s/%s", b->to, name + 1) >= (int)sizeof to ? INK_FS_ERR_PATH : 0;
            if (!r) r = b->op == 2 ? fs_move_for(&b->context, e.path, to, 0, NULL) : fs_job_begin_for(&b->context, b->op, e.path, b->op ? NULL : to);
            if (b->op != 2 && r >= 0) { ink_fs_job_on_end(r, batch_ended, b); return; }
        } else r = r < 0 ? r : INK_FS_ERR_NOT_FOUND;
        batch_result(b, r);
    }
    ink_jw_raw(&b->w, "],\"total\":"); ink_jw_int(&b->w, (int64_t)b->count);
    ink_jw_raw(&b->w, ",\"succeeded\":"); ink_jw_int(&b->w, (int64_t)(b->count - b->failed));
    ink_jw_raw(&b->w, ",\"failed\":"); ink_jw_int(&b->w, (int64_t)b->failed);
    ink_jw_raw(&b->w, ",\"more\":"); ink_jw_int(&b->w, b->failed > BATCH_FAILURE_MAX ? (int64_t)(b->failed - BATCH_FAILURE_MAX) : 0);
    ink_jw_raw(&b->w, "}");
    b->call->rc.status = b->failed ? INK_RC_PARTIAL : INK_RC_DONE;
    snprintf(b->call->rc.say, sizeof b->call->rc.say, TR("完成 %zu/%zu，失败 %zu"), b->count - b->failed, b->count, b->failed);
    if (b->failed > BATCH_FAILURE_MAX) {
        size_t n = strlen(b->call->rc.say);
        snprintf(b->call->rc.say + n, sizeof b->call->rc.say - n, TR("，还有 %zu 个"), b->failed - BATCH_FAILURE_MAX);
    }
    ink_task_end(b->task, b->failed ? INK_TASK_FAILED : INK_TASK_DONE, b->call->rc.say, NULL);
    batch_publish(b, b->last_ok);
    ink_cap_result(b->call, b->out); ink_cap_done(b->call); osal_free(b->out); osal_free(b);
}
static void cap_batch(ink_call_t *c) {
    batch_t *b = osal_calloc(1, sizeof *b, OSAL_MEM_EXTERNAL); ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL);
    if (!b || !j) { osal_free(b); osal_free(j); refuse(c, INK_FS_ERR_IO); return; }
    char ids[INK_ARGS_TEXT], op[8], path[INK_FS_PATH_MAX]; b->call = c;
    bool ok = fs_context_for_call(&b->context, c);
    b->op = !strcmp(c->cap->id, "file.delete_many") ? 1 : ink_args_get_str(&c->args, "op", op, sizeof op) > 0 && !strcmp(op, "move") ? 2 : 0;
    if (b->op != 1) ok = ok && arg_path(c, "to", b->to, sizeof b->to);
    if (ink_args_get_str(&c->args, "ids", ids, sizeof ids) > 0 && !ink_json_parse(j, ids, strlen(ids)) && j->tok[0].type == 2) {
        for (int t = 1; t < j->ntok && b->count < INK_FS_INDEX_MAX; t++) {
            int64_t id = 0; if (!ink_json_int(j, t, &id) || id < 1 || id > UINT32_MAX) { ok = false; break; } b->ids[b->count++] = (uint32_t)id;
        }
    } else if (ink_args_get_str(&c->args, "all_path", path, sizeof path) > 0 && offered(path)) {
        uint32_t cursor = 0; ink_fs_entry_t e; int r;
        while ((r = fs_index_for(&b->context, path, INK_FS_ANY, &cursor, &e)) == 1 && b->count < INK_FS_INDEX_MAX)
            if ((size_t)(strrchr(e.path, '/') - e.path) == strlen(path)) b->ids[b->count++] = e.id;
        ok = ok && r >= 0;
    }
    osal_free(j);
    if (!ok || !b->count) { osal_free(b); if (c->rc.status == INK_RC_DONE) refuse(c, INK_FS_ERR_PATH); return; }
    b->out = osal_malloc(BATCH_RESULT_CAP, OSAL_MEM_EXTERNAL);
    if (!b->out) { osal_free(b); refuse(c, INK_FS_ERR_IO); return; }
    b->task = ink_task_begin_for(c->cap->id, TR("处理选中的文件"), b->context.member, NULL, NULL);
    ink_jw_init(&b->w, b->out, BATCH_RESULT_CAP); ink_jw_raw(&b->w, "{\"failures\":["); ink_cap_defer(c); batch_publish(b, true); batch_next(b);
}

static void cap_mkdir(ink_call_t *c) {
    file_context_t context;
    if (!fs_context_for_call(&context, c)) return;
    char path[INK_FS_PATH_MAX];
    if (!arg_path(c, "path", path, sizeof path)) return;
    ink_fs_stat_t st; int r = fs_stat_for(&context, path, &st);
    r = !r ? INK_FS_ERR_EXISTS : r == INK_FS_ERR_NOT_FOUND ? fs_mkdir_for(&context, path) : r;
    if (r) refuse(c, r);
    else snprintf(c->rc.object, sizeof c->rc.object, "file:%.40s", path);
}

static void cap_move(ink_call_t *c) {
    file_context_t context;
    if (!fs_context_for_call(&context, c)) return;
    char from[INK_FS_PATH_MAX], to[INK_FS_PATH_MAX];
    if (!arg_path(c, "from", from, sizeof from) || !arg_path(c, "to", to, sizeof to)) return;
    int64_t expected = 0;
    ink_args_get_int(&c->args, "if_revision", &expected);
    uint32_t revision = 0;
    int r = fs_move_for(&context, from, to, (uint32_t)expected, &revision);
    if (r) {
        if (expected) c->rc.status = INK_RC_STALE; else refuse(c, r);
        return;
    }
    c->rc.revision = revision;
    snprintf(c->rc.object, sizeof c->rc.object, "file:%.40s", to);
    ink_args_t undo;
    ink_args_init(&undo);
    ink_args_str(&undo, "from", to);
    ink_args_str(&undo, "to", from);
    ink_args_int(&undo, "if_revision", revision);
    if (strlen(to) <= 40) ink_cap_undoable(c, "file.move", &undo, UNDO_MS);
}

static void job_ended(void *ctx, const ink_fs_job_status_t *st) { /* exec.bg */
    ink_call_t *c = ctx;
    if (st->state != INK_FS_JOB_DONE) refuse(c, st->err ? st->err : INK_FS_ERR_IO);
    ink_cap_done(c);
}

/* Started as a job, which needs this executor, so it is not waited for here: a small one answers when it
 * ends, a large one answers "queued" now. */
static void answer_job(ink_call_t *c, int id, uint64_t size) {
    snprintf(c->rc.object, sizeof c->rc.object, "file-job:%d", id);
    if (size > COPY_NOW_BYTES) {
        c->rc.status = INK_RC_QUEUED;
        snprintf(c->rc.detail, sizeof c->rc.detail, "running; file.job_done tells the end");
        return;
    }
    ink_cap_defer(c);
    ink_fs_job_on_end(id, job_ended, c);
}

static void cap_copy(ink_call_t *c) {
    file_context_t context;
    if (!fs_context_for_call(&context, c)) return;
    char from[INK_FS_PATH_MAX], to[INK_FS_PATH_MAX];
    if (!arg_path(c, "from", from, sizeof from) || !arg_path(c, "to", to, sizeof to)) return;
    ink_fs_stat_t st;
    int r = fs_stat_for(&context, from, &st);
    if (!r) r = fs_job_begin_for(&context, 0, from, to);
    if (r < 0) {
        refuse(c, r);
        return;
    }
    answer_job(c, r, st.is_dir ? COPY_NOW_BYTES + 1 : (uint64_t)st.size);
}

static void cap_delete(ink_call_t *c) {
    file_context_t context;
    if (!fs_context_for_call(&context, c)) return;
    char path[INK_FS_PATH_MAX];
    if (!arg_path(c, "path", path, sizeof path)) return;
    int64_t revision = 0;
    if (ink_args_get_int(&c->args, "if_revision", &revision)) {
        if (fs_remove_for(&context, path, (uint32_t)revision) != INK_FS_OK) c->rc.status = INK_RC_STALE;
        return;
    }
    int r = fs_job_begin_for(&context, 1, path, NULL);
    if (r < 0) {
        refuse(c, r);
        return;
    }
    answer_job(c, r, 0);
}

static void cap_free(ink_call_t *c) {
    uint64_t f = 0, t = 0;
    int r = ink_fs_free(&f, &t);
    if (r) {
        refuse(c, r);
        return;
    }
    char out[80];
    snprintf(out, sizeof out, "{\"free\":%llu,\"total\":%llu}", (unsigned long long)f, (unsigned long long)t);
    ink_cap_result(c, out);
    ink_fs_refresh_state();
}

static void cap_usb_status(ink_call_t *c) {
    char out[64];
    int64_t connected = 0;
    ink_state_get_int(ink_state_find("usb.connected"), &connected);
    snprintf(out, sizeof out, "{\"connected\":%s,\"lent\":%s}", connected ? "true" : "false", ink_usb_disk_active() ? "true" : "false");
    ink_cap_result(c, out);
}

static void cap_usb_mount(ink_call_t *c) {
    int r = ink_usb_disk_start();
    if (r) refuse(c, r);
    else snprintf(c->rc.say, sizeof c->rc.say, TR("存储卡已经给电脑了，用完请先在电脑上弹出"));
}

static void cap_usb_unmount(ink_call_t *c) {
    int r = ink_usb_disk_stop();
    if (r) refuse(c, r);
    else snprintf(c->rc.say, sizeof c->rc.say, TR("存储卡回来了"));
}

#define PATH_ARG(name) "\"" name "\":{\"type\":\"string\",\"maxLength\":127}"
#define PATH_OBJ(name) "{\"type\":\"object\",\"properties\":{" PATH_ARG(name) "},\"required\":[\"" name "\"]}"

static void cap_index_query(ink_call_t *c, bool lookup) {
    static const char *const types[] = {"all", "other", "image", "audio", "text"};
    file_context_t context;
    if (!fs_context_for_call(&context, c)) return;
    char path[INK_FS_PATH_MAX], kind[12] = "all";
    int64_t id = 0, limit = 10; ink_fs_type_t type = INK_FS_ANY;
    if (lookup) ink_args_get_int(&c->args, "id", &id);
    else {
        if (!arg_path(c, "path", path, sizeof path)) return;
        ink_args_get_int(&c->args, "cursor", &id); ink_args_get_int(&c->args, "limit", &limit);
        if (ink_args_get_str(&c->args, "type", kind, sizeof kind) < 0) snprintf(kind, sizeof kind, "all");
        while (type <= INK_FS_TEXT && strcmp(kind, types[type + 1])) type++;
    }
    if (id < 0 || id > UINT32_MAX || (lookup && !id) || type > INK_FS_TEXT || limit < 1 || limit > 10) { refuse(c, INK_FS_ERR_PATH); return; }
    char *out = osal_malloc(RESULT_CAP, OSAL_MEM_EXTERNAL);
    if (!out) { refuse(c, INK_FS_ERR_IO); return; }
    ink_jw_t w; ink_jw_init(&w, out, RESULT_CAP);
    ink_jw_raw(&w, "{\"entries\":[");
    uint32_t cursor = (uint32_t)id; ink_fs_entry_t e; int r = 0, shown = 0;
    while (shown < (lookup ? 1 : limit) && (r = fs_index_for(&context, lookup ? NULL : path, type, &cursor, &e)) == 1) {
        if (shown++) ink_jw_raw(&w, ",");
        ink_jw_raw(&w, "{\"id\":"); ink_jw_int(&w, e.id);
        ink_jw_raw(&w, ",\"path\":"); ink_jw_str(&w, e.path);
        ink_jw_raw(&w, ",\"type\":"); ink_jw_str(&w, types[e.type + 1]);
        ink_jw_raw(&w, ",\"size\":"); ink_jw_int(&w, e.size); ink_jw_raw(&w, "}");
    }
    ink_jw_raw(&w, "]");
    if (!lookup && shown == limit) {
        uint32_t next = cursor;
        r = fs_index_for(&context, path, type, &next, &e);
        if (r == 1) { ink_jw_raw(&w, ",\"next\":"); ink_jw_int(&w, cursor); }
    }
    ink_jw_raw(&w, "}");
    if (r < 0 || (lookup && !shown) || w.overflow) refuse(c, r < 0 ? r : lookup ? INK_FS_ERR_NOT_FOUND : INK_FS_ERR_IO);
    else ink_cap_result(c, out);
    osal_free(out);
}
static void cap_index(ink_call_t *c) { cap_index_query(c, false); }
static void cap_lookup(ink_call_t *c) { cap_index_query(c, true); }

static void cap_thumb(ink_call_t *c) {
    file_context_t context;
    if (!fs_context_for_call(&context, c)) return;
    int64_t id = 0; ink_image_t im;
    if (!ink_args_get_int(&c->args, "id", &id) || id < 1 || id > UINT32_MAX) { refuse(c, INK_FS_ERR_PATH); return; }
    int r = fs_thumb_for(&context, (uint32_t)id, &im);
    if (r) { refuse(c, r); return; }
    size_t count = (size_t)im.width * im.height;
    char *out = osal_malloc(count + 96, OSAL_MEM_EXTERNAL);
    if (!out) refuse(c, INK_FS_ERR_IO);
    else {
        int n = snprintf(out, 96, "{\"id\":%u,\"width\":%u,\"height\":%u,\"gray4\":\"", (unsigned)id, im.width, im.height);
        for (size_t i = 0; i < count; i++) out[n + i] = "0123456789abcdef"[im.pixels[i] / 17];
        memcpy(out + n + count, "\"}", 3); ink_cap_result(c, out);
    }
    osal_free(out); ink_image_free(&im);
}

static const ink_cap_t FILE_CAPS[] = {
    {.id = "file.index", .locked_owner_only = true, .app = "files", .title = "按类型分页查询目录树的文件索引", .input = "{\"type\":\"object\",\"properties\":{" PATH_ARG("path") ",\"type\":{\"type\":\"string\",\"enum\":[\"all\",\"image\",\"audio\",\"text\",\"other\"]},\"cursor\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":4294967295},\"limit\":{\"type\":\"integer\",\"minimum\":1,\"maximum\":10}},\"required\":[\"path\"]}", .output = "{\"type\":\"object\"}",
     .effect = INK_FX_READ, .child_ok = true, .permission = "file", .qos = INK_QOS_BACKGROUND, .exec = INK_EXEC_BG, .run = cap_index},
    {.id = "file.lookup", .locked_owner_only = true, .app = "files", .title = "按稳定编号查询文件", .input = "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"integer\",\"minimum\":1,\"maximum\":4294967295}},\"required\":[\"id\"]}", .output = "{\"type\":\"object\"}",
     .effect = INK_FX_READ, .child_ok = true, .permission = "file", .qos = INK_QOS_BACKGROUND, .exec = INK_EXEC_BG, .run = cap_lookup},
    {.id = "file.list", .locked_owner_only = true, .app = "files",
     .title = "列出目录（/me 自己的文件，/family 全家的内容）",
     .input = "{\"type\":\"object\",\"properties\":{" PATH_ARG("path") ",\"cursor\":{\"type\":\"string\",\"maxLength\":15},\"limit\":{\"type\":\"integer\",\"minimum\":1,\"maximum\":200}},\"required\":[\"path\"]}",
     .output = "{\"type\":\"object\",\"properties\":{\"entries\":{\"type\":\"array\"},\"next\":{\"type\":\"string\"}}}",
     .effect = INK_FX_READ, .child_ok = true, .permission = "file", .qos = INK_QOS_BACKGROUND, .exec = INK_EXEC_BG, .run = cap_list},
    {.id = "file.stat", .locked_owner_only = true, .app = "files", .title = "一个文件或目录的信息", .input = PATH_OBJ("path"), .output = "{\"type\":\"object\"}",
     .effect = INK_FX_READ, .child_ok = true, .permission = "file", .qos = INK_QOS_BACKGROUND, .exec = INK_EXEC_BG, .run = cap_stat},
    {.id = "file.read", .locked_owner_only = true, .app = "files",
     .title = "读文本文件（每次最多 2 KiB）",
     .input = "{\"type\":\"object\",\"properties\":{" PATH_ARG("path") ",\"offset\":{\"type\":\"integer\",\"minimum\":0},\"max_bytes\":{\"type\":\"integer\",\"minimum\":1,\"maximum\":2048}},\"required\":[\"path\"]}",
     .output = "{\"type\":\"object\",\"properties\":{\"text\":{\"type\":\"string\"},\"next_offset\":{\"type\":\"integer\"},\"end\":{\"type\":\"boolean\"}}}",
     .effect = INK_FX_READ, .child_ok = true, .permission = "file", .qos = INK_QOS_BACKGROUND, .exec = INK_EXEC_BG, .run = cap_read},
    {.id = "file.write", .locked_owner_only = true, .app = "files",
     .title = "写文本文件（新建、追加，或明确替换 txt）",
     .input = "{\"type\":\"object\",\"properties\":{" PATH_ARG("path") ",\"text\":{\"type\":\"string\"},\"append\":{\"type\":\"boolean\"},\"replace\":{\"type\":\"boolean\"},\"draft\":{\"type\":\"integer\",\"minimum\":1},\"page\":{\"type\":\"string\",\"enum\":[\"FileEdit\"]}},\"required\":[\"path\",\"text\"]}",
     .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .permission = "file", .qos = INK_QOS_BACKGROUND, .exec = INK_EXEC_BG, .run = cap_write},
    {.id = "file.mkdir", .locked_owner_only = true, .app = "files", .title = "新建目录", .input = PATH_OBJ("path"),
     .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .permission = "file", .qos = INK_QOS_BACKGROUND, .exec = INK_EXEC_BG, .run = cap_mkdir},
    {.id = "file.batch", .locked_owner_only = true, .app = "files", .title = "按文件编号列表移动或拷贝，可选 all_path 的直属文件", .input = "{\"type\":\"object\",\"properties\":{\"op\":{\"type\":\"string\",\"enum\":[\"copy\",\"move\"]},\"ids\":{\"type\":\"array\"}," PATH_ARG("all_path") "," PATH_ARG("to") "},\"required\":[\"op\",\"to\"]}",
     .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .permission = "file", .exec = INK_EXEC_BG, .qos = INK_QOS_BACKGROUND, .timeout_ms = 300000, .run = cap_batch},
    {.id = "file.delete_many", .locked_owner_only = true, .app = "files", .title = "删除选中的文件（不能撤销）", .ask = TR_NOOP("删除这些文件 {ids}{all_path}，不能撤销"), .input = "{\"type\":\"object\",\"properties\":{\"ids\":{\"type\":\"array\"}," PATH_ARG("all_path") "}}",
     .effect = INK_FX_LOCAL_DESTRUCTIVE, .ask_by_hand = true, .child_ok = true, .permission = "file", .exec = INK_EXEC_BG, .qos = INK_QOS_BACKGROUND, .timeout_ms = 300000, .run = cap_batch},
    {.id = "file.batch_progress", .locked_owner_only = true, .app = "files", .title = "批量文件操作逐项进度", .form = INK_FORM_EVENT, .effect = INK_FX_READ, .child_ok = true, .permission = "file"},
    {.id = "file.copy", .locked_owner_only = true, .app = "files",
     .title = "拷贝文件或目录（大的在后台做）",
     .input = "{\"type\":\"object\",\"properties\":{" PATH_ARG("from") "," PATH_ARG("to") "},\"required\":[\"from\",\"to\"]}",
     .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .permission = "file", .qos = INK_QOS_BACKGROUND, .exec = INK_EXEC_BG, .run = cap_copy},
    {.id = "file.move", .locked_owner_only = true, .app = "files",
     .title = "移动或改名（可撤销）",
     .input = "{\"type\":\"object\",\"properties\":{" PATH_ARG("from") "," PATH_ARG("to") "},\"required\":[\"from\",\"to\"]}",
     .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .permission = "file", .qos = INK_QOS_BACKGROUND, .exec = INK_EXEC_BG, .run = cap_move},
    {.id = "file.delete", .locked_owner_only = true, .app = "files", .title = "删除文件或目录（不能撤销）", .ask = TR_NOOP("删除 {path}，不能撤销"), .input = PATH_OBJ("path"),
     .effect = INK_FX_LOCAL_DESTRUCTIVE, .ask_by_hand = true, .child_ok = true, .permission = "file", .qos = INK_QOS_BACKGROUND, .exec = INK_EXEC_BG, .run = cap_delete},
    {.id = "file.free", .locked_owner_only = true, .app = "files", .title = "存储卡还剩多少空间", .output = "{\"type\":\"object\"}",
     .effect = INK_FX_READ, .child_ok = true, .permission = "file", .qos = INK_QOS_BACKGROUND, .exec = INK_EXEC_BG, .run = cap_free},
    {.id = "file.changed", .locked_owner_only = true, .app = "files", .title = "文件变了", .form = INK_FORM_EVENT, .effect = INK_FX_READ, .child_ok = true, .permission = "file"},
    {.id = "file.job_done", .locked_owner_only = true, .app = "files", .title = "后台的拷贝或删除结束了", .form = INK_FORM_EVENT, .effect = INK_FX_READ, .child_ok = true, .permission = "file"},
    {.id = "usb.status", .title = "是否连着电脑、存储卡是否借给了电脑", .output = "{\"type\":\"object\"}",
     .effect = INK_FX_READ, .child_ok = true, .exec = INK_EXEC_DIRECT, .run = cap_usb_status},
    /* the computer sees the whole card, all members' files on it: adults, by a person */
    {.id = "usb.mount", .title = "把存储卡借给电脑当 U 盘", .effect = INK_FX_SYSTEM, .ask_by_hand = true, .qos = INK_QOS_BACKGROUND, .exec = INK_EXEC_BG, .run = cap_usb_mount},
    {.id = "usb.unmount", .title = "从电脑收回存储卡", .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .qos = INK_QOS_BACKGROUND, .exec = INK_EXEC_BG, .run = cap_usb_unmount},
    {.id = "usb.changed", .title = "电脑连上、断开或 U 盘状态变了", .form = INK_FORM_EVENT, .effect = INK_FX_READ, .child_ok = true},
    {.id = "file.thumb", .locked_owner_only = true, .app = "files", .title = "按文件编号读取缩略图（gray4 按行排列，每个十六进制字符表示一个灰阶）", .input = "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"integer\",\"minimum\":1,\"maximum\":4294967295}},\"required\":[\"id\"]}", .output = "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"integer\"},\"width\":{\"type\":\"integer\"},\"height\":{\"type\":\"integer\"},\"gray4\":{\"type\":\"string\"}}}",
     .effect = INK_FX_READ, .child_ok = true, .permission = "file", .qos = INK_QOS_BACKGROUND, .exec = INK_EXEC_MEDIA, .run = cap_thumb},
};

int ink_transfer_caps_init(void);
int ink_storage_usage_caps_init(void);
int ink_file_caps_init(void) {
    int r = ink_cap_register(FILE_CAPS, (int)(sizeof FILE_CAPS / sizeof FILE_CAPS[0]));
    if (!r) r = ink_transfer_caps_init();
    return r ? r : ink_storage_usage_caps_init();
}
