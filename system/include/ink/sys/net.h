/*
 * Network service: system state net.online / net.type / net.signal and the setting net.prefer (architecture v1.0 §4.2). Joining networks stays inside the platform.
 */
#ifndef INK_SYS_NET_H
#define INK_SYS_NET_H
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int ink_net_init(void);
/** Wi-Fi management: the networks it knows, the capabilities. After the data store. */
int ink_wifi_init(void);
/* Background setup page: do not discard optional Wi-Fi credentials before this succeeds. */
int ink_wifi_wait_saved(uint32_t revision);

#ifdef __cplusplus
}
#endif
#endif
