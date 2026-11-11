#include "tca9555.h"

enum { REG_INPUT0 = 0, REG_OUTPUT0 = 2, REG_POLARITY0 = 4, REG_CONFIG0 = 6 };

static hal_i2c_status_t read_reg(tca9555_t *t, uint8_t reg, uint8_t *value) {
#ifdef ESP_PLATFORM
    hal_i2c_status_t ink_s31_i2c_read_stop(hal_i2c_dev_t *, uint8_t, uint8_t *, size_t);
    return ink_s31_i2c_read_stop(t->dev, reg, value, 1);
#else
    return hal_i2c_write_read(t->dev, &reg, 1, value, 1);
#endif
}

static hal_i2c_status_t write_reg(tca9555_t *t, uint8_t reg, uint8_t value) {
    uint8_t b[2] = {reg, value};
    return hal_i2c_write(t->dev, b, sizeof b);
}

hal_i2c_status_t tca9555_init(tca9555_t *t, hal_i2c_dev_t *dev, const uint8_t out[2], const uint8_t dir[2]) {
    t->dev = dev;
    hal_i2c_status_t first = HAL_I2C_OK;
    for (int port = 0; port < 2; port++) {
        t->changed[port] |= (uint8_t)(t->out[port] ^ out[port]); /* a set-up again counts as a write too */
        t->out[port] = out[port];
        t->dir[port] = dir[port];
        t->stale[port] = false;
        hal_i2c_status_t s = write_reg(t, (uint8_t)(REG_OUTPUT0 + port), out[port]);
        if (s == HAL_I2C_OK) s = write_reg(t, (uint8_t)(REG_POLARITY0 + port), 0);
        if (s == HAL_I2C_OK) s = write_reg(t, (uint8_t)(REG_CONFIG0 + port), dir[port]);
        if (first == HAL_I2C_OK) first = s;
    }
    return first;
}

/* The shadow holds what the chip is known to drive. A failed write may or may not have landed (a
 * timeout can come after the chip latched the byte), so the shadow is marked stale and the next
 * change reads the output register back first instead of resending a guess. */
hal_i2c_status_t tca9555_set(tca9555_t *t, int port, uint8_t mask, uint8_t value) {
    if (t->stale[port]) {
        uint8_t reg = (uint8_t)(REG_OUTPUT0 + port), now;
        hal_i2c_status_t r = read_reg(t, reg, &now);
        if (r != HAL_I2C_OK) return r;
        t->changed[port] |= (uint8_t)(t->out[port] ^ now);
        t->out[port] = now;
        t->stale[port] = false;
    }
    uint8_t want = (uint8_t)((t->out[port] & ~mask) | (value & mask));
    hal_i2c_status_t s = write_reg(t, (uint8_t)(REG_OUTPUT0 + port), want);
    t->changed[port] |= (uint8_t)(t->out[port] ^ want); /* a failed write may have landed too */
    if (s == HAL_I2C_OK) t->out[port] = want;
    else t->stale[port] = true;
    return s;
}

hal_i2c_status_t tca9555_set_dir(tca9555_t *t, int port, uint8_t dir) {
    hal_i2c_status_t s = write_reg(t, (uint8_t)(REG_CONFIG0 + port), dir);
    if (s == HAL_I2C_OK) t->dir[port] = dir;
    return s;
}

hal_i2c_status_t tca9555_read(tca9555_t *t, int port, uint8_t *levels) {
    uint8_t reg = (uint8_t)(REG_INPUT0 + port);
    return read_reg(t, reg, levels);
}
