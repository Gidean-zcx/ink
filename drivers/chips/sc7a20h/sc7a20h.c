#include "sc7a20h/sc7a20h.h"

#include <stdint.h>

enum { WHO_AM_I = 0x0F, CTRL1 = 0x20, CTRL2, CTRL3, CTRL4, CTRL5, CTRL6, OUT_X_L = 0x28, AOI1_CFG = 0x30, AOI1_SRC, AOI1_THS, AOI1_DUR };
#define AUTO_INC 0x80
#define AOI1_IA 0x40

static hal_i2c_status_t wr(hal_i2c_dev_t *dev, uint8_t reg, uint8_t v) {
    uint8_t b[2] = {reg, v};
    return hal_i2c_write(dev, b, 2);
}

hal_i2c_status_t sc7a20h_init(hal_i2c_dev_t *dev) {
    uint8_t reg = WHO_AM_I, who = 0;
    hal_i2c_status_t s = hal_i2c_write_read(dev, &reg, 1, &who, 1);
    if (s != HAL_I2C_OK) return s;
    if (who != 0x11 && who != 0x33 && who != 0x32 && who != 0x44) return HAL_I2C_ERR; /* upstream's accepted IDs */
    static const uint8_t seq[][2] = {{CTRL1, 0x57}, {CTRL2, 0x01}, {CTRL3, 0x40}, {CTRL4, 0x88}, {CTRL5, 0x08},
                                     {CTRL6, 0x03}, {AOI1_THS, 0x10}, {AOI1_DUR, 0x00}, {AOI1_CFG, 0x2A}};
    for (unsigned i = 0; i < sizeof seq / sizeof seq[0] && s == HAL_I2C_OK; i++) s = wr(dev, seq[i][0], seq[i][1]);
    return s;
}

hal_i2c_status_t sc7a20h_read_mg(hal_i2c_dev_t *dev, int *x, int *y, int *z) {
    uint8_t reg = OUT_X_L | AUTO_INC, b[6];
    hal_i2c_status_t s = hal_i2c_write_read(dev, &reg, 1, b, sizeof b);
    if (s != HAL_I2C_OK) return s;
    *x = (int16_t)(b[1] << 8 | b[0]) >> 4;
    *y = (int16_t)(b[3] << 8 | b[2]) >> 4;
    *z = (int16_t)(b[5] << 8 | b[4]) >> 4;
    return HAL_I2C_OK;
}

hal_i2c_status_t sc7a20h_motion(hal_i2c_dev_t *dev, bool *moved) {
    uint8_t reg = AOI1_SRC, v = 0;
    hal_i2c_status_t s = hal_i2c_write_read(dev, &reg, 1, &v, 1);
    if (s == HAL_I2C_OK) *moved = v & AOI1_IA;
    return s;
}

hal_i2c_status_t sc7a20h_sleep(hal_i2c_dev_t *dev) {
    hal_i2c_status_t s = wr(dev, AOI1_CFG, 0x00);
    if (s == HAL_I2C_OK) s = wr(dev, CTRL3, 0x00);
    if (s == HAL_I2C_OK) s = wr(dev, CTRL5, 0x00);
    if (s == HAL_I2C_OK) s = wr(dev, CTRL1, 0x10);
    if (s == HAL_I2C_OK) s = wr(dev, CTRL6, 0x00);
    return s;
}
