/* Only a person's answer after reading the whole transaction can accept it. */
/* ui-scale-runtime: dimension C.panel_y,C.panel_h,C.row_y Saved LVGL geometry from the theme card, plus theme paging space. */
#include <stdio.h>
#include <string.h>

#include "ink/osal.h"
#include "ink/sys/confirm.h"
#include "ink/sys/exec.h"
#include "pinpad.h"
#include "shell_card.h"
#include "ink/ui_shell.h"

static struct {
    shell_card_t card;
    ink_confirm_view_t now;
    lv_obj_t *pad, *pager, *accept, *notice;
    lv_timer_t *read_timer;
    uint16_t starts[161]; /* At most one page per detail byte, plus the end. */
    int page, pages;
    int panel_y, panel_h, row_y;
    bool drawn, read, watching;
    int pin_result;
    uint32_t pin_wait;
    ink_exec_job_t told_job;
} C INK_PSRAM_BSS;

static void told_job(void *arg);

static void pin_told(void *ctx, int result, uint32_t wait_s) { /* exec.bg */
    (void)ctx;
    __atomic_store_n(&C.pin_result, result, __ATOMIC_RELEASE);
    C.pin_wait = wait_s;
    ink_exec_post(INK_EXEC_UI, &C.told_job, told_job, NULL);
}

static void told_job(void *arg) { /* UI task */
    (void)arg;
    int r = __atomic_load_n(&C.pin_result, __ATOMIC_ACQUIRE);
    if (r == INK_PIN_OK || !C.pad) return; /* the card goes by itself */
    char s[48];
    if (C.pin_wait) snprintf(s, sizeof s, "请等 %u 秒再试", (unsigned)C.pin_wait);
    else snprintf(s, sizeof s, "密码不对");
    shell_pinpad_clear();
    shell_pinpad_say(s);
}

static void pin_entered(const char *pin) {
    ink_confirm_accept_with_pin(C.now.token, pin, pin_told, NULL);
    shell_pinpad_clear();
    shell_pinpad_say("正在核对…");
}

static void on_answer(lv_event_t *e) {
    bool yes = lv_event_get_user_data(e) != NULL;
    if (yes && (C.page + 1 != C.pages || !C.read)) return;
    if (yes && C.now.parent) { /* a yes here takes a parent */
        if (!C.pad) C.pad = shell_pinpad(C.card.root, pin_entered);
        return;
    }
    ink_confirm_decide(C.now.token, yes, INK_SRC_TOUCH);
}

static int snapshot(char *buf, size_t cap) {
    int n = snprintf(buf, cap, "等你确认：%s。%s。阅读=%d/%d；可确认=%d；只能由你确认或取消。动作：confirm.withdraw。",
                     C.now.title, C.now.detail, C.page + 1, C.pages, C.read && C.page + 1 == C.pages);
    return n < 0 ? 0 : (size_t)n < cap ? n : (int)cap - 1;
}

static bool on_key(ink_key_t key, bool long_press) {
    if ((key == INK_KEY_BACK || key == INK_KEY_HOME) && !long_press) ink_confirm_decide(C.now.token, false, INK_SRC_KEY); /* leaving is a no: not every device has a back key */
    return true;
}

static const ink_ui_overlay_t OVERLAY = {.name = "confirm", .level = 1, .on_key = on_key, .snapshot = snapshot};

static void draw_page(void);
static void page_drawn(lv_event_t *e) { (void)e; if (C.card.root) C.drawn = true; }
static void read_ready(lv_timer_t *timer) {
    if (!C.drawn || ink_display_busy()) return;
    C.read = true;
    lv_timer_pause(timer);
    if (C.page + 1 == C.pages) lv_obj_remove_state(C.accept, LV_STATE_DISABLED);
}
static void read_pressed(lv_event_t *e) {
    (void)e;
    if (C.drawn && !ink_display_busy()) C.read = true;
}
static void page_action(ink_call_t *call) {
    int64_t token = 0, delta = 0;
    ink_args_get_int(&call->args, "token", &token); ink_args_get_int(&call->args, "delta", &delta);
    if (!C.card.root || C.pad || !C.read || token != C.now.token || strcmp(ink_ui_top_name(), "confirm")) { call->rc.status = INK_RC_STALE; return; }
    int page = C.page + (int)delta;
    if (page < 0 || page >= C.pages) { call->rc.status = INK_RC_INVALID; return; }
    C.page = page;
    draw_page();
}
static void page_clicked(lv_event_t *e) {
    char args[80];
    snprintf(args, sizeof args, "{\"token\":%u,\"delta\":%d}", (unsigned)C.now.token, (int)(intptr_t)lv_event_get_user_data(e));
    ink_receipt_t rc; ink_cap_invoke_json("confirm.page", args, INK_SRC_TOUCH, 0, &rc);
}

static void paginate(void) {
    lv_obj_update_layout(C.card.body);
    int w = lv_obj_get_width(C.card.body), h = lv_obj_get_height(C.card.body);
    const lv_font_t *font = lv_obj_get_style_text_font(C.card.body, LV_PART_MAIN);
    int letter = lv_obj_get_style_text_letter_space(C.card.body, LV_PART_MAIN);
    int line = lv_obj_get_style_text_line_space(C.card.body, LV_PART_MAIN);
    size_t at = 0, len = strlen(C.now.detail);
    C.page = C.pages = 0;
    C.starts[0] = 0;
    do {
        size_t end = at;
        while (end < len) {
            size_t next = end + 1;
            while (next < len && (C.now.detail[next] & 0xc0) == 0x80) next++;
            char text[sizeof C.now.detail];
            snprintf(text, sizeof text, "%.*s", (int)(next - at), C.now.detail + at);
            lv_point_t size;
            lv_text_get_size(&size, text, font, letter, line, w, LV_TEXT_FLAG_NONE);
            if (size.y > h) break;
            end = next;
        }
        if (end == at && at < len) { /* No glyph can fit: acceptance stays disabled. */
            C.pages = 0;
            return;
        }
        at = end;
        C.starts[++C.pages] = (uint16_t)at;
    } while (at < len);
}

static void draw_page(void) {
    int extra = C.pages > 1 ? ink_ui_px(INK_GAP_XXL) : 0;
    lv_obj_set_y(C.card.panel, C.panel_y - extra);
    lv_obj_set_height(C.card.panel, C.panel_h + extra);
    lv_obj_set_y(C.card.row, C.row_y + extra);
    char text[sizeof C.now.detail];
    if (C.pages) snprintf(text, sizeof text, "%.*s", C.starts[C.page + 1] - C.starts[C.page], C.now.detail + C.starts[C.page]);
    else snprintf(text, sizeof text, "说明无法完整显示，请取消。");
    C.drawn = C.read = false;
    lv_label_set_text(C.card.body, text);
    lv_obj_add_state(C.accept, LV_STATE_DISABLED);
    lv_label_set_text(C.notice, C.page + 1 == C.pages ? "只有你确认后才会继续。" : "请翻完说明，再决定是否继续。");
    if (C.pager) lv_obj_delete(C.pager);
    C.pager = C.pages > 1 ? ink_ui_pagebar(C.card.panel, C.page + 1, C.pages,
        INK_MARGIN + INK_CIRCLE + 2 * INK_GAP_L + INK_ROW_DOUBLE + 2 * INK_LINE_NOTE, page_clicked) : NULL;
    if (C.pager) {
        lv_obj_add_event_cb(lv_obj_get_child(C.pager, 0), read_pressed, LV_EVENT_PRESSED, NULL);
        lv_obj_add_event_cb(lv_obj_get_child(C.pager, 2), read_pressed, LV_EVENT_PRESSED, NULL);
    }
    ink_ui_hint_obj(C.card.panel, INK_REFRESH_FLASH_UI);
    lv_timer_resume(C.read_timer);
    lv_timer_reset(C.read_timer);
}

static void show(const ink_confirm_view_t *top) { /* UI task */
    if (!top) {
        if (!C.card.root) return;
        lv_timer_delete(C.read_timer);
        C.read_timer = NULL;
        shell_card_close(&C.card);
        C.pad = C.pager = C.accept = C.notice = NULL;
        ink_ui_overlay_remove(&OVERLAY);
        return;
    }
    if (C.card.root && C.now.token == top->token && C.now.parent == top->parent && C.now.src == top->src &&
        C.now.expires_us == top->expires_us && !strcmp(C.now.cap, top->cap) &&
        !strcmp(C.now.title, top->title) && !strcmp(C.now.detail, top->detail)) return;
    C.now = *top;
    if (C.pad) {
        lv_obj_delete(C.pad);
        C.pad = NULL;
    }
    if (!C.card.root) {
        shell_card_open(&C.card);
        lv_obj_update_layout(C.card.root);
        C.panel_y = lv_obj_get_y(C.card.panel); C.panel_h = lv_obj_get_height(C.card.panel); C.row_y = lv_obj_get_y(C.card.row);
        /* Poll presentation only while this page is pending, then pause. */
        C.read_timer = lv_timer_create(read_ready, 20, NULL);
        if (!C.watching) {
            lv_display_add_event_cb(lv_obj_get_display(C.card.root), page_drawn, LV_EVENT_REFR_READY, NULL);
            C.watching = true;
        }
        ink_ui_overlay_add(&OVERLAY, C.card.root);
        ink_ui_circle(C.card.panel, "ai", INK_MARGIN, INK_MARGIN, INK_CIRCLE, INK_ICON_LIST, INK_SURFACE_BLACK, NULL, NULL);
        lv_obj_set_pos(C.card.title, ink_ui_px(INK_MARGIN + INK_CIRCLE + INK_GAP), ink_ui_px(INK_MARGIN));
        lv_obj_set_width(C.card.title, ink_ui_px(INK_CANVAS_W - 2 * INK_MARGIN - INK_CIRCLE - INK_GAP));
        lv_obj_t *preview = ink_ui_box(C.card.panel, INK_MARGIN, INK_MARGIN + INK_CIRCLE + INK_GAP_L,
                                      INK_CANVAS_W - 2 * INK_MARGIN, INK_ROW_DOUBLE + INK_LINE_NOTE, INK_RADIUS_KEY, INK_SURFACE_STATIC, false);
        lv_obj_set_parent(C.card.body, preview);
        lv_obj_set_pos(C.card.body, ink_ui_px(INK_MARGIN), ink_ui_px(INK_GAP_L));
        lv_obj_set_size(C.card.body, ink_ui_px(INK_CANVAS_W - 4 * INK_MARGIN), ink_ui_px(INK_PRIMARY));
        C.notice = ink_ui_notice(C.card.panel, "", INK_MARGIN + INK_CIRCLE + INK_GAP_L + INK_ROW_DOUBLE + INK_LINE_NOTE + INK_GAP_L);
    }
    shell_card_text(&C.card, C.now.title, C.now.detail);
    shell_card_clear_buttons(&C.card);
    const ink_cap_t *cap = ink_cap_find(C.now.cap);
    shell_card_button(&C.card, cap && cap->effect == INK_FX_LOCAL_DESTRUCTIVE ? "删除" : "确认", on_answer, (void *)1);
    C.accept = lv_obj_get_child(C.card.row, 0);
    lv_obj_add_event_cb(C.accept, read_pressed, LV_EVENT_PRESSED, NULL);
    shell_card_button(&C.card, "取消", on_answer, NULL);
    lv_obj_set_flex_flow(C.card.row, LV_FLEX_FLOW_ROW_REVERSE);
    paginate();
    draw_page();
}

int ink_shell_confirm_card_init(void) {
    static const ink_cap_t PAGE = {.id = "confirm.page", .title = "翻阅确认说明（只由人在设备上操作）", .effect = INK_FX_NAVIGATE, .child_ok = true, .expose = INK_EXPOSE_OWNER,
        .input = "{\"type\":\"object\",\"properties\":{\"token\":{\"type\":\"integer\",\"minimum\":1},\"delta\":{\"type\":\"integer\",\"minimum\":-1,\"maximum\":1}},\"required\":[\"token\",\"delta\"]}", .run = page_action};
    if (ink_cap_register(&PAGE, 1) != 0) return -1;
    static const ink_confirm_presenter_t P = {.show = show, .exec = INK_EXEC_UI};
    ink_confirm_set_presenter(&P);
    return 0;
}
