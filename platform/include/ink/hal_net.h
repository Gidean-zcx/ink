/*
 * ink HAL — network link to the family hub.
 *
 * The device talks to exactly one service (the hub, D-04) over a WebSocket
 * that the device opens (works behind 4G NAT). The platform owns radio,
 * IP stack, TLS and the WebSocket framing; the system sees a message channel
 * plus the link state:
 *   device     Wi-Fi or the NT26 LTE module (UART-Ethernet, ~1.6 Mbit/s),
 *              lwIP + mbedTLS + esp_websocket_client
 *   POSIX      the host network stack
 *   simulator  link model (RTT distributions, bandwidth, loss, LTE idle
 *              resume, outages, handshake CPU and memory) + a scripted hub
 *
 * Callbacks run in a platform network task: keep them short (post a message).
 */
#ifndef INK_HAL_NET_H
#define INK_HAL_NET_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    HAL_LINK_NONE = 0, /* no usable link (airplane, no coverage, no Wi-Fi) */
    HAL_LINK_WIFI = 1,
    HAL_LINK_LTE = 2,
} hal_link_type_t;

typedef struct {
    hal_link_type_t type;
    bool up;          /* IP connectivity right now */
    int signal_dbm;   /* RSSI (Wi-Fi) or RSRP (LTE); 0 if unknown */
} hal_link_status_t;

void hal_net_link_status(hal_link_status_t *out);

typedef void (*hal_link_cb)(void *ctx, const hal_link_status_t *st);
/** Notified when the link changes (comes up, goes down, switches Wi-Fi/LTE). */
void hal_net_set_link_callback(hal_link_cb cb, void *ctx);

/* ---- cellular modem AT channel ----
 * The modem's AT command port (the Plus: NT26 over the UART-Ethernet link, multiplexed with the
 * data frames). Drivers (drivers/nt26) build features such as switching SIM cards on it. */
typedef enum { HAL_AT_OK = 0, HAL_AT_ERROR = -1, HAL_AT_TIMEOUT = -2, HAL_AT_NO_MODEM = -3 } hal_at_result_t;

/** Send one command (no CR/LF) and wait up to timeout_ms for its final result code. resp receives
 *  the information line before it (e.g. "+CEREG: 0,1"), or "" if none. Task context; callers
 *  serialize (one command at a time, as on the real link). */
hal_at_result_t hal_modem_at(const char *cmd, char *resp, size_t resp_cap, uint32_t timeout_ms);

typedef struct hal_ws hal_ws_t;

typedef enum {
    HAL_WS_CONNECTED = 1, /* TLS + WebSocket handshake done */
    HAL_WS_TEXT,          /* one complete text message */
    HAL_WS_BINARY,        /* one complete binary message (audio) */
    HAL_WS_CLOSED,        /* closed by either side or by link loss; the handle stays valid until hal_ws_free */
} hal_ws_event_t;

typedef void (*hal_ws_cb)(void *ctx, hal_ws_event_t ev, const uint8_t *data, size_t len);

/** Start connecting (asynchronous). Returns NULL only if out of memory. */
hal_ws_t *hal_ws_open(const char *url, hal_ws_cb cb, void *ctx);
/**
 * Queue one message. The platform copies it. Blocks up to timeout_us while
 * the send buffer is full. Returns 0, or -1 if not connected / timed out.
 */
int hal_ws_send(hal_ws_t *ws, bool binary, const void *data, size_t len, int64_t timeout_us);
/** Close (no more callbacks after HAL_WS_CLOSED) and release. */
void hal_ws_free(hal_ws_t *ws);

#ifdef __cplusplus
}
#endif
#endif
