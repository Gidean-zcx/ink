/*
 * GC2145 camera sensor on SCCB (I2C 0x3C), as the official board drives it (dvp_camera.cc and the
 * vendor's VGA table, Apache-2.0, @690a023): product ID 0x2145 at 0xF0/0xF1, page select 0xFE, a
 * software reset (0xFE = 0xF0, then 16 ms), output format RGB565 (page 0, 0x84 = 0x06), pad output
 * enable 0xF2 (0x0F streams, 0x00 stops). VGA 640x480, RGB565 with the high byte first.
 *
 * gc2145_start loads the vendor's full VGA RGB565 sequence (gc2145_vga_table.h, Apache-2.0), as
 * the official board does before every capture.
 */
#ifndef INK_GC2145_H
#define INK_GC2145_H

#include <stdbool.h>
#include <stdint.h>

#include "ink/hal_i2c.h"

#define GC2145_ADDR 0x3C
#define GC2145_PID 0x2145
#define GC2145_WIDTH 640
#define GC2145_HEIGHT 480
#define GC2145_FRAME_BYTES (GC2145_WIDTH * GC2145_HEIGHT * 2)
#define GC2145_WARMUP_FRAMES 8 /* upstream kWarmupFrames: frames dropped before the one kept */

hal_i2c_status_t gc2145_probe(hal_i2c_dev_t *dev, uint16_t *pid);
/** Reset, VGA RGB565, output on (task context; waits the reset time). The sequence takes ~0.4 s. */
hal_i2c_status_t gc2145_start(hal_i2c_dev_t *dev);
/** The same, giving up with HAL_I2C_TIMEOUT before a step that would end after deadline_us
 *  (osal_now_us() time; 0 = no deadline). The sensor is then half set up: start again. */
hal_i2c_status_t gc2145_start_until(hal_i2c_dev_t *dev, uint64_t deadline_us);
hal_i2c_status_t gc2145_stop(hal_i2c_dev_t *dev);

#endif
