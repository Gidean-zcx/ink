#include "bq27220/bq27220.h"

hal_i2c_status_t bq27220_read(hal_i2c_dev_t *dev, uint8_t cmd, int *value) {
    uint8_t b[2];
    hal_i2c_status_t s = hal_i2c_write_read(dev, &cmd, 1, b, 2);
    if (s != HAL_I2C_OK) return s;
    int v = b[0] | (b[1] << 8);
    if (cmd == BQ27220_CURRENT || cmd == BQ27220_AVERAGE_CURRENT) v = (int16_t)v;
    *value = v;
    return HAL_I2C_OK;
}

hal_i2c_status_t bq27220_reading(hal_i2c_dev_t *dev, bq27220_reading_t *out) {
    hal_i2c_status_t s = bq27220_read(dev, BQ27220_VOLTAGE, &out->mv);
    if (s == HAL_I2C_OK) s = bq27220_read(dev, BQ27220_CURRENT, &out->ma);
    if (s == HAL_I2C_OK) s = bq27220_read(dev, BQ27220_STATE_OF_CHARGE, &out->soc);
    return s;
}
