#include <stdio.h>
#include "sdkconfig.h"
#include "esp_app_desc.h"
#include "esp_heap_caps.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "ink/hal_device.h"
#include "ink/hal_ota.h"

const char *hal_device_reset_reason(void) {
    switch (esp_reset_reason()) {
    case ESP_RST_POWERON: return "poweron";
    case ESP_RST_EXT: return "external";
    case ESP_RST_SW: return "software";
    case ESP_RST_PANIC: return "panic";
    case ESP_RST_INT_WDT: return "interrupt-watchdog";
    case ESP_RST_TASK_WDT: return "task-watchdog";
    case ESP_RST_WDT: return "watchdog";
    case ESP_RST_DEEPSLEEP: return "deep-sleep";
    case ESP_RST_BROWNOUT: return "brownout";
    case ESP_RST_SDIO: return "sdio";
    case ESP_RST_USB: return "usb";
    case ESP_RST_JTAG: return "jtag";
    case ESP_RST_EFUSE: return "efuse";
    case ESP_RST_PWR_GLITCH: return "power-glitch";
    case ESP_RST_CPU_LOCKUP: return "cpu-lockup";
    default: return "unknown";
    }
}
void hal_device_get_info(hal_device_info_t *out) {
    *out = (hal_device_info_t){.profile_id = "metalio-e-ink4-plus", .display_name = "CloudZao E-Ink4 Plus (ESP32-S31)",
        .device_class = HAL_CLASS_MCU, .cpu_cores = 2, .cpu_mhz = CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ,
        .internal_heap_bytes = heap_caps_get_total_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
        .dma_heap_bytes = heap_caps_get_total_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA),
        .external_heap_bytes = heap_caps_get_total_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT),
        .has_modem = true, .has_wifi = true, .has_audio_out = true, .has_audio_in = true,
        .has_frontlight = true, .has_haptics = true, .has_rtc = true};
}
void hal_ota_info(hal_ota_info_t *out) {
    const esp_partition_t *p = esp_ota_get_running_partition();
    *out = (hal_ota_info_t){.running = p && p->subtype == ESP_PARTITION_SUBTYPE_APP_OTA_1, .slot_size = p ? p->size : 0};
    snprintf(out->version, sizeof out->version, "%.23s", esp_app_get_description()->version);
}
