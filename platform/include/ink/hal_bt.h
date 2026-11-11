/*
 * ink HAL — a Bluetooth headset.
 *
 * Plus: BH1098 mode 2, UART1 115200 8N1. Vendor v2.0.77 documents INQUIRING, CONNECT,
 * mode acknowledgements and SCO (docs/bh1098-audio-protocol.md:98-136). Pairing uses
 * CONNECT; mode 1 restores local audio. Chip-side pairing removal and the complete
 * unsolicited link-loss protocol remain unspecified. Host DB forgetting does not
 * erase the chip's remembered devices. Headset microphone slots/reference need
 * on-device verification. The simulator models behaviour without inventing commands.
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
