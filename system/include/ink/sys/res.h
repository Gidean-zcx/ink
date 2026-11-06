/*
 * Resource management (architecture v1.0 §8.2, §8.3; detailed design §5):
 * memory accounted per component in three domains with declared quotas and
 * admission for large blocks; pressure levels that shrink caches and pause
 * background work; jobs that run in small, cancellable steps on the
 * background executor.
 *
 * Pressure is an internal mechanism: it is not in the system state table.
 * Apps hear of it only as lifecycle callbacks (frozen, stopped).
 */
#ifndef INK_SYS_RES_H
#define INK_SYS_RES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { INK_MEM_INTERNAL = 0, INK_MEM_DMA, INK_MEM_PSRAM, INK_MEM_DOMAINS } ink_mem_domain_t;
typedef enum { INK_PRESSURE_NORMAL = 0, INK_PRESSURE_TIGHT, INK_PRESSURE_LOW, INK_PRESSURE_CRITICAL } ink_pressure_t;

int ink_res_init(void);

/** Declare a component's quota in a domain (once, at start-up). */
int ink_mem_budget(const char *component, ink_mem_domain_t d, size_t bytes);
/** Allocate for a component: NULL when it would exceed its quota, the block is large (>= 16 KiB)
 *  and not admitted, or the heap has no room. */
void *ink_mem_alloc(const char *component, ink_mem_domain_t d, size_t n);
void ink_mem_free(void *p);
/** Bytes a component holds in a domain. */
size_t ink_mem_used(const char *component, ink_mem_domain_t d);
/** May a large block of n bytes be taken now without pushing the domain past "tight"? */
bool ink_mem_admit(ink_mem_domain_t d, size_t n);

/** A cache that gives memory back: reclaim(ctx, want) frees up to `want` bytes and says how
 *  many it freed. Lower prio is asked first. Up to 8. */
int ink_mem_reclaimer(const char *name, int prio, size_t (*reclaim)(void *ctx, size_t want), void *ctx);
/** Re-evaluate the pressure level now (after big allocations, frees, or a test). */
ink_pressure_t ink_mem_check(void);
ink_pressure_t ink_mem_pressure(void);
const char *ink_pressure_name(ink_pressure_t p);
/** fn(ctx, from, to) on every level change, in the task that saw it. Up to 6. */
int ink_mem_on_pressure(void (*fn)(void *ctx, ink_pressure_t from, ink_pressure_t to), void *ctx);

/* ---- jobs ---- */

typedef enum { INK_JOB_MORE = 0, INK_JOB_DONE, INK_JOB_FAILED } ink_job_step_t;

typedef struct {
    const char *name;
    ink_job_step_t (*step)(void *ctx); /* one small step, within step_budget_ms */
    void (*finish)(void *ctx, ink_job_step_t how, bool cancelled); /* may be NULL */
    void *ctx;
    uint32_t step_budget_ms; /* 0 = 20 ms; a longer step is counted and logged, not killed */
    bool needs_net;          /* runs only while online */
    size_t psram_bytes;      /* admitted before it starts */
} ink_job_t;

/** Queue a job (copied). Its id (> 0), or -1: queue full (16) or not admitted. */
int ink_job_submit(const ink_job_t *job);
/** Cancel: no more steps; finish(…, cancelled = true). */
void ink_job_cancel(int id);
/** Hold all jobs (reference-counted), e.g. while a realtime session is open. */
void ink_jobs_hold(bool hold);
/** Background transfers share the realtime and memory-pressure hold. */
bool ink_jobs_held(void);
int ink_jobs_pending(void);

#ifdef __cplusplus
}
#endif
#endif
