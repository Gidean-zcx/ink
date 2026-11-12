#pragma once
static inline void ink_test_font_log(const char *tag, const char *format, ...) {
    (void)tag;
    (void)format;
}
#define ESP_LOGI(...) ink_test_font_log(__VA_ARGS__)
#define ESP_LOGW(...) ink_test_font_log(__VA_ARGS__)
#define ESP_LOGE(...) ink_test_font_log(__VA_ARGS__)
