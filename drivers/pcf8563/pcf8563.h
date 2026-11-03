/*
 * PCF8563 real-time clock (I2C 0x51), from the NXP datasheet: BCD time at 0x02..0x08 (VL flag in
 * the seconds register: the clock lost power and its time is not to be trusted), alarm at
 * 0x09..0x0C (bit 7 set = that field ignored), Control_status_2 0x01 (AIE bit 1, AF bit 3). On the
 * Plus V1.4 its INT goes to TCA9555 P16 (config.h @690a023). Times are UTC seconds.
 */
#ifndef INK_PCF8563_H
#define INK_PCF8563_H

#include <stdbool.h>
#include <stdint.h>

#include "ink/hal_i2c.h"

#define PCF8563_ADDR 0x51

hal_i2c_status_t pcf8563_get(hal_i2c_dev_t *dev, int64_t *unix_s, bool *valid);
hal_i2c_status_t pcf8563_set(hal_i2c_dev_t *dev, int64_t unix_s); /* also clears VL */
/** Alarm at the minute of unix_s (day of month, hour, minute must match); enables its interrupt. */
hal_i2c_status_t pcf8563_set_alarm(hal_i2c_dev_t *dev, int64_t unix_s);
/** Disable the alarm and clear its flag (releases INT). */
hal_i2c_status_t pcf8563_clear_alarm(hal_i2c_dev_t *dev);
hal_i2c_status_t pcf8563_alarm_flag(hal_i2c_dev_t *dev, bool *fired);

/* UTC calendar helpers (proleptic Gregorian) */
int64_t pcf8563_days_from_civil(int y, int m, int d);
void pcf8563_civil_from_days(int64_t days, int *y, int *m, int *d);

#endif
