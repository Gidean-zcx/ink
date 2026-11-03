#include "tca9555.h"

enum { REG_INPUT0 = 0, REG_OUTPUT0 = 2, REG_POLARITY0 = 4, REG_CONFIG0 = 6 };

static hal_i2c_status_t write_reg(tca9555_t *t, uint8_t reg, uint8_t value) {
    uint8_t b[2] = {reg, value};
    return hal_i2c_write(t->dev, b, sizeof b);
}

hal_i2c_status_t tca9555_init(tca9555_t *t, hal_i2c_dev_t *dev, const uint8_t out[2], const uint8_t dir[2]) {
    t->dev = dev;
    hal_i2c_status_t first = HAL_I2C_OK;
    for (int port = 0; port < 2; port++) {
        t->out[port] = out[port];
        t->dir[port] = dir[port];
        hal_i2c_status_t s = write_reg(t, (uint8_t)(REG_OUTPUT0 + port), out[port]);
        if (s == HAL_I2C_OK) s = write_reg(t, (uint8_t)(REG_POLARITY0 + port), 0);
        if (s == HAL_I2C_OK) s = write_reg(t, (uint8_t)(REG_CONFIG0 + port), dir[port]);
        if (first == HAL_I2C_OK) first = s;
    }
    return first;
}

hal_i2c_status_t tca9555_set(tca9555_t *t, int port, uint8_t mask, uint8_t value) {
    t->out[port] = (uint8_t)((t->out[port] & ~mask) | (value & mask));
    return write_reg(t, (uint8_t)(REG_OUTPUT0 + port), t->out[port]);
}

hal_i2c_status_t tca9555_read(tca9555_t *t, int port, uint8_t *levels) {
    uint8_t reg = (uint8_t)(REG_INPUT0 + port);
    return hal_i2c_write_read(t->dev, &reg, 1, levels, 1);
}
