/*
 * The interruption card: the system's placeholder for what ink_interrupt_set_presenter shows (a call, an alarm, a
 * nearly empty battery). Full screen: it covers the page, also of a locked device, and catches every touch. The
 * real look comes with the system UI (D-64); nothing about the mechanism is here.
 */
#include <stdio.h>
#include <string.h>

#include "ink/hal_display.h"
#include "ink/sys/cap.h"
#include "ink/sys/font.h"
#include "ink/sys/interrupt.h"
#include "ink/sys/ui.h"

static struct {
    lv_obj_t *root, *title, *body, *row;
    ink_interrupt_t now;
    int nbuttons;
} C;

static const lv_font_t *font(int pt) {
    hal_panel_info_t pi;
    hal_display_get_info(&pi);
    ink_font_t *cjk = ink_font_for_pt(pt, pi.dpi); /* the UI fonts are Latin only */
    return cjk ? ink_font_lvgl(cjk) : ink_ui_font(0);
}

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

static void build(void) {
    C.root = lv_obj_create(lv_layer_sys());
    ink_ui_style_screen(C.root);
    lv_obj_set_size(C.root, LV_PCT(100), LV_PCT(100));
    lv_obj_add_flag(C.root, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_flex_flow(C.root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(C.root, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(C.root, ink_dp(24), 0);
    C.title = lv_label_create(C.root);
    lv_obj_set_style_text_font(C.title, font(16), 0);
    C.body = lv_label_create(C.root);
    lv_obj_set_style_text_font(C.body, font(12), 0);
    C.row = lv_obj_create(C.root);
    lv_obj_remove_style_all(C.row);
    lv_obj_set_size(C.row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(C.row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(C.row, ink_dp(24), 0);
}

static void fill(void) {
    lv_label_set_text(C.title, C.now.title);
    lv_label_set_text(C.body, C.now.body);
    lv_obj_clean(C.row);
    for (int i = 0; i < C.now.nactions; i++) {
        lv_obj_t *b = lv_button_create(C.row);
        ink_ui_style_button(b);
        lv_obj_set_style_pad_hor(b, ink_dp(24), 0);
        lv_obj_set_style_pad_ver(b, ink_dp(14), 0);
        lv_obj_add_event_cb(b, on_button, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        lv_obj_t *l = lv_label_create(b);
        lv_obj_set_style_text_font(l, font(12), 0);
        lv_label_set_text(l, C.now.actions[i].label);
    }
}

static void show(const ink_interrupt_t *top) { /* UI task */
    if (!top) {
        if (!C.root) return;
        lv_obj_delete(C.root);
        C.root = NULL;
        ink_ui_overlay_remove(&OVERLAY);
        ink_ui_hint_frame(INK_REFRESH_FLASH_UI); /* the page comes back in one clean refresh */
        return;
    }
    bool same = C.root && C.now.id == top->id && C.now.nactions == top->nactions && !strcmp(C.now.title, top->title) && !strcmp(C.now.body, top->body);
    C.now = *top;
    if (same) return; /* only the sound changed */
    bool first = !C.root;
    if (first) {
        build();
        ink_ui_overlay_add(&OVERLAY);
    }
    fill();
    ink_ui_hint_frame(INK_REFRESH_FLASH_UI);
    ink_ui_cause(top->raised_us, "interrupt.to_visible_ms");
}

int ink_shell_interrupt_card_init(void) {
    static const ink_int_presenter_t P = {.show = show, .exec = INK_EXEC_UI};
    ink_interrupt_set_presenter(&P);
    return 0;
}
