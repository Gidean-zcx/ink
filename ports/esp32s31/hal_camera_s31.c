/* GC2145 DVP, C adaptation of CloudZao v2.0.77 dvp_camera.cc:50-239.
 * Copyright (c) 2025 Shenzhen Xinzhi Future Technology Co., Ltd.
 * Copyright (c) 2025 Project Contributors. MIT: vendor/gc2145/LICENSE-MIT. */
#include <string.h>
#include "driver/gpio.h"
#include "esp_attr.h"
#include "esp_cam_ctlr_dvp.h"
#include "esp_heap_caps.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "ink/hal_camera.h"
#include "ink/hal_device.h"
#include "ink/osal.h"
#include "metalio_plus/metalio_board.h"
#include "s31_io.h"
#include "vendor/gc2145/gc2145_vga_settings.h"

#define INK_CAM_BYTES (640 * 480 * 2)
#define INK_CAM_MCLK 55
#define INK_CAM_WARMUP 8u
static hal_i2c_dev_t *ink_cam;
static bool ink_cam_checked, ink_cam_present;
static int ink_cam_active;
static StaticSemaphore_t ink_cam_lock_buf;
static SemaphoreHandle_t ink_cam_lock;
static portMUX_TYPE ink_cam_mux = portMUX_INITIALIZER_UNLOCKED;
/* Internal RAM: ISR context must not live on the media executor's PSRAM stack. */
static struct {
    esp_cam_ctlr_handle_t controller;
    SemaphoreHandle_t done;
    uint8_t *frame;
    unsigned requests;
    size_t received;
    uint64_t deadline;
    bool stream, enabled, started;
} ink_cam_session;
#define S ink_cam_session

bool ink_s31_camera_busy(void) { return __atomic_load_n(&ink_cam_active, __ATOMIC_ACQUIRE) != 0; }
static bool ink_cam_enter(void) {
    taskENTER_CRITICAL(&ink_cam_mux);
    if (!ink_cam_lock) ink_cam_lock = xSemaphoreCreateMutexStatic(&ink_cam_lock_buf);
    taskEXIT_CRITICAL(&ink_cam_mux);
    return ink_cam_lock && xSemaphoreTake(ink_cam_lock, 0) == pdTRUE;
}
/* camera_acquire, epd_board_metalio_eink4_plus.c:146-179: read only, no P05/P12 pulse. */
static int ink_cam_check(void) {
    uint8_t p0 = 0, p1 = 0;
    if (!metalio_board_ready() || metalio_board_read(0, &p0) != HAL_I2C_OK ||
        metalio_board_read(1, &p1) != HAL_I2C_OK || !(p0 & MB_P0_CAM_SCR) || !(p1 & MB_P1_SHARED_RST)) return -5;
    return osal_now_us() >= S.deadline ? -4 : 0;
}
static int ink_cam_write(uint8_t reg, uint8_t value) {
    int r = ink_cam_check();
    uint8_t bytes[] = {reg, value};
    if (!r && hal_i2c_write(ink_cam, bytes, sizeof bytes) != HAL_I2C_OK) r = -3;
    return r ? r : ink_cam_check();
}
static int ink_cam_read(uint8_t reg, uint8_t *value) {
    int r = ink_cam_check();
    if (r) return r;
    hal_i2c_status_t status = hal_i2c_write_read(ink_cam, &reg, 1, value, 1);
    /* power_i2c.c:52-65: repeated START, then STOP-separated fallback. */
    if (status != HAL_I2C_OK && !ink_cam_check()) {
        esp_rom_delay_us(100);
        status = hal_i2c_write(ink_cam, &reg, 1);
        if (status == HAL_I2C_OK) {
            esp_rom_delay_us(100);
            status = hal_i2c_write_read(ink_cam, NULL, 0, value, 1);
        }
    }
    r = ink_cam_check();
    return r ? r : status == HAL_I2C_OK ? 0 : -2;
}
/* dvp_camera.cc:178-219; config.h:73-79. Backup DMA receives every other frame. */
static bool IRAM_ATTR ink_cam_next(esp_cam_ctlr_handle_t ctl, esp_cam_ctlr_trans_t *trans, void *arg) {
    (void)ctl; (void)arg;
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
    int r = ink_cam_check();
    if (r) return r;
    esp_cam_ctlr_dvp_pin_config_t pins = {.data_width = CAM_CTLR_DATA_WIDTH_8,
        .pclk_io = 54, .vsync_io = 56, .de_io = 57, .xclk_io = INK_CAM_MCLK};
    for (int i = 0; i < ESP_CAM_CTLR_DVP_DATA_SIG_NUM; i++) pins.data_io[i] = i < 8 ? 46 + i : GPIO_NUM_NC;
    esp_cam_ctlr_dvp_config_t config = {.clk_src = CAM_CLK_SRC_DEFAULT, .h_res = 640, .v_res = 480,
        .input_data_color_type = CAM_CTLR_COLOR_RGB565, .output_data_color_type = CAM_CTLR_COLOR_RGB565,
        .cam_data_width = 8, .external_xtal = false, .xclk_freq = 20000000, .dma_burst_size = 64, .pin = &pins};
    if (esp_cam_new_dvp_ctlr(&config, &S.controller) != ESP_OK) return -3;
    /* dvp_camera.cc:95-131: MCLK first, settle, ACK, page 0, two separate PID reads. */
    vTaskDelay(pdMS_TO_TICKS(10));
    if ((r = ink_cam_check())) return r;
    if (hal_i2c_write(ink_cam, NULL, 0) != HAL_I2C_OK) return -2;
    if ((r = ink_cam_write(0xfe, 0))) return r;
    uint8_t high = 0, low = 0;
    if ((r = ink_cam_read(0xf0, &high)) || (r = ink_cam_read(0xf1, &low))) return r;
    hal_log_write(high == 0x21 && low == 0x45 ? "camera: SCCB PID=0x2145" : "camera: SCCB PID mismatch");
    return high == 0x21 && low == 0x45 ? 0 : -2;
}
static int ink_cam_grab(void) {
    int r = 0;
    S.stream = true; /* dvp_camera.cc:133-173: partial table also needs output-off cleanup. */
    for (size_t i = 0; i < sizeof kGc2145Vga / sizeof *kGc2145Vga; i++) {
        if ((r = ink_cam_check())) return r;
        if (kGc2145Vga[i].reg == 0xff) osal_task_delay_us((uint64_t)kGc2145Vga[i].value * 1000);
        else if ((r = ink_cam_write(kGc2145Vga[i].reg, kGc2145Vga[i].value))) return r;
    }
    if ((r = ink_cam_write(0xfe, 0)) || (r = ink_cam_write(0xf2, 0))) return r;
    S.frame = esp_cam_ctlr_alloc_buffer(S.controller, INK_CAM_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA);
    S.done = xSemaphoreCreateBinary();
    if (!S.frame || !S.done) return -3;
    esp_cam_ctlr_evt_cbs_t callbacks = {.on_get_new_trans = ink_cam_next, .on_trans_finished = ink_cam_done};
    if (esp_cam_ctlr_register_event_callbacks(S.controller, &callbacks, NULL) != ESP_OK ||
        esp_cam_ctlr_enable(S.controller) != ESP_OK) return -3;
    S.enabled = true;
    S.started = true; /* IDF start can change state even on failure. */
    if (esp_cam_ctlr_start(S.controller) != ESP_OK) return -3;
    if ((r = ink_cam_write(0xf2, 0x0f))) return r;
    do {
        if ((r = ink_cam_check())) return r;
    } while (xSemaphoreTake(S.done, pdMS_TO_TICKS(20)) != pdTRUE);
    if ((r = ink_cam_check())) return r;
    return S.received == INK_CAM_BYTES ? 0 : -6;
}
/* dvp_camera.cc:54-70: stop DMA/ISR before deleting its context or buffer. */
static void ink_cam_cleanup(void) {
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
    if (S.done) vSemaphoreDelete(S.done);
}
static int ink_cam_run(uint8_t *buf, uint32_t timeout_ms) {
    memset(&S, 0, sizeof S);
    S.deadline = osal_now_us() + (uint64_t)timeout_ms * 1000;
    __atomic_store_n(&ink_cam_active, 1, __ATOMIC_RELEASE);
    /* Detect/Capture both hold EpdFlashGuard in v2.0.77:232-249. */
    int r = -4;
    if (ink_s31_display_acquire(timeout_ms)) {
        r = ink_cam_prepare();
        if (!r && buf) r = ink_cam_grab();
        ink_cam_cleanup();
        if (!r && buf) memcpy(buf, S.frame, INK_CAM_BYTES);
        heap_caps_free(S.frame);
        ink_s31_display_release();
    }
    __atomic_store_n(&ink_cam_active, 0, __ATOMIC_RELEASE);
    return r;
}
bool hal_camera_info(hal_camera_info_t *out) {
    if (out) *out = (hal_camera_info_t){0};
    if (!ink_cam_enter()) return false;
    if (!ink_cam_checked) {
        ink_cam = hal_i2c_device(0, 0x3c, "camera");
        ink_cam_present = ink_cam && !ink_cam_run(NULL, 8000);
        ink_cam_checked = true;
    }
    bool present = ink_cam_present;
    if (present && out) *out = (hal_camera_info_t){640, 480, INK_CAM_BYTES};
    xSemaphoreGive(ink_cam_lock);
    return present;
}
int hal_camera_capture(uint8_t *buf, size_t cap, uint32_t timeout_ms) {
    if (!ink_cam_present || !buf || cap < INK_CAM_BYTES || !ink_cam_enter()) return -1;
    int r = ink_cam_run(buf, timeout_ms);
    xSemaphoreGive(ink_cam_lock);
    return r ? r : INK_CAM_BYTES;
}
