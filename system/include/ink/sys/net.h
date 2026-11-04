/*
 * Network service: system state net.online / net.type / net.signal and the setting net.prefer (architecture v1.0 §4.2). Joining networks stays inside the platform.
 */
#ifndef INK_SYS_NET_H
#define INK_SYS_NET_H

#ifdef __cplusplus
extern "C" {
#endif

int ink_net_init(void);
/** Wi-Fi management: the networks it knows, the capabilities. After the data store. */
int ink_wifi_init(void);

#ifdef __cplusplus
}
#endif
#endif
