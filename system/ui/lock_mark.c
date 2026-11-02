/*
 * Lock mark: while the device is locked the page stays on the panel (e-ink
 * keeps it without power) with a small mark at the bottom saying how to wake
 * it. Taking the mark off is the first thing the user sees after a wake key:
 * that refresh carries the "power.unlock_to_visible_ms" probe. A small mark
 * instead of a full standby picture keeps the unlock refresh small (upstream
 * paints a whole standby page and must repaint the page under it).
 */
#include "ink/sys/power.h"
#include "ink/sys/ui.h"

static lv_obj_t *g_mark;
static uint64_t g_unlock_cause; /* written by the power task before posting, read in the UI task */

static void show(void *arg) { /* UI task */
    (void)arg;
    if (g_mark) return;
    g_mark = lv_obj_create(lv_layer_top());
    lv_obj_remove_flag(g_mark, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(g_mark, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(g_mark, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(g_mark, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(g_mark, lv_color_black(), 0);
    lv_obj_set_style_border_width(g_mark, 2, 0);
    lv_obj_set_style_radius(g_mark, ink_dp(6), 0);
    lv_obj_set_style_pad_hor(g_mark, ink_dp(10), 0);
    lv_obj_set_style_pad_ver(g_mark, ink_dp(4), 0);
    lv_obj_t *l = lv_label_create(g_mark);
    lv_obj_set_style_text_font(l, ink_ui_font(0), 0);
    lv_obj_set_style_text_color(l, lv_color_black(), 0);
    lv_label_set_text(l, "已锁定 · 按电源键唤醒");
    lv_obj_align(g_mark, LV_ALIGN_BOTTOM_MID, 0, -ink_dp(6));
    ink_ui_hint_obj(g_mark, INK_REFRESH_FAST); /* black on white: the fast binary waveform */
}

static void hide(void *arg) { /* UI task */
    (void)arg;
    if (!g_mark) return;
    ink_ui_hint_obj(g_mark, INK_REFRESH_TEXT); /* the page under it comes back with its gray levels */
    ink_ui_cause(g_unlock_cause, "power.unlock_to_visible_ms");
    lv_obj_delete(g_mark);
    g_mark = NULL;
}

static void on_power(void *ctx, ink_power_state_t from, ink_power_state_t to, uint64_t cause_us) { /* power task */
    (void)ctx;
    if (to == INK_PWR_LOCKED) {
        ink_ui_post(show, NULL);
    } else if (from == INK_PWR_LOCKED) {
        g_unlock_cause = cause_us;
        ink_ui_post(hide, NULL);
    }
}

int ink_ui_lock_mark_init(void) { return ink_power_subscribe(on_power, NULL); }
