/* Only a person's answer after reading the whole transaction can accept it. */
/* ui-scale-runtime: dimension C.head_h,C.preview_h,notice_y,pager_y Card geometry: theme sizes and text measured at the current size. */
/* ui-scale-runtime: dimension C.expired_h,C.expired_body_y Expired card geometry follows measured text at the current size. */
#include "ink/sys/i18n.h"
#include <stdio.h>
#include <string.h>

#include "ink/osal.h"
#include "ink/sys/confirm.h"
#include "ink/sys/exec.h"
#include "ink/sys/power.h"
#include "ink/sys/state.h"
#include "ink/sys/ui_semantics.h"
#include "pinpad.h"
#include "member_pages.h"
#include "shell_card.h"
#include "ink/ui_shell.h"

enum { CONFIRM_CONTEXT_CAP = 384 };
static struct {
    shell_card_t card;
    ink_confirm_view_t now;
    lv_obj_t *pad, *pager, *accept, *notice, *preview, *pin_dots, *context;
    int head_h, preview_h, expired_h, expired_body_y;
    lv_timer_t *read_timer;
    char context_text[CONFIRM_CONTEXT_CAP];
    uint16_t starts[161]; /* At most one page per detail byte, plus the end. */
    int page, pages;
    bool drawn, read, watching;
    bool expired;
    bool pin_present;
    uint32_t version, revision, pressed_revision;
    ink_member_identity_t owner;
    int pin_result;
    uint32_t pin_wait;
    ink_exec_job_t told_job;
} C INK_PSRAM_BSS;

static void told_job(void *arg);
static void close_card(void);
static void on_answer(lv_event_t *e);
static ink_ui_page_spec_t model(void);
static void changed(void) { C.revision++; C.version = ink_ui_content_version(); }
static void pressed(lv_event_t *e) { (void)e; C.version = ink_ui_content_version(); C.pressed_revision = C.revision; }
static void pin_changed(lv_event_t *e) {
    (void)e;
    if (!C.pin_dots) return;
    bool present = !strncmp(lv_label_get_text(C.pin_dots), "●", strlen("●"));
    if (present != C.pin_present) { C.pin_present = present; changed(); }
}

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
    if (C.now.parent) return TR("输入密码");
    if (!strcmp(C.now.cap, "call.dial")) return TR("打电话");
    if (!strcmp(C.now.cap, "settings.set")) return TR("修改设置");
    if (!strcmp(C.now.cap, "sms.send") || !strcmp(C.now.cap, "sms.compose")) return TR("发送");
    if (!strcmp(C.now.cap, "restore.apply")) return TR("恢复资料");
    if (!strcmp(C.now.cap, "system.factory_reset")) return TR("清空设备");
    if (!strcmp(C.now.cap, "usb.mount")) return TR("连接电脑");
    if (!strcmp(C.now.cap, "hub.confirm") && (strstr(C.now.title, "花钱") || strstr(C.now.title, "支付"))) return TR("支付");
    if (strstr(C.now.title, "填入")) return TR("允许填入");
    return cap && cap->effect == INK_FX_LOCAL_DESTRUCTIVE ? TR("删除") : TR("继续操作");
}

static const char *title_text(void) {
    if (!strcmp(C.now.cap, "settings.set")) return TR("修改这项设置？");
    return C.now.parent ? TR("需要大人确认") : answer_label();
}

static const char *notice_text(void) {
    if (!strncmp(C.now.cap, "member.time_", 12) && !ink_member_any_pin()) return TR(INK_PARENT_PIN_MISSING);
    if (C.page + 1 != C.pages) return TR("请翻完说明，再决定是否继续。");
    if (C.now.parent) return TR("需要大人的密码确认。");
    if (!strcmp(C.now.cap, "call.dial")) return TR("不确认就不会打。");
    if (!strcmp(C.now.cap, "settings.set")) return TR("不确认就不会改。");
    if (!strcmp(C.now.cap, "hub.confirm")) return TR("不确认就不会继续。");
    return TR("只有你确认后才会继续。");
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
    if (C.pin_wait) snprintf(s, sizeof s, TR("请等 %u 秒再试"), (unsigned)C.pin_wait);
    else snprintf(s, sizeof s, TR("密码不对"));
    shell_pinpad_clear();
    shell_pinpad_say(s);
    C.pin_present = false; changed();
}

static void pin_entered(const char *pin) {
    ink_confirm_accept_with_pin(C.now.token, pin, pin_told, NULL);
    shell_pinpad_clear();
    shell_pinpad_say(TR("正在核对…"));
    C.pin_present = false; changed();
}

/* Keep the existing password input and verification; add the transaction above it. */
static void parent_page(void) {
    C.pad = shell_pinpad(C.card.root, pin_entered);
    C.pin_dots = lv_obj_get_child(C.pad, 1);
    C.pin_present = false;
    for (uint32_t i = 2; i < lv_obj_get_child_count(C.pad); i++) {
        lv_obj_add_event_cb(lv_obj_get_child(C.pad, (int)i), pin_changed, LV_EVENT_CLICKED, NULL);
        lv_obj_add_event_cb(lv_obj_get_child(C.pad, (int)i), pressed, LV_EVENT_PRESSED, NULL);
    }
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
    lv_obj_t *cancel = ink_ui_pill(C.pad, TR("取消"), INK_MARGIN, y0 - ink_ui_line_height(INK_FONT_LIST) - 2 * INK_GAP - INK_CONTROL,
                w, INK_CONTROL, false, on_answer, NULL);
    lv_obj_add_event_cb(cancel, pressed, LV_EVENT_PRESSED, NULL);
    ink_ui_hint_obj(C.card.root, INK_REFRESH_FLASH_UI);
    changed();
}

static void on_answer(lv_event_t *e) {
    if (!ink_ui_touch_current(C.version) || C.pressed_revision != C.revision) return; /* the card changed under the finger */
    bool yes = lv_event_get_user_data(e) != NULL;
    if (yes && (C.page + 1 != C.pages || !C.read)) return;
    if (yes && C.now.parent) { /* a yes here takes a parent */
        if (!C.pad) parent_page();
        return;
    }
    ink_ui_page_spec_t page = model();
    ink_ui_action_target_t target = {C.version, page.object};
    ink_receipt_t rc;
    ink_ui_semantics_action(&page, yes ? "accept" : "reject", &target, INK_SRC_TOUCH, &rc);
}

static ink_ui_page_spec_t model(void) {
    enum { FIELD_COUNT = 8, ACTION_COUNT = 6, DELTA_COUNT = 3 };
    static char identity[128], reading[96], args[DELTA_COUNT][80], choices[96];
    static ink_ui_field_spec_t fields[FIELD_COUNT] INK_PSRAM_BSS;
    static ink_ui_action_spec_t actions[ACTION_COUNT] INK_PSRAM_BSS;
    int64_t pending = 0;
    ink_state_get_int(ink_state_find("confirm.pending"), &pending);
    snprintf(identity, sizeof identity, TR("页面=%s；家人=%s；确认编号=%u"), page_name(), C.owner.member, (unsigned)C.now.token);
    bool ready = !C.expired && !C.pad && C.read && C.page + 1 == C.pages;
    snprintf(reading, sizeof reading, TR("阅读=%d/%d；排队=%lld；可确认=%d"), C.page + 1, C.pages, (long long)pending, ready);
    snprintf(choices, sizeof choices, TR("%s、取消"), answer_label());
    fields[0] = (ink_ui_field_spec_t){.id = "transaction", .role = "status", .label = TR("确认编号"), .value = identity};
    fields[1] = (ink_ui_field_spec_t){.id = "title", .role = "text", .label = TR("标题"), .value = C.expired ? TR("这件事没有做") : title_text()};
    fields[2] = (ink_ui_field_spec_t){.id = "detail", .role = "text", .label = TR("说明"), .value = C.expired ? lv_label_get_text(C.card.body) : C.now.detail};
    fields[3] = (ink_ui_field_spec_t){.id = "notice", .role = "status", .label = TR("提示"), .value = C.pad ? TR("需要大人在设备上输入密码；密码内容不可见") : C.expired ? TR("已经过期，请重新发起") : notice_text()};
    fields[4] = (ink_ui_field_spec_t){.id = "reading", .role = "status", .label = TR("阅读"), .value = reading};
    fields[5] = (ink_ui_field_spec_t){.id = "buttons", .role = "button", .label = TR("本人操作"), .value = C.expired ? TR("关闭") : C.pad ? TR("取消") : choices};
    fields[6] = (ink_ui_field_spec_t){.id = "context", .role = "text", .label = TR("说明"), .value = C.expired ? TR("已经过期，请重新发起") : C.context_text};
    fields[7] = (ink_ui_field_spec_t){.id = "pin", .role = "textbox", .label = TR("密码"), .value = C.pin_present ? "1" : "", .secret = true};
    for (int i = 0; i < DELTA_COUNT; i++) snprintf(args[i], sizeof args[i], "{\"token\":%u,\"delta\":%d}", (unsigned)C.now.token, i - 1);
    actions[0] = (ink_ui_action_spec_t){.id = "accept", .label = answer_label(), .capability = "confirm.accept", .args_json = args[1], .enabled = ready && !C.now.parent};
    actions[1] = (ink_ui_action_spec_t){.id = "reject", .label = TR("取消"), .capability = "confirm.reject", .args_json = args[1], .enabled = !C.expired};
    actions[2] = (ink_ui_action_spec_t){.id = "withdraw", .label = TR("撤回"), .capability = "confirm.withdraw", .args_json = args[1], .enabled = !C.expired};
    actions[3] = (ink_ui_action_spec_t){.id = "prev", .label = TR("上一页"), .capability = "confirm.page", .args_json = args[0], .enabled = !C.expired && !C.pad && C.read && C.page > 0};
    actions[4] = (ink_ui_action_spec_t){.id = "next", .label = TR("下一页"), .capability = "confirm.page", .args_json = args[2], .enabled = !C.expired && !C.pad && C.read && C.page + 1 < C.pages};
    actions[5] = (ink_ui_action_spec_t){.id = "close", .label = TR("关闭"), .capability = "confirm.page", .args_json = args[1], .enabled = C.expired};
    return (ink_ui_page_spec_t){C.expired ? "ConfirmExpired" : "confirm", page_name(), {"confirm", C.revision}, fields, C.pad ? FIELD_COUNT : FIELD_COUNT - 1, actions, ACTION_COUNT};
}
static int semantic_snapshot(char *buf, size_t cap) { ink_ui_page_spec_t page = model(); return ink_ui_semantics_snapshot(&page, buf, cap); }

static bool on_key(ink_key_t key, bool long_press) {
    if ((key == INK_KEY_BACK || key == INK_KEY_HOME) && !long_press) {
        if (C.expired) close_card();
        else ink_confirm_decide(C.now.token, false, INK_SRC_KEY); /* leaving is a no */
    }
    return true;
}

static const ink_ui_overlay_t OVERLAY = {.name = "confirm", .level = 1, .on_key = on_key, .snapshot = semantic_snapshot};
static const ink_ui_overlay_t EXPIRED = {.name = "ConfirmExpired", .level = 1, .on_key = on_key, .snapshot = semantic_snapshot};

static void close_card(void) {
    if (!C.card.root) return;
    if (C.read_timer) lv_timer_delete(C.read_timer);
    C.read_timer = NULL;
    shell_card_close(&C.card);
    C.pad = C.pager = C.accept = C.notice = C.preview = C.pin_dots = C.context = NULL;
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
    (void)ctx; (void)h;
    if (!strcmp(id, "confirm.pending") && C.card.root) changed();
    if (C.expired && hidden()) close_card();
}

static void draw_page(void);
static void page_drawn(lv_event_t *e) { (void)e; if (C.card.root) C.drawn = true; }
static void read_ready(lv_timer_t *timer) {
    if (!C.drawn || ink_display_busy()) return;
    if (!C.read) { C.read = true; changed(); }
    lv_timer_pause(timer);
    if (C.page + 1 == C.pages) lv_obj_remove_state(C.accept, LV_STATE_DISABLED);
}
static void read_pressed(lv_event_t *e) {
    if (!C.read && C.drawn && !ink_display_busy()) { C.read = true; changed(); }
    pressed(e); /* after: the press's own change is not a change under the finger */
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
    ink_ui_page_spec_t page = model();
    ink_ui_action_target_t target = {C.version, page.object};
    ink_receipt_t rc;
    ink_ui_semantics_action(&page, (intptr_t)lv_event_get_user_data(e) < 0 ? "prev" : "next", &target, INK_SRC_TOUCH, &rc);
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
    lv_obj_set_y(C.context, ink_ui_px(C.head_h));
    C.head_h += ink_ui_text_height(C.context_text, INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_CANVAS_W - 2 * INK_MARGIN) + INK_GAP;
    C.preview_h = words_h + 2 * INK_GAP;
    lv_obj_set_height(C.card.title, ink_ui_px(title_h));
    lv_obj_set_y(C.preview, ink_ui_px(C.head_h));
    lv_obj_set_height(C.preview, ink_ui_px(C.preview_h));
    lv_obj_set_height(C.card.body, ink_ui_px(words_h));
}
static int notice_y(void) { return C.head_h + C.preview_h + INK_GAP_S; }
static int pager_y(void) { return notice_y() + ink_ui_text_height(notice_text(), INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_CANVAS_W - 2 * INK_MARGIN) + INK_GAP_S; }

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
    else snprintf(text, sizeof text, TR("说明无法完整显示，请取消。"));
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
    changed();
}

static void show(const ink_confirm_view_t *top) { /* UI task */
    if (!top) {
        if (!C.expired || hidden()) close_card();
        return;
    }
    if (C.expired) close_card();
    bool unchanged = C.now.token == top->token && C.now.parent == top->parent && C.now.src == top->src &&
        C.now.expires_us == top->expires_us && !strcmp(C.now.cap, top->cap) &&
        !strcmp(C.now.title, top->title) && !strcmp(C.now.detail, top->detail);
    if (C.card.root && unchanged) return;
    C.now = *top;
    ink_member_identity(&C.owner);
    if (!unchanged) {
        uint64_t now = osal_now_us(), seconds = top->expires_us > now ? (top->expires_us - now + 999999) / 1000000 : 0;
        const char *source = top->src == INK_SRC_AI ? TR("AI") : top->src == INK_SRC_REMOTE ? TR("家庭服务器") : top->src == INK_SRC_APP ? TR("应用") : TR("本人操作");
        snprintf(C.context_text, sizeof C.context_text, TR("以%s的身份 · 来源：%s\n首次显示后最多 %llu 秒有效，过期不执行。\n%s"), ink_member_current()->name, source, (unsigned long long)seconds, top->title);
    }
    if (C.pad) {
        lv_obj_delete(C.pad);
        C.pad = C.pin_dots = NULL;
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
        C.context = ink_ui_label(C.card.panel, "", INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_MARGIN, 0, INK_CANVAS_W - 2 * INK_MARGIN);
    }
    lv_label_set_text(C.context, C.context_text);
    fit_to_words();
    shell_card_text(&C.card, title_text(), C.now.detail);
    shell_card_clear_buttons(&C.card);
    const ink_cap_t *cap = ink_cap_find(C.now.cap);
    bool destructive = cap && (cap->effect == INK_FX_LOCAL_DESTRUCTIVE || !strcmp(C.now.cap, "system.factory_reset"));
    C.accept = ink_ui_pill(C.card.row, answer_label(), 0, 0, INK_PRIMARY, INK_PRIMARY, !destructive, on_answer, (void *)1);
    lv_obj_set_flex_grow(C.accept, 1);
    lv_obj_t *cancel = ink_ui_pill(C.card.row, TR("取消"), 0, 0, INK_PRIMARY, INK_PRIMARY, destructive, on_answer, NULL);
    lv_obj_set_flex_grow(cancel, 1);
    lv_obj_add_event_cb(C.accept, read_pressed, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(cancel, pressed, LV_EVENT_PRESSED, NULL);
    lv_obj_set_flex_flow(C.card.row, LV_FLEX_FLOW_ROW_REVERSE);
    paginate();
    draw_page();
}

static void expired_close(lv_event_t *e) {
    (void)e;
    ink_ui_page_spec_t page = model();
    ink_ui_action_target_t target = {C.version, page.object};
    ink_receipt_t rc;
    ink_ui_semantics_action(&page, "close", &target, INK_SRC_TOUCH, &rc);
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
    const char *text = TR("已经过期，没有执行。请重新发起。");
    int title_h = ink_ui_text_height(TR("这件事没有做"), INK_FONT_SUBTITLE, INK_WEIGHT_BOLD, w);
    int body_h = ink_ui_text_height(text, INK_FONT_NOTE, INK_WEIGHT_REGULAR, w);
    C.expired_body_y = INK_MARGIN + title_h + INK_GAP_L;
    C.expired_h = C.expired_body_y + body_h + INK_GAP_L + INK_PRIMARY + INK_MARGIN;
    lv_obj_set_y(C.card.panel, ink_ui_px(INK_CANVAS_H - C.expired_h));
    lv_obj_set_height(C.card.panel, ink_ui_px(C.expired_h + INK_RADIUS_PANEL));
    lv_obj_set_y(C.card.body, ink_ui_px(C.expired_body_y)); lv_obj_set_height(C.card.body, ink_ui_px(body_h));
    lv_obj_set_style_text_font(C.card.body, ink_ui_font_px(INK_FONT_NOTE, INK_WEIGHT_REGULAR), 0);
    lv_obj_set_y(C.card.row, ink_ui_px(C.expired_h - INK_PRIMARY - INK_MARGIN));
    shell_card_text(&C.card, TR("这件事没有做"), text);
    shell_card_button(&C.card, TR("关闭"), expired_close, NULL);
    lv_obj_add_event_cb(lv_obj_get_child(C.card.row, 0), pressed, LV_EVENT_PRESSED, NULL);
    changed();
}

int ink_shell_confirm_card_init(void) {
    static const ink_cap_t PAGE = {.id = "confirm.page", .title = "翻阅确认说明（只由人在设备上操作）", .effect = INK_FX_NAVIGATE, .child_ok = true, .expose = INK_EXPOSE_OWNER,
        .input = "{\"type\":\"object\",\"properties\":{\"token\":{\"type\":\"integer\",\"minimum\":1},\"delta\":{\"type\":\"integer\",\"minimum\":-1,\"maximum\":1}},\"required\":[\"token\",\"delta\"]}", .run = page_action};
    if (ink_cap_register(&PAGE, 1) != 0) return -1;
    if (ink_cap_subscribe("confirm.result", result, NULL, INK_EXEC_UI) != 0) return -1;
    if (ink_state_watch("member.", state_changed, NULL, INK_EXEC_UI) != 0 || ink_state_watch("power.state", state_changed, NULL, INK_EXEC_UI) != 0 ||
        ink_state_watch("interrupt.active", state_changed, NULL, INK_EXEC_UI) != 0 || ink_state_watch("confirm.pending", state_changed, NULL, INK_EXEC_UI) != 0) return -1;
    static const ink_confirm_presenter_t P = {.show = show, .exec = INK_EXEC_UI};
    ink_confirm_set_presenter(&P);
    return 0;
}
