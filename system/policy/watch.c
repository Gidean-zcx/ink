/* Watchers for the state and settings tables (see watch.h). */
#include <string.h>

#include "ink/probe.h"
#include "watch.h"

int watch_list_init(watch_list_t *l, osal_mutex_t *lock, const char *(*id_of)(int idx), const char *probe_dropped) {
    l->lock = lock;
    l->id_of = id_of;
    l->probe_dropped = probe_dropped;
    return lock ? 0 : -1;
}

int watch_add(watch_list_t *l, const char *prefix, watch_fn fn, void *ctx, ink_exec_t exec) {
    if (!l->lock || !fn) return -1;
    osal_mutex_lock(l->lock, OSAL_WAIT_FOREVER);
    int r = -1;
    if (l->n < WATCH_MAX) {
        watch_t *w = &l->w[l->n];
        *w = (watch_t){.list = l, .prefix = prefix ? prefix : "", .plen = prefix ? strlen(prefix) : 0, .fn = fn, .ctx = ctx, .exec = exec};
        r = l->n++;
    }
    osal_mutex_unlock(l->lock);
    return r;
}

static void deliver(void *arg) { /* on the watcher's executor */
    watch_t *w = arg;
    watch_list_t *l = w->list;
    osal_mutex_lock(l->lock, OSAL_WAIT_FOREVER);
    uint64_t bits = w->pending;
    w->pending = 0;
    w->scheduled = false;
    osal_mutex_unlock(l->lock);
    for (int i = 0; bits; i++, bits >>= 1)
        if (bits & 1) w->fn(w->ctx, i, l->id_of(i));
}

void watch_changed(watch_list_t *l, int idx, const char *id) {
    if (!l->lock || idx < 0 || idx >= 64) return;
    watch_t *post[WATCH_MAX];
    int np = 0;
    osal_mutex_lock(l->lock, OSAL_WAIT_FOREVER);
    for (int i = 0; i < l->n; i++) {
        watch_t *w = &l->w[i];
        if (strncmp(id, w->prefix, w->plen)) continue;
        w->pending |= 1ull << idx;
        if (!w->scheduled) {
            w->scheduled = true;
            post[np++] = w;
        }
    }
    osal_mutex_unlock(l->lock);
    for (int i = 0; i < np; i++)
        if (ink_exec_post(post[i]->exec, deliver, post[i]) != 0) {
            osal_mutex_lock(l->lock, OSAL_WAIT_FOREVER);
            post[i]->scheduled = false; /* the next change tries again; the bits stay pending */
            osal_mutex_unlock(l->lock);
            ink_probe_count(l->probe_dropped);
        }
}
