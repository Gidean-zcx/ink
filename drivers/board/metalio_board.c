/*
 * Metalio E-Ink4 Plus board IO expander (see metalio_board.h). Directions and power-on levels
 * follow upstream epd_board_metalio_eink4_plus.c @690a023 (P12/P14 high, P13 low, P05 high).
 */
#include "board/metalio_board.h"

#include "ink/osal.h"

#define DIR0 0xC0 /* P06, P07 inputs */
#define DIR1 0xE3 /* P10, P11, P15, P16, P17 inputs */

static tca9555_t g_tca;
static osal_mutex_t *g_lock;
static bool g_ready;
static unsigned g_p14_epoch;    /* times P14 came back on: what hangs on it lost its state */
static uint64_t g_p14_since_us; /* when it last came on */

hal_i2c_status_t metalio_board_init(void) {
    /* port 0: P00-P05 outputs, P06-P07 inputs; port 1: P12-P14 outputs, the rest inputs */
    static const uint8_t out[2] = {MB_P0_CAM_SCR, MB_P1_SHARED_RST | MB_P1_SHARED_PWR}, dir[2] = {DIR0, DIR1};
    if (!g_lock) g_lock = osal_mutex_create();
    if (!g_lock) return HAL_I2C_ERR;
    osal_mutex_lock(g_lock, OSAL_WAIT_FOREVER);
    hal_i2c_status_t s = tca9555_init(&g_tca, hal_i2c_device(0, TCA9555_ADDR_BASE, "io-expander"), out, dir);
    g_ready = s == HAL_I2C_OK;
    if (g_ready) g_p14_since_us = osal_now_us(); /* P14 on from here (epoch 0) */
    osal_mutex_unlock(g_lock);
    return s;
}

bool metalio_board_ready(void) { return g_ready; }
tca9555_t *metalio_ioexp(void) { return &g_tca; }
void metalio_board_lock(void) { osal_mutex_lock(g_lock, OSAL_WAIT_FOREVER); }
void metalio_board_unlock(void) { osal_mutex_unlock(g_lock); }

hal_i2c_status_t metalio_board_set(int port, uint8_t mask, uint8_t value) {
    metalio_board_lock();
    bool p14_was = g_tca.out[1] & MB_P1_SHARED_PWR;
    hal_i2c_status_t s = tca9555_set(&g_tca, port, mask, value);
    if (port == 1 && !p14_was && (g_tca.out[1] & MB_P1_SHARED_PWR)) {
        __atomic_store_n(&g_p14_since_us, osal_now_us(), __ATOMIC_RELEASE);
        __atomic_add_fetch(&g_p14_epoch, 1, __ATOMIC_ACQ_REL);
    }
    metalio_board_unlock();
    return s;
}

hal_i2c_status_t metalio_board_read(int port, uint8_t *levels) {
    metalio_board_lock();
    hal_i2c_status_t s = tca9555_read(&g_tca, port, levels);
    metalio_board_unlock();
    return s;
}

/* Light sleep, in upstream's steps (epd_board_metalio_eink4_plus.c and power_hw.cc @690a023). */
hal_i2c_status_t metalio_board_mask_for_sleep(bool rtc_wake) {
    metalio_board_lock();
    /* port 0: the PMIC pins, P06 and P07 low; motor, amplifier and P05 as they are */
    uint8_t p0 = (uint8_t)(g_tca.out[0] & (MB_P0_MOTOR | MB_P0_AMP | MB_P0_CAM_SCR));
    /* port 1: P12 and P14 as they are; the keys, and P16 when it will wake, at their idle level (high),
     * so that turning them back into inputs makes no edge unless they are active; P13, P15 and
     * otherwise P16 low */
    uint8_t p1 = (uint8_t)((g_tca.out[1] & (MB_P1_SHARED_RST | MB_P1_SHARED_PWR)) | MB_P1_KEYS | (rtc_wake ? MB_P1_RTC_INT : 0));
    hal_i2c_status_t s = tca9555_set(&g_tca, 0, 0xFF, p0);
    if (s == HAL_I2C_OK) s = tca9555_set_dir(&g_tca, 0, 0x00);
    if (s == HAL_I2C_OK) s = tca9555_set(&g_tca, 1, 0xFF, p1);
    if (s == HAL_I2C_OK) s = tca9555_set_dir(&g_tca, 1, 0x00);
    uint8_t levels;
    if (s == HAL_I2C_OK) s = tca9555_read(&g_tca, 0, &levels); /* reading both ports releases INT */
    if (s == HAL_I2C_OK) s = tca9555_read(&g_tca, 1, &levels);
    metalio_board_unlock();
    return s;
}

hal_i2c_status_t metalio_board_arm_wake(bool power_key, bool rtc) {
    uint8_t dir = (uint8_t)((power_key ? MB_P1_POWER_KEY : 0) | (rtc ? MB_P1_RTC_INT : 0));
    metalio_board_lock();
    hal_i2c_status_t s = tca9555_set_dir(&g_tca, 1, dir);
    metalio_board_unlock();
    return s;
}

hal_i2c_status_t metalio_board_cut_rails(bool keep_shared_power) {
    metalio_board_lock();
    hal_i2c_status_t s = tca9555_set(&g_tca, 0, MB_P0_CAM_SCR, 0);
    if (s == HAL_I2C_OK && !keep_shared_power) s = tca9555_set(&g_tca, 1, MB_P1_SHARED_PWR, 0);
    metalio_board_unlock();
    return s;
}

hal_i2c_status_t metalio_board_restore_keys(uint8_t *p0, uint8_t *p1) {
    metalio_board_lock();
    hal_i2c_status_t s = tca9555_set(&g_tca, 1, (uint8_t)~(MB_P1_SHARED_RST | MB_P1_SHARED_PWR), 0); /* P13 low */
    if (s == HAL_I2C_OK) s = tca9555_set_dir(&g_tca, 0, DIR0);
    if (s == HAL_I2C_OK) s = tca9555_set_dir(&g_tca, 1, DIR1);
    uint8_t l0 = 0, l1 = 0;
    if (s == HAL_I2C_OK) s = tca9555_read(&g_tca, 0, &l0);
    if (s == HAL_I2C_OK) s = tca9555_read(&g_tca, 1, &l1);
    metalio_board_unlock();
    if (p0) *p0 = l0;
    if (p1) *p1 = l1;
    return s;
}

hal_i2c_status_t metalio_board_restore_rails(void) {
    hal_i2c_status_t s = metalio_board_set(1, MB_P1_SHARED_PWR, MB_P1_SHARED_PWR);
    if (s == HAL_I2C_OK) s = metalio_board_set(0, MB_P0_CAM_SCR, MB_P0_CAM_SCR);
    return s;
}

uint8_t metalio_board_output(int port) {
    metalio_board_lock();
    uint8_t v = g_tca.out[port & 1];
    metalio_board_unlock();
    return v;
}

unsigned metalio_board_shared_power_epoch(void) { return __atomic_load_n(&g_p14_epoch, __ATOMIC_ACQUIRE); }
uint64_t metalio_board_shared_power_since_us(void) { return __atomic_load_n(&g_p14_since_us, __ATOMIC_ACQUIRE); }

void metalio_power_off(void) {
    for (int i = 0; i < MB_POWER_PULSES; i++) {
        metalio_board_set(1, MB_P1_PWR_PULSE, MB_P1_PWR_PULSE);
        osal_task_delay_us(MB_POWER_PULSE_MS * 1000u);
        metalio_board_set(1, MB_P1_PWR_PULSE, 0);
        osal_task_delay_us(MB_POWER_PULSE_MS * 1000u);
    }
}
