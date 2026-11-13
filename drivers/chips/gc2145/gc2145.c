#include "gc2145/gc2145.h"

#include "gc2145/gc2145_vga_table.h"
#include "ink/osal.h"

static hal_i2c_status_t wr(hal_i2c_dev_t *dev, uint8_t reg, uint8_t v) {
    uint8_t b[2] = {reg, v};
    return hal_i2c_write(dev, b, 2);
}

static hal_i2c_status_t rd(hal_i2c_dev_t *dev, uint8_t reg, uint8_t *v) { return hal_i2c_write_read(dev, &reg, 1, v, 1); }

hal_i2c_status_t gc2145_probe(hal_i2c_dev_t *dev, uint16_t *pid) {
    uint8_t hi = 0, lo = 0;
    hal_i2c_status_t s = rd(dev, 0xF0, &hi);
    if (s == HAL_I2C_OK) s = rd(dev, 0xF1, &lo);
    if (s == HAL_I2C_OK) *pid = (uint16_t)(hi << 8 | lo);
    return s;
}

/* The vendor's VGA RGB565 sequence (gc2145_vga_table.h, as the official board loads it on every
 * capture): software reset, clocks and PLL, windowing, ISP tuning, the 640x480 crop, output on. */
hal_i2c_status_t gc2145_start_until(hal_i2c_dev_t *dev, uint64_t deadline_us) {
    for (unsigned i = 0; i < sizeof GC2145_VGA_RGB565 / sizeof GC2145_VGA_RGB565[0]; i++) {
        const gc2145_reg_t *e = &GC2145_VGA_RGB565[i];
        uint64_t step_us = e->reg == 0xFF ? (uint64_t)e->value * 1000u : 0;
        if (deadline_us && osal_now_us() + step_us > deadline_us) return HAL_I2C_TIMEOUT;
        if (e->reg == 0xFF) {
            osal_task_delay_us(step_us);
            continue;
        }
        hal_i2c_status_t s = wr(dev, e->reg, e->value);
        if (s != HAL_I2C_OK) return s;
        /* Between writes the bus goes to a task that waits for it: the input task's touch read, whose priority this
         * task inherited and keeps while it holds the camera's locks (a 0.4 s table otherwise loses a cover-key press). */
        osal_task_yield();
    }
    return wr(dev, 0xFE, 0x00); /* page 0, as upstream leaves it */
}

hal_i2c_status_t gc2145_start(hal_i2c_dev_t *dev) { return gc2145_start_until(dev, 0); }

hal_i2c_status_t gc2145_stop(hal_i2c_dev_t *dev) {
    hal_i2c_status_t s = wr(dev, 0xFE, 0x00);
    if (s == HAL_I2C_OK) s = wr(dev, 0xF2, 0x00);
    return s;
}
