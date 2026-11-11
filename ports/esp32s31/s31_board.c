#include <stdio.h>
/*
 * The Plus's I2C chips on the S31 through the portable drivers, in the simulator's boot order
 * (sim_board_init: expander, then sim_power_chips_init, sim_sensors_init; touch in sim_input_init),
 * plus the front light on LEDC. The input task owns GPIO2/5 and calls the sensor latch reader.
 */
#include "bq27220/bq27220.h"
#include "cx25601n/cx25601n.h"
#include "driver/ledc.h"
#include "ft6336u/ft6336u.h"
#include "ink/hal_device.h"
#include "ink/hal_power.h"
#include "metalio_plus/metalio_board.h"
#include "pcf8563/pcf8563.h"
#include "s31_board.h"
#include "s31_cell.h"
#include "s31_io.h"
#include "sc7a20h/sc7a20h.h"

#define LIGHT_BITS LEDC_TIMER_10_BIT
#define LIGHT_MAX ((1u << 10) - 1)

static hal_i2c_dev_t *g_gauge, *g_charger, *g_rtc, *g_accel, *g_touch;
static uint32_t g_motions;
static bool g_light;
static bool g_uv_done;
static uint8_t g_kick_step;

static void charger_log(const char *phase) {
    int mv = -1, iterm = -1, battery_mv = -1;
    bool term = false;
    hal_i2c_status_t c = g_charger ? cx25601n_charge_config(g_charger, &mv, &iterm, &term) : HAL_I2C_ERR;
    hal_i2c_status_t b = g_gauge ? bq27220_read(g_gauge, BQ27220_VOLTAGE, &battery_mv) : HAL_I2C_ERR;
    char line[160];
    snprintf(line, sizeof line, "charger %s: i2c=%d VREG=%dmV ITERM=%dmA EN_TERM=%d gauge_i2c=%d VBAT=%dmV",
             phase, c, mv, iterm, c == HAL_I2C_OK ? (int)term : -1, b, battery_mv);
    hal_log_write(line);
}

uint32_t hal_power_charger_poll(void) {
    bq27220_reading_t g;
    if (!g_charger || !g_gauge || bq27220_reading(g_gauge, &g) != HAL_I2C_OK) return 10000;
    return cx25601n_poll(g_charger, g.mv, g.ma, &g_uv_done, &g_kick_step);
}

bool s31_accel_take_motion(void) {
    bool moved = false;
    if (!g_accel || sc7a20h_motion(g_accel, &moved) != HAL_I2C_OK || !moved) return false;
    __atomic_add_fetch(&g_motions, 1, __ATOMIC_ACQ_REL);
    return true;
}

bool s31_accel_sleep(bool watch) {
    bool moved;
    if (!g_accel) return !watch;
    if (watch) return sc7a20h_motion(g_accel, &moved) == HAL_I2C_OK && sc7a20h_motion_watch(g_accel) == HAL_I2C_OK;
    return sc7a20h_sleep(g_accel) == HAL_I2C_OK && sc7a20h_motion(g_accel, &moved) == HAL_I2C_OK;
}

void s31_accel_resume(void) {
    if (g_accel && sc7a20h_init(g_accel) != HAL_I2C_OK) hal_log_write("board: accelerometer restore failed");
}

static bool light_init(void) {
    ledc_timer_config_t t = {.speed_mode = LEDC_LOW_SPEED_MODE, .duty_resolution = LIGHT_BITS, .timer_num = LEDC_TIMER_0,
                             .freq_hz = S31_LIGHT_HZ, .clk_cfg = LEDC_AUTO_CLK};
    if (ledc_timer_config(&t) != ESP_OK) return false;
    static const int pins[2] = {S31_LIGHT_COLD, S31_LIGHT_WARM};
    for (int i = 0; i < 2; i++) {
        ledc_channel_config_t c = {.gpio_num = pins[i], .speed_mode = LEDC_LOW_SPEED_MODE, .channel = (ledc_channel_t)i,
                                   .timer_sel = LEDC_TIMER_0, .duty = 0};
        if (ledc_channel_config(&c) != ESP_OK) return false;
    }
    return true;
}

void ink_s31_board_init(void) { /* boot, task context */
    if (metalio_board_init() != HAL_I2C_OK) hal_log_write("board: TCA9555 init failed");
    g_gauge = hal_i2c_device(0, BQ27220_ADDR, "fuel-gauge");
    g_charger = hal_i2c_device(0, CX25601N_ADDR, "charger");
    charger_log("before"); /* Vendor common/power/cx25601n.cc:314-341; log before any charger write. */
    if (!g_charger || cx25601n_init_defaults(g_charger) != HAL_I2C_OK)
        hal_log_write("board: CX25601N setup failed");
    charger_log("after");
    g_rtc = hal_i2c_device(0, PCF8563_ADDR, "rtc");
    g_accel = hal_i2c_device(0, SC7A20H_ADDR, "accel");
    if (!g_accel || sc7a20h_init(g_accel) != HAL_I2C_OK) {
        hal_log_write("board: SC7A20H did not answer as expected");
        g_accel = NULL;
    }
    g_light = light_init();
    if (!g_light) hal_log_write("board: front light LEDC setup failed");
    g_touch = hal_i2c_device(0, FT6336U_ADDR, "touch");
    uint8_t fw = 0;
    if (!g_touch || ft6336u_init(g_touch, &fw) != HAL_I2C_OK) {
        hal_log_write("board: FT6336U did not answer as expected");
        g_touch = NULL;
    }
    s31_power_init();
    ink_s31_audio_init();
    s31_input_init(g_touch);
    ink_s31_cell_init();
    hal_battery_t b;
    hal_power_battery(&b);
    uint8_t reg = 0x02, t[7] = {0};
    hal_i2c_status_t rs = g_rtc ? hal_i2c_write_read(g_rtc, &reg, 1, t, sizeof t) : HAL_I2C_ERR;
    char rtc[160];
    snprintf(rtc, sizeof rtc, "board: rtc i2c=%d raw=%02x %02x %02x %02x %02x %02x %02x (sec min hour day wday month year, VL=bit7 of sec)",
             rs, t[0], t[1], t[2], t[3], t[4], t[5], t[6]);
    hal_log_write(rtc);
    char line[128];
    snprintf(line, sizeof line, "board: accel=%d light=%d touch=%d fw=0x%02x battery=%d%% %dmV charging=%d power=%d", g_accel != NULL, g_light,
             g_touch != NULL, fw, b.percent, b.millivolts, b.charging, b.external_power);
    hal_log_write(line);
}

void hal_power_battery(hal_battery_t *out) {
    bq27220_reading_t g;
    cx25601n_chg_t chg;
    int vbus = 0;
    if (!g_gauge || !g_charger || bq27220_reading(g_gauge, &g) != HAL_I2C_OK ||
        cx25601n_status(g_charger, &chg, &vbus) != HAL_I2C_OK) {
        *out = (hal_battery_t){.percent = -1};
        return;
    }
    out->percent = g.soc;
    out->millivolts = g.mv;
    /* As upstream decides it (common/power/bq27220_gauge.h:39-40): charging when the gauge sees more than +5 mA
     * going in and the cell is not full. The charger's input code reads 0 even on USB while charging in CV
     * (docs/hardware/2026-11-10-selftest.md, round 5), so external power also counts a charge phase or a
     * gauge current that is not a discharge. */
    out->charging = (g.ma > 5 && g.soc < 100) || chg == CX25601N_CC || chg == CX25601N_CV || chg == CX25601N_TOPOFF;
    out->external_power = vbus != 0 || out->charging || g.ma >= -5;
}

/* The RTC keeps LOCAL time, as the factory firmware writes it (common/sensor/pcf8563.cc: localtime_r, then SetTime);
 * trial run 11 read 2026-11-11 00:07 at 00:06 Beijing time. Local here is the system's default zone, China +8 h; a
 * zone changed in settings does not reach this yet (backlog). Both firmwares then read the same clock. */
#define RTC_LOCAL_OFFSET_S (8 * 3600)
#define RTC_SANE_FROM 1735689600LL /* 2025-01-01: the factory firmware's own window is 2025..2099 (metalio_rtc.cc:71) */
#define RTC_SANE_UNTIL 4102444800LL /* 2100-01-01 */

bool hal_rtc_get(int64_t *unix_s) {
    bool valid = false;
    int64_t local = 0;
    if (!g_rtc || pcf8563_get(g_rtc, &local, &valid) != HAL_I2C_OK) return false;
    if (!valid) {
        /* VL: the oscillator may have stopped once. The factory firmware then waits for the network; a date inside its
         * window is used here unverified (this unit's clock was right with VL set) and the next network time rewrites
         * it, which clears VL. */
        if (local < RTC_SANE_FROM || local >= RTC_SANE_UNTIL) return false;
        static bool said;
        if (!said) hal_log_write("board: RTC low-voltage flag set; using its time unverified until the network sets it");
        said = true;
    }
    *unix_s = local - RTC_LOCAL_OFFSET_S;
    return true;
}

bool hal_rtc_set(int64_t unix_s) { return g_rtc && pcf8563_set(g_rtc, unix_s + RTC_LOCAL_OFFSET_S) == HAL_I2C_OK; }
bool hal_rtc_set_alarm(int64_t unix_s) { return g_rtc && pcf8563_set_alarm(g_rtc, unix_s + RTC_LOCAL_OFFSET_S) == HAL_I2C_OK; }
void hal_rtc_clear_alarm(void) {
    if (g_rtc) pcf8563_clear_alarm(g_rtc);
}

bool hal_accel_read(int *x_mg, int *y_mg, int *z_mg) { return g_accel && sc7a20h_read_mg(g_accel, x_mg, y_mg, z_mg) == HAL_I2C_OK; }
uint32_t hal_accel_motion_count(void) { return __atomic_load_n(&g_motions, __ATOMIC_ACQUIRE); }

static uint32_t duty(int permille) {
    if (permille < 0) permille = 0;
    if (permille > 1000) permille = 1000;
    return (uint32_t)permille * LIGHT_MAX / 1000u;
}

void hal_light_set(int cold_permille, int warm_permille) {
    if (!g_light) return;
    const uint32_t d[2] = {duty(cold_permille), duty(warm_permille)};
    for (int i = 0; i < 2; i++)
        if (ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)i, d[i]) == ESP_OK)
            ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)i);
}
