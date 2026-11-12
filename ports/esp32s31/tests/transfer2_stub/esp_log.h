#pragma once
static inline void ink_test_log_ignore(const char *tag, const char *format, ...) { (void)tag; (void)format; }
#define ESP_LOGI(...) ink_test_log_ignore(__VA_ARGS__)
#define ESP_LOGW(...) ink_test_log_ignore(__VA_ARGS__)
