/* One UART1 owner for audio and headset control. */
#ifndef INK_S31_BH1098_H
#define INK_S31_BH1098_H
#include "ink/hal_audio.h"
#include "ink/hal_uart.h"
void ink_s31_bh_init(void);
bool ink_s31_bh_ready(void);
int ink_s31_bh_route(hal_audio_route_t route);
int ink_s31_bh_capture(bool on);
void ink_s31_bh_call(bool on);
bool ink_s31_bh_headset(void);
bool ink_s31_bh_amp(bool on);
void ink_s31_bh_suspend(void);
void ink_s31_bh_resume(void);
void ink_s31_bh_poll(void);
void ink_s31_bh_wake(void); /* wake the existing input/peripheral receiver */
bool ink_s31_uart_discard(hal_uart_t *u);
#endif
