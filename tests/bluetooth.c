/* Bluetooth controls through the real presenters, policy and headset model. */
#include "ink/ui_shell.h"
#define ink_main product_main
#include "../system/boot/main.c"
#undef ink_main
#include "integration/whole_product.h"
#include <stdlib.h>

#define ADDR "00:11:22:33:44:55"
#define ARGS "{\"addr\":\"" ADDR "\"}"
static char snapshot[4096];
static const char *wanted;
static int x, y;
static bool restarting;
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void quiet(void) { CHECK(WAIT_FOR(settled(), 10000), "display settles"); }
static bool connected(void) {
    int64_t v = 0; ink_state_get_int(ink_state_find("bt.connected"), &v); return v != 0;
}
static void read_page(void) {
    CHECK(ink_ui_snapshot(snapshot, sizeof snapshot, NULL) > 0, "page exports a complete snapshot");
    CHECK(ink_ui_context_snapshot(g_snap, sizeof g_snap, NULL) > 0, "page fits the real AI context");
}
static bool page_has(const char *text) { on_ui(read_page); return strstr(snapshot, text) != NULL; }
static void find_at(lv_obj_t *o) {
    if (lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN)) return;
    if (lv_obj_check_type(o, &lv_label_class) && !strcmp(lv_label_get_text(o), wanted)) {
        lv_area_t a; lv_obj_update_layout(o); lv_obj_get_coords(o, &a);
        x = (a.x1 + a.x2) / 2; y = (a.y1 + a.y2) / 2;
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) find_at(lv_obj_get_child(o, i));
}
static void find(void) { x = y = -1; find_at(lv_screen_active()); }
static bool locate(const char *text) {
    wanted = text; on_ui(find);
    for (int i = 0; x < 0 && i < 2 && !strcmp(ink_ui_page_name(), "SetBluetooth"); i++) {
        AI("shell.bluetooth_next", NULL); quiet(); on_ui(find);
    }
    return x >= 0 && y >= 0 && y < INK_CANVAS_H;
}
static void press(const char *text) {
    CHECK(locate(text), "visible touch target: %s", text);
    if (x >= 0) tap(x, y);
    quiet();
}
static void world(void *arg) { sim_headset_power(ADDR, arg != NULL); }
static void headset(bool on) {
    vtk_event_at(vtk_now() + SIM_MS(1), world, (void *)(uintptr_t)on); delay_ms(20);
}
static void capture(void *arg) { sim_display_snapshot(arg); }
static void save_snapshot(void *arg) {
    char path[600]; int n = snprintf(path, sizeof path, "%s/%s.json", g_sim.out_dir, (const char *)arg);
    if (n < 0 || (size_t)n >= sizeof path) abort();
    FILE *f = fopen(path, "wb");
    if (!f || fputs(snapshot, f) == EOF || fclose(f)) abort();
}
static void frame(const char *name) {
    quiet(); on_ui(read_page); vtk_host_call(save_snapshot, (void *)name);
    vtk_event_at(vtk_now() + SIM_MS(1), capture, (void *)name); delay_ms(50);
    if (!strcmp(name, "SetBluetooth")) {
        ink_probe_count("test.snapshot.settled"); ink_probe_count("test.snapshot.SetBluetooth");
    }
}
static void slow(void *arg) { (void)arg; delay_ms(11000); }
static ink_exec_job_t delay_job;
static void read_env(void *arg);
static void run(void *arg) {
    (void)arg;
    vtk_host_call(read_env, NULL);
    CHECK(WAIT_FOR(ink_member_space_open(), 10000), "member space opens"); test_start_home(); quiet();
    if (restarting) {
        CHECK(!ink_setting_get("bt.enabled"), "disabled setting survives a fresh process");
        CHECK(AI("bt.scan", NULL) && g_rc.status == INK_RC_REJECTED, "restart does not scan while off");
        ink_probe_count("test.finished"); return;
    }
    CHECK(AI("shell.settings", NULL) && g_rc.status == INK_RC_DONE, "open settings"); quiet();
    press("蓝牙"); CHECK(!strcmp(ink_ui_page_name(), "SetBluetooth"), "settings row opens Bluetooth");
    CHECK(page_has("bt.enabled") && page_has("bt.scan") && page_has("bt.forget"), "snapshot exposes switch, search and forget");
    press("蓝牙");
    CHECK(!ink_setting_get("bt.enabled"), "touch switches Bluetooth off");
    CHECK(AI("bt.scan", NULL) && g_rc.status == INK_RC_REJECTED, "disabled gate rejects scanning");
    CHECK(AI("bt.pair", ARGS) && g_rc.status == INK_RC_REJECTED, "disabled gate rejects pairing");
    CHECK(AI("bt.connect", ARGS) && g_rc.status == INK_RC_REJECTED, "disabled gate rejects connecting");
    CHECK(AI("settings.set", "{\"id\":\"bt.enabled\",\"value\":1}") && g_rc.status == INK_RC_DONE, "AI uses the same switch setting"); quiet();
    headset(false); press("搜索耳机");
    CHECK(WAIT_FOR(page_has("没有搜到耳机"), 5000), "empty search has an explicit state"); frame("empty");
    headset(true); press("搜索耳机");
    CHECK(WAIT_FOR(page_has("我的耳机"), 5000), "search lists the model headset");
    headset(false); press("我的耳机");
    CHECK(WAIT_FOR(page_has("配对失败"), 5000) && !connected(), "pairing failure is visible and not connected"); frame("pair-failed");
    headset(true); press("我的耳机");
    CHECK(WAIT_FOR(connected(), 7000), "a discovered row pairs and connects");
    CHECK(AI("bt.list", NULL) && strstr(g_result, ADDR), "paired headset is stored"); quiet();
    CHECK(page_has("已连接"), "connected row reports the actual link"); frame("SetBluetooth");
    press("我的耳机"); CHECK(WAIT_FOR(!connected(), 3000), "paired row disconnects");
    press("我的耳机"); CHECK(WAIT_FOR(connected(), 5000), "paired row reconnects");
    press("蓝牙"); CHECK(WAIT_FOR(!connected(), 3000) && !ink_setting_get("bt.enabled"), "switching off disconnects the headset");
    CHECK(AI("settings.set", "{\"id\":\"bt.enabled\",\"value\":1}") && g_rc.status == INK_RC_DONE, "enable again"); quiet();
    CHECK(locate("我的耳机"), "paired headset remains in the list while disconnected");
    hw(HW_PRESS, x, y); delay_ms(1200); hw(HW_RELEASE, x, y); quiet();
    CHECK(WAIT_FOR(page_has("已完成"), 5000), "long press finishes forgetting");
    CHECK(AI("bt.list", NULL) && !strstr(g_result, ADDR), "forget removes the saved headset");
    CHECK(AI("bt.connect", ARGS) && g_rc.status == INK_RC_FAILED, "forgotten headset cannot connect without pairing");
    press("搜索耳机");
    CHECK(WAIT_FOR(page_has("我的耳机"), 5000), "find the headset again for visible forget");
    press("我的耳机"); CHECK(WAIT_FOR(connected(), 7000), "pair again before explicit forget");
    quiet(); press("忘记");
    CHECK(WAIT_FOR(page_has("已完成"), 5000), "visible forget button completes without a long press");
    CHECK(AI("bt.list", NULL) && !strstr(g_result, ADDR), "visible forget removes the saved headset");
    quiet(); press("搜索耳机");
    CHECK(AI("settings.set", "{\"id\":\"bt.enabled\",\"value\":0}") && g_rc.status == INK_RC_DONE, "disable during a scan");
    CHECK(WAIT_FOR(page_has("蓝牙已关闭"), 5000), "in-flight scan is discarded after disabling");
    CHECK(!page_has("我的耳机"), "disabled scan leaves no discovered row");
    CHECK(AI("settings.set", "{\"id\":\"bt.enabled\",\"value\":1}") && g_rc.status == INK_RC_DONE, "enable for timeout test"); quiet();
    CHECK(ink_exec_post(INK_EXEC_BG, &delay_job, slow, NULL) == 0, "queue realistic background latency");
    press("搜索耳机");
    CHECK(WAIT_FOR(page_has("操作超时"), 13000), "scan deadline has an explicit timeout state"); frame("timeout");
    CHECK(AI("bt.list", NULL), "background queue drains after timeout");
    CHECK(AI("system.home", NULL), "return home before sound entry");
    CHECK(AI("shell.sound", NULL), "open sound page"); quiet(); press("蓝牙耳机");
    CHECK(!strcmp(ink_ui_page_name(), "SetBluetooth"), "sound row opens Bluetooth");
    CHECK(AI("system.home", NULL), "return home before phone entry");
    CHECK(BY_HAND("call.dial", "{\"number\":\"10086\"}") && g_rc.status == INK_RC_DONE, "dial a synthetic simulator call"); quiet();
    CHECK(!strcmp(ink_ui_page_name(), "PhoneCall"), "real call page is visible"); press("耳机未连");
    CHECK(!strcmp(ink_ui_page_name(), "SetBluetooth"), "unconnected phone headset button opens Bluetooth");
    CHECK(BY_HAND("call.hangup", NULL), "hang up the simulated call");
    CHECK(AI("system.home", NULL), "return home before child policy"); quiet();
    CHECK(WAIT_FOR(ink_storage_pending() == 0, 10000), "writes settle before member change");
    CHECK(ink_member_switch("kid") == 0, "enter child space"); quiet();
    CHECK(AI("shell.settings", NULL) && g_rc.status == INK_RC_REJECTED, "general settings stays hidden from children");
    CHECK(AI("shell.bluetooth", NULL) && g_rc.status == INK_RC_DONE, "child can open headset controls as existing bt capabilities allow"); quiet();
    CHECK(AI("settings.set", "{\"id\":\"bt.enabled\",\"value\":0}") && g_rc.status == INK_RC_DONE, "child can change the matching headset switch"); quiet();
    frame("child");
    CHECK(WAIT_FOR(ink_storage_pending() == 0, 10000), "settings persist before restart"); delay_ms(1500);
    ink_probe_count("test.finished");
}
static void read_env(void *arg) {
    (void)arg;
    const char *value = getenv("INK_BT_RESTART"); restarting = value && !strcmp(value, "1");
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name = "bttest", .fn = run, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL,
        .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    if (osal_task_create(&cfg, NULL) != OSAL_OK) osal_panic("Bluetooth test");
}
