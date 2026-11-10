/* Pure helpers of the S31 HALs, kept free of drivers so the host tests them (unit-s31-hal-map). */
#ifndef INK_S31_HAL_MAP_H
#define INK_S31_HAL_MAP_H

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

/* Line assembly for hal_uart_read_line: CR dropped, LF ends a line, empty lines skipped, a line
 * longer than the buffer keeps its start. */
#define S31_LINE_CAP 256
typedef struct {
    char buf[S31_LINE_CAP];
    size_t n;
} s31_line_t;

/** Feed one byte; returns the length of the line that c completed (in l->buf, NUL-terminated), else -1. */
static inline int s31_line_feed(s31_line_t *l, char c) {
    if (c == '\r') return -1;
    if (c == '\n') {
        if (!l->n) return -1;
        int n = (int)l->n;
        l->buf[n] = 0;
        l->n = 0;
        return n;
    }
    if (l->n < sizeof l->buf - 1) l->buf[l->n++] = c;
    return -1;
}

#endif
