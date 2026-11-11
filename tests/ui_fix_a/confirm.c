#ifndef INK_UI_FIX_SOURCE
#define INK_UI_FIX_SOURCE "../../system/ui/shell/confirm_card.c"
#endif
#include INK_UI_FIX_SOURCE
#include "common.h"
static ink_confirm_view_t fx_view;
static void fx_show(void) { show(&fx_view); }
static void fx_next(void) {
    ink_call_t c = {0};
    ink_args_int(&c.args, "token", C.now.token); ink_args_int(&c.args, "delta", 1);
    page_action(&c);
    CHECK(c.rc.status == INK_RC_DONE, "person turns the presented confirmation page");
}
static void fx_check_page(void) {
    CHECK(lv_obj_has_state(C.accept, LV_STATE_DISABLED) == (C.page + 1 < C.pages || !C.read), "UX05 only the fully presented final page enables acceptance");
    lv_area_t body, notice, buttons;
    lv_obj_get_coords(C.card.body, &body); lv_obj_get_coords(C.notice, &notice); lv_obj_get_coords(C.card.row, &buttons);
    CHECK(body.y2 < notice.y1 && notice.y2 < buttons.y1, "UX05 Chinese text, notice and buttons never overlap");
    bool context_fits = false;
    for (uint32_t i = 0; i < lv_obj_get_child_count(C.card.panel); i++) {
        lv_obj_t *o = lv_obj_get_child(C.card.panel, (int)i);
        if (!lv_obj_check_type(o, &lv_label_class) || !strstr(lv_label_get_text(o), "的身份")) continue;
        lv_area_t a; lv_obj_get_coords(o, &a); context_fits = a.y1 >= 0 && a.y2 < body.y1;
    }
    CHECK(context_fits, "UX05 identity and actual validity remain fully visible above the original transaction pages");
    CHECK(C.starts[0] == 0 && C.starts[C.pages] >= strlen(C.now.detail), "UX05 pagination retains the entire supplied object and detail");
}
static void fx_secret(void) {
    static const ink_cap_t cap = {.id = "hub.confirm", .title = "允许填入密码", .ask = "example.com 的登录密码：{password}"};
    ink_args_t args = {0}; ink_receipt_t rc = {0};
    ink_args_str(&args, "password", "SYNTH_SECRET_NEVER_DISPLAY");
    CHECK(ink_confirm_hold(&cap, &args, INK_SRC_AI, 0, &rc) == INK_RC_PENDING_CONFIRMATION, "real confirmation service prepares a masked secret");
}
static void fx_expire(void) {
    ink_args_t a = {0};
    ink_args_int(&a, "token", C.now.token); ink_args_str(&a, "decision", "expired");
    ink_args_str(&a, "member", C.owner.member); result(NULL, "confirm.result", &a);
}
static void fx_scenario(const char *cap, const char *title, const char *detail, bool parent, const char *frame) {
    on_ui(close_card);
    fx_view = (ink_confirm_view_t){.token = 4243, .src = INK_SRC_REMOTE, .parent = parent, .expires_us = osal_now_us() + 17000000};
    snprintf(fx_view.cap, sizeof fx_view.cap, "%s", cap); snprintf(fx_view.title, sizeof fx_view.title, "%s", title);
    snprintf(fx_view.detail, sizeof fx_view.detail, "%s", detail);
    on_ui(fx_show); fx_capture(frame);
    CHECK(strstr(g_snap, detail), "UX05 supplied object and consequences are kept verbatim: %s", frame);
    while (C.page + 1 < C.pages) { CHECK(WAIT_FOR(C.read, 3000), "read scenario page"); on_ui(fx_next); fx_quiet(); }
    CHECK(WAIT_FOR(C.read, 3000), "read final scenario page");
}
static void run(void *arg) {
    (void)arg; fx_start();
    fx_view = (ink_confirm_view_t){.token = 4242, .src = INK_SRC_AI, .expires_us = osal_now_us() + 47000000};
    snprintf(fx_view.cap, sizeof fx_view.cap, "sms.send");
    snprintf(fx_view.title, sizeof fx_view.title, "给测试家人发送资料");
    snprintf(fx_view.detail, sizeof fx_view.detail, "收件人：测试家人。内容：这是合成的长中文说明，请逐页核对对象和内容，翻到最后一页才可发送。末尾测试");
    on_ui(fx_show); fx_capture("confirm-first"); on_ui(fx_check_page);
    CHECK(strstr(g_snap, "的身份") && strstr(g_snap, "来源") && strstr(g_snap, "秒有效") && strstr(g_snap, "末尾测试"), "UX05 identity, source, actual validity and full object are in the snapshot");
    CHECK(!strcmp(answer_label(), "发送") && C.pages > 1, "UX05 send has a concrete verb and long text is paginated");
    CHECK(AI("confirm.accept", "{\"token\":4242}") && (g_rc.status == INK_RC_REJECTED || g_rc.status == INK_RC_INVALID), "UX05 AI cannot accept");
    CHECK(AI("confirm.page", "{\"token\":4242,\"delta\":1}") && (g_rc.status == INK_RC_REJECTED || g_rc.status == INK_RC_INVALID), "UX05 AI cannot skip reading");
    while (C.page + 1 < C.pages) {
        CHECK(WAIT_FOR(C.read, 3000), "confirmation page is physically presented");
        on_ui(fx_next); fx_quiet(); on_ui(fx_check_page);
    }
    CHECK(WAIT_FOR(C.read, 3000), "final page is physically presented");
    fx_capture("confirm-last"); on_ui(fx_check_page);
    on_ui(close_card); on_ui(fx_secret); fx_quiet(); fx_capture("secret");
    CHECK(!strstr(g_snap, "SYNTH_SECRET_NEVER_DISPLAY") && strstr(g_snap, "example.com") && !strcmp(answer_label(), "允许填入"), "UX05 service secret stays masked, domain and concrete verb remain visible");
    CHECK(ink_confirm_decide(C.now.token, true, INK_SRC_AI) != 0, "UX05 service refuses AI approval of a real pending secret");
    CHECK(!ink_confirm_decide(C.now.token, false, INK_SRC_TOUCH), "person cancels synthetic secret without using it");
    fx_quiet();
    fx_scenario("file.delete", "删除测试文件", "对象：测试文件，共三项。删除后找不回来。", false, "delete");
    CHECK(!strcmp(answer_label(), "删除"), "UX05 destructive action names deletion");
    fx_scenario("hub.confirm", "支付测试订单", "商户：example.com；测试金额与币种未提供。", false, "web");
    CHECK(!strstr(g_snap, "人民币") && !strstr(g_snap, "元"), "UX05 missing amounts and currency are never invented");
    fx_scenario("settings.set", "修改孩子的规则", "测试孩子：每天使用时间从三十分钟改为四十分钟。", false, "setting");
    CHECK(!strcmp(answer_label(), "修改设置") && !strstr(g_snap, "1 分钟内有效"), "UX05 setting uses supplied values and actual expiry");
    fx_scenario("call.dial", "网页提出拨号", "example.com 提出拨打测试号码；以当前家人的身份拨打。", false, "call");
    CHECK(!strcmp(answer_label(), "打电话"), "UX05 unrequested call names the action");
    fx_scenario("settings.set", "给测试孩子授权", "测试孩子：把今天可用时间从三十分钟改为四十分钟。", true, "parent-review");
    fx_press("输入密码"); fx_capture("parent-password");
    CHECK(C.pad && strstr(g_snap, "密码内容不可见"), "UX05 only the person proceeds to the existing masked password pad");
    on_ui(close_card); fx_view.parent = false; fx_view.token++; on_ui(fx_show); on_ui(fx_expire); fx_capture("expired");
    CHECK(strstr(g_snap, "已经过期") && strstr(g_snap, "没有执行"), "UX05 expiry remains an honest non-execution result");
    on_ui(close_card); ink_probe_count("test.finished");
}
void ink_main(void) { product_main(); fx_task(run); }
