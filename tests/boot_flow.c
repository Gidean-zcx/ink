/* Synthetic household, real UI, Wi-Fi, enrollment and restore services. */
#include <stdlib.h>
#include "ink/ui_shell.h"
#include "ink/sys/textfield.h"
#define ink_main product_main
#include "../system/boot/main.c"
#undef ink_main
#include "integration/whole_product.h"

#define CONFIG "{\"url\":\"wss://example.com/v1/device\",\"console\":\"https://example.com\",\"pin\":\"1111111111111111111111111111111111111111111111111111111111111111\"}"
static const char SECRET[] = "test-secret-123";
static const char WRONG[] = "test-wrong-secret";
static const char *label, *picture;
static int label_x, label_y;
static volatile bool captured;
static char context[6144];
static bool restore_mode;
static int probe;
static int qr_count(lv_obj_t *root) {
    int n = lv_obj_check_type(root, &lv_qrcode_class) ? 1 : 0;
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); i++) n += qr_count(lv_obj_get_child(root, (int32_t)i));
    return n;
}
static void locate(lv_obj_t *root) {
    if (!lv_obj_is_visible(root)) return;
    if (lv_obj_check_type(root, &lv_label_class) && !strcmp(lv_label_get_text(root), label)) {
        lv_area_t a; lv_obj_get_coords(lv_obj_get_parent(root), &a);
        label_x = (a.x1 + a.x2) / 2; label_y = (a.y1 + a.y2) / 2;
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); i++) locate(lv_obj_get_child(root, (int32_t)i));
}
static void locate_label(void) {
    label_x = label_y = -1; lv_obj_update_layout(lv_screen_active()); lv_obj_update_layout(lv_layer_sys());
    locate(lv_screen_active()); locate(lv_layer_sys());
}
static void press(const char *name) {
    label = name; on_ui(locate_label);
    CHECK(label_x >= 0 && label_y >= 0, "visible product control: %s", name);
    if (label_x >= 0 && label_y >= 0) tap(label_x, label_y);
}
static void read_page(void) { read_screen(); snprintf(g_top, sizeof g_top, "%s", ink_ui_page_name()); }
static bool at(const char *name) { on_ui(read_page); return !strcmp(g_top, name); }
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static bool ready(const char *name) {
    bool ok = WAIT_FOR(at(name), 18000) && WAIT_FOR(settled(), 10000);
    CHECK(ok, "arrive at %s (actual %s)", name, g_top);
    return ok;
}
static void inspect(void) {
    const char *name = NULL;
    CHECK(ink_ui_snapshot(context, sizeof context, &name) > 0 && name &&
          (!strcmp(name, picture) || (!strcmp(picture, "BootWifiPass") && !strcmp(name, "Keyboard"))),
          "page exports a complete AI snapshot: %s", picture);
    CHECK(!strstr(context, SECRET) && !strstr(context, WRONG) && !strstr(context, "WIFI:T:WPA") &&
          !strstr(context, "test-only-pending-token"), "page snapshot excludes all credential material");
    CHECK(ink_ui_context_snapshot(context, sizeof context, NULL) > 0 && !strstr(context, SECRET) &&
          !strstr(context, WRONG) && !strstr(context, "WIFI:T:WPA"), "AI context excludes credentials including beneath keyboard");
    if (!strcmp(picture, "FirstBoot")) CHECK(qr_count(lv_screen_active()) == 1, "FirstBoot renders the real setup QR returned only to its owner UI");
    if (!strcmp(picture, "BootWifiPass")) {
        CHECK(ink_text_type() == INK_TEXT_PASSWORD && ink_text_ai_hidden(), "password uses the protected input service");
        ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL);
        char *page = osal_malloc(sizeof context, OSAL_MEM_EXTERNAL);
        bool ok = j && page && ink_json_parse(j, context, strlen(context)) == 0 &&
            ink_json_str(j, ink_json_find(j, 0, "page"), page, sizeof context) > 0 &&
            ink_json_parse(j, page, strlen(page)) == 0;
        CHECK(ok && ink_json_eq(j, ink_json_find(j, 0, "input.field"), "password") &&
            ink_json_eq(j, ink_json_find(j, 0, "input.focused"), "true"), "password uses the focused F2 short input host");
        osal_free(page); osal_free(j);
    }
}
/* A snapshot is taken only of a settled scene. A periodic refresh can start right after the scene settled (the
 * status-bar clock at a minute boundary: the restore scene reaches BootHello near 60 s); then wait for it to end
 * rather than call the scene unpresented. Still a failure if it does not settle within the capture's 2 s. */
static int snapshot_tries;
static void snapshot_event(void *arg) {
    if (!settled()) {
        if (++snapshot_tries < 150) { vtk_event_at(vtk_now() + SIM_MS(10), snapshot_event, arg); return; }
        CHECK(false, "all scene snapshots must be fully presented");
        return;
    }
    sim_display_snapshot((const char *)arg); captured = true;
}
static void capture(const char *name) {
    if (!ready(name)) return;
    picture = name; on_ui(inspect);
    captured = false;
    snapshot_tries = 0;
    vtk_event_at(vtk_now() + SIM_MS(1), snapshot_event, (void *)name);
    CHECK(WAIT_FOR(captured, 2000), "settled scene captured: %s", name);
    char metric[80]; snprintf(metric, sizeof metric, "test.snapshot.%s", name); ink_probe_count(metric);
}
static void environment(void *arg) { (void)arg; sim_wifi_ap_add("test-wifi", -42, SECRET); }
static void offer_event(void *arg) {
    (void)arg; sim_hub_request_device_raw("pair/offer",
        "{\"txid\":\"test-tx-1\",\"family\":\"测试家庭\",\"by\":\"测试家人\",\"name\":\"测试设备\"}");
}
static void revoke_event(void *arg) { (void)arg; sim_hub_pair_faults(0, -32011, false); }
static void reset_fault(void *arg) { (void)arg; sim_hub_pair_faults(0, 0, false); }
static void native_type(const char *value) {
    for (const char *p = value; *p; p++) {
        char args[40]; snprintf(args, sizeof args, "{\"code\":%u}", (unsigned char)*p);
        CHECK(BY_HAND("keyboard.key", args) && g_rc.status == INK_RC_DONE, "human keyboard accepts a synthetic password character");
    }
}
static void password_snapshot(void) {
    CHECK(ink_ui_snapshot(context, sizeof context, NULL) > 0 && !strstr(context, SECRET) && !strstr(context, WRONG),
          "typed password absent from page snapshot");
    CHECK(ink_ui_context_snapshot(context, sizeof context, NULL) > 0 && !strstr(context, SECRET) && !strstr(context, WRONG),
          "typed password absent from nested AI context");
    ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL);
    CHECK(j && ink_json_parse(j, context, strlen(context)) == 0, "keyboard context is complete JSON");
    osal_free(j);
}
static bool backup_phase(const char *phase) {
    return AI("backup.status", NULL) && g_rc.status == INK_RC_DONE && strstr(g_result, phase);
}
static void poweroff(void *arg) { (void)arg; sim_power_off("boot flow checkpoint"); }
static void checkpoint(const char *page) {
    CHECK(ready(page), "checkpoint reaches %s", page);
    ink_db_flush_all();
    CHECK(WAIT_FOR(ink_db_idle() && ink_storage_pending() == 0 && settled(), 15000), "progress is durable before power loss");
    ink_probe_count("test.finished");
    vtk_event_at(vtk_now() + SIM_MS(1), poweroff, NULL);
}
static void resume_run(const char *phase) {
    if (!strcmp(phase, "cold")) {
        ink_db_t *members = ink_db_open(ink_db_find_def("member.table"), NULL);
        CHECK(members && ink_db_count(members) == 0 && !ink_pairing_enabled(), "blank flash has no saved members or pairing");
        if (members) ink_db_close(members);
        CHECK(ready("FirstBoot") && sim_provision_active(), "blank device starts setup without a network settings action");
        checkpoint("FirstBoot");
    } else if (!strcmp(phase, "wifi")) {
        CHECK(ready("FirstBoot"), "first page survives a restart");
        press("在设备上设置");
        CHECK(WAIT_FOR(at("BootWifi") && strstr(g_snap, "test-wifi"), 8000), "scan completes before checkpoint");
        checkpoint("BootWifi");
    } else if (!strcmp(phase, "password")) {
        CHECK(WAIT_FOR(at("BootWifi") && strstr(g_snap, "test-wifi"), 8000), "saved Wi-Fi page rescans after restart");
        press("test-wifi"); CHECK(ready("BootWifiPass"), "enter selected network password");
        native_type(WRONG); checkpoint("BootWifiPass");
    } else if (!strcmp(phase, "failure")) {
        CHECK(ready("BootWifiPass") && strstr(g_snap, "test-wifi"), "password page keeps selected network after restart");
        CHECK(ink_text_type() == INK_TEXT_PASSWORD && !ink_text_value()[0], "password draft is never restored");
        native_type(WRONG);
        CHECK(BY_HAND("keyboard.key", "{\"code\":10}") && g_rc.status == INK_RC_DONE, "person submits wrong password after reboot");
        checkpoint("BootJoinFail");
    } else if (!strcmp(phase, "joining")) {
        CHECK(ready("BootJoinFail"), "failure page survives restart");
        press("重新输入"); CHECK(ready("BootWifiPass"), "resume failure through protected input");
        native_type(SECRET);
        CHECK(BY_HAND("keyboard.key", "{\"code\":10}") && g_rc.status == INK_RC_DONE, "person connects after reboot");
        checkpoint("BootJoining");
    } else if (!strcmp(phase, "offline")) {
        CHECK(ready("BootWifiPass") && strstr(g_snap, "test-wifi") && !ink_text_value()[0], "interrupted join resumes protected input without a saved password draft");
        native_type(SECRET);
        CHECK(BY_HAND("keyboard.key", "{\"code\":10}") && g_rc.status == INK_RC_DONE, "person retries interrupted connection");
        checkpoint("BootNoHub");
    } else if (!strcmp(phase, "claim")) {
        CHECK(ready("BootNoHub"), "offline page survives restart");
        CHECK(BY_HAND("pairing.configure", CONFIG) && g_rc.status == INK_RC_DONE, "configure synthetic server after reboot");
        checkpoint("BootClaim");
    } else if (!strcmp(phase, "confirm")) {
        CHECK(ready("BootClaim"), "claim page survives restart");
        CHECK(ink_pairing_state() == INK_PAIR_PAIRING, "pairing reconnects without becoming authorized before confirmation");
        vtk_event_at(vtk_now() + SIM_MS(1), offer_event, NULL); checkpoint("BootConfirm");
    } else if (!strcmp(phase, "restore")) {
        CHECK(ready("BootConfirm") && strstr(g_snap, "测试家庭"), "pending invitation is reconstructed from pairing service");
        CHECK(AI("pairing.accept", "{\"txid\":\"test-tx-1\"}") && g_rc.status != INK_RC_DONE, "reboot never permits AI confirmation");
        press("加入");
        CHECK(WAIT_FOR(at("BootRestore") && strstr(g_snap, "还没有可恢复"), 20000), "restored invitation completes through real services");
        checkpoint("BootRestore");
    } else if (!strcmp(phase, "hello")) {
        CHECK(WAIT_FOR(at("BootRestore") && strstr(g_snap, "还没有可恢复"), 20000), "restore page queries backups again after reboot");
        press("当新设备用"); checkpoint("BootHello");
    } else if (!strcmp(phase, "complete")) {
        CHECK(ready("BootHello"), "welcome page survives restart until person leaves it");
        press("跳过"); checkpoint("home");
    } else if (!strcmp(phase, "done")) {
        CHECK(ready("home") && !sim_provision_active(), "completed setup does not restart on the next boot");
        checkpoint("home");
    } else if (!strcmp(phase, "revoke")) {
        CHECK(ready("home"), "revocation starts from the completed device");
        vtk_event_at(vtk_now() + SIM_MS(1), revoke_event, NULL); delay_ms(10); ink_hub_reconnect();
        checkpoint("BootRevoked");
    } else if (!strcmp(phase, "revoked")) {
        CHECK(ready("BootRevoked"), "revoked page survives reboot without exposing a former invitation");
        press("重新加入"); checkpoint("FirstBoot");
    } else CHECK(false, "unknown reboot phase");
}
static void run(void *arg) {
    (void)arg;
    CHECK(WAIT_FOR(settled(), 15000), "initial page settles");
    vtk_event_at(vtk_now() + SIM_MS(1), environment, NULL); delay_ms(10);
    const char *phase = getenv("INK_BOOT_PHASE");
    if (phase) { resume_run(phase); return; }
    ink_db_t *members = ink_db_open(ink_db_find_def("member.table"), NULL);
    CHECK(members && ink_db_count(members) == 0 && !ink_pairing_enabled(), "real first boot has no saved members or pairing");
    if (members) ink_db_close(members);
    CHECK(ready("FirstBoot"), "initial entry is FirstBoot before creating any backup fixture");
    if (restore_mode) {
        CHECK(ink_fs_write_atomic("/me/boot-note.txt", "synthetic saved note", 20) == INK_FS_OK, "create synthetic member backup data");
        CHECK(AI("backup.now", NULL) && g_rc.status == INK_RC_QUEUED, "create a real backup fixture");
        CHECK(WAIT_FOR(ink_hub_online(), 20000), "backup request opens the on-demand synthetic hub connection");
        CHECK(WAIT_FOR(backup_phase("\"phase\":\"saved\""), 60000), "backup fixture received by simulated hub");
    }
    capture("FirstBoot");
    CHECK(sim_provision_active(), "FirstBoot starts the actual modeled setup hotspot");
    press("在设备上设置");
    CHECK(WAIT_FOR(at("BootWifi") && strstr(g_snap, "test-wifi"), 8000), "Wi-Fi scan returns an actual environment network");
    capture("BootWifi"); press("test-wifi"); capture("BootWifiPass");
    if (probe == 1) { ink_probe_count("test.finished"); return; }
    CHECK(AI("text.insert", "{\"text\":\"test-ai-injection\"}") && g_rc.status != INK_RC_DONE, "AI cannot fill a Wi-Fi password");
    CHECK(AI("text.submit", "{}") && g_rc.status != INK_RC_DONE, "AI cannot submit the person's password");
    native_type(WRONG); on_ui(password_snapshot);
    CHECK(AI("text.state", NULL) && !strstr(g_result, WRONG), "text state masks typed password");
    CHECK(BY_HAND("keyboard.key", "{\"code\":10}") && g_rc.status == INK_RC_DONE, "human submits incorrect password");
    capture("BootJoinFail"); press("重新输入"); CHECK(ready("BootWifiPass"), "wrong-password branch can return to protected input");
    native_type(WRONG);
    CHECK(BY_HAND("keyboard.key", "{\"code\":10}") && g_rc.status == INK_RC_DONE, "person retries an incorrect password");
    CHECK(ready("BootJoining") && !strstr(g_snap, "密码不对"), "retry replaces the old password error with the current connection state");
    CHECK(ready("BootJoinFail"), "failed retry remains recoverable"); press("换个网络");
    CHECK(WAIT_FOR(at("BootWifi") && strstr(g_snap, "test-wifi"), 8000), "failure returns to live network list");
    press("其他网络"); CHECK(ready("BootWifiPass"), "other network uses the short input page");
    CHECK(BY_HAND("clipboard.write", "{\"text\":\"test-wifi\"}") && g_rc.status == INK_RC_DONE &&
          BY_HAND("keyboard.edit", "{\"action\":4}") && g_rc.status == INK_RC_DONE, "person pastes a complete synthetic network name");
    CHECK(BY_HAND("keyboard.key", "{\"code\":10}") && g_rc.status == INK_RC_DONE, "person submits an actual network name");
    CHECK(ready("BootWifiPass"), "return to password field for the entered network");
    on_ui(password_snapshot);
    native_type(SECRET); on_ui(password_snapshot);
    CHECK(BY_HAND("keyboard.key", "{\"code\":10}") && g_rc.status == INK_RC_DONE, "human submits correct password");
    capture("BootJoining"); capture("BootNoHub");
    press("先用起来"); CHECK(ready("BootHello"), "no-hub branch can exit to usable device");
    CHECK(BY_HAND("shell.launch", "{\"id\":\"boot\"}") && g_rc.status == INK_RC_DONE, "return to setup after offline exit");
    CHECK(BY_HAND("pairing.configure", CONFIG) && g_rc.status == INK_RC_DONE, "synthetic provisioning sets public server endpoint and key");
    capture("BootClaim");
    on_ui(read_screen); CHECK(!strstr(g_snap, "https://example.com/claim/") && !strstr(g_snap, "CEIRCEIRCEIR"),
        "page snapshot masks displayed joining codes");
    CHECK(BY_HAND("pairing.status", NULL) && strstr(g_result, "https://example.com/claim/") && strstr(g_result, "CEIRCEIRCEIR"),
        "person's pairing result supplies the actual URL and peer verification code");
    vtk_event_at(vtk_now() + SIM_MS(1), offer_event, NULL);
    capture("BootConfirm");
    if (probe == 2) { ink_probe_count("test.finished"); return; }
    CHECK(AI("pairing.accept", "{\"txid\":\"test-tx-1\"}") && g_rc.status != INK_RC_DONE && ink_pairing_state() == INK_PAIR_PAIRING,
        "only the person can confirm joining the household");
    press("不是我们家"); CHECK(ready("BootClaim"), "rejecting offer returns to claim page");
    vtk_event_at(vtk_now() + SIM_MS(1), offer_event, NULL); CHECK(ready("BootConfirm"), "new offer can be reviewed again");
    press("加入");
    CHECK(WAIT_FOR(ink_pairing_state() == INK_PAIR_BOUND && ink_hub_online(), 25000), "human confirmation reaches real bound state");
    if (restore_mode) {
        CHECK(WAIT_FOR(at("BootRestore") && strstr(g_snap, "放回来"), 60000), "actual restore preview enables restore action");
        capture("BootRestore");
        press("放回来");
        ink_confirm_view_t view;
        CHECK(WAIT_FOR(ink_confirm_list(&view, 1) == 1, 5000) && !strcmp(view.cap, "restore.apply"), "restore displays the real overwrite confirmation");
        CHECK(ink_confirm_decide(view.token, true, INK_SRC_AI) != 0, "AI cannot authorize restore");
        CHECK(ink_confirm_decide(view.token, true, INK_SRC_TOUCH) == 0, "person approves restore through existing confirmation policy");
        CHECK(WAIT_FOR(backup_phase("\"phase\":\"done\""), 60000), "restore service actually finishes");
    } else {
        CHECK(WAIT_FOR(at("BootRestore") && strstr(g_snap, "还没有可恢复"), 15000), "missing backup has a real empty state");
        capture("BootRestore"); press("当新设备用");
    }
    capture("BootHello");
    vtk_event_at(vtk_now() + SIM_MS(1), revoke_event, NULL); delay_ms(10); ink_hub_reconnect();
    capture("BootRevoked");
    vtk_event_at(vtk_now() + SIM_MS(1), reset_fault, NULL); delay_ms(10);
    /* Network and family server are still known, so setup skips FirstBoot and shows the new join code
     * (system-ui-spec §7.1); a tap on the FirstBoot it passes through would race that code. */
    press("重新加入"); CHECK(ready("BootClaim"), "revoked device returns to setup with a new join code");
    press("返回选网"); CHECK(ready("BootWifi"), "revoked retry can return to Wi-Fi selection");
    CHECK(BY_HAND("system.home", NULL) && g_rc.status == INK_RC_DONE, "leave setup before member switch");
    CHECK(ink_member_switch("kid") == 0, "switch to synthetic child");
    CHECK(AI("shell.launch", "{\"id\":\"boot\"}") && g_rc.status == INK_RC_REJECTED, "child cannot enter household setup");
    ink_probe_count("test.snapshot.settled"); ink_probe_count("test.finished");
}
void ink_main(void) {
    const char *mode = getenv("INK_BOOT_MODE"); restore_mode = mode && !strcmp(mode, "restore");
    probe = mode && !strcmp(mode, "probe-password") ? 1 : mode && !strcmp(mode, "probe-offer") ? 2 : 0;
    product_main();
    osal_task_cfg_t cfg = {.name = "boot-flow-test", .fn = run, .stack_bytes = 16384,
        .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start synthetic boot flow driver");
}
