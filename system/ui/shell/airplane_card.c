/* Airplane mode stopped a call or a message: the opaque panel says so and offers the way out (the same capability as
 * the switch in the control centre); a child is told to ask a parent. A button the policy gate refused says why on
 * the same panel. */
#include "ink/sys/i18n.h"
#include <stdio.h>
#include <string.h>

#include "ink/sys/cap.h"
#include "ink/sys/members.h"
#include "shell_card.h"
#include "ink/ui_shell.h"

static struct {
    shell_card_t card;
    char say[160];
    const char *why; /* NULL: airplane mode with the way out */
    const ink_ui_overlay_t *overlay;
} A;

static int snapshot(char *buf, size_t cap) {
    int n = A.why ? snprintf(buf, cap, TR("%s；%s。%s。按钮：知道了。"), A.overlay->name, A.say, A.why)
                  : snprintf(buf, cap, TR("AirplaneOn；飞行模式已打开：%s。按钮：关闭飞行模式（net.set_airplane enabled=false）、取消。"), A.say);
    return n < 0 ? 0 : (size_t)n < cap ? n : (int)cap - 1;
}

static bool on_key(ink_key_t key, bool long_press);
static const ink_ui_overlay_t OVERLAYS[] = {{.name = "AirplaneOn", .level = 2, .on_key = on_key, .snapshot = snapshot},
                                            {.name = "Refused", .level = 2, .on_key = on_key, .snapshot = snapshot}};

static void close_card(void) {
    if (!A.card.root) return;
    shell_card_close(&A.card);
    ink_ui_overlay_remove(A.overlay);
}

static bool on_key(ink_key_t key, bool long_press) {
    if (long_press || (key != INK_KEY_HOME && key != INK_KEY_BACK)) return false;
    close_card();
    return true;
}

static void refused_done(void *ctx, const ink_receipt_t *rc) { (void)ctx; ink_shell_refused(rc); }

static void on_button(lv_event_t *e) {
    bool off = lv_event_get_user_data(e) != NULL;
    close_card();
    if (!off) return;
    ink_call_opts_t o = {.src = INK_SRC_TOUCH, .args_json = "{\"enabled\":false}", .reply_exec = INK_EXEC_UI,
                         .member = ink_member_current()->id, .done = refused_done};
    ink_cap_call("net.set_airplane", NULL, &o);
}

static void show(const ink_ui_overlay_t *overlay, const char *title, const char *body) {
    close_card();
    A.overlay = overlay;
    shell_card_open(&A.card);
    ink_ui_overlay_add(overlay, A.card.root);
    if (!A.why) shell_card_button(&A.card, TR("关闭飞行模式"), on_button, (void *)1);
    shell_card_button(&A.card, A.why ? TR("知道了") : TR("取消"), on_button, NULL);
    shell_card_text(&A.card, title, body);
}

bool ink_shell_airplane_card(const ink_receipt_t *rc) {
    if (rc->status != INK_RC_REJECTED || strcmp(rc->detail, "airplane mode is on")) return false;
    static const char said[] = "飞行模式已打开，"; /* the title says it already */
    snprintf(A.say, sizeof A.say, "%s", strncmp(rc->say, said, sizeof said - 1) ? rc->say : rc->say + sizeof said - 1);
    A.why = ink_member_current()->role == INK_ROLE_CHILD ? TR("请家长关闭飞行模式") : NULL; /* the switch is not a child's */
    char body[sizeof A.say + 96];
    snprintf(body, sizeof body, "%s%s%s", A.say, A.why ? "\n" : "", A.why ? A.why : "");
    show(&OVERLAYS[0], TR("飞行模式已打开"), body);
    return true;
}

bool ink_shell_refused(const ink_receipt_t *rc) {
    static const char *const WHY[][2] = {{"children cannot run system actions", TR_NOOP("请家长来操作")},
                                         {"not on this child's whitelist", TR_NOOP("请家长来操作")},
                                         {"children cannot change this setting", TR_NOOP("请家长来操作")},
                                         {"today's time is up", TR_NOOP("今天的时间用完了")},
                                         {"this member's space is locked", TR_NOOP("请先解锁")}};
    if (ink_shell_airplane_card(rc)) return true;
    for (unsigned i = 0; rc->status == INK_RC_REJECTED && i < sizeof WHY / sizeof *WHY; i++)
        if (!strcmp(rc->detail, WHY[i][0])) {
            snprintf(A.say, sizeof A.say, TR("没有做"));
            A.why = TR(WHY[i][1]);
            show(&OVERLAYS[1], TR("没有做"), A.why);
            return true;
        }
    return false;
}
