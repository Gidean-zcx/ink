/* Real S31 HAL and board with deterministic SCCB/DVP recorders, no hardware. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef INK_CAMERA_SOURCE
#define INK_CAMERA_SOURCE "../hal_camera_s31.c"
#endif
#include INK_CAMERA_SOURCE

struct osal_mutex { bool held; };
struct hal_i2c_dev { int addr; };
static struct osal_mutex board_lock;
static struct hal_i2c_dev tca = {0x20}, sensor = {0x3c};
static uint8_t expander[8], regs[4][256], page, pointer;
static uint64_t now;
static bool absent, wrong_id, no_frames, fail_write, repeated_nack, display_held, refreshing;
static bool bad_size, no_memory, start_error;
static unsigned cut_frame, reset_frame, frames, saved_frame, output_requests, sensor_resets, writes, table_at;
static bool check_table;
struct test_camera {
    bool enabled, running;
    esp_cam_ctlr_evt_cbs_t cb;
    void *arg;
    esp_cam_ctlr_trans_t current;
};
static struct test_camera ctl;
static uint8_t output[INK_CAM_BYTES];

osal_mutex_t *osal_mutex_create(void) { return &board_lock; }
osal_err_t osal_mutex_lock(osal_mutex_t *m, int64_t timeout) {
    assert(m == &board_lock && !m->held && timeout == OSAL_WAIT_FOREVER);
    m->held = true; return OSAL_OK;
}
void osal_mutex_unlock(osal_mutex_t *m) { assert(m == &board_lock && m->held); m->held = false; }
uint64_t osal_now_us(void) { return now; }
void osal_task_delay_us(uint64_t us) { now += us; }
void vTaskDelay(uint32_t ticks) { now += ticks * 1000u; }
void esp_rom_delay_us(unsigned us) { now += us; }
void hal_log_write(const char *line) { assert(line); }
hal_i2c_dev_t *hal_i2c_device(int bus, uint8_t addr, const char *name) {
    assert(!bus && name); assert(addr == 0x20 || addr == 0x3c);
    return addr == 0x20 ? &tca : &sensor;
}
hal_i2c_status_t hal_i2c_write(hal_i2c_dev_t *d, const uint8_t *b, size_t n) {
    if (d == &tca) {
        assert(n == 2 && b[0] < 8);
        if (b[0] == 2 || b[0] == 3) assert(!refreshing); /* shared outputs never change in a waveform */
        expander[b[0]] = b[1]; return HAL_I2C_OK;
    }
    assert(d == &sensor && display_held && S.controller);
    if (absent) return HAL_I2C_NACK;
    assert(now >= 10000); /* MCLK settled before the first SCCB transfer. */
    if (!n) return HAL_I2C_OK;
    pointer = b[0];
    if (n == 1) return HAL_I2C_OK;
    assert(n == 2);
    if (fail_write && b[0] != 0xfe) return HAL_I2C_NACK;
    if (check_table && (table_at || (b[0] == 0xfe && b[1] == 0xf0)) && table_at < sizeof kGc2145Vga / sizeof *kGc2145Vga) {
        while (kGc2145Vga[table_at].reg == 0xff) table_at++;
        assert(b[0] == kGc2145Vga[table_at].reg && b[1] == kGc2145Vga[table_at].value);
        table_at++;
    }
    writes++;
    if (b[0] == 0xfe) {
        if (b[1] == 0xf0) { memset(regs, 0, sizeof regs); page = 0; sensor_resets++; }
        else page = b[1] & 3;
    } else regs[page][b[0]] = b[1];
    return HAL_I2C_OK;
}
hal_i2c_status_t hal_i2c_write_read(hal_i2c_dev_t *d, const uint8_t *w, size_t wn, uint8_t *r, size_t rn) {
    assert(rn == 1);
    if (d == &tca) {
        assert(wn == 1 && *w < 8);
        *r = *w < 2 ? (uint8_t)(expander[2 + *w] | expander[6 + *w]) : expander[*w];
        return HAL_I2C_OK;
    }
    assert(d == &sensor && display_held && S.controller);
    if (absent || (wn && repeated_nack)) return HAL_I2C_NACK;
    if (wn) { assert(wn == 1); pointer = *w; }
    *r = pointer == 0xf0 ? 0x21 : pointer == 0xf1 ? (wrong_id ? 0x44 : 0x45) : regs[page][pointer];
    return HAL_I2C_OK;
}
bool ink_s31_display_acquire(uint32_t timeout_ms) {
    assert(!display_held && ink_s31_camera_busy());
    if (refreshing) {
        now += 300000;
        refreshing = false;
        if (timeout_ms < 300) return false;
    }
    display_held = true;
    return true;
}
void ink_s31_display_release(void) { assert(display_held && !ctl.running); display_held = false; }
void gpio_reset_pin(int pin) { assert(pin == 55 && !ctl.running); }
void heap_caps_free(void *p) { assert(!ctl.running); free(p); }
SemaphoreHandle_t xSemaphoreCreateMutexStatic(StaticSemaphore_t *s) { *s = (StaticSemaphore_t){.mutex = true}; return s; }
SemaphoreHandle_t xSemaphoreCreateBinary(void) { return calloc(1, sizeof(StaticSemaphore_t)); }
void xSemaphoreGive(SemaphoreHandle_t s) { assert(s->mutex && s->held); s->held = false; }
void xSemaphoreGiveFromISR(SemaphoreHandle_t s, BaseType_t *wake) { s->signalled = true; *wake = pdFALSE; }
void vSemaphoreDelete(SemaphoreHandle_t s) { assert(!ctl.running); free(s); }
static void next_frame(void) {
    ctl.current = (esp_cam_ctlr_trans_t){0};
    ctl.cb.on_get_new_trans(&ctl, &ctl.current, ctl.arg);
    if (ctl.current.buffer) { saved_frame = frames + 1; output_requests++; }
}
int xSemaphoreTake(SemaphoreHandle_t s, uint32_t ticks) {
    if (s->mutex) {
        if (s->held) return pdFALSE;
        s->held = true; return pdTRUE;
    }
    if (s->signalled) { s->signalled = false; return pdTRUE; }
    now += ticks * 1000u;
    if (ctl.running && !no_frames) {
        frames++;
        if (cut_frame == frames) expander[2] &= ~MB_P0_CAM_SCR;
        if (reset_frame == frames) expander[3] &= ~MB_P1_SHARED_RST;
        esp_cam_ctlr_trans_t done = ctl.current;
        if (done.buffer) { memset(done.buffer, (int)frames, INK_CAM_BYTES); done.received_size = bad_size ? 8 : INK_CAM_BYTES; }
        next_frame(); /* Match IDF: next DMA starts BEFORE completion is delivered. */
        if (done.buffer) ctl.cb.on_trans_finished(&ctl, &done, ctl.arg);
    }
    if (s->signalled) { s->signalled = false; return pdTRUE; }
    return pdFALSE;
}
esp_err_t esp_cam_new_dvp_ctlr(const esp_cam_ctlr_dvp_config_t *c, esp_cam_ctlr_handle_t *out) {
    assert(display_held && !refreshing);
    assert(c->h_res == 640 && c->v_res == 480 && c->cam_data_width == 8 && c->dma_burst_size == 64);
    assert(c->input_data_color_type == CAM_CTLR_COLOR_RGB565 && c->output_data_color_type == CAM_CTLR_COLOR_RGB565);
    assert(!c->external_xtal && c->xclk_freq == 20000000);
    assert(c->pin->pclk_io == 54 && c->pin->xclk_io == 55 && c->pin->vsync_io == 56 && c->pin->de_io == 57);
    for (int i = 0; i < 16; i++) assert(c->pin->data_io[i] == (i < 8 ? 46 + i : GPIO_NUM_NC));
    memset(&ctl, 0, sizeof ctl); *out = &ctl; return ESP_OK;
}
void *esp_cam_ctlr_alloc_buffer(esp_cam_ctlr_handle_t c, size_t n, uint32_t caps) {
    assert(c == &ctl && n == INK_CAM_BYTES && caps == (MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA));
    return no_memory ? NULL : malloc(n);
}
esp_err_t esp_cam_ctlr_register_event_callbacks(esp_cam_ctlr_handle_t c, const esp_cam_ctlr_evt_cbs_t *cb, void *arg) { c->cb = *cb; c->arg = arg; return 0; }
esp_err_t esp_cam_ctlr_enable(esp_cam_ctlr_handle_t c) { assert(!c->enabled); c->enabled = true; return 0; }
esp_err_t esp_cam_ctlr_start(esp_cam_ctlr_handle_t c) { assert(c->enabled); c->running = true; next_frame(); return start_error ? -1 : 0; }
esp_err_t esp_cam_ctlr_stop(esp_cam_ctlr_handle_t c) { assert(c->running); c->running = false; return 0; }
esp_err_t esp_cam_ctlr_disable(esp_cam_ctlr_handle_t c) { assert(c->enabled && !c->running); c->enabled = false; return 0; }
esp_err_t esp_cam_ctlr_del(esp_cam_ctlr_handle_t c) { assert(!c->running && !c->enabled && display_held); return 0; }
static void reset(void) {
    assert(!ink_s31_camera_busy() && !display_held);
    absent = wrong_id = no_frames = fail_write = repeated_nack = bad_size = no_memory = start_error = false;
    cut_frame = reset_frame = frames = saved_frame = output_requests = sensor_resets = writes = 0;
    check_table = false; table_at = 0;
    now = 0; ink_cam_checked = ink_cam_present = false;
    assert(metalio_board_init() == HAL_I2C_OK);
    refreshing = true; /* Detect must wait for the running waveform before touching LCD_CAM. */
}
static void present(void) {
    hal_camera_info_t info;
    assert(hal_camera_info(&info));
    assert(info.width == 640 && info.height == 480 && info.frame_bytes == sizeof output);
    assert(expander[2] == MB_P0_CAM_SCR && (expander[3] & MB_P1_SHARED_RST));
    assert(!ink_s31_camera_busy() && !ctl.running && !display_held);
}
int main(void) {
    hal_camera_info_t info = {1, 1, 1};
    reset(); absent = true; assert(!hal_camera_info(&info) && !info.width && !info.height && !info.frame_bytes);
    absent = false; assert(!hal_camera_info(&info)); /* Failed boot detection stays absent. */
    assert(hal_camera_capture(output, sizeof output, 3000) == -1);
    reset(); wrong_id = true; assert(!hal_camera_info(&info));
    reset(); repeated_nack = true; present();
    reset(); present();
    assert(hal_camera_capture(NULL, sizeof output, 3000) == -1);
    assert(hal_camera_capture(output, sizeof output - 1, 3000) == -1);
    check_table = true;
    assert(hal_camera_capture(output, sizeof output, 3000) == sizeof output);
    assert(table_at == sizeof kGc2145Vga / sizeof *kGc2145Vga);
    assert(sensor_resets == 1 && saved_frame == 9 && frames == 9 && output_requests == 1);
    for (size_t i = 0; i < sizeof output; i++) assert(output[i] == 9);
    assert(regs[0][0xf2] == 0 && expander[2] == MB_P0_CAM_SCR && (expander[3] & MB_P1_SHARED_RST));
    assert(!display_held && !ctl.running && !ink_s31_camera_busy());
    reset(); present(); no_frames = true; assert(hal_camera_capture(output, sizeof output, 80) == -4);
    reset(); present(); refreshing = true; assert(hal_camera_capture(output, sizeof output, 80) == -4);
    reset(); present(); cut_frame = 4; assert(hal_camera_capture(output, sizeof output, 3000) == -5);
    reset(); present(); reset_frame = 4; assert(hal_camera_capture(output, sizeof output, 3000) == -5);
    reset(); present(); bad_size = true; assert(hal_camera_capture(output, sizeof output, 3000) == -6);
    reset(); present(); fail_write = true; assert(hal_camera_capture(output, sizeof output, 3000) == -3);
    reset(); present(); absent = true; assert(hal_camera_capture(output, sizeof output, 3000) == -2);
    reset(); present(); no_memory = true; assert(hal_camera_capture(output, sizeof output, 3000) == -3);
    reset(); present(); start_error = true; assert(hal_camera_capture(output, sizeof output, 3000) == -3);
    reset(); present(); assert(hal_camera_capture(output, sizeof output, 3000) == sizeof output);
    puts("s31 camera: detection, ID, SCCB fallback, warmup, timeout, power/reset loss, cleanup and shared lines passed");
    return 0;
}
