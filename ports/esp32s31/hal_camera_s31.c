/* GC2145 DVP, retained CloudZao v2.0.77 session; see vendor/dvp_camera/SOURCE.md.
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
static const int *ink_cam_preview;
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
    bool (*cancelled)(void *);
    void *context;
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
static bool ink_cam_cancelled(void *context) {
    (void)context;
    uint8_t p0 = 0, p1 = 0;
    if (!metalio_board_ready() || metalio_board_read(0, &p0) != HAL_I2C_OK ||
        metalio_board_read(1, &p1) != HAL_I2C_OK || !(p0 & MB_P0_CAM_SCR) || !(p1 & MB_P1_SHARED_RST)) return true;
    return ink_cam_preview && !__atomic_load_n(ink_cam_preview, __ATOMIC_ACQUIRE);
}
static int ink_cam_bus_write(uint8_t reg, uint8_t value) {
    uint8_t bytes[] = {reg, value};
    return hal_i2c_write(ink_cam, bytes, sizeof bytes) == HAL_I2C_OK ? 0 : -3;
}
static int ink_cam_bus_read(uint8_t reg, uint8_t *value) {
    int r = ink_cam_cancelled(NULL) ? -5 : osal_now_us() >= S.deadline ? -4 : 0;
    if (r) return r;
    hal_i2c_status_t status = hal_i2c_write_read(ink_cam, &reg, 1, value, 1);
    /* power_i2c.c:52-65: repeated START, then STOP-separated fallback. */
    if (status != HAL_I2C_OK && !ink_cam_cancelled(NULL)) {
        esp_rom_delay_us(100);
        status = hal_i2c_write(ink_cam, &reg, 1);
        if (status == HAL_I2C_OK) {
            esp_rom_delay_us(100);
            status = hal_i2c_write_read(ink_cam, NULL, 0, value, 1);
        }
    }
    r = ink_cam_cancelled(NULL) ? -5 : osal_now_us() >= S.deadline ? -4 : 0;
    return r ? r : status == HAL_I2C_OK ? 0 : -2;
}
#include "vendor/dvp_camera/session.inc"
static int ink_cam_run(uint8_t *buf, uint32_t timeout_ms) {
    S.deadline = osal_now_us() + (uint64_t)timeout_ms * 1000;
    S.cancelled = ink_cam_cancelled;
    __atomic_store_n(&ink_cam_active, 1, __ATOMIC_RELEASE);
    int r = -4;
    if (ink_s31_display_acquire(timeout_ms)) {
        r = S.controller ? ink_cam_check() : ink_cam_prepare();
        if (!r && buf) r = ink_cam_grab();
        if (!r && buf) memcpy(buf, S.frame, INK_CAM_BYTES);
        if (r || !buf || !ink_cam_preview) ink_cam_cleanup();
        ink_s31_display_release();
    }
    __atomic_store_n(&ink_cam_active, S.controller != NULL, __ATOMIC_RELEASE);
    return r;
}
/* MEDIA owns cleanup; the service's atomic flag also cancels an in-flight grab. */
void hal_camera_session(const int *active) {
    if (!ink_cam_enter()) return;
    if (__atomic_load_n(active, __ATOMIC_ACQUIRE)) {
        ink_cam_preview = active;
        ink_cam_checked = false;
    } else {
        if (S.controller && ink_s31_display_acquire(UINT32_MAX)) {
            ink_cam_cleanup();
            ink_s31_display_release();
        }
        ink_cam_preview = NULL;
        __atomic_store_n(&ink_cam_active, S.controller != NULL, __ATOMIC_RELEASE);
    }
    xSemaphoreGive(ink_cam_lock);
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
