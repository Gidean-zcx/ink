/*
 * TPS65185 e-paper PMIC: register access (TI datasheet register map). The
 * WAKEUP, PWRUP and VCOM_CTRL pins and the power sequence belong to the
 * board (boards/metalio_plus/metalio_epd_power.c). Portable: talks only to hal_i2c.
 */
#ifndef INK_TPS65185_H
#define INK_TPS65185_H

#include <stdint.h>

#include "ink/hal_i2c.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TPS65185_ADDR 0x68

enum {
    TPS_REG_TMST_VALUE = 0x00,
    TPS_REG_ENABLE = 0x01,
    TPS_REG_VADJ = 0x02,
    TPS_REG_VCOM1 = 0x03, /* VCOM[7:0] */
    TPS_REG_VCOM2 = 0x04, /* bit 0: VCOM[8]; also ACQ, PROG, HiZ, AVG */
    TPS_REG_INT_EN1 = 0x05,
    TPS_REG_INT_EN2 = 0x06,
    TPS_REG_INT1 = 0x07, /* cleared by reading */
    TPS_REG_INT2 = 0x08, /* cleared by reading */
    TPS_REG_UPSEQ0 = 0x09,
    TPS_REG_UPSEQ1 = 0x0A,
    TPS_REG_DWNSEQ0 = 0x0B,
    TPS_REG_DWNSEQ1 = 0x0C,
    TPS_REG_TMST1 = 0x0D,
    TPS_REG_TMST2 = 0x0E,
    TPS_REG_PG = 0x0F,
    TPS_REG_REVID = 0x10,
    TPS_REG_COUNT
};

#define TPS_ENABLE_ALL_RAILS 0x3F /* V3P3, VCOM, VDDH, VPOS, VEE and VNEG enable bits */
#define TPS_ENABLE_VCOM 0x10
#define TPS_PG_ALL_GOOD 0xFA      /* power-good bits the upstream code waits for */
#define TPS_VCOM_STEP_MV 10
#define TPS_VCOM_MAX_MV 5110      /* 9-bit code */

hal_i2c_status_t tps65185_probe(hal_i2c_dev_t *dev); /* address-only transfer: does it ACK? */
hal_i2c_status_t tps65185_read(hal_i2c_dev_t *dev, uint8_t reg, uint8_t *value);
hal_i2c_status_t tps65185_write(hal_i2c_dev_t *dev, uint8_t reg, uint8_t value);

/** VCOM magnitude in mV (the output is negative: 1200 -> -1.20 V). Writes VCOM2 (bit 8), then VCOM1. */
hal_i2c_status_t tps65185_set_vcom_mv(hal_i2c_dev_t *dev, unsigned mv);

#ifdef __cplusplus
}
#endif
#endif
