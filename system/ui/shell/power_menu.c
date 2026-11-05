/* The power menu (placeholder look, D-64): the power key held down. What its buttons do are the capabilities the AI has too. */
#include <stdio.h>
#include <string.h>

#include "ink/sys/cap.h"
#include "ink/sys/exec.h"
#include "ink/sys/input.h"
#include "ink/sys/interrupt.h"
#include "ink/sys/state.h"
#include "shell_card.h"
#include "ink/ui_shell.h"

static struct {
    shell_card_t card;
    ink_exec_job_t open_job, close_job;
} M;

static int snapshot(char *buf, size_t cap) {
    int n = snprintf(buf, cap, "电源菜单：关机、重启、锁屏、取消。动作：system.shutdown、system.reboot、shell.lock。");
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

static void power_tile(const char *label, const char *icon, int col, const char *action, bool primary) {
    int cell = (INK_CANVAS_W - 2 * INK_MARGIN) / 3;
    lv_obj_t *b = ink_ui_pill(M.card.panel, "", INK_MARGIN + col * cell, INK_MARGIN, cell,
                              INK_CIRCLE_LARGE + INK_GAP_M + INK_LINE_LIST, false, on_button, (void *)action);
    lv_obj_set_style_border_width(b, 0, 0);
    ink_ui_circle(b, icon, (cell - INK_CIRCLE_LARGE) / 2 - INK_GAP, 0, INK_CIRCLE_LARGE, INK_ICON_APP,
                  primary ? INK_SURFACE_BLACK : INK_SURFACE_WHITE, NULL, NULL);
    lv_obj_t *l = ink_ui_label(b, label, INK_FONT_LIST, INK_WEIGHT_MEDIUM, 0, INK_CIRCLE_LARGE + INK_GAP_M, cell - 2 * INK_GAP);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
}

static void open_job(void *arg) { /* UI task */
    (void)arg;
    if (M.card.root || ink_interrupt_active()) return;
    shell_card_open(&M.card);
    ink_ui_overlay_add(&OVERLAY, M.card.root);
    int h = 2 * INK_MARGIN + INK_CIRCLE_LARGE + INK_GAP_M + INK_LINE_LIST + INK_MARGIN + INK_PRIMARY;
    lv_obj_clean(M.card.panel);
    lv_obj_set_y(M.card.panel, ink_ui_px(INK_CANVAS_H - h));
    lv_obj_set_height(M.card.panel, ink_ui_px(h + INK_RADIUS_PANEL));
    power_tile("关机", "power", 0, "system.shutdown", true);
    power_tile("重启", "update", 1, "system.reboot", false);
    lv_obj_t *lock = ink_ui_circle(M.card.panel, "lock", INK_MARGIN + 2 * ((INK_CANVAS_W - 2 * INK_MARGIN) / 3) + INK_GAP_L,
                                   INK_MARGIN, INK_CIRCLE_LARGE, INK_ICON_APP, INK_SURFACE_WHITE, NULL, NULL);
    ink_shell_bind(lock, "shell.lock", NULL);
    ink_ui_label(M.card.panel, "锁屏", INK_FONT_LIST, INK_WEIGHT_MEDIUM,
                 INK_CANVAS_W - INK_MARGIN - INK_CIRCLE_LARGE + INK_GAP_L, INK_MARGIN + INK_CIRCLE_LARGE + INK_GAP_M, INK_CIRCLE_LARGE);
    ink_ui_pill(M.card.panel, "取消", INK_MARGIN, h - INK_PRIMARY - INK_MARGIN,
                 INK_CANVAS_W - 2 * INK_MARGIN, INK_PRIMARY, false, on_button, NULL);
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
