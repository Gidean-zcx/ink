/*
 * FT6336U capacitive touch controller (I2C 0x38), written from the FocalTech register map and the
 * official board's use of it (metalio_touch.c @690a023): TD_STATUS at 0x02, then 6 bytes per
 * point; chip ID 0x64 at 0xA3; INT mode 0x00 at 0xA4 keeps INT low while a finger is down.
 */
#ifndef INK_FT6336U_H
#define INK_FT6336U_H

#include <stdbool.h>
#include <stdint.h>

#include "ink/hal_i2c.h"

#define FT6336U_ADDR 0x38
#define FT6336U_CHIP_ID 0x64
#define FT6336U_MAX_POINTS 2

typedef struct {
    int id;     /* contact ID, 0 or 1 */
    int x, y;   /* raw controller coordinates */
    int event;  /* 0 press down, 1 lift up, 2 contact */
} ft6336u_point_t;

/** Check the chip ID, read the firmware version, set INT to stay low while touched. */
hal_i2c_status_t ft6336u_init(hal_i2c_dev_t *dev, uint8_t *fw_version);
/** Read the current points (up to max_points, one transfer). Points with a lift-up event are
 *  dropped, so *n counts fingers still down. */
hal_i2c_status_t ft6336u_read(hal_i2c_dev_t *dev, ft6336u_point_t *pts, int max_points, int *n);

#endif
