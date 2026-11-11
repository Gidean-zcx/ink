/* CloudZao Metalio-E-INK4-Plus @874538e. MIT: LICENSE. Adaptations: SOURCE.md. */
#include "driver/gpio.h"
#include "esp_log.h"
#define TAG "frontlight"
#define INK_VENDOR_MIN(a,b) ((a) < (b) ? (a) : (b))
#define FL_LEDC_MODE    LEDC_LOW_SPEED_MODE
#define FL_LEDC_TIMER   LEDC_TIMER_0
#define FL_CH_COOL      LEDC_CHANNEL_0
#define FL_CH_WARM      LEDC_CHANNEL_1
#define FL_LEDC_RES     LEDC_TIMER_10_BIT
#define FL_DUTY_MAX     1023
#define FL_OUTPUT_MAX_PERCENT 10 // 界面亮度 100 对应满量程 PWM 输出的 10%。

static bool ink_vendor_light_init(void) {
    if (true) {
        const ledc_timer_config_t timer = {
            .speed_mode = FL_LEDC_MODE,
            .duty_resolution = FL_LEDC_RES,
            .timer_num = FL_LEDC_TIMER,
            .freq_hz = S31_LIGHT_HZ,
            .clk_cfg = LEDC_AUTO_CLK,
        };
        if (ledc_timer_config(&timer) != ESP_OK) {
            ESP_LOGE(TAG, "ledc_timer_config fail");
            return false;
        }

        ledc_channel_config_t ch = {
            .gpio_num = S31_LIGHT_COLD,
            .speed_mode = FL_LEDC_MODE,
            .channel = FL_CH_COOL,
            .intr_type = LEDC_INTR_DISABLE,
            .timer_sel = FL_LEDC_TIMER,
            .duty = 0,
            .hpoint = 0,
            .flags = {.output_invert = 0},
        };
        if (ledc_channel_config(&ch) != ESP_OK) {
            ESP_LOGE(TAG, "ledc cool channel fail");
            return false;
        }
        ch.channel = FL_CH_WARM;
        ch.gpio_num = S31_LIGHT_WARM;
        if (ledc_channel_config(&ch) != ESP_OK) {
            ESP_LOGE(TAG, "ledc warm channel fail");
            return false;
        }
        return true;
    }

    const gpio_config_t io = {
        .pin_bit_mask = (1ULL << S31_LIGHT_COLD) | (1ULL << S31_LIGHT_WARM),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    if (gpio_config(&io) != ESP_OK) {
        ESP_LOGE(TAG, "gpio_config fail");
        return false;
    }
    gpio_set_level(S31_LIGHT_COLD, 0);
    gpio_set_level(S31_LIGHT_WARM, 0);
    return true;
}

static void ink_vendor_light_output(uint8_t brightness, unsigned warmth_) {
    const bool cool_on = true;
    const bool warm_on = true;

    if (true) {
        const uint32_t duty = (FL_DUTY_MAX * brightness * FL_OUTPUT_MAX_PERCENT) / 10000;
        const uint32_t cool_mix = INK_VENDOR_MIN(100, 2 * (100 - warmth_));
        const uint32_t warm_mix = INK_VENDOR_MIN(100, 2 * warmth_);
        ledc_set_duty(FL_LEDC_MODE, FL_CH_COOL, cool_on ? duty * cool_mix / 100 : 0);
        ledc_update_duty(FL_LEDC_MODE, FL_CH_COOL);
        ledc_set_duty(FL_LEDC_MODE, FL_CH_WARM, warm_on ? duty * warm_mix / 100 : 0);
        ledc_update_duty(FL_LEDC_MODE, FL_CH_WARM);
        return;
    }

    const int level = brightness > 0 ? 1 : 0;
    gpio_set_level(S31_LIGHT_COLD, cool_on ? level : 0);
    gpio_set_level(S31_LIGHT_WARM, warm_on ? level : 0);
}

#undef TAG
