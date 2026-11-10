/* Airplane mode stopped a call or a message: the opaque panel says so and offers the way out (the same capability as
 * the switch in the control centre). */
#include <stdio.h>
#include <string.h>

#include "ink/sys/cap.h"
#include "ink/sys/members.h"
#include "shell_card.h"
#include "ink/ui_shell.h"

static struct {
    shell_card_t card;
    char say[160];
} A;

static int snapshot(char *buf, size_t cap) {
    int n = snprintf(buf, cap, "AirplaneOn；飞行模式已打开：%s。按钮：关闭飞行模式（net.set_airplane enabled=false）、取消。", A.say);
    return n < 0 ? 0 : (size_t)n < cap ? n : (int)cap - 1;
}

static bool on_key(ink_key_t key, bool long_press);
static const ink_ui_overlay_t OVERLAY = {.name = "AirplaneOn", .level = 2, .on_key = on_key, .snapshot = snapshot};

static void close_card(void) {
    if (!A.card.root) return;
    shell_card_close(&A.card);
    ink_ui_overlay_remove(&OVERLAY);
}

static bool on_key(ink_key_t key, bool long_press) {
    if (long_press || (key != INK_KEY_HOME && key != INK_KEY_BACK)) return false;
    close_card();
    return true;
}

static void on_button(lv_event_t *e) {
    bool off = lv_event_get_user_data(e) != NULL;
    close_card();
    if (!off) return;
    ink_call_opts_t o = {.src = INK_SRC_TOUCH, .args_json = "{\"enabled\":false}", .reply_exec = INK_EXEC_DIRECT,
                         .member = ink_member_current()->id};
    ink_cap_call("net.set_airplane", NULL, &o);
}

bool ink_shell_airplane_card(const ink_receipt_t *rc) {
    if (rc->status != INK_RC_REJECTED || strcmp(rc->detail, "airplane mode is on")) return false;
    snprintf(A.say, sizeof A.say, "%s", rc->say);
    if (!A.card.root) {
        shell_card_open(&A.card);
        ink_ui_overlay_add(&OVERLAY, A.card.root);
        shell_card_button(&A.card, "关闭飞行模式", on_button, (void *)1);
        shell_card_button(&A.card, "取消", on_button, NULL);
    }
    static const char said[] = "飞行模式已打开，"; /* the title says it already */
    shell_card_text(&A.card, "飞行模式已打开", strncmp(A.say, said, sizeof said - 1) ? A.say : A.say + sizeof said - 1);
    return true;
}
