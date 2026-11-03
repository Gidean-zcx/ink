#include "gc2145/gc2145.h"

#include "ink/osal.h"

static hal_i2c_status_t wr(hal_i2c_dev_t *dev, uint8_t reg, uint8_t v) {
    uint8_t b[2] = {reg, v};
    return hal_i2c_write(dev, b, 2);
}

static hal_i2c_status_t rd(hal_i2c_dev_t *dev, uint8_t reg, uint8_t *v) { return hal_i2c_write_read(dev, &reg, 1, v, 1); }

hal_i2c_status_t gc2145_probe(hal_i2c_dev_t *dev, uint16_t *pid) {
    uint8_t hi = 0, lo = 0;
    hal_i2c_status_t s = rd(dev, 0xF0, &hi);
    if (s == HAL_I2C_OK) s = rd(dev, 0xF1, &lo);
    if (s == HAL_I2C_OK) *pid = (uint16_t)(hi << 8 | lo);
    return s;
}

hal_i2c_status_t gc2145_start(hal_i2c_dev_t *dev) {
    /* the vendor table's head: software reset three times, then 16 ms */
    hal_i2c_status_t s = HAL_I2C_OK;
    for (int i = 0; i < 3 && s == HAL_I2C_OK; i++) s = wr(dev, 0xFE, 0xF0);
    if (s != HAL_I2C_OK) return s;
    osal_task_delay_us(16000);
    static const uint8_t seq[][2] = {
        {0xFE, 0x00}, {0xFC, 0x06}, {0xF6, 0x00}, {0xF7, 0x1D}, {0xF8, 0x84}, {0xFA, 0x00}, {0xF9, 0xFE}, /* clocks, PLL */
        {0xFE, 0x00}, {0x84, 0x06},                                                                    /* RGB565 */
        {0xF2, 0x0F},                                                                                  /* output on */
    };
    for (unsigned i = 0; i < sizeof seq / sizeof seq[0] && s == HAL_I2C_OK; i++) s = wr(dev, seq[i][0], seq[i][1]);
    return s;
}

hal_i2c_status_t gc2145_stop(hal_i2c_dev_t *dev) {
    hal_i2c_status_t s = wr(dev, 0xFE, 0x00);
    if (s == HAL_I2C_OK) s = wr(dev, 0xF2, 0x00);
    return s;
}
