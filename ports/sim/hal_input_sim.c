/*
 * Input model: capacitive touch controller and GPIO keys behind realistic
 * drivers, plus the shared I2C bus.
 *
 * Touch (FT6336U-class on a 100 kHz bus shared with the PMIC, fuel gauge and
 * RTC): the controller notices a finger after touch_detect_us and raises INT;
 * the driver task then reads it over I2C (touch_read_bytes at i2c_khz, waits if the bus is
 * busy, e.g. during the e-ink PMIC power-up) and keeps polling every
 * touch_poll_ms (RTOS-tick quantized) until the finger lifts.
 * Keys, per the profile's key_sources:
 *   gpio      interrupt + debounce timer (AI key = BOOT on the Plus)
 *   expander  IO expander INT, then I2C reads on the shared bus: one when the
 *             INT fires, one after the debounce time (POWER, VOL+/-)
 *   touch     cover keys read by the touch controller (HOME/PREV/NEXT): like a
 *             finger — INT after touch_detect_us, I2C read, polling until
 *             release — so they also wait while the PMIC holds the bus.
 *
 * Events carry t_us = the physical time of the action (known exactly here),
 * so system-side latency probes measure true end-to-end latency.
 *
 * Power: after touch_monitor_after_ms untouched the controller scans in its
 * slow monitor mode: the first touch (or cover key) waits up to one monitor
 * period longer. While the chip sleeps, touches, cover keys and keys that
 * are not armed wake sources are lost; an armed key wakes it and is read
 * after the wake. Background bus polling slows down when the system is idle.
 */
#include <stdio.h>
#include <string.h>

#include "ink/hal_input.h"
#include "ink/rotation.h"
#include "sim.h"

#define MAX_TOUCH 2
#define KEY_FIFO 32
#define INPUT_STACK 3072
#define INPUT_PRIO 12
#define BG_STACK 2048

typedef struct {
    bool down;
    int nx, ny;
    uint64_t t_change; /* physical time of last down/up */
} phys_touch_t;

static struct {
    hal_input_sink_fn sink;
    void *ctx;
    vtk_waitq_t wq;
    bool irq;            /* touch INT asserted */
    phys_touch_t phys[MAX_TOUCH];
    struct { bool down; int nx, ny; } reported[MAX_TOUCH];
    hal_input_event_t keys[KEY_FIFO];
    int kh, kn;
    /* IO expander keys: changes waiting for the driver's reads */
    bool exp_irq, exp_confirm;
    struct { hal_key_t key; bool pressed; uint64_t t; } exp_pending[KEY_FIFO];
    int exp_n;
    /* cover keys: touch points the controller reports outside the panel (upstream: Y ~ 1600), one
     * per key, sharing the controller's touch points with the fingers */
    struct { bool down, reported; uint64_t t_change; } vkey[HAL_KEY_COUNT];
    osal_task_t *task;
    /* stats */
    uint64_t touches;
    /* power */
    uint64_t last_touch_activity;
    hal_key_t dropped_key; /* press lost while asleep: drop its release too */
    uint32_t bg_poll_ms;
} IN;

void sim_power_input_lost(void);

/* Extra detection delay if the controller has dropped to its monitor scan. ISR context. */
static uint64_t monitor_delay_ns(void) {
    uint64_t now = vtk_isr_now(), extra = 0;
    if (g_profile.touch_monitor_after_ms && g_profile.touch_monitor_period_ms &&
        now - IN.last_touch_activity >= SIM_MS(g_profile.touch_monitor_after_ms))
        extra = (uint64_t)(sim_rand01() * (double)SIM_MS(g_profile.touch_monitor_period_ms));
    IN.last_touch_activity = now;
    if (extra) sim_metric_sample("input.touch_monitor_extra_ms", extra / 1e6);
    return extra;
}

void sim_input_set_power_state(hal_power_state_t s) {
    IN.bg_poll_ms = s == HAL_PWR_STATE_ACTIVE ? g_profile.i2c_background_poll_ms : g_profile.i2c_background_idle_poll_ms;
}

static struct {
    osal_mutex_t *bus;
    uint64_t busy_ns;
} I2C;

/* ------------------------------------------------------------------- I2C */

void sim_i2c_init(void) {
    const char *prev = osal_mem_tag("i2c-driver");
    I2C.bus = osal_mutex_create();
    osal_mem_tag(prev);
    if (!I2C.bus) osal_panic("i2c init: no memory");
}

static void bus_run(const char *who, uint64_t ns) {
    uint64_t t_req = vtk_now();
    osal_mutex_lock(I2C.bus, OSAL_WAIT_FOREVER);
    uint64_t t0 = vtk_now();
    if (t0 > t_req) {
        char key[64];
        snprintf(key, sizeof key, "i2c.wait_ms.%s", who);
        sim_metric_sample(key, (t0 - t_req) / 1e6);
    }
    sim_charge_ns_always(SIM_US(15)); /* driver setup + ISR work */
    vtk_sleep_ns(ns);
    I2C.busy_ns += ns;
    sim_trace_span("i2c", who, t0, vtk_now(), NULL);
    osal_mutex_unlock(I2C.bus);
}

void sim_i2c_transfer(const char *who, int bytes) {
    /* address + register + repeated start + data, 9 bit times per byte */
    uint64_t bits = (uint64_t)(bytes + 3) * 9u;
    uint64_t ns = bits * 1000000ull / (uint64_t)(g_profile.i2c_khz > 0 ? g_profile.i2c_khz : 100) + SIM_US(60);
    bus_run(who, ns);
}

void sim_i2c_hold(const char *who, uint64_t ns) { bus_run(who, ns); }

/* -------------------------------------------------------------- physical */

static void touch_irq_cb(void *arg) { /* ISR: controller raises INT */
    (void)arg;
    IN.irq = true;
    vtk_isr_wake_one(&IN.wq);
}

void sim_input_touch(int id, bool down, int lx, int ly) {
    if (id < 0 || id >= MAX_TOUCH || !g_profile.has_touch) return;
    if (sim_power_sleeping()) { /* touch is not a wake source: the chip never sees it */
        if (down) sim_power_input_lost();
        return;
    }
    phys_touch_t *p = &IN.phys[id];
    int nx, ny;
    ink_logical_to_native(g_profile.panel.width, g_profile.panel.height, g_profile.panel.rotation_deg, lx, ly, &nx, &ny);
    bool was = p->down;
    p->nx = nx;
    p->ny = ny;
    p->down = down;
    if (down != was) p->t_change = vtk_isr_now();
    if (down && !was) {
        IN.touches++;
        vtk_event_after(SIM_US(g_profile.touch_detect_us) + monitor_delay_ns(), touch_irq_cb, NULL);
        sim_trace_instant("input", "finger-down", vtk_isr_now(), NULL);
    } else if (!down && was) {
        sim_trace_instant("input", "finger-up", vtk_isr_now(), NULL);
    }
}

typedef struct {
    hal_key_t key;
    bool pressed;
    uint64_t t;
} key_ev_t;

static key_ev_t KPEND[KEY_FIFO];
static int kpend_n;

static void key_debounced_cb(void *arg) { /* ISR: debounce timer expired */
    key_ev_t *k = arg;
    if (IN.kn < KEY_FIFO) {
        hal_input_event_t *e = &IN.keys[(IN.kh + IN.kn++) % KEY_FIFO];
        memset(e, 0, sizeof *e);
        e->type = HAL_IN_KEY;
        e->t_us = k->t / 1000;
        e->key.key = k->key;
        e->key.pressed = k->pressed;
        vtk_isr_wake_one(&IN.wq);
    } else {
        sim_metric_add("input.key_overflow", 1);
    }
}

static void expander_confirm_cb(void *arg) { /* ISR: debounce time over -> second read */
    (void)arg;
    IN.exp_confirm = true;
    vtk_isr_wake_one(&IN.wq);
}

void sim_input_key(hal_key_t key, bool pressed) {
    if (!(g_profile.keys_mask & (1u << key))) {
        /* a shared script on a device without this key: not a device problem */
        sim_metric_add("input.absent_key_presses", 1);
        return;
    }
    sim_trace_instant("input", pressed ? "key-down" : "key-up", vtk_isr_now(), NULL);
    if (sim_power_sleeping()) {
        /* asleep: an armed wake key wakes the chip and is read afterwards; anything else is lost */
        uint32_t src = key == HAL_KEY_POWER ? HAL_WAKE_POWER_KEY : key == HAL_KEY_AI ? HAL_WAKE_AI_KEY : 0;
        bool keep = sim_power_waking() || (pressed && src && sim_power_wake(src));
        if (!pressed && IN.dropped_key == key) keep = false;
        if (!keep) {
            if (pressed) {
                IN.dropped_key = key;
                sim_power_input_lost();
            }
            return;
        }
    }
    if (pressed && IN.dropped_key == key) IN.dropped_key = HAL_KEY_NONE;
    if (g_profile.key_source[key] == 1) { /* IO expander: INT now, reads by the driver task */
        if (IN.exp_n < KEY_FIFO) {
            IN.exp_pending[IN.exp_n].key = key;
            IN.exp_pending[IN.exp_n].pressed = pressed;
            IN.exp_pending[IN.exp_n].t = vtk_isr_now();
            IN.exp_n++;
        }
        IN.exp_irq = true;
        vtk_isr_wake_one(&IN.wq);
        return;
    }
    if (g_profile.key_source[key] == 2 && g_profile.has_touch) { /* cover key: the touch controller sees a finger */
        if (pressed != IN.vkey[key].down) {
            IN.vkey[key].down = pressed;
            IN.vkey[key].t_change = vtk_isr_now();
            if (pressed) vtk_event_after(SIM_US(g_profile.touch_detect_us) + monitor_delay_ns(), touch_irq_cb, NULL);
        }
        return;
    }
    key_ev_t *k = &KPEND[kpend_n++ % KEY_FIFO];
    k->key = key;
    k->pressed = pressed;
    k->t = vtk_isr_now();
    vtk_event_after(SIM_US(g_profile.key_debounce_us), key_debounced_cb, k);
}

/* ---------------------------------------------------------------- driver */

static void deliver(const hal_input_event_t *e) {
    if (IN.sink) IN.sink(e, IN.ctx);
}

static int points_reported(void) {
    int n = 0;
    for (int i = 0; i < MAX_TOUCH; i++) n += IN.reported[i].down;
    for (int k = 0; k < HAL_KEY_COUNT; k++) n += IN.vkey[k].reported;
    return n;
}

static bool any_reported_down(void) { return points_reported() > 0; }

static void deliver_key(hal_key_t key, bool pressed, uint64_t t_ns) {
    hal_input_event_t e = {.type = HAL_IN_KEY, .t_us = t_ns / 1000};
    e.key.key = key;
    e.key.pressed = pressed;
    sim_metric_sample("input.key_delivery_ms", (vtk_now() - t_ns) / 1e6);
    deliver(&e);
}

static void poll_touch_once(void) {
    sim_i2c_transfer("touch", g_profile.touch_read_bytes);
    uint64_t t_read = vtk_now();
    /* cover keys, each its own point: releases first (they free points), then presses while the
     * controller has points left (a press beyond that is reported once a point frees) */
    for (int k = 0; k < HAL_KEY_COUNT; k++)
        if (!IN.vkey[k].down && IN.vkey[k].reported) {
            IN.vkey[k].reported = false;
            deliver_key((hal_key_t)k, false, IN.vkey[k].t_change);
        }
    for (int k = 0; k < HAL_KEY_COUNT; k++)
        if (IN.vkey[k].down && !IN.vkey[k].reported && points_reported() < g_profile.touch_points) {
            IN.vkey[k].reported = true;
            deliver_key((hal_key_t)k, true, IN.vkey[k].t_change);
        }
    for (int i = 0; i < MAX_TOUCH && i < g_profile.touch_points; i++) {
        phys_touch_t *p = &IN.phys[i];
        hal_input_event_t e = {.type = HAL_IN_TOUCH};
        e.touch.id = i;
        if (p->down && !IN.reported[i].down) {
            e.touch.phase = HAL_TOUCH_DOWN;
            e.t_us = p->t_change / 1000;
        } else if (p->down && (p->nx != IN.reported[i].nx || p->ny != IN.reported[i].ny)) {
            e.touch.phase = HAL_TOUCH_MOVE;
            e.t_us = t_read / 1000;
        } else if (!p->down && IN.reported[i].down) {
            e.touch.phase = HAL_TOUCH_UP;
            e.t_us = p->t_change / 1000;
        } else {
            continue;
        }
        e.touch.x = p->down ? p->nx : IN.reported[i].nx;
        e.touch.y = p->down ? p->ny : IN.reported[i].ny;
        IN.reported[i].down = p->down;
        IN.reported[i].nx = e.touch.x;
        IN.reported[i].ny = e.touch.y;
        sim_metric_sample("input.touch_delivery_ms", (vtk_now() / 1000 - e.t_us) / 1e3);
        deliver(&e);
    }
}

static void input_task(void *arg) {
    (void)arg;
    for (;;) {
        vtk_lock();
        while (!IN.irq && IN.kn == 0 && !IN.exp_irq && !IN.exp_confirm) vtk_block_locked(&IN.wq, VTK_TIME_INF);
        bool irq = IN.irq, exp_irq = IN.exp_irq, exp_confirm = IN.exp_confirm;
        IN.irq = IN.exp_irq = IN.exp_confirm = false;
        vtk_unlock();
        if (exp_irq) { /* read the expander's input ports (clears its INT), confirm after the debounce time */
            sim_i2c_transfer("io-expander", 2);
            vtk_lock();
            vtk_event_after(SIM_US(g_profile.key_debounce_us), expander_confirm_cb, NULL);
            vtk_unlock();
        }
        if (exp_confirm) {
            sim_i2c_transfer("io-expander", 2);
            for (int i = 0; i < IN.exp_n; i++) deliver_key(IN.exp_pending[i].key, IN.exp_pending[i].pressed, IN.exp_pending[i].t);
            IN.exp_n = 0;
        }
        while (IN.kn > 0) {
            hal_input_event_t e = IN.keys[IN.kh];
            IN.kh = (IN.kh + 1) % KEY_FIFO;
            IN.kn--;
            sim_metric_sample("input.key_delivery_ms", (vtk_now() / 1000 - e.t_us) / 1e3);
            deliver(&e);
        }
        if (!irq) continue;
        /* touch session: read, then poll while any finger is down */
        do {
            poll_touch_once();
            if (!any_reported_down()) break;
            osal_task_delay_us((uint64_t)g_profile.touch_poll_ms * 1000ull);
            /* keys pressed during a touch session are still delivered promptly */
            while (IN.kn > 0) {
                hal_input_event_t e = IN.keys[IN.kh];
                IN.kh = (IN.kh + 1) % KEY_FIFO;
                IN.kn--;
                deliver(&e);
            }
        } while (1);
    }
}

/* Other devices on the shared bus (fuel gauge, RTC): periodic background traffic. */
static void i2c_background_task(void *arg) {
    (void)arg;
    for (;;) {
        osal_task_delay_us((uint64_t)(IN.bg_poll_ms ? IN.bg_poll_ms : g_profile.i2c_background_poll_ms) * 1000ull);
        sim_i2c_transfer("fuel-gauge", g_profile.i2c_background_bytes);
    }
}

/* -------------------------------------------------------------------- API */

void hal_input_set_sink(hal_input_sink_fn fn, void *ctx) {
    IN.sink = fn;
    IN.ctx = ctx;
}

void hal_input_get_info(hal_input_info_t *out) {
    out->has_touch = g_profile.has_touch;
    out->touch_points = g_profile.touch_points;
    out->keys = g_profile.keys_mask;
}

void sim_input_init(void) {
    const char *prev = osal_mem_tag("input-driver");
    osal_task_cfg_t cfg = {.name = "input", .fn = input_task, .stack_bytes = INPUT_STACK, .priority = INPUT_PRIO, .core = -1};
    if (osal_task_create(&cfg, &IN.task) != OSAL_OK) osal_panic("input driver init failed");
    if (g_profile.i2c_background_poll_ms) {
        osal_task_cfg_t bg = {.name = "i2c_bg", .fn = i2c_background_task, .stack_bytes = BG_STACK,
                              .priority = OSAL_PRIO_LOW, .core = -1};
        if (osal_task_create(&bg, NULL) != OSAL_OK) osal_panic("i2c background task init failed");
    }
    osal_mem_tag(prev);
}

void sim_input_report(void) {
    sim_metric_set("input.touches", (double)IN.touches);
    sim_metric_set("i2c.busy_ms", I2C.busy_ns / 1e6);
}
