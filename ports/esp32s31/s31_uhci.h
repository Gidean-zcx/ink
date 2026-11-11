#ifndef INK_S31_UHCI_H
#define INK_S31_UHCI_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
bool ink_s31_at_allowed(const char *command);
bool ink_s31_at_urc(const char *line);
void ink_s31_urc_init(void);
void ink_s31_at_log(const char *command, bool ok, size_t length);
#ifdef __cplusplus
}
#endif
#endif
