/*
 * BQ27220 fuel gauge (I2C 0x55): standard commands from TI's bq27220 reference manual, each a
 * 16-bit little-endian value read from its command code. The official board reads Voltage and
 * Current (bq27220_gauge.cc @690a023).
 */
#ifndef INK_BQ27220_H
#define INK_BQ27220_H

#include "ink/hal_i2c.h"

#define BQ27220_ADDR 0x55

enum {
    BQ27220_TEMPERATURE = 0x06,   /* 0.1 K */
    BQ27220_VOLTAGE = 0x08,       /* mV */
    BQ27220_BATTERY_STATUS = 0x0A,
    BQ27220_CURRENT = 0x0C,       /* mA, signed: + charging */
    BQ27220_REMAINING_CAP = 0x10, /* mAh */
    BQ27220_FULL_CHARGE_CAP = 0x12,
    BQ27220_AVERAGE_CURRENT = 0x14,
    BQ27220_STATE_OF_CHARGE = 0x2C, /* % */
    BQ27220_STATE_OF_HEALTH = 0x2E,
    BQ27220_DESIGN_CAP = 0x3C,
};

typedef struct {
    int mv, ma, soc;
} bq27220_reading_t;

hal_i2c_status_t bq27220_read(hal_i2c_dev_t *dev, uint8_t cmd, int *value); /* signed for CURRENT */
/** Voltage/current and reported percent (S31: vendor voltage curve; raw SOC uses bq27220_read). */
hal_i2c_status_t bq27220_reading(hal_i2c_dev_t *dev, bq27220_reading_t *out);

#endif
