/* HAL bus 0 binds to the vendor's single synchronous I2C1 owner. */
#include "ink/hal_i2c.h"
#include "s31_board.h"
#include "s31_hal_map.h"
#include "vendor/board_874538e/bus.inc"

struct hal_i2c_dev { uint8_t addr; };
static struct hal_i2c_dev g_devs[DEVICE_SLOTS];
static unsigned g_n;
static const power_i2c_t g_power_bus = {
    .port = I2C_NUM_1, .lock = metalio_i2c1_lock,
    .unlock = metalio_i2c1_unlock, .device = metalio_i2c1_device_locked,
};

hal_i2c_dev_t *hal_i2c_device(int bus, uint8_t addr7, const char *name) {
    (void)name;
    if (bus != 0 || addr7 > 0x7f || metalio_i2c1_init(S31_I2C_SDA, S31_I2C_SCL) != ESP_OK ||
        !metalio_i2c1_lock(S31_I2C_TIMEOUT_MS)) return NULL;
    hal_i2c_dev_t *d = NULL;
    for (unsigned i = 0; i < g_n; ++i)
        if (g_devs[i].addr == addr7) d = &g_devs[i];
    i2c_master_dev_handle_t handle;
    if (!d && g_n < DEVICE_SLOTS && metalio_i2c1_device_locked(I2C_NUM_1, addr7, &handle) == ESP_OK) {
        d = &g_devs[g_n++];
        d->addr = addr7;
    }
    metalio_i2c1_unlock();
    return d;
}

hal_i2c_status_t hal_i2c_write(hal_i2c_dev_t *dev, const uint8_t *data, size_t len) {
    if (!dev) return HAL_I2C_ERR;
    if (!metalio_i2c1_lock(S31_I2C_TIMEOUT_MS)) return HAL_I2C_TIMEOUT;
    esp_err_t e = len ? metalio_i2c1_transmit_locked(I2C_NUM_1, dev->addr, data, len, S31_I2C_TIMEOUT_MS) :
                       metalio_i2c1_probe_locked(I2C_NUM_1, dev->addr, S31_I2C_TIMEOUT_MS);
    metalio_i2c1_unlock();
    return e == ESP_ERR_NOT_FOUND ? HAL_I2C_NACK : s31_i2c_status(e);
}

/* Port-private TCA helper: pointer write and receive share the vendor mutex. */
hal_i2c_status_t ink_s31_i2c_read_stop(hal_i2c_dev_t *dev, uint8_t reg, uint8_t *data, size_t len) {
    if (!dev) return HAL_I2C_ERR;
    if (!metalio_i2c1_lock(S31_I2C_TIMEOUT_MS)) return HAL_I2C_TIMEOUT;
    esp_err_t e = metalio_i2c1_read_reg_stop_locked(I2C_NUM_1, dev->addr, reg, data, len, S31_I2C_TIMEOUT_MS);
    metalio_i2c1_unlock();
    return s31_i2c_status(e);
}

hal_i2c_status_t hal_i2c_write_read(hal_i2c_dev_t *dev, const uint8_t *wdata, size_t wlen,
                                  uint8_t *rdata, size_t rlen) {
    if (!dev || (!wlen && !rlen)) return HAL_I2C_ERR;
    if (!rlen) return hal_i2c_write(dev, wdata, wlen);
    if (wlen == 1 && (dev->addr == 0x55 || dev->addr == 0x6b || dev->addr == 0x51 ||
                     dev->addr == 0x19 || dev->addr == 0x18))
        return s31_i2c_status(power_i2c_write_read(&g_power_bus, dev->addr, *wdata, rdata, rlen,
                              dev->addr == 0x55 ? 200 : 100));
    if (!metalio_i2c1_lock(S31_I2C_TIMEOUT_MS)) return HAL_I2C_TIMEOUT;
    esp_err_t e = wlen ? metalio_i2c1_transmit_receive_locked(I2C_NUM_1, dev->addr, wdata, wlen, rdata, rlen, S31_I2C_TIMEOUT_MS) :
                        metalio_i2c1_receive_locked(I2C_NUM_1, dev->addr, rdata, rlen, S31_I2C_TIMEOUT_MS);
    metalio_i2c1_unlock();
    return s31_i2c_status(e);
}

const char *hal_i2c_status_name(hal_i2c_status_t s) {
    switch (s) {
    case HAL_I2C_OK: return "ok";
    case HAL_I2C_NACK: return "nack";
    case HAL_I2C_TIMEOUT: return "timeout";
    case HAL_I2C_ERR: return "error";
    }
    return "?";
}
