/*
 * Real-time clock and accelerometer HALs on the simulated Plus, through our drivers (pcf8563,
 * sc7a20h) against the register models in dev_rtc_accel.c, as the device port will do. GPIO2 is
 * shared by the IO expander's INT and the accelerometer's INT1: whoever services it reads both
 * (sim_sensors_service_gpio2), or a latched INT1 would hold the line low and hide key presses.
 */
#include "ink/hal_device.h"
#include "pcf8563/pcf8563.h"
#include "sc7a20h/sc7a20h.h"
#include "sim.h"

static hal_i2c_dev_t *g_rtc, *g_accel;
static uint32_t g_motions;

void sim_sensors_init(void) { /* task context (boot), after the models are attached */
    if (!sim_board_present()) return;
    sim_rtc_accel_init();
    g_rtc = hal_i2c_device(0, PCF8563_ADDR, "rtc");
    g_accel = hal_i2c_device(0, SC7A20H_ADDR, "accel");
    if (sc7a20h_init(g_accel) != HAL_I2C_OK) {
        sim_warn("board: SC7A20H did not answer as expected");
        g_accel = NULL;
    }
}

void sim_sensors_service_gpio2(void) { /* task context: GPIO2 fell */
    bool moved = false;
    if (g_accel && sim_accel_int_low() && sc7a20h_motion(g_accel, &moved) == HAL_I2C_OK && moved) g_motions++;
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
