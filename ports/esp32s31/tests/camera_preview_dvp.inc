/* Deterministic 15 fps DVP input; real sensor registers and panel use the simulator. */
struct test_camera {
    bool enabled, running;
    esp_cam_ctlr_evt_cbs_t callbacks;
    void *arg;
    esp_cam_ctlr_trans_t current;
};
static struct test_camera dvp;
static StaticSemaphore_t dvp_done;
static unsigned dvp_allocations, dvp_releases, dvp_frames;
static void dvp_next(void) {
    dvp.current = (esp_cam_ctlr_trans_t){0};
    dvp.callbacks.on_get_new_trans(&dvp, &dvp.current, dvp.arg);
}
static void dvp_fill(void *arg) {
    uint8_t *p = arg;
    for (size_t i = 0; i < INK_CAM_BYTES; i++) p[i] = (uint8_t)(i + dvp_frames);
}
SemaphoreHandle_t xSemaphoreCreateMutexStatic(StaticSemaphore_t *s) { *s = (StaticSemaphore_t){.mutex = true}; return s; }
SemaphoreHandle_t xSemaphoreCreateBinary(void) { dvp_done = (StaticSemaphore_t){0}; return &dvp_done; }
int xSemaphoreTake(SemaphoreHandle_t s, uint32_t ticks) {
    if (s->mutex) {
        if (s->held) return pdFALSE;
        s->held = true;
        return pdTRUE;
    }
    if (s->signalled) { s->signalled = false; return pdTRUE; }
    if (!ticks) return pdFALSE;
    osal_task_delay_us(1000000u / 15);
    if (dvp.running) {
        dvp_frames++;
        esp_cam_ctlr_trans_t done = dvp.current;
        if (done.buffer) { vtk_host_call(dvp_fill, done.buffer); done.received_size = INK_CAM_BYTES; }
        dvp_next();
        if (done.buffer) dvp.callbacks.on_trans_finished(&dvp, &done, dvp.arg);
    }
    if (s->signalled) { s->signalled = false; return pdTRUE; }
    return pdFALSE;
}
void xSemaphoreGive(SemaphoreHandle_t s) { s->held = false; }
void xSemaphoreGiveFromISR(SemaphoreHandle_t s, BaseType_t *wake) { s->signalled = true; *wake = pdFALSE; }
void vSemaphoreDelete(SemaphoreHandle_t s) { (void)s; }
void vTaskDelay(uint32_t ticks) { osal_task_delay_us((uint64_t)ticks * 1000); }
void esp_rom_delay_us(unsigned us) { osal_task_delay_us(us); }
void gpio_reset_pin(int pin) { (void)pin; }
void heap_caps_free(void *p) {
    if (p) dvp_releases++;
    osal_free(p);
}
esp_err_t esp_cam_new_dvp_ctlr(const esp_cam_ctlr_dvp_config_t *c, esp_cam_ctlr_handle_t *out) {
    (void)c; dvp = (struct test_camera){0}; *out = &dvp; return ESP_OK;
}
void *esp_cam_ctlr_alloc_buffer(esp_cam_ctlr_handle_t c, size_t bytes, uint32_t caps) {
    (void)c; (void)caps; dvp_allocations++; return osal_malloc(bytes, OSAL_MEM_EXTERNAL);
}
esp_err_t esp_cam_ctlr_register_event_callbacks(esp_cam_ctlr_handle_t c, const esp_cam_ctlr_evt_cbs_t *cb, void *arg) {
    c->callbacks = *cb; c->arg = arg; return ESP_OK;
}
esp_err_t esp_cam_ctlr_enable(esp_cam_ctlr_handle_t c) { c->enabled = true; return ESP_OK; }
esp_err_t esp_cam_ctlr_start(esp_cam_ctlr_handle_t c) { c->running = true; dvp_next(); return ESP_OK; }
esp_err_t esp_cam_ctlr_stop(esp_cam_ctlr_handle_t c) { c->running = false; return ESP_OK; }
esp_err_t esp_cam_ctlr_disable(esp_cam_ctlr_handle_t c) { c->enabled = false; return ESP_OK; }
esp_err_t esp_cam_ctlr_del(esp_cam_ctlr_handle_t c) { (void)c; return ESP_OK; }
bool ink_s31_display_acquire(uint32_t timeout) { (void)timeout; ink_display_flash_guard(true); return true; }
void ink_s31_display_release(void) { ink_display_flash_guard(false); }
