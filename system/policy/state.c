/*
 * System state table (see ink/sys/state.h). Values live in a static table
 * under one lock; watchers are told outside it, on their own executors. An
 * item with a minimum interval that changes again too soon is delivered
 * later by one timer, with whatever value it has by then.
 */
#include <string.h>

#include "ink/osal.h"
#include "ink/sys/json.h"
#include "ink/sys/state.h"
#include "watch.h"

typedef struct {
    const ink_state_def_t *def;
    int64_t i;
    char s[INK_STATE_STR];
    bool set;
    bool deferred;          /* changed within min_interval: the timer delivers it */
    uint64_t last_notify_us;
} item_t;

static struct {
    osal_mutex_t *lock;
    item_t it[INK_STATE_MAX];
    int n;
    watch_list_t watch;
    osal_timer_t *timer;
    uint64_t timer_due; /* when the timer fires (0 = not armed) */
} ST;

static const char *id_of(int h) { return h >= 0 && h < ST.n ? ST.it[h].def->id : ""; }

static void timer_cb(void *arg);

int ink_state_init(void) {
    if (ST.lock) return 0;
    ST.lock = osal_mutex_create();
    ST.timer = osal_timer_create("state", timer_cb, NULL);
    if (!ST.lock || !ST.timer || watch_list_init(&ST.watch, ST.lock, id_of, "state.watch_dropped")) return -1;
    return 0;
}

int ink_state_define(const ink_state_def_t *def) {
    if (!ST.lock || !def || !def->id) return -1;
    osal_mutex_lock(ST.lock, OSAL_WAIT_FOREVER);
    int h = -1;
    if (ST.n < INK_STATE_MAX) {
        h = ST.n++;
        memset(&ST.it[h], 0, sizeof ST.it[h]);
        ST.it[h].def = def;
    }
    osal_mutex_unlock(ST.lock);
    return h;
}

int ink_state_find(const char *id) {
    for (int h = 0; h < ST.n; h++)
        if (!strcmp(ST.it[h].def->id, id)) return h;
    return -1;
}

/* Under the lock, after a change: deliver now, or leave it to the timer. Returns "deliver now". */
static bool due_now(item_t *it, uint64_t now) {
    uint64_t gap = (uint64_t)it->def->min_interval_ms * 1000u;
    if (!gap || !it->last_notify_us || now - it->last_notify_us >= gap) {
        it->last_notify_us = now ? now : 1;
        it->deferred = false;
        return true;
    }
    if (!it->deferred) {
        it->deferred = true;
        uint64_t due = it->last_notify_us + gap;
        if (!ST.timer_due || due < ST.timer_due) { /* only ever brought forward */
            ST.timer_due = due;
            osal_timer_start(ST.timer, due - now, 0);
        }
    }
    return false;
}

static void changed(int h, bool now) {
    if (now) watch_changed(&ST.watch, h, ST.it[h].def->id);
}

void ink_state_set_int(int h, int64_t v) {
    if (h < 0 || h >= ST.n || ST.it[h].def->type == INK_ST_STR) return;
    if (ST.it[h].def->type == INK_ST_BOOL) v = v != 0;
    osal_mutex_lock(ST.lock, OSAL_WAIT_FOREVER);
    item_t *it = &ST.it[h];
    bool now = false;
    if (!it->set || it->i != v) {
        it->i = v;
        it->set = true;
        now = due_now(it, osal_now_us());
    }
    osal_mutex_unlock(ST.lock);
    changed(h, now);
}

void ink_state_set_bool(int h, bool v) { ink_state_set_int(h, v ? 1 : 0); }

void ink_state_set_str(int h, const char *s) {
    if (h < 0 || h >= ST.n || ST.it[h].def->type != INK_ST_STR || !s) return;
    osal_mutex_lock(ST.lock, OSAL_WAIT_FOREVER);
    item_t *it = &ST.it[h];
    bool now = false;
    if (!it->set || strncmp(it->s, s, sizeof it->s - 1)) {
        size_t n = strlen(s);
        if (n >= sizeof it->s) n = sizeof it->s - 1;
        memcpy(it->s, s, n);
        it->s[n] = 0;
        it->set = true;
        now = due_now(it, osal_now_us());
    }
    osal_mutex_unlock(ST.lock);
    changed(h, now);
}

/* The minimum-interval timer: deliver the items whose interval has passed, re-arm for the rest. */
static void timer_cb(void *arg) {
    (void)arg;
    int ready[INK_STATE_MAX], nr = 0;
    uint64_t now = osal_now_us(), next = 0;
    osal_mutex_lock(ST.lock, OSAL_WAIT_FOREVER);
    for (int h = 0; h < ST.n; h++) {
        item_t *it = &ST.it[h];
        if (!it->deferred) continue;
        uint64_t due = it->last_notify_us + (uint64_t)it->def->min_interval_ms * 1000u;
        if (due <= now) {
            it->deferred = false;
            it->last_notify_us = now;
            ready[nr++] = h;
        } else if (!next || due < next) next = due;
    }
    ST.timer_due = next;
    if (next) osal_timer_start(ST.timer, next - now, 0);
    osal_mutex_unlock(ST.lock);
    for (int i = 0; i < nr; i++) watch_changed(&ST.watch, ready[i], ST.it[ready[i]].def->id);
}

bool ink_state_get_int(int h, int64_t *out) {
    if (h < 0 || h >= ST.n || ST.it[h].def->type == INK_ST_STR) return false;
    osal_mutex_lock(ST.lock, OSAL_WAIT_FOREVER);
    bool set = ST.it[h].set;
    if (set) *out = ST.it[h].i;
    osal_mutex_unlock(ST.lock);
    return set;
}

int ink_state_get_str(int h, char *out, size_t cap) {
    if (h < 0 || h >= ST.n || ST.it[h].def->type != INK_ST_STR || !cap) return -1;
    osal_mutex_lock(ST.lock, OSAL_WAIT_FOREVER);
    int n = -1;
    if (ST.it[h].set) {
        n = (int)strlen(ST.it[h].s);
        size_t k = (size_t)n < cap - 1 ? (size_t)n : cap - 1;
        memcpy(out, ST.it[h].s, k);
        out[k] = 0;
    }
    osal_mutex_unlock(ST.lock);
    return n;
}

int ink_state_watch(const char *prefix, ink_state_fn fn, void *ctx, ink_exec_t exec) {
    return watch_add(&ST.watch, prefix, fn, ctx, exec) >= 0 ? 0 : -1;
}

int ink_state_json(char *out, size_t cap, uint8_t expose) {
    ink_jw_t w;
    ink_jw_init(&w, out, cap);
    ink_jw_raw(&w, "{");
    bool first = true;
    osal_mutex_lock(ST.lock, OSAL_WAIT_FOREVER);
    for (int h = 0; h < ST.n; h++) {
        const item_t *it = &ST.it[h];
        if (!it->set || (expose && it->def->expose && !(it->def->expose & expose))) continue;
        if (!first) ink_jw_raw(&w, ",");
        first = false;
        ink_jw_str(&w, it->def->id);
        ink_jw_raw(&w, ":");
        if (it->def->type == INK_ST_STR) ink_jw_str(&w, it->s);
        else if (it->def->type == INK_ST_BOOL) ink_jw_raw(&w, it->i ? "true" : "false");
        else ink_jw_int(&w, it->i);
    }
    osal_mutex_unlock(ST.lock);
    ink_jw_raw(&w, "}");
    return w.overflow ? -1 : (int)w.len;
}
