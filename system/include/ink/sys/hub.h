/*
 * Hub link service: the device's single connection to the family hub
 * (D-04, D-12, D-24). JSON-RPC 2.0 messages with the hub's own envelope over
 * a WebSocket; the connection is opened on demand and closed after a quiet
 * period (active long connection, disconnect when idle).
 *
 * Callbacks run in the hub task: keep them short and post work elsewhere
 * (ink_exec_post for the UI or another executor).
 */
#ifndef INK_SYS_HUB_H
#define INK_SYS_HUB_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ink/sys/json.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
    INK_HUB_OK = 0,
    INK_HUB_ERR_REMOTE = -1,  /* the hub answered with an error object */
    INK_HUB_ERR_TIMEOUT = -2, /* no answer in time */
    INK_HUB_ERR_OFFLINE = -3, /* no link / could not connect in time */
    INK_HUB_ERR_LOST = -4,    /* connection lost after the request was sent */
    INK_HUB_ERR_NOMEM = -5,
};

/* `msg` is the whole reply message (parsed), `result` the index of its result/error token. */
typedef void (*ink_hub_reply_fn)(void *ctx, int status, const ink_json_t *msg, int result);
/* A notification (e.g. "ai.delta"); `params` = index of its params token. */
typedef void (*ink_hub_notify_fn)(void *ctx, const char *method, const ink_json_t *msg, int params);

int ink_hub_init(const char *url);
/** Asynchronous request. `params_json` = a JSON object text (copied). Returns the request id (> 0) or < 0. */
int ink_hub_call(const char *method, const char *params_json, int64_t timeout_us, ink_hub_reply_fn cb, void *ctx);
/** Receive notifications whose method starts with `prefix` (e.g. "ai."). Up to 8 subscribers. */
int ink_hub_subscribe(const char *prefix, ink_hub_notify_fn fn, void *ctx);
/** User activity: open the connection now if needed and keep it up for a while (pre-connect on key press). */
void ink_hub_demand(void);
bool ink_hub_online(void);
/** What the hub said the time was when this connection came up, and how long that took to hear. False if it did not say. */
bool ink_hub_server_time(int64_t *unix_ms, uint32_t *round_trip_ms);
/** fn(ctx) in the hub task each time the connection is (re)established. Up to 4. */
int ink_hub_on_online(void (*fn)(void *ctx), void *ctx);
/** fn() in the hub task, whenever it is awake and online: what waits to go out goes now. One. */
void ink_hub_set_tick(void (*fn)(void));
/** Have it look soon (any task, never waits). */
void ink_hub_wake(void);
/* Enrollment service: reconnect after durable configuration/identity changes. */
void ink_hub_reconnect(void);
int ink_hub_pair_call(const char *method, const char *params);
/** Stay connected while held (reference-counted; realtime sessions hold it). */
void ink_hub_keep(bool on);

/**
 * Fire-and-forget while online (returns INK_HUB_ERR_OFFLINE otherwise): a JSON-RPC notification,
 * or one binary message. Binary messages carry media: [kind u8][stream u8][seq u16 LE] + payload,
 * kind 1 = microphone Opus frame (device -> hub), 2 = TTS Opus frame (hub -> device).
 */
int ink_hub_notify(const char *method, const char *params_json);
int ink_hub_send_binary(const void *data, size_t len);
/* Send classes, highest first: what waits in the queue is sent in this order (detailed design §6.4). */
enum { INK_HUB_TX_VOICE = 0, INK_HUB_TX_INPUT, INK_HUB_TX_SCREEN, INK_HUB_TX_CONTROL, INK_HUB_TX_BACKGROUND };
/** One binary message in a send class (ink_hub_send_binary is the voice class). */
int ink_hub_send_frame(const void *data, size_t len, int cls);
typedef void (*ink_hub_binary_fn)(void *ctx, const uint8_t *data, size_t len);
/** The (single) receiver of binary messages from the hub; runs in the hub task. */
void ink_hub_on_binary(ink_hub_binary_fn fn, void *ctx);

/**
 * Requests FROM the hub (integer ids), e.g. "tools/call" when the model uses a device action.
 * The handler runs in the hub task and must answer exactly once (possibly later, from any task)
 * with ink_hub_respond / ink_hub_respond_error.
 */
typedef void (*ink_hub_request_fn)(void *ctx, int64_t id, const char *method, const ink_json_t *msg, int params);
int ink_hub_on_request(const char *prefix, ink_hub_request_fn fn, void *ctx);
int ink_hub_respond(int64_t id, const char *result_json);
int ink_hub_respond_error(int64_t id, int code, const char *message);

#ifdef __cplusplus
}
#endif
#endif
