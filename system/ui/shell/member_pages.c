/* PIN material stays in this local input surface, outside capability arguments. */
/* ui-scale-runtime: dimension y Text positions are derived from the theme. */
/* ui-scale-runtime: font font Centered labels use theme font tokens. */
/* ui-scale-runtime: dimension x,y,w Buttons receive theme-derived positions and widths. */
#include <stdio.h>
#include <string.h>
#include "ink/osal.h"
#include "ink/sys/exec.h"
#include "ink/sys/members.h"
#include "ink/sys/power.h"
#include "ink/sys/state.h"
#include "ink/ui_shell.h"
#include "shell_card.h"
#include "member_pages.h"

enum { SWITCH, UNLOCK, OLD, NEW, FORGOT, PARENT };
static struct {
    shell_card_t card;
    const ink_ui_overlay_t *overlay;
    lv_obj_t *dots, *message;
    char id[16], name[32], pin[9], old[9], status[160];
    int mode, result;
    uint32_t wait_s;
    uint32_t epoch;
    uint64_t retry_us;
    bool busy;
    lv_timer_t *wait_timer;
    ink_exec_job_t result_job;
} P;
static void draw(void);
static void erase(char *p, size_t n) { volatile char *v = p; while (n--) *v++ = 0; }
static uint32_t remaining(void) {
    uint64_t now = osal_now_us();
    return P.retry_us > now ? (uint32_t)((P.retry_us - now + 999999) / 1000000) : 0;
}
static int snapshot(char *out, size_t cap) {
    const char *actions = !ink_member_space_open() ? "请本人在设备上操作。" :
        P.mode == SWITCH ? "动作：shell.pin_cancel、shell.member_choose。" :
        P.mode == UNLOCK ? "动作：shell.pin_cancel、shell.pin_forgot。" :
        P.mode == FORGOT ? "动作：shell.pin_cancel、shell.pin_parent。" : "动作：shell.pin_cancel。";
    return snprintf(out, cap, "%s；家人：%s；%s；还需等待 %u 秒；密码内容不可见。%s",
        P.overlay->name, P.name, P.status, (unsigned)remaining(), actions);
}
static void close_page(void) {
    if (P.wait_timer) { lv_timer_delete(P.wait_timer); P.wait_timer = NULL; }
    if (P.card.root) { ink_ui_overlay_remove(P.overlay); shell_card_close(&P.card); }
    P.dots = P.message = NULL;
    erase(P.pin, sizeof P.pin); erase(P.old, sizeof P.old);
}
static bool current(void) {
    if (P.card.root && (P.epoch != ink_member_epoch() || ink_power_state() == INK_PWR_LOCKED)) close_page();
    return P.card.root != NULL;
}
static void changed(void *ctx, int h, const char *id) {
    (void)ctx; (void)h; (void)id; current();
}
static void wait_done(lv_timer_t *timer) {
    (void)timer; P.wait_timer = NULL;
    if (!current()) return;
    P.retry_us = 0;
    snprintf(P.status, sizeof P.status, "%s", P.mode == OLD || P.mode == NEW ? "四到八位数字" : P.mode == PARENT ? "请家长本人输入" : "输入密码");
    draw();
}
static bool key(ink_key_t k, bool lng) {
    if (!lng && !P.busy && (k == INK_KEY_HOME || k == INK_KEY_BACK)) close_page();
    return true;
}
static const ink_ui_overlay_t VIEWS[] = {
    {.name = "MemberSwitch", .level = 2, .snapshot = snapshot, .on_key = key},
    {.name = "member_gate", .level = 2, .snapshot = snapshot, .on_key = key},
    {.name = "PinSet", .level = 2, .snapshot = snapshot, .on_key = key},
    {.name = "PinForgot", .level = 2, .snapshot = snapshot, .on_key = key},
    {.name = "PinWait", .level = 2, .snapshot = snapshot, .on_key = key}
};
static void center(const char *text, int font, int y, bool bold) {
    lv_obj_t *l = ink_ui_label(P.card.panel, text, font, bold ? INK_WEIGHT_BOLD : INK_WEIGHT_REGULAR,
        INK_MARGIN, y, INK_CANVAS_W - 2 * INK_MARGIN);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
}
static void clicked(lv_event_t *e) {
    ink_receipt_t rc; ink_cap_invoke(lv_event_get_user_data(e), NULL, INK_SRC_TOUCH, 0, &rc);
}
static void button(const char *text, const char *cap, int x, int y, int w, bool primary) {
    ink_ui_pill(P.card.panel, text, x, y, w, INK_CONTROL, primary, clicked, (void *)cap);
}
static void dots(void) {
    char text[32] = "";
    for (size_t i = 0; i < strlen(P.pin); i++) strcat(text, "●");
    lv_label_set_text(P.dots, text[0] ? text : "○ ○ ○ ○");
    ink_ui_hint_obj(P.dots, INK_REFRESH_FAST);
}
static void digit(lv_event_t *e) {
    if (!current() || P.busy || remaining()) return;
    int d = (int)(intptr_t)lv_event_get_user_data(e);
    size_t n = strlen(P.pin);
    if (d == 10) { if (n) P.pin[n - 1] = 0; }
    else if (n < sizeof P.pin - 1) P.pin[n] = (char)('0' + d);
    dots();
}
static void draw(void) {
    if (!P.card.root) P.epoch = ink_member_epoch();
    if (P.wait_timer) { lv_timer_delete(P.wait_timer); P.wait_timer = NULL; }
    if (P.card.root) { ink_ui_overlay_remove(P.overlay); shell_card_close(&P.card); }
    P.overlay = &VIEWS[remaining() ? 4 : P.mode == SWITCH ? 0 : P.mode == UNLOCK ? 1 : P.mode == FORGOT || P.mode == PARENT ? 3 : 2];
    shell_card_open(&P.card);
    if (P.mode != SWITCH) shell_card_full(&P.card);
    lv_obj_clean(P.card.panel); P.dots = P.message = NULL;
    ink_ui_overlay_add(P.overlay, P.card.root);
    int w = INK_CANVAS_W - 2 * INK_MARGIN, half = (w - INK_GAP) / 2;
    if (P.mode == SWITCH) {
        ink_ui_label(P.card.panel, "换人之前，先看看", INK_FONT_SUBTITLE, INK_WEIGHT_BOLD, INK_MARGIN, INK_MARGIN, w);
        ink_ui_label(P.card.panel, "换人后显示对方的内容。\n正在播放的声音会停下。\n你的页面和 AI 对话不会给别人看。", INK_FONT_LIST, INK_WEIGHT_REGULAR,
            INK_MARGIN, INK_MARGIN + INK_CONTROL + INK_GAP, w);
        int y = ink_ui_height() - lv_obj_get_y(P.card.panel) / ink_ui_px(1) - INK_MARGIN - INK_CONTROL;
        button("取消", "shell.pin_cancel", INK_MARGIN, y, half, false);
        button("换人", "shell.member_choose", INK_MARGIN + half + INK_GAP, y, half, true);
        return;
    }
    lv_obj_remove_flag(ink_shell_statusbar(P.card.panel, true), LV_OBJ_FLAG_CLICKABLE);
    if (P.mode == FORGOT) {
        ink_ui_circle(P.card.panel, "back", INK_MARGIN, INK_STATUS_BAR, INK_CONTROL, INK_ICON_MEDIUM, INK_SURFACE_STATIC, clicked, "shell.pin_cancel");
        ink_ui_label(P.card.panel, "忘记密码", INK_FONT_TITLE, INK_WEIGHT_BOLD, INK_MARGIN + INK_CONTROL + INK_GAP, INK_STATUS_BAR + INK_GAP, w - INK_CONTROL - INK_GAP);
        ink_ui_circle(P.card.panel, "lock", (INK_CANVAS_W - INK_CIRCLE_LARGE) / 2, INK_CANVAS_H / 2 - INK_CIRCLE_LARGE,
            INK_CIRCLE_LARGE, INK_ICON_STATE, INK_SURFACE_STATIC, NULL, NULL);
        center("请家长验证", INK_FONT_SUBTITLE, INK_CANVAS_H / 2 + INK_GAP_L, true);
        center(P.status, INK_FONT_NOTE, INK_CANVAS_H / 2 + INK_CONTROL, false);
        button("返回", "shell.pin_cancel", INK_MARGIN, INK_CANVAS_H - INK_MARGIN - INK_CONTROL, half, false);
        button("家长验证", "shell.pin_parent", INK_MARGIN + half + INK_GAP, INK_CANVAS_H - INK_MARGIN - INK_CONTROL, half, true);
        return;
    }
    int diameter = ink_ui_line_height(INK_FONT_SUBTITLE) > INK_LINE_SUBTITLE ? INK_CIRCLE : INK_CIRCLE_APP;
    lv_obj_t *avatar = ink_ui_pill(P.card.panel, "", (INK_CANVAS_W - diameter) / 2, INK_STATUS_BAR,
        diameter, diameter, true, NULL, NULL);
    char first[8] = ""; size_t len = 1;
    while (len < sizeof first - 1 && (P.name[len] & 0xc0) == 0x80) len++;
    memcpy(first, P.name, len); lv_label_set_text(lv_obj_get_child(avatar, 0), P.mode == PARENT ? "家长" : first);
    char title[96]; snprintf(title, sizeof title, "%s%s", P.mode == PARENT ? "请家长输入密码" : P.name,
        P.mode == OLD ? "，输入原密码" : P.mode == NEW ? "，设一个密码" : "");
    int y = INK_STATUS_BAR + diameter + INK_GAP_XS;
    center(title, INK_FONT_SUBTITLE, y, true); y += ink_ui_line_height(INK_FONT_SUBTITLE) + INK_GAP_XS;
    P.dots = ink_ui_label(P.card.panel, "", INK_FONT_LIST, INK_WEIGHT_REGULAR, INK_MARGIN, y, w);
    lv_obj_set_style_text_align(P.dots, LV_TEXT_ALIGN_CENTER, 0); dots();
    y += ink_ui_line_height(INK_FONT_LIST) + INK_GAP_XS;
    char message[192];
    if (remaining()) snprintf(message, sizeof message, "错太多次了，还要等 %u 秒", (unsigned)remaining());
    else snprintf(message, sizeof message, "%s", P.status);
    P.message = ink_ui_label(P.card.panel, message, INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_MARGIN, y, w);
    lv_obj_set_style_text_align(P.message, LV_TEXT_ALIGN_CENTER, 0);
    int y0 = INK_CANVAS_H - INK_MARGIN - 4 * INK_DIGIT - 3 * INK_GAP;
    button(P.mode == UNLOCK ? "忘记密码" : "取消", P.mode == UNLOCK ? "shell.pin_forgot" : "shell.pin_cancel",
        INK_MARGIN, y0 - INK_CONTROL - INK_GAP_XS, half, false);
    button("确定", "shell.pin_submit", INK_MARGIN + half + INK_GAP, y0 - INK_CONTROL - INK_GAP_XS, half, false);
    int x0 = (INK_CANVAS_W - 3 * INK_DIGIT - 2 * INK_GAP) / 2;
    for (int row = 0; row < 4; row++) for (int col = 0; col < 3; col++) {
        int d = row < 3 ? row * 3 + col + 1 : col == 1 ? 0 : 10;
        char label[12]; snprintf(label, sizeof label, "%d", d);
        lv_obj_t *b = ink_ui_pill(P.card.panel, row == 3 && col != 1 ? (col ? "删除" : "取消") : label,
            x0 + col * (INK_DIGIT + INK_GAP), y0 + row * (INK_DIGIT + INK_GAP), INK_DIGIT, INK_DIGIT, false,
            row == 3 && col == 0 ? clicked : digit, row == 3 && col == 0 ? (void *)"shell.pin_cancel" : (void *)(intptr_t)d);
        ink_ui_pill_text(b, row == 3 && col != 1 ? INK_FONT_LIST : INK_FONT_HEADING, INK_WEIGHT_REGULAR);
    }
    ink_ui_capture_touch(P.card.panel);
    uint64_t now = osal_now_us();
    if (P.retry_us > now) {
        P.wait_timer = lv_timer_create(wait_done, (uint32_t)((P.retry_us - now + 999) / 1000) + 1, NULL);
        lv_timer_set_repeat_count(P.wait_timer, 1);
    }
}
static void completed_ui(void *arg) {
    (void)arg; P.busy = false;
    erase(P.pin, sizeof P.pin); erase(P.old, sizeof P.old);
    if (P.result == INK_PIN_OK && P.mode != PARENT) { close_page(); ink_shell_member_gate_done(); return; }
    if (!current()) return;
    P.retry_us = P.wait_s ? osal_now_us() + (uint64_t)P.wait_s * 1000000 : 0;
    if (P.result == INK_PIN_OK) {
        P.mode = FORGOT;
        snprintf(P.status, sizeof P.status, "家长已验证。本机还不能重置密码，请在家庭服务器处理。");
    } else {
        if (P.mode == NEW) P.mode = OLD;
        snprintf(P.status, sizeof P.status, "%s", P.result == INK_PIN_WAIT ? "请稍后再试" : P.result == INK_PIN_BAD_FORM ? "请输入四到八位数字" : "密码不对");
    }
    draw();
}
static void completed(void *ctx, int result, uint32_t wait_s) {
    (void)ctx; P.result = result; P.wait_s = wait_s;
    ink_exec_post(INK_EXEC_UI, &P.result_job, completed_ui, NULL);
}
static void submit(ink_call_t *c) {
    if (!current() || P.busy || P.mode == SWITCH || P.mode == FORGOT) { c->rc.status = INK_RC_REJECTED; return; }
    if (remaining()) { erase(P.pin, sizeof P.pin); draw(); return; }
    if (P.mode == OLD) {
        snprintf(P.old, sizeof P.old, "%s", P.pin); erase(P.pin, sizeof P.pin);
        P.mode = NEW; snprintf(P.status, sizeof P.status, "四到八位数字"); draw(); return;
    }
    if (strlen(P.pin) < 4) { snprintf(P.status, sizeof P.status, "请输入四到八位数字"); draw(); return; }
    int r = P.mode == PARENT ? ink_member_check_parent(P.pin, completed, NULL) :
        P.mode == NEW ? ink_member_set_pin(P.id, P.old, P.pin, completed, NULL) : ink_member_unlock(P.id, P.pin, completed, NULL);
    erase(P.pin, sizeof P.pin); erase(P.old, sizeof P.old);
    P.busy = r == 0;
    snprintf(P.status, sizeof P.status, "%s", P.busy ? "正在核对…" : "暂时没能核对，请重试");
    if (r) c->rc.status = INK_RC_FAILED;
    draw();
}
static void action(ink_call_t *c) {
    if (P.busy) { c->rc.status = INK_RC_REJECTED; return; }
    const char *id = c->cap->id;
    if (!strcmp(id, "shell.pin_set")) {
        if (!ink_member_space_open()) { c->rc.status = INK_RC_REJECTED; return; }
        close_page(); P.mode = ink_member_has_pin(ink_member_current()->id) ? OLD : NEW;
        snprintf(P.id, sizeof P.id, "%s", ink_member_current()->id); snprintf(P.name, sizeof P.name, "%s", ink_member_current()->name);
        P.retry_us = 0; snprintf(P.status, sizeof P.status, "四到八位数字"); draw(); return;
    }
    if (!current()) { c->rc.status = INK_RC_STALE; return; }
    if (!strcmp(id, "shell.pin_cancel")) close_page();
    else if (!strcmp(id, "shell.member_choose") && P.mode == SWITCH) { close_page(); ink_shell_members_open(true); }
    else if (!strcmp(id, "shell.pin_forgot") && P.mode == UNLOCK) {
        erase(P.pin, sizeof P.pin); P.mode = FORGOT; P.retry_us = 0;
        snprintf(P.status, sizeof P.status, "先请家长输入密码。忘记密码后的重置还未接通。"); draw();
    } else if (!strcmp(id, "shell.pin_parent") && P.mode == FORGOT) {
        if (!ink_member_any_pin()) { snprintf(P.status, sizeof P.status, "还没有家长密码，请在家庭服务器处理。"); draw(); return; }
        P.mode = PARENT; snprintf(P.status, sizeof P.status, "请家长本人输入"); draw();
    } else c->rc.status = INK_RC_REJECTED;
}
void ink_shell_member_switch_open(void) {
    if (P.busy) return;
    close_page(); ink_shell_close(); P.mode = SWITCH; P.retry_us = 0;
    snprintf(P.name, sizeof P.name, "%s", ink_member_current()->name); P.status[0] = 0; draw();
}
void ink_shell_member_pin_open(const char *id) {
    if (P.busy) return;
    close_page(); P.mode = UNLOCK; P.retry_us = 0;
    snprintf(P.id, sizeof P.id, "%s", id);
    for (int i = 0; ink_member_at(i); i++) if (!strcmp(ink_member_at(i)->id, id)) snprintf(P.name, sizeof P.name, "%s", ink_member_at(i)->name);
    snprintf(P.status, sizeof P.status, "输入密码"); draw();
}
int ink_member_pages_init(void) {
    static const ink_cap_t caps[] = {
        {.id = "shell.member_choose", .title = "选择下一位家人", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = action},
        {.id = "shell.pin_set", .title = "打开设置或修改密码", .effect = INK_FX_NAVIGATE, .run = action},
        {.id = "shell.pin_forgot", .title = "忘记密码时请家长帮助", .effect = INK_FX_NAVIGATE, .child_ok = true, .while_locked = true, .run = action},
        {.id = "shell.pin_parent", .title = "请家长在设备上输入密码", .effect = INK_FX_NAVIGATE, .child_ok = true, .while_locked = true, .run = action},
        {.id = "shell.pin_cancel", .title = "收起密码页面", .effect = INK_FX_NAVIGATE, .child_ok = true, .while_locked = true, .run = action},
        {.id = "shell.pin_submit", .title = "本人提交设备上的密码", .expose = INK_EXPOSE_OWNER, .effect = INK_FX_NAVIGATE,
            .child_ok = true, .while_locked = true, .run = submit}
    };
    return ink_cap_register(caps, sizeof caps / sizeof *caps) ||
        ink_state_watch("member.", changed, NULL, INK_EXEC_UI) ||
        ink_state_watch("power.state", changed, NULL, INK_EXEC_UI) ? -1 : 0;
}
