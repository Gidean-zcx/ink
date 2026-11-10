/*
 * ink HAL — auxiliary UARTs with line-based peripherals (the Plus: UART1 at 115200 to the BH1098G
 * Bluetooth audio chip, AT commands ending in CR LF). Task context.
 */
#ifndef INK_HAL_UART_H
#define INK_HAL_UART_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct hal_uart hal_uart_t;

/** NULL if this device has no such port. */
hal_uart_t *hal_uart_open(int port, int baud);
int hal_uart_write(hal_uart_t *u, const void *data, size_t len);
/** One received line without CR/LF; returns its length, -1 after timeout_ms without one, or -2 for a
 *  line longer than the port takes (dropped and counted, not passed on cut). */
int hal_uart_read_line(hal_uart_t *u, char *buf, size_t cap, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif
