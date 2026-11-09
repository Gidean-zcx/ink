/* Only a person's answer after reading the whole transaction can accept it. */
/* ui-scale-runtime: dimension C.head_h,C.preview_h,notice_y,pager_y Card geometry: theme sizes and text measured at the current size. */
/* ui-scale-runtime: dimension C.expired_h,C.expired_body_y Expired card geometry follows measured text at the current size. */
#include <stdio.h>
#include <string.h>

#include "ink/osal.h"
#include "ink/sys/confirm.h"
#include "ink/sys/exec.h"
#include "ink/sys/power.h"
#include "ink/sys/state.h"
#include "pinpad.h"
#include "shell_card.h"
#include "ink/ui_shell.h"

static struct {
    shell_card_t card;
    ink_confirm_view_t now;
    lv_obj_t *pad, *pager, *accept, *notice, *preview;
    int head_h, preview_h, expired_h, expired_body_y;
    lv_timer_t *read_timer;
    uint16_t starts[161]; /* At most one page per detail byte, plus the end. */
    int page, pages;
    bool drawn, read, watching;
    bool expired;
    ink_member_identity_t owner;
    int pin_result;
    uint32_t pin_wait;
    ink_exec_job_t told_job;
} C INK_PSRAM_BSS;

static void told_job(void *arg);
static void close_card(void);
static void on_answer(lv_event_t *e);

static const char *page_name(void) {
    if (C.expired) return "ConfirmExpired";
    if (C.pad || C.now.parent) return "ConfirmParent";
    if (!strcmp(C.now.cap, "call.dial")) return "ConfirmCall";
    if (!strcmp(C.now.cap, "settings.set")) return "ConfirmSetting";
    if (!strcmp(C.now.cap, "hub.confirm")) return "ConfirmWeb";
    return C.pages > 1 ? "StressConfirm" : "Confirm";
}

static const char *answer_label(void) {
    const ink_cap_t *cap = ink_cap_find(C.now.cap);
    if (C.now.parent) return "输入密码";
    if (!strcmp(C.now.cap, "call.dial")) return "打电话";
    if (!strcmp(C.now.cap, "settings.set")) return "确认修改";
    if (!strcmp(C.now.cap, "sms.send") || !strcmp(C.now.cap, "sms.compose")) return "确认发送";
    if (!strcmp(C.now.cap, "hub.confirm") && (strstr(C.now.title, "花钱") || strstr(C.now.title, "支付"))) return "确认支付";
    return cap && cap->effect == INK_FX_LOCAL_DESTRUCTIVE ? "删除" : "确认";
}

static const char *title_text(void) {
    if (!strcmp(C.now.cap, "call.dial")) return "这个电话不是你要打的";
    if (!strcmp(C.now.cap, "settings.set")) return "修改这项设置？";
    return C.now.title;
}

static const char *notice_text(void) {
    if (C.page + 1 != C.pages) return "请翻完说明，再决定是否继续。";
    if (C.now.parent) return "需要大人的密码确认。";
    if (!strcmp(C.now.cap, "call.dial")) return "不确认就不会打。";
    if (!strcmp(C.now.cap, "settings.set")) return "1 分钟内有效。不确认就不会改。";
    if (!strcmp(C.now.cap, "hub.confirm")) return "不确认就不会继续。";
    return "只有你确认后才会继续。";
}

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

/* Keep the existing password input and verification; add the transaction above it. */
static void parent_page(void) {
    C.pad = shell_pinpad(C.card.root, pin_entered);
    int y0 = INK_CANVAS_H - INK_READING_MARGIN - 4 * INK_DIGIT - 3 * INK_GAP;
    int w = INK_CANVAS_W - 2 * INK_MARGIN;
    int title_h = ink_ui_text_height(C.now.title, INK_FONT_SUBTITLE, INK_WEIGHT_BOLD, w);
    int body_h = ink_ui_text_height(C.now.detail, INK_FONT_NOTE, INK_WEIGHT_REGULAR, w);
    int y = y0 - ink_ui_line_height(INK_FONT_LIST) - 4 * INK_GAP - INK_CONTROL - body_h - title_h - INK_MARGIN;
    if (y < 0) y = 0;
    lv_obj_set_style_bg_opa(C.pad, LV_OPA_TRANSP, 0);
    lv_obj_add_flag(lv_obj_get_child(C.pad, 0), LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_y(lv_obj_get_child(C.pad, 1), ink_ui_px(y0 - ink_ui_line_height(INK_FONT_LIST) - INK_GAP));
    lv_obj_t *panel = ink_ui_panel(C.pad, INK_CANVAS_H - y, true);
    lv_obj_move_background(panel);
    ink_ui_label(panel, C.now.title, INK_FONT_SUBTITLE, INK_WEIGHT_BOLD, INK_MARGIN, INK_MARGIN, w);
    ink_ui_label(panel, C.now.detail, INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_MARGIN, INK_MARGIN + title_h + INK_GAP, w);
    ink_ui_pill(C.pad, "取消", INK_MARGIN, y0 - ink_ui_line_height(INK_FONT_LIST) - 2 * INK_GAP - INK_CONTROL,
                w, INK_CONTROL, false, on_answer, NULL);
    ink_ui_hint_obj(C.card.root, INK_REFRESH_FLASH_UI);
}

static void on_answer(lv_event_t *e) {
    bool yes = lv_event_get_user_data(e) != NULL;
    if (yes && (C.page + 1 != C.pages || !C.read)) return;
    if (yes && C.now.parent) { /* a yes here takes a parent */
        if (!C.pad) parent_page();
        return;
    }
    ink_confirm_decide(C.now.token, yes, INK_SRC_TOUCH);
}

static int snapshot(char *buf, size_t cap) {
    int64_t pending = 0;
    ink_state_get_int(ink_state_find("confirm.pending"), &pending);
    int n = snprintf(buf, cap, "页面=%s；家人=%s；确认编号=%u；%s：%s。%s。阅读=%d/%d；排队=%lld；可确认=%d；%s。动作：%s。",
                     page_name(), C.owner.member, (unsigned)C.now.token, C.expired ? "这件事没有做" : "等你确认", title_text(), C.now.detail,
                     C.page + 1, C.pages, (long long)pending, !C.expired && !C.pad && C.read && C.page + 1 == C.pages,
                     C.pad ? "需要大人在设备上输入密码；密码内容不可见" : C.expired ? "已经过期，请重新发起" : "只能由你确认或取消",
                     C.expired ? "confirm.page（delta=0，关闭说明，只由人在设备上操作）" : "confirm.withdraw");
    if (n < 0 || (size_t)n >= cap) { if (cap) buf[0] = 0; return -1; }
    return n;
}

static bool on_key(ink_key_t key, bool long_press) {
    if ((key == INK_KEY_BACK || key == INK_KEY_HOME) && !long_press) {
        if (C.expired) close_card();
        else ink_confirm_decide(C.now.token, false, INK_SRC_KEY); /* leaving is a no */
    }
    return true;
}

static const ink_ui_overlay_t OVERLAY = {.name = "confirm", .level = 1, .on_key = on_key, .snapshot = snapshot};
static const ink_ui_overlay_t EXPIRED = {.name = "ConfirmExpired", .level = 1, .on_key = on_key, .snapshot = snapshot};

static void close_card(void) {
    if (!C.card.root) return;
    if (C.read_timer) lv_timer_delete(C.read_timer);
    C.read_timer = NULL;
    shell_card_close(&C.card);
    C.pad = C.pager = C.accept = C.notice = C.preview = NULL;
    ink_ui_overlay_remove(&OVERLAY);
    ink_ui_overlay_remove(&EXPIRED);
    C.expired = false;
}

static bool hidden(void) {
    char active[16] = "";
    ink_state_get_str(ink_state_find("interrupt.active"), active, sizeof active);
    return !ink_member_space_open() || strcmp(C.owner.member, ink_member_current()->id) || C.owner.epoch != ink_member_epoch() ||
        ink_power_state() == INK_PWR_LOCKED || (active[0] && strcmp(active, "none"));
}

static void state_changed(void *ctx, int h, const char *id) { /* UI task */
    (void)ctx; (void)h; (void)id;
    if (C.expired && hidden()) close_card();
}

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
    if (C.expired && token == C.now.token && delta == 0) { close_card(); return; }
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

/* From the top of the card: icon and title, the words being confirmed, a line of notice, the page
 * turner. The card is as tall as what it has to say, up to three lines of words a page. */
static void fit_to_words(void) {
    int title_w = INK_CANVAS_W - 2 * INK_MARGIN - INK_CIRCLE_SMALL - INK_GAP;
    int title_h = ink_ui_text_height(title_text(), INK_FONT_SUBTITLE, INK_WEIGHT_BOLD, title_w);
    if (title_h > 2 * ink_ui_line_height(INK_FONT_SUBTITLE)) title_h = 2 * ink_ui_line_height(INK_FONT_SUBTITLE);
    int line = ink_ui_line_height(INK_FONT_BODY);
    int words_h = ink_ui_text_height(C.now.detail, INK_FONT_BODY, INK_WEIGHT_REGULAR, INK_CANVAS_W - 4 * INK_MARGIN);
    if (words_h > 3 * line) words_h = 3 * line;
    if (words_h < line) words_h = line;
    C.head_h = INK_MARGIN + (title_h > INK_CIRCLE_SMALL ? title_h : INK_CIRCLE_SMALL) + INK_GAP;
    C.preview_h = words_h + 2 * INK_GAP;
    lv_obj_set_height(C.card.title, ink_ui_px(title_h));
    lv_obj_set_y(C.preview, ink_ui_px(C.head_h));
    lv_obj_set_height(C.preview, ink_ui_px(C.preview_h));
    lv_obj_set_height(C.card.body, ink_ui_px(words_h));
}
static int notice_y(void) { return C.head_h + C.preview_h + INK_GAP_S; }
static int pager_y(void) { return notice_y() + ink_ui_line_height(INK_FONT_NOTE) + INK_GAP_S; }

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
    int height = pager_y() + (C.pages > 1 ? INK_CONTROL + INK_GAP_S : 0) + INK_PRIMARY + INK_GAP_L;
    lv_obj_set_y(C.card.panel, ink_ui_px(INK_CANVAS_H - height));
    lv_obj_set_height(C.card.panel, ink_ui_px(height + INK_RADIUS_PANEL));
    lv_obj_set_y(C.card.row, ink_ui_px(height - INK_PRIMARY - INK_GAP_L));
    lv_obj_set_y(C.notice, ink_ui_px(notice_y()));
    char text[sizeof C.now.detail];
    if (C.pages) snprintf(text, sizeof text, "%.*s", C.starts[C.page + 1] - C.starts[C.page], C.now.detail + C.starts[C.page]);
    else snprintf(text, sizeof text, "说明无法完整显示，请取消。");
    C.drawn = C.read = false;
    lv_label_set_text(C.card.body, text);
    lv_obj_add_state(C.accept, LV_STATE_DISABLED);
    lv_label_set_text(C.notice, notice_text());
    if (C.pager) lv_obj_delete(C.pager);
    C.pager = C.pages > 1 ? ink_ui_pagebar(C.card.panel, C.page + 1, C.pages, pager_y(), page_clicked) : NULL;
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
        if (!C.expired || hidden()) close_card();
        return;
    }
    if (C.expired) close_card();
    if (C.card.root && C.now.token == top->token && C.now.parent == top->parent && C.now.src == top->src &&
        C.now.expires_us == top->expires_us && !strcmp(C.now.cap, top->cap) &&
        !strcmp(C.now.title, top->title) && !strcmp(C.now.detail, top->detail)) return;
    C.now = *top;
    ink_member_identity(&C.owner);
    if (C.pad) {
        lv_obj_delete(C.pad);
        C.pad = NULL;
    }
    if (!C.card.root) {
        shell_card_open(&C.card);
        /* Poll presentation only while this page is pending, then pause. */
        C.read_timer = lv_timer_create(read_ready, 20, NULL);
        if (!C.watching) {
            lv_display_add_event_cb(lv_obj_get_display(C.card.root), page_drawn, LV_EVENT_REFR_READY, NULL);
            C.watching = true;
        }
        ink_ui_overlay_add(&OVERLAY, C.card.root);
        ink_ui_circle(C.card.panel, "ai", INK_MARGIN, INK_MARGIN, INK_CIRCLE_SMALL, INK_ICON_MEDIUM, INK_SURFACE_BLACK, NULL, NULL);
        lv_obj_set_pos(C.card.title, ink_ui_px(INK_MARGIN + INK_CIRCLE_SMALL + INK_GAP), ink_ui_px(INK_MARGIN));
        lv_obj_set_width(C.card.title, ink_ui_px(INK_CANVAS_W - 2 * INK_MARGIN - INK_CIRCLE_SMALL - INK_GAP));
        lv_label_set_long_mode(C.card.title, LV_LABEL_LONG_WRAP);
        C.preview = ink_ui_box(C.card.panel, INK_MARGIN, 0, INK_CANVAS_W - 2 * INK_MARGIN, 0, INK_RADIUS_KEY, INK_SURFACE_STATIC, false);
        lv_obj_set_parent(C.card.body, C.preview);
        lv_obj_set_pos(C.card.body, ink_ui_px(INK_MARGIN), ink_ui_px(INK_GAP));
        lv_obj_set_width(C.card.body, ink_ui_px(INK_CANVAS_W - 4 * INK_MARGIN));
        C.notice = ink_ui_notice(C.card.panel, "", 0);
    }
    fit_to_words();
    shell_card_text(&C.card, title_text(), C.now.detail);
    shell_card_clear_buttons(&C.card);
    shell_card_button(&C.card, answer_label(), on_answer, (void *)1);
    C.accept = lv_obj_get_child(C.card.row, 0);
    lv_obj_add_event_cb(C.accept, read_pressed, LV_EVENT_PRESSED, NULL);
    shell_card_button(&C.card, "取消", on_answer, NULL);
    lv_obj_set_flex_flow(C.card.row, LV_FLEX_FLOW_ROW_REVERSE);
    paginate();
    draw_page();
}

static void expired_close(lv_event_t *e) {
    (void)e;
    char args[80];
    snprintf(args, sizeof args, "{\"token\":%u,\"delta\":0}", (unsigned)C.now.token);
    ink_receipt_t rc; ink_cap_invoke_json("confirm.page", args, INK_SRC_TOUCH, 0, &rc);
}

static void result(void *ctx, const char *id, const ink_args_t *args) {
    (void)ctx; (void)id;
    int64_t token = 0;
    ink_confirm_view_t waiting;
    char decision[24], member[sizeof C.owner.member];
    ink_args_get_int(args, "token", &token);
    ink_args_get_str(args, "decision", decision, sizeof decision);
    ink_args_get_str(args, "member", member, sizeof member);
    if (token != C.now.token || strcmp(decision, "expired") || strcmp(member, C.owner.member) || ink_confirm_list(&waiting, 1) || hidden()) return;
    close_card(); C.expired = true;
    shell_card_open(&C.card);
    ink_ui_overlay_add(&EXPIRED, C.card.root);
    int w = INK_CANVAS_W - 2 * INK_MARGIN;
    const char *text = "已经过期，没有执行。请重新发起。";
    int title_h = ink_ui_text_height("这件事没有做", INK_FONT_SUBTITLE, INK_WEIGHT_BOLD, w);
    int body_h = ink_ui_text_height(text, INK_FONT_NOTE, INK_WEIGHT_REGULAR, w);
    C.expired_body_y = INK_MARGIN + title_h + INK_GAP_L;
    C.expired_h = C.expired_body_y + body_h + INK_GAP_L + INK_PRIMARY + INK_MARGIN;
    lv_obj_set_y(C.card.panel, ink_ui_px(INK_CANVAS_H - C.expired_h));
    lv_obj_set_height(C.card.panel, ink_ui_px(C.expired_h + INK_RADIUS_PANEL));
    lv_obj_set_y(C.card.body, ink_ui_px(C.expired_body_y)); lv_obj_set_height(C.card.body, ink_ui_px(body_h));
    lv_obj_set_style_text_font(C.card.body, ink_ui_font_px(INK_FONT_NOTE, INK_WEIGHT_REGULAR), 0);
    lv_obj_set_y(C.card.row, ink_ui_px(C.expired_h - INK_PRIMARY - INK_MARGIN));
    shell_card_text(&C.card, "这件事没有做", text);
    shell_card_button(&C.card, "关闭", expired_close, NULL);
}

int ink_shell_confirm_card_init(void) {
    static const ink_cap_t PAGE = {.id = "confirm.page", .title = "翻阅确认说明（只由人在设备上操作）", .effect = INK_FX_NAVIGATE, .child_ok = true, .expose = INK_EXPOSE_OWNER,
        .input = "{\"type\":\"object\",\"properties\":{\"token\":{\"type\":\"integer\",\"minimum\":1},\"delta\":{\"type\":\"integer\",\"minimum\":-1,\"maximum\":1}},\"required\":[\"token\",\"delta\"]}", .run = page_action};
    if (ink_cap_register(&PAGE, 1) != 0) return -1;
    if (ink_cap_subscribe("confirm.result", result, NULL, INK_EXEC_UI) != 0) return -1;
    if (ink_state_watch("member.", state_changed, NULL, INK_EXEC_UI) != 0 || ink_state_watch("power.state", state_changed, NULL, INK_EXEC_UI) != 0 ||
        ink_state_watch("interrupt.active", state_changed, NULL, INK_EXEC_UI) != 0) return -1;
    static const ink_confirm_presenter_t P = {.show = show, .exec = INK_EXEC_UI};
    ink_confirm_set_presenter(&P);
    return 0;
}
