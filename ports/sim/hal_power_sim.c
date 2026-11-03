/*
 * Power model: resource reference counts, front light, battery drain and an
 * energy estimate per scenario. Currents are ESTIMATES from the profile until
 * bench measurements exist (SP-14) — use the result to compare designs, not
 * to promise battery life.
 *
 * CPU clock: full speed while the system is active or anything holds a CPU
 * boost (esp_pm CPU_FREQ_MAX lock), otherwise the idle frequency; CPU work
 * charged in cycles takes longer at the lower clock and the base current
 * follows the clock. No automatic light sleep (upstream keeps it off on the
 * S31: tickless idle disabled, light_sleep_enable false).
 *
 * Standby is an explicit light sleep (hal_power_sleep, upstream
 * esp_light_sleep_start in 60 s slices): entry sequence (panel PMIC to sleep,
 * SD park, rails), every other task suspended, only armed wake sources end
 * it (after the chip's wake latency), then the restore sequence. Input from
 * other sources is lost while asleep.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ink/hal_device.h"
#include "ink/hal_power.h"
#include "sim.h"

#define MAX_CONSUMERS 24

typedef struct {
    const char *name;
    bool on;
    double ma;
    uint64_t since, on_ns;
    double mah;
} consumer_t;

static struct {
    consumer_t c[MAX_CONSUMERS];
    int n;
    int refs[HAL_PWR_COUNT];
    double soc; /* state of charge 0..1 */
    bool charging;
    int light_cold, light_warm;
    double cpu_mah;
    /* clock and sleep */
    hal_power_state_t state;
    uint64_t state_since, state_ns[3];
    double state_mah[3], mah_mark; /* energy per system state (incl. refreshes and wake windows) */
    int boosts;
    bool sleeping;
    uint32_t wake_mask, wake_cause;
    vtk_waitq_t sleep_wq;
    vtk_event_id_t slice_ev;
    uint64_t sleep_t0, sleep_ns, sleeps, wakes_by[8], lost_inputs;
} PW;

int g_sim_mhz;

static consumer_t *find(const char *name) {
    for (int i = 0; i < PW.n; i++)
        if (!strcmp(PW.c[i].name, name)) return &PW.c[i];
    if (PW.n >= MAX_CONSUMERS) return &PW.c[MAX_CONSUMERS - 1];
    consumer_t *c = &PW.c[PW.n++];
    memset(c, 0, sizeof *c);
    c->name = name;
    return c;
}

static double default_ma(const char *name) {
    if (!strcmp(name, "display.rails")) return g_profile.ma_display_rails;
    if (!strcmp(name, "modem")) return g_profile.ma_lte_idle;
    if (!strcmp(name, "wifi")) return g_profile.ma_wifi_idle;
    if (!strcmp(name, "audio.out")) return g_profile.ma_audio_out;
    if (!strcmp(name, "audio.in")) return g_profile.ma_audio_in;
    return 0;
}

static uint64_t now_ns(void) { return vtk_in_isr() ? vtk_isr_now() : vtk_now(); }

static void settle(consumer_t *c, uint64_t t) {
    if (c->on && t > c->since) {
        double h = (double)(t - c->since) / 3.6e12;
        c->mah += c->ma * h;
        c->on_ns += t - c->since;
    }
    c->since = t;
}

void sim_power_consumer(const char *name, bool on, double ma) {
    consumer_t *c = find(name);
    uint64_t t = now_ns();
    settle(c, t);
    c->on = on;
    c->ma = ma > 0 ? ma : default_ma(name);
}

static const char *RES_NAME[HAL_PWR_COUNT] = {"display", "audio.out", "audio.in", "modem", "wifi"};

void hal_power_request(hal_power_res_t res, bool on) {
    if (res >= HAL_PWR_COUNT) return;
    int before = PW.refs[res];
    PW.refs[res] += on ? 1 : -1;
    if (PW.refs[res] < 0) {
        sim_warn("hal_power_request: unbalanced release of %s", RES_NAME[res]);
        PW.refs[res] = 0;
    }
    if ((before == 0) != (PW.refs[res] == 0) && res != HAL_PWR_DISPLAY)
        sim_power_consumer(RES_NAME[res], PW.refs[res] > 0, 0);
}

void hal_light_set(int cold_permille, int warm_permille) {
    if (!g_profile.info.has_frontlight) return;
    PW.light_cold = cold_permille < 0 ? 0 : cold_permille > 1000 ? 1000 : cold_permille;
    PW.light_warm = warm_permille < 0 ? 0 : warm_permille > 1000 ? 1000 : warm_permille;
    double frac = (PW.light_cold + PW.light_warm) / 2000.0;
    sim_power_consumer("frontlight", frac > 0, g_profile.ma_light_full * frac);
}

bool hal_haptic_available(void) { return g_profile.info.has_haptics; }
void hal_haptic_pulse(int ms) {
    (void)ms;
    if (g_profile.info.has_haptics) sim_metric_add("haptics.pulses", 1);
}

static double total_mah(void) {
    uint64_t t = now_ns();
    double sum = 0;
    for (int i = 0; i < PW.n; i++) {
        settle(&PW.c[i], t);
        sum += PW.c[i].mah;
    }
    /* CPU base current is the "cpu.base" consumer (follows the clock, off while asleep); here the
     * per-core busy share on top (at full-clock current: pessimistic) */
    double busy_h = 0;
    for (int c = 0; c < g_profile.cores; c++) busy_h += (double)vtk_core_busy_ns(c) / 3.6e12;
    PW.cpu_mah = g_profile.ma_per_core_busy * busy_h;
    return sum + PW.cpu_mah;
}

/* ------------------------------------------------------------ clock */

static void apply_clock(void) { /* kernel lock not required: consumers settle with the current time */
    bool full = PW.state == HAL_PWR_STATE_ACTIVE || PW.boosts > 0;
    int mhz = full ? g_profile.mhz : g_profile.idle_mhz;
    g_sim_mhz = mhz;
    if (!PW.sleeping)
        sim_power_consumer("cpu.base", true, mhz == g_profile.mhz ? g_profile.ma_base_awake : g_profile.ma_base_awake_idle);
}

static void account_state(uint64_t t) { /* t = now: total_mah() settles every consumer to now */
    if (t > PW.state_since) PW.state_ns[PW.state] += t - PW.state_since;
    PW.state_since = t;
    double m = total_mah();
    PW.state_mah[PW.state] += m - PW.mah_mark;
    PW.mah_mark = m;
}

void sim_input_set_power_state(hal_power_state_t s);

void hal_power_set_state(hal_power_state_t s) {
    if ((unsigned)s > HAL_PWR_STATE_LOCKED) return;
    account_state(now_ns());
    PW.state = s;
    apply_clock();
    sim_input_set_power_state(s);
}

hal_power_state_t sim_power_state(void) { return PW.state; }

struct hal_cpu_boost {
    char name[24];
    bool held;
};

hal_cpu_boost_t *hal_cpu_boost_create(const char *name) {
    hal_cpu_boost_t *b = sim_host_calloc(1, sizeof *b);
    if (!b) abort();
    sim_copy_name(b->name, sizeof b->name, name ? name : "boost");
    return b;
}

void hal_cpu_boost_acquire(hal_cpu_boost_t *b) {
    if (!b || b->held) return;
    b->held = true;
    PW.boosts++;
    apply_clock();
}

void hal_cpu_boost_release(hal_cpu_boost_t *b) {
    if (!b || !b->held) return;
    b->held = false;
    PW.boosts--;
    apply_clock();
}

/* ------------------------------------------------------------ light sleep */

bool sim_power_sleeping(void) { return PW.sleeping; }
bool sim_power_waking(void) { return PW.sleeping && PW.wake_cause != 0; }

static void wake_cb(void *arg) { /* ISR: the chip is awake: the sleeper continues */
    (void)arg;
    vtk_isr_wake_one(&PW.sleep_wq);
}

bool sim_power_wake(uint32_t cause) { /* ISR */
    if (!PW.sleeping || !(PW.wake_mask & cause) || PW.wake_cause) return false;
    PW.wake_cause = cause;
    if (PW.slice_ev) vtk_event_cancel(PW.slice_ev);
    PW.slice_ev = 0;
    vtk_event_at(vtk_isr_now() + SIM_US(g_profile.sleep_wake_us), wake_cb, NULL);
    return true;
}

void sim_power_input_lost(void) { PW.lost_inputs++; }

static void slice_cb(void *arg) { /* ISR: the sleep timer expired */
    (void)arg;
    PW.slice_ev = 0;
    sim_power_wake(HAL_WAKE_TIMER);
}

uint32_t hal_power_sleep(const hal_sleep_cfg_t *cfg) {
    if (vtk_in_isr() || !vtk_self()) osal_panic("hal_power_sleep must be called from a task");
    if (!cfg) return 0;
    /* refuse while something is still busy (the system must quiesce first) */
    if (hal_display_pending() > 0 || sim_display_playing() || sim_audio_busy()) {
        sim_metric_add("power.sleep_refused", 1);
        return 0;
    }
    uint64_t t_enter = vtk_now();
    osal_task_delay_us(g_profile.sleep_enter_us); /* PMIC to sleep, SD parked, rails cut */
    /* other tasks ran during the entry sequence: anything they started must finish awake (the
     * real port checks again with the scheduler suspended, right before esp_light_sleep_start) */
    if (hal_display_pending() > 0 || sim_display_playing() || sim_audio_busy()) {
        sim_metric_add("power.sleep_aborted", 1);
        osal_task_delay_us(g_profile.sleep_exit_us);
        return 0;
    }
    if (cfg->cancel && cfg->cancel(cfg->cancel_ctx)) { /* e.g. unlocked during the entry sequence */
        sim_metric_add("power.sleep_cancelled", 1);
        osal_task_delay_us(g_profile.sleep_exit_us);
        return 0;
    }
    uint32_t wake_mask = cfg->wake_mask | (cfg->max_us ? HAL_WAKE_TIMER : 0);
    if (!cfg->keep_modem) wake_mask &= ~(uint32_t)HAL_WAKE_MODEM;
    vtk_lock();
    uint64_t t0 = vtk_now();
    PW.sleeping = true;
    PW.sleep_t0 = t0;
    PW.wake_cause = 0;
    PW.wake_mask = wake_mask;
    vtk_set_sleeper_locked(vtk_self());
    if (cfg->max_us) PW.slice_ev = vtk_event_at(t0 + cfg->max_us * 1000ull, slice_cb, NULL);
    vtk_unlock();
    /* rails cut: every consumer stops drawing (the modem, the Wi-Fi radio ...) and is restored
     * after the wake; answerable standby keeps the 4G rail: the module idles at its paging
     * current plus whatever shares that rail (modem.standby replaces the awake modem current) */
    bool parked[MAX_CONSUMERS] = {false};
    double parked_ma[MAX_CONSUMERS] = {0};
    for (int i = 0; i < PW.n; i++)
        if (PW.c[i].on) {
            parked[i] = true;
            parked_ma[i] = PW.c[i].ma;
            sim_power_consumer(PW.c[i].name, false, 0);
        }
    sim_power_consumer("cpu.sleep", true, g_profile.ma_sleep);
    if (cfg->keep_modem && g_profile.info.has_modem) sim_power_consumer("modem.standby", true, g_profile.ma_standby_modem);
    sim_trace_instant("power", "sleep", t0, NULL);
    vtk_lock();
    while (!PW.wake_cause) vtk_block_locked(&PW.sleep_wq, VTK_TIME_INF);
    uint32_t cause = PW.wake_cause;
    uint64_t t1 = vtk_now();
    PW.sleeping = false;
    PW.wake_mask = 0;
    vtk_set_sleeper_locked(NULL);
    vtk_unlock();
    sim_power_consumer("cpu.sleep", false, 0);
    sim_power_consumer("modem.standby", false, 0);
    for (int i = 0; i < PW.n; i++)
        if (parked[i] && strcmp(PW.c[i].name, "cpu.base") != 0) sim_power_consumer(PW.c[i].name, true, parked_ma[i]);
    apply_clock(); /* cpu.base at the current clock */
    PW.sleep_ns += t1 - t0;
    PW.sleeps++;
    for (int i = 0; i < 8; i++)
        if (cause & (1u << i)) PW.wakes_by[i]++;
    sim_trace_span("power", "light sleep", t0, t1, NULL);
    osal_task_delay_us(g_profile.sleep_exit_us); /* rails back, key baseline */
    if (cause != HAL_WAKE_TIMER) osal_task_delay_us(g_profile.sd_remount_us);
    sim_trace_span("power", "standby enter+exit", t_enter, vtk_now(), NULL);
    return cause;
}

void hal_power_battery(hal_battery_t *out) {
    double used = total_mah();
    double soc = PW.soc - (g_profile.batt_mah > 0 ? used / g_profile.batt_mah : 0);
    if (soc < 0) soc = 0;
    out->percent = (int)(soc * 100.0 + 0.5);
    out->millivolts = 3300 + (int)(soc * 900.0); /* crude linear OCV */
    out->charging = PW.charging;
    out->external_power = PW.charging;
}

void sim_power_set_battery(int percent, bool charging) {
    double used = total_mah();
    PW.soc = percent / 100.0 + (g_profile.batt_mah > 0 ? used / g_profile.batt_mah : 0);
    PW.charging = charging;
}

void sim_power_init(void) {
    PW.soc = 0.8;
    find("display.rails");
    PW.state = HAL_PWR_STATE_ACTIVE;
    apply_clock();
}

void sim_power_report(void) {
    double total = total_mah();
    double h = (double)vtk_now() / 3.6e12;
    char key[80];
    for (int i = 0; i < PW.n; i++) {
        snprintf(key, sizeof key, "power.mah.%s", PW.c[i].name);
        sim_metric_set(key, PW.c[i].mah);
    }
    sim_metric_set("power.mah.cpu_busy", PW.cpu_mah);
    sim_metric_set("power.mah_total", total);
    sim_metric_set("power.avg_ma_estimate", h > 0 ? total / h : 0);
    account_state(now_ns());
    static const char *SN[3] = {"active", "idle", "locked"};
    for (int i = 0; i < 3; i++) {
        snprintf(key, sizeof key, "power.state_ms.%s", SN[i]);
        sim_metric_set(key, PW.state_ns[i] / 1e6);
        if (PW.state_ns[i] > 0) { /* average current in this state: e.g. "locked" = the standby current */
            snprintf(key, sizeof key, "power.avg_ma.%s", SN[i]);
            sim_metric_set(key, PW.state_mah[i] / (PW.state_ns[i] / 3.6e12));
        }
    }
    uint64_t slept = PW.sleep_ns + (PW.sleeping ? now_ns() - PW.sleep_t0 : 0); /* the run may end asleep */
    sim_metric_set("power.sleep_ms", slept / 1e6);
    if (PW.state_ns[HAL_PWR_STATE_LOCKED] > 0)
        sim_metric_set("power.locked_sleep_share", (double)slept / (double)PW.state_ns[HAL_PWR_STATE_LOCKED]);
    sim_metric_set("power.sleeps", (double)PW.sleeps);
    static const char *WN[5] = {"timer", "power_key", "ai_key", "rtc", "modem"};
    for (int i = 0; i < 5; i++)
        if (PW.wakes_by[i]) {
            snprintf(key, sizeof key, "power.wakes.%s", WN[i]);
            sim_metric_set(key, (double)PW.wakes_by[i]);
        }
    if (PW.lost_inputs) sim_metric_set("power.inputs_lost_asleep", (double)PW.lost_inputs);
}

/* ------------------------------------------------------------ device */

void hal_device_get_info(hal_device_info_t *out) { *out = g_profile.info; }

const char *hal_device_reset_reason(void) { return g_sim.reset_reason ? g_sim.reset_reason : "poweron"; }

static void log_on_host(void *line) {
    uint64_t t = now_ns();
    fprintf(stdout, "[%6llu.%03llu] %s\n", (unsigned long long)(t / 1000000000ull),
            (unsigned long long)(t / 1000000ull % 1000ull), (const char *)line);
}

void hal_log_write(const char *line) {
    if (g_sim.quiet) return;
    vtk_host_call(log_on_host, (void *)line); /* host stdout, not the caller's stack (--quiet must not change it) */
}
