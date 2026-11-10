/*
 * Communication service: text messages, calls and the SIM card in use (architecture v1.0 §4.2). Capabilities message.*, call.*, events message.received / call.incoming / call.ended, state cell.*, setting cell.sim_slot.
 */
#ifndef INK_SYS_COMM_H
#define INK_SYS_COMM_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int ink_comm_init(void);
/* Local keypad display only; never export these values in events or snapshots. */
void ink_comm_call_tones(char *out, size_t cap);

#ifdef __cplusplus
}
#endif
#endif
