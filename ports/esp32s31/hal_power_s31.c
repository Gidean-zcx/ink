/* Explicit vendor light sleep and fixed CPU frequency, with scoped boost holds. */
#include "driver/gpio.h"
#include "driver/rtc_io.h"
#include "driver/usb_serial_jtag.h"
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
#include "s31_cell.h"
#include "s31_input_logic.h"
#include "s31_io.h"
#include "s31_bh1098.h"
#include "s31_safety.h"
#include "hal/usb_serial_jtag_ll.h"

struct hal_cpu_boost { bool held; };
static osal_mutex_t *g_cpu_lock;
static unsigned g_cpu_holds;
static int g_cpu_requested = CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ;
static int g_refs[HAL_PWR_COUNT];

static bool ink_cpu_apply(int mhz) {
    esp_pm_config_t cfg = {.max_freq_mhz = mhz, .min_freq_mhz = mhz, .light_sleep_enable = false};
    return esp_pm_configure(&cfg) == ESP_OK;
}

hal_cpu_boost_t *hal_cpu_boost_create(const char *name) {
    (void)name;
    return osal_calloc(1, sizeof(hal_cpu_boost_t), OSAL_MEM_INTERNAL);
}
void hal_cpu_boost_acquire(hal_cpu_boost_t *b) {
    if (!b) return;
    osal_mutex_lock(g_cpu_lock, OSAL_WAIT_FOREVER);
    if (!b->held && (g_cpu_holds || ink_cpu_apply(CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ))) {
        b->held = true;
        g_cpu_holds++;
    }
    osal_mutex_unlock(g_cpu_lock);
}
void hal_cpu_boost_release(hal_cpu_boost_t *b) {
    if (!b) return;
    osal_mutex_lock(g_cpu_lock, OSAL_WAIT_FOREVER);
    if (b->held && (g_cpu_holds > 1 || ink_cpu_apply(g_cpu_requested))) {
        b->held = false;
        g_cpu_holds--;
    }
    osal_mutex_unlock(g_cpu_lock);
}
void hal_power_set_state(hal_power_state_t s) {
    osal_mutex_lock(g_cpu_lock, OSAL_WAIT_FOREVER);
    int mhz = s == HAL_PWR_STATE_ACTIVE ? CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ : S31_IDLE_MHZ;
    if (g_cpu_holds || ink_cpu_apply(mhz)) g_cpu_requested = mhz;
    else hal_log_write("power HAL: CPU clock change failed");
    osal_mutex_unlock(g_cpu_lock);
}
void s31_power_init(void) {
    g_cpu_lock = osal_mutex_create();
    if (!g_cpu_lock || !ink_cpu_apply(CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ))
        osal_panic("power HAL: CPU clock setup failed");
#if INK_S31_LIGHT_SLEEP
    if (ink_s31_sleep_init() != ESP_OK) osal_panic("power HAL: sleep setup failed");
#endif
}

void hal_power_request(hal_power_res_t res, bool on) {
    if ((unsigned)res >= HAL_PWR_COUNT) return;
    int old = __atomic_load_n(&g_refs[res], __ATOMIC_ACQUIRE), next;
    do {
        next = on ? old + 1 : old > 0 ? old - 1 : 0;
    } while (!__atomic_compare_exchange_n(&g_refs[res], &old, next, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE));
}

#if INK_S31_LIGHT_SLEEP
static bool busy(void) { /* no locks: also used immediately before sleep with scheduling stopped */
    return hal_display_pending() > 0 || s31_haptic_busy() || ink_s31_camera_busy() ||
           __atomic_load_n(&g_refs[HAL_PWR_AUDIO_IN], __ATOMIC_ACQUIRE) > 0 ||
           __atomic_load_n(&g_refs[HAL_PWR_AUDIO_OUT], __ATOMIC_ACQUIRE) > 0;
}

static void wake_disable(void) {
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
    esp_sleep_disable_ext1_wakeup_io(0);
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
    return us || (armed & ~HAL_WAKE_TIMER);
}

/* USB sequence: CloudZao/Metalio-E-INK4-Plus @690a023, power_hw.cc.
 * Copyright (c) 2025 Shenzhen Xinzhi Future Technology Co., Ltd. and Project Contributors.
 * MIT license: vendor/s31_sleep/LICENSE. */
static void ink_usb_revive(void) {
    usb_serial_jtag_ll_enable_bus_clock(true);
    usb_serial_jtag_ll_phy_enable_pad(false);
    osal_task_delay_us(150000);
    usb_serial_jtag_ll_phy_set_defaults();
    usb_serial_jtag_ll_phy_enable_pad(true);
    for (int waited = 0; waited < 800 && !usb_serial_jtag_is_connected(); waited += 20)
        osal_task_delay_us(20000);
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
    uint32_t armed = cfg->wake_mask & (HAL_WAKE_POWER_KEY | HAL_WAKE_RTC | HAL_WAKE_MODEM | HAL_WAKE_MOTION);
    if (!cfg->keep_modem) armed &= ~(uint32_t)HAL_WAKE_MODEM;
    if ((armed & HAL_WAKE_RTC) && (metalio_board_read(1, &p1) != HAL_I2C_OK || MB_RTC_PENDING(p1))) {
        hal_log_write("power HAL: RTC wake line is not idle");
        return 0;
    }
    if (!armed && !cfg->max_us) return 0;
    bool masked = false, cut = false, keys_back = true, cell_stopped = false;
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
    if (gpio_set_direction(S31_GPIO_EXP_INT, GPIO_MODE_INPUT) != ESP_OK ||
        gpio_pullup_en(S31_GPIO_EXP_INT) != ESP_OK || gpio_pulldown_dis(S31_GPIO_EXP_INT) != ESP_OK) goto out;
    for (int i = 0; !gpio_get_level(S31_GPIO_EXP_INT); i++) {
        if (i == 30) goto out;
        osal_task_delay_us(20000);
    }
    if (metalio_board_arm_wake((armed & HAL_WAKE_POWER_KEY) != 0, (armed & HAL_WAKE_RTC) != 0) != HAL_I2C_OK ||
        !gpio_get_level(S31_GPIO_EXP_INT) || !wake_enable(armed, cfg->max_us)) goto out;
    hal_light_set(0, 0);
    if (!cfg->keep_modem) {
        if (!ink_s31_cell_suspend()) goto out;
        cell_stopped = true;
        ink_s31_bh_suspend();
    }
    cut = true;
    if (metalio_board_cut_rails(cfg->keep_modem) != HAL_I2C_OK) goto out;
    esp_err_t sleep_rc = ESP_OK;
    vTaskSuspendAll();
    if (!busy() && !(cfg->cancel && cfg->cancel(cfg->cancel_ctx)) && gpio_get_level(S31_GPIO_EXP_INT)) {
        sleep_rc = ink_s31_light_sleep(); /* IDF stalls the other CPU while asleep */
        if (sleep_rc == ESP_OK) {
            uint32_t sources = esp_sleep_get_wakeup_causes();
            wake_pins = esp_sleep_get_ext1_wakeup_status();
            woke_at = osal_now_us();
            if (sources & (1u << ESP_SLEEP_WAKEUP_TIMER)) cause |= HAL_WAKE_TIMER;
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
    bool rails_back = !cut || restore(restore_rails);
    if (!rails_back) hal_log_write("power HAL: rail restore failed");
    if (cell_stopped && rails_back) {
        ink_s31_cell_resume();
        ink_s31_bh_resume();
    }
    /* No SD/CNNT mux in this port yet: place USB after rails, before input resumes. */
    if (woke_at && cause != HAL_WAKE_TIMER) ink_usb_revive();
    s31_accel_resume();
    s31_input_resume(cause, woke_at, keys_back);
    return cause;
}
#else
uint32_t hal_power_sleep(const hal_sleep_cfg_t *cfg) { (void)cfg; return 0; }
#endif

void hal_power_shutdown(void) {
    ink_s31_display_shutdown();
    hal_light_set(0, 0);
    if (metalio_board_ready()) metalio_power_off();
    hal_log_write("power HAL: power-off pulse train returned");
}
void hal_power_reboot(void) { esp_restart(); }
