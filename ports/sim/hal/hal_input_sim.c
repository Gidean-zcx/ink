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
 * period longer. While the chip sleeps, touches and cover keys are lost; on the
 * Plus a key only changes its expander pin, and the chip wakes when that pin
 * is an armed input (P17 POWER: the expander's INT pulls GPIO2 low); the key
 * that woke it is reported after the wake, the rest is lost. Without the
 * board model an armed key wakes the chip directly. Background bus polling
 * slows down when the system is idle.
 */
#include <stdio.h>
#include <string.h>

#include "ink/hal_input.h"
#include "metalio_plus/metalio_board.h"
#include "metalio_plus/metalio_epd_power.h"
#include "metalio_plus/metalio_touch.h"
#include "ft6336u/ft6336u.h"
#include "ink/rotation.h"
#include "sim.h"

#define MAX_TOUCH 2
#define KEY_FIFO 32
#define INPUT_STACK 3072
#define INPUT_PRIO OSAL_PRIO_MAX /* Read short contacts before the display feeders. */
#define BG_STACK 2048

typedef struct {
    bool down;
    int nx, ny;
    int lx, ly;        /* the same point in logical (UI) coordinates */
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
    uint64_t gpio2_fell; /* time of the last falling edge on GPIO2 */
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
    osal_mutex_t *sample_lock; /* expander sampling vs the sleep sequence (upstream g_sample_mu) */
    bool sampling_off;         /* the sleep sequence masked the expander: no key reads */
    /* the wake could not restore the expander (bus errors): the input task retries */
    bool restore_keys, restore_rails, restore_due;
    unsigned restore_tries;
    /* GPIO2 still low after a service: looked at again later */
    bool recheck_armed;
    unsigned rechecks;
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

typedef struct {
    const char *who;
    double ms;
} bus_wait_t;

static void sample_wait_on_host(void *p) {
    const bus_wait_t *w = p;
    char key[64];
    snprintf(key, sizeof key, "i2c.wait_ms.%s", w->who);
    sim_metric_sample(key, w->ms);
}

static void bus_run(const char *who, uint64_t ns) {
    uint64_t t_req = vtk_now();
    osal_mutex_lock(I2C.bus, OSAL_WAIT_FOREVER);
    uint64_t t0 = vtk_now();
    if (t0 > t_req) {
        bus_wait_t w = {who, (t0 - t_req) / 1e6};
        vtk_host_call(sample_wait_on_host, &w); /* bookkeeping, not the driver's stack */
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

/* GPIO2 = the expander's INT and the accelerometer's INT1, wired together (low = asserted). A
 * falling edge wakes the driver task, which services both. ISR or locked task context. */
static bool g_gpio2_low;
bool sim_gpio2_low(void) { return g_gpio2_low; }
void sim_gpio2_update(void) {
    bool low = sim_tca_int_low() || sim_accel_int_low();
    bool fell = low && !g_gpio2_low;
    g_gpio2_low = low;
    if (low) sim_power_gpio2_low(); /* EXT1 wakes on the level when armed */
    if (!fell) return;
    IN.gpio2_fell = vtk_in_isr() ? vtk_isr_now() : vtk_now();
    IN.exp_irq = true;
    if (vtk_in_isr()) vtk_isr_wake_one(&IN.wq);
    else vtk_wake_one_locked(&IN.wq);
}

/* GPIO2 interrupts on the falling edge. A read that failed leaves the expander's INT or the
 * accelerometer's latch asserted, and while the line stays low no further edge comes: no key would
 * get through again. So while the line is still low after a service the task looks again, after the
 * debounce time and backing off to once a second (on the device: the GPIO2 level after the service).
 * Not while the sleep sequence has the keys: the wake starts from the levels anyway. Locked. */
static void gpio2_recheck_cb(void *arg) { /* ISR */
    (void)arg;
    IN.recheck_armed = false;
    if (!g_gpio2_low) return;
    IN.gpio2_fell = vtk_isr_now();
    IN.exp_irq = true;
    vtk_isr_wake_one(&IN.wq);
}

static void gpio2_recheck_locked(void) {
    if (!g_gpio2_low) {
        IN.rechecks = 0;
        return;
    }
    if (IN.recheck_armed || IN.sampling_off) return;
    uint64_t gap = SIM_US(g_profile.key_debounce_us) << (IN.rechecks < 6 ? IN.rechecks : 6);
    if (gap > SIM_MS(1000)) gap = SIM_MS(1000);
    IN.rechecks++;
    IN.recheck_armed = true;
    vtk_event_after(gap, gpio2_recheck_cb, NULL);
    sim_metric_add("input.gpio2_rechecks", 1);
}

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
    p->lx = lx;
    p->ly = ly;
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
    if (sim_power_sleeping() && g_profile.key_source[key] == 1 && sim_board_present()) {
        /* asleep, the Plus: the key moves its pin; whether that wakes the chip is the expander's
         * direction and the sleep's wake configuration (P17 armed -> INT -> GPIO2) */
        uint8_t bit = key == HAL_KEY_POWER ? MB_P1_POWER_KEY : key == HAL_KEY_VOL_UP ? MB_P1_VOL_UP : MB_P1_VOL_DOWN;
        bool woke = sim_power_waking();
        sim_tca_input(1, bit, pressed ? 0 : bit);
        if (pressed && !woke && !sim_power_waking()) sim_power_input_lost();
        return;
    }
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
        uint8_t bit = key == HAL_KEY_POWER ? MB_P1_POWER_KEY : key == HAL_KEY_VOL_UP ? MB_P1_VOL_UP : key == HAL_KEY_VOL_DOWN ? MB_P1_VOL_DOWN : 0;
        if (sim_board_present() && bit) {
            sim_tca_input(1, bit, pressed ? 0 : bit); /* key pulls its pin low; the expander raises INT (GPIO2) */
        } else {
            IN.exp_irq = true;
            vtk_isr_wake_one(&IN.wq);
        }
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

static hal_i2c_dev_t *g_ft; /* the Plus: FT6336U through our driver (NULL: timing-only touch model) */
static int ft_reported(void);
static bool any_reported_down(void) { return g_ft ? ft_reported() > 0 : points_reported() > 0; }

static void deliver_key(hal_key_t key, bool pressed, uint64_t t_ns) {
    hal_input_event_t e = {.type = HAL_IN_KEY, .t_us = t_ns / 1000};
    e.key.key = key;
    e.key.pressed = pressed;
    sim_metric_sample("input.key_delivery_ms", (vtk_now() - t_ns) / 1e6);
    deliver(&e);
}

/* ------------------------------------------------- FT6336U, register level (the Plus)
 * The controller hands out its two contact slots: releases free them, cover keys take free slots
 * before fingers, and a contact beyond two waits for a free slot. A read from TD_STATUS (0x02)
 * returns the slots in use: event, X, ID, Y in portrait coordinates matching the UI (cover keys at
 * Y 1600, X 100/200/300, metalio_touch.h). Chip ID 0x64 (0xA3), firmware 0x03 (0xA6, the version
 * upstream updates to), INT mode (0xA4). INT stays low while touched; when the controller notices
 * a contact (detection delay, monitor scan) is modelled by the physical part above. */
static struct {
    struct { bool used, key; int idx; uint64_t t_down, t_up; } slot[FT6336U_MAX_POINTS];
    uint8_t ptr, int_mode;
} FT;

static bool ft_down(bool key, int idx) { return key ? IN.vkey[idx].down : IN.phys[idx].down; }
static uint64_t ft_t_change(bool key, int idx) { return key ? IN.vkey[idx].t_change : IN.phys[idx].t_change; }
static int ft_slots(void) { return g_profile.touch_points < FT6336U_MAX_POINTS ? g_profile.touch_points : FT6336U_MAX_POINTS; }

static bool ft_held(bool key, int idx) {
    for (int s = 0; s < ft_slots(); s++)
        if (FT.slot[s].used && FT.slot[s].key == key && FT.slot[s].idx == idx) return true;
    return false;
}

static void ft_take(bool key, int idx) {
    for (int s = 0; s < ft_slots(); s++)
        if (!FT.slot[s].used) {
            FT.slot[s].used = true;
            FT.slot[s].key = key;
            FT.slot[s].idx = idx;
            FT.slot[s].t_down = ft_t_change(key, idx);
            return;
        }
}

static void ft_assign(void) {
    for (int s = 0; s < ft_slots(); s++)
        if (FT.slot[s].used && !ft_down(FT.slot[s].key, FT.slot[s].idx)) {
            FT.slot[s].used = false;
            FT.slot[s].t_up = ft_t_change(FT.slot[s].key, FT.slot[s].idx);
        }
    for (int k = 0; k < HAL_KEY_COUNT; k++)
        if (IN.vkey[k].down && !ft_held(true, k)) ft_take(true, k);
    for (int i = 0; i < MAX_TOUCH; i++)
        if (IN.phys[i].down && !ft_held(false, i)) ft_take(false, i);
}

static void ft_raw(int s, int *x, int *y) {
    if (FT.slot[s].key) {
        hal_key_t k = (hal_key_t)FT.slot[s].idx;
        *x = k == HAL_KEY_HOME ? MT_VK_HOME_X : k == HAL_KEY_PREV ? MT_VK_PREV_X : MT_VK_NEXT_X;
        *y = MT_VK_Y;
    } else {
        *x = IN.phys[FT.slot[s].idx].lx;
        *y = IN.phys[FT.slot[s].idx].ly;
    }
}

static uint8_t ft_reg(uint8_t reg) {
    int used = 0;
    for (int s = 0; s < ft_slots(); s++) used += FT.slot[s].used;
    switch (reg) {
    case 0x02: return (uint8_t)used;
    case 0xA3: return FT6336U_CHIP_ID;
    case 0xA4: return FT.int_mode;
    case 0xA6: return 0x03;
    default: break;
    }
    if (reg < 0x03 || reg >= 0x03 + 6 * FT6336U_MAX_POINTS) return 0;
    int k = (reg - 0x03) / 6, off = (reg - 0x03) % 6, s = -1;
    for (int i = 0, seen = 0; i < ft_slots(); i++)
        if (FT.slot[i].used && seen++ == k) s = i;
    if (s < 0) return 0xFF;
    int x, y;
    ft_raw(s, &x, &y);
    switch (off) {
    case 0: return (uint8_t)((2 << 6) | ((x >> 8) & 0x0F)); /* event: contact */
    case 1: return (uint8_t)x;
    case 2: return (uint8_t)((s << 4) | ((y >> 8) & 0x0F));
    case 3: return (uint8_t)y;
    case 4: return 0x10; /* weight */
    default: return 0;
    }
}

static int ft_xfer(void *state, const uint8_t *w, size_t wlen, uint8_t *r, size_t rlen) {
    (void)state;
    if (sim_board_shared_reset_held()) return HAL_I2C_NACK; /* P12 low holds it in reset */
    if (wlen) FT.ptr = w[0];
    for (size_t i = 1; i < wlen; i++, FT.ptr++)
        if (FT.ptr == 0xA4) FT.int_mode = w[i];
    if (rlen && FT.ptr == 0x02) ft_assign(); /* a frame read */
    for (size_t i = 0; i < rlen; i++, FT.ptr++) r[i] = ft_reg(FT.ptr);
    return HAL_I2C_OK;
}

/* The driver side (the device port does the same with the same drivers): read the points, tell the
 * cover keys from fingers by position, report what changed per contact ID. Events carry the
 * physical time of the action, which the model knows, so latency probes stay end to end. */
static struct { bool down, key; hal_key_t vk; int x, y; } RP[FT6336U_MAX_POINTS];

static void deliver_touch(int id, hal_touch_phase_t phase, int lx, int ly, uint64_t t_ns) {
    hal_input_event_t e = {.type = HAL_IN_TOUCH, .t_us = t_ns / 1000};
    e.touch.id = id;
    e.touch.phase = phase;
    ink_logical_to_native(g_profile.panel.width, g_profile.panel.height, g_profile.panel.rotation_deg, lx, ly, &e.touch.x, &e.touch.y);
    sim_metric_sample("input.touch_delivery_ms", (vtk_now() / 1000 - e.t_us) / 1e3);
    deliver(&e);
}

static int ft_reported(void) {
    int n = 0;
    for (int i = 0; i < FT6336U_MAX_POINTS; i++) n += RP[i].down;
    return n;
}

static void poll_touch_ft(void) {
    ft6336u_point_t pts[FT6336U_MAX_POINTS];
    int n = 0;
    if (ft6336u_read(g_ft, pts, ft_slots(), &n) != HAL_I2C_OK) {
        sim_metric_add("input.touch_read_errors", 1);
        return;
    }
    uint64_t t_read = vtk_now();
    bool seen[FT6336U_MAX_POINTS] = {false};
    for (int i = 0; i < n; i++)
        if (pts[i].id >= 0 && pts[i].id < FT6336U_MAX_POINTS) seen[pts[i].id] = true;
    for (int id = 0; id < FT6336U_MAX_POINTS; id++) /* releases first: they free contacts */
        if (RP[id].down && !seen[id]) {
            RP[id].down = false;
            if (RP[id].key) deliver_key(RP[id].vk, false, FT.slot[id].t_up);
            else deliver_touch(id, HAL_TOUCH_UP, RP[id].x, RP[id].y, FT.slot[id].t_up);
        }
    for (int i = 0; i < n; i++) {
        int id = pts[i].id;
        if (id < 0 || id >= FT6336U_MAX_POINTS) continue;
        hal_key_t vk = metalio_touch_cover_key(pts[i].x, pts[i].y);
        bool panel = metalio_touch_on_panel(pts[i].x, pts[i].y);
        if (!RP[id].down && (vk != HAL_KEY_NONE || panel)) {
            RP[id].down = true;
            RP[id].key = vk != HAL_KEY_NONE;
            RP[id].vk = vk;
            RP[id].x = pts[i].x;
            RP[id].y = pts[i].y;
            if (RP[id].key) deliver_key(vk, true, FT.slot[id].t_down);
            else deliver_touch(id, HAL_TOUCH_DOWN, pts[i].x, pts[i].y, FT.slot[id].t_down);
        } else if (RP[id].down && !RP[id].key && panel && (pts[i].x != RP[id].x || pts[i].y != RP[id].y)) {
            RP[id].x = pts[i].x;
            RP[id].y = pts[i].y;
            deliver_touch(id, HAL_TOUCH_MOVE, pts[i].x, pts[i].y, t_read);
        }
    }
}

static void poll_touch_once(void) {
    if (g_ft) {
        poll_touch_ft();
        return;
    }
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
            /* fingers and cover keys share the controller's points: a finger beyond them waits */
            if (points_reported() >= g_profile.touch_points) continue;
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

/* GPIO2 service on the Plus (the expander's INT and the accelerometer's INT1 share the line): read
 * both expander ports, which releases INT (upstream metalio_keys.cc reads port 0 for that too, as
 * the panel PMIC's PWR_GOOD on P07 raises it with every power cycle), and the accelerometer's latch.
 * Key events come from the levels read, confirmed by a second read after the debounce time; a
 * failed read delivers nothing and is retried. Events carry the time of the edge. */

static uint8_t g_keys_last = MB_P1_KEYS; /* debounced key levels (a bit high = released) */
static uint8_t g_keys_first;             /* levels at the read that started a debounce */
static bool g_keys_pending;
static uint64_t g_keys_t;

static void confirm_later(void) {
    vtk_lock();
    vtk_event_after(SIM_US(g_profile.key_debounce_us), expander_confirm_cb, NULL);
    vtk_unlock();
}

static void deliver_key_changes(uint8_t now) {
    static const struct { uint8_t bit; hal_key_t key; } K[] = {
        {MB_P1_POWER_KEY, HAL_KEY_POWER}, {MB_P1_VOL_UP, HAL_KEY_VOL_UP}, {MB_P1_VOL_DOWN, HAL_KEY_VOL_DOWN}};
    for (unsigned i = 0; i < sizeof K / sizeof K[0]; i++)
        if ((now ^ g_keys_last) & K[i].bit) deliver_key(K[i].key, !(now & K[i].bit), g_keys_t);
    g_keys_last = now;
}

static void service_expander_locked(bool edge, bool confirm);

static void service_expander(bool edge, bool confirm) {
    if (!IN.sample_lock) {
        service_expander_locked(edge, confirm);
        return;
    }
    osal_mutex_lock(IN.sample_lock, OSAL_WAIT_FOREVER);
    if (!IN.sampling_off) service_expander_locked(edge, confirm);
    osal_mutex_unlock(IN.sample_lock);
}

/* Light sleep (the port's sleep sequence, upstream MetalioKeys_MaskForSleep / ResumeAfterSleep):
 * stop reading the keys before the expander is masked, so driving the key pins does not look like
 * presses. After the wake: the key that woke the chip is reported pressed at t_wake_ns unless it
 * already was, then whatever differs between what was reported and the levels now (a key released
 * meanwhile, one held now). Presses and releases that came and went while asleep are lost. Task
 * context. */
void sim_input_sleep_begin(void) {
    if (!IN.sample_lock) return;
    osal_mutex_lock(IN.sample_lock, OSAL_WAIT_FOREVER);
    IN.sampling_off = true;
    osal_mutex_unlock(IN.sample_lock);
}

static void queue_key(hal_key_t key, bool pressed, uint64_t t_ns) { /* locked */
    if (IN.kn >= KEY_FIFO) {
        sim_metric_add("input.key_overflow", 1);
        return;
    }
    hal_input_event_t *e = &IN.keys[(IN.kh + IN.kn++) % KEY_FIFO];
    memset(e, 0, sizeof *e);
    e->type = HAL_IN_KEY;
    e->t_us = t_ns / 1000;
    e->key.key = key;
    e->key.pressed = pressed;
}

/* The expander is back in its normal setup: take the levels now as the baseline, report what
 * differs from what was reported (a key released meanwhile, one held now), read keys again. A failed
 * read: check again after the debounce time. Sample lock held. */
static void keys_resume_locked(void) {
    static const struct { uint8_t bit; hal_key_t key; } K[] = {
        {MB_P1_POWER_KEY, HAL_KEY_POWER}, {MB_P1_VOL_UP, HAL_KEY_VOL_UP}, {MB_P1_VOL_DOWN, HAL_KEY_VOL_DOWN}};
    uint8_t p1 = 0;
    bool ok = metalio_board_read(1, &p1) == HAL_I2C_OK;
    vtk_lock();
    IN.exp_irq = IN.exp_confirm = false; /* edges while masked or asleep: covered here */
    if (ok) {
        uint8_t now = (uint8_t)(p1 & MB_P1_KEYS);
        for (unsigned i = 0; i < sizeof K / sizeof K[0]; i++)
            if ((now ^ g_keys_last) & K[i].bit) queue_key(K[i].key, !(now & K[i].bit), vtk_now());
        g_keys_last = now;
        g_keys_pending = false;
    } else { /* keep what was reported and look again soon: a release meanwhile still gets reported */
        g_keys_first = g_keys_last;
        g_keys_pending = true;
        g_keys_t = vtk_now();
    }
    IN.sampling_off = false;
    gpio2_recheck_locked(); /* an INT left from the masked time */
    vtk_wake_one_locked(&IN.wq);
    vtk_unlock();
    if (!ok) confirm_later();
}

void sim_input_sleep_end(hal_key_t woke_by, uint64_t t_wake_ns, bool keys_back) {
    if (!IN.sample_lock) return;
    osal_mutex_lock(IN.sample_lock, OSAL_WAIT_FOREVER);
    vtk_lock();
    if (woke_by == HAL_KEY_POWER && (g_keys_last & MB_P1_POWER_KEY)) { /* the wake key, whatever the bus does */
        queue_key(HAL_KEY_POWER, true, t_wake_ns);
        g_keys_last &= (uint8_t)~MB_P1_POWER_KEY;
        vtk_wake_one_locked(&IN.wq);
    }
    vtk_unlock();
    if (keys_back) keys_resume_locked(); /* else sampling stays off until sim_input_restore_later succeeds */
    osal_mutex_unlock(IN.sample_lock);
}

static void restore_due_cb(void *arg) { /* ISR: time for another try */
    (void)arg;
    IN.restore_due = true;
    vtk_isr_wake_one(&IN.wq);
}

static void restore_retry_later(void) { /* 50 ms, doubling to one try a second */
    uint64_t gap = SIM_MS(50) << (IN.restore_tries < 5 ? IN.restore_tries : 5);
    if (gap > SIM_MS(1000)) gap = SIM_MS(1000);
    vtk_lock();
    vtk_event_after(gap, restore_due_cb, NULL);
    vtk_unlock();
}

/* After a wake whose restore failed (power HAL, task context): what is still to do; the input task
 * retries until the bus answers. Until the keys are back no key is read from the expander. */
void sim_input_restore_later(bool keys, bool rails) {
    if (!IN.sample_lock) return;
    osal_mutex_lock(IN.sample_lock, OSAL_WAIT_FOREVER);
    IN.restore_keys |= keys;
    IN.restore_rails |= rails;
    IN.restore_tries = 0;
    osal_mutex_unlock(IN.sample_lock);
    restore_retry_later();
}

static void restore_try(void) { /* input task */
    osal_mutex_lock(IN.sample_lock, OSAL_WAIT_FOREVER);
    if (IN.restore_rails && metalio_board_restore_rails() == HAL_I2C_OK) IN.restore_rails = false;
    if (IN.restore_keys && metalio_board_restore_keys(NULL, NULL) == HAL_I2C_OK) {
        IN.restore_keys = false;
        osal_task_delay_us(20000); /* inputs settle before the baseline */
        keys_resume_locked();
    }
    bool more = IN.restore_keys || IN.restore_rails;
    if (more) IN.restore_tries++;
    else if (IN.restore_tries) sim_metric_add("input.expander_restored_late", 1);
    osal_mutex_unlock(IN.sample_lock);
    if (more) restore_retry_later();
}

static void service_expander_locked(bool edge, bool confirm) {
    uint8_t p0, p1;
    if (sim_board_present() && !metalio_board_ready()) {
        /* the board's expander did not set up (bus errors at boot): as on the device, no key can be
         * read until it does; every GPIO2 edge tries again (audit AUD2-012) */
        if (metalio_board_init() != HAL_I2C_OK) {
            sim_metric_add("input.expander_not_set_up", 1);
            return;
        }
        sim_metric_add("input.expander_set_up_late", 1);
        keys_resume_locked(); /* the levels now are the baseline; a key held now is reported */
        return;
    }
    if (!sim_board_present()) { /* no register model: timing only */
        if (edge) {
            sim_i2c_transfer("io-expander", 2);
            confirm_later();
        }
        if (confirm) {
            sim_i2c_transfer("io-expander", 2);
            for (int i = 0; i < IN.exp_n; i++) deliver_key(IN.exp_pending[i].key, IN.exp_pending[i].pressed, IN.exp_pending[i].t);
            IN.exp_n = 0;
        }
        return;
    }
    if (edge) {
        bool ok = metalio_board_read(1, &p1) == HAL_I2C_OK;
        /* reading port 0 releases an INT it raised; PWR_GOOD (P07) low there while the panel should
         * be powered means its supply went away: the display port stops the waveform */
        if (metalio_board_read(0, &p0) == HAL_I2C_OK && !(p0 & EPD_PWR_GOOD)) sim_display_supply_low();
        sim_sensors_service_gpio2(); /* the accelerometer shares GPIO2 */
        if (ok && !g_keys_pending && (p1 & MB_P1_KEYS) != g_keys_last) {
            g_keys_first = p1 & MB_P1_KEYS;
            g_keys_pending = true;
            g_keys_t = IN.gpio2_fell;
            confirm_later();
        } else if (!ok && !g_keys_pending) {
            g_keys_first = g_keys_last; /* unknown: read again after the debounce time */
            g_keys_pending = true;
            g_keys_t = IN.gpio2_fell;
            confirm_later();
        }
    }
    if (confirm && g_keys_pending) {
        if (metalio_board_read(1, &p1) != HAL_I2C_OK) {
            sim_metric_add("input.key_read_errors", 1);
            confirm_later();
            return;
        }
        uint8_t now = p1 & MB_P1_KEYS;
        if (now == g_keys_first) {
            g_keys_pending = false;
            if (now != g_keys_last) deliver_key_changes(now);
        } else { /* still bouncing, or changed again */
            g_keys_first = now;
            confirm_later();
        }
    }
}

static void deliver_gpio_keys(bool sample) {
    while (IN.kn > 0) {
        hal_input_event_t e = IN.keys[IN.kh];
        IN.kh = (IN.kh + 1) % KEY_FIFO;
        IN.kn--;
        if (sample) sim_metric_sample("input.key_delivery_ms", (vtk_now() / 1000 - e.t_us) / 1e3);
        deliver(&e);
    }
}

static void input_task(void *arg) {
    (void)arg;
    for (;;) {
        vtk_lock();
        while (!IN.irq && IN.kn == 0 && !IN.exp_irq && !IN.exp_confirm && !IN.restore_due) vtk_block_locked(&IN.wq, VTK_TIME_INF);
        bool irq = IN.irq, exp_irq = IN.exp_irq, exp_confirm = IN.exp_confirm, restore = IN.restore_due;
        IN.irq = IN.exp_irq = IN.exp_confirm = IN.restore_due = false;
        vtk_unlock();
        if (restore) restore_try();
        if (exp_irq || exp_confirm) service_expander(exp_irq, exp_confirm);
        if (exp_irq || exp_confirm || restore) {
            vtk_lock();
            gpio2_recheck_locked();
            vtk_unlock();
        }
        deliver_gpio_keys(true);
        if (!irq) continue;
        /* touch session: read, then poll while any finger is down; keys keep being served */
        do {
            poll_touch_once();
            if (!any_reported_down()) break;
            osal_task_delay_us((uint64_t)g_profile.touch_poll_ms * 1000ull);
            vtk_lock();
            exp_irq = IN.exp_irq;
            exp_confirm = IN.exp_confirm;
            restore = IN.restore_due; /* a finger held down must not hold the expander's restore back */
            IN.exp_irq = IN.exp_confirm = IN.restore_due = false;
            vtk_unlock();
            if (restore) restore_try();
            if (exp_irq || exp_confirm || restore) {
                service_expander(exp_irq, exp_confirm);
                vtk_lock();
                gpio2_recheck_locked();
                vtk_unlock();
            }
            deliver_gpio_keys(false);
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
    if (sim_board_present() && g_profile.has_touch) { /* the Plus: FT6336U on the shared bus */
        static const sim_i2c_model_t ft = {ft_xfer};
        sim_i2c_attach(0, FT6336U_ADDR, &ft, NULL);
        g_ft = hal_i2c_device(0, FT6336U_ADDR, "touch");
        uint8_t fw = 0;
        if (ft6336u_init(g_ft, &fw) != HAL_I2C_OK) {
            sim_warn("input: FT6336U did not answer as expected");
            g_ft = NULL;
        }
    }
    if (sim_board_present() && !(IN.sample_lock = osal_mutex_create())) osal_panic("input driver init failed: no memory");
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
