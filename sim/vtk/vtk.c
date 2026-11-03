/*
 * vtk — virtual-time kernel. See vtk.h for the model.
 *
 * Scheduling: a sequential discrete-event simulation of N cores. The core
 * with the smallest virtual time acts next; events due before that time fire
 * first (ISR context). Each core runs the highest-priority eligible ready
 * task (FreeRTOS semantics); a running task is preempted at its next
 * scheduling point when a higher-priority task became ready. Exactly one
 * host thread runs simulated code at a time.
 */
#define _GNU_SOURCE
#include "vtk.h"

#include <alloca.h>
#include <errno.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#define STACK_PATTERN 0xA5u
#define MIN_HOST_STACK (64u * 1024u)
#define KERNEL_ZONE (64u * 1024u) /* host stack below each task's own zone, where the kernel runs (in_kernel_zone) */

struct vtk_task {
    int id;
    char name[32];
    vtk_task_fn fn;
    void *arg;
    int prio, base_prio, core_aff;
    uint32_t flags;
    vtk_state_t state;
    pthread_t th;
    pthread_cond_t cv;
    bool go;
    int core;
    uint64_t ready_at;
    uint64_t ready_seq;
    /* waiting */
    vtk_waitq_t *wq;
    vtk_task_t *wq_next;
    uint64_t wq_seq;
    bool woken;
    vtk_event_id_t timeout_ev;
    vtk_waitq_t sleepq;
    /* accounting */
    uint64_t cpu_ns, stall_ns, max_burst_ns, preempt_delay_max_ns, switches;
    uint64_t last_host_cpu_ns;
    uint64_t pending_ns;
    uint64_t run_since;
    unsigned burst_warns;
    /* stack: [guard][kernel zone][own zone], see in_kernel_zone */
    uint8_t *stack_mem;
    size_t stack_total;
    size_t guard;
    uint32_t stack_target;
    uint8_t *own_lo;     /* lowest byte of the task's own zone */
    uint8_t *entry;      /* stack position when the task function was entered */
    size_t used_at_exit; /* own_stack_used() when the task exited */
};

typedef struct {
    uint64_t t;
    uint64_t seq;
    vtk_event_id_t id;
    vtk_event_fn fn;
    void *arg;
    bool cancelled;
} vtk_ev_t;

typedef struct ext_item {
    vtk_event_fn fn;
    void *arg;
    struct ext_item *next;
} ext_item_t;

static struct {
    vtk_config_t cfg;
    pthread_mutex_t m;
    pthread_cond_t main_cv;
    bool initialized, running, finished, stop_req;
    vtk_task_t *tasks[VTK_MAX_TASKS];
    int ntasks;
    uint64_t now[VTK_MAX_CORES];
    vtk_task_t *cur[VTK_MAX_CORES];
    vtk_task_t *last[VTK_MAX_CORES];
    vtk_ev_t *heap;
    size_t nheap, capheap;
    uint64_t seq;
    vtk_event_id_t next_id;
    bool in_isr;
    uint64_t isr_now;
    uint64_t stall_from, stall_until, stall_total;
    uint64_t stall_hist_a[16], stall_hist_b[16]; /* recent stall windows (work crossing one freezes inside it) */
    int stall_hist_last;
    vtk_task_t *sleeper; /* light sleep: only this task may run (it called the sleep) */
    vtk_stall_hook_fn stall_hook;
    /* external injection (realtime) */
    pthread_mutex_t ext_m;
    pthread_cond_t ext_cv;
    ext_item_t *ext_head, *ext_tail;
    struct timespec rt_start;
    uint64_t rt_wall0, rt_virt0; /* realtime anchor: virtual rt_virt0 at wall rt_wall0 (speed changes, pause) */
    bool rt_paused;
    vtk_warn_fn warn;
    vtk_switch_hook_fn hook;
    vtk_task_t *stack_probe;
    uint64_t busy[VTK_MAX_CORES];
    uint64_t stall_busy[VTK_MAX_CORES]; /* cores spinning while the flash API runs */
    uint64_t slice_start[VTK_MAX_CORES];
} K;

static __thread vtk_task_t *tls_self;

/* ------------------------------------------------------------------ utils */

/* Everything the kernel does on a task's host thread (pthread waits, event callbacks, trace
 * hooks, creating tasks, the exit path) runs in a zone at the bottom of that thread's stack,
 * below the task's own zone. The stack estimate then sees only the task's code: it does not
 * depend on host timing (which pthread paths run) or on how deep the host's libc goes. */
__attribute__((noinline)) static void in_zone_of(const vtk_task_t *t, void (*fn)(void *), void *arg, bool wipe) {
    uint8_t *here = __builtin_frame_address(0);
    if (!t || here <= t->own_lo) { /* main thread, or already in the kernel zone */
        fn(arg);
        return;
    }
    size_t n = (size_t)(here - t->own_lo);
    uint8_t *volatile gap = alloca(n); /* moves the stack pointer, writes nothing */
    /* At thread start: whatever the host's thread start left below us (glibc writes ~1.9 KiB) is not
     * the task's. The own zone's part of the gap is unused, and memset runs below it. */
    if (wipe && gap <= t->own_lo) memset(t->own_lo, STACK_PATTERN, (size_t)(gap + n - t->own_lo));
    fn(arg);
    (void)gap; /* used after the call, so fn is not a tail call and does run below the gap */
}

static void in_kernel_zone(void (*fn)(void *), void *arg) { in_zone_of(tls_self, fn, arg, false); }

void vtk_host_call(void (*fn)(void *), void *arg) { in_kernel_zone(fn, arg); }

static void default_warn(const char *msg) { fprintf(stderr, "[vtk] %s\n", msg); }

static void warnf(const char *fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    (K.warn ? K.warn : default_warn)(buf);
}

static uint64_t host_cpu_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

static uint64_t wall_ns_since_start(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    int64_t s = (int64_t)ts.tv_sec - (int64_t)K.rt_start.tv_sec;
    int64_t ns = (int64_t)ts.tv_nsec - (int64_t)K.rt_start.tv_nsec;
    return (uint64_t)(s * 1000000000ll + ns);
}

static uint64_t min_now(void) {
    uint64_t m = VTK_TIME_INF;
    for (int c = 0; c < K.cfg.ncores; c++)
        if (K.now[c] < m) m = K.now[c];
    return m;
}

/* ---------------------------------------------------------------- events */

static bool ev_less(const vtk_ev_t *a, const vtk_ev_t *b) {
    return a->t < b->t || (a->t == b->t && a->seq < b->seq);
}

static void heap_grow(void *arg) {
    (void)arg;
    K.capheap = K.capheap ? K.capheap * 2 : 256;
    K.heap = realloc(K.heap, K.capheap * sizeof(vtk_ev_t));
    if (!K.heap) abort();
}

static void heap_push(vtk_ev_t e) {
    if (K.nheap == K.capheap) in_kernel_zone(heap_grow, NULL);
    size_t i = K.nheap++;
    K.heap[i] = e;
    while (i > 0) {
        size_t p = (i - 1) / 2;
        if (!ev_less(&K.heap[i], &K.heap[p])) break;
        vtk_ev_t tmp = K.heap[i];
        K.heap[i] = K.heap[p];
        K.heap[p] = tmp;
        i = p;
    }
}

static vtk_ev_t heap_pop(void) {
    vtk_ev_t top = K.heap[0];
    K.heap[0] = K.heap[--K.nheap];
    size_t i = 0;
    for (;;) {
        size_t l = 2 * i + 1, r = l + 1, s = i;
        if (l < K.nheap && ev_less(&K.heap[l], &K.heap[s])) s = l;
        if (r < K.nheap && ev_less(&K.heap[r], &K.heap[s])) s = r;
        if (s == i) break;
        vtk_ev_t tmp = K.heap[i];
        K.heap[i] = K.heap[s];
        K.heap[s] = tmp;
        i = s;
    }
    return top;
}

static void heap_drop_cancelled_top(void) {
    while (K.nheap && K.heap[0].cancelled) (void)heap_pop();
}

static vtk_event_id_t event_at_locked(uint64_t t, vtk_event_fn fn, void *arg) {
    vtk_ev_t e = {.t = t, .seq = K.seq++, .id = ++K.next_id, .fn = fn, .arg = arg, .cancelled = false};
    heap_push(e);
    return e.id;
}

static void event_cancel_locked(vtk_event_id_t id) {
    if (!id) return;
    for (size_t i = 0; i < K.nheap; i++)
        if (K.heap[i].id == id) {
            K.heap[i].cancelled = true;
            return;
        }
}

static uint64_t next_event_time(void) {
    heap_drop_cancelled_top();
    return K.nheap ? K.heap[0].t : VTK_TIME_INF;
}

static void fire_events_upto_locked(uint64_t t0) {
    for (;;) {
        heap_drop_cancelled_top();
        if (!K.nheap || K.heap[0].t > t0) break;
        vtk_ev_t e = heap_pop();
        K.in_isr = true;
        K.isr_now = e.t;
        e.fn(e.arg);
        K.in_isr = false;
    }
}

/* --------------------------------------------------------------- tasks */

static bool stalled_at(uint64_t t) { return t >= K.stall_from && t < K.stall_until; }

static bool eligible(const vtk_task_t *t, int c) {
    if (t->state != VTK_ST_READY) return false;
    if (t->ready_at > K.now[c]) return false;
    if (t->core_aff >= 0 && t->core_aff != c) return false;
    if (stalled_at(K.now[c])) return false; /* scheduler suspended on all cores */
    if (K.sleeper && t != K.sleeper) return false; /* the chip sleeps: everything else waits for the wake */
    return true;
}

static vtk_task_t *best_ready_for_core(int c) {
    vtk_task_t *best = NULL;
    for (int i = 0; i < K.ntasks; i++) {
        vtk_task_t *t = K.tasks[i];
        if (!eligible(t, c)) continue;
        if (!best || t->prio > best->prio ||
            (t->prio == best->prio && (t->ready_at < best->ready_at ||
                                       (t->ready_at == best->ready_at && t->ready_seq < best->ready_seq))))
            best = t;
    }
    return best;
}

static void make_ready_locked(vtk_task_t *t, uint64_t at) {
    t->state = VTK_ST_READY;
    t->ready_at = at;
    t->ready_seq = K.seq++;
}

typedef struct {
    int core;
    const vtk_task_t *t;
    bool running;
    uint64_t t_ns;
} switch_args_t;

static void switch_hook_in_zone(void *p) {
    const switch_args_t *a = p;
    K.hook(a->core, a->t, a->running, a->t_ns);
}

/* Also reached from task context (a task blocks or yields): the hook writes the trace. */
static void switch_hook(int c, const vtk_task_t *t, bool running, uint64_t t_ns) {
    if (!K.hook) return;
    switch_args_t a = {c, t, running, t_ns};
    in_kernel_zone(switch_hook_in_zone, &a);
}

static void start_on_core_locked(vtk_task_t *t, int c) {
    uint64_t delay = K.now[c] - t->ready_at;
    if (delay > t->preempt_delay_max_ns) t->preempt_delay_max_ns = delay;
    uint64_t slice_from = K.now[c];
    if (K.last[c] && K.last[c] != t) {
        K.now[c] += K.cfg.ctx_switch_ns;
        t->cpu_ns += K.cfg.ctx_switch_ns;
    }
    K.last[c] = t;
    t->state = VTK_ST_RUNNING;
    t->core = c;
    t->run_since = K.now[c];
    t->switches++;
    K.cur[c] = t;
    K.slice_start[c] = slice_from;
    switch_hook(c, t, true, slice_from);
}

/* The task running on core c stops running (blocks, is preempted, sleeps or exits). */
static void core_release_locked(int c) {
    vtk_task_t *t = K.cur[c];
    if (!t) return;
    K.busy[c] += K.now[c] - K.slice_start[c];
    switch_hook(c, t, false, K.now[c]);
    K.cur[c] = NULL;
}

/* Returns the task that should execute next on core c, or NULL (core time may have advanced). */
static vtk_task_t *dispatch_on_core_locked(int c) {
    vtk_task_t *cur = K.cur[c];
    vtk_task_t *cand = best_ready_for_core(c);
    if (cur && K.sleeper && cur != K.sleeper) { /* light sleep began: this task stops where it is */
        make_ready_locked(cur, K.now[c]);
        core_release_locked(c);
        cur = NULL;
    }
    if (cur) {
        if (stalled_at(K.now[c])) {
            uint64_t d = K.stall_until - K.now[c];
            cur->stall_ns += d;
            K.now[c] = K.stall_until;
            K.slice_start[c] += d; /* frozen; the core's spin time is counted in stall_busy */
            return NULL;
        }
        bool rotate = false;
        if (cand && cand->prio == cur->prio && K.cfg.tick_ns) {
            /* FreeRTOS time slicing: at every tick an equal-priority ready task gets the core */
            uint64_t tb = K.now[c] / K.cfg.tick_ns * K.cfg.tick_ns;
            rotate = cur->run_since < tb;
        }
        if (cand && (cand->prio > cur->prio || rotate)) {
            make_ready_locked(cur, K.now[c]);
            core_release_locked(c);
            start_on_core_locked(cand, c);
            return cand;
        }
        return cur;
    }
    if (cand) {
        start_on_core_locked(cand, c);
        return cand;
    }
    return NULL;
}

static int pick_core_locked(uint64_t t0) {
    for (int c = 0; c < K.cfg.ncores; c++)
        if (K.now[c] == t0 && K.cur[c]) return c;
    for (int c = 0; c < K.cfg.ncores; c++)
        if (K.now[c] == t0 && best_ready_for_core(c)) return c;
    return -1;
}

static uint64_t next_interesting_time_locked(uint64_t t0) {
    uint64_t t1 = next_event_time();
    for (int c = 0; c < K.cfg.ncores; c++)
        if (K.now[c] > t0 && K.now[c] < t1) t1 = K.now[c];
    for (int i = 0; i < K.ntasks; i++) {
        vtk_task_t *t = K.tasks[i];
        if (t->state != VTK_ST_READY) continue;
        uint64_t when = t->ready_at > t0 ? t->ready_at : t0;
        if (stalled_at(when)) when = K.stall_until;
        if (when > t0 && when < t1) t1 = when;
        /* ready but pinned to a core that is ahead: that core's time is already counted above */
    }
    return t1;
}

static void finish_locked(void) {
    if (!K.finished) {
        K.finished = true;
        pthread_cond_broadcast(&K.main_cv);
    }
}

static void park_forever_locked(vtk_task_t *self) {
    for (;;) pthread_cond_wait(&self->cv, &K.m);
}

static void hand_baton_locked(vtk_task_t *t) {
    t->go = true;
    pthread_cond_signal(&t->cv);
}

static void wait_baton_locked(vtk_task_t *self) {
    while (!self->go) pthread_cond_wait(&self->cv, &K.m);
    self->go = false;
}

static void pull_external_locked(uint64_t at) {
    pthread_mutex_lock(&K.ext_m);
    ext_item_t *it = K.ext_head;
    K.ext_head = K.ext_tail = NULL;
    pthread_mutex_unlock(&K.ext_m);
    while (it) {
        ext_item_t *n = it->next;
        event_at_locked(at, it->fn, it->arg);
        free(it);
        it = n;
    }
}

/* Pace to wall clock or wait for external input. Returns the (possibly earlier) time to advance to. */
static uint64_t realtime_wait_locked(uint64_t target) {
    for (;;) {
        double speed = K.cfg.realtime_speed > 0 ? K.cfg.realtime_speed : 1.0;
        uint64_t wall = wall_ns_since_start();
        uint64_t wall_v = K.rt_virt0 + (uint64_t)((double)(wall > K.rt_wall0 ? wall - K.rt_wall0 : 0) * speed);
        pthread_mutex_lock(&K.ext_m);
        bool have_ext = K.ext_head != NULL;
        pthread_mutex_unlock(&K.ext_m);
        if (K.rt_paused && !K.stop_req) {
            wall_v = 0; /* frozen: no time passes and external input waits */
            have_ext = false;
        }
        if (have_ext) {
            uint64_t at = wall_v < target ? wall_v : target;
            if (at < min_now()) at = min_now();
            pull_external_locked(at);
            return at;
        }
        if (K.stop_req || (!K.rt_paused && wall_v >= target)) return target;
        uint64_t wait_ns = K.rt_paused ? 50000000ull : (uint64_t)((double)(target - wall_v) / speed);
        if (wait_ns > 50000000ull) wait_ns = 50000000ull;
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_nsec += (long)wait_ns;
        while (ts.tv_nsec >= 1000000000L) {
            ts.tv_sec++;
            ts.tv_nsec -= 1000000000L;
        }
        pthread_mutex_unlock(&K.m);
        pthread_mutex_lock(&K.ext_m);
        if (!K.ext_head) pthread_cond_timedwait(&K.ext_cv, &K.ext_m, &ts);
        pthread_mutex_unlock(&K.ext_m);
        pthread_mutex_lock(&K.m);
    }
}

/* Core scheduler. Called with K.m held by `self` (or NULL from the main thread).
 * Returns when `self` should continue executing (it holds the baton). */
static void schedule_locked(vtk_task_t *self) {
    for (;;) {
        if (K.finished) {
            if (self && self->state != VTK_ST_DONE) park_forever_locked(self);
            return;
        }
        if (K.stop_req) {
            finish_locked();
            continue;
        }
        uint64_t t0 = min_now();
        if (t0 >= K.cfg.end_time_ns) {
            finish_locked();
            continue;
        }
        fire_events_upto_locked(t0);
        int c = pick_core_locked(t0);
        if (c >= 0) {
            vtk_task_t *run = dispatch_on_core_locked(c);
            if (!run) continue;
            if (run == self) return;
            hand_baton_locked(run);
            if (!self || self->state == VTK_ST_DONE) return;
            wait_baton_locked(self);
            if (K.finished) park_forever_locked(self);
            return;
        }
        uint64_t t1 = next_interesting_time_locked(t0);
        if (K.cfg.realtime) {
            uint64_t target = t1 == VTK_TIME_INF ? t0 + 100000000ull : t1;
            if (target > K.cfg.end_time_ns) target = K.cfg.end_time_ns;
            t1 = realtime_wait_locked(target);
            if (t1 < t0) t1 = t0;
        } else if (t1 == VTK_TIME_INF) {
            finish_locked();
            continue;
        }
        if (t1 > K.cfg.end_time_ns) t1 = K.cfg.end_time_ns;
        for (int k = 0; k < K.cfg.ncores; k++)
            if (K.now[k] == t0 && !K.cur[k]) K.now[k] = t1;
    }
}

/* --------------------------------------------------- kernel call helpers */

/* `cost` of CPU from s: work stops at the start of every stall window it runs into and resumes at
 * its end (the window may have been registered by another core after this work began). */
static uint64_t run_through_stalls_locked(uint64_t s, uint64_t cost, uint64_t *frozen) {
    uint64_t t = s, left = cost;
    *frozen = 0;
    for (;;) {
        int best = -1;
        for (int i = 0; i < 16; i++) {
            uint64_t a = K.stall_hist_a[i], b = K.stall_hist_b[i];
            if (b > a && b > t && a < t + left && (best < 0 || a < K.stall_hist_a[best])) best = i;
        }
        if (best < 0) return t + left;
        uint64_t a = K.stall_hist_a[best];
        if (a > t) {
            left -= a - t;
            t = a;
        }
        *frozen += K.stall_hist_b[best] - t;
        t = K.stall_hist_b[best];
    }
}

static void charge_locked(vtk_task_t *t, uint64_t host_now, bool kernel_call) {
    uint64_t cost = t->pending_ns + (kernel_call ? K.cfg.sched_overhead_ns : 0);
    t->pending_ns = 0;
    if (K.cfg.cpu_mode == VTK_CPU_MEASURED && host_now >= t->last_host_cpu_ns) {
        double d = (double)(host_now - t->last_host_cpu_ns);
        cost += (uint64_t)(d * K.cfg.host_to_target);
    }
    int c = t->core;
    uint64_t frozen = 0;
    K.now[c] = run_through_stalls_locked(K.now[c], cost, &frozen);
    if (frozen) {
        t->stall_ns += frozen;
        K.slice_start[c] += frozen; /* the frozen time is counted in stall_busy */
    }
    t->cpu_ns += cost;
    if (cost > t->max_burst_ns) t->max_burst_ns = cost;
    if (K.cfg.long_burst_warn_ns && cost > K.cfg.long_burst_warn_ns && t->burst_warns < 5) {
        t->burst_warns++;
        warnf("task '%s' ran %.2f ms without entering the kernel: the simulator cannot preempt inside, so "
              "higher-priority work looks delayed; add osal_preempt_point() to long loops (free on the device)",
              t->name, cost / 1e6);
    }
}

typedef struct {
    vtk_task_t *self;
    bool kernel_call;
} kenter_args_t;

static void kenter_in_zone(void *p) {
    const kenter_args_t *a = p;
    uint64_t h = (K.cfg.cpu_mode == VTK_CPU_MEASURED) ? host_cpu_ns() : 0;
    pthread_mutex_lock(&K.m);
    charge_locked(a->self, h, a->kernel_call);
}

static void kenter_ex(vtk_task_t *self, bool kernel_call) {
    kenter_args_t a = {self, kernel_call};
    in_kernel_zone(kenter_in_zone, &a);
}

/* A real RTOS call: charges the kernel overhead. */
static void kenter(vtk_task_t *self) { kenter_ex(self, true); }

static void schedule_in_zone(void *self) { schedule_locked(self); }

static void kexit_in_zone(void *p) {
    vtk_task_t *self = p;
    schedule_locked(self);
    if (K.cfg.cpu_mode == VTK_CPU_MEASURED) self->last_host_cpu_ns = host_cpu_ns();
    pthread_mutex_unlock(&K.m);
}

static void kexit(vtk_task_t *self) { in_kernel_zone(kexit_in_zone, self); }

/* ------------------------------------------------------------ public */

void vtk_init(const vtk_config_t *cfg) {
    memset(&K, 0, sizeof K);
    K.cfg = *cfg;
    if (K.cfg.ncores < 1) K.cfg.ncores = 1;
    if (K.cfg.ncores > VTK_MAX_CORES) K.cfg.ncores = VTK_MAX_CORES;
    if (K.cfg.host_stack_ratio <= 0) K.cfg.host_stack_ratio = 2.0;
    if (K.cfg.end_time_ns == 0) K.cfg.end_time_ns = VTK_TIME_INF;
    pthread_mutex_init(&K.m, NULL);
    pthread_cond_init(&K.main_cv, NULL);
    pthread_mutex_init(&K.ext_m, NULL);
    pthread_cond_init(&K.ext_cv, NULL);
    K.warn = default_warn;
    K.initialized = true;
}

const vtk_config_t *vtk_config(void) { return &K.cfg; }
void vtk_set_warn_sink(vtk_warn_fn fn) { K.warn = fn ? fn : default_warn; }
void vtk_set_switch_hook(vtk_switch_hook_fn fn) { K.hook = fn; }
uint64_t vtk_core_busy_ns(int core) {
    if (core < 0 || core >= K.cfg.ncores) return 0;
    uint64_t b = K.busy[core] + K.stall_busy[core];
    if (K.cur[core]) b += K.now[core] - K.slice_start[core];
    return b;
}
int vtk_task_index(const vtk_task_t *t) { return t ? t->id : -1; }
bool vtk_in_isr(void) { return K.in_isr; }

static void first_baton_in_zone(void *p) {
    vtk_task_t *t = p;
    tls_self = t; /* Apple platforms allocate a thread's TLS on first use (dyld + malloc) */
    pthread_mutex_lock(&K.m);
    wait_baton_locked(t);
    if (K.finished) park_forever_locked(t);
    if (K.cfg.cpu_mode == VTK_CPU_MEASURED) t->last_host_cpu_ns = host_cpu_ns();
    pthread_mutex_unlock(&K.m);
}

static void *task_thread(void *p) {
    vtk_task_t *t = p;
    in_zone_of(t, first_baton_in_zone, t, true);
    t->entry = __builtin_frame_address(0);
    t->fn(t->arg);
    vtk_task_exit();
}

typedef struct {
    const char *name;
    vtk_task_fn fn;
    void *arg;
    int priority, core;
    uint32_t flags, stack_target_bytes;
    vtk_task_t *created;
} create_args_t;

static vtk_task_t *task_create(const char *name, vtk_task_fn fn, void *arg, int priority, int core, uint32_t flags,
                               uint32_t stack_target_bytes);

static void task_create_in_zone(void *p) {
    create_args_t *a = p;
    a->created = task_create(a->name, a->fn, a->arg, a->priority, a->core, a->flags, a->stack_target_bytes);
}

vtk_task_t *vtk_task_create(const char *name, vtk_task_fn fn, void *arg, int priority, int core,
                            uint32_t flags, uint32_t stack_target_bytes) {
    create_args_t a = {name, fn, arg, priority, core, flags, stack_target_bytes, NULL};
    in_kernel_zone(task_create_in_zone, &a); /* from a task: host allocations and pthread_create are not its stack */
    return a.created;
}

static vtk_task_t *task_create(const char *name, vtk_task_fn fn, void *arg, int priority, int core, uint32_t flags,
                               uint32_t stack_target_bytes) {
    vtk_task_t *t = calloc(1, sizeof *t);
    if (!t) return NULL;
    snprintf(t->name, sizeof t->name, "%s", name ? name : "task");
    t->fn = fn;
    t->arg = arg;
    t->prio = t->base_prio = priority;
    t->core_aff = core < K.cfg.ncores ? core : -1;
    t->flags = flags;
    t->stack_target = stack_target_bytes;
    pthread_cond_init(&t->cv, NULL);

    long page = sysconf(_SC_PAGESIZE);
    size_t want = (size_t)(stack_target_bytes * K.cfg.host_stack_ratio * 1.25);
    if (want < MIN_HOST_STACK) want = MIN_HOST_STACK;
    want = (want + (size_t)page - 1) & ~((size_t)page - 1);
    t->guard = (size_t)page;
    t->stack_total = t->guard + KERNEL_ZONE + want;
    t->stack_mem = mmap(NULL, t->stack_total, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (t->stack_mem == MAP_FAILED) {
        free(t);
        return NULL;
    }
    mprotect(t->stack_mem, t->guard, PROT_NONE);
    t->own_lo = t->stack_mem + t->guard + KERNEL_ZONE;
    memset(t->own_lo, STACK_PATTERN, want);

    bool from_task = tls_self != NULL;
    if (from_task) kenter(tls_self);
    else pthread_mutex_lock(&K.m);

    if (K.ntasks >= VTK_MAX_TASKS) {
        if (from_task) kexit(tls_self);
        else pthread_mutex_unlock(&K.m);
        munmap(t->stack_mem, t->stack_total);
        free(t);
        return NULL;
    }
    t->id = K.ntasks;
    K.tasks[K.ntasks++] = t;
    uint64_t now = from_task ? K.now[tls_self->core] : min_now();
    if (now == VTK_TIME_INF) now = 0;
    make_ready_locked(t, now);

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstack(&attr, t->stack_mem + t->guard, KERNEL_ZONE + want);
    int rc = pthread_create(&t->th, &attr, task_thread, t);
    pthread_attr_destroy(&attr);
    if (rc != 0) {
        fprintf(stderr, "[vtk] pthread_create failed: %s\n", strerror(rc));
        abort();
    }
    if (from_task) kexit(tls_self);
    else pthread_mutex_unlock(&K.m);
    return t;
}

void vtk_run(void) {
    clock_gettime(CLOCK_MONOTONIC, &K.rt_start);
    pthread_mutex_lock(&K.m);
    K.rt_wall0 = 0;
    K.rt_virt0 = min_now();
    K.running = true;
    schedule_locked(NULL);
    while (!K.finished) pthread_cond_wait(&K.main_cv, &K.m);
    K.running = false;
    pthread_mutex_unlock(&K.m);
}

void vtk_stop(void) {
    K.stop_req = true;
    pthread_mutex_lock(&K.ext_m);
    pthread_cond_broadcast(&K.ext_cv);
    pthread_mutex_unlock(&K.ext_m);
}

bool vtk_stopping(void) { return K.stop_req || K.finished; }

static void realtime_kick(void) {
    pthread_mutex_lock(&K.ext_m);
    pthread_cond_broadcast(&K.ext_cv);
    pthread_mutex_unlock(&K.ext_m);
}

/* Virtual time continues from where the simulation is now, at the new rate. */
static void realtime_anchor_locked(void) {
    K.rt_wall0 = wall_ns_since_start();
    K.rt_virt0 = min_now();
}

void vtk_set_realtime_speed(double speed) {
    pthread_mutex_lock(&K.m);
    if (K.running) realtime_anchor_locked();
    K.cfg.realtime_speed = speed > 0 ? speed : 1.0;
    pthread_mutex_unlock(&K.m);
    realtime_kick();
}

void vtk_set_paused(bool paused) {
    pthread_mutex_lock(&K.m);
    if (K.rt_paused != paused) {
        K.rt_paused = paused;
        if (K.running) realtime_anchor_locked();
    }
    pthread_mutex_unlock(&K.m);
    realtime_kick();
}

uint64_t vtk_now(void) {
    if (K.in_isr) return K.isr_now;
    vtk_task_t *t = tls_self;
    if (t) return K.now[t->core] + t->pending_ns;
    uint64_t m = min_now();
    return m == VTK_TIME_INF ? 0 : m;
}

vtk_task_t *vtk_self(void) { return tls_self; }
int vtk_current_core(void) { return tls_self && !K.in_isr ? tls_self->core : -1; }
const char *vtk_task_name(const vtk_task_t *t) { return t ? t->name : "?"; }
int vtk_task_count(void) { return K.ntasks; }

/* Host bytes the task's own code has used below its entry point (pattern scan of its own zone). */
static size_t own_stack_used(const vtk_task_t *t) {
    if (!t->entry) return 0; /* never ran */
    const uint8_t *p = t->own_lo;
    size_t n = (size_t)(t->entry - p), i = 0;
    while (i < n && p[i] == STACK_PATTERN) i++;
    return n - i;
}

static size_t host_stack_used(const vtk_task_t *t) {
    return t->state == VTK_ST_DONE ? t->used_at_exit : own_stack_used(t);
}

/* Target stack estimate: host bytes the task's code used below its entry point, minus what an
 * empty probe task needs to reach the kernel, scaled by the host/target ratio. Thread start,
 * the kernel and the exit path are not counted (in_kernel_zone). Rough; the real check is
 * -fstack-usage on the RISC-V build. */
static uint32_t stack_used_est(const vtk_task_t *t) {
    size_t used_host = host_stack_used(t);
    size_t base = K.stack_probe ? host_stack_used(K.stack_probe) : 0;
    size_t net = used_host > base ? used_host - base : 0;
    return (uint32_t)((double)net / K.cfg.host_stack_ratio) + 256u; /* + minimal RTOS frame */
}

static void stack_probe_fn(void *arg) {
    (void)arg;
    vtk_yield();
    vtk_sleep_ns(1);
}

void vtk_calibrate_stack(void) {
    K.stack_probe = vtk_task_create("(stack-probe)", stack_probe_fn, NULL, 0, -1, 0, 0);
}

void vtk_task_info(int index, vtk_task_info_t *out) {
    memset(out, 0, sizeof *out);
    if (index < 0 || index >= K.ntasks) return;
    vtk_task_t *t = K.tasks[index];
    snprintf(out->name, sizeof out->name, "%s", t->name);
    out->priority = t->base_prio;
    out->core = t->core_aff;
    out->state = t->state;
    out->cpu_ns = t->cpu_ns;
    out->stall_ns = t->stall_ns;
    out->max_burst_ns = t->max_burst_ns;
    out->preempt_delay_max_ns = t->preempt_delay_max_ns;
    out->stack_target_bytes = t->stack_target;
    out->stack_used_est_bytes = stack_used_est(t);
    out->switches = t->switches;
}

/* Preemption check: free (on the device preemption is interrupt-driven, not a call). */
void vtk_sched_point(void) {
    vtk_task_t *self = tls_self;
    if (!self) return;
    kenter_ex(self, false);
    kexit(self);
}

#define PREEMPT_QUANTUM_NS 200000ull

void vtk_consume_ns(uint64_t ns) {
    vtk_task_t *self = tls_self;
    if (!self || K.in_isr) return;
    /* charge long work in slices so higher-priority tasks preempt within ~200 us, like on the device */
    while (ns) {
        uint64_t room = self->pending_ns < PREEMPT_QUANTUM_NS ? PREEMPT_QUANTUM_NS - self->pending_ns : 0;
        uint64_t take = ns < room ? ns : room;
        self->pending_ns += take;
        ns -= take;
        if (self->pending_ns >= PREEMPT_QUANTUM_NS) vtk_sched_point();
    }
}

void vtk_lock(void) {
    vtk_task_t *self = tls_self;
    if (self) kenter(self);
    else pthread_mutex_lock(&K.m);
}

void vtk_unlock(void) {
    vtk_task_t *self = tls_self;
    if (self) kexit(self);
    else pthread_mutex_unlock(&K.m);
}

static void timeout_cb(void *arg) {
    vtk_task_t *t = arg;
    t->timeout_ev = 0;
    if (t->state != VTK_ST_BLOCKED || !t->wq) return;
    vtk_task_t **pp = &t->wq->head;
    while (*pp && *pp != t) pp = &(*pp)->wq_next;
    if (*pp) *pp = t->wq_next;
    t->wq = NULL;
    t->wq_next = NULL;
    t->woken = false;
    make_ready_locked(t, K.isr_now);
}

bool vtk_block_locked(vtk_waitq_t *wq, uint64_t timeout_ns) {
    vtk_task_t *self = tls_self;
    if (!self) return false;
    int c = self->core;
    self->state = VTK_ST_BLOCKED;
    self->woken = false;
    self->wq = wq;
    self->wq_next = NULL;
    self->wq_seq = K.seq++;
    vtk_task_t **pp = &wq->head;
    while (*pp) pp = &(*pp)->wq_next;
    *pp = self;
    if (timeout_ns != VTK_TIME_INF) {
        uint64_t when = K.now[c] + timeout_ns;
        if (when < K.now[c]) when = VTK_TIME_INF - 1;
        self->timeout_ev = event_at_locked(when, timeout_cb, self);
    }
    core_release_locked(c);
    in_kernel_zone(schedule_in_zone, self);
    if (self->timeout_ev) {
        event_cancel_locked(self->timeout_ev);
        self->timeout_ev = 0;
    }
    return self->woken;
}

static vtk_task_t *wake_one_at_locked(vtk_waitq_t *wq, uint64_t at) {
    vtk_task_t **best = NULL;
    for (vtk_task_t **pp = &wq->head; *pp; pp = &(*pp)->wq_next)
        if (!best || (*pp)->prio > (*best)->prio) best = pp;
    if (!best) return NULL;
    vtk_task_t *t = *best;
    *best = t->wq_next;
    t->wq = NULL;
    t->wq_next = NULL;
    t->woken = true;
    if (t->timeout_ev) {
        event_cancel_locked(t->timeout_ev);
        t->timeout_ev = 0;
    }
    make_ready_locked(t, at);
    return t;
}

static uint64_t caller_time(void) {
    if (K.in_isr) return K.isr_now;
    vtk_task_t *t = tls_self;
    return t ? K.now[t->core] : min_now();
}

vtk_task_t *vtk_wake_one_locked(vtk_waitq_t *wq) { return wake_one_at_locked(wq, caller_time()); }

int vtk_wake_all_locked(vtk_waitq_t *wq) {
    int n = 0;
    while (wake_one_at_locked(wq, caller_time())) n++;
    return n;
}

vtk_task_t *vtk_isr_wake_one(vtk_waitq_t *wq) { return wake_one_at_locked(wq, K.isr_now); }

int vtk_waitq_max_priority(const vtk_waitq_t *wq) {
    int m = -1;
    for (const vtk_task_t *t = wq->head; t; t = t->wq_next)
        if (t->prio > m) m = t->prio;
    return m;
}

void vtk_sleep_ns(uint64_t ns) {
    vtk_task_t *self = tls_self;
    if (!self) return;
    kenter(self);
    if (ns == 0) {
        make_ready_locked(self, K.now[self->core]);
        core_release_locked(self->core);
    } else {
        (void)vtk_block_locked(&self->sleepq, ns);
    }
    kexit(self);
}

void vtk_yield(void) { vtk_sleep_ns(0); }

static void task_exit_in_zone(void *p) {
    vtk_task_t *self = p;
    kenter(self);
    self->state = VTK_ST_DONE;
    core_release_locked(self->core);
    schedule_locked(self);
    pthread_mutex_unlock(&K.m);
    pthread_exit(NULL);
}

void vtk_task_exit(void) {
    vtk_task_t *self = tls_self;
    self->used_at_exit = own_stack_used(self); /* glibc's pthread_exit unwinds back up into the task's zone */
    in_kernel_zone(task_exit_in_zone, self);
    abort(); /* not reached */
}

void vtk_set_priority(vtk_task_t *t, int prio) { t->prio = prio; }
int vtk_priority(const vtk_task_t *t) { return t->prio; }
int vtk_base_priority(const vtk_task_t *t) { return t->base_prio; }

vtk_event_id_t vtk_event_at(uint64_t time_ns, vtk_event_fn fn, void *arg) {
    bool need_lock = !K.in_isr && !tls_self;
    /* From task context callers must hold the lock via vtk_lock(); from main thread we lock here. */
    if (need_lock) pthread_mutex_lock(&K.m);
    vtk_event_id_t id = event_at_locked(time_ns, fn, arg);
    if (need_lock) pthread_mutex_unlock(&K.m);
    return id;
}

vtk_event_id_t vtk_event_after(uint64_t delay_ns, vtk_event_fn fn, void *arg) {
    return vtk_event_at(caller_time() + delay_ns, fn, arg);
}

void vtk_event_cancel(vtk_event_id_t id) { event_cancel_locked(id); }
uint64_t vtk_isr_now(void) { return K.isr_now; }

/* Cores whose simulated time already ran past the stall start executed work that
 * the real core could not have done while frozen: push that work later. */
static void shift_cores_ahead_locked(uint64_t from, uint64_t shift) {
    vtk_task_t *self = tls_self;
    for (int c = 0; c < K.cfg.ncores; c++) {
        if (self && self->core == c && !K.in_isr) continue;
        vtk_task_t *t = K.cur[c];
        if (!t || K.now[c] <= from) continue;
        K.now[c] += shift;
        K.slice_start[c] += shift;
        t->stall_ns += shift;
    }
}

static void stall_busy_add_locked(uint64_t d) {
    for (int c = 0; c < K.cfg.ncores; c++) K.stall_busy[c] += d;
}

void vtk_stall_begin_locked(uint64_t until_ns) {
    uint64_t from = caller_time();
    if (until_ns <= from) return;
    uint64_t added_from = from;
    if (from < K.stall_until && K.stall_until > K.stall_from) {
        if (until_ns <= K.stall_until) return;
        uint64_t ext = until_ns - K.stall_until;
        added_from = K.stall_until;
        shift_cores_ahead_locked(K.stall_until, ext);
        K.stall_total += ext;
        stall_busy_add_locked(ext);
        K.stall_until = until_ns;
        K.stall_hist_b[K.stall_hist_last] = until_ns;
    } else {
        shift_cores_ahead_locked(from, until_ns - from);
        K.stall_from = from;
        K.stall_until = until_ns;
        K.stall_total += until_ns - from;
        stall_busy_add_locked(until_ns - from);
        K.stall_hist_last = (K.stall_hist_last + 1) % 16;
        K.stall_hist_a[K.stall_hist_last] = from;
        K.stall_hist_b[K.stall_hist_last] = until_ns;
    }
    if (K.stall_hook) K.stall_hook(added_from, until_ns);
}

void vtk_set_stall_hook(vtk_stall_hook_fn fn) { K.stall_hook = fn; }

void vtk_set_sleeper_locked(vtk_task_t *t) { K.sleeper = t; }

uint64_t vtk_stall_total_ns(void) { return K.stall_total; }

void vtk_inject_external(vtk_event_fn fn, void *arg) {
    ext_item_t *it = malloc(sizeof *it);
    if (!it) return;
    it->fn = fn;
    it->arg = arg;
    it->next = NULL;
    pthread_mutex_lock(&K.ext_m);
    if (K.ext_tail) K.ext_tail->next = it;
    else K.ext_head = it;
    K.ext_tail = it;
    pthread_cond_broadcast(&K.ext_cv);
    pthread_mutex_unlock(&K.ext_m);
}
