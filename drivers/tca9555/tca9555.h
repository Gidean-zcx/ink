/*
 * TCA9555 16-bit I2C I/O expander (TI datasheet: register map). Keeps a copy
 * of the output registers so changing pins is one register write, the way
 * the upstream board code drives it. Portable: talks only to hal_i2c.
 */
#ifndef INK_TCA9555_H
#define INK_TCA9555_H

#include <stdbool.h>
#include <stdint.h>

#include "ink/hal_i2c.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TCA9555_ADDR_BASE 0x20 /* + A2..A0 */

typedef struct {
    hal_i2c_dev_t *dev;
    uint8_t out[2]; /* output levels the chip is known to drive */
    uint8_t dir[2]; /* configuration registers: 1 = input */
    bool stale[2]; /* a write failed: the output register is read back before the next change */
} tca9555_t;

/** Write the output levels first, then the directions, so pins come up at the intended level. */
hal_i2c_status_t tca9555_init(tca9555_t *t, hal_i2c_dev_t *dev, const uint8_t out[2], const uint8_t dir[2]);

/** Change the `mask` bits of port 0 or 1 to `value` and write that output register. */
hal_i2c_status_t tca9555_set(tca9555_t *t, int port, uint8_t mask, uint8_t value);

/** Read the pin levels of port 0 or 1. */
hal_i2c_status_t tca9555_read(tca9555_t *t, int port, uint8_t *levels);

#ifdef __cplusplus
}
#endif
#endif
