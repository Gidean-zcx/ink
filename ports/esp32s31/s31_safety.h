#ifndef INK_S31_SAFETY_H
#define INK_S31_SAFETY_H
#include <stdint.h>
void ink_s31_boot_begin(void);
void ink_s31_display_shutdown(void);
void ink_s31_display_panic(void);
void ink_s31_panel_panic(int xoe, int mode, uint8_t port0);
#endif
