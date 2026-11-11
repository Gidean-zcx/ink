/* CloudZao/Metalio-E-INK4-Plus @874538e; MIT, see LICENSE and SOURCE.md. */
static void epd_board_poweron_once(epd_ctrl_state_t* state) {
    state->ok = false;
    const int vcom = (int)state->power->vcom_mv;
    if (!state->power->ioexp) {
        ESP_LOGE(TAG, "poweron skipped: no IO expander");
        return;
    }
    // Restore the schematic's original VBORDER bias. GPIO37 HIGH in 2.0.67
    // did not resolve the reported flashing of addressable edge pixels.
    gpio_set_level(EPD_BORDER,0);
    epd_ctrl_state_t mask = {
        .ep_output_enable = true,
        .ep_mode = true,
        .ep_stv = true,
    };

    state->ep_output_enable = false;
    state->ep_stv = true;
    state->ep_mode = false;

    // 1) PWRUP 低 + WAKEUP 高 → STANDBY，I2C 可用
    if (set_ctrl_with_tps(state, &mask, CFG_P0_TPS_OUT_MASK, CFG_PIN_WAKEUP) != HAL_I2C_OK) return;
    // 深睡后 TPS 从 SLEEP 醒：多等一会再碰寄存器
    vTaskDelay(pdMS_TO_TICKS(50));

    hal_i2c_status_t probe = HAL_I2C_ERR;
    probe = tps_probe(state->power->pmic);
    if (probe != HAL_I2C_OK) {
        ESP_LOGE(TAG, "TPS probe failed: %s", hal_i2c_status_name(probe));
        return;
    }

    // 2) 先读版本号
    uint8_t revid = 0;
    hal_i2c_status_t revid_err = tps_read_register_ex(state->power->pmic, TPS_REG_REVID, &revid);
    ESP_LOGI(TAG, "STANDBY REVID(%s)=0x%02X probe=%s", hal_i2c_status_name(revid_err), revid,
             hal_i2c_status_name(probe));

    state->power->revid = revid;

    // 3) 配置升压时序；写失败交给完整上电重试。
    hal_i2c_status_t up0 = tps_write_register(state->power->pmic, TPS_REG_UPSEQ0, 0xE4);
    hal_i2c_status_t up1 = tps_write_register(state->power->pmic, TPS_REG_UPSEQ1, 0x55);
    ESP_LOGI(TAG, "UPSEQ0 write %s read=0x%02X; UPSEQ1 write %s read=0x%02X", hal_i2c_status_name(up0),
             tps_read_register(state->power->pmic, TPS_REG_UPSEQ0), hal_i2c_status_name(up1),
             tps_read_register(state->power->pmic, TPS_REG_UPSEQ1));

    if (revid_err != HAL_I2C_OK || up0 != HAL_I2C_OK || up1 != HAL_I2C_OK) {
        ESP_LOGE(TAG, "TPS standby configuration failed");
        return;
    }

    // PWRUP 拉高前：读 INT2(0x08) / PG(0x0F)
    uint8_t int2_pre = 0;
    uint8_t pg_pre = 0;
    hal_i2c_status_t e_int2_pre = tps_read_register_ex(state->power->pmic, TPS_REG_INT2, &int2_pre);
    hal_i2c_status_t e_pg_pre = tps_read_register_ex(state->power->pmic, TPS_REG_PG, &pg_pre);
    ESP_LOGI(TAG, "before PWRUP: INT2(%s)=0x%02X PG(%s)=0x%02X", hal_i2c_status_name(e_int2_pre),
             int2_pre, hal_i2c_status_name(e_pg_pre), pg_pre);

    // 4) 配置成功后拉高 PWRUP（上升沿）
    if (set_ctrl_with_tps(state, &mask, CFG_PIN_PWRUP | CFG_PIN_VCOM_CTRL,
                          CFG_PIN_PWRUP | CFG_PIN_VCOM_CTRL) != HAL_I2C_OK) return;
    ESP_LOGI(TAG, "PWRUP high (after UPSEQ)");
    vTaskDelay(pdMS_TO_TICKS(50)); // 等升压启动片刻再采一次

    // PWRUP 拉高后：再读 INT2 / PG
    uint8_t int2_post = 0;
    uint8_t pg_post = 0;
    hal_i2c_status_t e_int2_post = tps_read_register_ex(state->power->pmic, TPS_REG_INT2, &int2_post);
    hal_i2c_status_t e_pg_post = tps_read_register_ex(state->power->pmic, TPS_REG_PG, &pg_post);
    ESP_LOGI(TAG, "after PWRUP+50ms: INT2(%s)=0x%02X PG(%s)=0x%02X", hal_i2c_status_name(e_int2_post),
             int2_post, hal_i2c_status_name(e_pg_post), pg_post);

    bool success = false;
    if (set_ctrl_with_tps(state, &mask, CFG_PIN_PWRUP | CFG_PIN_VCOM_CTRL,
                          CFG_PIN_PWRUP | CFG_PIN_VCOM_CTRL) != HAL_I2C_OK) return;

    // demo：只等扩展器 GPIO PWR_GOOD，成功前不写 ENABLE
    for (int wait_ms = 0; wait_ms < 200; ++wait_ms) {
        uint8_t p0 = pca9555_read_input(state->power, CFG_PORT_CTRL);
        if (p0 & CFG_PIN_PWRGOOD) {
            ESP_LOGI(TAG, "PWR_GOOD ok p0=0x%02X @%dms", p0, wait_ms);
            success = true;
            break;
        }
        if (wait_ms == 0 || (wait_ms % 50) == 49) {
            uint8_t en = (probe == HAL_I2C_OK) ? tps_read_register(state->power->pmic, TPS_REG_ENABLE) : 0;
            uint8_t pg = (probe == HAL_I2C_OK) ? tps_read_register(state->power->pmic, TPS_REG_PG) : 0;
            uint8_t int1 = (probe == HAL_I2C_OK) ? tps_read_register(state->power->pmic, TPS_REG_INT1) : 0;
            uint8_t int2 = (probe == HAL_I2C_OK) ? tps_read_register(state->power->pmic, TPS_REG_INT2) : 0;
            ESP_LOGI(TAG, "wait GPIO PG... p0=0x%02X ENABLE=0x%02X PG=0x%02X INT1=0x%02X INT2=0x%02X",
                     p0, en, pg, int1, int2);
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    if (!success) {
        uint8_t p0 = pca9555_read_input(state->power, CFG_PORT_CTRL);
        uint8_t en = (probe == HAL_I2C_OK) ? tps_read_register(state->power->pmic, TPS_REG_ENABLE) : 0;
        uint8_t pg = (probe == HAL_I2C_OK) ? tps_read_register(state->power->pmic, TPS_REG_PG) : 0;
        uint8_t int1 = (probe == HAL_I2C_OK) ? tps_read_register(state->power->pmic, TPS_REG_INT1) : 0;
        uint8_t int2 = (probe == HAL_I2C_OK) ? tps_read_register(state->power->pmic, TPS_REG_INT2) : 0;
        ESP_LOGE(TAG,
                 "TPS PowerOn failed; p0=0x%02X REVID=0x%02X ENABLE=0x%02X INT1=0x%02X INT2=0x%02X "
                 "PG=0x%02X (check VDD4.2V / L6 L7)",
                 p0, revid, en, int1, int2, pg);
        state->ok = false;
        return;
    }

    // demo：GPIO PG 成功后再写寄存器
    if (probe == HAL_I2C_OK) {
        hal_i2c_status_t err = tps_write_register(state->power->pmic, TPS_REG_ENABLE, 0x3F);
        if (err != HAL_I2C_OK) {
            ESP_LOGE(TAG, "tps ENABLE write: %s", hal_i2c_status_name(err));
            state->ok = false;
            return;
        }
        const unsigned value = vcom / 10;
        err = tps_write_register(state->power->pmic, 4, (value & 0x100) >> 8);
        if (err == HAL_I2C_OK) err = tps_write_register(state->power->pmic, 3, value & 0xff);
        if (err != HAL_I2C_OK) {
            ESP_LOGE(TAG, "TPS VCOM write: %s", hal_i2c_status_name(err));
            return;
        }
    }

    state->ep_sth = true;
    epd_ctrl_state_t sth_mask = {.ep_sth = true};
    epd_board_set_ctrl(state, &sth_mask);

    int tries = 0;
    while (probe == HAL_I2C_OK && !((tps_read_register(state->power->pmic, TPS_REG_PG) & 0xFA) == 0xFA)) {
        if (tries >= 200) {
            ESP_LOGE(TAG, "TPS PG register timeout");
            state->ok = false;
            return;
        }
        tries++;
        vTaskDelay(1);
    }

    state->ep_output_enable = true;
    epd_ctrl_state_t out_mask = {.ep_output_enable = true};
    if (set_ctrl_with_tps(state, &out_mask, 0, 0) != HAL_I2C_OK) return;
    state->ok = true;
    ESP_LOGI(TAG, "TPS PowerOn ok vcom=%d mV", vcom);
}

static void epd_board_poweroff(epd_ctrl_state_t* state);

static void epd_board_poweron(epd_ctrl_state_t* state) {
    if (!state->power->ioexp) {
        state->ok = false;
        ESP_LOGE(TAG, "poweron skipped: no IO expander");
        return;
    }
    // Initial attempt plus at most three complete power-cycle retries.
    for (int attempt = 0; attempt <= 3; ++attempt) {
        state->power->attempts++;
        epd_board_poweron_once(state);
        if (state->ok) return;
        epd_board_poweroff(state);
        if (attempt < 3) {
            state->power->retries++;
            ESP_LOGW(TAG, "TPS power-on retry %d/3", attempt + 1);
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
    ESP_LOGE(TAG, "TPS power-on failed after 3 retries; restarting");
    /* Return failure to ink; do not restart the product. */
}

static void epd_board_poweroff(epd_ctrl_state_t* state) {
    state->ok = false;
    epd_ctrl_state_t mask = {
        .ep_stv = true,
        .ep_output_enable = true,
        .ep_mode = true,
    };
    state->ep_stv = false;
    state->ep_output_enable = false;
    state->ep_mode = false;
    epd_board_set_ctrl(state, &mask);
    // E0470A14-AF 6.1: XOE LOW must precede VPOS/VNEG OFF by >=12us.
    esp_rom_delay_us(12);
    if (set_ctrl_with_tps(state, &mask, CFG_PIN_VCOM_CTRL | CFG_PIN_PWRUP, 0) != HAL_I2C_OK) return;
    vTaskDelay(1);
    set_ctrl_with_tps(state, &mask, CFG_PIN_WAKEUP, 0);
}
