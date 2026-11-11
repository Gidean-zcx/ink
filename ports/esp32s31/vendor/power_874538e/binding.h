/* CloudZao Metalio-E-INK4-Plus @874538e. MIT: LICENSE; adaptations: SOURCE.md. */
#ifndef INK_VENDOR_POWER_BINDING_H
#define INK_VENDOR_POWER_BINDING_H
#include "ink/hal_i2c.h"
#include "ink/osal.h"
#include "esp_err.h"
#include "esp_log.h"
#include <time.h>
typedef struct { hal_i2c_dev_t *dev; } power_i2c_t;
static inline esp_err_t power_i2c_probe(const power_i2c_t *bus, uint8_t addr, int ms) {
    (void)addr; (void)ms;
    return bus && hal_i2c_write(bus->dev, NULL, 0) == HAL_I2C_OK ? ESP_OK : ESP_FAIL;
}
static inline esp_err_t power_i2c_write_read(const power_i2c_t *bus, uint8_t addr, uint8_t reg, uint8_t *data, size_t len, int ms) {
    (void)addr; (void)ms;
    return hal_i2c_write_read(bus->dev, &reg, 1, data, len) == HAL_I2C_OK ? ESP_OK : ESP_FAIL;
}
static inline esp_err_t power_i2c_write_reg(const power_i2c_t *bus, uint8_t addr, uint8_t reg, const uint8_t *data, size_t len, int ms) {
    (void)addr; (void)ms;
    uint8_t bytes[17];
    if (len > sizeof bytes - 1) return ESP_ERR_INVALID_ARG;
    bytes[0] = reg;
    for (size_t i = 0; i < len; ++i) bytes[i + 1] = data[i];
    return hal_i2c_write(bus->dev, bytes, len + 1) == HAL_I2C_OK ? ESP_OK : ESP_FAIL;
}
#endif
