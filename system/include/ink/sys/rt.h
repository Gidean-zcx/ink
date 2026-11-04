/*
 * Realtime sessions (architecture v1.0 §9.2, detailed design §6): a remote
 * capability ("voice.chat", "browser.session") and a local endpoint joined
 * by several streams over the hub connection. Control is JSON-RPC (rt.open,
 * rt.turn, rt.nav, rt.close); data is binary frames with a 10-byte header:
 *
 *   [kind u8][sid u8][seq u16 LE][t_ms u32 LE][flags u8][0]  payload...
 *
 * The device picks the session id; frames can be sent once the hub has
 * confirmed the session on the current connection (ink_rt_ready; after a
 * reconnect the session is announced again by itself). Frames are sent by class (voice > input > screen > control > background), and while a
 * session is open background jobs are held.
 */
#ifndef INK_SYS_RT_H
#define INK_SYS_RT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ink/sys/json.h"

#ifdef __cplusplus
extern "C" {
#endif

enum { INK_RT_VOICE_UP = 1, INK_RT_VOICE_DOWN, INK_RT_SCREEN, INK_RT_INPUT, INK_RT_CREDIT, INK_RT_KINDS };
enum { INK_RT_F_LAST = 1, INK_RT_F_FULL = 2, INK_RT_F_DISCARDABLE = 4 };
#define INK_RT_HDR 10
#define INK_RT_MAX 4
#define INK_RT_PARAMS 240 /* longest session parameters (JSON) */

typedef struct {
    /* hub task: keep them short, hand the work on */
    void (*on_frame)(void *ctx, int sid, uint8_t kind, const uint8_t *payload, size_t len, uint16_t seq, uint32_t t_ms, uint8_t flags);
    void (*on_control)(void *ctx, int sid, const char *method, const ink_json_t *msg, int params);
    void (*on_closed)(void *ctx, int sid, int reason); /* the hub refused or ended it: the session is gone */
    void (*on_ready)(void *ctx, int sid); /* the hub confirmed it on this connection: also after a reconnect,
                                           * when that hub knows nothing of what was sent before */
    void *ctx;
} ink_rt_handler_t;

int ink_rt_init(void);
/** Open a session with a remote capability: its id (1..255), or -1 (all INK_RT_MAX in use, or
 *  parameters longer than INK_RT_PARAMS). */
int ink_rt_open(const char *cap, const char *params_json, const ink_rt_handler_t *h);
/** One frame (copied). 0, or < 0 (no such session, not ready yet, queue full). */
int ink_rt_send(int sid, uint8_t kind, const void *payload, size_t len, uint8_t flags);
/** A control message in the session ("rt.turn", "rt.nav" ...): params get "sid" added. */
int ink_rt_control(int sid, const char *method, const char *params_json);
void ink_rt_close(int sid);
/** The hub confirmed the session on the current connection: frames can go. */
bool ink_rt_ready(int sid);
int ink_rt_open_count(void);

#ifdef __cplusplus
}
#endif
#endif
