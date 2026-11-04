/*
 * The confirmation card: the system's placeholder for what ink_confirm_set_presenter shows. It is drawn here, over
 * everything, so that no page can look like it; only a touch or a key on it counts as a person's answer. The real look
 * comes with the system UI (D-64).
 */
#include <stdio.h>
#include <string.h>

#include "ink/sys/confirm.h"
#include "shell_card.h"

static struct {
    shell_card_t card;
    ink_confirm_view_t now;
} C;

static void on_answer(lv_event_t *e) { ink_confirm_decide(C.now.token, lv_event_get_user_data(e) != NULL, INK_SRC_TOUCH); }

static int snapshot(char *buf, size_t cap) {
    int n = snprintf(buf, cap, "等你确认：%s。%s。按钮：确认、取消。", C.now.title, C.now.detail);
    return n < 0 ? 0 : (size_t)n < cap ? n : (int)cap - 1;
}

static bool on_key(ink_key_t key, bool long_press) {
    if ((key == INK_KEY_BACK || key == INK_KEY_HOME) && !long_press) ink_confirm_decide(C.now.token, false, INK_SRC_KEY); /* leaving is a no: not every device has a back key */
    return true;
}

static const ink_ui_overlay_t OVERLAY = {.name = "confirm", .on_key = on_key, .snapshot = snapshot};

static void show(const ink_confirm_view_t *top) { /* UI task */
    if (!top) {
        if (!C.card.root) return;
        shell_card_close(&C.card);
        ink_ui_overlay_remove(&OVERLAY);
        return;
    }
    if (C.card.root && C.now.token == top->token) return;
    C.now = *top;
    if (!C.card.root) {
        shell_card_open(&C.card);
        ink_ui_overlay_add(&OVERLAY);
    }
    shell_card_text(&C.card, C.now.title, C.now.detail);
    shell_card_clear_buttons(&C.card);
    shell_card_button(&C.card, "确认", on_answer, (void *)1);
    shell_card_button(&C.card, "取消", on_answer, NULL);
}

int ink_shell_confirm_card_init(void) {
    static const ink_confirm_presenter_t P = {.show = show, .exec = INK_EXEC_UI};
    ink_confirm_set_presenter(&P);
    return 0;
}
