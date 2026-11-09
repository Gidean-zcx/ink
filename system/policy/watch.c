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
    int i = 0;
    while (i < l->n && (l->w[i].fn || (l->w[i].exec != exec &&
           (l->w[i].posting || __atomic_load_n(&l->w[i].job.queued, __ATOMIC_ACQUIRE))))) i++;
    if (i < WATCH_MAX) {
        watch_t *w = &l->w[i];
        w->list = l; w->prefix = prefix ? prefix : ""; w->plen = strlen(w->prefix);
        w->fn = fn; w->ctx = ctx; w->exec = exec; w->pending = 0;
        r = w->handle = ++l->next_handle;
        if (i == l->n) l->n++;
    }
    osal_mutex_unlock(l->lock);
    return r;
}

int watch_remove(watch_list_t *l, int handle) {
    if (!l->lock || handle <= 0) return -1;
    osal_mutex_lock(l->lock, OSAL_WAIT_FOREVER);
    int r = -1;
    for (int i = 0; i < l->n; i++) {
        watch_t *w = &l->w[i];
        if (w->handle == handle && w->fn && w->exec != INK_EXEC_DIRECT && ink_exec_is_current(w->exec)) {
            w->fn = NULL; w->pending = 0; r = 0;
        }
    }
    osal_mutex_unlock(l->lock);
    return r;
}

static void deliver(void *arg) { /* on the watcher's executor */
    watch_t *w = arg;
    watch_list_t *l = w->list;
    osal_mutex_lock(l->lock, OSAL_WAIT_FOREVER);
    uint64_t bits = w->pending;
    int handle = w->handle;
    w->pending = 0;
    osal_mutex_unlock(l->lock);
    for (int i = 0; bits; i++, bits >>= 1) {
        osal_mutex_lock(l->lock, OSAL_WAIT_FOREVER);
        watch_fn fn = w->handle == handle ? w->fn : NULL;
        void *ctx = w->ctx;
        osal_mutex_unlock(l->lock);
        if ((bits & 1) && fn) fn(ctx, i, l->id_of(i));
    }
}

void watch_changed(watch_list_t *l, int idx, const char *id) {
    if (!l->lock || idx < 0 || idx >= 64) return;
    watch_t *post[WATCH_MAX];
    int np = 0;
    osal_mutex_lock(l->lock, OSAL_WAIT_FOREVER);
    for (int i = 0; i < l->n; i++) {
        watch_t *w = &l->w[i];
        if (!w->fn || strncmp(id, w->prefix, w->plen)) continue;
        w->pending |= 1ull << idx;
        w->posting++;
        post[np++] = w;
    }
    osal_mutex_unlock(l->lock);
    for (int i = 0; i < np; i++) { /* keep the executor fixed until this post finishes */
        if (ink_exec_post(post[i]->exec, &post[i]->job, deliver, post[i]) != 0) ink_probe_count(l->probe_dropped);
        osal_mutex_lock(l->lock, OSAL_WAIT_FOREVER);
        post[i]->posting--;
        osal_mutex_unlock(l->lock);
    }
}
