/*
 * ink OSAL — operating-system abstraction layer.
 *
 * System services and apps use ONLY this header (plus ink/hal_*.h) for
 * tasks, synchronization, time and memory. Each platform provides one
 * implementation:
 *   ports/esp32s31  FreeRTOS (ESP-IDF)            — the real device
 *   ports/sim       virtual-time kernel            — the simulator (deterministic, resource-limited)
 *   ports/posix     pthreads + monotonic clock     — Linux SoC (e.g. RK3566) / desktop
 *
 * Semantics follow FreeRTOS (fixed priorities, higher number = more urgent,
 * preemptive, equal priorities time-sliced at each tick; mutexes with
 * priority inheritance but no hand-over), because that is the most
 * constrained target. Code that is correct under these rules is correct on
 * the richer platforms too.
 *
 * Delays and timeouts mean "at least N µs", ending on the first RTOS tick at
 * or after now + N (tick = 10 ms on the Plus). The FreeRTOS port computes
 * the target tick from the microsecond clock (target = ceil((now + N) / T),
 * wait = target - xTaskGetTickCount()); a naive pdMS_TO_TICKS(N) could wake
 * up to one tick early and is not allowed.
 */
#ifndef INK_OSAL_H
#define INK_OSAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- results ---------- */
typedef enum {
    OSAL_OK = 0,
    OSAL_TIMEOUT = -1,
    OSAL_NO_MEM = -2,
    OSAL_INVALID = -3,
    OSAL_FULL = -4,
    OSAL_EMPTY = -5,
} osal_err_t;

#define OSAL_WAIT_FOREVER (-1LL)
#define OSAL_NO_WAIT (0LL)

/* ---------- time ---------- */
/** Monotonic microseconds since boot (virtual time in the simulator). */
uint64_t osal_now_us(void);
/** Wall clock as Unix microseconds (UTC); 0 if unknown (RTC not set). */
int64_t osal_wall_us(void);
void osal_set_wall_us(int64_t unix_us);

/* ---------- memory ----------
 * Capabilities mirror ESP-IDF heap_caps. The default policy mirrors the
 * upstream firmware: small blocks (<= 16 B) internal, larger blocks external
 * (PSRAM) with internal fallback. Never call libc malloc() from system code.
 */
enum {
    OSAL_MEM_DEFAULT = 0,
    OSAL_MEM_INTERNAL = 1u << 0, /* on-chip SRAM (scarce: tens of KiB available) */
    OSAL_MEM_EXTERNAL = 1u << 1, /* PSRAM (MiBs; stalls while internal flash is written) */
    OSAL_MEM_DMA = 1u << 2,      /* DMA-capable (subset of internal) */
};

void *osal_malloc(size_t size, uint32_t caps);
void *osal_calloc(size_t n, size_t size, uint32_t caps);
void *osal_realloc(void *ptr, size_t size, uint32_t caps);
void osal_free(void *ptr);

typedef struct {
    size_t total;         /* bytes in this pool */
    size_t free;          /* bytes free now */
    size_t min_free;      /* low-water mark since boot */
    size_t largest_free;  /* largest allocatable block now */
    size_t alloc_count;   /* live allocations */
    size_t failed_allocs; /* allocations that failed */
} osal_mem_stats_t;

/** caps = exactly one of OSAL_MEM_INTERNAL / OSAL_MEM_EXTERNAL / OSAL_MEM_DMA. */
void osal_mem_stats(uint32_t caps, osal_mem_stats_t *out);

/** Attribute subsequent allocations of the calling task to `tag` (e.g. "fonts",
 *  "layout-cache") for memory reports; NULL = task name. Returns the previous
 *  tag. Free on the device (no-op); the simulator reports usage per tag. */
const char *osal_mem_tag(const char *tag);

/* ---------- tasks ---------- */
typedef struct osal_task osal_task_t;
typedef void (*osal_task_fn)(void *arg);

#define OSAL_PRIO_IDLE 0
#define OSAL_PRIO_LOW 2
#define OSAL_PRIO_NORMAL 5
#define OSAL_PRIO_HIGH 10
#define OSAL_PRIO_MAX 24

enum {
    /* Put the stack in external RAM (PSRAM) to save internal RAM. Default is
     * internal, as in ESP-IDF. Such a task must never call the flash API at
     * all — reading /user included: the flash driver disables the cache, the
     * stack becomes unreachable and ESP-IDF asserts. Use the storage service.
     * (No task of any kind runs while the flash API is busy: ESP-IDF
     * suspends the scheduler on both cores; only IRAM-safe ISRs run.) */
    OSAL_TASK_STACK_EXTERNAL = 1u << 1,
};

typedef struct {
    const char *name;
    osal_task_fn fn;
    void *arg;
    uint32_t stack_bytes; /* device stack size (the simulator scales it for the host) */
    int priority;         /* OSAL_PRIO_IDLE .. OSAL_PRIO_MAX */
    int core;             /* -1 = any core, otherwise 0 or 1 */
    uint32_t flags;       /* OSAL_TASK_* */
} osal_task_cfg_t;

osal_err_t osal_task_create(const osal_task_cfg_t *cfg, osal_task_t **out);
osal_task_t *osal_task_self(void);
const char *osal_task_name(const osal_task_t *t);
void osal_task_delay_us(uint64_t us);
void osal_task_yield(void);
/** Return from the task function or call this to end the task. */
void osal_task_exit(void);

/* ---------- CPU cost annotation ----------
 * On the device these compile to nothing. In the simulator they charge
 * modeled CPU time to the calling task (deterministic mode) so that long
 * computations delay other tasks exactly like on the real cores.
 * Use them in hot loops where the work is proportional to a known quantity
 * (pixels converted, glyphs rasterized, bytes inflated ...).
 */
void osal_cost_cycles(uint64_t cycles);
/** Named cost class from the device profile, e.g. "lvgl.px", "glyph.blit". */
void osal_cost_units(const char *cost_class, uint64_t units);
/** Scheduling point for long loops: lets higher-priority work run. */
void osal_preempt_point(void);

/* ---------- queues ---------- */
typedef struct osal_queue osal_queue_t;
osal_queue_t *osal_queue_create(size_t item_size, size_t length);
void osal_queue_delete(osal_queue_t *q);
osal_err_t osal_queue_send(osal_queue_t *q, const void *item, int64_t timeout_us);
osal_err_t osal_queue_send_front(osal_queue_t *q, const void *item, int64_t timeout_us);
osal_err_t osal_queue_recv(osal_queue_t *q, void *item, int64_t timeout_us);
size_t osal_queue_count(osal_queue_t *q);

/* ---------- mutex (priority inheritance) ---------- */
typedef struct osal_mutex osal_mutex_t;
osal_mutex_t *osal_mutex_create(void);
void osal_mutex_delete(osal_mutex_t *m);
osal_err_t osal_mutex_lock(osal_mutex_t *m, int64_t timeout_us);
void osal_mutex_unlock(osal_mutex_t *m);

/* ---------- counting semaphore ---------- */
typedef struct osal_sem osal_sem_t;
osal_sem_t *osal_sem_create(uint32_t initial, uint32_t max);
void osal_sem_delete(osal_sem_t *s);
osal_err_t osal_sem_take(osal_sem_t *s, int64_t timeout_us);
void osal_sem_give(osal_sem_t *s);

/* ---------- software timers (callbacks run in the timer service task) ---------- */
typedef struct osal_timer osal_timer_t;
typedef void (*osal_timer_fn)(void *arg);
osal_timer_t *osal_timer_create(const char *name, osal_timer_fn fn, void *arg);
void osal_timer_delete(osal_timer_t *t);
/** period_us = 0 for one-shot. Restarting an active timer re-arms it. */
osal_err_t osal_timer_start(osal_timer_t *t, uint64_t delay_us, uint64_t period_us);
osal_err_t osal_timer_stop(osal_timer_t *t);

/* ---------- misc ---------- */
/** Fatal error: logs, records a reset reason, and restarts (device) or aborts (sim). */
void osal_panic(const char *fmt, ...) __attribute__((noreturn, format(printf, 1, 2)));

#ifdef __cplusplus
}
#endif
#endif /* INK_OSAL_H */
