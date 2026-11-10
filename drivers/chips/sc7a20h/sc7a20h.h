/*
 * SC7A20H 3-axis accelerometer (I2C 0x19, or 0x18), set up like the official driver
 * (sc7a20h.cc @690a023): 100 Hz, X/Y/Z on, high-pass on the AOI1 path, AOI1 -> INT1 latched until
 * INT1_SRC is read, BDU + high resolution at +-2 g (1 mg per LSB, 12 bits left-justified), AOI1 on
 * any axis above ~256 mg. On the Plus INT1 shares GPIO2 with the IO expander's INT.
 */
#ifndef INK_SC7A20H_H
#define INK_SC7A20H_H

#include <stdbool.h>

#include "ink/hal_i2c.h"

#define SC7A20H_ADDR 0x19
#define SC7A20H_ADDR_ALT 0x18

hal_i2c_status_t sc7a20h_init(hal_i2c_dev_t *dev);
hal_i2c_status_t sc7a20h_read_mg(hal_i2c_dev_t *dev, int *x, int *y, int *z);
/** Read and clear the motion latch (INT1_SRC.IA); releases INT1. */
hal_i2c_status_t sc7a20h_motion(hal_i2c_dev_t *dev, bool *moved);
/** Quiet mode before sleep (upstream: interrupts off, 1 Hz, axes off). */
hal_i2c_status_t sc7a20h_sleep(hal_i2c_dev_t *dev);
/** Motion watch while the chip sleeps: 10 Hz low-power, AOI1 above ~128 mg for one sample (100 ms) -> INT1
 *  latched. A tap is shorter than a sample; lifting the device lasts several. Calibrate on the device. */
hal_i2c_status_t sc7a20h_motion_watch(hal_i2c_dev_t *dev);

#endif
