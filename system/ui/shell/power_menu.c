/* The power menu (placeholder look, D-64): the power key held down. What its buttons do are the capabilities the AI has too. */
#include <stdio.h>
#include <string.h>

#include "ink/sys/cap.h"
#include "ink/sys/exec.h"
#include "ink/sys/input.h"
#include "ink/sys/interrupt.h"
#include "ink/sys/state.h"
#include "shell_card.h"

static struct {
    shell_card_t card;
    ink_exec_job_t open_job, close_job;
} M;

static int snapshot(char *buf, size_t cap) {
    int n = snprintf(buf, cap, "电源菜单：关机、重启、取消。");
    return n < 0 ? 0 : (size_t)n < cap ? n : (int)cap - 1;
}

static bool on_key(ink_key_t key, bool long_press);

static const ink_ui_overlay_t OVERLAY = {.name = "power_menu", .level = 2, .on_key = on_key, .snapshot = snapshot};

static void close_menu(void) { /* UI task */
    if (!M.card.root) return;
    shell_card_close(&M.card);
    ink_ui_overlay_remove(&OVERLAY);
}

static bool on_key(ink_key_t key, bool long_press) {
    (void)long_press;
    if (key == HAL_KEY_HOME) {
        close_menu();
        return true;
    }
    return false;
}

static void on_button(lv_event_t *e) {
    const char *cap = lv_event_get_user_data(e);
    close_menu();
    if (!cap) return;
    ink_call_opts_t o = {.src = INK_SRC_TOUCH, .reply_exec = INK_EXEC_DIRECT};
    ink_cap_call(cap, NULL, &o);
}

static void open_job(void *arg) { /* UI task */
    (void)arg;
    if (M.card.root || ink_interrupt_active()) return;
    shell_card_open(&M.card);
    ink_ui_overlay_add(&OVERLAY, M.card.root);
    shell_card_text(&M.card, "电源", "");
    shell_card_button(&M.card, "关机", on_button, (void *)"system.shutdown");
    shell_card_button(&M.card, "重启", on_button, (void *)"system.reboot");
    shell_card_button(&M.card, "取消", on_button, NULL);
}

static void on_long(void) { /* the timer task. A call or an alarm does not answer to it */
    ink_exec_post(INK_EXEC_UI, &M.open_job, open_job, NULL);
}

static void close_job(void *arg) { /* UI task */
    (void)arg;
    close_menu();
}

static void on_power(void *ctx, int h, const char *id) { /* locked: nobody is looking at it */
    (void)ctx;
    (void)id;
    char s[16] = "";
    ink_state_get_str(h, s, sizeof s);
    if (!strcmp(s, "locked")) ink_exec_post(INK_EXEC_UI, &M.close_job, close_job, NULL);
}

int ink_shell_power_menu_init(void) {
    ink_input_set_power_long_hook(on_long);
    return ink_state_watch("power.state", on_power, NULL, INK_EXEC_DIRECT);
}
