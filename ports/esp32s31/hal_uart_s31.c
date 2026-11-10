/*
 * hal_uart on ESP-IDF uart: port 1 the BH1098G, port 2 the NT26 (pins in s31_board.h), 8N1, no
 * flow control. One reader per port; a line cut by a timeout continues on the next read.
 */
#include <stdio.h>

#include "driver/uart.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "ink/hal_uart.h"
#include "s31_board.h"
#include "s31_hal_map.h"

#define RX_BUF 2048

struct hal_uart {
    uart_port_t port;
    bool open;
    s31_line_t line;
};

static struct hal_uart g_ports[2] = {{.port = UART_NUM_1}, {.port = UART_NUM_2}};

hal_uart_t *hal_uart_open(int port, int baud) {
    if ((port != 1 && port != 2) || baud <= 0) return NULL;
    struct hal_uart *u = &g_ports[port - 1];
    if (u->open) return uart_set_baudrate(u->port, (uint32_t)baud) == ESP_OK ? u : NULL;
    uart_config_t c = {.baud_rate = baud, .data_bits = UART_DATA_8_BITS, .parity = UART_PARITY_DISABLE,
                       .stop_bits = UART_STOP_BITS_1, .flow_ctrl = UART_HW_FLOWCTRL_DISABLE, .source_clk = UART_SCLK_DEFAULT};
    int tx = port == 1 ? S31_UART1_TX : S31_UART2_TX, rx = port == 1 ? S31_UART1_RX : S31_UART2_RX;
    if (uart_driver_install(u->port, RX_BUF, 0, 0, NULL, 0) != ESP_OK) return NULL;
    if (uart_param_config(u->port, &c) != ESP_OK || uart_set_pin(u->port, tx, rx, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE) != ESP_OK) {
        uart_driver_delete(u->port);
        return NULL;
    }
    u->open = true;
    return u;
}

int hal_uart_write(hal_uart_t *u, const void *data, size_t len) {
    if (!u || !u->open) return -1;
    return uart_write_bytes(u->port, data, len);
}

int hal_uart_read_line(hal_uart_t *u, char *buf, size_t cap, uint32_t timeout_ms) {
    if (!u || !u->open || !buf || !cap) return -1;
    int64_t deadline = esp_timer_get_time() + (int64_t)timeout_ms * 1000;
    for (;;) {
        int64_t left_us = deadline - esp_timer_get_time();
        if (left_us < 0) left_us = 0;
        TickType_t ticks = (TickType_t)((left_us + portTICK_PERIOD_MS * 1000 - 1) / (portTICK_PERIOD_MS * 1000));
        char c;
        if (uart_read_bytes(u->port, &c, 1, ticks) != 1) return -1;
        int n = s31_line_feed(&u->line, c);
        if (n >= 0) {
            snprintf(buf, cap, "%s", u->line.buf);
            return n < (int)cap ? n : (int)cap - 1;
        }
    }
}
