/*
 * Who is using the device (placeholder, D-64): an adult's space that has closed (idle, locked) asks who it is before anything
 * else. It shows the members; one with a PIN asks for it; the keys do nothing. The rules are the members' and the policy's.
 */
#include <stdio.h>
#include <string.h>

#include "ink/hal_display.h"
#include "ink/osal.h"
#include "ink/sys/exec.h"
#include "ink/sys/font.h"
#include "ink/sys/members.h"
#include "ink/sys/power.h"
#include "ink/sys/state.h"
#include "ink/sys/ui.h"
#include "pinpad.h"
#include "shell_card.h"

static struct {
    shell_card_t card;
    char picked[16];
    lv_obj_t *pad;
    int result;
    uint32_t wait_s;
    ink_exec_job_t sync_job, result_job;
} G;

static void sync_gate(void *arg);

static int snapshot(char *buf, size_t cap) {
    int n = snprintf(buf, cap, "设备被锁定：请选择成员，或输入 PIN。");
    return n < 0 ? 0 : (size_t)n < cap ? n : (int)cap - 1;
}

static const ink_ui_overlay_t OVERLAY = {.name = "member_gate", .level = 2, .snapshot = snapshot};

static void result_job(void *arg) { /* UI task */
    (void)arg;
    if (G.result == INK_PIN_OK) return; /* the state changes, the gate goes */
    char s[48];
    if (G.result == INK_PIN_WAIT || G.wait_s) snprintf(s, sizeof s, "请等 %u 秒再试", (unsigned)G.wait_s);
    else snprintf(s, sizeof s, "PIN 不对");
    shell_pinpad_clear();
    shell_pinpad_say(s);
}

static void pin_result(void *ctx, int result, uint32_t wait_s) { /* exec.bg */
    (void)ctx;
    G.result = result;
    G.wait_s = wait_s;
    ink_exec_post(INK_EXEC_UI, &G.result_job, result_job, NULL);
}

static void pin_entered(const char *pin) { /* UI task */
    ink_member_unlock(G.picked, pin, pin_result, NULL);
    shell_pinpad_clear();
    shell_pinpad_say("正在核对…");
}

static void on_member(lv_event_t *e) {
    const ink_member_t *m = lv_event_get_user_data(e);
    snprintf(G.picked, sizeof G.picked, "%s", m->id);
    if (ink_member_has_pin(m->id)) {
        if (!G.pad) G.pad = shell_pinpad(G.card.root, pin_entered);
        shell_card_text(&G.card, m->name, "");
    } else if (ink_member_switch(m->id) == 0) {
        ink_exec_post(INK_EXEC_UI, &G.sync_job, sync_gate, NULL);
    }
}

static void sync_gate(void *arg) { /* UI task */
    (void)arg;
    bool want = !ink_member_space_open() && ink_power_state() != INK_PWR_LOCKED;
    if (!want) {
        if (G.card.root) {
            shell_card_close(&G.card);
            G.pad = NULL;
            ink_ui_overlay_remove(&OVERLAY);
        }
        return;
    }
    if (G.card.root) return;
    shell_card_open(&G.card);
    ink_ui_overlay_add(&OVERLAY, G.card.root);
    shell_card_text(&G.card, "谁在用？", "");
    shell_card_clear_buttons(&G.card);
    const ink_member_t *m;
    for (int i = 0; (m = ink_member_at(i)) != NULL; i++) shell_card_button(&G.card, m->name, on_member, (void *)m);
    /* the one whose space it is first */
    snprintf(G.picked, sizeof G.picked, "%s", ink_member_current()->id);
    G.pad = shell_pinpad(G.card.root, pin_entered);
}

static void on_change(void *ctx, int h, const char *id) {
    (void)ctx;
    (void)h;
    (void)id;
    ink_exec_post(INK_EXEC_UI, &G.sync_job, sync_gate, NULL);
}

int ink_shell_member_gate_init(void) {
    if (ink_state_watch("member.authed", on_change, NULL, INK_EXEC_DIRECT) != 0 || ink_state_watch("power.state", on_change, NULL, INK_EXEC_DIRECT) != 0) return -1;
    ink_exec_post(INK_EXEC_UI, &G.sync_job, sync_gate, NULL);
    return 0;
}
