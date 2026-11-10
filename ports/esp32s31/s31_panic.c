/* No RTOS or I2C-driver calls after IDF has stalled the other core.
 * Recovery/power sequence adapted from CloudZao/Metalio-E-INK4-Plus @690a023 (MIT).
 * Copyright (c) 2025 Shenzhen Xinzhi Future Technology Co., Ltd.
 * Copyright (c) 2025 Project Contributors. License: vendor/s31_sleep/LICENSE. */
#include "esp_attr.h"
#include "esp_private/panic_internal.h"
#include "esp_rom_gpio.h"
#include "esp_rom_sys.h"
#include "hal/gpio_ll.h"
#include "soc/gpio_sig_map.h"
#include "s31_board.h"
#include "s31_safety.h"

static void IRAM_ATTR ink_panic_pin(int pin, int high) {
    gpio_ll_set_level(&GPIO, pin, high);
    esp_rom_delay_us(5); /* 100 kHz, as the factory bus and its recovery pulses. */
}
static bool IRAM_ATTR ink_panic_byte(uint8_t value) {
    for (int bit = 7; bit >= 0; bit--) {
        ink_panic_pin(S31_I2C_SCL, 0);
        ink_panic_pin(S31_I2C_SDA, (value >> bit) & 1);
        ink_panic_pin(S31_I2C_SCL, 1);
    }
    ink_panic_pin(S31_I2C_SCL, 0);
    ink_panic_pin(S31_I2C_SDA, 1);
    ink_panic_pin(S31_I2C_SCL, 1);
    bool ack = gpio_ll_get_level(&GPIO, S31_I2C_SDA) == 0;
    ink_panic_pin(S31_I2C_SCL, 0);
    return ack;
}
static void IRAM_ATTR ink_panic_write(uint8_t value) {
    ink_panic_pin(S31_I2C_SDA, 0);
    bool sent = ink_panic_byte(0x40) && ink_panic_byte(2) && ink_panic_byte(value);
    (void)sent; /* Best effort: a broken bus cannot guarantee that the rails went off. */
    ink_panic_pin(S31_I2C_SDA, 0);
    ink_panic_pin(S31_I2C_SCL, 1);
    ink_panic_pin(S31_I2C_SDA, 1);
}
void IRAM_ATTR ink_s31_panel_panic(int xoe, int mode, uint8_t port0) {
    gpio_ll_set_level(&GPIO, xoe, 0);
    gpio_ll_set_level(&GPIO, mode, 0);
    esp_rom_delay_us(12); /* XOE low before removing VPOS/VNEG (factory board power-off). */
    for (int pin = S31_I2C_SDA; pin <= S31_I2C_SCL; pin++) {
        esp_rom_gpio_pad_select_gpio(pin);
        esp_rom_gpio_connect_out_signal(pin, SIG_GPIO_OUT_IDX, false, false);
        gpio_ll_set_level(&GPIO, pin, 1);
        gpio_ll_od_enable(&GPIO, pin);
        gpio_ll_input_enable(&GPIO, pin);
        gpio_ll_output_enable(&GPIO, pin);
    }
    /* Factory metalio_i2c1.c:55-83 recovery; then TCA9555 output register 2. */
    for (int i = 0; i < 9; i++) {
        ink_panic_pin(S31_I2C_SCL, 0);
        ink_panic_pin(S31_I2C_SCL, 1);
    }
    ink_panic_pin(S31_I2C_SDA, 0);
    ink_panic_pin(S31_I2C_SCL, 1);
    ink_panic_pin(S31_I2C_SDA, 1);
    ink_panic_write((port0 & ~7u) | 4u); /* VCOM_CTRL/PWRUP low, WAKEUP high. */
    esp_rom_delay_us(10000);
    ink_panic_write(port0 & ~7u); /* WAKEUP low, as trip()/epd_power_off(). */
}
void __real_esp_panic_handler(panic_info_t *info);
void IRAM_ATTR __wrap_esp_panic_handler(panic_info_t *info) {
    ink_s31_display_panic();
    __real_esp_panic_handler(info);
}
