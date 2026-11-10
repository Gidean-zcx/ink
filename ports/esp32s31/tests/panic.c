/* Decode the actual panic GPIO traffic; no scheduler or I2C driver is provided. */
#include <stdio.h>
#include <string.h>
#include "panic_idf.h"
#include "s31_board.h"
#include "s31_safety.h"
gpio_dev_t GPIO;
enum { TX_LIMIT = 2, BYTE_LIMIT = 3, XOE = 18, MODE = 19 };
static unsigned levels[64], now, bits, byte, tx_count, byte_count, real_calls, mappings;
static unsigned start_times[TX_LIMIT], lengths[TX_LIMIT];
static uint8_t bytes[TX_LIMIT][BYTE_LIMIT];
static bool active, nack;
void gpio_ll_set_level(gpio_dev_t *hw, unsigned pin, unsigned level) {
    assert(hw == &GPIO && pin < 64);
    unsigned old = levels[pin];
    levels[pin] = level;
    if (pin == S31_I2C_SDA && levels[S31_I2C_SCL] && old != level) {
        if (!level) {
            assert(!levels[XOE] && !levels[MODE] && now >= 12 && tx_count < TX_LIMIT);
            active = true;
            bits = byte = byte_count = 0;
            start_times[tx_count] = now;
        } else if (active) {
            if (byte_count) lengths[tx_count++] = byte_count;
            active = false;
        }
    }
    if (pin == S31_I2C_SCL && !old && level && active) {
        if (bits < 8) { byte = (byte << 1) | levels[S31_I2C_SDA]; bits++; }
        else {
            assert(byte_count < BYTE_LIMIT && tx_count < TX_LIMIT);
            bytes[tx_count][byte_count++] = (uint8_t)byte;
            bits = byte = 0;
        }
    }
}
unsigned gpio_ll_get_level(gpio_dev_t *hw, unsigned pin) {
    assert(hw == &GPIO && pin == S31_I2C_SDA);
    return nack;
}
void gpio_ll_od_enable(gpio_dev_t *hw, unsigned pin) { assert(hw == &GPIO && pin <= S31_I2C_SCL); }
void gpio_ll_input_enable(gpio_dev_t *hw, unsigned pin) { assert(hw == &GPIO && pin <= S31_I2C_SCL); }
void gpio_ll_output_enable(gpio_dev_t *hw, unsigned pin) { assert(hw == &GPIO && pin <= S31_I2C_SCL); }
void esp_rom_gpio_pad_select_gpio(unsigned pin) { assert(pin <= S31_I2C_SCL); }
void esp_rom_gpio_connect_out_signal(unsigned pin, unsigned signal, bool invert, bool invert_enable) {
    assert(pin <= S31_I2C_SCL && signal == SIG_GPIO_OUT_IDX && !invert && !invert_enable);
    mappings++;
}
void esp_rom_delay_us(unsigned us) { now += us; }
void ink_s31_display_panic(void) { ink_s31_panel_panic(XOE, MODE, 0x3f); }
void __real_esp_panic_handler(panic_info_t *info) {
    assert(info && tx_count == TX_LIMIT && !active && !levels[XOE] && !levels[MODE]);
    real_calls++;
}
void __wrap_esp_panic_handler(panic_info_t *info);
static void reset(void) {
    memset(levels, 0, sizeof levels);
    levels[XOE] = levels[MODE] = levels[S31_I2C_SDA] = levels[S31_I2C_SCL] = 1;
    now = bits = byte = tx_count = byte_count = mappings = 0;
}
int main(void) {
    reset();
    panic_info_t info = {0};
    __wrap_esp_panic_handler(&info);
    assert(real_calls == 1 && mappings == 2 && tx_count == 2);
    assert(lengths[0] == 3 && lengths[1] == 3);
    assert(bytes[0][0] == 0x40 && bytes[0][1] == 2 && bytes[0][2] == 0x3c);
    assert(bytes[1][0] == 0x40 && bytes[1][1] == 2 && bytes[1][2] == 0x38);
    assert(start_times[1] - start_times[0] >= 10000 && now < 20000);
    reset();
    nack = true;
    ink_s31_display_panic();
    assert(tx_count == 2 && lengths[0] == 1 && lengths[1] == 1 && !active && now < 20000);
    puts("s31 panic: XOE low, PMIC pins in order, bounded NACK and original panic handler passed");
    return 0;
}
