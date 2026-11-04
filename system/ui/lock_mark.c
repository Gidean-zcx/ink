/*
 * Lock mark: while the device is locked the page stays on the panel (e-ink
 * keeps it without power) with a small mark at the bottom saying how to wake
 * it. Taking the mark off is the first thing the user sees after a wake key:
 * that refresh carries the "power.unlock_to_visible_ms" probe. A small mark
 * instead of a full standby picture keeps the unlock refresh small (upstream
 * paints a whole standby page and must repaint the page under it).
 */
#include "ink/hal_display.h"
#include "ink/sys/exec.h"
#include "ink/sys/font.h"
#include "ink/sys/power.h"
#include "ink/sys/ui.h"

static lv_obj_t *g_mark;
static uint64_t g_unlock_cause; /* written by the power task before posting, read in the UI task */
static int g_locked;            /* what the mark should show, the newest wins */
static ink_exec_job_t g_job;

static void show(void) { /* UI task */
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
    hal_panel_info_t pi;
    hal_display_get_info(&pi);
    ink_font_t *cjk = ink_font_for_pt(8, pi.dpi); /* the UI fonts are Latin only */
    lv_obj_set_style_text_font(l, cjk ? ink_font_lvgl(cjk) : ink_ui_font(2), 0);
    lv_obj_set_style_text_color(l, lv_color_black(), 0);
    lv_label_set_text(l, cjk ? "已锁定，按电源键或 AI 键唤醒" : "Locked - press power or AI to wake");
    lv_obj_align(g_mark, LV_ALIGN_BOTTOM_MID, 0, -ink_dp(6));
    ink_ui_hint_obj(g_mark, INK_REFRESH_FAST); /* black on white: the fast binary waveform */
}

static void hide(void) { /* UI task */
    if (!g_mark) return;
    ink_ui_hint_obj(g_mark, INK_REFRESH_TEXT); /* the page under it comes back with its gray levels */
    ink_ui_cause(g_unlock_cause, "power.unlock_to_visible_ms");
    lv_obj_delete(g_mark);
    g_mark = NULL;
}

static void sync_mark(void *arg) { /* UI task */
    (void)arg;
    if (__atomic_load_n(&g_locked, __ATOMIC_ACQUIRE)) show();
    else hide();
}

static void on_power(void *ctx, ink_power_state_t from, ink_power_state_t to, uint64_t cause_us) { /* power task */
    (void)ctx;
    if (to == INK_PWR_LOCKED) {
        __atomic_store_n(&g_locked, 1, __ATOMIC_RELEASE);
    } else if (from == INK_PWR_LOCKED) {
        g_unlock_cause = cause_us;
        __atomic_store_n(&g_locked, 0, __ATOMIC_RELEASE);
    } else return;
    ink_exec_post(INK_EXEC_UI, &g_job, sync_mark, NULL);
}

int ink_ui_lock_mark_init(void) {
    /* the chip must not sleep before the mark (or anything else the UI still draws) is on the panel */
    if (ink_power_add_busy_check(ink_ui_busy) != 0) return -1;
    return ink_power_subscribe(on_power, NULL);
}
