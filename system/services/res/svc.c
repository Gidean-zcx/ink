/* The small services' shared lock and timer (see ink/sys/svc.h). */
#include <stddef.h>

#include "ink/osal.h"
#include "ink/sys/svc.h"

static struct {
    osal_mutex_t *lock;
    osal_timer_t *timer;
    ink_svc_deadline_t *list; /* unordered: a handful of nodes */
} SV;

void ink_svc_lock(void) { osal_mutex_lock(SV.lock, OSAL_WAIT_FOREVER); }
void ink_svc_unlock(void) { osal_mutex_unlock(SV.lock); }

static void unlink_node(ink_svc_deadline_t *d) { /* under the lock */
    for (ink_svc_deadline_t **p = &SV.list; *p; p = &(*p)->next)
        if (*p == d) {
            *p = d->next;
            break;
        }
    d->next = NULL;
    d->armed = false;
}

/* Under the lock: the timer for the earliest node. */
static void rearm(void) {
    uint64_t next = 0;
    for (ink_svc_deadline_t *d = SV.list; d; d = d->next)
        if (!next || d->at_us < next) next = d->at_us;
    if (!next) {
        osal_timer_stop(SV.timer);
        return;
    }
    uint64_t now = osal_now_us();
    osal_timer_start(SV.timer, next > now ? next - now : 1, 0);
}

static void timer_cb(void *arg) {
    (void)arg;
    for (;;) {
        uint64_t now = osal_now_us();
        ink_svc_lock();
        ink_svc_deadline_t *due = NULL;
        for (ink_svc_deadline_t *d = SV.list; d; d = d->next)
            if (d->at_us <= now && (!due || d->at_us < due->at_us)) due = d;
        void (*fn)(void *) = NULL;
        void *ctx = NULL;
        if (due) {
            fn = due->fn;
            ctx = due->ctx;
            unlink_node(due);
        } else rearm();
        ink_svc_unlock();
        if (!fn) return;
        fn(ctx);
    }
}

int ink_svc_init(void) {
    if (SV.lock) return 0;
    const char *prev = osal_mem_tag("svc");
    SV.lock = osal_mutex_create();
    SV.timer = osal_timer_create("svc", timer_cb, NULL);
    osal_mem_tag(prev);
    return SV.lock && SV.timer ? 0 : -1;
}

void ink_svc_at(ink_svc_deadline_t *d, uint64_t t_us, void (*fn)(void *ctx), void *ctx) {
    ink_svc_lock();
    if (d->armed) unlink_node(d);
    d->at_us = t_us ? t_us : 1;
    d->fn = fn;
    d->ctx = ctx;
    d->armed = true;
    d->next = SV.list;
    SV.list = d;
    rearm();
    ink_svc_unlock();
}

void ink_svc_cancel(ink_svc_deadline_t *d) {
    ink_svc_lock();
    if (d->armed) {
        unlink_node(d);
        rearm();
    }
    ink_svc_unlock();
}
