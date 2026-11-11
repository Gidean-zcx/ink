/* Bluetooth_handle_response_line @874538e: modes 1/2, scan, link and SCO only.
 * UI posts are replaced by the existing HAL state, never raw UART logging. */
static void ink_response(const char *line) {
    if (strstr(line, "SET MODE 1")) {
        B.mode = 1;
        ink_link(false);
        return;
    }
    if (strstr(line, "SET MODE 2")) {
        B.mode = 2;
        return;
    }
    if (strstr(line, "RECONNECT")) {
        return; /* Name only: no confirmed link or member identity. */
    }
    if (strstr(line, "CONNECT SUCCESS")) {
        if (B.mode == 2) ink_link(true);
        return;
    }
    if (strstr(line, "CONNECT TIMEOUT")) {
        ink_link(false);
        return;
    }
    if (strstr(line, "SETUP SCO")) {
        B.sco = true;
        return;
    }
    if (strstr(line, "DISC SCO")) {
        B.sco = false;
        return;
    }
}
