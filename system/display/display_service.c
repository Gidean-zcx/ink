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

typedef enum { DM_REGION, DM_FRAME_END, DM_DONE, DM_INTERACTIVE, DM_CLEANUP_EVERY } dm_type_t;

typedef struct {
    uint8_t type;
    uint8_t flag;
    uint8_t intent;
    hal_rect_t rect;
    uint64_t cause_us;
    hal_display_result_t result;
} dmsg_t;

static struct {
    osal_queue_t *q;
    osal_mutex_t *lock;
    uint8_t *fb;
    int w, h, stride;
    hal_panel_info_t info;
    rp_state_t rp;
    bool in_flight;
    rp_dispatch_t cur;
    bool interactive;
    volatile int busy;
} DS;

static const char *INTENT_NAME[INK_REFRESH_COUNT] = {"A2", "FAST", "UI", "TEXT", "FLASH_UI", "FULL"};

static void done_cb(void *ctx, const hal_display_result_t *r) {
    (void)ctx;
    dmsg_t m = {.type = DM_DONE, .result = *r};
    osal_queue_send(DS.q, &m, OSAL_WAIT_FOREVER);
}

static void dispatch_next(void) {
    rp_dispatch_t d;
    if (DS.in_flight || !rp_next(&DS.rp, &d)) return;
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
    DS.in_flight = true;
    DS.cur = d;
    char key[48];
    snprintf(key, sizeof key, "display.dispatch.%s%s", INTENT_NAME[d.intent], d.cleanup ? "+clean" : "");
    ink_probe_count(key);
}

static void on_done(const hal_display_result_t *r) {
    DS.in_flight = false;
    rp_done(&DS.rp, &DS.cur, r->ok);
    if (DS.cur.cause_us) {
        ink_probe_value("ui.action_to_first_change_ms", (double)(r->started_us - DS.cur.cause_us) / 1000.0);
        ink_probe_value("ui.action_to_visible_ms", (double)(r->done_us - DS.cur.cause_us) / 1000.0);
        char key[64];
        snprintf(key, sizeof key, "ui.action_to_visible_ms.%s", INTENT_NAME[DS.cur.intent]);
        ink_probe_value(key, (double)(r->done_us - DS.cur.cause_us) / 1000.0);
    }
}

static void service_task(void *arg) {
    (void)arg;
    dmsg_t m;
    for (;;) {
        osal_queue_recv(DS.q, &m, OSAL_WAIT_FOREVER);
        switch (m.type) {
        case DM_REGION: rp_add(&DS.rp, m.rect, (ink_refresh_t)m.intent, m.cause_us); break;
        case DM_FRAME_END: break;
        case DM_DONE: on_done(&m.result); break;
        case DM_INTERACTIVE:
            DS.interactive = m.flag != 0;
            if (!DS.interactive && !DS.in_flight) hal_display_hold_power(0);
            break;
        case DM_CLEANUP_EVERY: DS.rp.text_cleanup_every = (int)m.cause_us; break;
        }
        /* only dispatch on frame boundaries so a half-committed frame is never driven */
        if (m.type != DM_REGION) dispatch_next();
        DS.busy = DS.rp.npending + (DS.in_flight ? 1 : 0);
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
    osal_mem_tag(prev);
    if (!DS.fb || !DS.lock || !DS.q) return -1;
    memset(DS.fb, 0xff, (size_t)DS.stride * DS.h);
    rp_init(&DS.rp, DS.w, DS.h, DS.info.waveforms, DS.info.tech == HAL_PANEL_LCD);
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
    for (int i = 0; i < n; i++) {
        dmsg_t m = {.type = DM_REGION, .intent = (uint8_t)regions[i].intent, .rect = regions[i].rect, .cause_us = cause_us};
        osal_queue_send(DS.q, &m, OSAL_WAIT_FOREVER);
    }
    dmsg_t end = {.type = DM_FRAME_END};
    osal_queue_send(DS.q, &end, OSAL_WAIT_FOREVER);
    DS.busy += n;
}

void ink_display_interactive(bool on) {
    dmsg_t m = {.type = DM_INTERACTIVE, .flag = on};
    osal_queue_send(DS.q, &m, OSAL_WAIT_FOREVER);
}

void ink_display_set_text_cleanup_every(int n) {
    dmsg_t m = {.type = DM_CLEANUP_EVERY, .cause_us = (uint64_t)n};
    osal_queue_send(DS.q, &m, OSAL_WAIT_FOREVER);
}

int ink_display_busy(void) { return DS.busy; }
