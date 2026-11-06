/* Transport timeouts retry; missing devices and exhausted attempts fail. */
#include "tps65185/tps65185.h"

static int calls, timeouts;
static hal_i2c_status_t final;

hal_i2c_status_t hal_i2c_write(hal_i2c_dev_t *dev, const uint8_t *data, size_t len) {
    (void)dev; (void)data; (void)len;
    calls++;
    return calls <= timeouts ? HAL_I2C_TIMEOUT : final;
}

hal_i2c_status_t hal_i2c_write_read(hal_i2c_dev_t *dev, const uint8_t *data, size_t len, uint8_t *out, size_t cap) {
    (void)out; (void)cap;
    return hal_i2c_write(dev, data, len);
}

static void setup(int failures, hal_i2c_status_t result) {
    calls = 0; timeouts = failures; final = result;
}

int main(void) {
    uint8_t value = 0;
    setup(2, HAL_I2C_OK);
    if (tps65185_probe(NULL) != HAL_I2C_TIMEOUT || calls != 1) return 1;
    setup(0, HAL_I2C_NACK);
    if (tps65185_probe(NULL) != HAL_I2C_NACK || calls != 1) return 2;
    setup(2, HAL_I2C_OK);
    if (tps65185_read(NULL, TPS_REG_REVID, &value) != HAL_I2C_OK || calls != 3) return 3;
    setup(10, HAL_I2C_OK);
    if (tps65185_write(NULL, TPS_REG_UPSEQ0, 0xE4) != HAL_I2C_TIMEOUT || calls != 3) return 4;
    setup(0, HAL_I2C_ERR);
    if (tps65185_write(NULL, TPS_REG_ENABLE, 0) != HAL_I2C_ERR || calls != 1) return 5;
    setup(2, HAL_I2C_OK);
    if (tps65185_set_vcom_mv(NULL, 2000) != HAL_I2C_OK || calls != 4) return 6;
    return 0;
}
