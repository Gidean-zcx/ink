/*
 * The Plus's I2C chips on the S31 through the portable drivers, in the simulator's boot order
 * (sim_board_init: expander, then sim_power_chips_init, sim_sensors_init; touch in sim_input_init),
 * plus the front light on LEDC and the GPIO2 service. Key and touch events are not dispatched yet
 * (hal_input is still a stub): GPIO2 only counts accelerometer motion and reads the expander so
 * its INT lets go.
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
#include "sc7a20h/sc7a20h.h"

#define LIGHT_BITS LEDC_TIMER_10_BIT
#define LIGHT_MAX ((1u << 10) - 1)

static hal_i2c_dev_t *g_gauge, *g_charger, *g_rtc, *g_accel, *g_touch;
static volatile uint32_t g_motions;
static bool g_light;

static void gpio2_service(void *ctx) { /* irq task: expander INT and accelerometer INT1 share GPIO2 */
    (void)ctx;
    bool moved = false;
    if (g_accel && sc7a20h_motion(g_accel, &moved) == HAL_I2C_OK && moved) g_motions = g_motions + 1;
    uint8_t levels;
    if (metalio_board_ready() && metalio_board_read(1, &levels) == HAL_I2C_OK) metalio_board_read(0, &levels);
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
    int applied = 0; /* upstream metalio_power.cc: 1 A, charging on */
    if (!g_charger || cx25601n_set_charge_current(g_charger, 1000, &applied) != HAL_I2C_OK ||
        cx25601n_enable_charging(g_charger, true) != HAL_I2C_OK)
        hal_log_write("board: CX25601N setup failed");
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
    if (s31_irq_watch(S31_GPIO_EXP_INT, gpio2_service, NULL)) hal_log_write("board: GPIO2 interrupt setup failed");
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
    out->charging = chg == CX25601N_CC || chg == CX25601N_CV || chg == CX25601N_TOPOFF;
    out->external_power = vbus != 0;
}

bool hal_rtc_get(int64_t *unix_s) {
    bool valid = false;
    return g_rtc && pcf8563_get(g_rtc, unix_s, &valid) == HAL_I2C_OK && valid;
}

bool hal_rtc_set(int64_t unix_s) { return g_rtc && pcf8563_set(g_rtc, unix_s) == HAL_I2C_OK; }
bool hal_rtc_set_alarm(int64_t unix_s) { return g_rtc && pcf8563_set_alarm(g_rtc, unix_s) == HAL_I2C_OK; }
void hal_rtc_clear_alarm(void) {
    if (g_rtc) pcf8563_clear_alarm(g_rtc);
}

bool hal_accel_read(int *x_mg, int *y_mg, int *z_mg) { return g_accel && sc7a20h_read_mg(g_accel, x_mg, y_mg, z_mg) == HAL_I2C_OK; }
uint32_t hal_accel_motion_count(void) { return g_motions; }

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
