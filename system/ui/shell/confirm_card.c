/*
 * The confirmation card: the system's placeholder for what ink_confirm_set_presenter shows. It is drawn here, over
 * everything, so that no page can look like it; only a touch or a key on it counts as a person's answer. The real look
 * comes with the system UI (D-64).
 */
#include <stdio.h>
#include <string.h>

#include "ink/sys/confirm.h"
#include "ink/sys/exec.h"
#include "pinpad.h"
#include "shell_card.h"

static struct {
    shell_card_t card;
    ink_confirm_view_t now;
    lv_obj_t *pad;
    int pin_result;
    uint32_t pin_wait;
    ink_exec_job_t told_job;
} C;

static void told_job(void *arg);

static void pin_told(void *ctx, int result, uint32_t wait_s) { /* exec.bg */
    (void)ctx;
    __atomic_store_n(&C.pin_result, result, __ATOMIC_RELEASE);
    C.pin_wait = wait_s;
    ink_exec_post(INK_EXEC_UI, &C.told_job, told_job, NULL);
}

static void told_job(void *arg) { /* UI task */
    (void)arg;
    int r = __atomic_load_n(&C.pin_result, __ATOMIC_ACQUIRE);
    if (r == INK_PIN_OK || !C.pad) return; /* the card goes by itself */
    char s[48];
    if (C.pin_wait) snprintf(s, sizeof s, "请等 %u 秒再试", (unsigned)C.pin_wait);
    else snprintf(s, sizeof s, "密码不对");
    shell_pinpad_clear();
    shell_pinpad_say(s);
}

static void pin_entered(const char *pin) {
    ink_confirm_accept_with_pin(C.now.token, pin, pin_told, NULL);
    shell_pinpad_clear();
    shell_pinpad_say("正在核对…");
}

static void on_answer(lv_event_t *e) {
    bool yes = lv_event_get_user_data(e) != NULL;
    if (yes && C.now.parent) { /* a yes here takes a parent */
        if (!C.pad) C.pad = shell_pinpad(C.card.root, pin_entered);
        return;
    }
    ink_confirm_decide(C.now.token, yes, INK_SRC_TOUCH);
}

static int snapshot(char *buf, size_t cap) {
    int n = snprintf(buf, cap, "等你确认：%s。%s。按钮：确认、取消。", C.now.title, C.now.detail);
    return n < 0 ? 0 : (size_t)n < cap ? n : (int)cap - 1;
}

static bool on_key(ink_key_t key, bool long_press) {
    if ((key == INK_KEY_BACK || key == INK_KEY_HOME) && !long_press) ink_confirm_decide(C.now.token, false, INK_SRC_KEY); /* leaving is a no: not every device has a back key */
    return true;
}

static const ink_ui_overlay_t OVERLAY = {.name = "confirm", .level = 1, .on_key = on_key, .snapshot = snapshot};

static void show(const ink_confirm_view_t *top) { /* UI task */
    if (!top) {
        if (!C.card.root) return;
        shell_card_close(&C.card);
        C.pad = NULL;
        ink_ui_overlay_remove(&OVERLAY);
        return;
    }
    if (C.card.root && C.now.token == top->token) return;
    C.now = *top;
    if (C.pad) {
        lv_obj_delete(C.pad);
        C.pad = NULL;
    }
    if (!C.card.root) {
        shell_card_open(&C.card);
        ink_ui_overlay_add(&OVERLAY, C.card.root);
    }
    shell_card_text(&C.card, C.now.title, C.now.detail);
    shell_card_clear_buttons(&C.card);
    shell_card_button(&C.card, "确认", on_answer, (void *)1);
    shell_card_button(&C.card, "取消", on_answer, NULL);
    lv_obj_set_flex_flow(C.card.row, LV_FLEX_FLOW_ROW_REVERSE);
}

int ink_shell_confirm_card_init(void) {
    static const ink_confirm_presenter_t P = {.show = show, .exec = INK_EXEC_UI};
    ink_confirm_set_presenter(&P);
    return 0;
}
