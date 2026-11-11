/* MetalioAudio ApplyMode/ModeWorker @874538e; C HAL seam: see SOURCE.md. */
static bool ink_vendor_power_unchanged(unsigned epoch) {
    return ink_powered() && metalio_board_shared_power_epoch() == epoch;
}

static bool ink_vendor_apply_mode(unsigned epoch) {
    if (!ink_vendor_power_unchanged(epoch) || ink_send("AT+RX=2\r\n")) {
        return false;
    }
    osal_task_delay_us(700000);
    if (!ink_vendor_power_unchanged(epoch)) {
        return false;
    }
    char line[128];
    while (ink_read(line, sizeof line, 0) >= 0) {
        if (strstr(line, "ERROR") || strstr(line, "FAIL")) return false;
    }
    if (ink_send("AT+MODE=1\r\n")) {
        return false;
    }
    const uint64_t deadline = osal_now_us() + 1000 * 1000;
    while (ink_vendor_power_unchanged(epoch) && osal_now_us() < deadline) {
        if (ink_read(line, sizeof line, 10) < 0) continue;
        if (strstr(line, "ERROR") || strstr(line, "FAIL")) {
            return false;
        }
        const char *mode = strstr(line, "SET MODE 1");
        if (mode && (mode[10] == 0 || mode[10] == ' ')) {
            return true;
        }
    }
    return false;
}

static bool ink_vendor_init_mode(unsigned epoch) {
    const int kRetries = 3;
    const uint64_t kBootSettleUs = 700000;
    bool success = false;
    for (int attempt = 0; attempt <= kRetries; ++attempt) {
        if (!ink_vendor_power_unchanged(epoch)) {
            break;
        }
        const int64_t settle = (int64_t)kBootSettleUs -
            (int64_t)(osal_now_us() - metalio_board_shared_power_since_us());
        if (settle > 0) {
            osal_task_delay_us((uint64_t)settle);
        }
        if (ink_vendor_apply_mode(epoch)) {
            success = true;
            break;
        }
        if (attempt == kRetries || !ink_vendor_power_unchanged(epoch)) {
            break;
        }
        osal_task_delay_us(1000000);
    }
    return success;
}
