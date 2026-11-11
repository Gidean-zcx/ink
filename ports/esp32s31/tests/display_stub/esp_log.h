#include <stdarg.h>
#include <stdio.h>
static inline void ink_test_panel_log(const char *tag, const char *fmt, ...) {
    (void)tag;
    char line[240];
    va_list args;
    va_start(args, fmt);
    vsnprintf(line, sizeof line, fmt, args);
    va_end(args);
}
#define ESP_LOGI ink_test_panel_log
#define ESP_LOGW ink_test_panel_log
#define ESP_LOGE ink_test_panel_log
