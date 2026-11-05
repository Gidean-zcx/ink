/*
 * The interruption card: the system's placeholder for what ink_interrupt_set_presenter shows (a call, an alarm, a
 * nearly empty battery). It covers the page, also of a locked device. The real look comes with the system UI (D-64);
 * nothing about the mechanism is here.
 */
#include <stdio.h>
#include <string.h>

#include "ink/sys/cap.h"
#include "ink/sys/interrupt.h"
#include "ink/sys/members.h"
#include "shell_card.h"
#include "ink/ui_shell.h"

static struct {
    shell_card_t card;
    ink_interrupt_t now;
    lv_obj_t *icon, *date, *notice;
    char clock[16], date_text[96];
} C;

static void on_button(lv_event_t *e) {
    int index = (int)(intptr_t)lv_event_get_user_data(e);
    ink_interrupt_action(C.now.id, index, INK_SRC_TOUCH);
}

static int snapshot(char *buf, size_t cap) {
    if (!ink_member_space_open()) return snprintf(buf, cap, "有系统打断；大人验证前 AI 无权读取个人内容。");
    int n = snprintf(buf, cap, "%s：%s。", C.now.title, C.now.body);
    if (C.now.kind == INK_INT_ALARM && n >= 0 && (size_t)n < cap)
        n += snprintf(buf + n, cap - (size_t)n, "时间=%s；日期=%s；", C.clock, C.date_text);
    for (int i = 0; i < C.now.nactions && n >= 0 && (size_t)n < cap; i++) n += snprintf(buf + n, cap - (size_t)n, "按钮：%s。", C.now.actions[i].label);
    return n < 0 ? 0 : (size_t)n < cap ? n : (int)cap - 1;
}

static const ink_ui_overlay_t OVERLAY = {.name = "interrupt", .level = 3, .snapshot = snapshot}; /* the keys do nothing: only the power key does (in the input service) */

static void show(const ink_interrupt_t *top) { /* UI task */
    if (!top) {
        if (!C.card.root) return;
        shell_card_close(&C.card);
        C.icon = C.date = C.notice = NULL;
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
    bool call = C.now.kind == INK_INT_CALL;
    bool alarm = C.now.kind == INK_INT_ALARM;
    bool alarm_actions = alarm && C.now.nactions == 2 && C.now.actions[0].cap && C.now.actions[1].cap &&
                         !strcmp(C.now.actions[0].cap, "alarm.dismiss") && !strcmp(C.now.actions[1].cap, "alarm.snooze");
    if (C.date) { lv_obj_delete(C.date); C.date = NULL; }
    if (C.notice) { lv_obj_delete(C.notice); C.notice = NULL; }
    if (alarm) ink_shell_time_strings(C.clock, sizeof C.clock, C.date_text, sizeof C.date_text);
    shell_card_text(&C.card, alarm ? C.now.body : C.now.title, alarm ? C.clock : C.now.body);
    lv_obj_set_style_text_font(C.card.title, ink_ui_font_px(call ? INK_FONT_LIST : alarm ? INK_FONT_TITLE : INK_FONT_HEADING, INK_WEIGHT_BOLD), 0);
    lv_obj_set_y(C.card.title, ink_ui_px(call ? INK_LINE_BODY + INK_BLOCK : INK_LINE_BODY + INK_BLOCK_L + INK_CIRCLE_LARGE + INK_MARGIN));
    lv_obj_set_y(C.card.body, ink_ui_px(INK_LINE_BODY + INK_BLOCK_L + INK_CIRCLE_LARGE + INK_MARGIN + INK_LINE_TITLE));
    lv_obj_set_style_text_font(C.card.body, ink_ui_font_px(alarm ? INK_FONT_NUMBER : INK_FONT_BODY, INK_WEIGHT_REGULAR), 0);
    lv_obj_set_height(C.card.body, ink_ui_px(alarm ? INK_LINE_NUMBER : 2 * INK_ROW_DOUBLE));
    if (alarm) {
        int y = INK_LINE_BODY + INK_BLOCK_L + INK_CIRCLE_LARGE + INK_MARGIN + INK_LINE_TITLE + INK_LINE_NUMBER + INK_GAP_L;
        C.date = ink_ui_label(C.card.panel, C.date_text, INK_FONT_LIST, INK_WEIGHT_MEDIUM, INK_MARGIN, y, INK_CANVAS_W - 2 * INK_MARGIN);
        lv_obj_set_style_text_align(C.date, LV_TEXT_ALIGN_CENTER, 0);
        C.notice = ink_ui_notice(C.card.panel, "按电源键再睡 10 分钟", INK_CANVAS_H - INK_MARGIN - INK_LINE_NOTE);
    }
    if (C.icon) lv_obj_delete(C.icon);
    C.icon = ink_ui_circle(C.card.panel, call ? "person" : alarm ? "clock" : "battery",
                  (INK_CANVAS_W - INK_CIRCLE_LARGE) / 2, call ? INK_LINE_BODY + INK_BLOCK + INK_LINE_HEADING + INK_MARGIN : INK_LINE_BODY + INK_BLOCK_L,
                  INK_CIRCLE_LARGE, INK_ICON_STATE, call ? INK_SURFACE_STATIC : INK_SURFACE_BLACK, NULL, NULL);
    shell_card_clear_buttons(&C.card);
    for (int i = 0; i < C.now.nactions; i++) {
        int action = alarm_actions ? 1 - i : i;
        shell_card_button(&C.card, C.now.actions[action].label, on_button, (void *)(intptr_t)action);
    }
    if (call && C.now.nactions == 2) {
        int h = INK_CIRCLE_LARGE + INK_GAP_M + INK_LINE_LIST;
        lv_obj_set_flex_flow(C.card.row, LV_FLEX_FLOW_ROW_REVERSE);
        lv_obj_set_height(C.card.row, ink_ui_px(h));
        lv_obj_set_y(C.card.row, ink_ui_px(INK_CANVAS_H - INK_MARGIN - h));
        for (int i = 0; i < 2; i++) {
            lv_obj_t *button = lv_obj_get_child(C.card.row, i);
            lv_obj_set_height(button, ink_ui_px(h));
            lv_obj_set_style_bg_opa(button, LV_OPA_TRANSP, 0);
            lv_obj_set_style_border_width(button, 0, 0);
            lv_obj_set_style_text_color(button, ink_ui_ink(), 0);
            lv_label_set_text(lv_obj_get_child(button, 0), "");
            int cell = (INK_CANVAS_W - 2 * INK_MARGIN - INK_GAP) / 2;
            ink_ui_circle(button, i ? "callEnd" : "phone", (cell - INK_CIRCLE_LARGE) / 2 - INK_GAP, 0,
                           INK_CIRCLE_LARGE, INK_ICON_APP, i ? INK_SURFACE_WHITE : INK_SURFACE_BLACK, NULL, NULL);
            const char *label = i && C.now.actions[i].cap && !strcmp(C.now.actions[i].cap, "call.hangup") ? "拒接" : C.now.actions[i].label;
            lv_obj_t *caption = ink_ui_label(button, label, INK_FONT_LIST, INK_WEIGHT_MEDIUM,
                                             0, INK_CIRCLE_LARGE + INK_GAP_M, cell - 2 * INK_GAP);
            lv_obj_set_style_text_align(caption, LV_TEXT_ALIGN_CENTER, 0);
        }
    } else if (alarm && C.now.nactions == 2) {
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
