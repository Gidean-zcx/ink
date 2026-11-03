/*
 * ink HAL — I2C master.
 *
 * Board drivers (drivers/) reach their chips only through this interface, so
 * the same driver code runs on the device (ESP-IDF i2c_master) and in the
 * simulator, where a bus model routes each transfer to a register-level chip
 * model and can inject the faults seen on the device (e.g. a timeout on the
 * shared bus). Task context only; transfers on one bus are serialised.
 */
#ifndef INK_HAL_I2C_H
#define INK_HAL_I2C_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    HAL_I2C_OK = 0,
    HAL_I2C_NACK = -1,    /* no chip answered at the address (absent, asleep) */
    HAL_I2C_TIMEOUT = -2, /* bus held or transfer not finished in time */
    HAL_I2C_ERR = -3,
} hal_i2c_status_t;

typedef struct hal_i2c_dev hal_i2c_dev_t;

/** Handle for the chip at a 7-bit address on bus `bus`; `name` labels metrics and traces. */
hal_i2c_dev_t *hal_i2c_device(int bus, uint8_t addr7, const char *name);

hal_i2c_status_t hal_i2c_write(hal_i2c_dev_t *dev, const uint8_t *data, size_t len);

/** Write then read with a repeated start (register reads). */
hal_i2c_status_t hal_i2c_write_read(hal_i2c_dev_t *dev, const uint8_t *wdata, size_t wlen, uint8_t *rdata,
                                    size_t rlen);

const char *hal_i2c_status_name(hal_i2c_status_t s);

#ifdef __cplusplus
}
#endif
#endif
