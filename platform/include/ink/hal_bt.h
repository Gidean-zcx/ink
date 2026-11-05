/*
 * ink HAL — a Bluetooth headset.
 *
 * What is known about the Plus: the BH1098G audio chip has a mode 2, officially "calls through a Bluetooth headset". Its
 * serial protocol for scanning, pairing and connecting has not been seen, nor how good the sound is, nor whether the
 * echo reference exists in that mode (system services design §10.4, §14). So this is an interface and no more: a port
 * that does not know how answers hal_bt_available() false, and the system simply uses the speaker. The simulator's headset
 * is a model of behaviour at this level: it invents no chip commands.
 */
#ifndef INK_HAL_BT_H
#define INK_HAL_BT_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char addr[18];
    char name[32];
} hal_bt_device_t;

typedef enum { HAL_BT_EV_CONNECTED = 1, HAL_BT_EV_LOST } hal_bt_event_t;

bool hal_bt_available(void);
/** Looks for headsets for this long (blocks). How many (up to max). */
int hal_bt_scan(hal_bt_device_t *out, int max, int timeout_ms);
/** All of these block, 0 or -1: not there, not paired. */
int hal_bt_pair(const char *addr);
int hal_bt_connect(const char *addr);
void hal_bt_disconnect(void);
void hal_bt_forget(const char *addr);
/** One listener; it runs where the link's news arrives: keep it short. */
void hal_bt_on_event(void (*fn)(void *ctx, hal_bt_event_t ev), void *ctx);

#ifdef __cplusplus
}
#endif
#endif
