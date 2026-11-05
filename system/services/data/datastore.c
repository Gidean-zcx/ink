/*
 * Data store (see ink/sys/data.h; formats and power-cut rules in the system
 * services design §3).
 *
 * Files of a collection, in the member's or the device's data area:
 *   <name>.snap       snapshot, replaced atomically: header, then record frames
 *   <name>.<gen>.log  append log: record frames
 *
 * The snapshot names the log generation G it continues from. Loading replays
 * generations G and G+1 and skips what the snapshot already holds, by change
 * counter. Folding the log: while the log in use is generation G it moves on
 * to G+1 first (appends go to the new file), the snapshot is written with its
 * new generation, and only when that is on the flash is the old log removed.
 * So at most two generations exist and a power cut anywhere loses nothing that
 * was durable.
 *
 * One append at a time per collection; what piles up meanwhile goes in the next
 * batch (LittleFS rewrites the file's last block on every commit: batches save
 * erases). A failed batch stays and is tried again, doubling the wait.
 *
 * Everything is under one lock, which is never held across the flash: it nests
 * the storage service's and the shared lock inside. ink_mem_alloc is not used
 * under it, because its pressure check runs the reclaimer here.
 */
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "ink/osal.h"
#include "ink/probe.h"
#include "ink/sys/cap.h"
#include "ink/sys/crc.h"
#include "ink/sys/data.h"
#include "ink/sys/layout.h"
#include "ink/sys/members.h"
#include "ink/sys/power.h"
#include "ink/sys/res.h"
#include "ink/sys/exec.h"
#include "ink/sys/state.h"
#include "ink/sys/storage.h"
#include "ink/sys/svc.h"

#define MAX_COLLECTIONS 32
#define MAX_DEFS 32
#define MAX_TOMBSTONES 256
#define MEM_QUOTA (2u * 1024u * 1024u) /* all collections, PSRAM */
#define PEND_MAX (256u * 1024u)        /* of one collection, waiting to be written */
#define LAZY_US 60000000ull
#define RETRY_FIRST_US 5000000ull
#define RETRY_MAX_US 300000000ull
#define QUEUE_FULL_US 100000ull        /* the storage queue had no room */
#define COMPACT_MIN 8192u              /* the log may grow to this, or half of the snapshot */
#define COMPACT_RETRY_US 30000000ull   /* a snapshot that could not be written */

#define SNAP_MAGIC 0x31424449u /* "IDB1" */
#define SNAP_FORMAT 1
#define FRAME_SYNC 0xA55Au
#define OP_PUT 1
#define OP_DEL 2

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint16_t format;
    uint16_t schema;
    uint32_t count, last_ver, next_id, log_gen, acked_ver;
    uint32_t crc; /* of the header before this field and all frames */
} snap_hdr_t;

typedef struct __attribute__((packed)) {
    uint16_t sync;
    uint8_t op, src;
    uint32_t ver, id;
    int64_t sort, updated_s;
    uint16_t len;
    uint8_t schema, rsv;
} frame_t; /* then the content, then u32 crc of both */

_Static_assert(sizeof(frame_t) == 32, "frame header");
_Static_assert(sizeof(snap_hdr_t) == 32, "snapshot header");

#define FRAME_SIZE(len) (sizeof(frame_t) + (size_t)(len) + 4u)

typedef struct {
    uint32_t id, ver;
    int64_t sort, updated_s;
    uint16_t len;
    uint8_t src, schema;
    bool dead;
    uint8_t data[];
} rec_t;

typedef struct {
    int64_t sort;
    uint32_t id;
    rec_t *r;
} idx_t;

struct ink_db {
    const ink_db_def_t *def;
    char member[INK_MEMBER_ID_MAX]; /* "" = device data */
    bool used, loading, loaded;
    int refs;
    uint32_t last_use;
    /* records */
    idx_t *idx; /* by (sort, id), tombstones included */
    int n, cap, live, tombs;
    size_t bytes; /* of record contents */
    uint32_t last_ver, next_id, acked_ver, durable_ver;
    /* the log */
    uint32_t gen, snap_gen;
    size_t gen_bytes, old_bytes, snap_bytes; /* in the log in use, in the older log, in the snapshot */
    uint8_t *pend; /* frames waiting */
    size_t pend_len, pend_cap;
    uint32_t pend_ver;
    uint8_t *fly; /* the batch handed to the storage service, or waiting to be tried again */
    size_t fly_len;
    uint32_t fly_ver;
    bool in_flight;
    int err;
    uint64_t retry_us, retry_at; /* backoff after a failed batch: new changes do not hurry it */
    ink_svc_deadline_t flush_at, tell_at;
    /* folding */
    bool compacting, compact_wanted;
    uint8_t *snap_buf;
    size_t snap_len;
    uint32_t snap_ver, snap_new_gen;
    uint64_t compact_not_before;
    ink_svc_deadline_t compact_at;
};

static struct {
    osal_mutex_t *lock;
    ink_db_t c[MAX_COLLECTIONS];
    const ink_db_def_t *defs[MAX_DEFS];
    int ndefs;
    size_t used;
    uint32_t clock;
    ink_exec_job_t free_job;
    int h_free;
} DB INK_PSRAM_BSS;

static void lock(void) { osal_mutex_lock(DB.lock, OSAL_WAIT_FOREVER); }
static void unlock(void) { osal_mutex_unlock(DB.lock); }

/* ------------------------------------------------------------ memory (the count is atomic: a collection being loaded is private, outside the lock) */

static size_t used_bytes(void) { return __atomic_load_n(&DB.used, __ATOMIC_ACQUIRE); }

static void *mem_get(size_t n) {
    if (used_bytes() + n > MEM_QUOTA) return NULL;
    const char *prev = osal_mem_tag("data");
    void *p = osal_malloc(n, OSAL_MEM_EXTERNAL);
    osal_mem_tag(prev);
    if (p) __atomic_add_fetch(&DB.used, n, __ATOMIC_ACQ_REL);
    return p;
}

static void mem_put(void *p, size_t n) {
    if (!p) return;
    osal_free(p);
    __atomic_sub_fetch(&DB.used, n, __ATOMIC_ACQ_REL);
}

static size_t rec_size(const rec_t *r) { return sizeof(rec_t) + r->len; }

/* ------------------------------------------------------------ paths */

static int log_path(const ink_db_t *c, uint32_t gen, char *out, size_t cap) {
    char name[INK_DB_NAME_MAX + 16];
    snprintf(name, sizeof name, "%s.%u.log", c->def->name, (unsigned)gen);
    return c->member[0] ? ink_layout_member(out, cap, c->member, name) : ink_layout_sys(out, cap, name);
}

static int snap_path(const ink_db_t *c, char *out, size_t cap) {
    char name[INK_DB_NAME_MAX + 8];
    snprintf(name, sizeof name, "%s.snap", c->def->name);
    return c->member[0] ? ink_layout_member(out, cap, c->member, name) : ink_layout_sys(out, cap, name);
}

/* ------------------------------------------------------------ the index */

static int cmp_key(const idx_t *e, int64_t sort, uint32_t id) {
    if (e->sort != sort) return e->sort < sort ? -1 : 1;
    return e->id == id ? 0 : e->id < id ? -1 : 1;
}

/* First position not before (sort, id). */
static int lower_bound(const ink_db_t *c, int64_t sort, uint32_t id) {
    int lo = 0, hi = c->n;
    while (lo < hi) {
        int mid = (lo + hi) / 2;
        if (cmp_key(&c->idx[mid], sort, id) < 0) lo = mid + 1;
        else hi = mid;
    }
    return lo;
}

static int find_id(const ink_db_t *c, uint32_t id) {
    for (int i = 0; i < c->n; i++)
        if (c->idx[i].id == id) return i;
    return -1;
}

static bool idx_room(ink_db_t *c) {
    if (c->n < c->cap) return true;
    int cap = c->cap ? c->cap * 2 : 16;
    idx_t *bigger = mem_get(sizeof(idx_t) * (size_t)cap);
    if (!bigger) return false;
    if (c->n) memcpy(bigger, c->idx, sizeof(idx_t) * (size_t)c->n);
    mem_put(c->idx, sizeof(idx_t) * (size_t)c->cap);
    c->idx = bigger;
    c->cap = cap;
    return true;
}

static void idx_insert(ink_db_t *c, rec_t *r) { /* room checked */
    int at = lower_bound(c, r->sort, r->id);
    memmove(&c->idx[at + 1], &c->idx[at], sizeof(idx_t) * (size_t)(c->n - at));
    osal_cost_units("psram.copy_byte", sizeof(idx_t) * (uint64_t)(c->n - at));
    c->idx[at] = (idx_t){r->sort, r->id, r};
    c->n++;
}

static void idx_remove(ink_db_t *c, int at) {
    memmove(&c->idx[at], &c->idx[at + 1], sizeof(idx_t) * (size_t)(c->n - at - 1));
    osal_cost_units("psram.copy_byte", sizeof(idx_t) * (uint64_t)(c->n - at - 1));
    c->n--;
}

static void rec_forget(ink_db_t *c, int at) { /* the record leaves the index and memory */
    rec_t *r = c->idx[at].r;
    if (r->dead) c->tombs--;
    else {
        c->live--;
        c->bytes -= r->len;
    }
    idx_remove(c, at);
    mem_put(r, rec_size(r));
}

/* The oldest tombstones go first, also when loading: what the hub needs it had time to get. */
static void trim_tombstones(ink_db_t *c) {
    while (c->tombs > MAX_TOMBSTONES) {
        int oldest = -1;
        for (int i = 0; i < c->n; i++)
            if (c->idx[i].r->dead && (oldest < 0 || c->idx[i].r->ver < c->idx[oldest].r->ver)) oldest = i;
        if (oldest < 0) return;
        rec_forget(c, oldest);
        ink_probe_count("data.tombstones_dropped");
    }
}

/* ------------------------------------------------------------ flushing */

static void flush_locked(ink_db_t *c);

static void flush_cb(void *arg) { /* timer task: a batch's time came, or a retry's */
    ink_db_t *c = arg;
    lock();
    flush_locked(c);
    unlock();
}

static void tell_cb(void *arg) { /* timer task: once per trouble, whatever the retries do */
    ink_db_t *c = arg;
    static ink_args_t a INK_PSRAM_BSS; /* the timer task only */
    ink_args_init(&a);
    lock();
    bool valid = c->def != NULL;
    if (valid) {
        ink_args_str(&a, "collection", c->def->name);
        ink_args_str(&a, "member", c->member);
    }
    unlock();
    if (valid) ink_cap_emit("data.write_failed", &a);
}

static void compact_cb(void *arg);

static void want_compaction(ink_db_t *c) { /* under the lock */
    size_t limit = c->snap_bytes / 2 > COMPACT_MIN ? c->snap_bytes / 2 : COMPACT_MIN;
    if (c->compacting || c->compact_wanted || c->gen_bytes + c->old_bytes < limit) return;
    c->compact_wanted = true;
    uint64_t now = osal_now_us();
    ink_svc_at(&c->compact_at, c->compact_not_before > now ? c->compact_not_before : now, compact_cb, c);
}

static void append_done(void *ctx, int result) { /* storage task */
    ink_db_t *c = ctx;
    lock();
    c->in_flight = false;
    if (result == 0) {
        c->gen_bytes += c->fly_len;
        if (c->fly_ver > c->durable_ver) c->durable_ver = c->fly_ver;
        mem_put(c->fly, c->fly_len);
        c->fly = NULL;
        c->fly_len = 0;
        c->err = 0;
        c->retry_us = 0;
        flush_locked(c); /* what piled up meanwhile (a lazy batch waits for its own time only when alone) */
        want_compaction(c);
    } else { /* it stays and is tried again; nothing of it reached the file (LittleFS commits whole) */
        ink_probe_count("data.write_errors");
        c->err = result;
        uint64_t now = osal_now_us();
        if (!c->retry_us) ink_svc_at(&c->tell_at, now, tell_cb, c);
        c->retry_us = c->retry_us ? c->retry_us * 2 : RETRY_FIRST_US;
        if (c->retry_us > RETRY_MAX_US) c->retry_us = RETRY_MAX_US;
        c->retry_at = now + c->retry_us;
        ink_svc_at(&c->flush_at, c->retry_at, flush_cb, c);
    }
    unlock();
}

/* Under the lock. The batch that failed goes first, alone. */
static void flush_locked(ink_db_t *c) {
    if (c->in_flight) return;
    if (c->fly && osal_now_us() < c->retry_at) return; /* it failed: its time comes (flush_at) */
    if (!c->fly) {
        if (!c->pend_len) return;
        c->fly = c->pend;
        c->fly_len = c->pend_len;
        c->fly_ver = c->pend_ver;
        __atomic_sub_fetch(&DB.used, c->pend_cap - c->pend_len, __ATOMIC_ACQ_REL); /* counted by its length from here (the block is as it was) */
        c->pend = NULL;
        c->pend_len = c->pend_cap = 0;
    }
    char path[INK_LAYOUT_PATH_MAX];
    if (log_path(c, c->gen, path, sizeof path) < 0) return;
    c->in_flight = true;
    if (ink_storage_append(path, c->fly, c->fly_len, append_done, c) != 0) {
        c->in_flight = false;
        ink_svc_at(&c->flush_at, osal_now_us() + QUEUE_FULL_US, flush_cb, c);
    }
}

/* Under the lock: one change, as a frame, into what waits. False: no room for it. */
static bool frame_add(ink_db_t *c, uint8_t op, const rec_t *r, const void *content) {
    size_t need = FRAME_SIZE(r->len);
    if (c->pend_len + need > PEND_MAX) return false;
    if (c->pend_len + need > c->pend_cap) {
        size_t cap = c->pend_cap ? c->pend_cap * 2 : 512;
        while (cap < c->pend_len + need) cap *= 2;
        uint8_t *bigger = mem_get(cap);
        if (!bigger) return false;
        if (c->pend_len) memcpy(bigger, c->pend, c->pend_len);
        mem_put(c->pend, c->pend_cap);
        c->pend = bigger;
        c->pend_cap = cap;
    }
    frame_t f = {.sync = FRAME_SYNC, .op = op, .src = r->src, .ver = r->ver, .id = r->id, .sort = r->sort, .updated_s = r->updated_s,
                 .len = r->len, .schema = r->schema};
    uint8_t *p = c->pend + c->pend_len;
    memcpy(p, &f, sizeof f);
    if (r->len) memcpy(p + sizeof f, content, r->len);
    uint32_t crc = ink_crc32(0, p, sizeof f + r->len);
    memcpy(p + sizeof f + r->len, &crc, 4);
    osal_cost_units("psram.copy_byte", need);
    c->pend_len += need;
    c->pend_ver = r->ver;
    return true;
}

/* ------------------------------------------------------------ folding the log into a snapshot */

static void free_job(void *arg) { /* exec.bg: it walks the file system */
    (void)arg;
    ink_data_refresh_state();
}

static void compact_done(void *ctx, int result) { /* storage task */
    ink_db_t *c = ctx;
    lock();
    size_t written = c->snap_len;
    mem_put(c->snap_buf, written);
    c->snap_buf = NULL;
    c->compacting = false;
    if (result == 0) {
        uint32_t new_gen = c->snap_new_gen;
        c->snap_gen = new_gen;
        c->snap_bytes = c->snap_len;
        c->old_bytes = 0;
        if (c->snap_ver > c->durable_ver) c->durable_ver = c->snap_ver;
        char path[INK_LAYOUT_PATH_MAX]; /* the older log is not needed any more; if this cannot be queued, the next load removes it */
        if (new_gen > 0 && log_path(c, new_gen - 1, path, sizeof path) > 0) ink_storage_remove(path, NULL, NULL);
        ink_probe_count("data.compactions");
    } else {
        ink_probe_count("data.compaction_errors");
        c->compact_not_before = osal_now_us() + COMPACT_RETRY_US; /* nothing was lost: the logs are all there */
    }
    want_compaction(c);
    unlock();
    if (result == 0) ink_exec_post(INK_EXEC_BG, &DB.free_job, free_job, NULL);
}

/* Fold it, on exec.bg. Copied under the lock, the checksums outside. */
static ink_job_step_t compact_step(void *ctx) {
    ink_db_t *c = ctx;
    uint8_t *buf = NULL;
    size_t cap = 0, len = 0;
    uint32_t count = 0;
    snap_hdr_t h = {.magic = SNAP_MAGIC, .format = SNAP_FORMAT};
    for (int tries = 0; tries < 4; tries++) {
        lock();
        size_t need = sizeof(snap_hdr_t);
        for (int i = 0; i < c->n; i++) need += FRAME_SIZE(c->idx[i].r->len);
        if (need <= cap) {
            /* a log in use that the old snapshot continues from moves on: appends go to a new file */
            if (c->gen == c->snap_gen) {
                c->old_bytes += c->gen_bytes;
                c->gen_bytes = 0;
                c->gen++;
            }
            uint8_t *p = buf + sizeof h;
            count = 0;
            for (int i = 0; i < c->n; i++) {
                const rec_t *r = c->idx[i].r;
                if (r->dead && r->ver <= c->acked_ver) continue; /* the hub has it: the tombstone may go */
                frame_t f = {.sync = FRAME_SYNC, .op = r->dead ? OP_DEL : OP_PUT, .src = r->src, .ver = r->ver, .id = r->id, .sort = r->sort,
                             .updated_s = r->updated_s, .len = r->len, .schema = r->schema};
                memcpy(p, &f, sizeof f);
                if (r->len) memcpy(p + sizeof f, r->data, r->len);
                p += FRAME_SIZE(r->len);
                count++;
            }
            len = (size_t)(p - buf);
            h.count = count;
            h.schema = c->def->schema;
            h.last_ver = c->last_ver;
            h.next_id = c->next_id;
            h.log_gen = c->gen;
            h.acked_ver = c->acked_ver;
            c->snap_ver = c->last_ver;
            c->snap_new_gen = c->gen;
            osal_cost_units("psram.copy_byte", len);
            unlock();
            break;
        }
        unlock();
        const char *prev = osal_mem_tag("data");
        uint8_t *bigger = osal_malloc(need + need / 8, OSAL_MEM_EXTERNAL); /* outside the quota: it is short-lived */
        osal_mem_tag(prev);
        osal_free(buf);
        buf = bigger;
        cap = bigger ? need + need / 8 : 0;
        if (!buf) break;
    }
    if (!buf || !len) {
        osal_free(buf);
        lock();
        c->compacting = false;
        c->compact_not_before = osal_now_us() + COMPACT_RETRY_US;
        unlock();
        ink_probe_count("data.compaction_errors");
        return INK_JOB_FAILED;
    }
    /* checksums */
    for (uint8_t *p = buf + sizeof h; p < buf + len;) {
        frame_t f;
        memcpy(&f, p, sizeof f);
        uint32_t crc = ink_crc32(0, p, sizeof f + f.len);
        memcpy(p + sizeof f + f.len, &crc, 4);
        p += FRAME_SIZE(f.len);
        osal_preempt_point();
    }
    h.crc = ink_crc32(0, &h, offsetof(snap_hdr_t, crc));
    h.crc = ink_crc32(h.crc, buf + sizeof h, len - sizeof h);
    memcpy(buf, &h, sizeof h);
    char path[INK_LAYOUT_PATH_MAX];
    lock();
    int pn = snap_path(c, path, sizeof path);
    __atomic_add_fetch(&DB.used, len, __ATOMIC_ACQ_REL); /* now it counts */
    c->snap_buf = buf;
    c->snap_len = len;
    int r = pn > 0 ? ink_storage_write_borrow(path, buf, len, compact_done, c) : -1;
    if (r != 0) { /* the queue is full: next time */
        mem_put(c->snap_buf, c->snap_len);
        c->snap_buf = NULL;
        c->compacting = false;
        c->compact_not_before = osal_now_us() + COMPACT_RETRY_US / 10;
        c->compact_wanted = false;
        want_compaction(c);
    }
    unlock();
    return INK_JOB_DONE;
}

static bool battery_low(void) {
    int64_t low = 0;
    return ink_state_get_int(ink_state_find("battery.low"), &low) && low;
}

static void compact_cb(void *arg) { /* timer task */
    ink_db_t *c = arg;
    bool hold_off = battery_low(); /* it can wait: the flash work is not worth what is left (nothing is lost, the logs are all there) */
    lock();
    bool go = c->compact_wanted && !c->compacting && !hold_off;
    c->compact_wanted = false;
    if (hold_off) {
        c->compact_not_before = osal_now_us() + COMPACT_RETRY_US;
        want_compaction(c);
    }
    if (go) c->compacting = true;
    unlock();
    if (!go) return;
    ink_job_t j = {.name = "data.compact", .step = compact_step, .ctx = c, .step_budget_ms = 200};
    if (ink_job_submit(&j) < 0) {
        lock();
        c->compacting = false;
        c->compact_not_before = osal_now_us() + COMPACT_RETRY_US;
        want_compaction(c);
        unlock();
    }
}

/* ------------------------------------------------------------ loading (the collection is still private) */

/* One frame at p: its total size, or 0 if it is not a good frame. */
static size_t frame_check(const uint8_t *p, size_t left, frame_t *f) {
    if (left < sizeof *f + 4) return 0;
    memcpy(f, p, sizeof *f);
    if (f->sync != FRAME_SYNC || (f->op != OP_PUT && f->op != OP_DEL) || f->len > INK_DB_RECORD_MAX) return 0;
    size_t total = FRAME_SIZE(f->len);
    if (left < total) return 0;
    uint32_t crc;
    memcpy(&crc, p + sizeof *f + f->len, 4);
    return ink_crc32(0, p, sizeof *f + f->len) == crc ? total : 0;
}

static bool apply_frame(ink_db_t *c, const frame_t *f, const uint8_t *content) {
    int at = find_id(c, f->id);
    if (at >= 0) rec_forget(c, at);
    else if (!idx_room(c)) return false;
    rec_t *r = mem_get(sizeof(rec_t) + (f->op == OP_PUT ? f->len : 0));
    if (!r) return false;
    *r = (rec_t){.id = f->id, .ver = f->ver, .sort = f->sort, .updated_s = f->updated_s, .len = f->op == OP_PUT ? f->len : 0,
                 .src = f->src, .schema = f->schema, .dead = f->op == OP_DEL};
    if (r->len) memcpy(r->data, content, r->len);
    idx_insert(c, r);
    if (r->dead) c->tombs++;
    else {
        c->live++;
        c->bytes += r->len;
    }
    if (f->id >= c->next_id) c->next_id = f->id + 1;
    if (f->ver > c->last_ver) c->last_ver = f->ver;
    return true;
}

/* 0, or -1 when out of memory. After a bad frame it looks for the next good one: what was appended
 * behind a damaged spot still counts. `is_log`: frames are in change order and what is not newer than
 * what is already here is skipped; a snapshot's are in sort order and all count. */
static int replay(ink_db_t *c, const uint8_t *buf, size_t len, bool is_log, bool *bad) {
    size_t off = 0;
    while (off < len) {
        frame_t f;
        size_t n = frame_check(buf + off, len - off, &f);
        if (!n) {
            *bad = true;
            off++;
            continue;
        }
        if ((!is_log || f.ver > c->last_ver) && !apply_frame(c, &f, buf + off + sizeof f)) return -1;
        off += n;
    }
    return 0;
}

/* The snapshot's header is gone, and with it where the logs are: the generation is looked for. */
#define GEN_SEARCH 256

static uint32_t find_log_gen(const ink_db_t *c) {
    char path[INK_LAYOUT_PATH_MAX];
    for (uint32_t g = 0; g < GEN_SEARCH; g++)
        if (log_path(c, g, path, sizeof path) > 0 && ink_storage_size(path) >= 0) return g;
    return 0;
}

static int load(ink_db_t *c) {
    char path[INK_LAYOUT_PATH_MAX];
    void *buf;
    size_t len;
    bool bad = false;
    c->next_id = 1;
    if (snap_path(c, path, sizeof path) < 0) return -1;
    int got = ink_storage_load(path, &buf, &len);
    if (got == INK_STORAGE_LOAD_FAILED) { /* it is there and cannot be had now: opened empty, the next write would bury it */
        ink_probe_count("data.load_error");
        return -1;
    }
    if (got == 0 && buf) {
        snap_hdr_t h;
        bool ok = len >= sizeof h;
        bool magic = false;
        if (ok) {
            memcpy(&h, buf, sizeof h);
            ok = magic = h.magic == SNAP_MAGIC;
        }
        if (ok && h.format > SNAP_FORMAT) { /* a newer firmware wrote this: never overwrite it */
            ink_storage_free(buf);
            ink_probe_count("data.format_too_new");
            return -1;
        }
        if (ok) {
            uint32_t crc = ink_crc32(0, &h, offsetof(snap_hdr_t, crc));
            ok = ink_crc32(crc, (uint8_t *)buf + sizeof h, len - sizeof h) == h.crc;
        }
        if (ok) {
            if (replay(c, (uint8_t *)buf + sizeof h, len - sizeof h, false, &bad) != 0) {
                ink_storage_free(buf);
                return -1;
            }
            if (h.last_ver > c->last_ver) c->last_ver = h.last_ver; /* logs: only what came after it */
            if (h.next_id > c->next_id) c->next_id = h.next_id;
            c->gen = c->snap_gen = h.log_gen;
            c->acked_ver = h.acked_ver;
            c->snap_bytes = len;
        } else { /* the logs still have what came after it */
            ink_probe_count("data.snapshot_bad");
            bad = true;
            c->gen = c->snap_gen = magic ? h.log_gen : find_log_gen(c);
        }
        ink_storage_free(buf);
    }
    for (uint32_t g = c->snap_gen; g <= c->snap_gen + 1; g++) {
        if (log_path(c, g, path, sizeof path) < 0) return -1;
        got = ink_storage_load(path, &buf, &len);
        if (got == INK_STORAGE_LOAD_FAILED) {
            ink_probe_count("data.load_error");
            return -1;
        }
        if (got != 0) continue;
        c->gen = g;
        if (g == c->snap_gen + 1) c->old_bytes = c->gen_bytes;
        c->gen_bytes = len;
        if (buf) {
            bool file_bad = false;
            int r = replay(c, buf, len, true, &file_bad);
            ink_storage_free(buf);
            if (r != 0) return -1;
            if (file_bad) {
                ink_probe_count("data.log_bad");
                bad = true;
            }
        }
    }
    if (c->snap_gen > 0 && log_path(c, c->snap_gen - 1, path, sizeof path) > 0) ink_storage_remove(path, NULL, NULL); /* left by a power cut */
    trim_tombstones(c);
    c->durable_ver = c->last_ver;
    if (bad) c->gen_bytes = c->gen_bytes > COMPACT_MIN ? c->gen_bytes : COMPACT_MIN; /* fold soon: drops the damaged part */
    return 0;
}

static void free_records(ink_db_t *c) {
    for (int i = 0; i < c->n; i++) mem_put(c->idx[i].r, rec_size(c->idx[i].r));
    mem_put(c->idx, sizeof(idx_t) * (size_t)c->cap);
    mem_put(c->pend, c->pend_cap);
    c->idx = NULL;
    c->pend = NULL;
    c->n = c->cap = c->live = c->tombs = 0;
    c->bytes = c->pend_len = c->pend_cap = 0;
}

/* ------------------------------------------------------------ cache of closed collections */

static bool evictable(const ink_db_t *c) {
    return c->used && c->loaded && !c->refs && !c->pend_len && !c->fly && !c->in_flight && !c->compacting && !c->compact_wanted;
}

/* The least recently used closed collection with nothing left to write goes, with this many bytes (an empty one counts: its
 * slot is free). Under the lock. */
static bool evict_one(size_t *freed) {
    ink_db_t *best = NULL;
    for (int i = 0; i < MAX_COLLECTIONS; i++)
        if (evictable(&DB.c[i]) && (!best || DB.c[i].last_use < best->last_use)) best = &DB.c[i];
    if (!best) return false;
    size_t before = used_bytes();
    free_records(best);
    ink_svc_cancel(&best->flush_at);
    ink_svc_cancel(&best->tell_at);
    ink_svc_cancel(&best->compact_at);
    memset(best, 0, sizeof *best);
    ink_probe_count("data.evictions");
    if (freed) *freed = before - used_bytes();
    return true;
}

static size_t reclaim(void *ctx, size_t want) { /* never from under the lock: nothing here calls ink_mem_alloc */
    (void)ctx;
    size_t got = 0;
    lock();
    size_t n = 0;
    while (got < want && evict_one(&n)) got += n;
    unlock();
    return got;
}

/* ------------------------------------------------------------ the API */

ink_db_t *ink_db_open(const ink_db_def_t *def, const char *member) {
    if (!DB.lock || !def || !def->name || strlen(def->name) >= INK_DB_NAME_MAX || !ink_layout_name_ok(def->name)) return NULL;
    char who[INK_MEMBER_ID_MAX] = "";
    if (def->scope == INK_DB_MEMBER) {
        const char *id = member ? member : ink_member_current()->id;
        if (!ink_layout_member_ok(id)) return NULL;
        snprintf(who, sizeof who, "%s", id);
    }
    for (;;) {
        ink_db_t *slot = NULL, *found = NULL;
        lock();
        for (int i = 0; i < MAX_COLLECTIONS && !found; i++) {
            ink_db_t *c = &DB.c[i];
            if (c->used && c->def && !strcmp(c->def->name, def->name) && !strcmp(c->member, who)) found = c;
            else if (!c->used && !slot) slot = c;
        }
        if (found && found->loading) { /* someone else is reading it */
            unlock();
            osal_task_delay_us(5000);
            continue;
        }
        if (found) {
            found->refs++;
            found->last_use = ++DB.clock;
            unlock();
            return found;
        }
        if (!slot && evict_one(NULL)) {
            unlock();
            continue;
        }
        if (!slot) {
            unlock();
            ink_probe_count("data.too_many_collections");
            return NULL;
        }
        *slot = (ink_db_t){.def = def, .used = true, .loading = true, .refs = 1, .last_use = ++DB.clock};
        snprintf(slot->member, sizeof slot->member, "%s", who);
        unlock();
        int r = load(slot); /* waits for the flash; the slot is private until `loading` drops */
        lock();
        if (r != 0) {
            free_records(slot);
            memset(slot, 0, sizeof *slot);
            unlock();
            ink_probe_count("data.load_failed");
            return NULL;
        }
        slot->loading = false;
        slot->loaded = true;
        want_compaction(slot);
        unlock();
        return slot;
    }
}

void ink_db_close(ink_db_t *db) {
    if (!db) return;
    lock();
    if (db->refs > 0) db->refs--;
    unlock();
}

/* Under the lock: `adding` more bytes and, if it is a new record, one more record fit the collection's
 * limits; the oldest go if the collection says so. */
static bool make_room(ink_db_t *c, size_t adding, bool one_more) {
    const ink_db_def_t *d = c->def;
    size_t max_bytes = d->max_bytes ? d->max_bytes : 256u * 1024u;
    uint32_t more = one_more ? 1 : 0;
    bool over_n = d->max_records && (uint32_t)c->live + more > d->max_records, over_b = c->bytes + adding > max_bytes;
    if (!over_n && !over_b) return true;
    if (!d->drop_oldest || adding > max_bytes) return false;
    while ((d->max_records && (uint32_t)c->live + more > d->max_records) || c->bytes + adding > max_bytes) {
        int oldest = -1;
        for (int i = 0; i < c->n && oldest < 0; i++)
            if (!c->idx[i].r->dead) oldest = i;
        if (oldest < 0) return false;
        rec_t *r = c->idx[oldest].r;
        rec_t t = *r;
        t.ver = c->last_ver + 1;
        t.len = 0;
        t.dead = true;
        if (!frame_add(c, OP_DEL, &t, NULL)) return false;
        c->last_ver = t.ver;
        rec_forget(c, oldest); /* the frame says it is gone; no tombstone kept in memory for what merely aged out */
        ink_probe_count("data.dropped_oldest");
    }
    return true;
}

int ink_db_put(ink_db_t *db, const ink_db_write_t *w, uint32_t *out_id) {
    if (!db || !w || (w->len && !w->data)) return INK_DB_ERR_ARG;
    if (w->len > INK_DB_RECORD_MAX) return INK_DB_ERR_TOO_LARGE;
    lock();
    int old = w->id ? find_id(db, w->id) : -1;
    bool is_new = old < 0 || db->idx[old].r->dead;
    size_t old_len = is_new ? 0 : db->idx[old].r->len;
    rec_t *nr;
    while (!(nr = mem_get(sizeof(rec_t) + w->len)) && evict_one(NULL)) {} /* over the store's quota: closed collections go first */
    if (!nr || (old < 0 && !idx_room(db))) {
        mem_put(nr, nr ? sizeof(rec_t) + w->len : 0);
        unlock();
        return nr ? INK_DB_ERR_NOMEM : INK_DB_ERR_FULL;
    }
    if (!make_room(db, w->len > old_len ? w->len - old_len : 0, is_new)) {
        mem_put(nr, sizeof(rec_t) + w->len);
        unlock();
        return INK_DB_ERR_FULL;
    }
    int64_t wall = osal_wall_us();
    *nr = (rec_t){.id = w->id ? w->id : db->next_id, .ver = db->last_ver + 1, .sort = w->sort, .updated_s = wall > 0 ? wall / 1000000 : 0,
                  .len = (uint16_t)w->len, .src = w->src, .schema = w->schema ? w->schema : db->def->schema};
    if (w->len) memcpy(nr->data, w->data, w->len);
    if (!frame_add(db, OP_PUT, nr, w->data)) {
        mem_put(nr, sizeof(rec_t) + w->len);
        unlock();
        ink_probe_count("data.write_backlog_full");
        return INK_DB_ERR_FULL;
    }
    old = find_id(db, nr->id);
    if (old >= 0) rec_forget(db, old);
    idx_insert(db, nr); /* forgetting freed a position, or idx_room made one */
    db->live++;
    db->bytes += nr->len;
    db->last_ver = nr->ver;
    if (nr->id >= db->next_id) db->next_id = nr->id + 1;
    if (out_id) *out_id = nr->id;
    if (w->flags & INK_DB_LAZY) {
        if (!db->flush_at.armed) ink_svc_at(&db->flush_at, osal_now_us() + LAZY_US, flush_cb, db);
    } else flush_locked(db);
    unlock();
    return INK_DB_OK;
}

uint32_t ink_db_new_id(ink_db_t *db) {
    if (!db) return 0;
    lock();
    uint32_t id = db->next_id++;
    unlock();
    return id;
}

int ink_db_del(ink_db_t *db, uint32_t id, uint8_t src) {
    if (!db) return INK_DB_ERR_ARG;
    lock();
    int at = find_id(db, id);
    if (at < 0 || db->idx[at].r->dead) {
        unlock();
        return INK_DB_ERR_NOT_FOUND;
    }
    rec_t *old = db->idx[at].r;
    rec_t *t = mem_get(sizeof(rec_t));
    if (!t) {
        unlock();
        return INK_DB_ERR_NOMEM;
    }
    int64_t wall = osal_wall_us();
    *t = (rec_t){.id = id, .ver = db->last_ver + 1, .sort = old->sort, .updated_s = wall > 0 ? wall / 1000000 : 0, .src = src, .schema = old->schema, .dead = true};
    if (!frame_add(db, OP_DEL, t, NULL)) {
        mem_put(t, sizeof(rec_t));
        unlock();
        return INK_DB_ERR_FULL;
    }
    rec_forget(db, at);
    idx_insert(db, t);
    db->tombs++;
    db->last_ver = t->ver;
    trim_tombstones(db);
    flush_locked(db);
    unlock();
    return INK_DB_OK;
}

static void fill(const rec_t *r, ink_rec_t *out, void *buf, size_t cap) {
    if (out) *out = (ink_rec_t){.id = r->id, .ver = r->ver, .sort = r->sort, .updated_s = r->updated_s, .len = r->len, .src = r->src,
                                .schema = r->schema, .deleted = r->dead};
    if (buf && cap) {
        size_t n = r->len < cap ? r->len : cap;
        if (n) memcpy(buf, r->data, n);
        if (n < cap) ((char *)buf)[n] = 0;
        osal_cost_units("psram.copy_byte", n);
    }
}

int ink_db_get(ink_db_t *db, uint32_t id, ink_rec_t *out, void *buf, size_t cap) {
    if (!db) return INK_DB_ERR_ARG;
    lock();
    int at = find_id(db, id);
    bool hit = at >= 0 && !db->idx[at].r->dead;
    if (hit) fill(db->idx[at].r, out, buf, cap);
    unlock();
    return hit ? INK_DB_OK : INK_DB_ERR_NOT_FOUND;
}

int ink_db_next(ink_db_t *db, ink_db_cursor_t *c, ink_rec_t *out, void *buf, size_t cap) {
    if (!db || !c) return INK_DB_ERR_ARG;
    lock();
    int i, step = c->desc ? -1 : 1;
    if (!c->started) i = c->desc ? db->n - 1 : 0;
    else {
        i = lower_bound(db, c->sort, c->id); /* the first not before the cursor */
        if (c->desc) i--;
        else if (i < db->n && cmp_key(&db->idx[i], c->sort, c->id) == 0) i++;
    }
    while (i >= 0 && i < db->n && db->idx[i].r->dead) i += step;
    int r = 0;
    if (i >= 0 && i < db->n) {
        fill(db->idx[i].r, out, buf, cap);
        c->sort = db->idx[i].sort;
        c->id = db->idx[i].id;
        c->started = true;
        r = 1;
    }
    unlock();
    return r;
}

int ink_db_count(ink_db_t *db) {
    if (!db) return INK_DB_ERR_ARG;
    lock();
    int n = db->live;
    unlock();
    return n;
}

int ink_db_changes(ink_db_t *db, uint32_t *since_ver, ink_rec_t *out, void *buf, size_t cap) {
    if (!db || !since_ver) return INK_DB_ERR_ARG;
    lock();
    const rec_t *best = NULL;
    for (int i = 0; i < db->n; i++) {
        const rec_t *r = db->idx[i].r;
        if (r->ver > *since_ver && (!best || r->ver < best->ver)) best = r;
    }
    if (best) {
        fill(best, out, buf, cap);
        *since_ver = best->ver;
    }
    unlock();
    return best ? 1 : 0;
}

void ink_db_ack(ink_db_t *db, uint32_t ver) {
    if (!db) return;
    lock();
    if (ver > db->acked_ver) db->acked_ver = ver;
    for (int i = db->n - 1; i >= 0; i--)
        if (db->idx[i].r->dead && db->idx[i].r->ver <= db->acked_ver) rec_forget(db, i);
    unlock();
}

uint32_t ink_db_last_ver(ink_db_t *db) {
    if (!db) return 0;
    lock();
    uint32_t v = db->last_ver;
    unlock();
    return v;
}

uint32_t ink_db_durable_ver(ink_db_t *db) {
    if (!db) return 0;
    lock();
    uint32_t v = db->durable_ver;
    unlock();
    return v;
}

uint32_t ink_db_acked_ver(ink_db_t *db) {
    if (!db) return 0;
    lock();
    uint32_t v = db->acked_ver;
    unlock();
    return v;
}

void ink_db_flush(ink_db_t *db) {
    if (!db) return;
    lock();
    flush_locked(db);
    unlock();
}

int ink_db_wait(ink_db_t *db, uint32_t ver, int64_t timeout_us) {
    ink_db_flush(db);
    uint64_t end = osal_now_us() + (uint64_t)timeout_us;
    while (ink_db_durable_ver(db) < ver) {
        if (osal_now_us() >= end) return -1;
        osal_task_delay_us(10000);
    }
    return 0;
}

int ink_db_error(ink_db_t *db) {
    if (!db) return INK_DB_ERR_ARG;
    lock();
    int e = db->err;
    unlock();
    return e;
}

void ink_db_forget_member(const char *member) {
    if (!DB.lock) return;
    lock();
    for (int i = 0; i < MAX_COLLECTIONS; i++) {
        ink_db_t *c = &DB.c[i];
        if (!c->used || !c->loaded || strcmp(c->member, member) || c->refs || c->fly || c->in_flight || c->compacting) continue;
        ink_svc_cancel(&c->flush_at);
        ink_svc_cancel(&c->tell_at);
        ink_svc_cancel(&c->compact_at);
        c->compact_wanted = false;
        free_records(c);
        memset(c, 0, sizeof *c);
    }
    unlock();
}

void ink_db_flush_all(void) {
    if (!DB.lock) return;
    lock();
    for (int i = 0; i < MAX_COLLECTIONS; i++)
        if (DB.c[i].used && DB.c[i].loaded) flush_locked(&DB.c[i]);
    unlock();
}

bool ink_db_idle(void) {
    if (!DB.lock) return true;
    lock();
    bool idle = true;
    for (int i = 0; i < MAX_COLLECTIONS && idle; i++) {
        const ink_db_t *c = &DB.c[i];
        idle = !c->used || (!c->pend_len && !c->fly && !c->in_flight);
    }
    unlock();
    return idle;
}

/* ------------------------------------------------------------ the registry */

int ink_db_register(const ink_db_def_t *def) {
    if (!DB.lock || !def) return -1;
    lock();
    int r = -1;
    if (DB.ndefs < MAX_DEFS) {
        DB.defs[DB.ndefs++] = def;
        r = 0;
    }
    unlock();
    return r;
}

const ink_db_def_t *ink_db_find_def(const char *name) {
    if (!DB.lock || !name) return NULL;
    lock();
    const ink_db_def_t *d = NULL;
    for (int i = 0; i < DB.ndefs && !d; i++)
        if (!strcmp(DB.defs[i]->name, name)) d = DB.defs[i];
    unlock();
    return d;
}

const ink_db_def_t *ink_db_def_at(int i) {
    if (!DB.lock) return NULL;
    lock();
    const ink_db_def_t *d = i >= 0 && i < DB.ndefs ? DB.defs[i] : NULL;
    unlock();
    return d;
}

static void on_power(void *ctx, ink_power_state_t from, ink_power_state_t to, uint64_t cause_us) {
    (void)ctx;
    (void)from;
    (void)cause_us;
    if (to == INK_PWR_LOCKED) ink_db_flush_all(); /* before the chip sleeps (the power service waits for the queue) */
}

int ink_data_caps_init(void);

static const ink_state_def_t ST_USER_FREE = {.id = "storage.user_free", .type = INK_ST_INT, .title = "内部存储还剩多少字节（设置、通知、备忘这些放在这里）"};

void ink_data_refresh_state(void) {
    uint64_t total = 0, free_bytes = 0;
    if (ink_storage_volume(&total, &free_bytes) == 0) ink_state_set_int(DB.h_free, (int64_t)free_bytes);
}

int ink_data_init(void) {
    if (DB.lock) return 0;
    const char *prev = osal_mem_tag("data");
    DB.lock = osal_mutex_create();
    osal_mem_tag(prev);
    if (!DB.lock) return -1;
    DB.h_free = ink_state_define(&ST_USER_FREE);
    if (ink_mem_reclaimer("data", 20, reclaim, NULL) != 0 || ink_power_subscribe(on_power, NULL) != 0) return -1;
    return ink_data_caps_init();
}
