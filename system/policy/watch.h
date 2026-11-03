/*
 * Watchers for the state and settings tables (internal): a subscriber is told
 * which items changed, on its own executor; changes that pile up before it
 * runs are delivered once (it reads the latest value).
 */
#ifndef INK_POLICY_WATCH_H
#define INK_POLICY_WATCH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ink/osal.h"
#include "ink/sys/exec.h"

#define WATCH_MAX 16

typedef void (*watch_fn)(void *ctx, int idx, const char *id);

typedef struct watch_list watch_list_t;

typedef struct {
    watch_list_t *list;
    const char *prefix;
    size_t plen;
    watch_fn fn;
    void *ctx;
    ink_exec_t exec;
    uint64_t pending; /* items changed since the last delivery (bit = item index, < 64) */
    bool scheduled;
} watch_t;

struct watch_list {
    osal_mutex_t *lock; /* the table's own lock (shared: one mutex per table) */
    int n;
    watch_t w[WATCH_MAX];
    const char *(*id_of)(int idx);
    const char *probe_dropped; /* counted when a delivery cannot be posted */
};

/** `lock`: the table's mutex; watch_changed must be called without holding it. */
int watch_list_init(watch_list_t *l, osal_mutex_t *lock, const char *(*id_of)(int idx), const char *probe_dropped);
int watch_add(watch_list_t *l, const char *prefix, watch_fn fn, void *ctx, ink_exec_t exec);
/** Item `idx` (named `id`) changed: mark it for every matching watcher and schedule delivery. Any task. */
void watch_changed(watch_list_t *l, int idx, const char *id);

#endif
