/* CloudZao Metalio-E-INK4-Plus @874538e. MIT: LICENSE. Adaptations: SOURCE.md. */
#include "esp_log.h"
#include "esp_timer.h"
#define TAG "MetalioVibe"
#define VIBE_PWM_PERIOD_US 20000ULL
static const uint32_t k_duty_on_us[4] = {0, 14000, 18000, 20000};
static esp_timer_handle_t pulse_timer_ = NULL;
static esp_timer_handle_t pwm_timer_ = NULL;
static uint8_t intensity_ = 1;
static bool continuous_ = false;
static bool pulse_active_ = false;
static bool pwm_phase_on_ = false;
static bool initialized_ = false;
static bool ink_vendor_vibe_Init(void);
static void ink_vendor_vibe_SetIntensity(uint8_t level);
static void ink_vendor_vibe_Pulse(int duration_ms);
static void ink_vendor_vibe_Set(bool on);
static void ink_vendor_vibe_StopOutput(void);
static void ink_vendor_vibe_ApplyActiveDrive(void);
static void ink_vendor_vibe_OnPulseEnd(void);
static void ink_vendor_vibe_OnSoftPwm(void);
static void ink_vibe_end_cb(void *arg) { (void)arg; ink_vendor_vibe_OnPulseEnd(); }
static void ink_vibe_pwm_cb(void *arg) { (void)arg; ink_vendor_vibe_OnSoftPwm(); }
static void ink_vendor_vibe_create(void) {
    const esp_timer_create_args_t pulse_args = {
        .callback = ink_vibe_end_cb, .arg = NULL, .dispatch_method = ESP_TIMER_TASK,
        .name = "vibe_pulse", .skip_unhandled_events = true,
    };
    ESP_ERROR_CHECK(esp_timer_create(&pulse_args, &pulse_timer_));
    const esp_timer_create_args_t pwm_args = {
        .callback = ink_vibe_pwm_cb, .arg = NULL, .dispatch_method = ESP_TIMER_TASK,
        .name = "vibe_pwm", .skip_unhandled_events = true,
    };
    ESP_ERROR_CHECK(esp_timer_create(&pwm_args, &pwm_timer_));
}
static bool ink_vendor_vibe_Init(void) {
    if (initialized_) {
        return true;
    }
    if (!epd_board_metalio_eink4_plus_ioexp_ok()) {
        ESP_LOGE(TAG, "init fail: no TCA");
        return false;
    }
    intensity_ = 1;
    continuous_ = false;
    pulse_active_ = false;
    epd_board_metalio_eink4_plus_motor_set(false);
    initialized_ = true;
    ESP_LOGI(TAG, "init ok TCA P%d pulse=%dms intensity=1", EPD_IO_MOTOR,
             VIBRATION_MOTOR_PULSE_MS);
    return true;
}

static void ink_vendor_vibe_SetIntensity(uint8_t level) {
    if (level > 3) {
        level = 3;
    }
    intensity_ = level;
    if (!initialized_) {
        return;
    }
    if (continuous_ || pulse_active_) {
        ink_vendor_vibe_ApplyActiveDrive();
    } else if (level == 0) {
        ink_vendor_vibe_StopOutput();
    }
    ESP_LOGI(TAG, "intensity=%u", (unsigned)intensity_);
}

static void ink_vendor_vibe_Pulse(int duration_ms) {
    if (duration_ms <= 0) return;
    if (!initialized_ || continuous_ || intensity_ == 0 || pulse_timer_ == NULL) {
        return;
    }
    pulse_active_ = true;
    ink_vendor_vibe_ApplyActiveDrive();
    esp_timer_stop(pulse_timer_);
    esp_err_t err = esp_timer_start_once(
        pulse_timer_, (uint64_t)(duration_ms) * 1000ULL);
    if (err != ESP_OK) {
        pulse_active_ = false;
        ink_vendor_vibe_StopOutput();
        ESP_LOGW(TAG, "pulse timer: %s", esp_err_to_name(err));
    }
}

static void ink_vendor_vibe_Set(bool on) {
    if (!initialized_) {
        return;
    }
    if (pulse_timer_ != NULL) {
        esp_timer_stop(pulse_timer_);
    }
    pulse_active_ = false;
    continuous_ = on && intensity_ != 0;
    if (continuous_) {
        ink_vendor_vibe_ApplyActiveDrive();
    } else {
        continuous_ = false;
        ink_vendor_vibe_StopOutput();
    }
    ESP_LOGI(TAG, "continuous=%d intensity=%u", continuous_ ? 1 : 0, (unsigned)intensity_);
}

static void ink_vendor_vibe_StopOutput() {
    if (pwm_timer_ != NULL) {
        esp_timer_stop(pwm_timer_);
    }
    pwm_phase_on_ = false;
    if (epd_board_metalio_eink4_plus_ioexp_ok()) {
        epd_board_metalio_eink4_plus_motor_set(false);
    }
}

static void ink_vendor_vibe_ApplyActiveDrive() {
    if (intensity_ == 0) {
        ink_vendor_vibe_StopOutput();
        return;
    }
    if (intensity_ == 3) {
        if (pwm_timer_ != NULL) {
            esp_timer_stop(pwm_timer_);
        }
        pwm_phase_on_ = false;
        epd_board_metalio_eink4_plus_motor_set(true);
        return;
    }
    pwm_phase_on_ = true;
    epd_board_metalio_eink4_plus_motor_set(true);
    if (pwm_timer_ != NULL) {
        esp_timer_stop(pwm_timer_);
        esp_timer_start_once(pwm_timer_, k_duty_on_us[intensity_]);
    }
}

static void ink_vendor_vibe_OnPulseEnd() {
    pulse_active_ = false;
    if (!continuous_) {
        ink_vendor_vibe_StopOutput();
    }
}

static void ink_vendor_vibe_OnSoftPwm() {
    if (!initialized_ || intensity_ == 0 || intensity_ == 3) {
        return;
    }
    if (!continuous_ && !pulse_active_) {
        ink_vendor_vibe_StopOutput();
        return;
    }
    pwm_phase_on_ = !pwm_phase_on_;
    epd_board_metalio_eink4_plus_motor_set(pwm_phase_on_);
    const uint32_t on_us = k_duty_on_us[intensity_];
    const uint32_t off_us = (uint32_t)(VIBE_PWM_PERIOD_US) - on_us;
    const uint64_t next_us = pwm_phase_on_ ? on_us : off_us;
    if (next_us == 0 || pwm_timer_ == NULL) {
        return;
    }
    esp_timer_start_once(pwm_timer_, next_us);
}

#undef TAG
