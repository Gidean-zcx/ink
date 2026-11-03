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
 * esp_light_sleep_start in 60 s slices). On the Plus the entry and exit run
 * through our drivers in upstream's order (power_hw_light_sleep_once
 * @690a023), so the chip models see every step: panel PMIC to SLEEP, the
 * accelerometer quiet, key reads stopped and the expander masked, GPIO2 idle,
 * the wake inputs armed (P17; P16 when the RTC may wake), the front light off,
 * P05 and P14 cut (P14 kept for answerable standby); then every other task
 * is suspended and only an armed line ends it (after the chip's wake latency):
 * the timer, GPIO2, the AI key, the modem's SRDY. After it: what woke it is read
 * from the expander, the keys come back (settle, baseline, the wake key
 * reported), P14 then P05, the accelerometer set up again. Peripherals still
 * busy (display, audio, camera, motor, amplifier) refuse the sleep. While
 * asleep each consumer draws what its hardware state says. Without the board
 * model a fixed entry and exit time stand in for all of it.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "metalio_plus/metalio_board.h"
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
    unsigned haptic_pulses;
    uint64_t haptic_until;
    double chg_ma, chg_mah; /* current into the battery from the charger (dev_power_chips.c) */
    uint64_t chg_since;
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
    uint64_t sleep_t0, sleep_ns, sleeps, wakes_by[8], lost_inputs, wakes_unexplained, wake_t;
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
static void battery_model(hal_battery_t *out);

/* The motor hangs on TCA9555 P03: a small task switches it on, waits, and off (upstream uses an
 * esp_timer callback for the same I2C writes). */
static osal_queue_t *g_haptic_q;

static void haptic_task(void *arg) {
    (void)arg;
    for (;;) {
        int ms;
        if (osal_queue_recv(g_haptic_q, &ms, OSAL_WAIT_FOREVER) != OSAL_OK) continue;
        metalio_board_set(0, MB_P0_MOTOR, MB_P0_MOTOR);
        osal_task_delay_us((uint64_t)ms * 1000u);
        metalio_board_set(0, MB_P0_MOTOR, 0);
    }
}

void hal_haptic_pulse(int ms) {
    if (!g_profile.info.has_haptics) return;
    if (ms <= 0) ms = MB_MOTOR_PULSE_MS;
    sim_metric_add("haptics.pulses", 1);
    PW.haptic_pulses++;
    uint64_t until = vtk_now() + SIM_MS(ms);
    if (until > PW.haptic_until) PW.haptic_until = until;
    if (!metalio_board_ready()) return;
    if (!g_haptic_q) {
        const char *prev = osal_mem_tag("haptics");
        g_haptic_q = osal_queue_create(sizeof(int), 4);
        osal_task_cfg_t c = {.name = "haptic", .fn = haptic_task, .stack_bytes = 2048, .priority = 5, .core = -1};
        if (!g_haptic_q || osal_task_create(&c, NULL) != OSAL_OK) osal_panic("haptics: cannot start the motor task");
        osal_mem_tag(prev);
    }
    osal_queue_send(g_haptic_q, &ms, 0);
}

static const char *g_off_reason;

void sim_power_off(const char *reason) { /* any context: the power chip cut the battery */
    if (g_off_reason) return;
    g_off_reason = reason;
    sim_metric_set("power.off", 1);
    sim_note("power off: %s", reason);
    sim_trace_instant("power", "POWER OFF", vtk_in_isr() ? vtk_isr_now() : vtk_now(), NULL);
    vtk_stop();
}

const char *sim_power_off_reason(void) { return g_off_reason; }

void hal_power_shutdown(void) {
    if (metalio_board_ready()) metalio_power_off(); /* the expander model cuts power at the last pulse */
    sim_power_off("shutdown");
}

void sim_power_gui_state(sim_power_gui_t *out) { /* ISR context (desktop simulator) */
    hal_battery_t b;
    battery_model(&b);
    out->percent = b.percent;
    out->millivolts = b.millivolts;
    out->charging = b.charging;
    out->battery_ma = sim_battery_ma();
    out->charge_ma = (int)PW.chg_ma;
    out->light_cold = PW.light_cold;
    out->light_warm = PW.light_warm;
    out->state = (int)PW.state;
    out->sleeping = sim_power_sleeping();
    out->haptic_pulses = PW.haptic_pulses;
    out->haptic_on = sim_board_present() ? sim_board_motor_on() : vtk_isr_now() < PW.haptic_until;
}

static void chg_settle(uint64_t t) {
    if (t > PW.chg_since) PW.chg_mah += PW.chg_ma * (double)(t - PW.chg_since) / 3.6e12;
    PW.chg_since = t;
}

static double total_mah(void) {
    uint64_t t = now_ns();
    double sum = 0;
    chg_settle(t);
    for (int i = 0; i < PW.n; i++) {
        settle(&PW.c[i], t);
        sum += PW.c[i].mah;
    }
    /* CPU base current is the "cpu.base" consumer (follows the clock, off while asleep); here the
     * per-core busy share on top (at full-clock current: pessimistic) */
    double busy_h = 0;
    for (int c = 0; c < g_profile.cores; c++) busy_h += (double)vtk_core_busy_ns(c) / 3.6e12;
    PW.cpu_mah = g_profile.ma_per_core_busy * busy_h;
    return sum + PW.cpu_mah - PW.chg_mah;
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

/* ------------------------------------------------------------ light sleep
 * The chip's wake logic (upstream: EXT1 ANY_LOW on GPIO2, the timer): what the sleep armed and what
 * fired. Direct lines are the timer, the AI key (GPIO61) and the modem (SRDY, GPIO3); on the Plus the
 * POWER key and the RTC both come through GPIO2 (the expander's INT), so which one it was is read from
 * the expander after the wake. Without the board model POWER is a direct line too. */
#define WAKE_GPIO2 (1u << 16) /* GPIO2 low: classified after the wake */

bool sim_power_sleeping(void) { return PW.sleeping; }
bool sim_power_waking(void) { return PW.sleeping && PW.wake_cause != 0; }

static void wake_cb(void *arg) { /* ISR: the chip is awake: the sleeper continues */
    (void)arg;
    vtk_isr_wake_one(&PW.sleep_wq);
}

static bool start_wake(uint32_t line) { /* ISR or locked */
    if (!PW.sleeping || !(PW.wake_mask & line) || PW.wake_cause) return false;
    PW.wake_cause = line;
    PW.wake_t = now_ns();
    if (PW.slice_ev) vtk_event_cancel(PW.slice_ev);
    PW.slice_ev = 0;
    vtk_event_at(PW.wake_t + SIM_US(g_profile.sleep_wake_us), wake_cb, NULL);
    return true;
}

bool sim_power_wake(uint32_t cause) { return start_wake(cause); } /* a direct wake line; ISR or locked */
void sim_power_gpio2_low(void) { start_wake(WAKE_GPIO2); }       /* EXT1 sees GPIO2 low; ISR or locked */

void sim_power_input_lost(void) { PW.lost_inputs++; }

static void slice_cb(void *arg) { /* ISR: the sleep timer expired */
    (void)arg;
    PW.slice_ev = 0;
    start_wake(HAL_WAKE_TIMER);
}

/* Peripherals that must be idle before the rails go (upstream: no active capture, shared-power
 * operation or display work); the system quiesces them, the HAL refuses otherwise. */
static const char *busy_reason(void) {
    if (hal_display_pending() > 0 || sim_display_playing()) return "display";
    if (sim_audio_busy()) return "audio";
    if (sim_camera_busy()) return "camera";
    if (metalio_board_ready()) {
        uint8_t p0 = metalio_board_output(0);
        if (p0 & MB_P0_MOTOR) return "motor";
        if (p0 & MB_P0_AMP) return "amplifier";
    }
    return NULL;
}

static void refused(const char *metric, const char *why) {
    char key[64];
    sim_metric_add(metric, 1);
    snprintf(key, sizeof key, "%s.%s", metric, why);
    sim_metric_add(key, 1);
}

/* The Plus, the board part of the entry in upstream's order (power_hw_light_sleep_once @690a023),
 * through our drivers: the models see every step. *applied tells the exit what to undo. */
enum { APPLIED_KEYS = 1, APPLIED_RAILS = 2 };

static const char *board_enter(const hal_sleep_cfg_t *cfg, bool power_key, bool rtc, int *applied) {
    sim_display_park();                           /* TPS65185 to SLEEP (upstream tps_enter_sleep) */
    osal_task_delay_us(g_profile.sleep_enter_us); /* SD park and the display bus held low: not modelled */
    sim_sensors_sleep();                          /* the accelerometer lets go of GPIO2 */
    sim_input_sleep_begin();                      /* no key reads from here: the pins are driven next */
    *applied |= APPLIED_KEYS;
    if (metalio_board_mask_for_sleep(rtc) != HAL_I2C_OK) return "expander";
    for (int i = 0; sim_gpio2_low(); i++) { /* upstream WaitIntLineHigh: 30 x 20 ms */
        if (i == 30) return "gpio2_low";
        osal_task_delay_us(20000);
    }
    if (metalio_board_arm_wake(power_key, rtc) != HAL_I2C_OK) return "expander";
    hal_light_set(0, 0); /* upstream ForceAllLow before the rails go; it stays off after the wake */
    *applied |= APPLIED_RAILS;
    if (metalio_board_cut_rails(cfg->keep_modem) != HAL_I2C_OK) return "expander";
    return NULL;
}

/* After the wake or an aborted entry, upstream's order: keys (inputs back, settle, baseline), rails
 * P14 then P05, then the rest. woke_by: the key reported as the cause (HAL_KEY_NONE: none). */
#define RESTORE_TRIES 3
#define RESTORE_GAP_US 10000

static bool restore_keys(void) { return metalio_board_restore_keys(NULL, NULL) == HAL_I2C_OK; }
static bool restore_rails(void) { return metalio_board_restore_rails() == HAL_I2C_OK; }

static bool with_retries(bool (*step)(void)) { /* a short bus fault must not leave the sleep setup behind */
    for (int i = 0; i < RESTORE_TRIES; i++) {
        if (i) osal_task_delay_us(RESTORE_GAP_US);
        if (step()) return true;
        sim_metric_add("power.sleep_restore_retries", 1);
    }
    return false;
}

static void board_exit(int applied, hal_key_t woke_by) {
    bool keys = true, rails = true;
    if (applied & APPLIED_KEYS) {
        keys = with_retries(restore_keys);
        if (keys) osal_task_delay_us(20000); /* upstream: inputs settle before the key baseline */
        sim_input_sleep_end(woke_by, PW.wake_t, keys); /* keys not back: no key reads until they are */
    }
    if (applied & APPLIED_RAILS) rails = with_retries(restore_rails);
    if (!keys || !rails) { /* the bus stayed bad: the input task keeps trying (sim_input_restore_later) */
        sim_metric_add("power.sleep_restore_errors", 1);
        sim_input_restore_later(!keys, !rails);
    }
    sim_sensors_resume();
}

/* GPIO2 woke the chip: read the wake inputs before their directions change (upstream latches P17
 * first). The RTC's INT stays low until its flag is cleared; a POWER press may already be over, and
 * with the RTC quiet nothing else can pull the line, so that is POWER (upstream: a released short
 * press keeps its POWER-wake meaning). With POWER masked only P16 can pull the line: RTC. */
static uint32_t classify_gpio2(bool power_key, bool rtc) {
    uint8_t p1 = 0xFF;
    bool ok = metalio_board_read(1, &p1) == HAL_I2C_OK;
    uint32_t why = 0;
    if (rtc && ok && !(p1 & MB_P1_RTC_INT)) why |= HAL_WAKE_RTC;
    if (power_key && (!ok || !(p1 & MB_P1_POWER_KEY) || !why)) why |= HAL_WAKE_POWER_KEY;
    if (!why) {
        PW.wakes_unexplained++;
        why = HAL_WAKE_RTC;
    }
    return why;
}

/* While the chip sleeps, current follows the hardware: the chip's CPU and radio stop (the Wi-Fi
 * link and the link radio, "wifi" and "modem"), what hangs on a rail that was cut stops, the 4G module
 * pages at its standby current while P14 stays up ("modem.standby"), anything else left on keeps
 * drawing. Without the board model every consumer stops, as before. */
static bool stops_asleep(const char *name, bool board) {
    if (!board || !strcmp(name, "cpu.base") || !strcmp(name, "wifi") || !strcmp(name, "modem")) return true;
    if (!strncmp(name, "audio.", 6)) return !sim_board_shared_power(); /* the BH1098G and amplifier on P14 */
    return false;
}

/* P16 (the RTC's INT) idles high: only then can it wake the chip. Upstream never arms it; on the board
 * in hand the factory log reads it low with the RTC idle (see dev_tca9555.c), so check. */
static bool rtc_line_idle(void) {
    uint8_t p1 = 0;
    return metalio_board_read(1, &p1) == HAL_I2C_OK && (p1 & MB_P1_RTC_INT);
}

uint32_t hal_power_sleep(const hal_sleep_cfg_t *cfg) {
    if (vtk_in_isr() || !vtk_self()) osal_panic("hal_power_sleep must be called from a task");
    if (!cfg) return 0;
    const char *busy = busy_reason();
    if (busy) { /* the system must quiesce first */
        refused("power.sleep_busy", busy); /* (power.sleep_refused is the system's count of 0 returns) */
        return 0;
    }
    uint64_t t_enter = vtk_now();
    bool board = metalio_board_ready();
    uint32_t lines = cfg->wake_mask | (cfg->max_us ? HAL_WAKE_TIMER : 0);
    if (!cfg->keep_modem) lines &= ~(uint32_t)HAL_WAKE_MODEM;
    bool power_key = board && (lines & HAL_WAKE_POWER_KEY), rtc = board && (lines & HAL_WAKE_RTC);
    if (board) lines = (lines & ~(uint32_t)(HAL_WAKE_POWER_KEY | HAL_WAKE_RTC)) | (power_key || rtc ? WAKE_GPIO2 : 0);
    if (rtc && !rtc_line_idle()) { /* a line that is already low cannot signal: the timer bounds the wait */
        rtc = false;
        sim_metric_add("power.rtc_wake_unavailable", 1);
        if (!power_key) lines &= ~(uint32_t)WAKE_GPIO2;
    }
    int applied = 0;
    const char *why_not = NULL;
    if (board) why_not = board_enter(cfg, power_key, rtc, &applied);
    else osal_task_delay_us(g_profile.sleep_enter_us); /* PMIC to sleep, SD parked, rails cut */
    /* other tasks ran during the entry: anything they started must finish awake (the real port
     * checks again with the scheduler suspended, right before esp_light_sleep_start) */
    if (!why_not && (busy = busy_reason())) why_not = busy;
    if (why_not) {
        refused("power.sleep_aborted", why_not);
        if (board) board_exit(applied, HAL_KEY_NONE);
        osal_task_delay_us(g_profile.sleep_exit_us);
        return 0;
    }
    if (cfg->cancel && cfg->cancel(cfg->cancel_ctx)) { /* e.g. unlocked during the entry sequence */
        sim_metric_add("power.sleep_cancelled", 1);
        if (board) board_exit(applied, HAL_KEY_NONE);
        osal_task_delay_us(g_profile.sleep_exit_us);
        return 0;
    }
    vtk_lock();
    uint64_t t0 = vtk_now();
    PW.sleeping = true;
    PW.sleep_t0 = t0;
    PW.wake_cause = 0;
    PW.wake_mask = lines;
    vtk_set_sleeper_locked(vtk_self());
    if (cfg->max_us) PW.slice_ev = vtk_event_at(t0 + cfg->max_us * 1000ull, slice_cb, NULL);
    if (sim_gpio2_low()) start_wake(WAKE_GPIO2); /* EXT1 is level triggered: already low wakes at once */
    vtk_unlock();
    bool parked[MAX_CONSUMERS] = {false};
    double parked_ma[MAX_CONSUMERS] = {0};
    for (int i = 0; i < PW.n; i++)
        if (PW.c[i].on && stops_asleep(PW.c[i].name, board)) {
            parked[i] = true;
            parked_ma[i] = PW.c[i].ma;
            sim_power_consumer(PW.c[i].name, false, 0);
        }
    sim_power_consumer("cpu.sleep", true, g_profile.ma_sleep);
    bool modem_rail = board ? sim_board_shared_power() : cfg->keep_modem;
    if (modem_rail && g_profile.info.has_modem) sim_power_consumer("modem.standby", true, g_profile.ma_standby_modem);
    sim_trace_instant("power", "sleep", t0, NULL);
    vtk_lock();
    while (!PW.wake_cause) vtk_block_locked(&PW.sleep_wq, VTK_TIME_INF);
    uint32_t cause = PW.wake_cause;
    uint64_t t1 = vtk_now();
    PW.sleeping = false;
    PW.wake_mask = 0;
    vtk_set_sleeper_locked(NULL);
    vtk_unlock();
    if (cause == WAKE_GPIO2) cause = classify_gpio2(power_key, rtc); /* the scheduler runs again (upstream too) */
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
    if (board) board_exit(applied, cause & HAL_WAKE_POWER_KEY ? HAL_KEY_POWER : HAL_KEY_NONE);
    osal_task_delay_us(g_profile.sleep_exit_us); /* the display bus re-initialised */
    if (cause != HAL_WAKE_TIMER) osal_task_delay_us(g_profile.sd_remount_us);
    sim_trace_span("power", "standby enter+exit", t_enter, vtk_now(), NULL);
    return cause;
}

/* ------------------------------------------------------------ battery
 * State of charge from the energy the consumers drew minus what the charger put in. Terminal
 * voltage: the official 424860-2000 discharge curve measured at ~1 A (bq27220_gauge.cc @690a023),
 * corrected by an internal resistance (estimate) for the actual current. */
static const int BATT_CURVE_1A[11] = {3300, 3481, 3565, 3624, 3663, 3702, 3757, 3828, 3919, 4021, 4211};
#define BATT_R_MOHM 150 /* (est.) */

double sim_battery_soc(void) {
    double used = total_mah();
    double soc = PW.soc - (g_profile.batt_mah > 0 ? used / g_profile.batt_mah : 0);
    return soc < 0 ? 0 : soc > 1 ? 1 : soc;
}

double sim_power_load_ma(void) {
    uint64_t t = now_ns();
    double ma = 0;
    for (int i = 0; i < PW.n; i++)
        if (PW.c[i].on) ma += PW.c[i].ma;
    (void)t;
    return ma;
}

int sim_battery_mv(void) {
    double soc = sim_battery_soc() * 100.0;
    int i = soc >= 100 ? 9 : (int)(soc / 10);
    double v = BATT_CURVE_1A[i] + (BATT_CURVE_1A[i + 1] - BATT_CURVE_1A[i]) * (soc - 10.0 * i) / 10.0;
    double into = PW.chg_ma - sim_power_load_ma(); /* + charging */
    return (int)(v + (1000.0 + into) * BATT_R_MOHM / 1000.0);
}

int sim_battery_ma(void) { return (int)(PW.chg_ma - sim_power_load_ma()); }
bool sim_power_vbus(void) { return PW.charging; }

void sim_power_set_charge_ma(double ma) { /* ISR or locked: the charger's current into the battery */
    chg_settle(now_ns());
    PW.chg_ma = ma > 0 ? ma : 0;
}

static void battery_model(hal_battery_t *out) {
    double soc = sim_battery_soc();
    out->percent = (int)(soc * 100.0 + 0.5);
    out->millivolts = sim_battery_mv();
    out->charging = PW.charging && PW.chg_ma > 0;
    out->external_power = PW.charging;
}

void hal_power_battery(hal_battery_t *out) {
    if (sim_board_present() && !vtk_in_isr() && sim_board_battery(out)) return; /* gauge + charger drivers */
    battery_model(out);
}

void sim_power_set_battery(int percent, bool charging) {
    double used = total_mah();
    PW.soc = percent / 100.0 + (g_profile.batt_mah > 0 ? used / g_profile.batt_mah : 0);
    PW.charging = charging;
    sim_charger_update();
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
    if (PW.wakes_unexplained) sim_metric_set("power.wakes.unexplained", (double)PW.wakes_unexplained);
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
