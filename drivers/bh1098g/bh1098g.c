#include "bh1098g/bh1098g.h"

#include <stdio.h>
#include <string.h>

#include "ink/osal.h"

static int send(hal_uart_t *u, const char *cmd) {
    char line[32];
    size_t n = strlen(cmd);
    if (n + 2 > sizeof line) return -1;
    memcpy(line, cmd, n);
    line[n] = '\r';
    line[n + 1] = '\n';
    return hal_uart_write(u, line, n + 2) == (int)(n + 2) ? 0 : -1;
}

int bh1098g_set_mode(hal_uart_t *u, int mode) {
    static const char *const first[4] = {NULL, "AT+RX=2", "AT+TX=1", "AT+RX=1"};
    static const char *const second[4] = {NULL, "AT+MODE=1", "AT+MODE=2", "AT+MODE=3"};
    if (mode < 1 || mode > 3) return -1;
    if (send(u, first[mode])) return -1;
    osal_task_delay_us(700000); /* upstream waits ~700 ms between the two */
    return send(u, second[mode]);
}

int bh1098g_version(hal_uart_t *u, char *buf, int cap) {
    if (send(u, "AT+GVER=1")) return -1;
    char line[64];
    uint64_t deadline = osal_now_us() + 5000000u;
    while (osal_now_us() < deadline) {
        int n = hal_uart_read_line(u, line, sizeof line, 500);
        if (n > 0 && !strncmp(line, "AT+GVER=", 8)) {
            snprintf(buf, (size_t)cap, "%s", line + 8);
            return 0;
        }
    }
    return -1;
}

int bh1098g_headset_call(hal_uart_t *u) {
    if (send(u, "AT+PP=1")) return -1;
    osal_task_delay_us(200000);
    return send(u, "AT+BTSCO=1");
}
