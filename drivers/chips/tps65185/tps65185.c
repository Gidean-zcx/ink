#include "tps65185.h"

hal_i2c_status_t tps65185_probe(hal_i2c_dev_t *dev) { return hal_i2c_write(dev, 0, 0); }

hal_i2c_status_t tps65185_read(hal_i2c_dev_t *dev, uint8_t reg, uint8_t *value) {
    hal_i2c_status_t s = HAL_I2C_ERR;
    for (int i = 0; i < 3; i++) {
        s = hal_i2c_write_read(dev, &reg, 1, value, 1);
        if (s != HAL_I2C_TIMEOUT) break;
    }
    return s;
}

hal_i2c_status_t tps65185_write(hal_i2c_dev_t *dev, uint8_t reg, uint8_t value) {
    uint8_t b[2] = {reg, value};
    hal_i2c_status_t s = HAL_I2C_ERR;
    for (int i = 0; i < 3; i++) {
        s = hal_i2c_write(dev, b, sizeof b);
        if (s != HAL_I2C_TIMEOUT) break;
    }
    return s;
}

hal_i2c_status_t tps65185_set_vcom_mv(hal_i2c_dev_t *dev, unsigned mv) {
    if (mv > TPS_VCOM_MAX_MV) return HAL_I2C_ERR;
    unsigned code = mv / TPS_VCOM_STEP_MV;
    hal_i2c_status_t s = tps65185_write(dev, TPS_REG_VCOM2, (uint8_t)((code >> 8) & 1));
    if (s == HAL_I2C_OK) s = tps65185_write(dev, TPS_REG_VCOM1, (uint8_t)(code & 0xFF));
    return s;
}
