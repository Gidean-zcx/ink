/* A full-screen system card (placeholder look, D-64): over everything, covering the page, catching every touch. */
#ifndef INK_SHELL_CARD_H
#define INK_SHELL_CARD_H

#include "ink/sys/ui.h"

typedef struct {
    lv_obj_t *root, *panel, *title, *body, *row;
} shell_card_t;

/** UI task, all of them. The next frame is one clean refresh. */
void shell_card_open(shell_card_t *c);
void shell_card_text(shell_card_t *c, const char *title, const char *body);
void shell_card_clear_buttons(shell_card_t *c);
void shell_card_button(shell_card_t *c, const char *label, lv_event_cb_t cb, void *user);
void shell_card_close(shell_card_t *c);
void shell_card_full(shell_card_t *c);

#endif
