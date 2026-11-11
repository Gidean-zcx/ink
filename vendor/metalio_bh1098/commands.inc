/* Complete Bluetooth mode/SCO command functions @874538e, with HAL seams. */
int bh1098g_set_mode(hal_uart_t *u, int mode) {
    switch (mode) {
        case 1:
            if (send(u, "AT+RX=2")) return -1;
            osal_task_delay_us(700000);
            return send(u, "AT+MODE=1");
        case 2:
            if (send(u, "AT+TX=1")) return -1;
            osal_task_delay_us(700000);
            return send(u, "AT+MODE=2");
        case 3:
            if (send(u, "AT+RX=1")) return -1;
            osal_task_delay_us(700000);
            return send(u, "AT+MODE=3");
        default:
            return -1;
    }
}

int bh1098g_headset_call(hal_uart_t *u) {
    if (send(u, "AT+PP=1")) return -1;
    osal_task_delay_us(200000);
    return send(u, "AT+BTSCO=1");
}

int bh1098g_headset_music(hal_uart_t *u) {
    if (send(u, "AT+BTSCO=0")) return -1;
    osal_task_delay_us(200000);
    return send(u, "AT+PP=1");
}
