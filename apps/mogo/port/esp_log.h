#pragma once
static inline void ink_mogo_log(const char *tag, const char *fmt, ...) { (void)tag; (void)fmt; }
#define ESP_LOGI(...) ink_mogo_log(__VA_ARGS__)
#define ESP_LOGW(...) ink_mogo_log(__VA_ARGS__)
#define ESP_LOGE(...) ink_mogo_log(__VA_ARGS__)
