/*
 * CX25601N charger (I2C 0x6B). Register layout as the official driver uses it
 * (cx25601n.cc @690a023); no datasheet in hand. ICHG in 80 mA steps (0x02[7:6] low bits,
 * 0x03[3:0] high bits), charge enable CHG_CTRL0 0x14 bit 2, STATUS1 0x1E: [4:3] charge state,
 * [2:0] input (VBUS) type, CHG_CTRL3 0x18 = 0x01 turns the battery FET off (ship mode).
 */
#ifndef INK_CX25601N_H
#define INK_CX25601N_H

#include <stdbool.h>

#include "ink/hal_i2c.h"

#define CX25601N_ADDR 0x6B
#define CX25601N_ICHG_STEP_MA 80
#define CX25601N_ICHG_MIN_MA 80
#define CX25601N_ICHG_MAX_MA 3040

typedef enum { CX25601N_NOT_CHARGING = 0, CX25601N_CC = 1, CX25601N_CV = 2, CX25601N_TOPOFF = 3 } cx25601n_chg_t;

hal_i2c_status_t cx25601n_part_info(hal_i2c_dev_t *dev, uint8_t *part);
/** Charge current, rounded down to a step; *applied_ma gets what was set (upstream: 1 A -> 960). */
hal_i2c_status_t cx25601n_set_charge_current(hal_i2c_dev_t *dev, int ma, int *applied_ma);
hal_i2c_status_t cx25601n_enable_charging(hal_i2c_dev_t *dev, bool on);
/** Charge state and input type (0 = no input). */
hal_i2c_status_t cx25601n_status(hal_i2c_dev_t *dev, cx25601n_chg_t *chg, int *vbus);
/** Ship mode: the battery is disconnected (the device turns off unless USB powers it). */
hal_i2c_status_t cx25601n_ship_mode(hal_i2c_dev_t *dev);

#endif
