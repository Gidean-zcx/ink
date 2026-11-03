/*
 * Metalio E-Ink4 Plus board IO expander (see metalio_board.h). Directions and power-on levels
 * follow upstream epd_board_metalio_eink4_plus.c @690a023 (P12/P14 high, P13 low, P05 high).
 */
#include "board/metalio_board.h"

#include "ink/osal.h"

static tca9555_t g_tca;
static osal_mutex_t *g_lock;
static bool g_ready;

hal_i2c_status_t metalio_board_init(void) {
    /* port 0: P00-P05 outputs, P06-P07 inputs; port 1: P12-P14 outputs, the rest inputs */
    static const uint8_t out[2] = {MB_P0_CAM_SCR, MB_P1_SHARED_RST | MB_P1_SHARED_PWR}, dir[2] = {0xC0, 0xE3};
    if (!g_lock) g_lock = osal_mutex_create();
    if (!g_lock) return HAL_I2C_ERR;
    osal_mutex_lock(g_lock, OSAL_WAIT_FOREVER);
    hal_i2c_status_t s = tca9555_init(&g_tca, hal_i2c_device(0, TCA9555_ADDR_BASE, "io-expander"), out, dir);
    g_ready = s == HAL_I2C_OK;
    osal_mutex_unlock(g_lock);
    return s;
}

bool metalio_board_ready(void) { return g_ready; }
tca9555_t *metalio_ioexp(void) { return &g_tca; }
void metalio_board_lock(void) { osal_mutex_lock(g_lock, OSAL_WAIT_FOREVER); }
void metalio_board_unlock(void) { osal_mutex_unlock(g_lock); }

hal_i2c_status_t metalio_board_set(int port, uint8_t mask, uint8_t value) {
    metalio_board_lock();
    hal_i2c_status_t s = tca9555_set(&g_tca, port, mask, value);
    metalio_board_unlock();
    return s;
}

hal_i2c_status_t metalio_board_read(int port, uint8_t *levels) {
    metalio_board_lock();
    hal_i2c_status_t s = tca9555_read(&g_tca, port, levels);
    metalio_board_unlock();
    return s;
}

void metalio_power_off(void) {
    for (int i = 0; i < MB_POWER_PULSES; i++) {
        metalio_board_set(1, MB_P1_PWR_PULSE, MB_P1_PWR_PULSE);
        osal_task_delay_us(MB_POWER_PULSE_MS * 1000u);
        metalio_board_set(1, MB_P1_PWR_PULSE, 0);
        osal_task_delay_us(MB_POWER_PULSE_MS * 1000u);
    }
}
