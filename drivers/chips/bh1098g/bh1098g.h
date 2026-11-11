/*
 * BH1098G Bluetooth audio chip on UART1 (115200), commands as the official firmware sends them
 * (metalio_audio.cc, bluetooth_uart.cc, README 12.1 @690a023; no datasheet in hand). Lines end in
 * CR LF; v2.0.77 documents SET MODE n acknowledgements (protocol.md:37-69).
 *   mode 1, AI conversation (default at boot): AT+RX=2, ~700 ms, AT+MODE=1
 *   mode 2, transmit to Bluetooth headphones:  AT+TX=1, AT+MODE=2
 *   mode 3, music receiver (device as speaker): AT+RX=1, AT+MODE=3
 *   version: AT+GVER=1 -> "AT+GVER=V1.03"; call audio over a headset: AT+PP=1, 200 ms, AT+BTSCO=1
 * The chip shares the P14 supply with the modem and the amplifier and needs ~700 ms after power-up.
 */
#ifndef INK_BH1098G_H
#define INK_BH1098G_H

#include <stdbool.h>

#include "ink/hal_uart.h"

#define BH1098G_UART_PORT 1
#define BH1098G_BAUD 115200
#define BH1098G_BOOT_US 700000 /* upstream kBootSettleUs */

int bh1098g_set_mode(hal_uart_t *u, int mode); /* 1, 2 or 3; 0 ok */
/** "V1.03" style version into buf; 0 ok, -1 no answer within 5 s (upstream's wait). */
int bh1098g_version(hal_uart_t *u, char *buf, int cap);
int bh1098g_headset_call(hal_uart_t *u);

#endif
