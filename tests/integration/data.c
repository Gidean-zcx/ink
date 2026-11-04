/*
 * The structured data store (system services design §3). The store's source is included unchanged, so
 * that a test can make the storage service refuse or fail at exactly the steps that matter. Results
 * are the probe counters "test.pass" / "test.fail", as in systest.c.
 *
 * INK_DATA_PHASE=write | verify: the two runs of the power-cut test (data_powercut.cmake): "write" does a
 * fixed series of changes with a power cut armed at the flash write INK_DATA_CUT, noting what was
 * reported durable in the file INK_DATA_ACK; "verify" boots from the torn image and checks the collection
 * is exactly the state after some prefix of the series, at least as far as was reported durable.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ink/osal.h"
#include "ink/probe.h"
#include "ink/sys/cap.h"
#include "ink/sys/data.h"
#include "ink/sys/display.h"
#include "ink/sys/exec.h"
#include "ink/sys/hub.h"
#include "ink/sys/json.h"
#include "ink/sys/members.h"
#include "ink/sys/net.h"
#include "ink/sys/power.h"
#include "ink/sys/res.h"
#include "ink/sys/rt.h"
#include "ink/sys/settings.h"
#include "ink/sys/state.h"
#include "ink/sys/storage.h"
#include "ink/sys/svc.h"
#include "sim.h"

static int g_fail_appends, g_fail_snapshots;
static int test_append(const char *path, const void *data, size_t len, ink_storage_done_fn done, void *ctx);
static int test_write_borrow(const char *path, const void *data, size_t len, ink_storage_done_fn done, void *ctx);
#define ink_storage_append test_append
#define ink_storage_write_borrow test_write_borrow
#include "../../system/services/data/datastore.c"
#undef ink_storage_append
#undef ink_storage_write_borrow

#define CHECK(cond, ...)                                                 \
    do {                                                                 \
        if (cond) {                                                      \
            ink_probe_count("test.pass");                                \
        } else {                                                         \
            char _m[300];                                                \
            snprintf(_m, sizeof _m, __VA_ARGS__);                        \
            sim_warn("CHECK FAILED (%s:%d) %s", __FILE__, __LINE__, _m); \
            ink_probe_count("test.fail");                                \
        }                                                                \
    } while (0)

/* A failure the storage service reports later, as a full flash would: nothing reached the file. */
static void *g_failed_ctx;
static ink_storage_done_fn g_failed_done;
static ink_exec_job_t g_fail_job;
static void fail_later(void *arg) {
    (void)arg;
    g_failed_done(g_failed_ctx, -1);
}

static int test_append(const char *path, const void *data, size_t len, ink_storage_done_fn done, void *ctx) {
    if (g_fail_appends > 0) {
        g_fail_appends--;
        g_failed_done = done;
        g_failed_ctx = ctx;
        ink_exec_post(INK_EXEC_BG, &g_fail_job, fail_later, NULL);
        return 0;
    }
    return ink_storage_append(path, data, len, done, ctx);
}

static int test_write_borrow(const char *path, const void *data, size_t len, ink_storage_done_fn done, void *ctx) {
    if (g_fail_snapshots > 0) {
        g_fail_snapshots--;
        g_failed_done = done;
        g_failed_ctx = ctx;
        ink_exec_post(INK_EXEC_BG, &g_fail_job, fail_later, NULL);
        return 0;
    }
    return ink_storage_write_borrow(path, data, len, done, ctx);
}

static const ink_db_def_t NOTES = {.name = "test.notes", .title = "测试笔记", .schema = 2, .json = true, .ai_read = true, .ai_write = true};
static const ink_db_def_t PRIVATE = {.name = "test.private", .title = "凭据", .scope = INK_DB_DEVICE, .schema = 1};
static const ink_db_def_t SMALL = {.name = "test.small", .scope = INK_DB_MEMBER, .schema = 1, .max_records = 3, .drop_oldest = true};
static const ink_db_def_t BYTES = {.name = "test.bytes", .scope = INK_DB_MEMBER, .schema = 1, .max_bytes = 100};
static const ink_db_def_t FOLD = {.name = "test.fold", .scope = INK_DB_DEVICE, .schema = 1};
static const ink_db_def_t RELOAD = {.name = "test.reload", .scope = INK_DB_DEVICE, .schema = 1, .json = true};

static uint32_t put_text(ink_db_t *db, uint32_t id, int64_t sort, const char *s) {
    ink_db_write_t w = {.id = id, .sort = sort, .data = s, .len = strlen(s), .src = INK_SRC_TOUCH};
    uint32_t out = 0;
    int r = ink_db_put(db, &w, &out);
    return r == INK_DB_OK ? out : 0;
}

static void drain(ink_db_t *db) {
    CHECK(ink_db_wait(db, ink_db_last_ver(db), 5000000) == 0, "everything reaches the flash");
    for (int i = 0; i < 500 && ink_storage_pending() > 0; i++) osal_task_delay_us(10000);
}

/* Forget a closed collection in memory, so the next open reads the flash. */
static void forget(const ink_db_def_t *def, const char *member) {
    lock();
    for (int i = 0; i < MAX_COLLECTIONS; i++) {
        ink_db_t *c = &DB.c[i];
        if (c->used && c->def == def && !strcmp(c->member, member ? member : "")) {
            CHECK(evictable(c), "%s can be forgotten: closed, nothing to write", def->name);
            c->last_use = 0; /* it is the least recently used now */
            evict_one(NULL);
        }
    }
    unlock();
}

static int page_ids(ink_db_t *db, ink_db_cursor_t *c, uint32_t *ids, int max) {
    int n = 0;
    ink_rec_t r;
    while (n < max && ink_db_next(db, c, &r, NULL, 0) == 1) ids[n++] = r.id;
    return n;
}

static void basics(void) {
    ink_db_t *db = ink_db_open(&NOTES, NULL);
    CHECK(db && ink_db_count(db) == 0, "a new collection is empty");
    uint32_t a = put_text(db, 0, 30, "{\"t\":\"third\"}"), b = put_text(db, 0, 10, "{\"t\":\"first\"}"), c = put_text(db, 0, 20, "{\"t\":\"second\"}");
    CHECK(a && b && c && a != b && b != c && ink_db_count(db) == 3, "records get their own ids (%u %u %u)", a, b, c);
    char buf[64];
    ink_rec_t r;
    CHECK(ink_db_get(db, b, &r, buf, sizeof buf) == INK_DB_OK && !strcmp(buf, "{\"t\":\"first\"}") && r.sort == 10 && r.src == INK_SRC_TOUCH && r.schema == 2,
          "a record reads back with its fields (%s)", buf);
    CHECK(r.updated_s > 0, "it says when it changed (%lld)", (long long)r.updated_s);
    CHECK(ink_db_get(db, 999, &r, buf, sizeof buf) == INK_DB_ERR_NOT_FOUND, "an unknown id is not found");

    /* pages, both ways; a change between pages neither skips nor repeats */
    ink_db_cursor_t cur = {0};
    uint32_t ids[8];
    CHECK(page_ids(db, &cur, ids, 2) == 2 && ids[0] == b && ids[1] == c, "pages by sort key, ascending");
    uint32_t d = put_text(db, 0, 5, "{\"t\":\"before the cursor\"}"), e = put_text(db, 0, 25, "{\"t\":\"after the cursor\"}");
    CHECK(ink_db_del(db, a, INK_SRC_TOUCH) == INK_DB_OK, "delete one");
    CHECK(page_ids(db, &cur, ids, 8) == 1 && ids[0] == e, "the next page sees what came after the cursor, not what went before it");
    ink_db_cursor_t back = {.desc = true};
    CHECK(page_ids(db, &back, ids, 8) == 4 && ids[0] == e && ids[1] == c && ids[2] == b && ids[3] == d, "newest first");
    CHECK(ink_db_get(db, a, &r, NULL, 0) == INK_DB_ERR_NOT_FOUND && ink_db_count(db) == 4, "a deleted record is gone from reads");
    CHECK(ink_db_del(db, a, INK_SRC_TOUCH) == INK_DB_ERR_NOT_FOUND, "deleting it twice says so");

    /* replacing moves it */
    uint32_t same = put_text(db, b, 40, "{\"t\":\"moved\"}");
    ink_db_cursor_t again = {.desc = true};
    CHECK(same == b && page_ids(db, &again, ids, 1) == 1 && ids[0] == b && ink_db_count(db) == 4, "a replace keeps the id, moves in the order");

    /* a cut copy: the length says so */
    char tiny[4];
    CHECK(ink_db_get(db, b, &r, tiny, sizeof tiny) == INK_DB_OK && r.len == strlen("{\"t\":\"moved\"}") && !memcmp(tiny, "{\"t\"", 4), "a small buffer gets a cut copy and the full length");

    /* the change feed, for the hub: everything since, deleted records too */
    uint32_t since = 0;
    int changes = 0, deleted = 0;
    while (ink_db_changes(db, &since, &r, NULL, 0) == 1) {
        changes++;
        deleted += r.deleted;
    }
    CHECK(changes == 5 && deleted == 1 && since == ink_db_last_ver(db), "the changes since 0: every record's last change, the deleted one too (%d, %d)", changes, deleted);
    ink_db_ack(db, ink_db_last_ver(db));
    since = 0;
    deleted = 0;
    while (ink_db_changes(db, &since, &r, NULL, 0) == 1) deleted += r.deleted;
    CHECK(deleted == 0, "once the hub has them tombstones go");

    /* what was durable comes back from the flash */
    uint32_t ver = ink_db_last_ver(db);
    CHECK(ink_db_wait(db, ver, 5000000) == 0 && ink_db_durable_ver(db) >= ver, "durable");
    drain(db);
    ink_db_close(db);
    forget(&NOTES, "owner");
    db = ink_db_open(&NOTES, NULL);
    ink_db_cursor_t after = {.desc = true};
    CHECK(db && ink_db_count(db) == 4 && ink_db_last_ver(db) == ver && page_ids(db, &after, ids, 8) == 4 && ids[0] == b && ids[3] == d,
          "the collection reloads from the flash, same order, same counter");
    uint32_t f = put_text(db, 0, 50, "{\"t\":\"new\"}");
    CHECK(f && f > e && ink_db_last_ver(db) == ver + 1, "ids and the counter carry on after a reload");
    ink_db_close(db);

    /* the same collection opened twice is one */
    ink_db_t *x = ink_db_open(&NOTES, NULL), *y = ink_db_open(&NOTES, NULL);
    CHECK(x == y, "the same collection, opened twice");
    ink_db_close(x);
    ink_db_close(y);
    CHECK(ink_db_open(&NOTES, "BAD/ID") == NULL, "a member id that could leave the area is refused");
}

static void members(void) {
    static const char *const OWNER_FILE = "/user/m/owner/test.notes.0.log", *const KID_FILE = "/user/m/kid/test.notes.0.log";
    ink_db_t *mine = ink_db_open(&NOTES, "owner"), *kid = ink_db_open(&NOTES, "kid");
    CHECK(mine && kid && mine != kid, "each member has their own");
    int before = ink_db_count(mine);
    uint32_t id = put_text(kid, 0, 1, "{\"who\":\"kid\"}");
    char buf[32];
    CHECK(id && ink_db_count(kid) == 1 && ink_db_count(mine) == before, "what one writes the other does not see");
    CHECK(ink_db_get(mine, id, NULL, buf, sizeof buf) == INK_DB_ERR_NOT_FOUND || strcmp(buf, "{\"who\":\"kid\"}"), "not by id either");
    drain(kid);
    drain(mine);
    CHECK(ink_storage_size(OWNER_FILE) > 0 && ink_storage_size(KID_FILE) > 0, "and the files are apart");
    ink_db_close(kid);
    ink_db_close(mine);
    /* NULL means whoever is current */
    CHECK(ink_member_switch("kid") == 0, "switch to the child");
    ink_db_t *now = ink_db_open(&NOTES, NULL);
    CHECK(now == kid, "the current member's collection");
    ink_db_close(now);
    /* back to the adult: the test goes through the store's own record of who is current */
    ink_member_test_reset();
    ink_db_t *dev = ink_db_open(&PRIVATE, "kid");
    uint32_t pid = put_text(dev, 0, 1, "secret");
    drain(dev);
    ink_db_close(dev);
    CHECK(pid && ink_storage_size("/user/sys/test.private.0.log") > 0, "device data is in the device's area, whoever asks");
}

static void limits(void) {
    ink_db_t *db = ink_db_open(&SMALL, NULL);
    for (int i = 1; i <= 5; i++) put_text(db, 0, i, "x");
    ink_db_cursor_t cur = {0};
    ink_rec_t r;
    CHECK(ink_db_count(db) == 3 && ink_db_next(db, &cur, &r, NULL, 0) == 1 && r.sort == 3, "at its limit the oldest go (%d, oldest %lld)", ink_db_count(db), (long long)r.sort);
    drain(db);
    ink_db_close(db);
    forget(&SMALL, "owner");
    db = ink_db_open(&SMALL, NULL);
    CHECK(ink_db_count(db) == 3, "they stay gone after a reload (%d)", ink_db_count(db));
    ink_db_close(db);

    db = ink_db_open(&BYTES, NULL);
    char big[60];
    memset(big, 'b', sizeof big);
    ink_db_write_t w = {.data = big, .len = sizeof big, .sort = 1};
    uint32_t first = 0;
    CHECK(ink_db_put(db, &w, &first) == INK_DB_OK, "60 bytes of 100 fit");
    CHECK(ink_db_put(db, &w, NULL) == INK_DB_ERR_FULL && ink_db_count(db) == 1, "60 more do not: refused, nothing changed");
    w.id = first;
    CHECK(ink_db_put(db, &w, NULL) == INK_DB_OK, "replacing counts the difference");
    static char huge[INK_DB_RECORD_MAX + 1];
    ink_db_write_t h = {.data = huge, .len = sizeof huge};
    CHECK(ink_db_put(db, &h, NULL) == INK_DB_ERR_TOO_LARGE, "a record over the limit is refused");
    ink_db_close(db);
}

static uint32_t g_write_failed;
static void on_failed(void *ctx, const char *id, const ink_args_t *a) {
    (void)ctx;
    (void)id;
    (void)a;
    g_write_failed++;
}

/* Folding: many changes make a snapshot and a new log; nothing is lost when writing it fails. */
static void folding(void) {
    ink_db_t *db = ink_db_open(&FOLD, NULL);
    static char text[320];
    memset(text, 'f', sizeof text - 1);
    int folds0 = (int)sim_metric_value("data.compactions");
    for (int i = 1; i <= 40; i++) {
        snprintf(text, 8, "%04d", i);
        text[4] = 'f';
        CHECK(put_text(db, (uint32_t)(i % 10) + 1, i, text) != 0, "put %d", i);
        drain(db);
    }
    for (int i = 0; i < 200 && (int)sim_metric_value("data.compactions") == folds0; i++) osal_task_delay_us(20000);
    drain(db);
    CHECK((int)sim_metric_value("data.compactions") > folds0, "the log was folded into a snapshot");
    CHECK(ink_storage_size("/user/sys/test.fold.snap") > 0, "the snapshot is there");
    uint32_t ver = ink_db_last_ver(db);
    ink_db_close(db);
    forget(&FOLD, "");
    db = ink_db_open(&FOLD, NULL);
    CHECK(db && ink_db_count(db) == 10 && ink_db_last_ver(db) == ver, "snapshot plus log give the same collection (%d, ver %u of %u)", ink_db_count(db), ink_db_last_ver(db), ver);
    /* a snapshot's frames are in sort order, not change order: the last change here sorts first */
    put_text(db, 50, -5, "first in the order, last in time");
    uint32_t order_ver = ink_db_last_ver(db);
    drain(db);
    ink_db_close(db);
    forget(&FOLD, "");
    db = ink_db_open(&FOLD, NULL);
    ink_db_cursor_t oc = {0};
    ink_rec_t or0;
    CHECK(ink_db_last_ver(db) == order_ver && ink_db_count(db) == 11 && ink_db_next(db, &oc, &or0, NULL, 0) == 1 && or0.id == 50, "the order and the counter survive");
    CHECK(ink_db_del(db, 50, INK_SRC_TOUCH) == INK_DB_OK, "take it out again");
    drain(db);
    char got[8];
    ink_rec_t r;
    CHECK(ink_db_get(db, (40 % 10) + 1, &r, got, 4) == INK_DB_OK && !memcmp(got, "0040", 4), "with the latest values (%.4s)", got);

    /* the snapshot cannot be written: the logs stay, everything comes back */
    g_fail_snapshots = 3;
    int errs0 = (int)sim_metric_value("data.compaction_errors");
    for (int i = 41; i <= 80 && !sim_metric_value("data.compaction_errors"); i++) {
        snprintf(text, 8, "%04d", i);
        text[4] = 'f';
        put_text(db, (uint32_t)(i % 10) + 1, i, text);
        drain(db);
    }
    for (int i = 0; i < 100 && (int)sim_metric_value("data.compaction_errors") == errs0; i++) osal_task_delay_us(20000);
    CHECK((int)sim_metric_value("data.compaction_errors") > errs0, "a snapshot could not be written");
    g_fail_snapshots = 0;
    ver = ink_db_last_ver(db);
    drain(db);
    ink_db_close(db);
    osal_task_delay_us(100000);
    for (int i = 0; i < 500; i++) { /* a retry of the fold may be under way */
        lock();
        bool idle = true;
        for (int k = 0; k < MAX_COLLECTIONS; k++)
            if (DB.c[k].used && DB.c[k].def == &FOLD && !evictable(&DB.c[k])) idle = false;
        unlock();
        if (idle) break;
        osal_task_delay_us(100000);
    }
    forget(&FOLD, "");
    db = ink_db_open(&FOLD, NULL);
    CHECK(db && ink_db_count(db) == 10 && ink_db_last_ver(db) == ver, "nothing was lost with it (%d, ver %u of %u)", ink_db_count(db), ink_db_last_ver(db), ver);
    ink_db_close(db);
}

/* A write that fails stays and is tried again, in order; whoever asked can tell. */
static void failing_writes(void) {
    CHECK(ink_cap_subscribe("data.", on_failed, NULL, INK_EXEC_DIRECT) == 0, "subscribe to the failure event");
    ink_db_t *db = ink_db_open(&RELOAD, NULL);
    drain(db);
    g_fail_appends = 1;
    uint32_t a = put_text(db, 0, 1, "{\"n\":1}");
    uint32_t ver_a = ink_db_last_ver(db);
    osal_task_delay_us(50000);
    CHECK(a && ink_db_error(db) != 0 && ink_db_durable_ver(db) < ver_a, "the write failed and says so: not durable (error %d)", ink_db_error(db));
    uint32_t b = put_text(db, 0, 2, "{\"n\":2}");
    uint32_t ver_b = ink_db_last_ver(db);
    char buf[16];
    CHECK(b && ink_db_get(db, a, NULL, buf, sizeof buf) == INK_DB_OK && ink_db_count(db) == 2, "what is in memory stays readable");
    for (int i = 0; i < 700 && ink_db_durable_ver(db) < ver_b; i++) osal_task_delay_us(10000);
    CHECK(ink_db_durable_ver(db) >= ver_b && ink_db_error(db) == 0, "tried again: both reach the flash, the error clears");
    CHECK(g_write_failed == 1, "the failure was told once (%u)", g_write_failed);
    drain(db);
    ink_db_close(db);
    forget(&RELOAD, "");
    db = ink_db_open(&RELOAD, NULL);
    CHECK(db && ink_db_count(db) == 2 && ink_db_last_ver(db) == ver_b, "in order, nothing twice (%d, ver %u of %u)", ink_db_count(db), ink_db_last_ver(db), ver_b);
    ink_db_close(db);
}

/* Damaged files: what can be read is kept, a later fold clears the damage, a newer format is not touched. */
static const ink_db_def_t BAD_LOG = {.name = "test.badlog", .scope = INK_DB_DEVICE, .schema = 1};
static const ink_db_def_t BAD_SNAP = {.name = "test.badsnap", .scope = INK_DB_DEVICE, .schema = 1};
static const ink_db_def_t NEWER = {.name = "test.newer", .scope = INK_DB_DEVICE, .schema = 1};

static size_t make_frame(uint8_t *out, uint32_t ver, uint32_t id, const char *text) {
    frame_t f = {.sync = FRAME_SYNC, .op = OP_PUT, .ver = ver, .id = id, .sort = id, .len = (uint16_t)strlen(text), .schema = 1};
    memcpy(out, &f, sizeof f);
    memcpy(out + sizeof f, text, f.len);
    uint32_t crc = ink_crc32(0, out, sizeof f + f.len);
    memcpy(out + sizeof f + f.len, &crc, 4);
    return FRAME_SIZE(f.len);
}

static void damage(void) {
    static uint8_t file[512];
    size_t n = make_frame(file, 1, 1, "one");
    size_t second = n;
    n += make_frame(file + n, 2, 2, "two");
    n += make_frame(file + n, 3, 3, "three");
    file[second + sizeof(frame_t)] ^= 0x40; /* the second frame's content */
    CHECK(ink_storage_write_atomic("/user/sys/test.badlog.0.log", file, n) == 0, "a log with a bad frame in the middle");
    for (int i = 0; i < 300 && ink_storage_pending() > 0; i++) osal_task_delay_us(10000);
    int bad0 = (int)sim_metric_value("data.log_bad");
    ink_db_t *db = ink_db_open(&BAD_LOG, NULL);
    char buf[8];
    CHECK(db && (int)sim_metric_value("data.log_bad") == bad0 + 1, "it opens and counts the damage");
    CHECK(ink_db_get(db, 1, NULL, buf, sizeof buf) == INK_DB_OK && ink_db_get(db, 2, NULL, buf, sizeof buf) == INK_DB_ERR_NOT_FOUND &&
              ink_db_get(db, 3, NULL, buf, sizeof buf) == INK_DB_OK && !strcmp(buf, "three"),
          "the frames around the bad one are kept");
    for (int i = 0; i < 300 && (ink_storage_size("/user/sys/test.badlog.snap") < 0 || ink_storage_pending() > 0); i++) osal_task_delay_us(10000);
    CHECK(ink_storage_size("/user/sys/test.badlog.snap") > 0, "the damage is folded away soon");
    ink_db_close(db);
    forget(&BAD_LOG, "");
    bad0 = (int)sim_metric_value("data.log_bad");
    db = ink_db_open(&BAD_LOG, NULL);
    CHECK(db && ink_db_count(db) == 2 && (int)sim_metric_value("data.log_bad") == bad0, "and gone at the next load (%d)", ink_db_count(db));
    ink_db_close(db);

    /* a snapshot whose checksum is wrong: the log after it still counts */
    static uint8_t snap[256];
    snap_hdr_t h = {.magic = SNAP_MAGIC, .format = SNAP_FORMAT, .schema = 1, .count = 1, .last_ver = 1, .next_id = 2, .log_gen = 3};
    size_t m = sizeof h;
    m += make_frame(snap + m, 1, 1, "lost");
    h.crc = 0x12345678u;
    memcpy(snap, &h, sizeof h);
    size_t l = make_frame(file, 2, 2, "kept");
    CHECK(ink_storage_write_atomic("/user/sys/test.badsnap.snap", snap, m) == 0 && ink_storage_write_atomic("/user/sys/test.badsnap.3.log", file, l) == 0, "a damaged snapshot and its log");
    for (int i = 0; i < 300 && ink_storage_pending() > 0; i++) osal_task_delay_us(10000);
    int badsnap0 = (int)sim_metric_value("data.snapshot_bad");
    db = ink_db_open(&BAD_SNAP, NULL);
    CHECK(db && (int)sim_metric_value("data.snapshot_bad") == badsnap0 + 1 && ink_db_get(db, 2, NULL, buf, sizeof buf) == INK_DB_OK && !strcmp(buf, "kept"), "the log's records are read");
    ink_db_close(db);

    /* a newer format: refused, and the file stays as it is */
    snap_hdr_t nh = {.magic = SNAP_MAGIC, .format = SNAP_FORMAT + 1};
    CHECK(ink_storage_write_atomic("/user/sys/test.newer.snap", &nh, sizeof nh) == 0, "a snapshot from a newer firmware");
    for (int i = 0; i < 300 && ink_storage_pending() > 0; i++) osal_task_delay_us(10000);
    CHECK(ink_db_open(&NEWER, NULL) == NULL && sim_metric_value("data.format_too_new") >= 1, "is refused");
    CHECK(ink_storage_size("/user/sys/test.newer.snap") == (int64_t)sizeof nh, "and left alone");
}

/* A lazy write waits for a batch; the flush at locking and its own time both take it. */
static void lazy(void) {
    static const ink_db_def_t LAZY = {.name = "test.lazy", .scope = INK_DB_DEVICE, .schema = 1};
    ink_db_t *db = ink_db_open(&LAZY, NULL);
    ink_db_write_t w = {.sort = 1, .data = "pos", .len = 3, .flags = INK_DB_LAZY};
    CHECK(ink_db_put(db, &w, NULL) == INK_DB_OK && ink_db_last_ver(db) == 1, "a lazy write is accepted");
    osal_task_delay_us(2000000);
    CHECK(ink_db_durable_ver(db) == 0 && !ink_db_idle(), "two seconds later it still waits");
    ink_db_flush_all();
    for (int i = 0; i < 300 && ink_db_durable_ver(db) < 1; i++) osal_task_delay_us(10000);
    CHECK(ink_db_durable_ver(db) == 1, "the flush for locking takes it");
    w.flags = INK_DB_LAZY;
    CHECK(ink_db_put(db, &w, NULL) == INK_DB_OK, "another");
    osal_task_delay_us(61000000);
    CHECK(ink_db_durable_ver(db) == 2, "its own time comes after a minute");
    ink_db_close(db);
}

/* Pressure: closed collections give their memory back; open ones and unsaved changes do not. */
static void memory(void) {
    ink_db_t *db = ink_db_open(&NOTES, NULL);
    size_t before = used_bytes();
    drain(db);
    ink_db_close(db);
    size_t got = reclaim(NULL, 1);
    CHECK(got > 0 && used_bytes() < before, "a closed collection gives memory back (%zu)", got);
    db = ink_db_open(&NOTES, NULL);
    CHECK(db && ink_db_count(db) > 0, "and comes back");
    put_text(db, 0, 99, "{\"unsaved\":true}");
    ink_db_close(db);
    db = ink_db_open(&NOTES, NULL);
    drain(db);
    ink_db_close(db);
}

/* ---- through the bus, as the AI ---- */

static volatile int g_done;
static ink_receipt_t g_rc;
static char g_result[2048];

static void done_cb(void *ctx, const ink_receipt_t *rc) {
    (void)ctx;
    g_rc = *rc;
    g_result[0] = 0;
    if (rc->result) snprintf(g_result, sizeof g_result, "%s", rc->result);
    g_rc.result = NULL;
    g_done++;
}

static bool call_json(const char *id, const char *args, ink_source_t src) {
    int before = g_done;
    ink_call_opts_t o = {.src = src, .args_json = args, .reply_exec = INK_EXEC_DIRECT, .done = done_cb};
    if (ink_cap_call(id, NULL, &o) != 0) return false;
    for (int i = 0; i < 500 && g_done == before; i++) osal_task_delay_us(10000);
    return g_done > before;
}

static void caps(void) {
    CHECK(ink_db_register(&NOTES) == 0 && ink_db_register(&PRIVATE) == 0, "collections are listed");
    CHECK(call_json("data.collections", NULL, INK_SRC_AI) && g_rc.status == INK_RC_DONE && strstr(g_result, "test.notes") && !strstr(g_result, "test.private"),
          "the AI sees the ones open to it (%s)", g_result);
    ink_db_t *db = ink_db_open(&NOTES, NULL);
    int count = ink_db_count(db);
    ink_db_close(db);
    CHECK(call_json("data.put", "{\"collection\":\"test.notes\",\"value\":{\"from\":\"ai\"}}", INK_SRC_AI) && g_rc.status == INK_RC_DONE && g_rc.undo_ms > 0 && g_rc.object[0],
          "the AI writes a record; it can be undone (%s, %s)", ink_rc_name(g_rc.status), g_rc.detail);
    char object[48];
    snprintf(object, sizeof object, "%s", g_rc.object);
    uint32_t id = (uint32_t)atoi(strchr(object, '#') + 1);
    char args[96];
    snprintf(args, sizeof args, "{\"collection\":\"test.notes\",\"id\":%u}", id);
    CHECK(call_json("data.get", args, INK_SRC_AI) && g_rc.status == INK_RC_DONE && strstr(g_result, "\"value\":{\"from\":\"ai\"}") && strstr(g_result, "\"by\":\"ai\""),
          "it reads back, and says the AI wrote it (%s)", g_result);
    CHECK(call_json("data.list", "{\"collection\":\"test.notes\",\"limit\":1}", INK_SRC_AI) && strstr(g_result, "\"records\":[") && strstr(g_result, "\"next\":"),
          "a page with a cursor for the next one (%s)", g_result);
    char *next = strstr(g_result, "\"next\":\"");
    char cursor[40] = "";
    if (next) sscanf(next + 8, "%39[^\"]", cursor);
    char args2[120];
    snprintf(args2, sizeof args2, "{\"collection\":\"test.notes\",\"limit\":50,\"cursor\":\"%s\"}", cursor);
    CHECK(call_json("data.list", args2, INK_SRC_AI) && !strstr(g_result, "\"from\":\"ai\"") && strstr(g_result, "\"records\":["), "the next page goes on (%s)", g_result);
    CHECK(call_json("data.get", "{\"collection\":\"test.private\",\"id\":1}", INK_SRC_AI) && g_rc.status == INK_RC_REJECTED, "a collection not open to it is refused (%s)", ink_rc_name(g_rc.status));
    CHECK(call_json("data.list", "{\"collection\":\"no.such\"}", INK_SRC_AI) && g_rc.status == INK_RC_INVALID, "so is one that does not exist");
    CHECK(call_json("data.delete", args, INK_SRC_AI) && g_rc.status == INK_RC_PENDING_CONFIRMATION, "deleting asked by the AI waits for a person (%s)", ink_rc_name(g_rc.status));
    snprintf(args2, sizeof args2, "{\"object\":\"%s\"}", object);
    CHECK(call_json("system.undo", args2, INK_SRC_AI) && g_rc.status == INK_RC_UNDONE, "undo takes the new record back (%s, %s)", ink_rc_name(g_rc.status), g_rc.detail);
    db = ink_db_open(&NOTES, NULL);
    CHECK(ink_db_count(db) == count, "it is gone (%d of %d)", ink_db_count(db), count);
    ink_db_close(db);
    CHECK(call_json("system.undo", args2, INK_SRC_AI) && g_rc.status == INK_RC_FAILED, "undo works once");
    CHECK(call_json("caps.domains", NULL, INK_SRC_AI) && strstr(g_result, "\"data\":") && strstr(g_result, "\"caps\":"), "caps.domains (%s)", g_result);
    CHECK(call_json("caps.find", "{\"query\":\"data.\"}", INK_SRC_AI) && strstr(g_result, "\"name\":\"data.list\"") && strstr(g_result, "inputSchema"), "caps.find gives whole descriptions");
    static char page[1024];
    int nextp = -1, n = ink_cap_manifest_page(0, NULL, page, 512, &nextp);
    CHECK(n > 0 && nextp > 0, "the tool list comes in pages (%d, next %d)", n, nextp);
    n = ink_cap_manifest_page(nextp, "data.", page, sizeof page, &nextp);
    CHECK(n > 0 && page[0] == '[' && page[n - 1] == ']' && strstr(page, "data."), "a page of one domain");
}

/* ---- the power-cut runs ---- */

#define PC_IDS 17
#define PC_OPS 150

/* The series: put id with value v<i>; every 5th deletes instead. Same every run. */
static void pc_value(int i, char *out, size_t cap) {
    int fill = 40 + (i * 37) % 300;
    int n = snprintf(out, cap, "v%d:", i);
    for (int k = n; k < fill && k < (int)cap - 1; k++) out[k] = (char)('a' + (i + k) % 26);
    out[fill < (int)cap - 1 ? fill : (int)cap - 1] = 0;
}

static const ink_db_def_t PC = {.name = "test.pc", .scope = INK_DB_DEVICE, .schema = 1};

static void note_ack(void *arg) {
    const char *path = getenv("INK_DATA_ACK");
    FILE *f = path ? fopen(path, "w") : NULL;
    if (f) {
        fprintf(f, "%u\n", *(uint32_t *)arg);
        fclose(f);
    }
}

static void pc_write(void) {
    ink_db_t *db = ink_db_open(&PC, NULL);
    static char v[400];
    const char *cut = getenv("INK_DATA_CUT");
    if (cut && atoi(cut) > 0) sim_storage_arm_power_cut(atoi(cut));
    for (int i = 0; i < PC_OPS; i++) {
        uint32_t id = (uint32_t)(i % PC_IDS) + 1;
        if (i % 5 == 4) {
            if (ink_db_del(db, id, INK_SRC_SYSTEM) != INK_DB_OK) ink_probe_count("test.del_missing");
        } else {
            pc_value(i, v, sizeof v);
            ink_db_write_t w = {.id = id, .sort = i, .data = v, .len = strlen(v)};
            CHECK(ink_db_put(db, &w, NULL) == INK_DB_OK, "put %d", i);
        }
        if (i % 3 == 2) { /* some wait until durable: from here on these are promised */
            uint32_t ver = ink_db_last_ver(db);
            if (ink_db_wait(db, ver, 20000000) == 0) vtk_host_call(note_ack, &ver);
        }
    }
    ink_probe_count("test.series_done");
    osal_task_delay_us(5000000);
}

static void pc_verify(void) {
    uint32_t acked = 0;
    const char *path = getenv("INK_DATA_ACK");
    if (path) {
        FILE *f = fopen(path, "r");
        if (f) {
            unsigned a = 0;
            if (fscanf(f, "%u", &a) == 1) acked = a;
            fclose(f);
        }
    }
    ink_db_t *db = ink_db_open(&PC, NULL);
    CHECK(db != NULL, "the collection opens after the cut");
    if (!db) return;
    uint32_t ver = ink_db_last_ver(db);
    CHECK(ver >= acked, "everything promised is there (at %u, promised %u)", ver, acked);
    /* replay the series up to this counter: the collection must be exactly that state */
    static struct { bool live; int last; } want[PC_IDS + 1];
    uint32_t v = 0;
    for (int i = 0; i < PC_OPS && v < ver; i++) {
        uint32_t id = (uint32_t)(i % PC_IDS) + 1;
        if (i % 5 == 4) {
            if (want[id].live) {
                want[id].live = false;
                v++;
            }
        } else {
            want[id].live = true;
            want[id].last = i;
            v++;
        }
    }
    CHECK(v == ver, "the counter matches the series (%u, %u)", v, ver);
    static char got[400], exp[400];
    int live = 0;
    for (uint32_t id = 1; id <= PC_IDS; id++) {
        ink_rec_t r;
        int rc = ink_db_get(db, id, &r, got, sizeof got - 1);
        if (want[id].live) {
            live++;
            pc_value(want[id].last, exp, sizeof exp);
            got[rc == INK_DB_OK && r.len < sizeof got ? r.len : 0] = 0;
            CHECK(rc == INK_DB_OK && !strcmp(got, exp), "record %u holds change %d (%.24s)", id, want[id].last, rc == INK_DB_OK ? got : "gone");
        } else CHECK(rc == INK_DB_ERR_NOT_FOUND, "record %u is deleted", id);
    }
    CHECK(ink_db_count(db) == live, "and nothing else (%d, %d)", ink_db_count(db), live);
    /* it goes on working: a new change is durable and reloads */
    ink_db_write_t w = {.id = 1000, .sort = 1000, .data = "after", .len = 5};
    CHECK(ink_db_put(db, &w, NULL) == INK_DB_OK, "a change after the cut");
    CHECK(ink_db_wait(db, ink_db_last_ver(db), 20000000) == 0, "is durable");
    uint32_t last = ink_db_last_ver(db);
    for (int i = 0; i < 500 && ink_storage_pending() > 0; i++) osal_task_delay_us(10000);
    ink_db_close(db);
    forget(&PC, "");
    db = ink_db_open(&PC, NULL);
    CHECK(db && ink_db_last_ver(db) == last && ink_db_count(db) == live + 1, "and reloads (%u of %u)", ink_db_last_ver(db), last);
    ink_db_close(db);
}

static void test_task(void *arg) {
    (void)arg;
    if (ink_display_init() != 0 || ink_storage_init() != 0 || ink_cap_init() != 0 || ink_exec_init() != 0 || ink_res_init() != 0 || ink_state_init() != 0 ||
        ink_settings_init() != 0 || ink_svc_init() != 0 || ink_data_init() != 0 || ink_power_init() != 0 || ink_net_init() != 0 ||
        ink_hub_init("wss://hub.local/v1/device") != 0 || ink_tools_init() != 0) {
        CHECK(0, "the services start");
        return;
    }
    ink_members_init();
    const char *phase = getenv("INK_DATA_PHASE");
    if (phase && !strcmp(phase, "write")) pc_write();
    else if (phase && !strcmp(phase, "verify")) pc_verify();
    else {
        basics();
        members();
        limits();
        folding();
        failing_writes();
        damage();
        memory();
        lazy();
        caps();
    }
    ink_probe_count("test.finished");
}

void ink_main(void) {
    osal_task_cfg_t cfg = {.name = "datatest", .fn = test_task, .stack_bytes = 8192, .priority = OSAL_PRIO_NORMAL, .core = -1};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "create the test task");
}
