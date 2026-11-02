/*
 * Storage service task: the ONLY code that touches /user (internal flash).
 *
 * Its stack is in internal RAM. On the MCU every flash API call — reads too —
 * disables the caches; a task whose stack is in PSRAM would lose its stack
 * (ESP-IDF asserts), so other tasks never call the flash API themselves:
 * reads are executed here on their behalf (the caller blocks), writes are
 * queued.
 *
 * Writes additionally wait until the panel is idle (a long erase during a
 * waveform underruns the panel) and until no one holds writes off (audio
 * streaming: its DMA ring covers only 60 ms), then hold the display's flash
 * guard while the flash is busy. Writes are atomic (temp file + rename) and
 * repeated writes of one path coalesce. Reads see queued writes.
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

typedef struct {
    char *path; /* path and data share one external-RAM block */
    uint8_t *data;
    size_t len;
    uint64_t queued_us;
    bool used;
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
} ST;

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

static int read_now(const char *path, void *buf, size_t cap) {
    hal_file_t *f = hal_fs_open(path, HAL_O_READ);
    if (!f) return -1;
    ssize_t n = hal_fs_read(f, buf, cap);
    hal_fs_close(f);
    return (int)n;
}

/* Serve a waiting read (storage task only). Queued writes win over flash content. */
static void serve_read(void) {
    osal_mutex_lock(ST.lock, OSAL_WAIT_FOREVER);
    read_req_t *r = ST.rreq;
    ST.rreq = NULL;
    int from_queue = -1;
    if (r)
        for (int i = 0; i < MAX_JOBS; i++)
            if (ST.jobs[i].used && !strcmp(ST.jobs[i].path, r->path)) {
                size_t n = ST.jobs[i].len < r->cap ? ST.jobs[i].len : r->cap;
                memcpy(r->buf, ST.jobs[i].data, n);
                from_queue = (int)n;
            }
    osal_mutex_unlock(ST.lock);
    if (!r) return;
    uint64_t t0 = osal_now_us();
    r->result = from_queue >= 0 ? from_queue : read_now(r->path, r->buf, r->cap);
    ink_probe_value("storage.read_ms", (double)(osal_now_us() - t0) / 1000.0);
    osal_sem_give(ST.read_done);
}

static bool take_job(job_t *out) {
    osal_mutex_lock(ST.lock, OSAL_WAIT_FOREVER);
    int best = -1;
    for (int i = 0; i < MAX_JOBS; i++)
        if (ST.jobs[i].used && (best < 0 || ST.jobs[i].queued_us < ST.jobs[best].queued_us)) best = i;
    if (best >= 0) {
        *out = ST.jobs[best];
        ST.jobs[best].used = false;
    }
    osal_mutex_unlock(ST.lock);
    return best >= 0;
}

static bool writes_held(void) {
    osal_mutex_lock(ST.lock, OSAL_WAIT_FOREVER);
    bool h = ST.holds > 0;
    osal_mutex_unlock(ST.lock);
    return h;
}

static void storage_task(void *arg) {
    (void)arg;
    hal_fs_mkdir("/user/apps");
    for (;;) {
        osal_sem_take(ST.kick, OSAL_WAIT_FOREVER);
        for (;;) {
            serve_read();
            job_t job;
            if (!take_job(&job)) break;
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
            osal_free(job.path);
            osal_mutex_lock(ST.lock, OSAL_WAIT_FOREVER);
            ST.pending--;
            osal_mutex_unlock(ST.lock);
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
    osal_mem_tag(prev);
    if (!ST.lock || !ST.read_lock || !ST.kick || !ST.read_done || !ST.jobs) return -1;
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
    for (int i = 0; i < MAX_JOBS; i++)
        if (ST.jobs[i].used && !strcmp(ST.jobs[i].path, path)) slot = i; /* coalesce: newest content wins */
    if (slot >= 0) {
        osal_free(ST.jobs[slot].path);
        ST.pending--;
    } else {
        for (int i = 0; i < MAX_JOBS && slot < 0; i++)
            if (!ST.jobs[i].used) slot = i;
    }
    if (slot < 0) {
        osal_mutex_unlock(ST.lock);
        osal_free(block);
        ink_probe_count("storage.queue_full");
        return -1;
    }
    uint64_t queued = slot >= 0 && ST.jobs[slot].used ? ST.jobs[slot].queued_us : osal_now_us();
    ST.jobs[slot] = (job_t){.path = block, .data = (uint8_t *)block + plen, .len = len, .queued_us = queued, .used = true};
    ST.pending++;
    osal_mutex_unlock(ST.lock);
    osal_sem_give(ST.kick);
    return 0;
}

int ink_storage_read(const char *path, void *buf, size_t cap) {
    if (osal_task_self() == ST.task) return read_now(path, buf, cap);
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
