/*
 * OSAL implementation for the simulator: FreeRTOS semantics on the
 * virtual-time kernel (simulator/kernel/vtk) with memory from the device-sized pools
 * (simulator/memory). Every kernel object costs what it costs on the device: TCBs,
 * stacks, queue storage and timers come out of internal RAM; delays and
 * timeouts are quantized to the RTOS tick; kernel calls charge CPU time.
 */
#define _GNU_SOURCE
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ink/osal.h"
#include "ink/probe.h"
#include "sim.h"

#define MAX_HELD 8
#define TIMER_TASK_PRIO 22  /* ESP-IDF esp_timer task */
#define TIMER_TASK_STACK 3584

struct osal_mutex;

struct osal_task {
    vtk_task_t *vt;
    osal_task_fn fn;
    void *arg;
    char name[24];
    void *tcb_mem, *stack_mem;
    uint32_t stack_bytes;
    uint32_t flags;
    struct osal_mutex *held[MAX_HELD];
    int nheld;
    struct osal_mutex *waiting_on;
    const char *mem_tag;
    bool exited;
};

static osal_task_t *BY_VT[VTK_MAX_TASKS];
/* Sanitizers add redzones to every stack frame: host stack estimates mean nothing then. */
#if defined(__SANITIZE_ADDRESS__)
#define INK_HOST_STACK_INFLATED 1
#elif defined(__has_feature)
#if __has_feature(address_sanitizer)
#define INK_HOST_STACK_INFLATED 1
#endif
#endif
#ifndef INK_HOST_STACK_INFLATED
#define INK_HOST_STACK_INFLATED 0
#endif

static __thread osal_task_t *tls_task;
static const char *g_boot_tag;
static bool g_wall_set;
static int64_t g_wall_base_us; /* wall time at virtual time 0 */

/* ------------------------------------------------------------- helpers */

static void require_task(const char *fn) {
    if (vtk_in_isr()) osal_panic("%s called from ISR/event context", fn);
    if (!vtk_self()) osal_panic("%s called outside a task (before the scheduler started?)", fn);
}

static osal_task_t *task_of(const vtk_task_t *vt) {
    int i = vtk_task_index(vt);
    return (i >= 0 && i < VTK_MAX_TASKS) ? BY_VT[i] : NULL;
}

static uint64_t tick_ns(void) { return g_profile.tick_hz > 0 ? 1000000000ull / (uint64_t)g_profile.tick_hz : 0; }

/* Absolute deadline for a timeout: rounded up to the RTOS tick (FreeRTOS timeouts are tick-based). */
static uint64_t deadline_for(int64_t timeout_us) {
    if (timeout_us < 0) return VTK_TIME_INF;
    uint64_t now = vtk_now();
    if (timeout_us == 0) return now;
    uint64_t want = now + (uint64_t)timeout_us * 1000ull;
    uint64_t T = tick_ns();
    if (T) want = (want + T - 1) / T * T;
    return want;
}

static uint64_t remaining(uint64_t deadline) {
    if (deadline == VTK_TIME_INF) return VTK_TIME_INF;
    uint64_t now = vtk_now();
    return deadline > now ? deadline - now : 0;
}

static void charge_kernel_op(void) {
    if (!vtk_in_isr() && vtk_self()) vtk_consume_ns(g_profile.queue_op_ns);
}

void sim_charge_cycles(double cycles) {
    if (vtk_in_isr() || !vtk_self() || g_sim.cpu_mode == VTK_CPU_MEASURED) return;
    vtk_consume_ns(sim_cycles_ns(cycles));
}

void sim_charge_ns_always(uint64_t ns) {
    if (vtk_in_isr() || !vtk_self()) return;
    vtk_consume_ns(ns);
}

const char *sim_current_owner(void) {
    if (vtk_in_isr()) return "isr";
    osal_task_t *t = tls_task;
    if (t) return t->mem_tag ? t->mem_tag : t->name;
    return g_boot_tag ? g_boot_tag : "boot";
}

/* ---------------------------------------------------------------- time */

uint64_t osal_now_us(void) {
    /* esp_timer_get_time() is not free; charging it also lets busy-wait loops make progress. */
    if (!vtk_in_isr() && vtk_self() && g_profile.now_cost_ns) vtk_consume_ns(g_profile.now_cost_ns);
    return vtk_now() / 1000ull;
}

int64_t osal_wall_us(void) {
    if (!g_wall_set) return 0;
    return g_wall_base_us + (int64_t)(vtk_now() / 1000ull);
}

void osal_set_wall_us(int64_t unix_us) {
    g_wall_set = unix_us != 0;
    g_wall_base_us = unix_us - (int64_t)(vtk_now() / 1000ull);
}

/* -------------------------------------------------------------- memory */

static double heap_op_cycles(void) {
    static double c = -1;
    if (c < 0) {
        c = profile_cost("heap.op");
        if (c <= 0) c = 1500;
    }
    return c;
}

static double bytes_cycles(int pool, size_t n, double fraction) {
    const char *cls = pool == POOL_EXTERNAL ? "psram.copy_byte" : "sram.copy_byte";
    double per = profile_cost(cls);
    return per * (double)n * fraction;
}

void *osal_malloc(size_t size, uint32_t caps) {
    if (vtk_in_isr()) osal_panic("osal_malloc(%zu) from ISR context", size);
    sim_charge_cycles(heap_op_cycles());
    return pools_alloc(size, caps, sim_current_owner());
}

void *osal_calloc(size_t n, size_t size, uint32_t caps) {
    if (size && n > SIZE_MAX / size) return NULL;
    void *p = osal_malloc(n * size, caps);
    if (p) {
        memset(p, 0, n * size);
        sim_charge_cycles(bytes_cycles(pools_pool_of(p), n * size, 0.5)); /* write-only */
    }
    return p;
}

void *osal_realloc(void *ptr, size_t size, uint32_t caps) {
    if (vtk_in_isr()) osal_panic("osal_realloc from ISR context");
    if (ptr && !pools_owns(ptr)) osal_panic("osal_realloc of a pointer not from osal_malloc (%p)", ptr);
    size_t old = ptr ? pools_block_size(ptr) : 0;
    sim_charge_cycles(heap_op_cycles());
    void *n = pools_realloc(ptr, size, caps, sim_current_owner());
    if (n && ptr && n != ptr) sim_charge_cycles(bytes_cycles(pools_pool_of(n), old < size ? old : size, 1.0));
    return n;
}

void osal_free(void *ptr) {
    if (!ptr) return;
    if (vtk_in_isr()) osal_panic("osal_free from ISR context");
    if (!pools_owns(ptr)) osal_panic("osal_free of a pointer not from osal_malloc (%p): mixing libc malloc?", ptr);
    sim_charge_cycles(heap_op_cycles());
    pools_free(ptr);
}

void osal_mem_stats(uint32_t caps, osal_mem_stats_t *out) {
    if (caps & OSAL_MEM_EXTERNAL) {
        pools_stats(POOL_EXTERNAL, out);
        return;
    }
    /* internal and DMA requests are served from regular internal RAM, then the reserve pool (both
     * DMA|INTERNAL on the S31, pools_alloc), so their stats cover both, as heap_caps_get_*_size()
     * does for MALLOC_CAP_INTERNAL / MALLOC_CAP_DMA */
    osal_mem_stats_t r;
    pools_stats(POOL_INTERNAL, out);
    pools_stats(POOL_DMA, &r);
    out->total += r.total;
    out->free += r.free;
    out->min_free = pools_internal_total_min_free();
    if (r.largest_free > out->largest_free) out->largest_free = r.largest_free;
    out->alloc_count += r.alloc_count;
    out->failed_allocs += r.failed_allocs;
}

const char *osal_mem_tag(const char *tag) {
    osal_task_t *t = tls_task;
    const char *prev;
    if (t && !vtk_in_isr()) {
        prev = t->mem_tag;
        t->mem_tag = tag;
    } else {
        prev = g_boot_tag;
        g_boot_tag = tag;
    }
    return prev;
}

/* --------------------------------------------------------------- tasks */

static void trampoline(void *arg) {
    osal_task_t *t = arg;
    t->vt = vtk_self();
    BY_VT[vtk_task_index(t->vt)] = t;
    tls_task = t;
    t->fn(t->arg);
    osal_task_exit();
}

osal_err_t osal_task_create(const osal_task_cfg_t *cfg, osal_task_t **out) {
    if (vtk_in_isr()) osal_panic("osal_task_create from ISR context");
    if (out) *out = NULL;
    if (!cfg || !cfg->fn || cfg->stack_bytes < 512) return OSAL_INVALID;
    int prio = cfg->priority < OSAL_PRIO_IDLE ? OSAL_PRIO_IDLE : cfg->priority > OSAL_PRIO_MAX ? OSAL_PRIO_MAX : cfg->priority;
    int core = cfg->core;
    if (core >= g_profile.cores) core = -1;

    osal_task_t *t = sim_host_calloc(1, sizeof *t);
    if (!t) abort();
    sim_copy_name(t->name, sizeof t->name, cfg->name ? cfg->name : "task");
    t->fn = cfg->fn;
    t->arg = cfg->arg;
    t->stack_bytes = cfg->stack_bytes;
    t->flags = cfg->flags;

    /* FreeRTOS allocates the TCB from internal RAM and, by default, the stack too. */
    sim_charge_ns_always(SIM_US(30));
    t->tcb_mem = pools_alloc(g_profile.tcb_bytes, OSAL_MEM_INTERNAL, t->name);
    bool ext = (cfg->flags & OSAL_TASK_STACK_EXTERNAL) != 0;
    t->stack_mem = t->tcb_mem ? pools_alloc(cfg->stack_bytes, ext ? OSAL_MEM_EXTERNAL : OSAL_MEM_INTERNAL, t->name) : NULL;
    if (!t->tcb_mem || !t->stack_mem) {
        sim_warn("task '%s' not created: no %s memory for TCB+stack (%u bytes)", t->name, ext ? "external" : "internal",
                 cfg->stack_bytes + g_profile.tcb_bytes);
        if (t->tcb_mem) pools_free(t->tcb_mem);
        sim_host_free(t);
        return OSAL_NO_MEM;
    }
    uint32_t vflags = 0;
    vtk_task_t *vt = vtk_task_create(t->name, trampoline, t, prio, core, vflags, cfg->stack_bytes);
    if (!vt) {
        pools_free(t->stack_mem);
        pools_free(t->tcb_mem);
        sim_host_free(t);
        sim_warn("task '%s' not created: the simulator's task table is full (%d tasks created in this run; "
                 "exited tasks keep their slot) - a simulator limit, not a device one",
                 cfg->name ? cfg->name : "?", vtk_task_count());
        return OSAL_NO_MEM;
    }
    if (!t->vt) {
        t->vt = vt;
        BY_VT[vtk_task_index(vt)] = t;
    }
    if (out) *out = t;
    return OSAL_OK;
}

osal_task_t *osal_task_self(void) { return vtk_in_isr() ? NULL : tls_task; }
bool sim_task_stack_external(void) { return !vtk_in_isr() && tls_task && (tls_task->flags & OSAL_TASK_STACK_EXTERNAL); }
const char *osal_task_name(const osal_task_t *t) { return t ? t->name : "?"; }

void osal_task_delay_us(uint64_t us) {
    require_task("osal_task_delay_us");
    if (us == 0) {
        vtk_yield();
        return;
    }
    uint64_t now = vtk_now();
    uint64_t want = now + us * 1000ull;
    uint64_t T = tick_ns();
    if (T) want = (want + T - 1) / T * T; /* wake on the first tick at or after the requested time */
    vtk_sleep_ns(want - now);
}

void osal_task_yield(void) {
    require_task("osal_task_yield");
    vtk_yield();
}

void osal_task_exit(void) {
    require_task("osal_task_exit");
    osal_task_t *t = tls_task;
    if (t) {
        if (t->nheld) sim_warn("task '%s' exits while holding %d mutex(es)", t->name, t->nheld);
        t->exited = true;
        pools_free(t->stack_mem);
        pools_free(t->tcb_mem);
        t->stack_mem = t->tcb_mem = NULL;
    }
    vtk_task_exit();
}

/* ------------------------------------------------------- cost annotation */

typedef struct {
    const char *ptr;
    double cycles;
} cost_cache_t;
static cost_cache_t CC[64];
static int nCC;
static char *UNKNOWN[32];
static int nUNKNOWN;

static double cost_lookup(const char *cls) {
    for (int i = 0; i < nCC; i++)
        if (CC[i].ptr == cls) return CC[i].cycles;
    double c = profile_cost(cls);
    if (c <= 0) {
        bool seen = false;
        for (int i = 0; i < nUNKNOWN; i++)
            if (!strcmp(UNKNOWN[i], cls)) seen = true;
        if (!seen && nUNKNOWN < 32) {
            UNKNOWN[nUNKNOWN++] = strdup(cls);
            sim_warn("unknown cost class '%s' (add it to costs_cycles in the profile); charged 0", cls);
            sim_metric_add("cost.unknown_classes", 1);
        }
    }
    if (nCC < 64) CC[nCC++] = (cost_cache_t){cls, c};
    return c;
}

/* System code runs from flash through the I-cache: annotated costs are warm-cache figures
 * (ISS), so add the profile's miss penalty. */
void osal_cost_cycles(uint64_t cycles) { sim_charge_cycles((double)cycles * (1.0 + g_profile.icache_penalty)); }

void osal_cost_units(const char *cost_class, uint64_t units) {
    if (!units || vtk_in_isr() || !vtk_self() || g_sim.cpu_mode == VTK_CPU_MEASURED) return;
    sim_charge_cycles(cost_lookup(cost_class) * (double)units * (1.0 + g_profile.icache_penalty));
}

void osal_preempt_point(void) {
    if (!vtk_in_isr() && vtk_self()) vtk_sched_point();
}

/* -------------------------------------------------------------- queues */

struct osal_queue {
    vtk_waitq_t senders, receivers;
    uint8_t *storage; /* pool memory: queue control block + items */
    uint8_t *items;
    size_t item_size, length, head, count;
};

osal_queue_t *osal_queue_create(size_t item_size, size_t length) {
    if (vtk_in_isr()) osal_panic("osal_queue_create from ISR context");
    if (!item_size || !length) return NULL;
    osal_queue_t *q = sim_host_calloc(1, sizeof *q);
    if (!q) abort();
    size_t bytes = g_profile.sync_obj_bytes + item_size * length;
    q->storage = pools_alloc(bytes, OSAL_MEM_INTERNAL, sim_current_owner());
    if (!q->storage) {
        sim_host_free(q);
        return NULL;
    }
    q->items = q->storage + g_profile.sync_obj_bytes;
    q->item_size = item_size;
    q->length = length;
    return q;
}

void osal_queue_delete(osal_queue_t *q) {
    if (!q) return;
    if (q->senders.head || q->receivers.head) osal_panic("osal_queue_delete with blocked tasks");
    pools_free(q->storage);
    sim_host_free(q);
}

static osal_err_t queue_send(osal_queue_t *q, const void *item, int64_t timeout_us, bool front) {
    require_task("osal_queue_send");
    charge_kernel_op();
    vtk_lock();
    uint64_t deadline = deadline_for(timeout_us);
    for (;;) {
        if (q->count < q->length) {
            size_t slot;
            if (front) {
                q->head = (q->head + q->length - 1) % q->length;
                slot = q->head;
            } else {
                slot = (q->head + q->count) % q->length;
            }
            memcpy(q->items + slot * q->item_size, item, q->item_size);
            q->count++;
            vtk_wake_one_locked(&q->receivers);
            vtk_unlock();
            return OSAL_OK;
        }
        uint64_t rem = remaining(deadline);
        if (rem == 0) {
            vtk_unlock();
            return OSAL_FULL;
        }
        vtk_block_locked(&q->senders, rem);
    }
}

osal_err_t osal_queue_send(osal_queue_t *q, const void *item, int64_t timeout_us) {
    return queue_send(q, item, timeout_us, false);
}

osal_err_t osal_queue_send_front(osal_queue_t *q, const void *item, int64_t timeout_us) {
    return queue_send(q, item, timeout_us, true);
}

osal_err_t osal_queue_recv(osal_queue_t *q, void *item, int64_t timeout_us) {
    require_task("osal_queue_recv");
    charge_kernel_op();
    vtk_lock();
    uint64_t deadline = deadline_for(timeout_us);
    for (;;) {
        if (q->count > 0) {
            memcpy(item, q->items + q->head * q->item_size, q->item_size);
            q->head = (q->head + 1) % q->length;
            q->count--;
            vtk_wake_one_locked(&q->senders);
            vtk_unlock();
            return OSAL_OK;
        }
        uint64_t rem = remaining(deadline);
        if (rem == 0) {
            vtk_unlock();
            return timeout_us == 0 ? OSAL_EMPTY : OSAL_TIMEOUT;
        }
        vtk_block_locked(&q->receivers, rem);
    }
}

size_t osal_queue_count(osal_queue_t *q) { return q ? q->count : 0; }

/* --------------------------------------------------------------- mutex */

struct osal_mutex {
    vtk_waitq_t wq;
    vtk_task_t *owner;
    void *mem;
};

osal_mutex_t *osal_mutex_create(void) {
    if (vtk_in_isr()) osal_panic("osal_mutex_create from ISR context");
    osal_mutex_t *m = sim_host_calloc(1, sizeof *m);
    if (!m) abort();
    m->mem = pools_alloc(g_profile.sync_obj_bytes, OSAL_MEM_INTERNAL, sim_current_owner());
    if (!m->mem) {
        sim_host_free(m);
        return NULL;
    }
    return m;
}

void osal_mutex_delete(osal_mutex_t *m) {
    if (!m) return;
    if (m->owner || m->wq.head) osal_panic("osal_mutex_delete of a held mutex");
    pools_free(m->mem);
    sim_host_free(m);
}

static void hold_add(vtk_task_t *vt, osal_mutex_t *m) {
    osal_task_t *t = task_of(vt);
    if (!t) return;
    if (t->nheld >= MAX_HELD) osal_panic("task '%s' holds more than %d mutexes", t->name, MAX_HELD);
    t->held[t->nheld++] = m;
}

static void hold_remove(vtk_task_t *vt, osal_mutex_t *m) {
    osal_task_t *t = task_of(vt);
    if (!t) return;
    for (int i = 0; i < t->nheld; i++)
        if (t->held[i] == m) {
            t->held[i] = t->held[--t->nheld];
            return;
        }
}

static vtk_task_t *next_owner(vtk_task_t *o) {
    osal_task_t *ot = task_of(o);
    return (ot && ot->waiting_on) ? ot->waiting_on->owner : NULL;
}

/* A simulator check (FreeRTOS has none): the chain is only formatted when it is a deadlock. */
static void check_deadlock_locked(osal_mutex_t *m, vtk_task_t *self) {
    int depth = 0;
    vtk_task_t *o = m->owner;
    while (o && o != self && ++depth < 16) o = next_owner(o);
    if (o != self) return;
    char chain[400];
    int n = snprintf(chain, sizeof chain, "%s", vtk_task_name(self));
    o = m->owner;
    for (int d = 0; d <= depth && (size_t)n < sizeof chain; d++, o = next_owner(o))
        n += snprintf(chain + n, sizeof chain - (size_t)n, " -> %s", vtk_task_name(o));
    osal_panic("deadlock: %s", chain);
}

/* FreeRTOS xTaskPriorityInherit: raise the holder to the waiter's priority. Only the
 * direct holder is raised (no chain through further mutexes). */
static void inherit_locked(osal_mutex_t *m, int prio) {
    vtk_task_t *o = m->owner;
    if (o && vtk_priority(o) < prio) vtk_set_priority(o, prio);
}

/* FreeRTOS vTaskPriorityDisinheritAfterTimeout: a waiter gave up. The holder drops to
 * max(base, remaining waiters of this mutex) only if it holds no other mutex. */
static void disinherit_after_timeout_locked(osal_mutex_t *m) {
    vtk_task_t *o = m->owner;
    osal_task_t *ot = task_of(o);
    if (!o || !ot || ot->nheld != 1) return;
    int p = vtk_base_priority(o);
    int w = vtk_waitq_max_priority(&m->wq);
    if (w > p) p = w;
    if (p != vtk_priority(o)) vtk_set_priority(o, p);
}

osal_err_t osal_mutex_lock(osal_mutex_t *m, int64_t timeout_us) {
    require_task("osal_mutex_lock");
    vtk_task_t *self = vtk_self();
    charge_kernel_op();
    vtk_lock();
    uint64_t deadline = deadline_for(timeout_us);
    for (;;) {
        if (!m->owner) {
            m->owner = self;
            hold_add(self, m);
            vtk_unlock();
            return OSAL_OK;
        }
        if (m->owner == self) osal_panic("mutex locked twice by '%s' (not recursive)", vtk_task_name(self));
        uint64_t rem = remaining(deadline);
        if (rem == 0) {
            vtk_unlock();
            return OSAL_TIMEOUT;
        }
        check_deadlock_locked(m, self);
        inherit_locked(m, vtk_priority(self));
        osal_task_t *st = task_of(self);
        if (st) st->waiting_on = m;
        bool woke = vtk_block_locked(&m->wq, rem);
        if (st) st->waiting_on = NULL;
        /* no hand-over: a woken waiter competes for the mutex again (FreeRTOS semantics) */
        if (!woke && m->owner) disinherit_after_timeout_locked(m);
    }
}

void osal_mutex_unlock(osal_mutex_t *m) {
    require_task("osal_mutex_unlock");
    vtk_task_t *self = vtk_self();
    charge_kernel_op();
    vtk_lock();
    if (m->owner != self)
        osal_panic("mutex unlocked by '%s' but owned by '%s'", vtk_task_name(self),
                   m->owner ? vtk_task_name(m->owner) : "nobody");
    hold_remove(self, m);
    m->owner = NULL;
    /* xTaskPriorityDisinherit: an inherited priority is kept until the task holds no mutex */
    osal_task_t *st = task_of(self);
    if (st && st->nheld == 0 && vtk_priority(self) != vtk_base_priority(self))
        vtk_set_priority(self, vtk_base_priority(self));
    vtk_wake_one_locked(&m->wq);
    vtk_unlock();
}

/* ----------------------------------------------------------- semaphore */

struct osal_sem {
    vtk_waitq_t wq;
    uint32_t count, max;
    void *mem;
};

osal_sem_t *osal_sem_create(uint32_t initial, uint32_t max) {
    if (vtk_in_isr()) osal_panic("osal_sem_create from ISR context");
    if (!max || initial > max) return NULL;
    osal_sem_t *s = sim_host_calloc(1, sizeof *s);
    if (!s) abort();
    s->mem = pools_alloc(g_profile.sync_obj_bytes, OSAL_MEM_INTERNAL, sim_current_owner());
    if (!s->mem) {
        sim_host_free(s);
        return NULL;
    }
    s->count = initial;
    s->max = max;
    return s;
}

void osal_sem_delete(osal_sem_t *s) {
    if (!s) return;
    if (s->wq.head) osal_panic("osal_sem_delete with blocked tasks");
    pools_free(s->mem);
    sim_host_free(s);
}

osal_err_t osal_sem_take(osal_sem_t *s, int64_t timeout_us) {
    require_task("osal_sem_take");
    charge_kernel_op();
    vtk_lock();
    uint64_t deadline = deadline_for(timeout_us);
    for (;;) {
        if (s->count > 0) {
            s->count--;
            vtk_unlock();
            return OSAL_OK;
        }
        uint64_t rem = remaining(deadline);
        if (rem == 0) {
            vtk_unlock();
            return OSAL_TIMEOUT;
        }
        vtk_block_locked(&s->wq, rem);
    }
}

void osal_sem_give(osal_sem_t *s) {
    require_task("osal_sem_give");
    charge_kernel_op();
    vtk_lock();
    if (s->count < s->max) s->count++;
    vtk_wake_one_locked(&s->wq);
    vtk_unlock();
}

/* -------------------------------------------------------------- timers */

struct osal_timer {
    char name[24];
    osal_timer_fn fn;
    void *arg;
    void *mem;
    uint64_t period_ns, due;
    vtk_event_id_t ev;
    bool active, pending;
    bool deleting;    /* osal_timer_delete is under way: start refuses (a callback cannot re-arm it) */
    uint32_t backlog; /* firings not yet dispatched (esp_timer catches up on missed periods) */
    struct osal_timer *next_pending;
};

static struct {
    osal_task_t *task;
    vtk_waitq_t wq, done_wq;
    osal_timer_t *head, *tail;
    osal_timer_t *running; /* its callback is running in the timer task */
} TS;

static void timer_fire(void *arg) { /* ISR context */
    osal_timer_t *t = arg;
    t->ev = 0;
    if (!t->active) return;
    t->backlog++;
    if (t->pending) {
        sim_metric_add("timer.late_periods", 1); /* still waiting for the timer task: runs back to back later */
    } else {
        t->pending = true;
        t->next_pending = NULL;
        if (TS.tail) TS.tail->next_pending = t;
        else TS.head = t;
        TS.tail = t;
        vtk_isr_wake_one(&TS.wq);
    }
    if (t->period_ns) {
        t->due += t->period_ns;
        t->ev = vtk_event_at(t->due, timer_fire, t);
    } else {
        t->active = false;
    }
}

static void unlink_pending(osal_timer_t *t) {
    osal_timer_t **pp = &TS.head, *prev = NULL;
    while (*pp && *pp != t) {
        prev = *pp;
        pp = &(*pp)->next_pending;
    }
    if (!*pp) {
        t->backlog = 0;
        return;
    }
    *pp = t->next_pending;
    if (TS.tail == t) TS.tail = prev;
    t->pending = false;
    t->backlog = 0;
}

static void timer_task(void *arg) {
    (void)arg;
    for (;;) {
        vtk_lock();
        while (!TS.head) vtk_block_locked(&TS.wq, VTK_TIME_INF);
        osal_timer_t *t = TS.head;
        TS.head = t->next_pending;
        if (!TS.head) TS.tail = NULL;
        t->pending = false;
        if (t->backlog) t->backlog--;
        if (t->backlog) { /* more periods are due: queue it again (after the others) */
            t->pending = true;
            t->next_pending = NULL;
            if (TS.tail) TS.tail->next_pending = t;
            else TS.head = t;
            TS.tail = t;
        }
        TS.running = t;
        vtk_unlock();
        sim_charge_ns_always(SIM_US(4)); /* esp_timer dispatch overhead */
        t->fn(t->arg); /* may delete t: not touched after this */
        vtk_lock();
        TS.running = NULL;
        vtk_wake_all_locked(&TS.done_wq);
        vtk_unlock();
    }
}

osal_timer_t *osal_timer_create(const char *name, osal_timer_fn fn, void *arg) {
    if (vtk_in_isr()) osal_panic("osal_timer_create from ISR context");
    if (!fn) return NULL;
    osal_timer_t *t = sim_host_calloc(1, sizeof *t);
    if (!t) abort();
    t->mem = pools_alloc(g_profile.sync_obj_bytes, OSAL_MEM_INTERNAL, sim_current_owner());
    if (!t->mem) {
        sim_host_free(t);
        return NULL;
    }
    sim_copy_name(t->name, sizeof t->name, name ? name : "timer");
    t->fn = fn;
    t->arg = arg;
    return t;
}

osal_err_t osal_timer_start(osal_timer_t *t, uint64_t delay_us, uint64_t period_us) {
    require_task("osal_timer_start");
    charge_kernel_op();
    vtk_lock();
    if (t->deleting) { /* its callback re-arming it while another task deletes it */
        vtk_unlock();
        return OSAL_INVALID;
    }
    if (t->ev) vtk_event_cancel(t->ev);
    unlink_pending(t);
    t->period_ns = period_us * 1000ull;
    t->due = vtk_now() + delay_us * 1000ull; /* esp_timer: microsecond resolution, no tick rounding */
    t->active = true;
    t->ev = vtk_event_at(t->due, timer_fire, t);
    vtk_unlock();
    return OSAL_OK;
}

osal_err_t osal_timer_stop(osal_timer_t *t) {
    require_task("osal_timer_stop");
    charge_kernel_op();
    vtk_lock();
    if (t->ev) vtk_event_cancel(t->ev);
    t->ev = 0;
    t->active = false;
    unlink_pending(t);
    vtk_unlock();
    return OSAL_OK;
}

static void disarm_locked(osal_timer_t *t) {
    if (t->ev) vtk_event_cancel(t->ev);
    t->ev = 0;
    t->active = false;
    unlink_pending(t);
}

void osal_timer_delete(osal_timer_t *t) {
    if (!t) return;
    bool task = vtk_self() && !vtk_in_isr();
    if (task) {
        charge_kernel_op();
        vtk_lock();
    }
    t->deleting = true; /* from here osal_timer_start refuses it */
    disarm_locked(t);
    if (task && osal_task_self() != TS.task) { /* from the timer task it cannot be running anyway */
        while (TS.running == t) vtk_block_locked(&TS.done_wq, VTK_TIME_INF);
        disarm_locked(t); /* belt and braces: nothing can have armed it meanwhile */
    }
    if (task) vtk_unlock();
    pools_free(t->mem);
    sim_host_free(t);
}

/* ---------------------------------------------------------------- misc */

typedef struct {
    const char *fmt;
    va_list *ap;
} panic_t;

static void panic_on_host(void *p) {
    const panic_t *a = p;
    char msg[512];
    vsnprintf(msg, sizeof msg, a->fmt, *a->ap);
    const char *who = vtk_in_isr() ? "isr" : (vtk_self() ? vtk_task_name(vtk_self()) : "main");
    fprintf(stderr, "PANIC in %s at %.6f s: %s\n", who, (double)vtk_now() / 1e9, msg);
    sim_metric_add("panic.count", 1);
    sim_warn("PANIC in %s: %s", who, msg);
    sim_fatal("panic");
}

void osal_panic(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    panic_t a = {fmt, &ap};
    vtk_host_call(panic_on_host, &a); /* the final report runs here too: not the panicking task's stack */
    va_end(ap);
    abort(); /* not reached: sim_fatal exits */
}

/* --------------------------------------------------------------- probes */

typedef struct {
    const char *metric;
    double value;
} probe_t;

static void probe_value_on_host(void *p) {
    const probe_t *pr = p;
    sim_metric_sample(pr->metric, pr->value);
    if (sim_trace_enabled()) {
        char args[96];
        snprintf(args, sizeof args, "{\"value\":%.3f}", pr->value);
        sim_trace_instant("probes", pr->metric, vtk_now(), args);
    }
}

void ink_probe_value(const char *metric, double value) {
    probe_t pr = {metric, value};
    vtk_host_call(probe_value_on_host, &pr);
}

void ink_probe_count(const char *metric) { sim_metric_add(metric, 1); }

void ink_probe_mark(const char *name) { sim_trace_instant("marks", name, vtk_now(), NULL); }

/* ---------------------------------------------------------- init/report */

void sim_osal_init(void) {
    if (g_sim.wall_start_us) osal_set_wall_us(g_sim.wall_start_us);
    osal_task_cfg_t cfg = {.name = "esp_timer", .fn = timer_task, .stack_bytes = TIMER_TASK_STACK,
                           .priority = TIMER_TASK_PRIO, .core = 0};
    if (osal_task_create(&cfg, &TS.task) != OSAL_OK) {
        fprintf(stderr, "cannot create timer service task (internal RAM too small?)\n");
        exit(3);
    }
}

void sim_osal_report(void) {
    char key[96];
    uint64_t total = vtk_now();
    for (int i = 0; i < vtk_task_count(); i++) {
        vtk_task_info_t ti;
        vtk_task_info(i, &ti);
        osal_task_t *t = BY_VT[i];
        if (ti.name[0] == '(') continue; /* simulator-internal */
        snprintf(key, sizeof key, "task.%s.cpu_ms", ti.name);
        sim_metric_set(key, ti.cpu_ns / 1e6);
        snprintf(key, sizeof key, "task.%s.max_burst_ms", ti.name);
        sim_metric_set(key, ti.max_burst_ns / 1e6);
        snprintf(key, sizeof key, "task.%s.ready_delay_max_ms", ti.name);
        sim_metric_set(key, ti.preempt_delay_max_ns / 1e6);
        if (ti.stall_ns) {
            snprintf(key, sizeof key, "task.%s.flash_stall_ms", ti.name);
            sim_metric_set(key, ti.stall_ns / 1e6);
        }
        snprintf(key, sizeof key, "task.%s.stack_bytes", ti.name);
        sim_metric_set(key, ti.stack_target_bytes);
        snprintf(key, sizeof key, "task.%s.stack_used_est_bytes", ti.name);
        sim_metric_set(key, ti.stack_used_est_bytes);
        if (ti.stack_target_bytes && ti.stack_used_est_bytes > ti.stack_target_bytes && !INK_HOST_STACK_INFLATED)
            sim_warn("task '%s' may overflow its %u-byte stack (pessimistic host-based estimate %u bytes; "
                     "verify with -fstack-usage on the RISC-V build)",
                     ti.name, ti.stack_target_bytes, ti.stack_used_est_bytes);
        (void)t;
    }
    for (int c = 0; c < g_profile.cores; c++) {
        snprintf(key, sizeof key, "cpu.core%d.busy_pct", c);
        sim_metric_set(key, total ? 100.0 * (double)vtk_core_busy_ns(c) / (double)total : 0);
    }
    sim_metric_set("flash.cpu_stall_total_ms", vtk_stall_total_ns() / 1e6);

    static const char *PN[POOL_COUNT] = {"internal", "dma", "external"};
    for (int p = 0; p < POOL_COUNT; p++) {
        osal_mem_stats_t st;
        pools_sample();
        pools_stats((pool_id_t)p, &st);
        snprintf(key, sizeof key, "mem.%s.total_bytes", PN[p]);
        sim_metric_set(key, (double)st.total);
        snprintf(key, sizeof key, "mem.%s.free_bytes", PN[p]);
        sim_metric_set(key, (double)st.free);
        snprintf(key, sizeof key, "mem.%s.min_free_bytes", PN[p]);
        sim_metric_set(key, (double)st.min_free);
        snprintf(key, sizeof key, "mem.%s.largest_free_min_bytes", PN[p]);
        sim_metric_set(key, (double)pools_min_largest((pool_id_t)p));
        snprintf(key, sizeof key, "mem.%s.failed_allocs", PN[p]);
        sim_metric_set(key, (double)st.failed_allocs);
    }
    sim_metric_set("mem.internal_total.min_free_bytes", (double)pools_internal_total_min_free());
    sim_metric_set("mem.internal_total.largest_free_min_bytes", (double)pools_internal_total_min_largest());
    for (int i = 0; i < pools_tag_count(); i++) {
        const char *tag;
        size_t cur[POOL_COUNT], peak[POOL_COUNT], live;
        pools_tag_info(i, &tag, cur, peak, &live);
        for (int p = 0; p < POOL_COUNT; p++) {
            if (!peak[p]) continue;
            snprintf(key, sizeof key, "mem.tag.%s.%s.peak_bytes", tag, PN[p]);
            sim_metric_set(key, (double)peak[p]);
            snprintf(key, sizeof key, "mem.tag.%s.%s.live_bytes", tag, PN[p]);
            sim_metric_set(key, (double)cur[p]);
        }
    }
}
