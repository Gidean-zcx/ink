/* Session from dvp_camera.cc:50-229, C/HAL adaptation; see SOURCE.md and LICENSE.
 * Copyright (c) 2025 Shenzhen Xinzhi Future Technology Co., Ltd.
 * Copyright (c) 2025 Project Contributors. */
static int ink_cam_check(void) {
    if (S.cancelled && S.cancelled(S.context)) return -5;
    return osal_now_us() >= S.deadline ? -4 : 0;
}
static int ink_cam_write(uint8_t reg, uint8_t value) {
    int err = ink_cam_check();
    return err == 0 ? ink_cam_bus_write(reg, value) : err;
}
static int ink_cam_read(uint8_t reg, uint8_t *value) {
    int err = ink_cam_check();
    return err == 0 ? ink_cam_bus_read(reg, value) : err;
}
static int ink_cam_init_controller(void) {
    esp_cam_ctlr_dvp_pin_config_t pins = {0};
    pins.data_width = CAM_CTLR_DATA_WIDTH_8;
    for (int i = 0; i < ESP_CAM_CTLR_DVP_DATA_SIG_NUM; ++i)
        pins.data_io[i] = i < 8 ? 46 + i : GPIO_NUM_NC;
    pins.pclk_io = 54;
    pins.vsync_io = 56;
    pins.de_io = 57;
    pins.xclk_io = INK_CAM_MCLK;
    esp_cam_ctlr_dvp_config_t config = {0};
    config.clk_src = CAM_CLK_SRC_DEFAULT;
    config.h_res = 640;
    config.v_res = 480;
    config.input_data_color_type = CAM_CTLR_COLOR_RGB565;
    config.output_data_color_type = CAM_CTLR_COLOR_RGB565;
    config.cam_data_width = 8;
    config.external_xtal = false;
    config.xclk_freq = 20000000;
    config.dma_burst_size = 64;
    config.pin = &pins;
    return esp_cam_new_dvp_ctlr(&config, &S.controller) == ESP_OK ? 0 : -3;
}
static bool IRAM_ATTR ink_cam_next(esp_cam_ctlr_handle_t ctl, esp_cam_ctlr_trans_t *trans, void *arg) {
    (void)ctl; (void)arg;
    /* IDF starts the NEXT DMA before delivering FrameDone. */
    if (++S.requests == INK_CAM_WARMUP + 1) {
        trans->buffer = S.frame;
        trans->buflen = INK_CAM_BYTES;
    }
    return false;
}
static bool IRAM_ATTR ink_cam_done(esp_cam_ctlr_handle_t ctl, esp_cam_ctlr_trans_t *trans, void *arg) {
    (void)ctl; (void)arg;
    if (trans->buffer != S.frame) return false;
    S.received = trans->received_size;
    BaseType_t wake = pdFALSE;
    xSemaphoreGiveFromISR(S.done, &wake);
    return wake == pdTRUE;
}
static int ink_cam_prepare(void) {
    int err = ink_cam_check();
    if (err != 0) return err;
    err = ink_cam_init_controller();
    if (err != 0) return err;
    vTaskDelay(pdMS_TO_TICKS(10));
    err = ink_cam_check();
    if (err != 0) return err;
    err = hal_i2c_write(ink_cam, NULL, 0) == HAL_I2C_OK ? 0 : -2;
    if (err != 0) return err;
    err = ink_cam_write(0xfe, 0);
    if (err != 0) return err;
    uint8_t pid[2] = {0};
    err = ink_cam_read(0xf0, &pid[0]);
    if (err != 0) return err;
    err = ink_cam_read(0xf1, &pid[1]);
    if (err != 0) return err;
    hal_log_write(pid[0] == 0x21 && pid[1] == 0x45 ? "camera: SCCB PID=0x2145" : "camera: SCCB PID mismatch");
    if (pid[0] != 0x21 || pid[1] != 0x45) return -2;
    return ink_cam_check();
}
static int ink_cam_grab(void) {
    int err;
    if (!S.frame) {
        /* Original Grab setup, once per viewfinder session. */
        S.stream = true;
        for (size_t i = 0; i < sizeof kGc2145Vga / sizeof *kGc2145Vga; i++) {
            err = ink_cam_check();
            if (err != 0) return err;
            const struct Gc2145Reg *entry = &kGc2145Vga[i];
            if (entry->reg == 0xff) vTaskDelay(pdMS_TO_TICKS(entry->value));
            else if ((err = ink_cam_write(entry->reg, entry->value)) != 0) return err;
        }
        err = ink_cam_write(0xfe, 0);
        if (err == 0) err = ink_cam_write(0xf2, 0);
        if (err != 0) return err;
        S.frame = esp_cam_ctlr_alloc_buffer(S.controller, INK_CAM_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA);
        S.done = xSemaphoreCreateBinary();
        if (!S.frame || !S.done) return -3;
        esp_cam_ctlr_evt_cbs_t callbacks = {0};
        callbacks.on_get_new_trans = ink_cam_next;
        callbacks.on_trans_finished = ink_cam_done;
        err = esp_cam_ctlr_register_event_callbacks(S.controller, &callbacks, NULL);
        if (err != ESP_OK) return -3;
        err = esp_cam_ctlr_enable(S.controller);
        if (err != ESP_OK) return -3;
        S.enabled = true;
    } else {
        S.requests = INK_CAM_WARMUP;
        S.received = 0;
        xSemaphoreTake(S.done, 0);
    }
    /* start() can change state even on failure. */
    S.started = true;
    err = esp_cam_ctlr_start(S.controller);
    if (err == ESP_OK) err = ink_cam_write(0xf2, 0x0f);
    if (err != ESP_OK) return -3;
    while (xSemaphoreTake(S.done, pdMS_TO_TICKS(20)) != pdTRUE) {
        if ((err = ink_cam_check()) != 0) return err;
    }
    err = ink_cam_check();
    if (err != 0) return err;
    err = esp_cam_ctlr_stop(S.controller);
    if (err != ESP_OK) return -3;
    S.started = false;
    return S.received == INK_CAM_BYTES ? 0 : -6;
}
static void ink_cam_cleanup(void) {
    /* DMA and ISR stop before freeing context/frame. */
    if (S.started) esp_cam_ctlr_stop(S.controller);
    if (S.enabled) esp_cam_ctlr_disable(S.controller);
    if (S.stream) {
        const uint8_t page[] = {0xfe, 0}, stop[] = {0xf2, 0};
        hal_i2c_write(ink_cam, page, sizeof page);
        hal_i2c_write(ink_cam, stop, sizeof stop);
    }
    if (S.controller) {
        esp_cam_ctlr_del(S.controller);
        gpio_reset_pin(INK_CAM_MCLK);
    }
    heap_caps_free(S.frame);
    if (S.done) vSemaphoreDelete(S.done);
    memset(&S, 0, sizeof S);
}
