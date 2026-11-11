/* The existing input task is the sole sampler; services keep key semantics. */
#include "main/boards/metalio-e-ink4-plus/config.h"
#include "esp_timer.h"
#include "esp_log.h"
#define epd_board_metalio_eink4_plus_ioexp_ok metalio_board_ready
static bool epd_board_metalio_eink4_plus_ioexp_read_port(int port, uint8_t *value) {
    return metalio_board_read(port, value) == HAL_I2C_OK;
}
static void epd_board_metalio_eink4_plus_motor_set(bool on) {
    metalio_board_set(0, MB_P0_MOTOR, on ? MB_P0_MOTOR : 0);
}
static void ink_vendor_boot_gpio(void) {
    gpio_reset_pin(BOOT_BUTTON_GPIO);
    gpio_config_t io = {.pin_bit_mask = 1ULL << BOOT_BUTTON_GPIO, .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE, .pull_down_en = GPIO_PULLDOWN_DISABLE, .intr_type = GPIO_INTR_ANYEDGE};
    gpio_config(&io);
}
#include "keys.inc"
#include "vibe.inc"
#define EPD_LOGICAL_W MT_PANEL_W
#define EPD_LOGICAL_H MT_PANEL_H
#include "touch_map.inc"
#undef epd_board_metalio_eink4_plus_ioexp_ok
