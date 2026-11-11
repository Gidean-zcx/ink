/* Never format vendor parameters: identifiers and commands remain private. */
#pragma once
#include "ink/hal_device.h"
#undef ESP_LOGE
#undef ESP_LOGW
#undef ESP_LOGI
#undef ESP_LOGD
#undef ESP_LOGV
static inline void ink_s31_vendor_event(const char *tag, const char *event, ...) {
    (void)tag;
    hal_log_write(event);
}
#define ESP_LOGE(tag, event, ...) ink_s31_vendor_event(tag, event, ##__VA_ARGS__)
#define ESP_LOGW(tag, event, ...) ink_s31_vendor_event(tag, event, ##__VA_ARGS__)
#define ESP_LOGI(tag, event, ...) ink_s31_vendor_event(tag, event, ##__VA_ARGS__)
#define ESP_LOGD(tag, event, ...) ((void)(tag))
#define ESP_LOGV(tag, event, ...) ((void)(tag))
