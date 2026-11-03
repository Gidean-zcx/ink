#include "metalio_epd_power.h"

#include "ink/osal.h"
#include "tps65185/tps65185.h"

#define TICK_US 10000 /* upstream's vTaskDelay(1): one FreeRTOS tick at HZ = 100 */
#define PIN_MASK (EPD_PWR_VCOM_CTRL | EPD_PWR_PWRUP | EPD_PWR_WAKEUP)
#define WATCH_MASK (PIN_MASK | EPD_PWR_SCREEN) /* what the panel's supply depends on */

static void io_lock(epd_power_t *p) {
    if (p->io_lock) p->io_lock();
}
static void io_unlock(epd_power_t *p) {
    if (p->io_unlock) p->io_unlock();
}

static void pins(epd_power_t *p, uint8_t levels) {
    /* upstream logs a failed write and carries on; the missing edge shows up in the next step */
    io_lock(p);
    if (tca9555_set(p->ioexp, 0, PIN_MASK, levels) != HAL_I2C_OK) p->ioexp_errors++;
    io_unlock(p);
}

static hal_i2c_status_t read_port0(epd_power_t *p, uint8_t *levels) {
    io_lock(p);
    hal_i2c_status_t s = tca9555_read(p->ioexp, 0, levels);
    io_unlock(p);
    return s;
}

static uint8_t shadow_port0(epd_power_t *p) {
    io_lock(p);
    uint8_t v = p->ioexp->out[0];
    io_unlock(p);
    return v;
}

static epd_power_result_t power_on_once(epd_power_t *p) {
    p->attempts++;
    if (p->gate_output) p->gate_output(p->ctx, false);

    /* STANDBY (WAKEUP high, PWRUP and VCOM_CTRL low); let the PMIC wake before any I2C */
    pins(p, EPD_PWR_WAKEUP);
    osal_task_delay_us(50000);
    if (tps65185_probe(p->pmic) != HAL_I2C_OK) return EPD_POWER_NO_PMIC;
    if (tps65185_read(p->pmic, TPS_REG_REVID, &p->revid) != HAL_I2C_OK) return EPD_POWER_CONFIG_FAILED;
    /* power-up order and delays; upstream writes them although they are the reset values */
    if (tps65185_write(p->pmic, TPS_REG_UPSEQ0, 0xE4) != HAL_I2C_OK ||
        tps65185_write(p->pmic, TPS_REG_UPSEQ1, 0x55) != HAL_I2C_OK)
        return EPD_POWER_CONFIG_FAILED;
    uint8_t v;
    (void)tps65185_read(p->pmic, TPS_REG_INT2, &v); /* clears latched faults */
    (void)tps65185_read(p->pmic, TPS_REG_PG, &v);

    /* the PWRUP rising edge starts the boost sequence; VCOM_CTRL rises with it */
    pins(p, PIN_MASK);
    osal_task_delay_us(50000);
    (void)tps65185_read(p->pmic, TPS_REG_INT2, &v);
    (void)tps65185_read(p->pmic, TPS_REG_PG, &v);
    pins(p, PIN_MASK); /* upstream writes the pins once more before polling */

    /* PWR_GOOD on the expander: 200 back-to-back reads (upstream's vTaskDelay(pdMS_TO_TICKS(1)) is
     * zero ticks at HZ = 100) */
    bool good = false;
    for (int i = 0; i < 200 && !good; i++) {
        uint8_t levels;
        if (read_port0(p, &levels) == HAL_I2C_OK && (levels & EPD_PWR_GOOD))
            good = true;
        else
            osal_task_yield();
    }
    if (!good) return EPD_POWER_NO_PWR_GOOD;

    if (tps65185_write(p->pmic, TPS_REG_ENABLE, TPS_ENABLE_ALL_RAILS) != HAL_I2C_OK ||
        tps65185_set_vcom_mv(p->pmic, p->vcom_mv) != HAL_I2C_OK)
        return EPD_POWER_RAILS_FAILED;
    for (int tries = 0;; tries++) {
        if (tps65185_read(p->pmic, TPS_REG_PG, &v) == HAL_I2C_OK && (v & TPS_PG_ALL_GOOD) == TPS_PG_ALL_GOOD)
            break;
        if (tries >= 200) return EPD_POWER_RAILS_FAILED;
        osal_task_delay_us(TICK_US);
    }
    if (p->gate_output) p->gate_output(p->ctx, true);
    io_lock(p);
    p->ioexp->changed[0] &= (uint8_t)~WATCH_MASK; /* from here a change of these pins is someone else's */
    io_unlock(p);
    return EPD_POWER_OK;
}

epd_power_result_t epd_power_on(epd_power_t *p) {
    epd_power_result_t r = power_on_once(p);
    for (int retry = 1; r != EPD_POWER_OK && retry <= EPD_PWR_RETRIES; retry++) {
        epd_power_off(p);
        osal_task_delay_us(100000);
        p->retries++;
        r = power_on_once(p);
    }
    if (r != EPD_POWER_OK) epd_power_off(p);
    return r;
}

void epd_power_off(epd_power_t *p) {
    /* The panel needs XOE low >= 12 us before VPOS/VNEG go off. Upstream busy-waits 12 us; here the
     * expander write that drops PWRUP takes far longer on the bus, and the simulator's PMIC model
     * checks the order on every power-down. */
    if (p->gate_output) p->gate_output(p->ctx, false);
    pins(p, EPD_PWR_WAKEUP); /* VCOM_CTRL and PWRUP low: rails power down */
    osal_task_delay_us(TICK_US);
    pins(p, 0); /* WAKEUP low: SLEEP */
}

bool epd_power_good(epd_power_t *p) {
    uint8_t levels = 0, pg = 0;
    hal_i2c_status_t s = HAL_I2C_ERR;
    for (int i = 0; i < 3 && s != HAL_I2C_OK; i++) s = read_port0(p, &levels);
    if (s != HAL_I2C_OK || !(levels & EPD_PWR_GOOD)) return false;
    s = HAL_I2C_ERR;
    for (int i = 0; i < 3 && s != HAL_I2C_OK; i++) s = tps65185_read(p->pmic, TPS_REG_PG, &pg);
    return s == HAL_I2C_OK && (pg & TPS_PG_ALL_GOOD) == TPS_PG_ALL_GOOD;
}

bool epd_power_pins_touched(epd_power_t *p) {
    io_lock(p);
    bool touched = p->ioexp->changed[0] & WATCH_MASK;
    io_unlock(p);
    return touched;
}

bool epd_power_kept(epd_power_t *p) { return !epd_power_pins_touched(p) && epd_power_good(p); }

void epd_power_sleep(epd_power_t *p) {
    uint8_t out0 = shadow_port0(p);
    if (out0 & PIN_MASK) {
        pins(p, out0 & EPD_PWR_WAKEUP); /* PWRUP and VCOM_CTRL low first */
        osal_task_delay_us(50000);
        pins(p, 0);
    }
    osal_task_delay_us(120000); /* datasheet: >= 100 ms in SLEEP before the next wake-up */
}

const char *epd_power_result_name(epd_power_result_t r) {
    switch (r) {
    case EPD_POWER_OK: return "ok";
    case EPD_POWER_NO_PMIC: return "no PMIC";
    case EPD_POWER_CONFIG_FAILED: return "PMIC configuration failed";
    case EPD_POWER_NO_PWR_GOOD: return "no PWR_GOOD";
    case EPD_POWER_RAILS_FAILED: return "rails failed";
    }
    return "?";
}
