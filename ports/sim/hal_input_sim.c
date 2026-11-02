/*
 * Input model: capacitive touch controller and GPIO keys behind realistic
 * drivers, plus the shared I2C bus.
 *
 * Touch (FT6336U-class on a 100 kHz bus shared with the PMIC, fuel gauge and
 * RTC): the controller notices a finger after touch_detect_us and raises INT;
 * the driver task then reads it over I2C (touch_read_us, waits if the bus is
 * busy, e.g. during the e-ink PMIC power-up) and keeps polling every
 * touch_poll_ms (RTOS-tick quantized) until the finger lifts.
 * Keys: GPIO interrupt + debounce, delivered by the same driver task.
 *
 * Events carry t_us = the physical time of the action (known exactly here),
 * so system-side latency probes measure true end-to-end latency.
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
#define DEBOUNCE_NS SIM_MS(20)

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
    osal_task_t *task;
    /* stats */
    uint64_t touches;
} IN;

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
        vtk_event_after(SIM_US(g_profile.touch_detect_us), touch_irq_cb, NULL);
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

void sim_input_key(hal_key_t key, bool pressed) {
    if (!(g_profile.keys_mask & (1u << key))) {
        sim_warn("script pressed a key this device does not have (%d)", (int)key);
        return;
    }
    key_ev_t *k = &KPEND[kpend_n++ % KEY_FIFO];
    k->key = key;
    k->pressed = pressed;
    k->t = vtk_isr_now();
    vtk_event_after(DEBOUNCE_NS, key_debounced_cb, k);
    sim_trace_instant("input", pressed ? "key-down" : "key-up", vtk_isr_now(), NULL);
}

/* ---------------------------------------------------------------- driver */

static void deliver(const hal_input_event_t *e) {
    if (IN.sink) IN.sink(e, IN.ctx);
}

static bool any_reported_down(void) {
    for (int i = 0; i < MAX_TOUCH; i++)
        if (IN.reported[i].down) return true;
    return false;
}

static void poll_touch_once(void) {
    sim_i2c_transfer("touch", g_profile.touch_read_bytes);
    uint64_t t_read = vtk_now();
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
        while (!IN.irq && IN.kn == 0) vtk_block_locked(&IN.wq, VTK_TIME_INF);
        bool irq = IN.irq;
        IN.irq = false;
        vtk_unlock();
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
        osal_task_delay_us((uint64_t)g_profile.i2c_background_poll_ms * 1000ull);
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
