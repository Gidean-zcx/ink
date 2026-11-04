/*
 * A connection does not survive a move to the other network, also without the hub link closing it
 * itself (audit AUD12-005, the check from the 2026-11-04 audit's experiments): the simulator's HAL
 * ends what ran on the old link.
 */
#include <stdio.h>

#include "ink/hal_net.h"
#include "ink/osal.h"
#include "ink/probe.h"
#include "sim.h"

#define CHECK(cond, ...)                                                 \
    do {                                                                 \
        if (cond) {                                                      \
            ink_probe_count("test.pass");                                \
        } else {                                                         \
            char _m[300];                                                \
            snprintf(_m, sizeof _m, __VA_ARGS__);                        \
            sim_warn("CHECK FAILED (%s:%d) %s", __FILE__, __LINE__, _m); \
            ink_probe_count("test.fail");                                \
        }                                                                \
    } while (0)

static osal_sem_t *g_connected, *g_closed;

static void on_ws(void *ctx, hal_ws_event_t ev, const uint8_t *data, size_t len) {
    (void)ctx;
    (void)data;
    (void)len;
    if (ev == HAL_WS_CONNECTED) osal_sem_give(g_connected);
    if (ev == HAL_WS_CLOSED) osal_sem_give(g_closed);
}

static void switch_task(void *arg) {
    (void)arg;
    hal_net_use(HAL_LINK_WIFI);
    osal_task_delay_us(3000000);
    hal_ws_t *ws = hal_ws_open("wss://hub.local/v1/device", on_ws, NULL);
    CHECK(ws && osal_sem_take(g_connected, 5000000) == OSAL_OK, "a connection on Wi-Fi");
    hal_net_use(HAL_LINK_LTE);
    CHECK(osal_sem_take(g_closed, 6000000) == OSAL_OK, "moved to LTE, with Wi-Fi still around: it ends");
    hal_link_status_t st;
    hal_net_link_status(&st);
    CHECK(st.type == HAL_LINK_LTE && st.up, "and LTE is up");
    hal_ws_free(ws);
    ink_probe_count("test.finished");
}

void ink_main(void) {
    g_connected = osal_sem_create(0, 1);
    g_closed = osal_sem_create(0, 1);
    osal_task_cfg_t c = {.name = "switch", .fn = switch_task, .stack_bytes = 4096, .priority = 3, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    if (osal_task_create(&c, NULL) != OSAL_OK) osal_panic("test task");
}
