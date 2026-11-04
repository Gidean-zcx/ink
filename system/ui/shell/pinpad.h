/* A row of digits on a card, for a PIN: the digits go nowhere but `done` (placeholder look, D-64). */
#ifndef INK_SHELL_PINPAD_H
#define INK_SHELL_PINPAD_H

#include "ink/sys/ui.h"

/** Added to `parent` (a column); `done(pin)` when the person presses OK. UI task. */
lv_obj_t *shell_pinpad(lv_obj_t *parent, void (*done)(const char *pin));
void shell_pinpad_say(const char *text);
void shell_pinpad_clear(void);

#endif
