/*
 * The interruption card: the system's placeholder for what ink_interrupt_set_presenter shows (a call, an alarm, a
 * nearly empty battery). It covers the page, also of a locked device. The real look comes with the system UI (D-64);
 * nothing about the mechanism is here.
 */
#include "ink/sys/i18n.h"
#include <stdio.h>
#include <string.h>

#include "ink/sys/cap.h"
#include "ink/osal.h"
#include "ink/sys/interrupt.h"
#include "ink/sys/members.h"
#include "ink/sys/ui_semantics.h"
#include "shell_card.h"
#include "ink/ui_shell.h"

static struct {
    shell_card_t card;
    ink_interrupt_t now;
    lv_obj_t *icon, *date, *notice, *context;
    char clock[16], date_text[96];
    uint32_t version, revision;
    bool touching;
} C;

static ink_ui_page_spec_t model(void) {
    enum { FIELD_COUNT = 4, ACTION_COUNT = sizeof C.now.actions / sizeof *C.now.actions };
    static ink_ui_field_spec_t fields[FIELD_COUNT + ACTION_COUNT];
    static ink_ui_action_spec_t actions[ACTION_COUNT];
    static const char *const ids[] = {"0", "1", "2"};
    _Static_assert(sizeof ids / sizeof *ids == ACTION_COUNT, "interrupt action ids");
    static char time[144];
    snprintf(time, sizeof time, TR("时间=%s；日期=%s；"), C.clock, C.date_text);
    fields[0] = (ink_ui_field_spec_t){.id = "title", .role = "text", .label = TR("标题"), .value = C.now.title};
    fields[1] = (ink_ui_field_spec_t){.id = "body", .role = "text", .label = TR("内容"), .value = C.now.body};
    fields[2] = (ink_ui_field_spec_t){.id = "return", .role = "status", .label = TR("返回"), .value = TR("结束后回到刚才的页面")};
    fields[3] = (ink_ui_field_spec_t){.id = "time", .role = "text", .label = TR("时间和日期"), .value = time};
    size_t count = C.now.kind == INK_INT_ALARM ? FIELD_COUNT : FIELD_COUNT - 1;
    for (int i = 0; i < C.now.nactions; i++) {
        fields[count++] = (ink_ui_field_spec_t){.id = ids[i], .role = "button", .label = TR("按钮"), .value = C.now.actions[i].label};
        actions[i] = (ink_ui_action_spec_t){.id = ids[i], .label = C.now.actions[i].label,
            .capability = C.now.actions[i].cap, .args_json = C.now.actions[i].args, .enabled = C.now.actions[i].cap != NULL};
    }
    return (ink_ui_page_spec_t){"interrupt", C.now.title, {"interrupt", C.revision}, fields,
        count, actions, (size_t)C.now.nactions};
}
static void pressed(lv_event_t *e) { (void)e; C.version = ink_ui_content_version(); C.touching = true; }

static void on_button(lv_event_t *e) {
    int index = (int)(intptr_t)lv_event_get_user_data(e);
    uint32_t version = C.touching ? C.version : ink_ui_content_version();
    C.touching = false;
    if (version != ink_ui_content_version()) return;
    ink_ui_page_spec_t page = model();
    ink_ui_action_target_t target = {version, page.object};
    ink_receipt_t rc;
    if (C.now.actions[index].cap) ink_ui_semantics_action(&page, page.actions[index].id, &target, INK_SRC_TOUCH, &rc);
    else ink_interrupt_action(C.now.id, index, INK_SRC_TOUCH);
}

static int semantic_snapshot(char *buf, size_t cap) {
    ink_ui_page_spec_t page = model();
    ink_ui_field_spec_t locked = {.id = "status", .role = "text", .label = TR("系统打断"), .value = TR("有系统打断；大人验证前 AI 无权读取个人内容。")};
    if (!ink_member_space_open()) { page.title = TR("系统打断"); page.fields = &locked; page.field_count = 1; page.action_count = 0; }
    return ink_ui_semantics_snapshot(&page, buf, cap);
}

static const ink_ui_overlay_t OVERLAY = {.name = "interrupt", .level = 3, .snapshot = semantic_snapshot}; /* the keys do nothing: only the power key does (in the input service) */

static void show(const ink_interrupt_t *top) { /* UI task */
    if (!top) {
        if (!C.card.root) return;
        shell_card_close(&C.card);
        C.icon = C.date = C.notice = C.context = NULL;
        ink_ui_overlay_remove(&OVERLAY);
        return;
    }
    bool same = C.card.root && C.now.id == top->id && C.now.kind == top->kind && C.now.nactions == top->nactions && !strcmp(C.now.title, top->title) && !strcmp(C.now.body, top->body);
    for (int i = 0; same && i < top->nactions; i++) same = !strcmp(C.now.actions[i].label, top->actions[i].label) &&
        C.now.actions[i].cap == top->actions[i].cap && !strcmp(C.now.actions[i].args, top->actions[i].args);
    C.now = *top;
    if (same) return; /* only the sound changed */
    C.touching = false;
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
    if (C.context) { lv_obj_delete(C.context); C.context = NULL; }
    if (alarm) ink_shell_time_strings(C.clock, sizeof C.clock, C.date_text, sizeof C.date_text);
    shell_card_text(&C.card, alarm ? C.now.body : C.now.title, alarm ? C.clock : C.now.body);
    lv_obj_set_style_text_font(C.card.title, ink_ui_font_px(call ? INK_FONT_LIST : alarm ? INK_FONT_TITLE : INK_FONT_HEADING, INK_WEIGHT_BOLD), 0);
    lv_obj_set_y(C.card.title, ink_ui_px(call ? INK_STATUS_BAR + INK_BLOCK : INK_STATUS_BAR + INK_BLOCK_L + INK_CIRCLE_LARGE + INK_MARGIN));
    lv_obj_set_y(C.card.body, ink_ui_px(INK_STATUS_BAR + INK_BLOCK_L + INK_CIRCLE_LARGE + INK_MARGIN + ink_ui_line_height(INK_FONT_TITLE)));
    lv_obj_set_style_text_font(C.card.body, ink_ui_font_px(alarm ? INK_FONT_NUMBER : INK_FONT_BODY, INK_WEIGHT_REGULAR), 0);
    lv_obj_set_height(C.card.body, ink_ui_px(alarm ? ink_ui_line_height(INK_FONT_NUMBER) : 2 * INK_ROW_DOUBLE));
    if (alarm) {
        int y = INK_STATUS_BAR + INK_BLOCK_L + INK_CIRCLE_LARGE + INK_MARGIN + ink_ui_line_height(INK_FONT_TITLE) + ink_ui_line_height(INK_FONT_NUMBER) + INK_GAP_L;
        C.date = ink_ui_label(C.card.panel, C.date_text, INK_FONT_LIST, INK_WEIGHT_MEDIUM, INK_MARGIN, y, INK_CANVAS_W - 2 * INK_MARGIN);
        lv_obj_set_style_text_align(C.date, LV_TEXT_ALIGN_CENTER, 0);
        C.notice = ink_ui_notice(C.card.panel, TR("按电源键再睡 10 分钟"), INK_CANVAS_H - INK_MARGIN - ink_ui_line_height(INK_FONT_NOTE));
    }
    if (C.icon) lv_obj_delete(C.icon);
    C.icon = ink_ui_circle(C.card.panel, call ? "person" : alarm ? "clock" : "battery",
                  (INK_CANVAS_W - INK_CIRCLE_LARGE) / 2, call ? INK_STATUS_BAR + INK_BLOCK + ink_ui_line_height(INK_FONT_HEADING) + INK_MARGIN : INK_STATUS_BAR + INK_BLOCK_L,
                  INK_CIRCLE_LARGE, INK_ICON_STATE, INK_SURFACE_WHITE, NULL, NULL);
    C.context = ink_ui_notice(C.card.panel, TR("结束后回到刚才的页面"),
        INK_STATUS_BAR + INK_GAP_M);
    shell_card_clear_buttons(&C.card);
    for (int i = 0; i < C.now.nactions; i++) {
        int action = alarm_actions ? 1 - i : i;
        shell_card_button(&C.card, C.now.actions[action].label, on_button, (void *)(intptr_t)action);
        lv_obj_add_event_cb(lv_obj_get_child(C.card.row, i), pressed, LV_EVENT_PRESSED, NULL);
    }
    if (call && C.now.nactions == 2) {
        int h = INK_CIRCLE_LARGE + INK_GAP_M + ink_ui_line_height(INK_FONT_LIST);
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
            const char *label = i && C.now.actions[i].cap && !strcmp(C.now.actions[i].cap, "call.hangup") ? TR("拒接") : C.now.actions[i].label;
            lv_obj_t *caption = ink_ui_label(button, label, INK_FONT_LIST, INK_WEIGHT_MEDIUM,
                                             0, INK_CIRCLE_LARGE + INK_GAP_M, cell - 2 * INK_GAP);
            lv_obj_set_style_text_align(caption, LV_TEXT_ALIGN_CENTER, 0);
        }
    } else if (alarm && C.now.nactions == 2) {
        lv_obj_set_flex_flow(C.card.row, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_row(C.card.row, ink_ui_px(INK_GAP), 0);
        lv_obj_set_height(C.card.row, ink_ui_px(2 * INK_PRIMARY + INK_GAP));
        lv_obj_set_y(C.card.row, ink_ui_px(INK_CANVAS_H - INK_MARGIN - ink_ui_line_height(INK_FONT_NOTE) - INK_GAP - 2 * INK_PRIMARY - INK_GAP));
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
    C.revision++; C.version = ink_ui_content_version();
}

int ink_shell_interrupt_card_init(void) {
    static const ink_int_presenter_t P = {.show = show, .exec = INK_EXEC_UI};
    ink_interrupt_set_presenter(&P);
    return 0;
}

void ink_shell_interrupt_present(const ink_interrupt_t *top) { show(top); }
