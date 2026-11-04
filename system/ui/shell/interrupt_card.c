/*
 * The interruption card: the system's placeholder for what ink_interrupt_set_presenter shows (a call, an alarm, a
 * nearly empty battery). It covers the page, also of a locked device. The real look comes with the system UI (D-64);
 * nothing about the mechanism is here.
 */
#include <stdio.h>
#include <string.h>

#include "ink/sys/cap.h"
#include "ink/sys/interrupt.h"
#include "shell_card.h"

static struct {
    shell_card_t card;
    ink_interrupt_t now;
} C;

static void on_button(lv_event_t *e) {
    int index = (int)(intptr_t)lv_event_get_user_data(e);
    ink_interrupt_action(C.now.id, index, INK_SRC_TOUCH);
}

static int snapshot(char *buf, size_t cap) {
    int n = snprintf(buf, cap, "%s：%s。", C.now.title, C.now.body);
    for (int i = 0; i < C.now.nactions && n >= 0 && (size_t)n < cap; i++) n += snprintf(buf + n, cap - (size_t)n, "按钮：%s。", C.now.actions[i].label);
    return n < 0 ? 0 : (size_t)n < cap ? n : (int)cap - 1;
}

static const ink_ui_overlay_t OVERLAY = {.name = "interrupt", .snapshot = snapshot}; /* the keys do nothing: only the power key does (in the input service) */

static void show(const ink_interrupt_t *top) { /* UI task */
    if (!top) {
        if (!C.card.root) return;
        shell_card_close(&C.card);
        ink_ui_overlay_remove(&OVERLAY);
        return;
    }
    bool same = C.card.root && C.now.id == top->id && C.now.nactions == top->nactions && !strcmp(C.now.title, top->title) && !strcmp(C.now.body, top->body);
    C.now = *top;
    if (same) return; /* only the sound changed */
    if (!C.card.root) {
        shell_card_open(&C.card);
        ink_ui_overlay_add(&OVERLAY);
    }
    shell_card_text(&C.card, C.now.title, C.now.body);
    shell_card_clear_buttons(&C.card);
    for (int i = 0; i < C.now.nactions; i++) shell_card_button(&C.card, C.now.actions[i].label, on_button, (void *)(intptr_t)i);
    ink_ui_cause(top->raised_us, "interrupt.to_visible_ms");
}

int ink_shell_interrupt_card_init(void) {
    static const ink_int_presenter_t P = {.show = show, .exec = INK_EXEC_UI};
    ink_interrupt_set_presenter(&P);
    return 0;
}
