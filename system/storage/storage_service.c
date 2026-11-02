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
 * coalesce. Reads see queued writes, including the one being written; then
 * the read cache (small files and "does not exist", external RAM, LRU),
 * filled by reads, writes and background prefetches; only then the flash.
 */
#include <stdio.h>
#include <string.h>

#include "ink/hal_storage.h"
#include "ink/osal.h"
#include "ink/probe.h"
#include "ink/sys/display.h"
#include "ink/sys/storage.h"

#define MAX_JOBS 8
#define ST_STACK 3584
#define ST_PRIO 3
#define IDLE_POLL_US 50000
#define MAX_DEFER_US 30000000ull /* writes held off longer than this are forced (counted) */
#define CACHE_MAX 32
#define CACHE_FILE_MAX 1024 /* larger files are not cached */
#define PREFETCH_MAX 16

typedef struct {
    char *path; /* path and data share one external-RAM block */
    uint8_t *data;
    int len; /* -1: the file does not exist */
    uint32_t used_at;
} centry_t;

typedef struct {
    char *path; /* path and data share one external-RAM block */
    uint8_t *data;
    size_t len;
    uint64_t queued_us;
    uint32_t seq;  /* content version: the newest queued content of a path wins */
    bool used;
    bool inflight; /* taken by the storage task: stays visible to reads, never coalesced into */
} job_t;

typedef struct {
    const char *path;
    void *buf;
    size_t cap;
    int result;
} read_req_t;

static struct {
    osal_mutex_t *lock;      /* jobs, pending, holds */
    osal_mutex_t *read_lock; /* one read request at a time */
    osal_sem_t *kick;        /* work for the storage task */
    osal_sem_t *read_done;
    osal_task_t *task;
    job_t *jobs;             /* MAX_JOBS, external RAM */
    read_req_t *rreq;        /* pending read (under lock) */
    int pending;
    int holds;
    uint32_t seq;
    centry_t *cache; /* CACHE_MAX, external RAM */
    uint32_t clock;
    char *prefetch[PREFETCH_MAX]; /* paths waiting to be cached */
    uint8_t *scratch;             /* CACHE_FILE_MAX + 1, external RAM (storage task) */
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

static void cache_put(const char *path, const void *data, int len) {
    if (len > CACHE_FILE_MAX) return;
    size_t plen = strlen(path) + 1, dlen = len > 0 ? (size_t)len : 0;
    const char *prev = osal_mem_tag("storage-cache");
    char *block = osal_malloc(plen + dlen + 1, OSAL_MEM_EXTERNAL);
    osal_mem_tag(prev);
    if (!block) return;
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
static int load_into_cache(const char *path) {
    int n = read_guarded(path, ST.scratch, CACHE_FILE_MAX + 1);
    if (n > CACHE_FILE_MAX) return -2;
    osal_mutex_lock(ST.lock, OSAL_WAIT_FOREVER);
    cache_put(path, ST.scratch, n < 0 ? -1 : n);
    osal_mutex_unlock(ST.lock);
    return n;
}

static int write_now(const job_t *j) {
    char tmp[200];
    if (snprintf(tmp, sizeof tmp, "%s.tmp", j->path) >= (int)sizeof tmp) return -1;
    hal_file_t *f = hal_fs_open(tmp, HAL_O_WRITE | HAL_O_CREATE | HAL_O_TRUNC);
    if (!f) return -1;
    ssize_t n = hal_fs_write(f, j->data, j->len);
    int ok = n == (ssize_t)j->len && hal_fs_close(f) == 0;
    if (!ok) return -1;
    return hal_fs_rename(tmp, j->path);
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

/* Serve a waiting read (storage task only). Queued writes win over flash content. */
static void serve_read(void) {
    osal_mutex_lock(ST.lock, OSAL_WAIT_FOREVER);
    read_req_t *r = ST.rreq;
    ST.rreq = NULL;
    int from_queue = -1, newest = -1;
    if (r)
        for (int i = 0; i < MAX_JOBS; i++)
            if (ST.jobs[i].used && !strcmp(ST.jobs[i].path, r->path) && (newest < 0 || ST.jobs[i].seq > ST.jobs[newest].seq))
                newest = i;
    if (newest >= 0) {
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
        if (ST.jobs[i].used && !ST.jobs[i].inflight && (best < 0 || ST.jobs[i].queued_us < ST.jobs[best].queued_us))
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
    ink_display_flash_guard(true);
    hal_fs_mkdir("/user/apps");
    ink_display_flash_guard(false);
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
                if (held && osal_now_us() - job.queued_us > MAX_DEFER_US) {
                    forced = true;
                    break;
                }
                osal_sem_take(ST.kick, IDLE_POLL_US); /* a new read wakes us early */
            }
            if (forced) ink_probe_count("storage.forced_writes");
            ink_display_flash_guard(true);
            uint64_t t0 = osal_now_us();
            int r = write_now(&job);
            ink_display_flash_guard(false);
            ink_probe_value("storage.write_ms", (double)(osal_now_us() - t0) / 1000.0);
            ink_probe_value("storage.wait_for_idle_ms", (double)(t0 - t_wait) / 1000.0);
            if (r != 0) ink_probe_count("storage.write_errors");
            finish_job(slot);
        }
    }
}

int ink_storage_init(void) {
    const char *prev = osal_mem_tag("storage");
    ST.lock = osal_mutex_create();
    ST.read_lock = osal_mutex_create();
    ST.kick = osal_sem_create(0, 1);
    ST.read_done = osal_sem_create(0, 1);
    ST.jobs = osal_malloc(sizeof(job_t) * MAX_JOBS, OSAL_MEM_EXTERNAL);
    ST.cache = osal_calloc(CACHE_MAX, sizeof(centry_t), OSAL_MEM_EXTERNAL);
    ST.scratch = osal_malloc(CACHE_FILE_MAX + 1, OSAL_MEM_EXTERNAL);
    osal_mem_tag(prev);
    if (!ST.lock || !ST.read_lock || !ST.kick || !ST.read_done || !ST.jobs || !ST.cache || !ST.scratch) return -1;
    memset(ST.jobs, 0, sizeof(job_t) * MAX_JOBS);
    osal_task_cfg_t cfg = {.name = "storage", .fn = storage_task, .stack_bytes = ST_STACK, .priority = ST_PRIO, .core = -1};
    return osal_task_create(&cfg, &ST.task) == OSAL_OK ? 0 : -1;
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
    int slot = -1;
    for (int i = 0; i < MAX_JOBS; i++) /* coalesce into a queued (not in-flight) job: newest content wins */
        if (ST.jobs[i].used && !ST.jobs[i].inflight && !strcmp(ST.jobs[i].path, path)) slot = i;
    char *old = NULL;
    uint64_t queued = osal_now_us();
    if (slot >= 0) {
        old = ST.jobs[slot].path;
        queued = ST.jobs[slot].queued_us; /* keeps its place in line */
    } else {
        for (int i = 0; i < MAX_JOBS && slot < 0; i++)
            if (!ST.jobs[i].used) slot = i;
        if (slot >= 0) ST.pending++;
    }
    if (slot < 0) {
        osal_mutex_unlock(ST.lock);
        osal_free(block);
        ink_probe_count("storage.queue_full");
        return -1;
    }
    ST.jobs[slot] = (job_t){.path = block, .data = (uint8_t *)block + plen, .len = len, .queued_us = queued,
                            .seq = ++ST.seq, .used = true};
    cache_put(path, data, (int)len); /* reads after this write never need the flash */
    osal_mutex_unlock(ST.lock);
    osal_free(old);
    osal_sem_give(ST.kick);
    return 0;
}

int ink_storage_read(const char *path, void *buf, size_t cap) {
    if (osal_task_self() == ST.task) return read_guarded(path, buf, cap);
    read_req_t req = {.path = path, .buf = buf, .cap = cap, .result = -1};
    osal_mutex_lock(ST.read_lock, OSAL_WAIT_FOREVER);
    osal_mutex_lock(ST.lock, OSAL_WAIT_FOREVER);
    ST.rreq = &req;
    osal_mutex_unlock(ST.lock);
    osal_sem_give(ST.kick);
    osal_sem_take(ST.read_done, OSAL_WAIT_FOREVER);
    osal_mutex_unlock(ST.read_lock);
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
