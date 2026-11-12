/* Synthetic updates through the real product, with separate boots and UI fixtures. */
#include <stdlib.h>
#include "ink/hal_power.h"
#include "ink/sys/update.h"
#include "ink/ui_shell.h"
static void test_boot_check(void);
static void test_reboot(void);
#define ink_main product_main
#define ink_update_boot_check test_boot_check
#include "configured_boot.c"
#undef ink_update_boot_check
#undef ink_main
#include "integration/whole_product.h"
#define hal_power_reboot test_reboot
#include "../system/services/update/update_service.c"
#undef hal_power_reboot

static char mode[32], scene[32];
static const char *state_of(const char *id) {
    static char value[32]; value[0] = 0;
    ink_state_get_str(ink_state_find(id), value, sizeof value); return value;
}
static const char *wanted;
static int x, y;
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void quiet(void) { CHECK(WAIT_FOR(settled(), 10000), "physical update frame settles"); }
static bool page(const char *name) { on_ui(read_screen); return !strcmp(g_top, name); }
static void find_label(lv_obj_t *root) {
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); i++) {
        lv_obj_t *o = lv_obj_get_child(root, (int32_t)i);
        if (lv_obj_check_type(o, &lv_label_class) && !strcmp(lv_label_get_text(o), wanted)) {
            lv_area_t a; lv_obj_get_coords(o, &a); x = (a.x1 + a.x2) / 2; y = (a.y1 + a.y2) / 2;
        }
        find_label(o);
    }
}
static void locate(void) { x = y = -1; find_label(lv_screen_active()); find_label(lv_layer_sys()); }
static void press(const char *label) {
    quiet(); wanted = label; on_ui(locate);
    CHECK(x >= 0 && y >= 0, "visible update control: %s", label);
    if (x >= 0) tap(x, y);
}
static void inspect(void) {
    read_screen();
    CHECK(!strcmp(g_top, scene) && strstr(g_snap, scene) && strstr(g_snap, "\"fields\""), "AI snapshot names the visible update page: %s", scene);
    CHECK(strstr(g_snap, "\"id\":\"version\"") && strstr(g_snap, "\"id\":\"detail\""), "snapshot exposes actual version and explanation");
    if (!strcmp(scene, "UpdateInstall")) CHECK(!strstr(g_snap, "\"capability\""), "installation offers no unrelated actions");
    if (!strcmp(scene, "UpdateReady")) CHECK(!strstr(g_snap, "\"capability\":\"system.upgrade\""), "AI cannot install on behalf of the person");
    CHECK(ink_cap_find("shell.update") && ink_cap_find("shell.update_close") && ink_cap_find("job.cancel"), "page controls exist in the capability table");
}
static void frame(void *arg) {
    (void)arg;
    CHECK(settled(), "capture only the committed physical update frame");
    char counter[80]; snprintf(counter, sizeof counter, "test.snapshot.%s", scene);
    ink_probe_count(counter); ink_probe_count("test.snapshot.settled"); sim_display_snapshot("scene");
}
static void capture(const char *name) {
    snprintf(scene, sizeof scene, "%s", name); quiet(); on_ui(inspect);
    vtk_event_at(vtk_now() + SIM_MS(1), frame, NULL); delay_ms(5);
}
static bool self_check_fails(void) { return false; }
static void test_boot_check(void) {
    if (!strcmp(mode, "badboot") && ink_update_self_check_add(self_check_fails)) osal_panic("cannot inject the failed self check");
    ink_update_boot_check();
}
static void test_reboot(void) {
    if (!strcmp(mode, "badboot")) {
        CHECK(U.me.pending_verify, "failed candidate is still awaiting boot acceptance");
        CHECK(sim_metric_value("update.self_check_failed") == 1, "actual self-check failure asks for this reboot");
        CHECK(!strcmp(U.st.state, "trying"), "failed candidate never commits the installation");
        CHECK(!strcmp(U.me.version, "1.4.0"), "self-check ran on the new synthetic candidate");
        ink_probe_count("test.finished");
    }
    if (!strcmp(mode, "flow") || !strcmp(mode, "rollback")) {
        CHECK(WAIT_FOR(page("UpdateInstall"), 5000), "real installation reaches its full-screen presenter");
        snprintf(scene, sizeof scene, "UpdateInstall"); quiet(); on_ui(inspect);
        CHECK(BY_HAND("shell.update_close", NULL) && g_rc.status == INK_RC_REJECTED, "installation cannot be dismissed");
        ink_probe_count("test.finished");
    }
    hal_power_reboot();
}
static void battery(int percent, bool charging) {
    hw(charging ? HW_BATTERY_CHARGING : HW_BATTERY, percent, 0);
    ink_power_battery_policy_start();
}
static void storage_low(void) { ink_state_set_int(ink_state_find("storage.free"), 1); }
static void restore_space(void) { ink_state_set_int(ink_state_find("storage.free"), 4ll * 1024 * 1024 * 1024); }
static void link_event(void *arg) { sim_net_set_link(arg); }
static void offer_good(void *arg) { (void)arg; CHECK(setenv("INK_SIM_UPDATE_PKG", getenv("INK_UPDATE_GOOD_PKG"), 1) == 0, "replace only the synthetic corrupt package"); }
static void ui_fixture(void) {
    snprintf(U.offer_version, sizeof U.offer_version, "1.3.0"); U.offer_size = 1024 * 1024;
    snprintf(U.dl_version, sizeof U.dl_version, "1.3.0"); U.dl_size = U.offer_size;
    ink_state_set_int(U.h_progress, 62);
    set_state(!strcmp(mode, "UpdateReady") || !strcmp(mode, "UpdateBlocked") ? "ready" : !strcmp(mode, "UpdateFail") ? "failed" : !strcmp(mode, "UpdateInstall") ? "installing" : "downloading");
    if (!strcmp(mode, "UpdateBlocked")) ink_state_set_int(ink_state_find("battery.level"), 23);
    if (!strcmp(mode, "UpdateDone") || !strcmp(mode, "BootRecovery")) {
        set_state("idle");
        ink_state_set_str(U.h_version, !strcmp(mode, "UpdateDone") ? "1.3.0" : "1.2.0");
        ink_args_t a; ink_args_init(&a); ink_args_bool(&a, "ok", !strcmp(mode, "UpdateDone"));
        ink_cap_emit("update.result", &a);
    }
}
static void entry(void) {
    ink_hub_demand(); CHECK(WAIT_FOR(ink_hub_online(), 30000), "hub online before opening settings");
    CHECK(AI("update.check", NULL) && g_rc.status == INK_RC_DONE, "discover the real offer before entering settings");
    CHECK(WAIT_FOR(!strcmp(state_of("update.state"), "available"), 5000), "offer already available before entering settings");
    CHECK(BY_HAND("shell.about", NULL) && WAIT_FOR(page("SetAbout"), 3000), "open the existing about page");
    for (int i = 0; i < 2; i++) {
        quiet(); on_ui(read_screen);
        CHECK(strstr(g_snap, "\"capability\":\"shell.update\""), "about snapshot exposes the same update navigation");
        press("系统更新");
        CHECK(WAIT_FOR(page("Update"), 5000) && strstr(g_snap, U.offer_version), "existing update row opens the known offer on visit %d", i + 1);
        press("返回"); CHECK(WAIT_FOR(page("SetAbout"), 3000), "return to about after visit %d", i + 1);
    }
}
static void check_retry(void) {
    CHECK(!U.offer_code && !ink_hub_online(), "offline boot has no package to download");
    CHECK(BY_HAND("shell.update", NULL) && WAIT_FOR(page("Update"), 3000), "open update before any offer is known");
    press("检查更新");
    CHECK(WAIT_FOR(page("UpdateFail"), 30000), "offline check reaches the failure page");
    quiet(); on_ui(read_screen);
    CHECK(strstr(g_snap, "\"capability\":\"update.check\"") && !strstr(g_snap, "\"capability\":\"update.download\""), "failed check exposes checking rather than downloading");
    CHECK(strstr(g_snap, "重新检查") && !strstr(g_snap, "重新下载"), "failed check explains the available retry");
    double calls = sim_metric_value("action.user.update.check");
    press("重新检查");
    CHECK(WAIT_FOR(sim_metric_value("action.user.update.check") == calls + 1, 3000), "retry touch calls the real check capability again");
    CHECK(WAIT_FOR(page("UpdateFail"), 30000) && !U.offer_code, "another offline check remains retryable without a package");
}
static void flow(void) {
    CHECK(AI("shell.update", NULL) && g_rc.status == INK_RC_DONE, "AI opens the same update surface");
    ink_hub_demand(); CHECK(WAIT_FOR(ink_hub_online(), 30000), "synthetic hub is online");
    CHECK(AI("update.check", NULL) && g_rc.status == INK_RC_DONE, "check the real hub offer");
    CHECK(WAIT_FOR(!strcmp(state_of("update.state"), "available"), 5000), "real service finds a new version");
    CHECK(WAIT_FOR(page("Update"), 5000), "new-version page is visible");
    on_ui(storage_low); CHECK(AI("shell.update", NULL), "reread throttled storage summary");
    CHECK(WAIT_FOR(page("UpdateBlocked"), 2000) && strstr(g_snap, "空间不够"), "insufficient published storage reaches UpdateBlocked");
    on_ui(restore_space); CHECK(AI("shell.update", NULL), "reread restored storage summary");
    CHECK(WAIT_FOR(page("Update"), 2000), "space recovery restores download action");
    press("下载");
    CHECK(WAIT_FOR(!strcmp(state_of("update.state"), "downloading"), 2000), "touch starts the real download capability");
    vtk_event_at(vtk_now() + SIM_MS(1), link_event, "none"); delay_ms(5);
    press("暂停"); CHECK(WAIT_FOR(!strcmp(state_of("update.state"), "available"), 5000), "pause cancels the real background job");
    vtk_event_at(vtk_now() + SIM_MS(1), link_event, "wifi"); delay_ms(5);
    CHECK(WAIT_FOR(ink_hub_online(), 30000), "network returns before retrying");
    press("下载"); CHECK(WAIT_FOR(!strcmp(state_of("update.state"), "failed"), 60000), "corrupt signature fails the real download");
    CHECK(WAIT_FOR(page("UpdateFail"), 5000), "failed download enters UpdateFail");
    snprintf(scene, sizeof scene, "UpdateFail"); quiet(); on_ui(inspect);
    vtk_event_at(vtk_now() + SIM_MS(1), offer_good, NULL); delay_ms(5);
    press("重新下载"); CHECK(WAIT_FOR(!strcmp(state_of("update.state"), "ready"), 60000), "retry downloads and verifies the real package");
    CHECK(WAIT_FOR(page("UpdateReady"), 5000), "ready package enters UpdateReady");
    snprintf(scene, sizeof scene, "UpdateReady"); quiet(); on_ui(inspect);
    battery(23, false); CHECK(WAIT_FOR(page("UpdateBlocked"), 3000) && strstr(g_snap, "30%"), "low battery enters UpdateBlocked");
    battery(23, true);
    /* When the device learns about the cable is the power service's business (the vendor polls the charger every
     * 2 or 10 s, and later still while idle; backlog "charging shown late"); this page has 3 s from then. */
    int64_t charging = 0;
    CHECK(WAIT_FOR(ink_state_get_int(ink_state_find("battery.charging"), &charging) && charging, 70000), "the power service reports charging");
    CHECK(WAIT_FOR(page("UpdateReady"), 3000), "charging restores the installation choice");
    CHECK(AI("system.upgrade", NULL) && g_rc.status == INK_RC_INVALID, "AI cannot install the update");
    press("以后再装"); CHECK(WAIT_FOR(!page("UpdateReady"), 2000), "later returns without installing");
    CHECK(AI("shell.update", NULL) && g_rc.status == INK_RC_DONE, "reopen the ready package");
    press("现在安装");
    ink_confirm_view_t confirmation;
    CHECK(WAIT_FOR(ink_confirm_list(&confirmation, 1) == 1, 3000) && !strcmp(confirmation.cap, "system.upgrade"), "install uses the existing human confirmation");
    CHECK(ink_confirm_decide(confirmation.token, true, INK_SRC_TOUCH) == 0, "person confirms installation");
    delay_ms(60000);
    CHECK(false, "install should end this process with a reboot");
}
static void rollback(void) {
    ink_hub_demand(); CHECK(WAIT_FOR(ink_hub_online(), 30000), "hub online for next synthetic update");
    CHECK(AI("update.check", NULL) && g_rc.status == INK_RC_DONE, "find next package");
    CHECK(AI("update.download", NULL) && g_rc.status == INK_RC_DONE, "download next package");
    CHECK(WAIT_FOR(!strcmp(state_of("update.state"), "ready"), 60000), "next package ready");
    CHECK(BY_HAND("system.upgrade", NULL) && g_rc.status == INK_RC_PENDING_CONFIRMATION, "person asks for next installation");
    unsigned token = 0; CHECK(sscanf(g_rc.object, "confirm:%u", &token) == 1 && ink_confirm_decide(token, true, INK_SRC_TOUCH) == 0, "person confirms next installation");
    delay_ms(60000); CHECK(false, "next installation should reboot");
}
static void run(void *arg) {
    (void)arg;
    CHECK(WAIT_FOR(ink_member_space_open(), 10000), "member space opens");
    if (!strcmp(mode, "done") || !strcmp(mode, "recovery")) {
        const char *name = !strcmp(mode, "done") ? "UpdateDone" : "BootRecovery";
        CHECK(WAIT_FOR(page(name), 20000), "boot service presents %s", name);
        CHECK(sim_metric_value(!strcmp(mode, "done") ? "update.self_check_passed" : "update.rolled_back") == 1, "result comes from the actual boot check");
        capture(name); press("知道了"); CHECK(WAIT_FOR(!page(name), 2000), "result returns to the previous page");
    } else {
        test_start_home(); quiet();
        if (!strcmp(mode, "flow")) { flow(); return; }
        if (!strcmp(mode, "rollback")) { rollback(); return; }
        if (!strcmp(mode, "badboot")) return;
        if (!strcmp(mode, "entry") || !strcmp(mode, "check-retry")) {
            if (!strcmp(mode, "entry")) entry(); else check_retry();
            ink_probe_count("test.finished"); return;
        }
        CHECK(AI("shell.update", NULL) && g_rc.status == INK_RC_DONE, "open fixture surface");
        on_ui(ui_fixture); CHECK(WAIT_FOR(page(mode), 5000), "service fixture selects %s", mode);
        capture(mode);
        if (strcmp(mode, "UpdateInstall")) {
            key(INK_KEY_HOME); CHECK(WAIT_FOR(!page(mode), 3000), "physical home restores the underlying screen");
            CHECK(ink_member_switch("kid") == 0, "switch to child");
            CHECK(AI("shell.update", NULL) && g_rc.status == INK_RC_REJECTED, "child cannot open adult update controls");
        }
    }
    ink_probe_count("test.finished");
}
void ink_main(void) {
    sim_copy_name(mode, sizeof mode, getenv("INK_UPDATE_PAGE_MODE"));
    product_main();
    osal_task_cfg_t cfg = {.name = "update-pages-test", .fn = run, .stack_bytes = 32768,
        .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start the update page driver");
}
