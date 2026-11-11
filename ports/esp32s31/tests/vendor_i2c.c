/* Execute the vendor bus and the HAL/TCA binding against an observed IDF bus. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "vendor_idf.h"
#ifndef INK_I2C_SOURCE
#define INK_I2C_SOURCE "../hal_i2c_s31.c"
#endif
#include INK_I2C_SOURCE
#include "tca9555/tca9555.h"
struct ink_test_i2c_bus { int unused; };
struct ink_test_i2c_dev { uint8_t addr; };
static struct ink_test_i2c_bus bus;
static struct ink_test_i2c_dev devices[DEVICE_SLOTS];
static unsigned ndev, pulses, writes, reads, combined, quiet, clears;
static uint32_t ticks, delayed_us;
static bool scl_high = true, recover = true, repeated_ok = true;
static uint8_t pointer;
static int stretch, transfer_ms;
static char trace[32];
static unsigned trace_n;
static void mark(char c) { assert(trace_n < sizeof trace); trace[trace_n++] = c; }
const char *esp_err_to_name(int e) { (void)e; return "fixture"; }
SemaphoreHandle_t xSemaphoreCreateMutexStatic(StaticSemaphore_t *s) { memset(s, 0, sizeof *s); return s; }
BaseType_t xSemaphoreTake(SemaphoreHandle_t s, TickType_t t) {
    if (s->held) { ticks += t; return pdFALSE; }
    s->held = true; s->owner = (void *)1; return pdTRUE;
}
BaseType_t xSemaphoreGive(SemaphoreHandle_t s) { assert(s->held); s->held = false; s->owner = NULL; return pdTRUE; }
TaskHandle_t xSemaphoreGetMutexHolder(SemaphoreHandle_t s) { return s->owner; }
TaskHandle_t xTaskGetCurrentTaskHandle(void) { return (void *)1; }
TickType_t xTaskGetTickCount(void) { return ticks; }
void esp_rom_delay_us(uint32_t us) { delayed_us += us; }
esp_err_t gpio_config(const gpio_config_t *c) {
    assert(c->pin_bit_mask == 3 && c->mode == GPIO_MODE_INPUT_OUTPUT_OD && c->intr_type == GPIO_INTR_DISABLE);
    return ESP_OK;
}
esp_err_t gpio_set_level(int pin, int level) {
    if (pin == 1) { if (!scl_high && level) pulses++; scl_high = level; }
    return ESP_OK;
}
int gpio_get_level(int pin) { assert(pin == 0); return !recover || pulses >= 3; }
esp_err_t i2c_new_master_bus(const i2c_master_bus_config_t *c, i2c_master_bus_handle_t *out) {
    assert(c->i2c_port == 1 && c->sda_io_num == 0 && c->scl_io_num == 1 && c->glitch_ignore_cnt == 7);
    assert(c->flags.enable_internal_pullup && c->trans_queue_depth == 0);
    *out = &bus; return ESP_OK;
}
esp_err_t i2c_master_bus_add_device(i2c_master_bus_handle_t b, const i2c_device_config_t *c, i2c_master_dev_handle_t *d) {
    assert(b == &bus && c->scl_speed_hz == 100000);
    stretch = c->scl_wait_us;
    assert(ndev < DEVICE_SLOTS);
    *d = &devices[ndev++]; (*d)->addr = c->device_address; return ESP_OK;
}
esp_err_t i2c_master_bus_rm_device(i2c_master_dev_handle_t d) { assert(d); return ESP_OK; }
esp_err_t i2c_del_master_bus(i2c_master_bus_handle_t b) { assert(b == &bus); return ESP_OK; }
esp_err_t i2c_master_get_bus_handle(int p, i2c_master_bus_handle_t *b) { assert(p == 1); *b = &bus; return ESP_OK; }
esp_err_t i2c_master_probe(i2c_master_bus_handle_t b, uint8_t a, int ms) {
    assert(b == &bus && s_mu->held && ms > 0); return a == 0x7f ? ESP_ERR_NOT_FOUND : ESP_OK;
}
esp_err_t i2c_master_transmit(i2c_master_dev_handle_t d, const uint8_t *p, size_t n, int ms) {
    assert(d && s_mu->held && p && n && ms > 0); writes++; pointer = p[0]; mark('W'); return ESP_OK;
}
esp_err_t i2c_master_receive(i2c_master_dev_handle_t d, uint8_t *p, size_t n, int ms) {
    assert(d && s_mu->held && n && ms > 0 && pointer == 0); reads++; memset(p, 0x55, n); mark('R'); return ESP_OK;
}
esp_err_t i2c_master_transmit_receive(i2c_master_dev_handle_t d, const uint8_t *w, size_t wn, uint8_t *r, size_t rn, int ms) {
    assert(d && s_mu->held && w && wn && r && rn && ms > 0); transfer_ms = ms; combined++; mark('C'); memset(r, 0x55, rn);
    return repeated_ok ? ESP_OK : ESP_ERR_TIMEOUT;
}
esp_err_t i2c_master_multi_buffer_transmit(i2c_master_dev_handle_t d, const i2c_master_transmit_multi_buffer_info_t *b, size_t n, int ms) {
    assert(d && b && n && ms > 0 && s_mu->held); return ESP_OK;
}
void i2c_ll_disable_intr_mask(int hw, uint32_t m) { assert(hw == 1 && m == I2C_LL_INTR_MASK); quiet++; }
void i2c_ll_clear_intr_mask(int hw, uint32_t m) { assert(hw == 1 && m == I2C_LL_INTR_MASK); clears++; }
int main(int argc, char **argv) {
    assert(argc == 2);
    hal_i2c_dev_t *d = hal_i2c_device(0, 0x20, "fixture");
    assert(d && !s_mu->held && stretch == 10000);
    if (!strcmp(argv[1], "recovery")) { assert(pulses == 3 && delayed_us == 65); }
    else if (!strcmp(argv[1], "stretch")) { assert(stretch == 10000); }
    else if (!strcmp(argv[1], "stop")) {
        tca9555_t t = {.dev = d}; uint8_t levels;
        assert(tca9555_read(&t, 0, &levels) == HAL_I2C_OK && levels == 0x55);
        assert(writes == 1 && reads == 1 && combined == 0 && trace_n == 2 && !memcmp(trace, "WR", 2));
    } else if (!strcmp(argv[1], "fallback")) {
        d = hal_i2c_device(0, 0x19, "fixture"); uint8_t reg = 0, value;
        repeated_ok = false;
        assert(hal_i2c_write_read(d, &reg, 1, &value, 1) == HAL_I2C_OK && value == 0x55);
        assert(trace_n == 3 && !memcmp(trace, "CWR", 3) && delayed_us == 265);
    } else if (!strcmp(argv[1], "flash")) {
        unsigned before = quiet;
        assert(metalio_i2c1_lock_for_flash(50));
        assert(s_mu->held && s_flash_active && quiet == before + 1 && quiet == clears);
        assert(!metalio_i2c1_lock_for_flash(50));
        ticks += 5; metalio_i2c1_unlock();
        assert(!s_mu->held && !s_flash_active && s_flash_ticks == 5);
        assert(hal_i2c_write(d, NULL, 0) == HAL_I2C_OK);
    } else if (!strcmp(argv[1], "budgets")) {
        const uint8_t addresses[] = {0x55, 0x6b, 0x51, 0x19, 0x18};
        const int expected_ms[] = {200, 100, 100, 100, 100};
        _Static_assert(sizeof addresses / sizeof addresses[0] == sizeof expected_ms / sizeof expected_ms[0], "timeout cases fit");
        for (unsigned i = 0; i < sizeof addresses / sizeof addresses[0]; ++i) {
            d = hal_i2c_device(0, addresses[i], "fixture"); uint8_t reg = 0, value;
            assert(hal_i2c_write_read(d, &reg, 1, &value, 1) == HAL_I2C_OK && transfer_ms == expected_ms[i]);
        }
    } else if (!strcmp(argv[1], "limits")) {
        assert(!hal_i2c_device(1, 0x20, "fixture") && !hal_i2c_device(0, 0x80, "fixture"));
        assert(hal_i2c_device(0, 0x20, "again") == d);
        d = hal_i2c_device(0, 0x7f, "absent");
        assert(hal_i2c_write(d, NULL, 0) == HAL_I2C_NACK);
    } else assert(false);
    puts("vendor I2C case passed"); return 0;
}
