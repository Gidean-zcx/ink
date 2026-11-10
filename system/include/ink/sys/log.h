#ifndef INK_SYS_LOG_H
#define INK_SYS_LOG_H
#include <stdbool.h>
#include <stdint.h>
typedef enum { INK_LOG_DEBUG, INK_LOG_INFO, INK_LOG_WARN, INK_LOG_ERROR } ink_log_level_t;
int ink_log_init(void);
int ink_log_start(void);
void ink_log(ink_log_level_t level, const char *module, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
void ink_log_poll(void); /* storage task only */
bool ink_log_flush(void); /* may wait: background/shutdown callers only; obeys audio holds */
void ink_log_health(void);
void ink_log_display(uint64_t us, bool ok);
void ink_log_input(void);
void ink_log_page(const char *app, const char *page);
#endif
