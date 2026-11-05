/*
 * Display service task: owns the panel, runs the refresh scheduler.
 * Producers commit regions (messages); the task dispatches one panel update at
 * a time and folds everything that arrives meanwhile into the next dispatch.
 */
#include <stdio.h>
#include <string.h>

#include "ink/osal.h"
#include "ink/probe.h"
#include "ink/sys/display.h"
#include "refresh_policy.h"

#define DS_QUEUE_LEN 24
#define DS_STACK 3072
#define DS_PRIO 8
#define HOLD_INTERACTIVE_MS 2000
#define HOLD_AFTER_FEEDBACK_MS 1500

typedef enum { DM_REGION, DM_FRAME_END, DM_DONE, DM_INTERACTIVE, DM_CLEANUP_EVERY, DM_GUARD } dm_type_t;

typedef struct {
    uint8_t type;
    uint8_t flag;
    uint8_t intent;
    hal_rect_t rect;
    uint64_t cause_us;
    const char *cause_tag;
    hal_display_result_t result;
} dmsg_t;

static struct {
    osal_queue_t *q;
    osal_mutex_t *lock;
    uint8_t *fb;
    int w, h, stride;
    hal_panel_info_t info;
    rp_state_t rp;
    int nflight; /* updates handed to the platform and not done (one at a time unless async) */
    struct {
        uint32_t id;
        rp_dispatch_t d;
    } flight[16];
    bool interactive;
    bool guard;
    osal_sem_t *guard_ack;
    int busy;  /* regions waiting in the policy + the update in flight (this task writes it) */
    int inbox; /* messages sent and not yet fully handled (atomic) */
    uint32_t commits;
} DS;

/* Every message goes through here: the inbox count rises before the message is queued and
 * falls only after the service task has handled it and updated busy, so ink_display_busy()
 * never reads 0 while a message is between the queue and the policy (the power service sleeps
 * the chip on that answer). */
static void ds_send(const dmsg_t *m) {
    __atomic_add_fetch(&DS.inbox, 1, __ATOMIC_SEQ_CST);
    osal_queue_send(DS.q, m, OSAL_WAIT_FOREVER);
}

static const char *INTENT_NAME[INK_REFRESH_COUNT] = {"A2", "FAST", "UI", "TEXT", "FLASH_UI", "FULL"};

static void done_cb(void *ctx, const hal_display_result_t *r) {
    (void)ctx;
    dmsg_t m = {.type = DM_DONE, .result = *r};
    ds_send(&m);
}

static void dispatch_one(rp_dispatch_t d);

/* EPDiy-class panels take one update at a time (global lock); a direct-drive panel (async) takes
 * every committed region at once and converges them together */
static void dispatch_next(void) {
    rp_dispatch_t d;
    const int cap = DS.info.async_updates ? (int)(sizeof DS.flight / sizeof DS.flight[0]) : 1;
    while (DS.nflight < cap && !DS.guard && rp_next(&DS.rp, &d)) dispatch_one(d);
}

static void dispatch_one(rp_dispatch_t d) {
    /* power policy: keep rails up around touch feedback and interactive phases;
     * otherwise power off right after the update (reading = long idle gaps) */
    if (DS.interactive) hal_display_hold_power(HOLD_INTERACTIVE_MS);
    else if (d.intent <= INK_REFRESH_FAST) hal_display_hold_power(HOLD_AFTER_FEEDBACK_MS);
    else hal_display_hold_power(0);
    uint32_t flags = d.intent <= INK_REFRESH_FAST ? HAL_UPD_FLAG_PRIORITY : 0;
    osal_mutex_lock(DS.lock, OSAL_WAIT_FOREVER);
    uint32_t id = hal_display_update(&d.rect, DS.fb + (size_t)d.rect.y * DS.stride + d.rect.x, DS.stride, d.wf, flags,
                                     done_cb, NULL);
    osal_mutex_unlock(DS.lock);
    if (!id) {
        rp_done(&DS.rp, &d, false);
        return;
    }
    DS.flight[DS.nflight].id = id;
    DS.flight[DS.nflight].d = d;
    DS.nflight++;
    char key[48];
    snprintf(key, sizeof key, "display.dispatch.%s%s", INTENT_NAME[d.intent], d.cleanup ? "+clean" : "");
    ink_probe_count(key);
}

/* A frame split into several dispatches carries the same cause: record each action once. */
static bool cause_seen(uint64_t cause, const char *tag) {
    static struct {
        uint64_t cause;
        const char *tag;
    } seen[8];
    static int next;
    for (int i = 0; i < 8; i++)
        if (seen[i].cause == cause && seen[i].tag == tag) return true;
    seen[next].cause = cause;
    seen[next].tag = tag;
    next = (next + 1) % 8;
    return false;
}

static void on_done(const hal_display_result_t *r) {
    int k = 0;
    while (k < DS.nflight && DS.flight[k].id != r->id) k++;
    if (k == DS.nflight) return; /* not ours (cannot happen) */
    rp_dispatch_t cur = DS.flight[k].d;
    DS.flight[k] = DS.flight[--DS.nflight];
    rp_done(&DS.rp, &cur, r->ok);
    if (cur.cause_us && !cause_seen(cur.cause_us, cur.cause_tag)) {
        if (cur.cause_tag) {
            /* a named cause (e.g. "AI answer visible after release") has its own metric and may
             * include network time, so it stays out of the generic input->screen latency */
            ink_probe_value(cur.cause_tag, (double)(r->done_us - cur.cause_us) / 1000.0);
        } else {
            ink_probe_value("ui.action_to_first_change_ms", (double)(r->started_us - cur.cause_us) / 1000.0);
            ink_probe_value("ui.action_to_visible_ms", (double)(r->done_us - cur.cause_us) / 1000.0);
            char key[64];
            snprintf(key, sizeof key, "ui.action_to_visible_ms.%s", INTENT_NAME[cur.intent]);
            ink_probe_value(key, (double)(r->done_us - cur.cause_us) / 1000.0);
        }
    }
}

static void service_task(void *arg) {
    (void)arg;
    dmsg_t m;
    for (;;) {
        osal_queue_recv(DS.q, &m, OSAL_WAIT_FOREVER);
        switch (m.type) {
        case DM_REGION: rp_add(&DS.rp, m.rect, (ink_refresh_t)m.intent, m.cause_us, m.cause_tag); break;
        case DM_FRAME_END: break;
        case DM_DONE: on_done(&m.result); break;
        case DM_INTERACTIVE:
            DS.interactive = m.flag != 0;
            if (!DS.interactive && !DS.nflight) hal_display_hold_power(0);
            break;
        case DM_CLEANUP_EVERY: DS.rp.text_cleanup_every = (int)m.cause_us; break;
        case DM_GUARD:
            DS.guard = m.flag != 0;
            /* EPDiy-class: acknowledge once the update in flight is done (below); async panels are
             * paused by the requester itself (ink_display_flash_guard) */
            if (!DS.info.async_updates && DS.guard && !DS.nflight) osal_sem_give(DS.guard_ack);
            break;
        }
        if (m.type == DM_DONE && DS.guard && !DS.info.async_updates && !DS.nflight) osal_sem_give(DS.guard_ack);
        /* only dispatch on frame boundaries so a half-committed frame is never driven */
        if (m.type != DM_REGION) dispatch_next();
        __atomic_store_n(&DS.busy, DS.rp.npending + DS.nflight, __ATOMIC_SEQ_CST);
        __atomic_sub_fetch(&DS.inbox, 1, __ATOMIC_SEQ_CST); /* after busy: see ds_send */
    }
}

int ink_display_init(void) {
    hal_display_get_info(&DS.info);
    DS.w = DS.info.width;
    DS.h = DS.info.height;
    DS.stride = DS.w;
    const char *prev = osal_mem_tag("display-service");
    DS.fb = osal_malloc((size_t)DS.stride * DS.h, OSAL_MEM_EXTERNAL);
    DS.lock = osal_mutex_create();
    DS.q = osal_queue_create(sizeof(dmsg_t), DS_QUEUE_LEN);
    DS.guard_ack = osal_sem_create(0, 1);
    osal_mem_tag(prev);
    if (!DS.fb || !DS.lock || !DS.q || !DS.guard_ack) return -1;
    memset(DS.fb, 0xff, (size_t)DS.stride * DS.h);
    rp_init(&DS.rp, DS.w, DS.h, DS.info.waveforms, DS.info.tech == HAL_PANEL_LCD);
    DS.rp.async = DS.info.async_updates;
    if (DS.rp.async) DS.rp.text_cleanup_every = 0;
    osal_task_cfg_t cfg = {.name = "display", .fn = service_task, .stack_bytes = DS_STACK, .priority = DS_PRIO, .core = -1,
                           .flags = OSAL_TASK_STACK_EXTERNAL}; /* touches the PSRAM framebuffer anyway */
    return osal_task_create(&cfg, NULL) == OSAL_OK ? 0 : -1;
}

void ink_display_native_size(int *w, int *h) {
    *w = DS.w;
    *h = DS.h;
}
uint8_t *ink_display_fb(void) { return DS.fb; }
int ink_display_stride(void) { return DS.stride; }
void ink_display_lock(void) { osal_mutex_lock(DS.lock, OSAL_WAIT_FOREVER); }
void ink_display_unlock(void) { osal_mutex_unlock(DS.lock); }

void ink_display_commit(const ink_region_t *regions, int n, uint64_t cause_us) {
    ink_display_commit_tagged(regions, n, cause_us, NULL);
}

uint32_t ink_display_commits(void) { return __atomic_load_n(&DS.commits, __ATOMIC_RELAXED); }

void ink_display_commit_tagged(const ink_region_t *regions, int n, uint64_t cause_us, const char *cause_tag) {
    __atomic_add_fetch(&DS.commits, 1, __ATOMIC_RELAXED);
    for (int i = 0; i < n; i++) {
        dmsg_t m = {.type = DM_REGION, .intent = (uint8_t)regions[i].intent, .rect = regions[i].rect, .cause_us = cause_us,
                    .cause_tag = cause_tag};
        ds_send(&m);
    }
    dmsg_t end = {.type = DM_FRAME_END};
    ds_send(&end);
}

void ink_display_interactive(bool on) {
    dmsg_t m = {.type = DM_INTERACTIVE, .flag = on};
    ds_send(&m);
}

void ink_display_set_text_cleanup_every(int n) {
    dmsg_t m = {.type = DM_CLEANUP_EVERY, .cause_us = (uint64_t)n};
    ds_send(&m);
}

/* Messages not yet handled + regions waiting in the policy + the update in flight. Read the
 * inbox first: the service task updates busy before it lowers the inbox. */
int ink_display_busy(void) {
    int inbox = __atomic_load_n(&DS.inbox, __ATOMIC_SEQ_CST);
    return inbox + __atomic_load_n(&DS.busy, __ATOMIC_SEQ_CST);
}

void ink_display_flash_guard(bool hold) {
    dmsg_t m = {.type = DM_GUARD, .flag = hold};
    if (DS.info.async_updates) {
        /* direct drive: the panel stops at the next phase boundary (pixels hold still) instead of
         * waiting for every update to settle; paused here, in the requester's task, so the service
         * keeps taking the driver's done messages meanwhile. New dispatches wait for the release. */
        if (hold) {
            ds_send(&m);
            hal_display_pause(true);
        } else {
            hal_display_pause(false);
            ds_send(&m);
        }
        return;
    }
    ds_send(&m);
    if (hold) osal_sem_take(DS.guard_ack, OSAL_WAIT_FOREVER);
}
