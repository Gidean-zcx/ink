/* Real boot, card, package files, controls and member policies; synthetic content only. */
#include "../../system/ui/app_packages.c"
#define ink_main product_main
#include "../configured_boot.c"
#undef ink_main
#include "../integration/whole_product.h"
#include "ink/sys/storage.h"

static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void quiet(void) { CHECK(WAIT_FOR(settled(), 15000), "the page settles"); }
static void inspect(void) { read_screen(); }
static bool listed(const char *id) { return AI("app.list", NULL) && g_rc.status == INK_RC_DONE && strstr(g_result, id); }
static bool scans_done(void) { return !PK.scanning; }
static void scan_idle(void) { CHECK(WAIT_FOR((on_ui(inspect), scans_done()), 15000), "package scan completes"); quiet(); }
static void ok(const char *cap, const char *args) {
    CHECK(AI(cap, args) && g_rc.status == INK_RC_DONE, "%s: %s %.90s", cap, ink_rc_name(g_rc.status), g_rc.detail); quiet();
}
static const char *wanted;
static lv_point_t point;
static bool found;
static void locate(lv_obj_t *o) {
    if (!lv_obj_is_visible(o)) return;
    if (lv_obj_check_type(o, &lv_label_class) && !strcmp(lv_label_get_text(o), wanted)) {
        lv_area_t a; lv_obj_get_coords(o, &a); point = (lv_point_t){(a.x1+a.x2)/2, (a.y1+a.y2)/2}; found = true;
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) locate(lv_obj_get_child(o, (int32_t)i));
}
static void find_label(void) { found = false; lv_obj_update_layout(lv_screen_active()); locate(lv_screen_active()); locate(lv_layer_sys()); }
static void touch(const char *label) {
    quiet(); wanted = label; on_ui(find_label); CHECK(found, "visible control: %s", label);
    if (found) tap(point.x, point.y);
    quiet();
}
static volatile int echo_runs;
static void echo(ink_call_t *c) {
    int64_t level = 0; bool on = false;
    ink_args_get_int(&c->args, "level", &level); ink_args_get_bool(&c->args, "on", &on);
    char json[96];
    if (ink_args_find(&c->args, "on")) snprintf(json, sizeof json, "{\"on\":%s}", on ? "true" : "false");
    else if (ink_args_find(&c->args, "level")) snprintf(json, sizeof json, "{\"level\":%lld}", (long long)level);
    else snprintf(json, sizeof json, "{}");
    ink_cap_result(c, json); echo_runs++;
}
static const ink_cap_t ECHO = {.id = "test.runtime_echo", .permission = "synthetic", .title = "合成控件回执",
    .child_ok = true, .effect = INK_FX_READ, .run = echo};
static volatile bool pin_done;
static int pin_result;
static void pin_reply(void *ctx, int result, uint32_t wait) { (void)ctx; (void)wait; pin_result = result; pin_done = true; }
static void policy(int mode) {
    char json[96]; snprintf(json, sizeof json, "{\"id\":\"kid\",\"app\":\"units\",\"mode\":%d}", mode);
    CHECK(AI("kid.app_set", json) && g_rc.status == INK_RC_PENDING_CONFIRMATION, "parent policy asks for confirmation");
    unsigned token = 0; CHECK(sscanf(g_rc.object, "confirm:%u", &token) == 1, "policy has a real system token");
    pin_done = false;
    CHECK(!ink_confirm_accept_with_pin(token, "24681357", pin_reply, NULL) && WAIT_FOR(pin_done, 6000) && pin_result == INK_PIN_OK,
        "the real parent confirms the child application list"); quiet();
}
static void card_out(void *arg) { (void)arg; sim_sd_set_present(false); }
static void card_in(void *arg) { (void)arg; sim_sd_set_present(true); }
static void capture(void *arg) { (void)arg; sim_display_snapshot("AppRuntime"); }
static bool package_registered(void) {
    for (int i = 0; i < ink_app_count(); i++) if (!strcmp(ink_app_at(i)->id, "units")) return true;
    return false;
}
static void run(void *arg) {
    (void)arg;
    CHECK(WAIT_FOR(ink_member_space_open(), 10000), "the adult member space opens"); quiet(); test_start_home(); scan_idle();
    CHECK(listed("units"), "INSTALL: copied example is installed at boot and AI can list it");
    CHECK(!listed("imposter") && !listed("broken") && sim_metric_value("app.package_invalid") >= 2, "BAD: invalid manifest and node packages are skipped independently");
    ok("app.open", "{\"id\":\"units\"}"); on_ui(inspect);
    CHECK(!strcmp(g_top, "AppPage") && strstr(g_snap, "2*100") && strstr(g_snap, "app.page_action"), "SNAPSHOT: the example exports its actual fields and shared action");
    touch("换算"); on_ui(inspect);
    CHECK(strstr(g_snap, "200") && !strstr(g_snap, "等待换算"), "touch conversion renders the capability receipt");
    ok("app.page_action", "{\"id\":\"expression\"}"); ok("text.select", "{\"start\":0,\"end\":5}");
    ok("text.insert", "{\"text\":\"3*100\"}"); ok("text.submit", NULL);
    ok("app.page_action", "{\"id\":\"convert\"}"); on_ui(inspect);
    CHECK(strstr(g_snap, "300"), "AI and touch use the same calculation and receipt binding");
    ok("file.write", "{\"path\":\"/apps/bad/manifest.json\",\"text\":\"{}\",\"append\":true}"); scan_idle(); on_ui(inspect);
    CHECK(!strcmp(g_top, "AppPage") && strstr(g_snap, "3*100") && strstr(g_snap, "300"), "a changed bad package never closes or resets an unchanged good app");
    vtk_event_at(vtk_now() + SIM_MS(1), capture, NULL); delay_ms(5);
    ok("app.page_turn", "{\"direction\":1}");
    CHECK(AI("app.page_action", "{\"id\":\"denied\"}") && g_rc.status == INK_RC_REJECTED, "PERMISSION: undeclared capability cannot be called");
    ok("app.page_action", "{\"id\":\"switch\",\"value\":true}");
    ok("app.page_action", "{\"id\":\"slider\",\"value\":7}");
    ok("app.page_action", "{\"id\":\"segment\",\"value\":1}"); on_ui(inspect);
    CHECK(echo_runs == 3 && strstr(g_snap, "7") && strstr(g_snap, "true"), "switch, slider, segment use typed capability arguments and receipts");
    CHECK(strstr(g_snap, "甲") && strstr(g_snap, "乙") && strstr(g_snap, "[0..10]"), "AI sees the segment choices and slider range, beyond the selected value");
    CHECK(AI("app.page_action", "{\"id\":\"slider\",\"value\":11}") && g_rc.status == INK_RC_INVALID, "slider rejects a value outside the declared control range");
    ok("app.page_turn", "{\"direction\":1}"); on_ui(inspect);
    CHECK(strstr(g_snap, "合成列表行"), "additional list nodes remain reachable by pagination");
    ok("shell.back", NULL); CHECK(strcmp(ink_ui_page_name(), "AppPage"), "back closes the package page");
    pin_done = false;
    CHECK(!ink_member_set_pin("owner", "", "24681357", pin_reply, NULL) && WAIT_FOR(pin_done, 6000) && pin_result == INK_PIN_OK, "set a synthetic parent PIN");
    policy(0);
    CHECK(!ink_member_switch("kid"), "switch into the real child member"); quiet();
    CHECK(!listed("units") && AI("app.open", "{\"id\":\"units\"}") && g_rc.status == INK_RC_REJECTED, "CHILD: a disabled package is absent and cannot open");
    CHECK(BY_HAND("file.mkdir", "{\"path\":\"/apps/forbidden\"}") && g_rc.status == INK_RC_REJECTED, "children cannot install or mutate shared package files");
    pin_done = false;
    CHECK(!ink_member_unlock("owner", "24681357", pin_reply, NULL) && WAIT_FOR(pin_done, 6000) && pin_result == INK_PIN_OK, "return to the real parent"); quiet(); policy(1);
    CHECK(!ink_member_switch("kid"), "switch to the allowed child"); quiet(); ok("app.open", "{\"id\":\"units\"}"); on_ui(inspect);
    CHECK(strstr(g_snap, "2*100") && !strstr(g_snap, "3*100") && !strstr(g_snap, "300"), "member switch never exposes the previous member's input or receipt");
    pin_done = false;
    CHECK(!ink_member_unlock("owner", "24681357", pin_reply, NULL) && WAIT_FOR(pin_done, 6000) && pin_result == INK_PIN_OK, "return to parent before card tests"); quiet();
    ok("app.open", "{\"id\":\"units\"}");
    vtk_event_at(vtk_now() + SIM_MS(1), card_out, NULL); delay_ms(5);
    CHECK(WAIT_FOR(!hal_fs_card_present(), 3000), "the real card model reports removal"); scan_idle();
    CHECK(!package_registered() && !listed("units") && strcmp(ink_ui_page_name(), "AppPage"), "CARD: removing the card closes package pages and removes installed definitions");
    vtk_event_at(vtk_now() + SIM_MS(1), card_in, NULL); delay_ms(5);
    CHECK(WAIT_FOR(hal_fs_card_present(), 3000), "the same card is reinserted");
    CHECK(WAIT_FOR(listed("units"), 15000), "card insertion scans and reinstalls the package"); scan_idle();
    ok("app.open", "{\"id\":\"units\"}"); touch("卸载");
    CHECK(!strcmp(ink_ui_top_name(), "confirm"), "uninstall uses the existing system confirmation card");
    touch("删除");
    CHECK(WAIT_FOR(!package_registered(), 15000), "UNINSTALL: the file deletion event removes the installed app"); scan_idle();
    CHECK(!listed("units") && strcmp(ink_ui_page_name(), "AppPage"), "uninstall removes AI entry and foreground page");
    CHECK(AI("file.copy", "{\"from\":\"/me/SYNTH_package\",\"to\":\"/apps/units\"}") &&
        (g_rc.status == INK_RC_DONE || g_rc.status == INK_RC_QUEUED), "the real file service accepts a package tree copy");
    CHECK(WAIT_FOR(listed("units"), 15000), "copying via the existing file capability installs without reboot"); scan_idle();
    ink_probe_count("test.finished");
}
void ink_main(void) {
    CHECK(!ink_cap_register(&ECHO, 1), "register the bounded synthetic control provider");
    product_main();
    osal_task_cfg_t cfg = {.name = "app-runtime-test", .fn = run, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1,
        .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start the test driver");
}
