/* RTC-only startup failure guard; never writes an application partition. */
#include <stdio.h>
#include "esp_app_desc.h"
#include "esp_attr.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "ink/hal_device.h"
#include "s31_safety.h"

#define INK_BOOT_MAGIC 0x494E4B42u
static RTC_NOINIT_ATTR struct { uint32_t magic, failures, armed, check; } ink_boot;
static esp_timer_handle_t ink_boot_timer;
static void ink_boot_save(unsigned failures, bool armed) {
    ink_boot.magic = INK_BOOT_MAGIC;
    ink_boot.failures = failures;
    ink_boot.armed = armed;
    ink_boot.check = ~(ink_boot.magic ^ failures ^ (unsigned)armed);
}
static void ink_boot_healthy(void *arg) {
    (void)arg;
    ink_boot_save(0, false);
    ESP_ERROR_CHECK(esp_timer_delete(ink_boot_timer));
}
void ink_s31_boot_begin(void) {
    esp_reset_reason_t why = esp_reset_reason();
    bool fault = why == ESP_RST_PANIC || why == ESP_RST_INT_WDT || why == ESP_RST_TASK_WDT ||
                 why == ESP_RST_WDT || why == ESP_RST_CPU_LOCKUP;
    bool valid = ink_boot.magic == INK_BOOT_MAGIC && ink_boot.failures <= 3 && ink_boot.armed == 1 &&
                 ink_boot.check == ~(ink_boot.magic ^ ink_boot.failures ^ ink_boot.armed);
    unsigned failures = fault && valid ? ink_boot.failures + 1 : 0;
    ink_boot_save(failures < 3 ? failures : 3, true);
    ESP_ERROR_CHECK(esp_register_shutdown_handler(ink_s31_display_shutdown));
    char line[128];
    snprintf(line, sizeof line, "ink: E-Ink4 Plus / ESP32-S31 %s reset=%s", esp_app_get_description()->version, hal_device_reset_reason());
    hal_log_write(line);
    if (failures >= 3) {
        const esp_partition_t *running = esp_ota_get_running_partition();
        const esp_partition_t *factory = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, NULL);
        if (running && running->subtype != ESP_PARTITION_SUBTYPE_APP_OTA_0 && factory) {
            hal_log_write("boot: three early crash/watchdog resets; selecting factory ota_0");
            if (esp_ota_set_boot_partition(factory) == ESP_OK) esp_restart();
            hal_log_write("boot: factory selection failed; keeping current slot");
        } else hal_log_write("boot: factory fallback unavailable or already running ota_0");
    }
    esp_timer_create_args_t timer = {.callback = ink_boot_healthy, .name = "boot-healthy"};
    ESP_ERROR_CHECK(esp_timer_create(&timer, &ink_boot_timer));
    int64_t left = 60000000 - esp_timer_get_time();
    ESP_ERROR_CHECK(esp_timer_start_once(ink_boot_timer, left > 0 ? (uint64_t)left : 1));
}
