/* Explicit light sleep, board power-off pulses and IDF CPU frequency locks. */
#include "driver/gpio.h"
#include "driver/rtc_io.h"
#include "esp_pm.h"
#include "esp_sleep.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ink/hal_device.h"
#include "ink/hal_display.h"
#include "ink/osal.h"
#include "metalio_plus/metalio_epd_power.h"
#include "s31_board.h"
#include "s31_input_logic.h"
#include "s31_io.h"

struct hal_cpu_boost { esp_pm_lock_handle_t lock; bool held; };
static hal_cpu_boost_t *g_active;
static int g_refs[HAL_PWR_COUNT];

hal_cpu_boost_t *hal_cpu_boost_create(const char *name) {
    hal_cpu_boost_t *b = osal_calloc(1, sizeof *b, OSAL_MEM_INTERNAL);
    if (b && esp_pm_lock_create(ESP_PM_CPU_FREQ_MAX, 0, name ? name : "boost", &b->lock) != ESP_OK) {
        osal_free(b);
        b = NULL;
    }
    return b;
}

void hal_cpu_boost_acquire(hal_cpu_boost_t *b) {
    if (b && !b->held && esp_pm_lock_acquire(b->lock) == ESP_OK) b->held = true;
}
void hal_cpu_boost_release(hal_cpu_boost_t *b) {
    if (b && b->held && esp_pm_lock_release(b->lock) == ESP_OK) b->held = false;
}
void hal_power_set_state(hal_power_state_t s) {
    if (s == HAL_PWR_STATE_ACTIVE) hal_cpu_boost_acquire(g_active);
    else if (s == HAL_PWR_STATE_IDLE || s == HAL_PWR_STATE_LOCKED) hal_cpu_boost_release(g_active);
}
void s31_power_init(void) {
    esp_pm_config_t cfg = {.max_freq_mhz = CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ, .min_freq_mhz = S31_IDLE_MHZ, .light_sleep_enable = false};
    if (esp_pm_configure(&cfg) != ESP_OK || !(g_active = hal_cpu_boost_create("active")))
        osal_panic("power HAL: CPU clock setup failed");
    hal_power_set_state(HAL_PWR_STATE_ACTIVE);
}

void hal_power_request(hal_power_res_t res, bool on) {
    if ((unsigned)res >= HAL_PWR_COUNT) return;
    int old = __atomic_load_n(&g_refs[res], __ATOMIC_ACQUIRE), next;
    do {
        next = on ? old + 1 : old > 0 ? old - 1 : 0;
    } while (!__atomic_compare_exchange_n(&g_refs[res], &old, next, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE));
}

static bool busy(void) { /* no locks: also used immediately before sleep with scheduling stopped */
    return hal_display_pending() > 0 || s31_haptic_busy() ||
           __atomic_load_n(&g_refs[HAL_PWR_AUDIO_IN], __ATOMIC_ACQUIRE) > 0 ||
           __atomic_load_n(&g_refs[HAL_PWR_AUDIO_OUT], __ATOMIC_ACQUIRE) > 0;
}

static void wake_disable(void) {
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
    esp_sleep_disable_ext1_wakeup_io(0);
    gpio_wakeup_disable(S31_GPIO_AI_KEY);
    rtc_gpio_deinit(S31_GPIO_EXP_INT);
    rtc_gpio_deinit(S31_GPIO_MODEM_SRDY);
}

static bool wake_enable(uint32_t armed, uint64_t us) {
    wake_disable();
    if (us && esp_sleep_enable_timer_wakeup(us) != ESP_OK) return false;
    if ((armed & (HAL_WAKE_POWER_KEY | HAL_WAKE_RTC | HAL_WAKE_MOTION)) &&
        esp_sleep_enable_ext1_wakeup_io(1ull << S31_GPIO_EXP_INT, ESP_EXT1_WAKEUP_ANY_LOW) != ESP_OK) return false;
    if ((armed & HAL_WAKE_MODEM) &&
        esp_sleep_enable_ext1_wakeup_io(1ull << S31_GPIO_MODEM_SRDY, ESP_EXT1_WAKEUP_ANY_HIGH) != ESP_OK) return false;
    if (armed & HAL_WAKE_AI_KEY) {
        if (gpio_wakeup_enable(S31_GPIO_AI_KEY, GPIO_INTR_LOW_LEVEL) != ESP_OK || esp_sleep_enable_gpio_wakeup() != ESP_OK)
            return false;
    }
    return us || (armed & ~HAL_WAKE_TIMER);
}

static bool restore(bool (*step)(void)) {
    for (int i = 0; i < 3; i++) {
        if (i) osal_task_delay_us(10000);
        if (step()) return true;
    }
    return false;
}
static bool restore_keys(void) { return metalio_board_restore_keys(NULL, NULL) == HAL_I2C_OK; }
static bool restore_rails(void) { return metalio_board_restore_rails() == HAL_I2C_OK; }

uint32_t hal_power_sleep(const hal_sleep_cfg_t *cfg) {
    if (!cfg || !metalio_board_ready() || busy()) return 0;
    uint8_t p0, p1 = 0xFF;
    if (metalio_board_read(0, &p0) != HAL_I2C_OK || (p0 & (MB_P0_AMP | MB_P0_MOTOR))) return 0;
    uint32_t armed = cfg->wake_mask & (HAL_WAKE_POWER_KEY | HAL_WAKE_AI_KEY | HAL_WAKE_RTC | HAL_WAKE_MODEM | HAL_WAKE_MOTION);
    if (!cfg->keep_modem) armed &= ~(uint32_t)HAL_WAKE_MODEM;
    if ((armed & HAL_WAKE_RTC) && (metalio_board_read(1, &p1) != HAL_I2C_OK || !(p1 & MB_P1_RTC_INT))) {
        armed &= ~(uint32_t)HAL_WAKE_RTC;
        hal_log_write("power HAL: RTC wake line is not idle");
    }
    if (!armed && !cfg->max_us) return 0;
    bool masked = false, cut = false, keys_back = true;
    uint32_t cause = 0;
    uint64_t wake_pins = 0;
    uint64_t woke_at = 0;
    epd_power_t panel = {.ioexp = metalio_ioexp(), .io_lock = metalio_board_lock, .io_unlock = metalio_board_unlock};
    /* SDMMC and panel-bus park/restore are not connected yet. */
    epd_power_sleep(&panel);
    s31_input_pause(); /* nobody may clear GPIO2's latches after the sensor's sleep setup */
    if (!s31_accel_sleep((armed & HAL_WAKE_MOTION) != 0)) goto out;
    masked = true; /* a failed write may already have changed the directions */
    if (metalio_board_mask_for_sleep((armed & HAL_WAKE_RTC) != 0) != HAL_I2C_OK) goto out;
    for (int i = 0; !gpio_get_level(S31_GPIO_EXP_INT); i++) {
        if (i == 30) goto out;
        osal_task_delay_us(20000);
    }
    if (metalio_board_arm_wake((armed & HAL_WAKE_POWER_KEY) != 0, (armed & HAL_WAKE_RTC) != 0) != HAL_I2C_OK ||
        !wake_enable(armed, cfg->max_us)) goto out;
    hal_light_set(0, 0);
    cut = true;
    if (metalio_board_cut_rails(cfg->keep_modem) != HAL_I2C_OK) goto out;
    esp_err_t sleep_rc = ESP_OK;
    vTaskSuspendAll();
    if (!busy() && !(cfg->cancel && cfg->cancel(cfg->cancel_ctx))) {
        sleep_rc = esp_light_sleep_start(); /* IDF stalls the other CPU while asleep */
        if (sleep_rc == ESP_OK) {
            uint32_t sources = esp_sleep_get_wakeup_causes();
            wake_pins = esp_sleep_get_ext1_wakeup_status();
            woke_at = osal_now_us();
            if (sources & (1u << ESP_SLEEP_WAKEUP_TIMER)) cause |= HAL_WAKE_TIMER;
            if ((sources & (1u << ESP_SLEEP_WAKEUP_GPIO)) && (armed & HAL_WAKE_AI_KEY)) cause |= HAL_WAKE_AI_KEY;
            if ((wake_pins & (1ull << S31_GPIO_MODEM_SRDY)) && (armed & HAL_WAKE_MODEM)) cause |= HAL_WAKE_MODEM;
        }
    }
    xTaskResumeAll();
    if (sleep_rc != ESP_OK) hal_log_write("power HAL: light sleep refused");
    if (wake_pins & (1ull << S31_GPIO_EXP_INT)) {
        bool ok = metalio_board_read(1, &p1) == HAL_I2C_OK;
        cause |= s31_wake_gpio2(armed, ok, p1, s31_accel_take_motion());
    }
out:
    wake_disable();
    if (masked) {
        keys_back = restore(restore_keys);
        if (keys_back) osal_task_delay_us(S31_KEY_DEBOUNCE_US);
        else hal_log_write("power HAL: key restore failed; sampling stays off");
    }
    if (cut && !restore(restore_rails)) hal_log_write("power HAL: rail restore failed");
    s31_accel_resume();
    s31_input_resume(cause, woke_at, keys_back);
    return cause;
}

void hal_power_shutdown(void) {
    hal_light_set(0, 0);
    if (metalio_board_ready()) metalio_power_off();
    hal_log_write("power HAL: power-off pulse train returned");
}
void hal_power_reboot(void) { esp_restart(); }
