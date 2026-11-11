/* CloudZao panel sequence with ink HAL glue. MIT; see SOURCE.md and LICENSE. */
#include "esp_log.h"
#include "config.h"
#include "driver/gpio.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ink/hal_device.h"
#include "ink/osal.h"
#include "metalio_plus/metalio_board.h"
#include "metalio_plus/metalio_epd_power.h"
#include "tps65185/tps65185.h"
#include "power.h"
#include "s31_board.h"

#define CFG_PORT_CTRL 0
#define CFG_PIN_VCOM_CTRL EPD_PWR_VCOM_CTRL
#define CFG_PIN_PWRUP EPD_PWR_PWRUP
#define CFG_PIN_WAKEUP EPD_PWR_WAKEUP
#define CFG_PIN_PWRGOOD EPD_PWR_GOOD
#define CFG_P0_TPS_OUT_MASK (EPD_PWR_VCOM_CTRL | EPD_PWR_PWRUP | EPD_PWR_WAKEUP)
#define EPD_BORDER EPD_PIN_BORDER
#define TAG "panel-power"
static bool metalio_i2c1_lock(unsigned ms) { (void)ms; metalio_board_lock(); return true; }
static void metalio_i2c1_unlock(void) { metalio_board_unlock(); }
static hal_i2c_status_t metalio_i2c1_read_reg_stop_locked(hal_i2c_dev_t *p, int addr, uint8_t reg, uint8_t *out, size_t n, int ms) {
    (void)addr; (void)ms;
    hal_i2c_status_t r = hal_i2c_write(p, &reg, 1);
    return r == HAL_I2C_OK ? hal_i2c_write_read(p, NULL, 0, out, n) : r;
}
static hal_i2c_status_t metalio_i2c1_write_reg_locked(hal_i2c_dev_t *p, int addr, uint8_t reg, uint8_t *data, size_t n, int ms) {
    (void)addr; (void)ms;
    if (n != 1) return HAL_I2C_ERR;
    uint8_t bytes[] = {reg, *data};
    return hal_i2c_write(p, bytes, sizeof bytes);
}
static hal_i2c_status_t metalio_i2c1_probe_locked(hal_i2c_dev_t *p, int addr, int ms) {
    (void)addr; (void)ms;
    return hal_i2c_write(p, NULL, 0);
}
#include "tps65185.c"
hal_i2c_status_t tps65185_probe(hal_i2c_dev_t *p) { return tps_probe(p); }
hal_i2c_status_t tps65185_read(hal_i2c_dev_t *p, uint8_t reg, uint8_t *v) { return tps_read_register_ex(p, reg, v); }
hal_i2c_status_t tps65185_write(hal_i2c_dev_t *p, uint8_t reg, uint8_t v) { return tps_write_register(p, reg, v); }
hal_i2c_status_t tps65185_set_vcom_mv(hal_i2c_dev_t *p, unsigned mv) {
    if (mv > TPS_VCOM_MAX_MV) return HAL_I2C_ERR;
    unsigned code = mv / TPS_VCOM_STEP_MV;
    hal_i2c_status_t r = tps_write_register(p, TPS_REG_VCOM2, (code >> 8) & 1);
    return r == HAL_I2C_OK ? tps_write_register(p, TPS_REG_VCOM1, code & 255) : r;
}
typedef struct {
    bool ep_output_enable, ep_mode, ep_stv, ep_sth, ok;
    epd_power_t *power;
} epd_ctrl_state_t;
static hal_i2c_status_t set_ctrl_with_tps(epd_ctrl_state_t *state, const epd_ctrl_state_t *mask, uint8_t bits, uint8_t value) {
    if (mask->ep_output_enable) gpio_set_level(EPD_PIN_XOE, state->ep_output_enable);
    if (mask->ep_mode) gpio_set_level(EPD_PIN_MODE, state->ep_mode);
    epd_power_t *p = state->power;
    if (!p->ioexp) return HAL_I2C_ERR;
    if (!bits && !mask->ep_output_enable && !mask->ep_mode && !mask->ep_stv) return HAL_I2C_OK;
    if (p->io_lock) p->io_lock();
    hal_i2c_status_t r = tca9555_set(p->ioexp, 0, bits, value);
    if (r != HAL_I2C_OK) p->ioexp_errors++;
    if (p->io_unlock) p->io_unlock();
    return r;
}
static void epd_board_set_ctrl(epd_ctrl_state_t *s, const epd_ctrl_state_t *m) { (void)set_ctrl_with_tps(s, m, 0, 0); }
static uint8_t pca9555_read_input(epd_power_t *p, int port) {
    uint8_t levels = 0;
    if (p->io_lock) p->io_lock();
    (void)tca9555_read(p->ioexp, port, &levels);
    if (p->io_unlock) p->io_unlock();
    return levels;
}
#include "epd_power.inc"

bool ink_panel_off_confirmed(epd_power_t *p) {
    if (p->io_lock) p->io_lock();
    bool off = p->ioexp && !p->ioexp->stale[0] && !(p->ioexp->out[0] & CFG_P0_TPS_OUT_MASK);
    if (p->io_unlock) p->io_unlock();
    return off;
}
epd_power_result_t epd_power_on(epd_power_t *p) {
    const uint64_t until = osal_now_us() + 300000;
    while (p->ioexp && p->screen_coming && p->screen_coming() && !(p->ioexp->out[0] & EPD_PWR_SCREEN) && osal_now_us() < until)
        osal_task_delay_us(10000);
    if (!p->ioexp || !(p->ioexp->out[0] & EPD_PWR_SCREEN) || !(pca9555_read_input(p, 0) & EPD_PWR_SCREEN)) return EPD_POWER_NO_SCREEN;
    epd_ctrl_state_t state = {.power = p};
    epd_board_poweron(&state);
    if (!state.ok) return EPD_POWER_RAILS_FAILED;
    if (p->io_lock) p->io_lock();
    p->ioexp->changed[0] &= (uint8_t)~(CFG_P0_TPS_OUT_MASK | EPD_PWR_SCREEN);
    if (p->io_unlock) p->io_unlock();
    return EPD_POWER_OK;
}
void epd_power_off(epd_power_t *p) {
    epd_ctrl_state_t state = {.power = p};
    epd_board_poweroff(&state);
}
bool epd_power_good(epd_power_t *p) {
    uint8_t pg = 0, levels = pca9555_read_input(p, 0);
    return (levels & (EPD_PWR_GOOD | EPD_PWR_SCREEN)) == (EPD_PWR_GOOD | EPD_PWR_SCREEN) &&
        tps65185_read(p->pmic, TPS_REG_PG, &pg) == HAL_I2C_OK && (pg & TPS_PG_ALL_GOOD) == TPS_PG_ALL_GOOD;
}
bool epd_power_pins_touched(epd_power_t *p) {
    if (p->io_lock) p->io_lock();
    bool touched = p->ioexp->changed[0] & (CFG_P0_TPS_OUT_MASK | EPD_PWR_SCREEN);
    if (p->io_unlock) p->io_unlock();
    return touched;
}
bool epd_power_kept(epd_power_t *p) { return !epd_power_pins_touched(p) && epd_power_good(p); }
void epd_power_sleep(epd_power_t *p) {
    if (!ink_panel_off_confirmed(p)) {
        epd_ctrl_state_t state = {.power = p};
        epd_ctrl_state_t mask = {0};
        if (set_ctrl_with_tps(&state, &mask, CFG_PIN_VCOM_CTRL | CFG_PIN_PWRUP, 0) != HAL_I2C_OK) return;
        vTaskDelay(pdMS_TO_TICKS(50));
        if (set_ctrl_with_tps(&state, &mask, CFG_PIN_WAKEUP, 0) != HAL_I2C_OK) return;
    }
    vTaskDelay(pdMS_TO_TICKS(120));
}
const char *epd_power_result_name(epd_power_result_t r) {
    return r == EPD_POWER_OK ? "ok" : r == EPD_POWER_NO_SCREEN ? "screen power P05 off" : "rails failed";
}
