/* Only boot calls this reset, before input, camera, cellular or display starts. */
#include "main/boards/metalio-e-ink4-plus/config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "s31_bh1098.h"
#define TAG "epd_eink4p"
#define CFG_PORT_CTRL 0
#define CFG_PORT_INT 1
#define CFG_PIN_PA_EN MB_P0_AMP
#define CFG_PIN_MOTOR MB_P0_MOTOR
#define CFG_PIN_BT_PA_PWR MB_P1_SHARED_PWR
#define CFG_PIN_TP_RST MB_P1_SHARED_RST
#define s_ioexp_ok metalio_board_ready()
#define output_lock metalio_board_lock
#define output_unlock metalio_board_unlock
#define output_update ink_reset_update
static bool s_bt_reset_active, s_bt_reset_motor_on;
static const uint32_t kBtResetLowMs = 1000;
static struct { uint8_t desired; } s_outputs[2];
static esp_err_t ink_reset_update(int port, uint8_t mask, uint8_t value) {
    return metalio_board_set(port, mask, value) == HAL_I2C_OK ? ESP_OK : ESP_FAIL;
}
static esp_err_t output_write_locked(int port) {
    uint8_t value = s_outputs[port].desired;
    if (s_bt_reset_active) {
        value &= (uint8_t)~(port == CFG_PORT_CTRL ? CFG_PIN_PA_EN : CFG_PIN_BT_PA_PWR);
        if (port == CFG_PORT_CTRL) {
            value &= (uint8_t)~CFG_PIN_MOTOR;
            if (s_bt_reset_motor_on) value |= CFG_PIN_MOTOR;
        }
    }
    if (port == CFG_PORT_INT && !s_bt_reset_active) {
        metalio_board_unlock();
        esp_err_t err = ink_reset_update(port, 0xff, value);
        metalio_board_lock();
        return err;
    }
    return tca9555_set(metalio_ioexp(), port, 0xff, value) == HAL_I2C_OK ? ESP_OK : ESP_FAIL;
}
static SemaphoreHandle_t shared_reset_mutex(void) {
    static StaticSemaphore_t storage;
    static SemaphoreHandle_t mu;
    if (!mu) mu = xSemaphoreCreateMutexStatic(&storage);
    return mu;
}
#include "reset.inc"
static bool ink_vendor_boot_reset(void) {
    if (!ink_s31_cell_suspend()) return false;
    ink_s31_bh_suspend();
    s_outputs[0].desired = metalio_board_output(0);
    s_outputs[1].desired = metalio_board_output(1);
    esp_err_t err = epd_board_metalio_eink4_plus_bt_power_reset();
    /* Update the existing board's epoch and settle timestamp through its owner. */
    if (err == ESP_OK) {
        osal_task_delay_us(POWER_RAIL_SETTLE_MS * 1000u);
        ink_s31_bh_resume();
        ink_s31_cell_resume();
    }
    return err == ESP_OK;
}
#undef s_ioexp_ok
#undef output_lock
#undef output_unlock
#undef output_update
#undef TAG
