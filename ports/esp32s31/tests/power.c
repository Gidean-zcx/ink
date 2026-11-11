/* Run the real power HAL against a board/IDF recorder; no device or simulator involved. */
#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include "test_idf.h"
#include "ink/hal_device.h"
#include "ink/hal_display.h"
#include "ink/osal.h"
#include "metalio_plus/metalio_board.h"
#include "metalio_plus/metalio_epd_power.h"
#include "s31_board.h"
#include "s31_io.h"

static char trace[256];
static unsigned trace_n;
static uint64_t now, wake_pins, armed_low, armed_high, timer_us, saved_low, saved_high, saved_timer;
static uint32_t sources, resumed_cause;
/* P16 at rest and with the clock chip's interrupt pending, whichever polarity the board has (metalio_board.h). */
#define RTC_IDLE_P1 ((uint8_t)(MB_RTC_INT_ACTIVE_HIGH ? (uint8_t)~MB_P1_RTC_INT : 0xFF))
#define RTC_PENDING_P1 ((uint8_t)(MB_RTC_INT_ACTIVE_HIGH ? 0xFF : (uint8_t)~MB_P1_RTC_INT))
static uint8_t pre_p1, wake_p1, port0;
static bool paused, suspended, slept, motion, haptic, low, watch, keep, cancel_sleep, late_busy, resumed_keys, cell_busy;
static bool low_after_arm, low_after_cut, wake_armed, rails_cut, usb_connected;
static int mask_fail, cut_fail, keys_fail, rails_fail, sleep_rc, restarts, offs, mhz;
static uint64_t usb_off_at;
static int pending, resume_calls;
static int stopped;
void ink_s31_display_shutdown(void) { stopped++; }
#define ALLOC_CAP 8
static void *allocs[ALLOC_CAP];
static unsigned alloc_n;

static void mark(char c) { assert(trace_n + 1 < sizeof trace); trace[trace_n++] = c; trace[trace_n] = 0; }
static unsigned pos(char c) { const char *p = strchr(trace, c); assert(p); return (unsigned)(p - trace); }
static void reset(void) {
    trace_n = 0;
    trace[0] = 0;
    now = wake_pins = armed_low = armed_high = timer_us = saved_low = saved_high = saved_timer = 0;
    sources = resumed_cause = 0;
    pre_p1 = wake_p1 = RTC_IDLE_P1;
    port0 = 0;
    paused = suspended = slept = motion = haptic = low = watch = keep = cancel_sleep = late_busy = resumed_keys = false;
    cell_busy = false;
    mask_fail = cut_fail = keys_fail = rails_fail = sleep_rc = pending = resume_calls = 0;
    low_after_arm = low_after_cut = wake_armed = rails_cut = false;
    usb_connected = true;
}

struct osal_mutex { bool held; };
static struct osal_mutex cpu_lock;
osal_mutex_t *osal_mutex_create(void) { return &cpu_lock; }
osal_err_t osal_mutex_lock(osal_mutex_t *m, int64_t us) {
    assert(m == &cpu_lock && !m->held && us == OSAL_WAIT_FOREVER);
    m->held = true;
    return OSAL_OK;
}
void osal_mutex_unlock(osal_mutex_t *m) { assert(m->held); m->held = false; }
void *osal_calloc(size_t n, size_t size, uint32_t caps) {
    (void)caps;
    assert(alloc_n < ALLOC_CAP);
    void *p = calloc(n, size);
    assert(p);
    allocs[alloc_n++] = p;
    return p;
}
void osal_free(void *p) { free(p); }
void osal_panic(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    abort();
}
uint64_t osal_now_us(void) { return now; }
void osal_task_delay_us(uint64_t us) { assert(!suspended); now += us; }
void hal_log_write(const char *s) { (void)s; assert(!suspended); }
int hal_display_pending(void) { return pending; }
bool s31_haptic_busy(void) { return haptic; }
void s31_input_pause(void) { assert(!paused); paused = true; mark('p'); }
void s31_input_resume(uint32_t cause, uint64_t t, bool back) {
    assert(paused && !suspended);
    resumed_keys = back;
    assert(!slept || t == 1000000);
    resumed_cause = cause;
    paused = false;
    resume_calls++;
    mark('i');
}
bool s31_accel_sleep(bool w) { assert(paused); watch = w; mark('a'); return true; }
void s31_accel_resume(void) { mark('e'); }
bool s31_accel_take_motion(void) { assert(paused); mark('v'); return motion; }
bool metalio_board_ready(void) { return true; }
void metalio_board_lock(void) { }
void metalio_board_unlock(void) { }
tca9555_t *metalio_ioexp(void) { return NULL; }
void epd_power_sleep(epd_power_t *p) { (void)p; assert(!paused); mark('d'); }
hal_i2c_status_t metalio_board_read(int port, uint8_t *levels) {
    *levels = port ? (slept ? wake_p1 : pre_p1) : port0;
    if (port && slept) {
        assert(paused);
        mark('c');
    }
    return HAL_I2C_OK;
}
hal_i2c_status_t metalio_board_mask_for_sleep(bool rtc) {
    (void)rtc;
    assert(paused);
    mark('m');
    return mask_fail ? HAL_I2C_ERR : HAL_I2C_OK;
}
hal_i2c_status_t metalio_board_arm_wake(bool power, bool rtc) {
    (void)power; (void)rtc;
    wake_armed = true;
    mark('w');
    return HAL_I2C_OK;
}
hal_i2c_status_t metalio_board_cut_rails(bool k) {
    keep = k;
    rails_cut = true;
    mark('x');
    if (late_busy) pending = 1;
    return cut_fail ? HAL_I2C_ERR : HAL_I2C_OK;
}
hal_i2c_status_t metalio_board_restore_keys(uint8_t *p0, uint8_t *p1) {
    (void)p0; (void)p1;
    mark('k');
    return keys_fail-- > 0 ? HAL_I2C_ERR : HAL_I2C_OK;
}
hal_i2c_status_t metalio_board_restore_rails(void) { mark('r'); return rails_fail-- > 0 ? HAL_I2C_ERR : HAL_I2C_OK; }
void metalio_power_off(void) { offs++; }
void hal_light_set(int cold, int warm) { assert(cold == 0 && warm == 0); mark('f'); }
esp_err_t esp_pm_configure(const void *arg) {
    const esp_pm_config_t *cfg = arg;
    assert(cfg->max_freq_mhz == cfg->min_freq_mhz && !cfg->light_sleep_enable);
    assert(cfg->max_freq_mhz == 160 || cfg->max_freq_mhz == 320);
    mhz = cfg->max_freq_mhz;
    return ESP_OK;
}
esp_err_t esp_pm_lock_create(int type, int arg, const char *name, esp_pm_lock_handle_t *out) {
    (void)type; (void)arg; (void)name; (void)out;
    assert(!"dynamic frequency locks must not be used");
    return ESP_OK;
}
esp_err_t esp_pm_lock_acquire(esp_pm_lock_handle_t lock) { (void)lock; assert(false); return ESP_FAIL; }
esp_err_t esp_pm_lock_release(esp_pm_lock_handle_t lock) { (void)lock; assert(false); return ESP_FAIL; }
esp_err_t esp_sleep_disable_wakeup_source(int source) { assert(source == ESP_SLEEP_WAKEUP_ALL); timer_us = 0; return ESP_OK; }
esp_err_t esp_sleep_disable_ext1_wakeup_io(uint64_t pins) { assert(!pins); armed_low = armed_high = 0; return ESP_OK; }
esp_err_t esp_sleep_enable_timer_wakeup(uint64_t us) { timer_us = us; return ESP_OK; }
esp_err_t esp_sleep_enable_ext1_wakeup_io(uint64_t pins, int level) {
    if (level == ESP_EXT1_WAKEUP_ANY_LOW) armed_low |= pins;
    else armed_high |= pins;
    return ESP_OK;
}
esp_err_t esp_sleep_enable_gpio_wakeup(void) { assert(!"GPIO61 must not wake sleep"); return ESP_FAIL; }
int ink_s31_sleep_init(void) { return ESP_OK; }
int ink_s31_light_sleep(void) { mark('h'); return esp_light_sleep_start(); }
esp_err_t esp_light_sleep_start(void) {
    assert(suspended && paused);
    saved_low = armed_low;
    saved_high = armed_high;
    saved_timer = timer_us;
    mark('s');
    if (sleep_rc == ESP_OK) {
        slept = true;
        now += 1000000;
    }
    return sleep_rc;
}
uint32_t esp_sleep_get_wakeup_causes(void) { return sources; }
uint64_t esp_sleep_get_ext1_wakeup_status(void) { return wake_pins; }
esp_err_t gpio_wakeup_enable(int pin, int level) { (void)pin; (void)level; assert(!"GPIO61 must not wake sleep"); return ESP_FAIL; }
esp_err_t gpio_wakeup_disable(int pin) { (void)pin; assert(false); return ESP_FAIL; }
esp_err_t rtc_gpio_deinit(int pin) { assert(pin == S31_GPIO_EXP_INT || pin == S31_GPIO_MODEM_SRDY); return ESP_OK; }
esp_err_t gpio_set_direction(int pin, int mode) { assert(pin == S31_GPIO_EXP_INT && mode == GPIO_MODE_INPUT); mark('g'); return ESP_OK; }
esp_err_t gpio_pullup_en(int pin) { assert(pin == S31_GPIO_EXP_INT); mark('U'); return ESP_OK; }
esp_err_t gpio_pulldown_dis(int pin) { assert(pin == S31_GPIO_EXP_INT); mark('N'); return ESP_OK; }
int gpio_get_level(int pin) {
    assert(pin == S31_GPIO_EXP_INT);
    return !(low || (wake_armed && low_after_arm) || (rails_cut && low_after_cut));
}
void usb_serial_jtag_ll_enable_bus_clock(bool on) { assert(on && !suspended); mark('b'); }
void usb_serial_jtag_ll_phy_enable_pad(bool on) {
    assert(!suspended);
    if (!on) usb_off_at = now;
    else assert(now - usb_off_at == 150000);
    mark(on ? '+' : '-');
}
void usb_serial_jtag_ll_phy_set_defaults(void) { assert(now - usb_off_at == 150000); mark('D'); }
bool usb_serial_jtag_is_connected(void) { return usb_connected; }
void esp_restart(void) { restarts++; }
void vTaskSuspendAll(void) { assert(!suspended); suspended = true; }
int xTaskResumeAll(void) { assert(suspended); suspended = false; return 0; }
static bool cancel(void *ctx) { assert(ctx == &cancel_sleep && suspended); return cancel_sleep; }

int main(void) {
    s31_power_init();
    assert(mhz == 320);
    hal_power_set_state(HAL_PWR_STATE_IDLE);
    hal_power_set_state(HAL_PWR_STATE_LOCKED);
    assert(mhz == 160);
    hal_cpu_boost_t *boost = hal_cpu_boost_create("test");
    hal_cpu_boost_t *boost2 = hal_cpu_boost_create("other");
    hal_cpu_boost_acquire(boost);
    hal_cpu_boost_acquire(boost);
    assert(mhz == 320);
    hal_cpu_boost_acquire(boost2);
    hal_power_set_state(HAL_PWR_STATE_IDLE);
    hal_cpu_boost_release(boost);
    hal_cpu_boost_release(boost);
    assert(mhz == 320);
    hal_cpu_boost_release(boost2);
    assert(mhz == 160);
    hal_cpu_boost_acquire(boost);
    hal_power_set_state(HAL_PWR_STATE_ACTIVE);
    hal_cpu_boost_release(boost);
    assert(mhz == 320);

    hal_sleep_cfg_t cfg = {.max_us = 60000000, .wake_mask = HAL_WAKE_POWER_KEY | HAL_WAKE_RTC | HAL_WAKE_MOTION};
    if (!INK_S31_LIGHT_SLEEP) {
        hal_power_set_state(HAL_PWR_STATE_LOCKED);
        reset();
        assert(hal_power_sleep(&cfg) == 0 && !slept && !paused && !suspended && trace_n == 0);
        assert(mhz == 160 && usb_connected && resume_calls == 0);
        assert(hal_power_sleep(NULL) == 0);
    } else {
    reset();
    sources = 1u << ESP_SLEEP_WAKEUP_EXT1;
    wake_pins = 1ull << S31_GPIO_EXP_INT;
    wake_p1 = (uint8_t)(RTC_PENDING_P1 & ~MB_P1_POWER_KEY);
    motion = true;
    assert(hal_power_sleep(&cfg) == cfg.wake_mask && resumed_cause == cfg.wake_mask && watch && !keep);
    assert(saved_low == (1ull << S31_GPIO_EXP_INT) && saved_timer == cfg.max_us);
    assert(pos('d') < pos('p') && pos('p') < pos('a') && pos('a') < pos('m') && pos('m') < pos('w'));
    assert(pos('m') < pos('g') && pos('g') < pos('U') && pos('U') < pos('N') && pos('N') < pos('w'));
    assert(pos('w') < pos('x') && pos('x') < pos('h') && pos('h') < pos('s'));
    assert(pos('C') < pos('x') && pos('r') < pos('R') && pos('R') < pos('i'));
    assert(pos('c') < pos('k') && pos('v') < pos('k') && pos('k') < pos('r') && pos('r') < pos('i'));
    assert(!paused && !suspended && resume_calls == 1 && !armed_low && !armed_high && !timer_us);
    assert(pos('r') < pos('b') && pos('b') < pos('-') && pos('-') < pos('D') && pos('D') < pos('+') && pos('+') < pos('i'));

    reset();
    sources = 1u << ESP_SLEEP_WAKEUP_TIMER;
    assert(hal_power_sleep(&cfg) == HAL_WAKE_TIMER && !strchr(trace, 'b'));
    for (int i = 0; i < 3; i++) {
        reset();
        sources = 1u << ESP_SLEEP_WAKEUP_TIMER;
        assert(hal_power_sleep(&cfg) == HAL_WAKE_TIMER && saved_timer == 60000000);
        assert(pos('k') < pos('r') && pos('r') < pos('i') && !timer_us && !paused);
    }
    reset();
    cfg.max_us = 1500000; /* an earlier alarm shortens this slice */
    sources = 1u << ESP_SLEEP_WAKEUP_TIMER;
    assert(hal_power_sleep(&cfg) == HAL_WAKE_TIMER && saved_timer == cfg.max_us);
    cfg.max_us = 60000000;
    reset();
    usb_connected = false;
    wake_pins = 1ull << S31_GPIO_EXP_INT;
    assert(hal_power_sleep(&cfg) == HAL_WAKE_POWER_KEY && now == 1970000);

    reset();
    sources = 1u << ESP_SLEEP_WAKEUP_EXT1;
    wake_pins = 1ull << S31_GPIO_EXP_INT;
    assert(hal_power_sleep(&cfg) == HAL_WAKE_POWER_KEY); /* released wake press is still delivered */
    reset();
    pre_p1 = RTC_PENDING_P1;
    sources = 1u << ESP_SLEEP_WAKEUP_TIMER;
    assert(hal_power_sleep(&cfg) == 0 && !slept && trace_n == 0);

    reset();
    cancel_sleep = true;
    wake_pins = 1ull << S31_GPIO_EXP_INT; /* stale status from the previous sleep */
    cfg.cancel = cancel;
    cfg.cancel_ctx = &cancel_sleep;
    assert(hal_power_sleep(&cfg) == 0 && !slept && !strchr(trace, 'c') && pos('k') < pos('r'));
    cfg.cancel = NULL;

    reset();
    late_busy = true;
    assert(hal_power_sleep(&cfg) == 0 && !slept && pos('k') < pos('r'));

    reset();
    pending = 1;
    assert(hal_power_sleep(&cfg) == 0 && trace_n == 0);
    reset();
    hal_power_request(HAL_PWR_AUDIO_IN, true);
    assert(hal_power_sleep(&cfg) == 0 && trace_n == 0);
    hal_power_request(HAL_PWR_AUDIO_IN, false);
    reset();
    haptic = true;
    assert(hal_power_sleep(&cfg) == 0 && trace_n == 0);
    reset();
    port0 = MB_P0_AMP;
    assert(hal_power_sleep(&cfg) == 0 && trace_n == 0);

    reset();
    low = true;
    assert(hal_power_sleep(&cfg) == 0 && now == 620000 && !strchr(trace, 's') && strchr(trace, 'k'));
    reset();
    low_after_arm = true;
    assert(hal_power_sleep(&cfg) == 0 && !slept && strchr(trace, 'k') && !strchr(trace, 'x'));
    reset();
    low_after_cut = true;
    assert(hal_power_sleep(&cfg) == 0 && !slept && pos('k') < pos('r'));
    reset();
    mask_fail = 1;
    assert(hal_power_sleep(&cfg) == 0 && strchr(trace, 'k') && !strchr(trace, 'x'));
    reset();
    cut_fail = 1;
    assert(hal_power_sleep(&cfg) == 0 && pos('k') < pos('r') && !strchr(trace, 's'));
    reset();
    sleep_rc = ESP_FAIL;
    assert(hal_power_sleep(&cfg) == 0 && !slept && pos('k') < pos('r') && !strchr(trace, 'b'));

    reset();
    keys_fail = 3;
    sources = 1u << ESP_SLEEP_WAKEUP_TIMER;
    assert(hal_power_sleep(&cfg) == HAL_WAKE_TIMER && !resumed_keys && resume_calls == 1);

    reset();
    cell_busy = true;
    assert(hal_power_sleep(&cfg) == 0 && !slept && !strchr(trace, 'x') && !strchr(trace, 'R'));
    reset();
    cfg.keep_modem = true;
    cfg.wake_mask = HAL_WAKE_AI_KEY | HAL_WAKE_MODEM;
    sources = 1u << ESP_SLEEP_WAKEUP_GPIO;
    wake_pins = 1ull << S31_GPIO_MODEM_SRDY;
    assert(hal_power_sleep(&cfg) == HAL_WAKE_MODEM && keep);
    assert(!strchr(trace, 'C') && !strchr(trace, 'R'));
    assert(saved_high == (1ull << S31_GPIO_MODEM_SRDY) && !saved_low);
    reset();
    cfg.wake_mask = HAL_WAKE_AI_KEY;
    cfg.max_us = 0;
    assert(hal_power_sleep(&cfg) == 0 && trace_n == 0);
    }
    reset();
    hal_power_shutdown();
    hal_power_reboot();
    assert(offs == 1 && restarts == 1 && stopped == 1);
    for (unsigned i = 0; i < alloc_n; i++) free(allocs[i]);
    puts("s31 power: fixed clocks, vendor sleep, USB revive, wake causes and unwind passed");
    return 0;
}

bool ink_s31_cell_suspend(void) { mark('C'); return !cell_busy; }
void ink_s31_cell_resume(void) { mark('R'); }
