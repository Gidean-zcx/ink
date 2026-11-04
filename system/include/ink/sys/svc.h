/*
 * What the small system services share (system services design §6.5): one
 * lock and one timer. Each costs internal RAM, the scarcest resource, and the
 * tightest scenario has only a few hundred bytes above its floor.
 *
 * The lock is a leaf: under it a service only touches its own tables. It never
 * calls another service, never waits and never allocates through
 * ink_mem_alloc (which may run reclaimers).
 *
 * A deadline is a node its owner keeps, as with executor jobs: arming cannot
 * run out of anything. The callback runs in the timer task, one at a time for
 * all deadlines: short, never blocks. A deadline cancelled at the very moment
 * it comes due may still run: the callback looks at its own state.
 */
#ifndef INK_SYS_SVC_H
#define INK_SYS_SVC_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ink_svc_deadline {
    struct ink_svc_deadline *next;
    uint64_t at_us;
    void (*fn)(void *ctx);
    void *ctx;
    bool armed;
} ink_svc_deadline_t;

int ink_svc_init(void);
void ink_svc_lock(void);
void ink_svc_unlock(void);
/** Run fn(ctx) at t_us on the monotonic clock (a time already past: as soon as possible). Armed
 *  again it moves. The node stays valid while armed. Any task; not under the lock. */
void ink_svc_at(ink_svc_deadline_t *d, uint64_t t_us, void (*fn)(void *ctx), void *ctx);
void ink_svc_cancel(ink_svc_deadline_t *d);

#ifdef __cplusplus
}
#endif
#endif
