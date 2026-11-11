#include "common.h"
#ifndef INK_TEST_SOURCE
#define INK_TEST_SOURCE "../../system/ui/shell/member_pages.c"
#endif
#define key test_source_key
#include INK_TEST_SOURCE
#undef key
#ifndef INK_TEST_PINPAD
#define INK_TEST_PINPAD "../../system/ui/shell/pinpad.c"
#endif
#define P PAD
#include INK_TEST_PINPAD
#undef P

static volatile int test_pin_ready;
static void test_pin_saved(void *ctx, int result, uint32_t wait_s) {
    (void)ctx; (void)wait_s; test_pin_ready = result == INK_PIN_OK ? 1 : -1;
}
static void test_pin(void) {
    ink_shell_member_pin_open("owner");
    CHECK(test_label(P.card.panel, P.name) && test_label(P.card.panel, "四到八位数字"), "UX16 identity and four-to-eight digit rule are visible");
    lv_obj_t *zero = test_label(P.card.panel, "0"), *ok = test_label(P.card.panel, "确定"), *del = test_label(P.card.panel, "删除");
    CHECK(zero && ok && del && lv_obj_get_y(lv_obj_get_parent(ok)) == lv_obj_get_y(lv_obj_get_parent(zero)) &&
        lv_obj_get_x(lv_obj_get_parent(ok)) > lv_obj_get_x(lv_obj_get_parent(zero)) &&
        lv_obj_get_x(lv_obj_get_parent(del)) < lv_obj_get_x(lv_obj_get_parent(zero)), "UX16 confirmation is at lower right and delete at lower left");
    CHECK(test_fits(ok) && test_fits(test_label(P.card.panel, "忘记密码")), "UX16 key actions fit at both sizes");
    lv_obj_t *r = test_label(P.card.panel, "四到八位数字");
    if (r) {
        lv_area_t rule; lv_obj_get_coords(r, &rule);
        CHECK(rule.y1 >= ink_ui_px(INK_STATUS_BAR + INK_CONTROL + INK_GAP), "UX16 password rule stays below emergency controls");
    }
}
static void test_wrong(void) {
    snprintf(P.pin, sizeof P.pin, "9876");
    ink_receipt_t rc; ink_cap_invoke_json("shell.pin_submit", "{}", INK_SRC_TOUCH, ink_ui_content_version(), &rc);
}
static void test_wait(void) {
    CHECK(remaining() > 0, "UX16 three real wrong passwords reach the existing cooldown");
    lv_obj_t *one = test_label(P.card.panel, "1"), *ok = test_label(P.card.panel, "确定");
    CHECK(one && lv_obj_has_state(lv_obj_get_parent(one), LV_STATE_DISABLED) && ok && lv_obj_has_state(lv_obj_get_parent(ok), LV_STATE_DISABLED), "UX16 cooldown disables digits and confirmation");
    ink_receipt_t rc; ink_cap_invoke_json("shell.pin_submit", "{}", INK_SRC_AI, ink_ui_content_version(), &rc);
    CHECK(rc.status != INK_RC_DONE, "UX16 AI cannot submit the local PIN");
    snprintf(P.pin, sizeof P.pin, "9876");
    CHECK(semantic_snapshot(test_snapshot, sizeof test_snapshot) > 0 && !strstr(test_snapshot, "9876"), "UX16 secret field never exports its digits");
    erase(P.pin, sizeof P.pin);
}
static void test_forgot(void) {
    P.retry_us = 0; P.mode = UNLOCK; draw();
    ink_receipt_t rc; ink_cap_invoke_json("shell.pin_forgot", "{}", INK_SRC_TOUCH, ink_ui_content_version(), &rc);
    CHECK(strstr(P.status, "本机不能重置") && strstr(P.status, "家庭服务器"), "UX16 forgot page states the actual recovery boundary");
}
static void test_set(void) { close_page(); P.mode = NEW; snprintf(P.name, sizeof P.name, "合成家人"); draw(); }
static void test_pad(void) {
    close_page(); lv_obj_t *pad = shell_pinpad(lv_layer_sys(), NULL);
    lv_obj_t *zero = test_label(pad, "0"), *ok = test_label(pad, "确定");
    CHECK(test_label(pad, "输入密码 · 四到八位数字"), "UX16 generic PIN pad states its input length");
    CHECK(ok && zero && lv_obj_get_y(lv_obj_get_parent(ok)) == lv_obj_get_y(lv_obj_get_parent(zero)), "UX16 generic PIN pad shares the lower-right confirmation");
    lv_area_t a; lv_obj_get_coords(lv_obj_get_parent(ok), &a);
    CHECK(a.y2 == ink_ui_px(INK_CANVAS_H - INK_MARGIN) - 1, "UX16 both numeric grids share the bottom margin");
}
static void test_run(void *arg) {
    (void)arg; test_init();
    CHECK(ink_member_set_pin("owner", NULL, "1234", test_pin_saved, NULL) == 0, "set a synthetic PIN");
    CHECK(WAIT_FOR(test_pin_ready != 0, 5000) && test_pin_ready == 1, "synthetic PIN is saved");
    on_ui(test_pin); test_frame("pin");
    on_ui(test_wrong); CHECK(WAIT_FOR(!P.busy, 5000), "first wrong password completes"); test_frame("pin-wrong");
    for (int i = 0; i < 2; i++) { on_ui(test_wrong); CHECK(WAIT_FOR(!P.busy, 5000), "wrong password completes"); }
    on_ui(test_wait); test_frame("pin-wait");
    on_ui(test_forgot); test_frame("pin-forgot"); on_ui(test_set); test_frame("pin-set");
    on_ui(test_pad); test_frame("pin-pad"); ink_probe_count("test.finished");
}
