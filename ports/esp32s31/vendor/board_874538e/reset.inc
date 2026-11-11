/* CloudZao Metalio-E-INK4-Plus @874538e. MIT: LICENSE. Adaptations: SOURCE.md. */
static esp_err_t epd_board_metalio_eink4_plus_bt_power_reset(void) {
    output_lock();
    if (!s_ioexp_ok || s_bt_reset_active) {
        output_unlock();
        return ESP_ERR_INVALID_STATE;
    }
    s_bt_reset_active = true;
    s_bt_reset_motor_on = false;
    s_outputs[CFG_PORT_CTRL].desired &= (uint8_t)~(CFG_PIN_PA_EN | CFG_PIN_MOTOR);
    const esp_err_t pa_err = output_write_locked(CFG_PORT_CTRL);
    if (pa_err != ESP_OK) {
        s_bt_reset_active = false;
        output_unlock();
        return pa_err;
    }
    const esp_err_t low_err = output_write_locked(CFG_PORT_INT);
    esp_err_t motor_on_err = ESP_OK;
    esp_err_t motor_off_err = ESP_OK;
    if (low_err == ESP_OK) {
        s_bt_reset_motor_on = true;
        motor_on_err = output_write_locked(CFG_PORT_CTRL);
    }
    output_unlock();
    if (low_err == ESP_OK) {
        ESP_LOGI(TAG, "BT reset: P14 low, motor pulse %u ms, power wait %lu ms",
                 (unsigned)POWER_DISCHARGE_MOTOR_PULSE_MS, (unsigned long)kBtResetLowMs);
        if (motor_on_err == ESP_OK) {
            vTaskDelay(pdMS_TO_TICKS(POWER_DISCHARGE_MOTOR_PULSE_MS));
        }
        output_lock();
        s_bt_reset_motor_on = false;
        s_outputs[CFG_PORT_CTRL].desired &= (uint8_t)~CFG_PIN_MOTOR;
        motor_off_err = output_write_locked(CFG_PORT_CTRL);
        output_unlock();
        const uint32_t pulse_ms = motor_on_err == ESP_OK ? POWER_DISCHARGE_MOTOR_PULSE_MS : 0;
        if (kBtResetLowMs > pulse_ms) {
            vTaskDelay(pdMS_TO_TICKS(kBtResetLowMs - pulse_ms));
        }
    }
    // 即使拉低失败也尝试恢复电源，并保持共享端口其他位。
    output_lock();
    s_bt_reset_motor_on = false;
    s_outputs[CFG_PORT_CTRL].desired &= (uint8_t)~(CFG_PIN_PA_EN | CFG_PIN_MOTOR);
    const esp_err_t off_err = output_write_locked(CFG_PORT_CTRL);
    s_bt_reset_active = false;
    const esp_err_t high_err = output_write_locked(CFG_PORT_INT);
    const bool rail_on = (s_outputs[CFG_PORT_INT].desired & CFG_PIN_BT_PA_PWR) != 0;
    output_unlock();
    if (high_err != ESP_OK) return high_err;
    if (off_err != ESP_OK) return off_err;
    if (!rail_on) return ESP_ERR_INVALID_STATE;
    if (low_err != ESP_OK) return low_err;
    if (motor_on_err != ESP_OK) return motor_on_err;
    if (motor_off_err != ESP_OK) return motor_off_err;
    ESP_LOGI(TAG, "BT reset: P14 high");
    return ESP_OK;
}

static void epd_board_metalio_eink4_plus_tp_reset(void) {
    if (!s_ioexp_ok) {
        ESP_LOGW(TAG, "TP RST skipped (no TCA)");
        vTaskDelay(pdMS_TO_TICKS(300));
        return;
    }
    SemaphoreHandle_t mu = shared_reset_mutex();
    xSemaphoreTake(mu, portMAX_DELAY);
    const esp_err_t low_err = output_update(CFG_PORT_INT, CFG_PIN_TP_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(10)); // Trst ≥ 5ms
    const esp_err_t high_err = output_update(CFG_PORT_INT, CFG_PIN_TP_RST, CFG_PIN_TP_RST);
    vTaskDelay(pdMS_TO_TICKS(300)); // Trsi ≥ 300ms
    xSemaphoreGive(mu);
    if (low_err == ESP_OK && high_err == ESP_OK) {
        ESP_LOGI(TAG, "TP RST pulse on TCA P1%d (io=%d)", EPD_IO_TP_RST & 7, EPD_IO_TP_RST);
    }
}
