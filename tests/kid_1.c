/* Real touch controls and persistent synthetic PINs, across separate simulator boots. */
#include <stdlib.h>
#define key member_page_key
#include "../system/ui/shell/member_pages.c"
#undef key
#define ink_main product_main
#include "configured_boot.c"
#undef ink_main
#include "integration/whole_product.h"

static char phase[24], capture_scene[24], captured[24];
static const char *wanted;
static lv_obj_t *found;
static int px, py;
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void quiet(void) { CHECK(WAIT_FOR(settled(), 12000), "member display settles"); }
static void find_label(lv_obj_t *root) {
    if (lv_obj_has_flag(root, LV_OBJ_FLAG_HIDDEN)) return;
    if (lv_obj_check_type(root, &lv_label_class) && lv_obj_check_type(lv_obj_get_parent(root), &lv_button_class) && !strcmp(lv_label_get_text(root), wanted)) found = root;
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); i++) find_label(lv_obj_get_child(root, (int32_t)i));
}
static void locate(void) {
    found = NULL; find_label(lv_layer_sys());
    CHECK(found != NULL, "requested local member control exists");
    if (!found) return;
    lv_area_t r; lv_obj_t *button = lv_obj_get_parent(found); lv_obj_update_layout(button); lv_obj_get_coords(button, &r);
    CHECK(lv_area_get_height(&r) >= ink_ui_px(INK_CONTROL), "member control has a full touch target");
    px = (r.x1 + r.x2) / 2; py = (r.y1 + r.y2) / 2;
}
static void touch(const char *label) {
    quiet(); wanted = label; on_ui(locate); if (found) tap(px, py); quiet();
}
static bool page(const char *name) { on_ui(read_screen); return !strcmp(g_top, name); }
static void host_context(void *arg) {
    (void)arg; char path[768];
    int n = snprintf(path, sizeof path, "%s/contexts.txt", g_sim.out_dir);
    if (n < 0 || (size_t)n >= sizeof path) abort();
    FILE *f = fopen(path, "ab");
    if (!f || fprintf(f, "%s\n", g_snap) < 0 || fclose(f)) abort();
}
static void context(void) {
    on_ui(read_screen);
    CHECK(g_snap[0] && !strstr(g_snap, "24681357") && !strstr(g_snap, "13572468") && !strstr(g_snap, "90817263"), "AI snapshot never contains a synthetic PIN");
    vtk_host_call(host_context, NULL);
}
static void screenshot(void *arg) {
    (void)arg; CHECK(settled(), "capture only a settled physical frame");
    ink_probe_count("test.snapshot.settled");
    char metric[64]; snprintf(metric, sizeof metric, "test.snapshot.%s", captured); ink_probe_count(metric);
    sim_display_snapshot("scene");
}
static void capture(const char *name) {
    context(); if (strcmp(capture_scene, name)) return;
    snprintf(captured, sizeof captured, "%s", name); quiet();
    vtk_event_at(vtk_now() + SIM_MS(1), screenshot, NULL); delay_ms(5);
}
static void type_pin(const char *pin) {
    for (const char *p = pin; *p; p++) { char label[2] = {*p, 0}; touch(label); context(); }
}
static void enter(const char *pin) { type_pin(pin); touch("确定"); }
static void choose_owner(void) {
    CHECK(BY_HAND("shell.members", NULL) && g_rc.status == INK_RC_DONE, "open member handoff through the bus"); quiet();
    CHECK(page("MemberSwitch"), "handoff preview precedes the actual member picker");
    touch("换人"); CHECK(page("member_gate"), "choose the next member");
    touch("我"); CHECK(page("member_gate"), "a protected member opens local PIN input");
    context();
}
static void wait_for_unlock(void) {
    CHECK(WAIT_FOR(ink_member_space_open() && !strcmp(ink_member_current()->id, "owner"), 5000), "a correct local PIN opens the adult space"); quiet();
    CHECK(strcmp(ink_ui_top_name(), "member_gate"), "successful authentication closes both local surfaces");
}
static void preview(void) {
    CHECK(AI("shell.members", NULL) && g_rc.status == INK_RC_DONE, "AI opens the same handoff preview as touch"); quiet();
    CHECK(page("MemberSwitch"), "preview is the actual product surface"); capture("MemberSwitch");
    touch("取消"); CHECK(!strcmp(ink_member_current()->id, "owner"), "cancelling never changes identity");
}
static void write_pins(void) {
    CHECK(BY_HAND("shell.members", NULL) && g_rc.status == INK_RC_DONE, "open member controls"); quiet();
    touch("换人"); touch("设置或修改密码");
    CHECK(page("PinSet"), "setting a PIN has its own page"); capture("PinSet");
    enter("123"); CHECK(!ink_member_has_pin("owner"), "a short PIN is not submitted or saved");
    touch("删除"); touch("删除"); touch("删除"); type_pin("24681357");
    CHECK(AI("shell.pin_submit", NULL) && g_rc.status != INK_RC_DONE, "AI cannot submit the local credential buffer");
    CHECK(!ink_member_has_pin("owner"), "denied AI submission cannot change the PIN");
    touch("确定"); CHECK(WAIT_FOR(ink_member_has_pin("owner"), 5000), "real touch sets an eight-digit synthetic PIN"); quiet();
    CHECK(AI("shell.pin_set", NULL) && g_rc.status == INK_RC_DONE, "AI can open the form without filling credentials"); quiet();
    CHECK(page("PinSet"), "change PIN page is visible"); enter("24681357");
    CHECK(page("PinSet"), "old PIN leads to entering a new PIN"); enter("13572468");
    CHECK(WAIT_FOR(!P.busy, 5000), "change finishes on the background executor"); quiet();
    CHECK(strcmp(ink_ui_top_name(), "PinSet"), "successful change closes the form");
    ink_db_flush_all(); CHECK(WAIT_FOR(ink_db_idle() && !ink_storage_pending(), 15000), "PIN hash and member table are durable before process exit");
}
static void verify_pins(void) {
    CHECK(ink_member_has_pin("owner"), "the PIN survives a separate simulator boot");
    CHECK(!ink_member_space_open(), "the protected adult space starts closed after reboot");
    touch("我"); enter("13572468"); wait_for_unlock();
    CHECK(ink_member_switch("kid") == 0, "start verification in the child's open space"); quiet();
    choose_owner(); enter("24681357");
    CHECK(!strcmp(ink_member_current()->id, "kid"), "the replaced PIN cannot enter the adult space");
    enter("13572468"); wait_for_unlock();
    CHECK(ink_member_switch("kid") == 0, "return to the child for access control checks"); quiet();
    CHECK(AI("shell.pin_set", NULL) && g_rc.status != INK_RC_DONE, "a child cannot change an adult PIN");
}
static void wrong_pins(void) {
    CHECK(ink_member_switch("kid") == 0, "the child begins the failed attempts"); quiet();
    choose_owner();
    for (int i = 0; i < 3; i++) {
        enter("90817263");
        CHECK(!strcmp(ink_member_current()->id, "kid"), "a wrong PIN never changes the member");
        CHECK(sim_metric_value("members.pin_wrong") == i + 1, "each failed local submission is counted exactly once");
    }
    CHECK(page("PinWait"), "the third failure enters PinWait"); capture("PinWait");
    CHECK(P.wait_s == 30, "the unchanged first wait is exactly thirty seconds");
    CHECK(strstr(g_snap, "还需等待") && remaining() > 0, "wait page and snapshot report the actual remaining delay");
    double wrong = sim_metric_value("members.pin_wrong");
    enter("13572468");
    CHECK(page("PinWait") && sim_metric_value("members.pin_wrong") == wrong, "waiting rejects input without consuming another attempt");
    ink_db_flush_all(); CHECK(WAIT_FOR(ink_db_idle() && !ink_storage_pending(), 15000), "attempt count and deadline survive the next boot");
}
static void recover(void) {
    CHECK(ink_member_switch("kid") == 0, "the child starts after the wait-state reboot"); quiet();
    choose_owner(); enter("13572468");
    CHECK(page("PinWait") && sim_metric_value("members.pin_too_soon") >= 1, "a reboot does not bypass the policy deadline");
    uint32_t left = remaining(); CHECK(left > 0 && left <= 900, "a reset simulator RTC preserves the deadline within the existing policy cap");
    delay_ms((int)left * 1000 + 1000); enter("13572468"); wait_for_unlock();
    CHECK(ink_member_switch("kid") == 0, "return to the child before asking a parent"); quiet();
    choose_owner(); touch("忘记密码"); CHECK(page("PinForgot"), "forgotten PIN has a help page"); capture("PinForgot");
    touch("家长验证"); enter("90817263");
    CHECK(!strcmp(ink_member_current()->id, "kid") && sim_metric_value("members.pin_wrong") == 1, "a wrong parent PIN uses the actual parent's attempt counter");
    enter("13572468");
    CHECK(WAIT_FOR(page("PinForgot") && strstr(g_snap, "家长已验证"), 5000), "the real parent verifier reports success");
    CHECK(!strcmp(ink_member_current()->id, "kid") && ink_member_has_pin("owner"), "parent verification cannot silently switch identity or erase a PIN");
    CHECK(strstr(g_snap, "还不能重置"), "the missing reset operation is reported honestly"); context();
    touch("返回");
}
static void test_task(void *arg) {
    (void)arg; ink_power_hold(INK_NEED_AWAKE); quiet();
    if (!strcmp(phase, "preview")) preview();
    else if (!strcmp(phase, "write")) write_pins();
    else if (!strcmp(phase, "verify")) verify_pins();
    else if (!strcmp(phase, "wait")) wrong_pins();
    else if (!strcmp(phase, "recover")) recover();
    else CHECK(false, "unknown kid-1 phase");
    ink_probe_count("test.finished");
}
void ink_main(void) {
    snprintf(phase, sizeof phase, "%s", getenv("INK_KID_PHASE"));
    snprintf(capture_scene, sizeof capture_scene, "%s", getenv("INK_KID_SCENE") ? getenv("INK_KID_SCENE") : "");
    product_main(); test_start_home();
    osal_task_cfg_t cfg = {.name = "test.kid-1", .fn = test_task, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    if (osal_task_create(&cfg, NULL) != OSAL_OK) osal_panic("kid-1 test task");
}
