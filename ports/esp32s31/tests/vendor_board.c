/* Real board/input/chip code, with a virtual clock and register-level hardware. */
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vendor_idf.h"
#include "ink/osal.h"
#ifndef INK_BOARD_SOURCE
#define INK_BOARD_SOURCE "../s31_board.c"
#endif
#ifndef INK_INPUT_SOURCE
#define INK_INPUT_SOURCE "../hal_input_s31.c"
#endif
#include INK_BOARD_SOURCE
#include INK_INPUT_SOURCE
#include "../vendor/board_874538e/boot_binding.inc"
struct hal_i2c_dev { uint8_t addr; };
struct osal_mutex { bool held; };
struct ink_test_timer { esp_timer_create_args_t args; uint64_t due; };
gpio_dev_t GPIO;
static struct hal_i2c_dev devices[128];
static struct osal_mutex mutexes[8];
static unsigned nmutex;
static uint8_t regs[128][256];
static uint64_t clock_us;
static bool present[128], accel_fail;
static bool gauge_soc_unreadable;
static unsigned touch_fails, touch_ids, key_read_fails;
enum { TRACE_CAP = 256, TIMER_CAP = 2, EVENT_CAP = 32 };
static struct { uint8_t addr, reg, value; uint64_t at; } trace[TRACE_CAP];
static size_t ntrace;
static struct ink_test_timer timers[TIMER_CAP];
static unsigned ntimer;
static bool timer_fail;
static uint32_t duty_values[2], notification;
static void (*task_entry)(void *);
static jmp_buf task_exit;
static unsigned iterations, last_wait, suspends, resumes, bh_off, bh_on;
static int gpio_level[64];
static uint64_t held_pins, disabled_irqs, disabled_wake, cleared_irqs;
static unsigned uhci_reset[UHCI_LL_NUM];
static hal_input_event_t events[EVENT_CAP];
static unsigned nevent;
static void collect(const hal_input_event_t *e, void *arg) { (void)arg; assert(nevent < EVENT_CAP); events[nevent++] = *e; }
static void tick_input(void) {
    iterations = 0;
    if (setjmp(task_exit) == 0) task_entry(NULL);
}
static void advance(uint64_t us) {
    uint64_t target = clock_us + us;
    for (;;) {
        struct ink_test_timer *next = NULL;
        for (unsigned i = 0; i < ntimer; ++i)
            if (timers[i].due && timers[i].due <= target && (!next || timers[i].due < next->due)) next = &timers[i];
        if (!next) break;
        clock_us = next->due; next->due = 0; next->args.callback(next->args.arg);
    }
    clock_us = target;
}
uint64_t osal_now_us(void) { return clock_us; }
int64_t esp_timer_get_time(void) { return (int64_t)clock_us; }
void osal_task_delay_us(uint64_t us) { advance(us); }
void vTaskDelay(TickType_t t) { advance((uint64_t)t * 1000); }
osal_mutex_t *osal_mutex_create(void) { assert(nmutex < 8); return &mutexes[nmutex++]; }
osal_err_t osal_mutex_lock(osal_mutex_t *m, int64_t us) { (void)us; assert(m && !m->held); m->held = true; return OSAL_OK; }
void osal_mutex_unlock(osal_mutex_t *m) { assert(m->held); m->held = false; }
void osal_panic(const char *fmt, ...) { (void)fmt; abort(); }
void hal_log_write(const char *s) { assert(s); }
SemaphoreHandle_t xSemaphoreCreateMutexStatic(StaticSemaphore_t *s) { memset(s, 0, sizeof *s); return s; }
BaseType_t xSemaphoreTake(SemaphoreHandle_t s, TickType_t t) { (void)t; assert(s && !s->held); s->held = true; return pdTRUE; }
BaseType_t xSemaphoreGive(SemaphoreHandle_t s) { assert(s && s->held); s->held = false; return pdTRUE; }
BaseType_t xTaskCreatePinnedToCore(void (*fn)(void *), const char *name, unsigned stack, void *arg, int priority, TaskHandle_t *out, int core) {
    (void)arg; assert(!strcmp(name, "input-hal") && stack == 4096 && priority == OSAL_PRIO_MAX && core == 0);
    task_entry = fn; *out = (void *)1; return pdPASS;
}
BaseType_t xTaskNotify(TaskHandle_t t, uint32_t bits, int mode) { assert(t && mode == eSetBits); notification |= bits; return pdTRUE; }
BaseType_t xTaskNotifyWait(uint32_t a, uint32_t b, uint32_t *bits, TickType_t wait) {
    assert(a == 0 && b == UINT32_MAX); last_wait = wait;
    if (iterations++) longjmp(task_exit, 1);
    *bits = notification; notification = 0; return pdTRUE;
}
int s31_irq_watch(int pin, s31_irq_fn fn, void *ctx) { (void)ctx; assert(fn && (pin == 2 || pin == 61)); return 0; }
int s31_irq_watch_task(int pin, void *task, uint32_t bits) { assert(task && pin == 5 && bits); return 0; }
uint64_t s31_irq_edge_us(int pin) { (void)pin; return 0; }
esp_err_t gpio_reset_pin(int pin) { assert(pin == 61); return ESP_OK; }
esp_err_t gpio_config(const gpio_config_t *c) { assert(c->pin_bit_mask == (1ULL << 61) && c->intr_type == GPIO_INTR_ANYEDGE); return ESP_OK; }
int gpio_get_level(int pin) { return gpio_level[pin]; }
esp_err_t gpio_set_level(int pin, int level) { gpio_level[pin] = level; return ESP_OK; }
esp_err_t gpio_intr_disable(int pin) { disabled_irqs |= 1ULL << pin; GPIO.pin[pin].int_ena = 0; return ESP_OK; }
esp_err_t gpio_wakeup_disable(int pin) { disabled_wake |= 1ULL << pin; return ESP_OK; }
esp_err_t gpio_set_intr_type(int pin, int type) { assert(type == GPIO_INTR_DISABLE); (void)pin; return ESP_OK; }
esp_err_t gpio_hold_dis(int pin) { held_pins &= ~(1ULL << pin); return ESP_OK; }
void gpio_ll_clear_intr_status(gpio_dev_t *g, uint32_t mask) { assert(g == &GPIO); cleared_irqs |= mask; }
void gpio_ll_clear_intr_status_high(gpio_dev_t *g, uint32_t mask) { assert(g == &GPIO); cleared_irqs |= (uint64_t)mask << 32; }
void uhci_ll_enable_bus_clock(int i, bool on) { (void)on; assert(i < UHCI_LL_NUM); }
void uhci_ll_reset_register(int i) { uhci_reset[i]++; }
const char *esp_err_to_name(int e) { (void)e; return "fixture"; }
esp_err_t ledc_timer_config(const ledc_timer_config_t *c) { assert(c->freq_hz == 20000 && c->duty_resolution == 10); return ESP_OK; }
esp_err_t ledc_channel_config(const ledc_channel_config_t *c) { assert(c->gpio_num == 6 + c->channel && c->duty == 0); return ESP_OK; }
esp_err_t ledc_set_duty(int mode, ledc_channel_t ch, uint32_t duty) { assert(mode == 0 && ch >= 0 && ch < 2); duty_values[ch] = duty; return ESP_OK; }
esp_err_t ledc_update_duty(int mode, ledc_channel_t ch) { assert(mode == 0 && ch >= 0 && ch < 2); return ESP_OK; }
esp_err_t esp_timer_create(const esp_timer_create_args_t *a, esp_timer_handle_t *t) { assert(ntimer < TIMER_CAP); *t = &timers[ntimer++]; (*t)->args = *a; return ESP_OK; }
esp_err_t esp_timer_stop(esp_timer_handle_t t) { assert(t); t->due = 0; return ESP_OK; }
esp_err_t esp_timer_start_once(esp_timer_handle_t t, uint64_t us) { assert(t && us); if (timer_fail) return ESP_FAIL; t->due = clock_us + us; return ESP_OK; }
hal_i2c_dev_t *hal_i2c_device(int bus, uint8_t addr, const char *name) { assert(bus == 0 && name); devices[addr].addr = addr; return &devices[addr]; }
hal_i2c_status_t hal_i2c_write(hal_i2c_dev_t *d, const uint8_t *p, size_t n) {
    if (!d || !present[d->addr]) return HAL_I2C_NACK;
    if (!n) return HAL_I2C_OK;
    assert(n >= 2);
    if (d->addr == 0x19 && p[0] == 0x23 && accel_fail) { accel_fail = false; return HAL_I2C_TIMEOUT; }
    for (size_t i = 1; i < n; ++i) {
        uint8_t reg = p[0] + (uint8_t)i - 1;
        regs[d->addr][reg] = p[i]; assert(ntrace < TRACE_CAP);
        trace[ntrace++] = (__typeof__(trace[0])){d->addr, reg, p[i], clock_us};
    }
    return HAL_I2C_OK;
}
hal_i2c_status_t hal_i2c_write_read(hal_i2c_dev_t *d, const uint8_t *w, size_t wn, uint8_t *r, size_t rn) {
    if (!d || !present[d->addr]) return HAL_I2C_NACK;
    assert(wn == 1 && w && r && rn);
    if (d->addr == 0x55 && *w == 0x2c && gauge_soc_unreadable) return HAL_I2C_NACK;
    if (d->addr == 0x38 && *w == 0xa3) { touch_ids++; if (touch_fails) { touch_fails--; return HAL_I2C_TIMEOUT; } }
    uint8_t reg = *w;
    if (d->addr == 0x18 || d->addr == 0x19) reg &= 0x7f;
    if (d->addr == 0x20 && reg == 1 && key_read_fails) { key_read_fails--; return HAL_I2C_TIMEOUT; }
    if (d->addr == 0x20 && reg < 2) {
        assert(rn == 1); *r = (regs[0x20][2 + reg] & (uint8_t)~regs[0x20][6 + reg]) | (regs[0x20][reg] & regs[0x20][6 + reg]);
    } else memcpy(r, regs[d->addr] + reg, rn);
    if ((d->addr == 0x18 || d->addr == 0x19) && reg == 0x31) regs[d->addr][0x31] = 0;
    return HAL_I2C_OK;
}
hal_i2c_status_t ink_s31_i2c_read_stop(hal_i2c_dev_t *d, uint8_t reg, uint8_t *p, size_t n) { return hal_i2c_write_read(d, &reg, 1, p, n); }
void s31_power_init(void) { }
void ink_s31_audio_init(void) { }
bool ink_s31_cell_suspend(void) { suspends++; return true; }
void ink_s31_cell_resume(void) { resumes++; }
void ink_s31_bh_suspend(void) { bh_off++; }
void ink_s31_bh_resume(void) { bh_on++; }
void ink_s31_bh_poll(void) { }
bool ink_s31_bh_headset(void) { return false; }
void ink_s31_cell_init(void) { assert(suspends == 1 && resumes == 1 && bh_off == 1 && bh_on == 1); }
static void fixture(void) {
    present[0x20] = present[0x55] = present[0x6b] = present[0x51] = present[0x19] = present[0x38] = true;
    regs[0x20][0] = 0xc0; regs[0x20][1] = 0xa3;
    regs[0x19][0x0f] = regs[0x18][0x0f] = 0x11;
    regs[0x38][0xa3] = 0x64; regs[0x38][0xa6] = 3;
    for (unsigned i = 0; i < 64; ++i) gpio_level[i] = 1;
}
static void light(void) {
    const int levels[] = {0, 50, 100};
    const int warmth[] = {0, 25, 50, 75, 100};
    for (unsigned i = 0; i < sizeof levels / sizeof levels[0]; ++i)
        for (unsigned j = 0; j < sizeof warmth / sizeof warmth[0]; ++j) {
            int total = levels[i] * 10, warm = total * warmth[j] / 100;
            hal_light_set(total - warm, warm);
            unsigned d = 1023 * levels[i] * 10 / 10000;
            unsigned cool = warmth[j] <= 50 ? 100 : 2 * (100 - warmth[j]);
            unsigned w = warmth[j] >= 50 ? 100 : 2 * warmth[j];
            assert(duty_values[0] == d * cool / 100 && duty_values[1] == d * w / 100);
        }
}
static void reset_trace(void) {
    uint64_t low = 0, motor_on = 0, motor_off = 0, high = 0, touch_low = 0, touch_high = 0;
    for (size_t i = 0; i < ntrace; ++i) {
        if (trace[i].addr != 0x20) continue;
        if (trace[i].reg == 3 && !(trace[i].value & MB_P1_SHARED_PWR)) low = trace[i].at;
        if (low && trace[i].reg == 2 && (trace[i].value & MB_P0_MOTOR)) motor_on = trace[i].at;
        if (motor_on && trace[i].reg == 2 && !(trace[i].value & MB_P0_MOTOR) && !motor_off) motor_off = trace[i].at;
        if (low && trace[i].reg == 3 && (trace[i].value & MB_P1_SHARED_PWR) && !high) high = trace[i].at;
        if (trace[i].reg == 3 && !(trace[i].value & MB_P1_SHARED_RST)) touch_low = trace[i].at;
        if (touch_low && trace[i].reg == 3 && (trace[i].value & MB_P1_SHARED_RST)) touch_high = trace[i].at;
    }
    assert(low && motor_on == low && motor_off - motor_on == 500000 && high - low == 1000000);
    assert(touch_low >= high + 80000 && touch_high - touch_low == 10000 && clock_us >= touch_high + 300000);
    assert(metalio_board_shared_power_epoch() == 1 && metalio_board_shared_power_since_us() == high);
    assert(!(metalio_board_output(0) & (MB_P0_MOTOR | MB_P0_AMP)));
}
static void touch(void) {
    ft6336u_point_t p[2] = {{.id = 0, .x = 900, .y = 100}, {.id = 1, .x = 200, .y = 1000}};
    regs[0x38][2] = 2;
    for (int i = 0; i < 2; ++i) {
        uint8_t *raw = &regs[0x38][3 + 6 * i];
        raw[0] = (uint8_t)(p[i].x >> 8); raw[1] = (uint8_t)p[i].x;
        raw[2] = (uint8_t)((i << 4) | (p[i].y >> 8)); raw[3] = (uint8_t)p[i].y;
    }
    tick_input();
    assert(nevent == 2 && events[0].touch.x == 900 && events[0].touch.y == 100 && events[1].touch.id == 1 && last_wait == 10);
    regs[0x38][2] = 0; tick_input();
    assert(nevent == 4 && events[2].touch.phase == HAL_TOUCH_UP && events[3].touch.phase == HAL_TOUCH_UP);
    nevent = 0; tick_input(); assert(last_wait == 20);
}
static void keys(void) {
    tick_input(); nevent = 0;
    advance(40000); regs[0x20][1] &= (uint8_t)~MB_P1_POWER_KEY; tick_input();
    assert(nevent == 1 && events[0].type == HAL_IN_KEY && events[0].key.key == HAL_KEY_POWER && events[0].key.pressed);
    advance(10000); regs[0x20][1] |= MB_P1_POWER_KEY; tick_input(); assert(nevent == 1);
    advance(10000); tick_input(); assert(nevent == 1);
    advance(10000); tick_input(); assert(nevent == 2 && !events[1].key.pressed);
    advance(40000); regs[0x20][1] &= (uint8_t)~MB_P1_VOL_UP; tick_input(); assert(nevent == 3 && events[2].key.key == HAL_KEY_VOL_UP);
    s31_input_pause(); regs[0x20][1] |= MB_P1_VOL_UP;
    s31_input_resume(HAL_WAKE_POWER_KEY, clock_us, true); tick_input();
    assert(nevent >= 5 && !g_resuming);
    s31_input_pause(); key_read_fails = 2;
    s31_input_resume(0, clock_us, true); assert(!g_keys_back);
}
static void vibe(void) {
    assert(hal_haptic_available());
    const int ms[] = {35, 50, 70};
    const unsigned on_us[] = {14000, 18000, 20000};
    _Static_assert(sizeof ms / sizeof ms[0] == sizeof on_us / sizeof on_us[0], "haptic cases fit");
    for (unsigned i = 0; i < sizeof ms / sizeof ms[0]; ++i) {
        hal_haptic_pulse(ms[i]); tick_input();
        assert(metalio_board_output(0) & MB_P0_MOTOR);
        if (i < 2) {
            advance(on_us[i] - 1); assert(metalio_board_output(0) & MB_P0_MOTOR);
            advance(1); assert(!(metalio_board_output(0) & MB_P0_MOTOR));
        }
        advance((uint64_t)ms[i] * 1000); tick_input();
        assert(!(metalio_board_output(0) & MB_P0_MOTOR) && !s31_haptic_busy());
    }
    timer_fail = true; hal_haptic_pulse(35); tick_input();
    assert(!(metalio_board_output(0) & MB_P0_MOTOR));
}
static void boot_cleanup(void) {
    const int pins[] = {3, 2, 5, 61};
    uint64_t mask = (1ULL << 3) | (1ULL << 2) | (1ULL << 5) | (1ULL << 61);
    held_pins = UINT64_MAX;
    for (unsigned i = 0; i < sizeof pins / sizeof pins[0]; ++i) GPIO.pin[pins[i]].int_ena = 1;
    epd_board_metalio_eink4_plus_boot_release_holds();
    assert(disabled_irqs == mask && disabled_wake == mask && cleared_irqs == mask);
    for (unsigned i = 0; i < UHCI_LL_NUM; ++i) assert(uhci_reset[i] == 1);
    for (int pin = 20; pin <= 25; ++pin) assert(!(held_pins & (1ULL << pin)));
    for (size_t i = 0; i < sizeof k_epd_bus_pins / sizeof k_epd_bus_pins[0]; ++i) assert(!(held_pins & (1ULL << k_epd_bus_pins[i])));
    assert(!(held_pins & 0xc3));
    epd_board_metalio_eink4_plus_boot_release_holds();
    for (unsigned i = 0; i < UHCI_LL_NUM; ++i) assert(uhci_reset[i] == 1);
}
int main(int argc, char **argv) {
    assert(argc == 2); fixture();
    if (!strcmp(argv[1], "boot")) { boot_cleanup(); puts("boot cleanup passed"); return 0; }
    if (!strcmp(argv[1], "retry")) touch_fails = 2;
    if (!strcmp(argv[1], "accel-alt")) { present[0x19] = false; present[0x18] = true; }
    if (!strcmp(argv[1], "accel-fail")) accel_fail = true;
    ink_s31_board_init(); hal_input_set_sink(collect, NULL);
    if (!strcmp(argv[1], "light")) light();
    else if (!strcmp(argv[1], "reset")) reset_trace();
    else if (!strcmp(argv[1], "touch")) touch();
    else if (!strcmp(argv[1], "retry")) { hal_input_info_t info; hal_input_get_info(&info); assert(info.has_touch && info.touch_points == 2 && touch_ids == 3); }
    else if (!strcmp(argv[1], "keys")) keys();
    else if (!strcmp(argv[1], "vibe")) vibe();
    else if (!strcmp(argv[1], "accel-alt")) {
        assert(g_accel && g_accel->addr == 0x18); int x, y, z;
        regs[0x18][0x29] = 0x10; regs[0x18][0x2b] = 0xf0; regs[0x18][0x2d] = 0x08;
        assert(hal_accel_read(&x, &y, &z) && x == 256 && y == -256 && z == 128);
    } else if (!strcmp(argv[1], "accel-fail")) {
        assert(!g_accel && regs[0x19][0x30] == 0 && regs[0x19][0x22] == 0 && regs[0x19][0x24] == 0);
        assert(regs[0x19][0x20] == 0x10 && regs[0x19][0x25] == 0 && regs[0x19][0x31] == 0);
    } else assert(false);
    puts("vendor board case passed"); return 0;
}
