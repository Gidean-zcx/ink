/*
 * Storage service task: the ONLY code that touches /user (internal flash).
 *
 * Its stack is in internal RAM. On the MCU every flash API call — reads too —
 * disables the caches; a task whose stack is in PSRAM would lose its stack
 * (ESP-IDF asserts), so other tasks never call the flash API themselves:
 * reads are executed here on their behalf (the caller blocks), writes are
 * queued.
 *
 * No flash operation runs while the panel scans: every read and write holds
 * the display's flash guard, which waits for the waveform in flight to end
 * and keeps the next one from starting until the flash is done (a flash
 * operation freezes the feeders and masks the LCD refill interrupt; IDF's
 * cross-core IPC also waits behind the max-priority feeders). Writes
 * additionally wait until nothing is queued for the panel and no one holds
 * writes off (audio streaming).
 *
 * Writes are atomic (temp file + rename); repeated writes of one path
 * coalesce. Appends and removes go through the same queue, in the order they
 * were accepted. Reads see queued writes, including the one being written; then
 * the read cache (small files and "does not exist", external RAM, LRU),
 * filled by reads, writes and background prefetches; only then the flash.
 * The cache never holds an older version than the newest write: a write
 * replaces the entry, or drops it when the new content does not fit (too
 * large, no memory) or the write failed; what a read or prefetch got from
 * the flash is not cached while a newer write for the path is queued.
 */
#include <stdio.h>
#include <string.h>

#include "ink/hal_display.h"
#include "ink/hal_storage.h"
#include "ink/osal.h"
#include "ink/probe.h"
#include "ink/sys/display.h"
#include "ink/sys/res.h"
#include "ink/sys/storage.h"

#define MAX_JOBS 8
#define ST_STACK 3584
#define ST_PRIO 3
#define IDLE_POLL_US 50000
#define MAX_DEFER_US 30000000ull /* writes held off longer than this are forced (counted) */
/* direct-drive panels (async) can pause between drive phases for a flash operation: a write waits
 * this long for a quiet panel, then goes ahead and pauses the drive (a hiccup, no garbage) instead
 * of waiting behind continuous updates (AI text streaming keeps such a panel busy) */
#define ASYNC_PANEL_WAIT_US 500000
#define CACHE_MAX 32
#define CACHE_FILE_MAX 1024 /* larger files are not cached */
#define PREFETCH_MAX 16

typedef struct {
    char *path; /* path and data share one external-RAM block */
    uint8_t *data;
    int len; /* -1: the file does not exist */
    uint32_t used_at;
} centry_t;

typedef enum { J_WRITE = 0, J_APPEND, J_REMOVE } job_kind_t;

typedef struct {
    char *path; /* path and (copied) data share one external-RAM block */
    const uint8_t *data;
    size_t len;
    uint64_t queued_us;
    uint32_t seq;   /* content version: the newest queued content of a path wins */
    uint32_t order; /* place in line, kept when a write is merged into it */
    uint8_t kind;
    bool used;
    bool inflight; /* taken by the storage task: stays visible to reads, never coalesced into */
    ink_storage_done_fn done;
    void *ctx;
} job_t;

typedef struct {
    const char *path;
    void *buf;
    size_t cap;
    int result;
    void (*fn)(void *arg); /* other work to run on the storage task, instead of a small-file read */
    void *arg;
    bool guarded;
} read_req_t;

static struct {
    osal_mutex_t *lock;      /* jobs, pending, holds */
    osal_mutex_t *read_lock; /* one read request at a time */
    osal_sem_t *kick;        /* work for the storage task */
    osal_sem_t *read_done;
    osal_task_t *task;
    bool panel_async;        /* the panel can pause between phases (hal_panel_info_t.async_updates) */
    job_t *jobs;             /* MAX_JOBS, external RAM */
    read_req_t *rreq;        /* pending read (under lock) */
    int pending;
    int failed; /* writes that failed (e.g. /user full) since boot */
    int holds;
    uint32_t seq, order;
    centry_t *cache; /* CACHE_MAX, external RAM */
    uint32_t clock;
    char *prefetch[PREFETCH_MAX]; /* paths waiting to be cached */
    uint8_t *scratch;             /* CACHE_FILE_MAX + 1, external RAM (storage task) */
    char *path_buf;               /* PATH_BUF, external RAM (storage task): its stack is internal RAM and LittleFS goes deep */
} ST;

/* ------------------------------------------------------------ read cache (under ST.lock) */

static centry_t *cache_find(const char *path) {
    for (int i = 0; i < CACHE_MAX; i++)
        if (ST.cache[i].path && !strcmp(ST.cache[i].path, path)) {
            ST.cache[i].used_at = ++ST.clock;
            return &ST.cache[i];
        }
    return NULL;
}

static void cache_drop(const char *path) {
    for (int i = 0; i < CACHE_MAX; i++)
        if (ST.cache[i].path && !strcmp(ST.cache[i].path, path)) {
            osal_free(ST.cache[i].path);
            ST.cache[i] = (centry_t){0};
        }
}

/* Give memory back under pressure (resource manager): least recently used entries first. */
static size_t reclaim(void *ctx, size_t want) {
    (void)ctx;
    size_t got = 0;
    osal_mutex_lock(ST.lock, OSAL_WAIT_FOREVER);
    while (got < want) {
        centry_t *e = NULL;
        for (int i = 0; i < CACHE_MAX; i++)
            if (ST.cache[i].path && (!e || ST.cache[i].used_at < e->used_at)) e = &ST.cache[i];
        if (!e) break;
        got += strlen(e->path) + 1 + (e->len > 0 ? (size_t)e->len : 0) + 1;
        osal_free(e->path);
        *e = (centry_t){0};
    }
    osal_mutex_unlock(ST.lock);
    return got;
}

/* Depth first, small steps: a folder at a time. */
static void wipe_dir(const char *dir, int depth) {
    if (depth > 6) return;
    char *path = osal_malloc(160, OSAL_MEM_EXTERNAL);
    hal_dirent_t *e = osal_malloc(sizeof *e, OSAL_MEM_EXTERNAL);
    hal_dir_t *d = path && e ? hal_fs_opendir(dir) : NULL;
    while (d && hal_fs_readdir(d, e) == 1) {
        if (!strcmp(e->name, ".") || !strcmp(e->name, "..")) continue;
        if (snprintf(path, 160, "%s/%s", dir, e->name) >= 160) continue;
        if (e->is_dir) wipe_dir(path, depth + 1);
        hal_fs_remove(path);
    }
    if (d) hal_fs_closedir(d);
    osal_free(path);
    osal_free(e);
}

void ink_storage_wipe_user(void) { wipe_dir("/user", 0); }

int ink_storage_reclaimable(void) { return ink_mem_reclaimer("storage-cache", 10, reclaim, NULL); }

/* The content of path is now `data` (len -1: no file): cache it, or forget what was cached. */
static void cache_put(const char *path, const void *data, int len) {
    if (len > CACHE_FILE_MAX) {
        cache_drop(path);
        return;
    }
    size_t plen = strlen(path) + 1, dlen = len > 0 ? (size_t)len : 0;
    const char *prev = osal_mem_tag("storage-cache");
    char *block = osal_malloc(plen + dlen + 1, OSAL_MEM_EXTERNAL);
    osal_mem_tag(prev);
    if (!block) {
        cache_drop(path);
        return;
    }
    memcpy(block, path, plen);
    if (dlen) memcpy(block + plen, data, dlen);
    centry_t *e = NULL;
    for (int i = 0; i < CACHE_MAX && !e; i++)
        if (ST.cache[i].path && !strcmp(ST.cache[i].path, path)) e = &ST.cache[i];
    for (int i = 0; i < CACHE_MAX && !e; i++)
        if (!ST.cache[i].path) e = &ST.cache[i];
    if (!e) { /* evict the least recently used */
        e = &ST.cache[0];
        for (int i = 1; i < CACHE_MAX; i++)
            if (ST.cache[i].used_at < e->used_at) e = &ST.cache[i];
    }
    osal_free(e->path);
    *e = (centry_t){.path = block, .data = (uint8_t *)block + plen, .len = len, .used_at = ++ST.clock};
}

/* Flash read into the cache (storage task, guard taken inside). Returns bytes, -1 = no file, -2 = too large. */
static int read_guarded(const char *path, void *buf, size_t cap);
static bool queued_for(const char *path) { /* under lock: a write of path is waiting or being written */
    for (int i = 0; i < MAX_JOBS; i++)
        if (ST.jobs[i].used && !strcmp(ST.jobs[i].path, path)) return true;
    return false;
}

static int load_into_cache(const char *path) {
    int n = read_guarded(path, ST.scratch, CACHE_FILE_MAX + 1);
    if (n > CACHE_FILE_MAX) return -2;
    osal_mutex_lock(ST.lock, OSAL_WAIT_FOREVER);
    if (!queued_for(path)) cache_put(path, ST.scratch, n < 0 ? -1 : n); /* else the flash is already old */
    osal_mutex_unlock(ST.lock);
    return n;
}

#define NO_PARENT (-2) /* the file could not be opened: its directory may not exist yet */
#define PATH_BUF 200

static int write_now(const job_t *j) {
    char *tmp = ST.path_buf;
    size_t plen = strlen(j->path);
    if (plen + 5 > PATH_BUF) return -1;
    memcpy(tmp, j->path, plen);
    memcpy(tmp + plen, ".tmp", 5);
    hal_file_t *f = hal_fs_open(tmp, HAL_O_WRITE | HAL_O_CREATE | HAL_O_TRUNC);
    if (!f) return NO_PARENT;
    ssize_t n = hal_fs_write(f, j->data, j->len);
    int closed = hal_fs_close(f); /* on every path: an open /user file holds internal RAM */
    if (n != (ssize_t)j->len || closed != 0 || hal_fs_rename(tmp, j->path) != 0) {
        hal_fs_remove(tmp); /* the old content stays in place; a partial temp file would only take space */
        return -1;
    }
    return 0;
}

static int append_now(const job_t *j) {
    hal_file_t *f = hal_fs_open(j->path, HAL_O_WRITE | HAL_O_CREATE | HAL_O_APPEND);
    if (!f) return NO_PARENT;
    ssize_t n = hal_fs_write(f, j->data, j->len);
    int closed = hal_fs_close(f); /* LittleFS commits on close, all of it or nothing */
    return n == (ssize_t)j->len && closed == 0 ? 0 : -1;
}

/* The directories of a path under /user, top first. */
static void make_parents(const char *path) {
    char *dir = ST.path_buf;
    size_t n = strlen(path);
    if (n >= PATH_BUF) return;
    memcpy(dir, path, n + 1);
    for (char *p = dir + 6; *p; p++) /* past "/user/" */
        if (*p == '/') {
            *p = 0;
            hal_fs_mkdir(dir);
            *p = '/';
        }
}

static int run_job(const job_t *j) {
    if (j->kind == J_REMOVE) {
        hal_fs_remove(j->path); /* a file that is not there is removed too */
        return hal_fs_size(j->path) < 0 ? 0 : -1;
    }
    return j->kind == J_APPEND ? append_now(j) : write_now(j);
}

static int read_now(const char *path, void *buf, size_t cap) { /* storage task, flash guard held */
    hal_file_t *f = hal_fs_open(path, HAL_O_READ);
    if (!f) return -1;
    ssize_t n = hal_fs_read(f, buf, cap);
    hal_fs_close(f);
    return (int)n;
}

static int read_guarded(const char *path, void *buf, size_t cap) {
    uint64_t t0 = osal_now_us();
    ink_display_flash_guard(true); /* waits for the waveform in flight */
    ink_probe_value("storage.read_wait_ms", (double)(osal_now_us() - t0) / 1000.0);
    int r = read_now(path, buf, cap);
    ink_display_flash_guard(false);
    return r;
}

/* Serve a waiting read or call (storage task only). Queued replaces and removes win over flash content. */
static void serve_read(void) {
    osal_mutex_lock(ST.lock, OSAL_WAIT_FOREVER);
    read_req_t *r = ST.rreq;
    ST.rreq = NULL;
    if (r && r->fn) {
        osal_mutex_unlock(ST.lock);
        if (r->guarded) ink_display_flash_guard(true);
        r->fn(r->arg);
        if (r->guarded) ink_display_flash_guard(false);
        osal_sem_give(ST.read_done);
        return;
    }
    int from_queue = -1, newest = -1;
    if (r)
        for (int i = 0; i < MAX_JOBS; i++)
            if (ST.jobs[i].used && ST.jobs[i].kind != J_APPEND && !strcmp(ST.jobs[i].path, r->path) &&
                (newest < 0 || ST.jobs[i].seq > ST.jobs[newest].seq))
                newest = i;
    if (newest >= 0 && ST.jobs[newest].kind == J_REMOVE) {
        newest = -2; /* known not to exist */
    } else if (newest >= 0) {
        size_t n = ST.jobs[newest].len < r->cap ? ST.jobs[newest].len : r->cap;
        memcpy(r->buf, ST.jobs[newest].data, n);
        from_queue = (int)n;
    } else if (r) {
        centry_t *e = cache_find(r->path);
        if (e) {
            size_t n = e->len > 0 ? ((size_t)e->len < r->cap ? (size_t)e->len : r->cap) : 0;
            if (n) memcpy(r->buf, e->data, n);
            from_queue = e->len < 0 ? -1 : (int)n;
            if (e->len < 0) newest = -2; /* known not to exist */
        }
    }
    osal_mutex_unlock(ST.lock);
    if (!r) return;
    uint64_t t0 = osal_now_us();
    if (from_queue >= 0 || newest == -2) {
        r->result = from_queue;
        ink_probe_count("storage.reads_from_ram");
    } else {
        int n = load_into_cache(r->path);
        if (n == -2) n = read_guarded(r->path, r->buf, r->cap); /* too large for the cache */
        else if (n > 0) memcpy(r->buf, ST.scratch, (size_t)n < r->cap ? (size_t)n : r->cap);
        r->result = n > 0 && (size_t)n > r->cap ? (int)r->cap : n;
        ink_probe_count("storage.reads_from_flash");
    }
    ink_probe_value("storage.read_ms", (double)(osal_now_us() - t0) / 1000.0);
    osal_sem_give(ST.read_done);
}

/* Oldest queued job; it stays in its slot (visible to reads) until written. Returns the slot or -1. */
static int take_job(job_t *out) {
    osal_mutex_lock(ST.lock, OSAL_WAIT_FOREVER);
    int best = -1;
    for (int i = 0; i < MAX_JOBS; i++)
        if (ST.jobs[i].used && !ST.jobs[i].inflight && (best < 0 || ST.jobs[i].order < ST.jobs[best].order))
            best = i;
    if (best >= 0) {
        ST.jobs[best].inflight = true;
        *out = ST.jobs[best];
    }
    osal_mutex_unlock(ST.lock);
    return best;
}

static void finish_job(int slot) {
    osal_mutex_lock(ST.lock, OSAL_WAIT_FOREVER);
    char *block = ST.jobs[slot].path;
    ST.jobs[slot] = (job_t){0};
    ST.pending--;
    osal_mutex_unlock(ST.lock);
    osal_free(block);
}

/* One background prefetch, only while the panel is quiet. */
static void prefetch_one(void) {
    if (ink_display_busy() > 0) return;
    osal_mutex_lock(ST.lock, OSAL_WAIT_FOREVER);
    char *path = NULL;
    for (int i = 0; i < PREFETCH_MAX && !path; i++)
        if (ST.prefetch[i]) {
            path = ST.prefetch[i];
            ST.prefetch[i] = NULL;
        }
    bool known = path && cache_find(path);
    osal_mutex_unlock(ST.lock);
    if (!path) return;
    if (!known) {
        load_into_cache(path);
        ink_probe_count("storage.prefetches");
    }
    osal_free(path);
    osal_sem_give(ST.kick); /* more may be waiting */
}

static bool writes_held(void) {
    osal_mutex_lock(ST.lock, OSAL_WAIT_FOREVER);
    bool h = ST.holds > 0;
    osal_mutex_unlock(ST.lock);
    return h;
}

static void storage_task(void *arg) {
    (void)arg;
    for (;;) {
        /* sleep until there is work; only waiting prefetches (they need a quiet panel) look again now and then */
        osal_mutex_lock(ST.lock, OSAL_WAIT_FOREVER);
        bool waiting = false;
        for (int i = 0; i < PREFETCH_MAX && !waiting; i++) waiting = ST.prefetch[i] != NULL;
        osal_mutex_unlock(ST.lock);
        osal_sem_take(ST.kick, waiting ? IDLE_POLL_US * 4 : OSAL_WAIT_FOREVER);
        for (;;) {
            serve_read();
            prefetch_one();
            job_t job;
            int slot = take_job(&job);
            if (slot < 0) break;
            /* wait for a quiet panel and no write hold (reads are still served meanwhile) */
            uint64_t t_wait = osal_now_us();
            bool forced = false;
            for (;;) {
                serve_read();
                bool held = writes_held();
                if (!held && ink_display_busy() <= 0) break;
                if (!held && ST.panel_async && osal_now_us() - t_wait >= ASYNC_PANEL_WAIT_US) {
                    ink_probe_count("storage.writes_pausing_panel");
                    break;
                }
                if (held && osal_now_us() - job.queued_us > MAX_DEFER_US) {
                    forced = true;
                    break;
                }
                osal_sem_take(ST.kick, IDLE_POLL_US); /* a new read wakes us early */
            }
            if (forced) ink_probe_count("storage.forced_writes");
            ink_display_flash_guard(true);
            uint64_t t0 = osal_now_us();
            int r = run_job(&job);
            if (r == NO_PARENT) {
                make_parents(job.path);
                r = run_job(&job);
                if (r == NO_PARENT) r = -1;
            }
            ink_display_flash_guard(false);
            ink_probe_value("storage.write_ms", (double)(osal_now_us() - t0) / 1000.0);
            ink_probe_value("storage.wait_for_idle_ms", (double)(t0 - t_wait) / 1000.0);
            if (r != 0) {
                /* not stored: reads of this path must show what the flash holds, not the lost content */
                ink_probe_count("storage.write_errors");
                osal_mutex_lock(ST.lock, OSAL_WAIT_FOREVER);
                ST.failed++;
                cache_drop(job.path);
                osal_mutex_unlock(ST.lock);
            }
            finish_job(slot);
            if (job.done) job.done(job.ctx, r);
        }
    }
}

int ink_storage_init(void) {
    hal_panel_info_t pi;
    hal_display_get_info(&pi);
    ST.panel_async = pi.async_updates;
    const char *prev = osal_mem_tag("storage");
    ST.lock = osal_mutex_create();
    ST.read_lock = osal_mutex_create();
    ST.kick = osal_sem_create(0, 1);
    ST.read_done = osal_sem_create(0, 1);
    ST.jobs = osal_malloc(sizeof(job_t) * MAX_JOBS, OSAL_MEM_EXTERNAL);
    ST.cache = osal_calloc(CACHE_MAX, sizeof(centry_t), OSAL_MEM_EXTERNAL);
    ST.scratch = osal_malloc(CACHE_FILE_MAX + 1, OSAL_MEM_EXTERNAL);
    ST.path_buf = osal_malloc(PATH_BUF, OSAL_MEM_EXTERNAL);
    osal_mem_tag(prev);
    if (!ST.lock || !ST.read_lock || !ST.kick || !ST.read_done || !ST.jobs || !ST.cache || !ST.scratch || !ST.path_buf) return -1;
    memset(ST.jobs, 0, sizeof(job_t) * MAX_JOBS);
    osal_task_cfg_t cfg = {.name = "storage", .fn = storage_task, .stack_bytes = ST_STACK, .priority = ST_PRIO, .core = -1};
    return osal_task_create(&cfg, &ST.task) == OSAL_OK ? 0 : -1;
}

/* A slot for a new job, under the lock; -1 when the queue is full. */
static int free_slot(void) {
    for (int i = 0; i < MAX_JOBS; i++)
        if (!ST.jobs[i].used) return i;
    ink_probe_count("storage.queue_full");
    return -1;
}

int ink_storage_write_atomic(const char *path, const void *data, size_t len) {
    size_t plen = strlen(path) + 1;
    const char *prev = osal_mem_tag("storage");
    char *block = osal_malloc(plen + (len ? len : 1), OSAL_MEM_EXTERNAL);
    osal_mem_tag(prev);
    if (!block) return -1;
    memcpy(block, path, plen);
    memcpy(block + plen, data, len);
    osal_mutex_lock(ST.lock, OSAL_WAIT_FOREVER);
    int last = -1;
    for (int i = 0; i < MAX_JOBS; i++) /* the last job of this path */
        if (ST.jobs[i].used && !strcmp(ST.jobs[i].path, path) && (last < 0 || ST.jobs[i].order > ST.jobs[last].order)) last = i;
    /* coalesce into a queued (not in-flight) replace, when nothing follows it: newest content wins */
    int slot = last >= 0 && ST.jobs[last].kind == J_WRITE && !ST.jobs[last].inflight && !ST.jobs[last].done ? last : -1;
    char *old = NULL;
    uint64_t queued = osal_now_us();
    uint32_t order = 0;
    if (slot >= 0) {
        old = ST.jobs[slot].path;
        queued = ST.jobs[slot].queued_us; /* keeps its place in line */
        order = ST.jobs[slot].order;
    } else {
        slot = free_slot();
        if (slot >= 0) {
            ST.pending++;
            order = ++ST.order;
        }
    }
    if (slot < 0) {
        osal_mutex_unlock(ST.lock);
        osal_free(block);
        return -1;
    }
    ST.jobs[slot] = (job_t){.path = block, .data = (uint8_t *)block + plen, .len = len, .queued_us = queued,
                            .seq = ++ST.seq, .order = order, .kind = J_WRITE, .used = true};
    cache_put(path, data, (int)len); /* reads after this write never need the flash */
    osal_mutex_unlock(ST.lock);
    osal_free(old);
    osal_sem_give(ST.kick);
    return 0;
}

static int submit(job_kind_t kind, const char *path, const void *data, size_t len, ink_storage_done_fn done, void *ctx) {
    size_t plen = strlen(path) + 1;
    const char *prev = osal_mem_tag("storage");
    char *block = osal_malloc(plen, OSAL_MEM_EXTERNAL);
    osal_mem_tag(prev);
    if (!block) return -1;
    memcpy(block, path, plen);
    osal_mutex_lock(ST.lock, OSAL_WAIT_FOREVER);
    int slot = free_slot();
    if (slot < 0) {
        osal_mutex_unlock(ST.lock);
        osal_free(block);
        return -1;
    }
    ST.pending++;
    ST.jobs[slot] = (job_t){.path = block, .data = data, .len = len, .queued_us = osal_now_us(), .seq = ++ST.seq, .order = ++ST.order,
                            .kind = (uint8_t)kind, .used = true, .done = done, .ctx = ctx};
    if (kind == J_WRITE) cache_put(path, data, (int)len);
    else if (kind == J_REMOVE) cache_put(path, NULL, -1);
    else cache_drop(path);
    osal_mutex_unlock(ST.lock);
    osal_sem_give(ST.kick);
    return 0;
}

int ink_storage_write_borrow(const char *path, const void *data, size_t len, ink_storage_done_fn done, void *ctx) {
    return submit(J_WRITE, path, data, len, done, ctx);
}

int ink_storage_append(const char *path, const void *data, size_t len, ink_storage_done_fn done, void *ctx) {
    return submit(J_APPEND, path, data, len, done, ctx);
}

int ink_storage_remove(const char *path, ink_storage_done_fn done, void *ctx) { return submit(J_REMOVE, path, NULL, 0, done, ctx); }

/* Hand a request to the storage task and wait for it. */
static void request(read_req_t *req) {
    osal_mutex_lock(ST.read_lock, OSAL_WAIT_FOREVER);
    osal_mutex_lock(ST.lock, OSAL_WAIT_FOREVER);
    ST.rreq = req;
    osal_mutex_unlock(ST.lock);
    osal_sem_give(ST.kick);
    osal_sem_take(ST.read_done, OSAL_WAIT_FOREVER);
    osal_mutex_unlock(ST.read_lock);
}

void ink_storage_run(void (*fn)(void *arg), void *arg, bool guarded) {
    if (osal_task_self() == ST.task) {
        if (guarded) ink_display_flash_guard(true);
        fn(arg);
        if (guarded) ink_display_flash_guard(false);
        return;
    }
    read_req_t req = {.fn = fn, .arg = arg, .guarded = guarded};
    request(&req);
}

typedef struct {
    const char *path;
    int64_t size;
} size_req_t;

static void size_fn(void *arg) {
    size_req_t *q = arg;
    q->size = hal_fs_size(q->path);
}

int64_t ink_storage_size(const char *path) {
    size_req_t q = {.path = path};
    ink_storage_run(size_fn, &q, true);
    return q.size;
}

#define LOAD_CHUNK 2048 /* the flash guard is let go between pieces: the panel may scan */

typedef struct {
    const char *path;
    uint8_t *buf;
    size_t cap;
    int64_t got;
} load_req_t;

static void load_fn(void *arg) { /* no guard from the caller: taken per piece */
    load_req_t *q = arg;
    q->got = -1;
    ink_display_flash_guard(true);
    hal_file_t *f = hal_fs_open(q->path, HAL_O_READ);
    ink_display_flash_guard(false);
    if (!f) return;
    size_t done = 0;
    bool ok = true;
    while (done < q->cap) {
        size_t n = q->cap - done < LOAD_CHUNK ? q->cap - done : LOAD_CHUNK;
        ink_display_flash_guard(true);
        ssize_t r = hal_fs_read(f, q->buf + done, n);
        ink_display_flash_guard(false);
        if (r <= 0) {
            ok = r == 0;
            break;
        }
        done += (size_t)r;
    }
    ink_display_flash_guard(true);
    hal_fs_close(f);
    ink_display_flash_guard(false);
    if (ok) q->got = (int64_t)done;
}

int ink_storage_load(const char *path, void **out, size_t *len) {
    *out = NULL;
    *len = 0;
    int64_t size = ink_storage_size(path);
    if (size < 0) return INK_STORAGE_NO_FILE;
    if (size == 0) return 0;
    const char *prev = osal_mem_tag("storage");
    uint8_t *buf = osal_malloc((size_t)size, OSAL_MEM_EXTERNAL);
    osal_mem_tag(prev);
    if (!buf) return INK_STORAGE_LOAD_FAILED;
    load_req_t q = {.path = path, .buf = buf, .cap = (size_t)size};
    ink_storage_run(load_fn, &q, false);
    if (q.got != size) {
        osal_free(buf);
        return INK_STORAGE_LOAD_FAILED;
    }
    *out = buf;
    *len = (size_t)size;
    return 0;
}

void ink_storage_free(void *block) { osal_free(block); }

typedef struct {
    uint64_t total, free;
    int r;
} volume_req_t;

static void volume_fn(void *arg) {
    volume_req_t *q = arg;
    hal_fs_volume_info_t vi;
    q->r = hal_fs_volume_info("/user", &vi);
    if (q->r == 0) {
        q->total = vi.total_bytes;
        q->free = vi.free_bytes;
    }
}

int ink_storage_volume(uint64_t *total_bytes, uint64_t *free_bytes) {
    volume_req_t q = {0};
    ink_storage_run(volume_fn, &q, true);
    if (q.r != 0) return -1;
    if (total_bytes) *total_bytes = q.total;
    if (free_bytes) *free_bytes = q.free;
    return 0;
}

int ink_storage_read(const char *path, void *buf, size_t cap) {
    if (osal_task_self() == ST.task) return read_guarded(path, buf, cap);
    read_req_t req = {.path = path, .buf = buf, .cap = cap, .result = -1};
    request(&req);
    return req.result;
}

void ink_storage_prefetch(const char *path) {
    size_t n = strlen(path) + 1;
    const char *prev = osal_mem_tag("storage");
    char *copy = osal_malloc(n, OSAL_MEM_EXTERNAL);
    osal_mem_tag(prev);
    if (!copy) return;
    memcpy(copy, path, n);
    osal_mutex_lock(ST.lock, OSAL_WAIT_FOREVER);
    int slot = -1;
    for (int i = 0; i < PREFETCH_MAX; i++) {
        if (ST.prefetch[i] && !strcmp(ST.prefetch[i], path)) slot = -2;
        if (slot == -1 && !ST.prefetch[i]) slot = i;
    }
    if (slot >= 0) ST.prefetch[slot] = copy;
    osal_mutex_unlock(ST.lock);
    if (slot < 0) osal_free(copy);
    else osal_sem_give(ST.kick);
}

void ink_storage_hold_writes(bool hold) {
    osal_mutex_lock(ST.lock, OSAL_WAIT_FOREVER);
    ST.holds += hold ? 1 : -1;
    if (ST.holds < 0) ST.holds = 0;
    osal_mutex_unlock(ST.lock);
    if (!hold) osal_sem_give(ST.kick);
}

int ink_storage_pending(void) {
    osal_mutex_lock(ST.lock, OSAL_WAIT_FOREVER);
    int n = ST.pending;
    osal_mutex_unlock(ST.lock);
    return n;
}

int ink_storage_failed(void) {
    osal_mutex_lock(ST.lock, OSAL_WAIT_FOREVER);
    int n = ST.failed;
    osal_mutex_unlock(ST.lock);
    return n;
}
