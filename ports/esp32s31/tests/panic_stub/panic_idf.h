#ifndef INK_PANIC_TEST_IDF_H
#define INK_PANIC_TEST_IDF_H
#include "boot_idf.h"
typedef struct { int unused; } gpio_dev_t;
typedef struct { int unused; } panic_info_t;
extern gpio_dev_t GPIO;
#define SIG_GPIO_OUT_IDX 128
void gpio_ll_set_level(gpio_dev_t *hw, unsigned pin, unsigned level);
unsigned gpio_ll_get_level(gpio_dev_t *hw, unsigned pin);
void gpio_ll_od_enable(gpio_dev_t *hw, unsigned pin);
void gpio_ll_input_enable(gpio_dev_t *hw, unsigned pin);
void gpio_ll_output_enable(gpio_dev_t *hw, unsigned pin);
void esp_rom_delay_us(unsigned us);
void esp_rom_gpio_pad_select_gpio(unsigned pin);
void esp_rom_gpio_connect_out_signal(unsigned pin, unsigned signal, bool invert, bool invert_enable);
#endif
