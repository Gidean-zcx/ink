/*
 * Communication service: text messages, calls and the SIM card in use (architecture v1.0 §4.2). Capabilities message.*, call.*, events message.received / call.incoming / call.ended, state cell.*, setting cell.sim_slot.
 */
#ifndef INK_SYS_COMM_H
#define INK_SYS_COMM_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Each part is billed; long outgoing text is read from an owned text-service draft. */
#define INK_SMS_PARTS_MAX 5
#define INK_SMS_TEXT_MAX 1024 /* bytes with the NUL: 5 parts of 67 Chinese characters fit */

int ink_comm_init(void);
/** The parts (each billed) a text message would go as; < 0 when it cannot be sent as text. Any task. */
int ink_comm_sms_parts(const char *text);
/* Local keypad display only; never export these values in events or snapshots. */
void ink_comm_call_tones(char *out, size_t cap);

#ifdef __cplusplus
}
#endif
#endif
