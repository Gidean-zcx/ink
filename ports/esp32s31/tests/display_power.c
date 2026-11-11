/* Real copied power functions against a register-level I2C recorder; no device. */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#ifndef INK_DISPLAY_POWER_SOURCE
#define INK_DISPLAY_POWER_SOURCE "../vendor/metalio_display/power.c"
#endif
#include INK_DISPLAY_POWER_SOURCE
struct hal_i2c_dev { int address; };
static struct hal_i2c_dev tca = {0x20}, pmic = {0x68};
static uint8_t regs[8], tps[32], selected;
static bool locked, pointer_valid, fail_read;
static unsigned fail_output, stop_pairs, probes, vcom_writes;
static uint64_t now, xoe_low_at;
static int xoe, mode;
static tca9555_t io;
void metalio_board_lock(void) { assert(!locked); locked = true; }
void metalio_board_unlock(void) { assert(locked); locked = false; }
uint64_t osal_now_us(void) { return now; }
void osal_task_delay_us(uint64_t us) { assert(!locked); now += us; }
void vTaskDelay(uint32_t ticks) { assert(!locked); now += ticks * 10000ull; }
void esp_rom_delay_us(unsigned us) { now += us; }
void gpio_set_level(int pin, int level) {
    if (pin == EPD_PIN_XOE) {
        if (!level && xoe) xoe_low_at = now;
        xoe = level;
    }
    if (pin == EPD_PIN_MODE) mode = level;
}
const char *hal_i2c_status_name(hal_i2c_status_t r) { return r == HAL_I2C_OK ? "ok" : "failed"; }
hal_i2c_status_t hal_i2c_write(hal_i2c_dev_t *dev, const uint8_t *b, size_t n) {
    assert(locked);
    if (dev == &pmic) {
        if (!n) { probes++; return HAL_I2C_OK; }
        if (n == 1) { assert(!pointer_valid); selected = *b; pointer_valid = true; return HAL_I2C_OK; }
        assert(n == 2 && b[0] < sizeof tps);
        tps[b[0]] = b[1];
        if (b[0] == TPS_REG_VCOM1 || b[0] == TPS_REG_VCOM2) vcom_writes++;
        return HAL_I2C_OK;
    }
    assert(dev == &tca && n == 2 && b[0] < sizeof regs);
    if (fail_output && b[0] == 2 && !(b[1] & EPD_PWR_PWRUP)) { fail_output--; return HAL_I2C_TIMEOUT; }
    if (b[0] == 2 && (regs[2] & EPD_PWR_PWRUP) && !(b[1] & EPD_PWR_PWRUP))
        assert(!xoe && now - xoe_low_at >= 12);
    regs[b[0]] = b[1];
    return HAL_I2C_OK;
}
hal_i2c_status_t hal_i2c_write_read(hal_i2c_dev_t *dev, const uint8_t *w, size_t wn, uint8_t *r, size_t rn) {
    assert(locked && rn == 1);
    if (dev == &pmic) {
        assert(!w && !wn && pointer_valid); /* repeated START is rejected */
        pointer_valid = false;
        stop_pairs++;
        if (fail_read) return HAL_I2C_TIMEOUT;
        *r = tps[selected];
        return HAL_I2C_OK;
    }
    assert(dev == &tca && wn == 1 && w && *w < sizeof regs);
    *r = *w == 0 ? (uint8_t)(regs[2] | EPD_PWR_GOOD) : regs[*w];
    return HAL_I2C_OK;
}
int main(void) {
    regs[2] = EPD_PWR_SCREEN;
    io = (tca9555_t){.dev = &tca, .out = {EPD_PWR_SCREEN, 0}};
    tps[TPS_REG_REVID] = 0x65;
    tps[TPS_REG_PG] = TPS_PG_ALL_GOOD;
    epd_power_t p = {.ioexp = &io, .pmic = &pmic, .vcom_mv = 1200,
        .io_lock = metalio_board_lock, .io_unlock = metalio_board_unlock};
    assert(epd_power_on(&p) == EPD_POWER_OK);
    assert(probes == 1 && stop_pairs > 0 && vcom_writes == 2);
    assert(p.revid == 0x65 && p.attempts == 1 && xoe && !mode);
    assert(tps[TPS_REG_UPSEQ0] == 0xE4 && tps[TPS_REG_UPSEQ1] == 0x55);
    assert(tps[TPS_REG_ENABLE] == 0x3F && tps[TPS_REG_VCOM1] == 120 && tps[TPS_REG_VCOM2] == 0);
    assert(epd_power_kept(&p));
    uint8_t unchanged = 0xA5;
    fail_read = true;
    assert(tps65185_read(&pmic, TPS_REG_REVID, &unchanged) == HAL_I2C_TIMEOUT && unchanged == 0xA5);
    fail_read = false;
    fail_output = 1;
    epd_power_off(&p);
    assert(!ink_panel_off_confirmed(&p) && io.stale[0] && !xoe && (regs[2] & EPD_PWR_PWRUP));
    epd_power_off(&p);
    assert(ink_panel_off_confirmed(&p) && !(regs[2] & 7) && (regs[2] & EPD_PWR_SCREEN));
    uint64_t before = now;
    epd_power_sleep(&p);
    assert(now - before == 120000);
    printf("factory panel power: STOP reads, native sequence, failed-off confirmation passed\n");
    return 0;
}
