/* Pure helpers of the S31 HALs, kept free of drivers so the host tests them (unit-s31-hal-map). */
#ifndef INK_S31_HAL_MAP_H
#define INK_S31_HAL_MAP_H

#include <stdbool.h>
#include <stddef.h>

#include "esp_err.h"
#include "ink/hal_i2c.h"

/* i2c_master: ESP_ERR_INVALID_RESPONSE is a NACK, ESP_ERR_TIMEOUT a held bus or a slow transfer. */
static inline hal_i2c_status_t s31_i2c_status(esp_err_t e) {
    switch (e) {
    case ESP_OK: return HAL_I2C_OK;
    case ESP_ERR_INVALID_RESPONSE: return HAL_I2C_NACK;
    case ESP_ERR_TIMEOUT: return HAL_I2C_TIMEOUT;
    default: return HAL_I2C_ERR;
    }
}

/* Line assembly for hal_uart_read_line: CR dropped, LF ends a line, empty lines skipped. A line longer
 * than the buffer is not passed on cut: it ends as an error and is counted. 512 takes a whole SMS in
 * UCS2 hex (70 characters: 280 in text mode; a PDU of up to 176 bytes: 352). */
#define S31_LINE_CAP 512
typedef struct {
    char buf[S31_LINE_CAP];
    size_t n;
    bool over;         /* this line no longer fits */
    unsigned overlong; /* lines dropped for that */
} s31_line_t;

/** Feed one byte; returns the length of the line that c completed (in l->buf, NUL-terminated), -2 when
 *  that line was too long (dropped, counted), else -1. */
static inline int s31_line_feed(s31_line_t *l, char c) {
    if (c == '\r') return -1;
    if (c == '\n') {
        if (!l->n) return -1;
        int n = l->over ? -2 : (int)l->n;
        l->buf[l->over ? 0 : l->n] = 0;
        if (l->over) l->overlong++;
        l->n = 0;
        l->over = false;
        return n;
    }
    if (l->n < sizeof l->buf - 1) l->buf[l->n++] = c;
    else l->over = true;
    return -1;
}

#endif
