/*
 * The interruption card: the system's placeholder for what ink_interrupt_set_presenter shows (a call, an alarm, a
 * nearly empty battery). It covers the page, also of a locked device. The real look comes with the system UI (D-64);
 * nothing about the mechanism is here.
 */
#include <stdio.h>
#include <string.h>

#include "ink/sys/cap.h"
#include "ink/sys/interrupt.h"
#include "shell_card.h"
#include "ink/ui_shell.h"

static struct {
    shell_card_t card;
    ink_interrupt_t now;
    lv_obj_t *icon;
} C;

static void on_button(lv_event_t *e) {
    int index = (int)(intptr_t)lv_event_get_user_data(e);
    ink_interrupt_action(C.now.id, index, INK_SRC_TOUCH);
}

static int snapshot(char *buf, size_t cap) {
    int n = snprintf(buf, cap, "%s：%s。", C.now.title, C.now.body);
    for (int i = 0; i < C.now.nactions && n >= 0 && (size_t)n < cap; i++) n += snprintf(buf + n, cap - (size_t)n, "按钮：%s。", C.now.actions[i].label);
    return n < 0 ? 0 : (size_t)n < cap ? n : (int)cap - 1;
}

static const ink_ui_overlay_t OVERLAY = {.name = "interrupt", .level = 3, .snapshot = snapshot}; /* the keys do nothing: only the power key does (in the input service) */

static void show(const ink_interrupt_t *top) { /* UI task */
    if (!top) {
        if (!C.card.root) return;
        shell_card_close(&C.card);
        C.icon = NULL;
        ink_ui_overlay_remove(&OVERLAY);
        return;
    }
    bool same = C.card.root && C.now.id == top->id && C.now.kind == top->kind && C.now.nactions == top->nactions && !strcmp(C.now.title, top->title) && !strcmp(C.now.body, top->body);
    C.now = *top;
    if (same) return; /* only the sound changed */
    if (!C.card.root) {
        shell_card_open(&C.card);
        shell_card_full(&C.card);
        ink_ui_overlay_add(&OVERLAY, C.card.root);
    }
    shell_card_text(&C.card, C.now.title, C.now.body);
    bool call = C.now.kind == INK_INT_CALL;
    bool alarm = C.now.kind == INK_INT_ALARM;
    lv_obj_set_y(C.card.title, ink_ui_px(call ? INK_LINE_BODY + INK_BLOCK : INK_LINE_BODY + INK_BLOCK_L + INK_CIRCLE_LARGE + INK_MARGIN));
    lv_obj_set_y(C.card.body, ink_ui_px(INK_LINE_BODY + INK_BLOCK_L + INK_CIRCLE_LARGE + INK_MARGIN + INK_LINE_TITLE));
    lv_obj_set_style_text_font(C.card.body, ink_ui_font_px(alarm ? INK_FONT_CLOCK : INK_FONT_BODY, INK_WEIGHT_REGULAR), 0);
    if (C.icon) lv_obj_delete(C.icon);
    C.icon = ink_ui_circle(C.card.panel, call ? "person" : alarm ? "clock" : "battery",
                  (INK_CANVAS_W - INK_CIRCLE_LARGE) / 2, call ? INK_LINE_BODY + INK_BLOCK + INK_LINE_HEADING + INK_MARGIN : INK_LINE_BODY + INK_BLOCK_L,
                  INK_CIRCLE_LARGE, INK_ICON_STATE, call ? INK_SURFACE_STATIC : INK_SURFACE_BLACK, NULL, NULL);
    shell_card_clear_buttons(&C.card);
    for (int i = 0; i < C.now.nactions; i++) shell_card_button(&C.card, C.now.actions[i].label, on_button, (void *)(intptr_t)i);
    if (alarm && C.now.nactions == 2) {
        lv_obj_set_flex_flow(C.card.row, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_row(C.card.row, ink_ui_px(INK_GAP), 0);
        lv_obj_set_height(C.card.row, ink_ui_px(2 * INK_PRIMARY + INK_GAP));
        lv_obj_set_y(C.card.row, ink_ui_px(INK_CANVAS_H - INK_MARGIN - INK_LINE_NOTE - INK_GAP - 2 * INK_PRIMARY - INK_GAP));
        for (int i = 0; i < 2; i++) {
            lv_obj_t *b = lv_obj_get_child(C.card.row, i);
            lv_obj_set_width(b, ink_ui_px(INK_CANVAS_W - 2 * INK_MARGIN));
            lv_obj_set_flex_grow(b, 0);
            lv_obj_set_style_bg_color(b, i ? ink_ui_ink() : ink_ui_paper(), 0);
            lv_obj_set_style_border_width(b, i ? 0 : ink_ui_px(INK_STROKE), 0);
            lv_obj_set_style_text_color(lv_obj_get_child(b, 0), i ? ink_ui_paper() : ink_ui_ink(), 0);
        }
    } else {
        lv_obj_set_flex_flow(C.card.row, LV_FLEX_FLOW_ROW);
        lv_obj_set_height(C.card.row, ink_ui_px(INK_PRIMARY));
        lv_obj_set_y(C.card.row, ink_ui_px(INK_CANVAS_H - INK_MARGIN - INK_PRIMARY));
    }
    ink_ui_cause(top->raised_us, "interrupt.to_visible_ms");
}

int ink_shell_interrupt_card_init(void) {
    static const ink_int_presenter_t P = {.show = show, .exec = INK_EXEC_UI};
    ink_interrupt_set_presenter(&P);
    return 0;
}
