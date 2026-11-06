/*
 * Resource management (see ink/sys/res.h): per-component memory accounting
 * on top of the platform heaps (heap_caps on the device, the simulator's
 * pools here), pressure levels with reclaimers, and the background job
 * runner.
 */
#include <stdio.h>
#include <string.h>

#include "ink/osal.h"
#include "ink/probe.h"
#include "ink/sys/exec.h"
#include "ink/sys/res.h"
#include "ink/sys/state.h"

#define COMP_MAX 24
#define RECLAIM_MAX 8
#define LISTEN_MAX 6
#define JOBS_MAX 16
#define LARGE_BLOCK (16u * 1024u) /* admitted before it is taken */
#define CHECK_BLOCK (4u * 1024u)  /* re-evaluates the pressure */
#define HDR 16                    /* keeps the payload 16-byte aligned */

/* Free bytes below which a domain is tight / low / critical (estimates; calibrate on the device). */
static const size_t LIMIT[INK_MEM_DOMAINS][3] = {
    [INK_MEM_INTERNAL] = {24u * 1024u, 12u * 1024u, 6u * 1024u},
    [INK_MEM_DMA] = {8u * 1024u, 4u * 1024u, 2u * 1024u},
    [INK_MEM_PSRAM] = {2u * 1024u * 1024u, 1024u * 1024u, 256u * 1024u},
};
static const uint32_t CAPS[INK_MEM_DOMAINS] = {OSAL_MEM_INTERNAL, OSAL_MEM_DMA, OSAL_MEM_EXTERNAL};
static const char *const PRESSURE_NAME[] = {"normal", "tight", "low", "critical"};

typedef struct {
    const char *name;
    size_t quota[INK_MEM_DOMAINS]; /* 0 = none declared */
    size_t used[INK_MEM_DOMAINS];
} comp_t;

typedef struct {
    uint16_t comp;
    uint8_t domain;
    uint32_t size;
} hdr_t;

typedef struct {
    ink_job_t j;
    int id;
    bool used, cancelled;
} jslot_t;

static struct {
    osal_mutex_t *lock;
    comp_t comp[COMP_MAX];
    int ncomp;
    struct {
        const char *name;
        int prio;
        size_t (*fn)(void *, size_t);
        void *ctx;
    } rec[RECLAIM_MAX];
    int nrec;
    struct {
        void (*fn)(void *, ink_pressure_t, ink_pressure_t);
        void *ctx;
    } lis[LISTEN_MAX];
    int nlis;
    ink_pressure_t level;
    int checking;
    jslot_t *jobs; /* JOBS_MAX, PSRAM */
    int next_id, cursor, holds;
    ink_exec_job_t tick_job;
    int h_online;
} R INK_PSRAM_BSS;

const char *ink_pressure_name(ink_pressure_t p) { return (unsigned)p < 4 ? PRESSURE_NAME[p] : "?"; }

static void job_kick(void);
static void on_online(void *ctx, int h, const char *id) {
    (void)ctx;
    (void)h;
    (void)id;
    job_kick();
}

int ink_res_init(void) {
    if (R.lock) return 0;
    const char *prev = osal_mem_tag("res");
    R.lock = osal_mutex_create();
    R.jobs = osal_calloc(JOBS_MAX, sizeof(jslot_t), OSAL_MEM_EXTERNAL);
    osal_mem_tag(prev);
    R.h_online = -1; /* net.online is looked up (and watched) with the first job: the network service starts later */
    return R.lock && R.jobs ? 0 : -1;
}

/* ------------------------------------------------------------ accounting */

static int comp_index(const char *name) { /* under the lock */
    for (int i = 0; i < R.ncomp; i++)
        if (R.comp[i].name == name || !strcmp(R.comp[i].name, name)) return i;
    if (R.ncomp >= COMP_MAX) return -1;
    R.comp[R.ncomp] = (comp_t){.name = name};
    return R.ncomp++;
}

int ink_mem_budget(const char *component, ink_mem_domain_t d, size_t bytes) {
    if (!R.lock || (unsigned)d >= INK_MEM_DOMAINS) return -1;
    osal_mutex_lock(R.lock, OSAL_WAIT_FOREVER);
    int i = comp_index(component);
    if (i >= 0) R.comp[i].quota[d] = bytes;
    osal_mutex_unlock(R.lock);
    return i >= 0 ? 0 : -1;
}

size_t ink_mem_used(const char *component, ink_mem_domain_t d) {
    if (!R.lock || (unsigned)d >= INK_MEM_DOMAINS) return 0;
    osal_mutex_lock(R.lock, OSAL_WAIT_FOREVER);
    size_t n = 0;
    for (int i = 0; i < R.ncomp; i++)
        if (!strcmp(R.comp[i].name, component)) n = R.comp[i].used[d];
    osal_mutex_unlock(R.lock);
    return n;
}

static size_t free_bytes(ink_mem_domain_t d, size_t *largest) {
    osal_mem_stats_t st;
    osal_mem_stats(CAPS[d], &st);
    if (largest) *largest = st.largest_free;
    return st.free;
}

bool ink_mem_admit(ink_mem_domain_t d, size_t n) {
    if ((unsigned)d >= INK_MEM_DOMAINS) return false;
    size_t largest, fr = free_bytes(d, &largest);
    bool ok = largest >= n && fr >= n && fr - n >= LIMIT[d][0];
    if (!ok) ink_probe_count("mem.not_admitted");
    return ok;
}

void *ink_mem_alloc(const char *component, ink_mem_domain_t d, size_t n) {
    if (!R.lock || (unsigned)d >= INK_MEM_DOMAINS || !n) return NULL;
    osal_mutex_lock(R.lock, OSAL_WAIT_FOREVER);
    int i = comp_index(component);
    bool over = i < 0 || (R.comp[i].quota[d] && R.comp[i].used[d] + n > R.comp[i].quota[d]);
    if (!over) R.comp[i].used[d] += n; /* reserved before the heap is asked */
    osal_mutex_unlock(R.lock);
    if (over) {
        ink_probe_count("mem.quota_refused");
        return NULL;
    }
    uint8_t *p = n >= LARGE_BLOCK && !ink_mem_admit(d, n) ? NULL : osal_malloc(n + HDR, CAPS[d]);
    if (!p) {
        osal_mutex_lock(R.lock, OSAL_WAIT_FOREVER);
        R.comp[i].used[d] -= n;
        osal_mutex_unlock(R.lock);
        return NULL;
    }
    *(hdr_t *)p = (hdr_t){.comp = (uint16_t)i, .domain = (uint8_t)d, .size = (uint32_t)n};
    if (n >= CHECK_BLOCK) ink_mem_check();
    return p + HDR;
}

void ink_mem_free(void *ptr) {
    if (!ptr) return;
    uint8_t *p = (uint8_t *)ptr - HDR;
    hdr_t h = *(hdr_t *)p;
    osal_mutex_lock(R.lock, OSAL_WAIT_FOREVER);
    R.comp[h.comp].used[h.domain] -= h.size;
    osal_mutex_unlock(R.lock);
    osal_free(p);
    if (h.size >= CHECK_BLOCK) ink_mem_check();
}

/* ------------------------------------------------------------ pressure */

int ink_mem_reclaimer(const char *name, int prio, size_t (*reclaim)(void *ctx, size_t want), void *ctx) {
    if (!R.lock || !reclaim) return -1;
    osal_mutex_lock(R.lock, OSAL_WAIT_FOREVER);
    int r = -1;
    if (R.nrec < RECLAIM_MAX) {
        int k = R.nrec++;
        while (k > 0 && R.rec[k - 1].prio > prio) { /* sorted: lowest prio asked first */
            R.rec[k] = R.rec[k - 1];
            k--;
        }
        R.rec[k].name = name;
        R.rec[k].prio = prio;
        R.rec[k].fn = reclaim;
        R.rec[k].ctx = ctx;
        r = 0;
    }
    osal_mutex_unlock(R.lock);
    return r;
}

int ink_mem_on_pressure(void (*fn)(void *ctx, ink_pressure_t from, ink_pressure_t to), void *ctx) {
    if (!R.lock || !fn || R.nlis >= LISTEN_MAX) return -1;
    R.lis[R.nlis].fn = fn;
    R.lis[R.nlis].ctx = ctx;
    R.nlis++;
    return 0;
}

ink_pressure_t ink_mem_pressure(void) { return __atomic_load_n(&R.level, __ATOMIC_ACQUIRE); }

/* The worst domain's level, and how many bytes would bring it back to "normal". */
static ink_pressure_t measure(size_t *short_by) {
    ink_pressure_t worst = INK_PRESSURE_NORMAL;
    *short_by = 0;
    for (int d = 0; d < INK_MEM_DOMAINS; d++) {
        size_t fr = free_bytes((ink_mem_domain_t)d, NULL);
        ink_pressure_t lv = fr < LIMIT[d][2] ? INK_PRESSURE_CRITICAL : fr < LIMIT[d][1] ? INK_PRESSURE_LOW : fr < LIMIT[d][0] ? INK_PRESSURE_TIGHT : INK_PRESSURE_NORMAL;
        if (lv > worst) worst = lv;
        if (lv != INK_PRESSURE_NORMAL && LIMIT[d][0] - fr > *short_by) *short_by = LIMIT[d][0] - fr;
    }
    return worst;
}

ink_pressure_t ink_mem_check(void) {
    if (!R.lock) return INK_PRESSURE_NORMAL;
    int idle = 0;
    if (!__atomic_compare_exchange_n(&R.checking, &idle, 1, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) return ink_mem_pressure();
    size_t short_by;
    ink_pressure_t lv = measure(&short_by);
    if (lv != INK_PRESSURE_NORMAL) { /* give memory back first, cheapest caches first */
        size_t got = 0;
        for (int i = 0; i < R.nrec && got < short_by; i++) {
            size_t n = R.rec[i].fn(R.rec[i].ctx, short_by - got);
            got += n;
            if (n) ink_probe_count("mem.reclaimed");
        }
        if (got) lv = measure(&short_by);
    }
    ink_pressure_t from = __atomic_exchange_n(&R.level, lv, __ATOMIC_ACQ_REL);
    __atomic_store_n(&R.checking, 0, __ATOMIC_RELEASE);
    if (from != lv) {
        char key[40];
        snprintf(key, sizeof key, "mem.pressure.%s", PRESSURE_NAME[lv]);
        ink_probe_count(key);
        for (int i = 0; i < R.nlis; i++) R.lis[i].fn(R.lis[i].ctx, from, lv);
        if (lv < INK_PRESSURE_LOW) job_kick(); /* paused jobs go on */
    }
    return lv;
}

/* ------------------------------------------------------------ jobs */

static bool online(void) {
    int64_t v = 1;
    if (R.h_online < 0 && (R.h_online = ink_state_find("net.online")) >= 0) ink_state_watch("net.online", on_online, NULL, INK_EXEC_DIRECT);
    return R.h_online < 0 || (ink_state_get_int(R.h_online, &v) && v);
}

static bool may_run(const jslot_t *s) { return s->used && !s->cancelled && (!s->j.needs_net || online()); }

static void job_tick(void *arg);

static void job_kick(void) {
    if (!R.lock) return;
    osal_mutex_lock(R.lock, OSAL_WAIT_FOREVER);
    bool any = false;
    for (int i = 0; i < JOBS_MAX && !any; i++) any = R.jobs[i].used;
    osal_mutex_unlock(R.lock);
    if (any) ink_exec_post(INK_EXEC_BG, &R.tick_job, job_tick, NULL); /* no jobs, no executor: exec.bg is created on its first real job */
}

static void finish_slot(jslot_t *s, ink_job_step_t how, bool cancelled) {
    ink_job_t j = s->j;
    osal_mutex_lock(R.lock, OSAL_WAIT_FOREVER);
    s->used = false;
    osal_mutex_unlock(R.lock);
    ink_probe_count(cancelled ? "job.cancelled" : how == INK_JOB_DONE ? "job.done" : "job.failed");
    if (j.finish) j.finish(j.ctx, how, cancelled);
}

/* exec.bg: one step of one job (round robin), then again while there is work and nothing holds it. */
static void job_tick(void *arg) {
    (void)arg;
    osal_mutex_lock(R.lock, OSAL_WAIT_FOREVER);
    bool held = R.holds > 0 || ink_mem_pressure() >= INK_PRESSURE_LOW;
    jslot_t *s = NULL, *cancelled = NULL;
    for (int k = 0; k < JOBS_MAX && !s; k++) {
        jslot_t *c = &R.jobs[(R.cursor + k) % JOBS_MAX];
        if (c->used && c->cancelled) cancelled = c;
        else if (!held && may_run(c)) {
            s = c;
            R.cursor = (int)(c - R.jobs + 1) % JOBS_MAX;
        }
    }
    osal_mutex_unlock(R.lock);
    if (cancelled) {
        finish_slot(cancelled, INK_JOB_FAILED, true);
        job_kick();
        return;
    }
    if (!s) return; /* nothing runnable: a kick (hold released, pressure eased, online) starts it again */
    uint64_t t0 = osal_now_us();
    ink_job_step_t r = s->j.step(s->j.ctx);
    uint64_t took = osal_now_us() - t0, budget = (uint64_t)(s->j.step_budget_ms ? s->j.step_budget_ms : 20) * 1000u;
    if (took > budget) ink_probe_count("job.step_overrun");
    if (r != INK_JOB_MORE) finish_slot(s, r, false);
    job_kick();
}

int ink_job_submit(const ink_job_t *job) {
    if (!R.lock || !job || !job->step) return -1;
    if (job->psram_bytes && !ink_mem_admit(INK_MEM_PSRAM, job->psram_bytes)) return -1;
    osal_mutex_lock(R.lock, OSAL_WAIT_FOREVER);
    int id = -1;
    for (int i = 0; i < JOBS_MAX; i++)
        if (!R.jobs[i].used) {
            R.jobs[i] = (jslot_t){.j = *job, .id = ++R.next_id, .used = true};
            id = R.jobs[i].id;
            break;
        }
    osal_mutex_unlock(R.lock);
    if (id < 0) ink_probe_count("job.queue_full");
    else job_kick();
    return id;
}

void ink_job_cancel(int id) {
    if (!R.lock) return;
    osal_mutex_lock(R.lock, OSAL_WAIT_FOREVER);
    for (int i = 0; i < JOBS_MAX; i++)
        if (R.jobs[i].used && R.jobs[i].id == id) R.jobs[i].cancelled = true;
    osal_mutex_unlock(R.lock);
    job_kick();
}

void ink_jobs_hold(bool hold) {
    if (!R.lock) return;
    osal_mutex_lock(R.lock, OSAL_WAIT_FOREVER);
    R.holds += hold ? 1 : -1;
    bool resume = R.holds == 0;
    osal_mutex_unlock(R.lock);
    if (resume) job_kick();
}

bool ink_jobs_held(void) {
    if (!R.lock) return false;
    osal_mutex_lock(R.lock, OSAL_WAIT_FOREVER);
    bool held = R.holds > 0;
    osal_mutex_unlock(R.lock);
    return held || ink_mem_pressure() >= INK_PRESSURE_LOW;
}

int ink_jobs_pending(void) {
    if (!R.lock) return 0;
    int n = 0;
    osal_mutex_lock(R.lock, OSAL_WAIT_FOREVER);
    for (int i = 0; i < JOBS_MAX; i++) n += R.jobs[i].used;
    osal_mutex_unlock(R.lock);
    return n;
}
