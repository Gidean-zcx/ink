/*
 * A Bluetooth headset, as behaviour (see ink/hal_bt.h): it is switched on, in range, paired and connected; the sound goes
 * to it when the route says so (hal_audio_sim.c). No chip commands are made up here.
 */
#include <stdio.h>
#include <string.h>

#include "ink/hal_bt.h"
#include "ink/osal.h"
#include "sim.h"

#define MAX_HEADSETS 4
#define SCAN_FIND_US 1500000ull
#define PAIR_US 2000000ull
#define CONNECT_US 1500000ull

static struct {
    struct {
        hal_bt_device_t d;
        bool used, on, in_range, paired;
    } h[MAX_HEADSETS];
    int connected; /* index + 1 */
    void (*fn)(void *ctx, hal_bt_event_t ev);
    void *ctx;
} BT;

bool hal_bt_available(void) { return sim_board_present(); }

static int find(const char *addr) {
    for (int i = 0; i < MAX_HEADSETS; i++)
        if (BT.h[i].used && !strcmp(BT.h[i].d.addr, addr)) return i;
    return -1;
}

int hal_bt_scan(hal_bt_device_t *out, int max, int timeout_ms) {
    osal_task_delay_us((uint64_t)timeout_ms * 1000u < SCAN_FIND_US ? (uint64_t)timeout_ms * 1000u : SCAN_FIND_US);
    int n = 0;
    for (int i = 0; i < MAX_HEADSETS && n < max; i++)
        if (BT.h[i].used && BT.h[i].on && BT.h[i].in_range) out[n++] = BT.h[i].d;
    return n;
}

int hal_bt_pair(const char *addr) {
    int i = find(addr);
    if (i < 0 || !BT.h[i].on || !BT.h[i].in_range) return -1;
    osal_task_delay_us(PAIR_US);
    if (!BT.h[i].on || !BT.h[i].in_range) return -1;
    BT.h[i].paired = true;
    sim_metric_add("bt.pairs", 1);
    return 0;
}

int hal_bt_connect(const char *addr) {
    int i = find(addr);
    if (i < 0 || !BT.h[i].paired || !BT.h[i].on || !BT.h[i].in_range) return -1;
    osal_task_delay_us(CONNECT_US);
    if (!BT.h[i].on || !BT.h[i].in_range) return -1;
    BT.connected = i + 1;
    sim_metric_add("bt.connects", 1);
    if (BT.fn) BT.fn(BT.ctx, HAL_BT_EV_CONNECTED);
    return 0;
}

void hal_bt_disconnect(void) { BT.connected = 0; }

void hal_bt_forget(const char *addr) {
    int i = find(addr);
    if (i < 0) return;
    BT.h[i].paired = false;
    if (BT.connected == i + 1) BT.connected = 0;
}

void hal_bt_on_event(void (*fn)(void *ctx, hal_bt_event_t ev), void *ctx) {
    BT.fn = fn;
    BT.ctx = ctx;
}

/* ---- the script's hands ---- */

static void tell_lost(void *arg) { /* the system event task, as with the network's news */
    (void)arg;
    if (BT.fn) BT.fn(BT.ctx, HAL_BT_EV_LOST);
}

static void gone(int i) {
    if (BT.connected != i + 1) return;
    BT.connected = 0;
    sim_sys_evt(tell_lost, NULL);
}

void sim_headset_add(const char *addr, const char *name) {
    for (int i = 0; i < MAX_HEADSETS; i++)
        if (!BT.h[i].used) {
            BT.h[i].used = BT.h[i].on = BT.h[i].in_range = true;
            snprintf(BT.h[i].d.addr, sizeof BT.h[i].d.addr, "%s", addr);
            snprintf(BT.h[i].d.name, sizeof BT.h[i].d.name, "%s", name);
            return;
        }
}

void sim_headset_power(const char *addr, bool on) {
    int i = find(addr);
    if (i < 0) return;
    BT.h[i].on = on;
    if (!on) gone(i);
}

void sim_headset_range(const char *addr, bool in) {
    int i = find(addr);
    if (i < 0) return;
    BT.h[i].in_range = in;
    if (!in) gone(i);
}
