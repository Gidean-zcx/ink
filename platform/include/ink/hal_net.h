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
/** The system's choice of link for IP traffic (Wi-Fi, LTE or none). Joining Wi-Fi takes a few
 *  seconds; the link comes up once the chosen network is there. Task context. */
void hal_net_use(hal_link_type_t type);
/** That network is around, whether in use or not: a known Wi-Fi network in range, a registered card
 *  under coverage. What the system chooses from. Task context, does not wait. */
bool hal_net_available(hal_link_type_t type);

/* ---- Wi-Fi management ---- */
typedef struct {
    char ssid[33];
    int rssi;
    bool secured;
} hal_wifi_ap_t;
typedef struct {
    char ssid[33];
    char password[65];
} hal_wifi_known_t;
enum { HAL_WIFI_OK = 0, HAL_WIFI_ERR_AUTH = -1, HAL_WIFI_ERR_NOT_FOUND = -2, HAL_WIFI_ERR_TIMEOUT = -3 };
/** What is around, strongest first (a task; blocks a few seconds). How many (up to max). */
int hal_wifi_scan(hal_wifi_ap_t *out, int max, uint32_t timeout_ms);
/** Try this network and these words now (a task; blocks). HAL_WIFI_*. */
int hal_wifi_connect(const char *ssid, const char *password, uint32_t timeout_ms);
/** What automatic choosing may join: hal_net_available(WIFI) is one of these in range. Copied. */
void hal_wifi_set_known(const hal_wifi_known_t *list, int n);

typedef void (*hal_link_cb)(void *ctx, const hal_link_status_t *st);
/** Notified when the link changes (comes up, goes down, switches Wi-Fi/LTE) and when a network
 *  appears or goes away. Up to four listeners, each added once (on the device: esp_event handlers);
 *  they run in the platform's event task. */
void hal_net_set_link_callback(hal_link_cb cb, void *ctx);
/** Network UTC seconds; short callback in the network task. NULL disables delivery. */
void hal_net_set_time_callback(void (*cb)(void *ctx, int64_t utc_s), void *ctx);

/* ---- cellular modem AT channel ----
 * The modem's AT command port (the Plus: NT26 over the UART-Ethernet link, multiplexed with the
 * data frames). Drivers (drivers/chips/nt26) build features such as switching SIM cards on it. */
typedef enum { HAL_AT_OK = 0, HAL_AT_ERROR = -1, HAL_AT_TIMEOUT = -2, HAL_AT_NO_MODEM = -3 } hal_at_result_t;

/** Send one command (no CR/LF) and wait up to timeout_ms for its final result code. resp receives
 *  the information lines before it, separated by '\n' (e.g. "+CEREG: 0,1"), or "" if none. Task
 *  context; callers serialize (one command at a time, as on the real link). */
hal_at_result_t hal_modem_at(const char *cmd, char *resp, size_t resp_cap, uint32_t timeout_ms);
/** A command that prompts for data ("> "), such as AT+CMGS: send cmd, then payload ended by
 *  Ctrl-Z, and wait for the final result. */
hal_at_result_t hal_modem_at_payload(const char *cmd, const char *payload, char *resp, size_t resp_cap, uint32_t timeout_ms);
/** Next unsolicited line from the modem (RING, +CMTI..., NO CARRIER), oldest first; its length, or
 *  -1 after timeout_ms. Task context. */
int hal_modem_urc(char *buf, size_t cap, uint32_t timeout_ms);

/*
 * WebSocket client (TLS). Threads and lifetime:
 *  - cb runs in a platform network task, one call at a time per connection, in this order: at most
 *    one HAL_WS_CONNECTED, then TEXT / BINARY, then at most one HAL_WS_CLOSED. `data` is valid only
 *    during the call. cb must return soon and must not wait without a bound on the task that frees
 *    the connection (hal_ws_free waits for cb); it must not call hal_ws_send or hal_ws_free for its
 *    own connection.
 *  - hal_ws_open, hal_ws_send, hal_ws_free: any task, not an ISR, not from cb. One task at a time
 *    per connection for send and free (the owner).
 *  - After HAL_WS_CLOSED the handle stays valid (send fails) until hal_ws_free.
 */
typedef struct hal_ws hal_ws_t;

typedef enum {
    HAL_WS_CONNECTED = 1, /* TLS + WebSocket handshake done */
    HAL_WS_TEXT,          /* one complete text message */
    HAL_WS_BINARY,        /* one complete binary message (audio) */
    HAL_WS_CLOSED,        /* closed by the peer, by link loss or a failed connect; not after hal_ws_free */
} hal_ws_event_t;

typedef void (*hal_ws_cb)(void *ctx, hal_ws_event_t ev, const uint8_t *data, size_t len);

/** Start connecting (asynchronous). Returns NULL only if out of memory. */
hal_ws_t *hal_ws_open(const char *url, hal_ws_cb cb, void *ctx);
/* Verify SHA-256 of the peer SPKI before CONNECTED. NULL preserves normal TLS validation. */
hal_ws_t *hal_ws_open_pinned(const char *url, const uint8_t *sha256, hal_ws_cb cb, void *ctx);
/* The observed peer SPKI digest, available after CONNECTED. */
bool hal_ws_peer_key(hal_ws_t *ws, uint8_t out[32]);
/**
 * Queue one message. The platform copies it. Blocks up to timeout_us while
 * the send buffer is full. Returns 0, or -1 if not connected / timed out.
 */
int hal_ws_send(hal_ws_t *ws, bool binary, const void *data, size_t len, int64_t timeout_us);
/* Owner task: finish queued writes before an intentional enrollment reconnect. Bounded, 0 or -1. */
int hal_ws_flush(hal_ws_t *ws, int64_t timeout_us);
/** Close if still open (no HAL_WS_CLOSED for that), wait until no callback is running and none can
 *  start, release the handle. ctx may be freed once it returns. May block for a network round trip. */
void hal_ws_free(hal_ws_t *ws);

#ifdef __cplusplus
}
#endif
#endif
