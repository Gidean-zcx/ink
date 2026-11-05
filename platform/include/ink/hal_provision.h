/* A local, password-protected setup AP and its small page. No hub traffic. */
#ifndef INK_HAL_PROVISION_H
#define INK_HAL_PROVISION_H
#include <stddef.h>
#include <stdint.h>
typedef void (*hal_provision_fn)(void *ctx, uint32_t request, const char *json, size_t len);
/* Callback: platform event task, must not block. JSON is borrowed. */
int hal_provision_start(const char *ssid, const char *password, hal_provision_fn fn, void *ctx);
void hal_provision_stop(void);
void hal_provision_reply(uint32_t request, int status);
#endif
