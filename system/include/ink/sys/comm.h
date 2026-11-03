/*
 * Communication service: text messages, calls and the SIM card in use (architecture v1.0 §4.2). Capabilities message.*, call.*, events message.received / call.incoming / call.ended, state cell.*, setting cell.sim_slot.
 */
#ifndef INK_SYS_COMM_H
#define INK_SYS_COMM_H

#ifdef __cplusplus
extern "C" {
#endif

int ink_comm_init(void);

#ifdef __cplusplus
}
#endif
#endif
