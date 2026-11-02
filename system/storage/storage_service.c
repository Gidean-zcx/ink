/*
 * Storage service task. Its stack is in internal RAM (it writes flash; a
 * PSRAM stack would be frozen by its own writes).
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

typedef struct {
    char path[64];
    uint8_t *data;
    size_t len;
    bool used;
} job_t;

static struct {
    osal_mutex_t *lock;
    osal_sem_t *kick;
    job_t jobs[MAX_JOBS];
    volatile int pending;
} ST;

static int write_now(const job_t *j) {
    char tmp[72];
    snprintf(tmp, sizeof tmp, "%s.tmp", j->path);
    hal_file_t *f = hal_fs_open(tmp, HAL_O_WRITE | HAL_O_CREATE | HAL_O_TRUNC);
    if (!f) return -1;
    ssize_t n = hal_fs_write(f, j->data, j->len);
    int ok = n == (ssize_t)j->len && hal_fs_close(f) == 0;
    if (!ok) return -1;
    return hal_fs_rename(tmp, j->path);
}

static void storage_task(void *arg) {
    (void)arg;
    for (;;) {
        osal_sem_take(ST.kick, OSAL_WAIT_FOREVER);
        for (;;) {
            job_t job = {0};
            osal_mutex_lock(ST.lock, OSAL_WAIT_FOREVER);
            for (int i = 0; i < MAX_JOBS; i++)
                if (ST.jobs[i].used) {
                    job = ST.jobs[i];
                    ST.jobs[i].used = false;
                    break;
                }
            osal_mutex_unlock(ST.lock);
            if (!job.used) break;
            /* wait for a quiet panel, then keep it quiet while the flash is busy */
            uint64_t t_wait = osal_now_us();
            while (ink_display_busy() > 0) osal_task_delay_us(IDLE_POLL_US);
            ink_display_flash_guard(true);
            uint64_t t0 = osal_now_us();
            int r = write_now(&job);
            ink_display_flash_guard(false);
            ink_probe_value("storage.write_ms", (double)(osal_now_us() - t0) / 1000.0);
            ink_probe_value("storage.wait_for_idle_ms", (double)(t0 - t_wait) / 1000.0);
            if (r != 0) ink_probe_count("storage.write_errors");
            osal_free(job.data);
            ST.pending--;
        }
    }
}

int ink_storage_init(void) {
    const char *prev = osal_mem_tag("storage");
    ST.lock = osal_mutex_create();
    ST.kick = osal_sem_create(0, 1);
    osal_mem_tag(prev);
    if (!ST.lock || !ST.kick) return -1;
    hal_fs_mkdir("/user/apps");
    osal_task_cfg_t cfg = {.name = "storage", .fn = storage_task, .stack_bytes = ST_STACK, .priority = ST_PRIO, .core = -1};
    return osal_task_create(&cfg, NULL) == OSAL_OK ? 0 : -1;
}

int ink_storage_write_atomic(const char *path, const void *data, size_t len) {
    const char *prev = osal_mem_tag("storage");
    uint8_t *copy = osal_malloc(len ? len : 1, OSAL_MEM_DEFAULT);
    osal_mem_tag(prev);
    if (!copy) return -1;
    memcpy(copy, data, len);
    osal_mutex_lock(ST.lock, OSAL_WAIT_FOREVER);
    int slot = -1;
    for (int i = 0; i < MAX_JOBS; i++)
        if (ST.jobs[i].used && !strcmp(ST.jobs[i].path, path)) slot = i; /* coalesce: newest content wins */
    if (slot >= 0) {
        osal_free(ST.jobs[slot].data);
        ST.pending--;
    } else {
        for (int i = 0; i < MAX_JOBS && slot < 0; i++)
            if (!ST.jobs[i].used) slot = i;
    }
    if (slot < 0) {
        osal_mutex_unlock(ST.lock);
        osal_free(copy);
        return -1;
    }
    snprintf(ST.jobs[slot].path, sizeof ST.jobs[slot].path, "%s", path);
    ST.jobs[slot].data = copy;
    ST.jobs[slot].len = len;
    ST.jobs[slot].used = true;
    ST.pending++;
    osal_mutex_unlock(ST.lock);
    osal_sem_give(ST.kick);
    return 0;
}

int ink_storage_read(const char *path, void *buf, size_t cap) {
    hal_file_t *f = hal_fs_open(path, HAL_O_READ);
    if (!f) return -1;
    ssize_t n = hal_fs_read(f, buf, cap);
    hal_fs_close(f);
    return (int)n;
}

int ink_storage_pending(void) { return ST.pending; }
