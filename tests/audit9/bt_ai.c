/* AI setting changes must reach both the page snapshot and its labels. */
#include "ink/ui_shell.h"
#define ink_main product_main
#include "../../system/boot/main.c"
#undef ink_main
#include "../integration/whole_product.h"

static char page[4096];
static bool off_label;
static bool connected(void) {
    int64_t value = 0;
    ink_state_get_int(ink_state_find("bt.connected"), &value);
    return value != 0;
}
static void labels(lv_obj_t *obj) {
    if (!lv_obj_is_visible(obj)) return;
    if (lv_obj_check_type(obj, &lv_label_class) && strstr(lv_label_get_text(obj), "蓝牙已关闭")) off_label = true;
    for (uint32_t i = 0; i < lv_obj_get_child_count(obj); i++) labels(lv_obj_get_child(obj, (int32_t)i));
}
static void inspect(void) {
    lv_obj_update_layout(lv_screen_active());
    CHECK(ink_ui_snapshot(page, sizeof page, NULL) > 0, "Bluetooth snapshot fits");
    CHECK(ink_ui_context_snapshot(g_snap, sizeof g_snap, NULL) > 0, "Bluetooth AI context fits");
    off_label = false;
    labels(lv_screen_active());
}
static void capture(void *arg) { sim_display_snapshot(arg); }
static void frame(const char *name, bool off) {
    CHECK(WAIT_FOR(!ink_ui_busy() && !ink_display_busy(), 10000), "Bluetooth display settles");
    on_ui(inspect);
    CHECK((strstr(page, "蓝牙已关闭") != NULL) == off, "snapshot matches the Bluetooth switch");
    CHECK((strstr(g_snap, "蓝牙已关闭") != NULL) == off, "AI context matches the Bluetooth switch");
    CHECK(off_label == off, "screen matches the Bluetooth switch");
    vtk_event_at(vtk_now() + SIM_MS(1), capture, (void *)name);
    delay_ms(20);
}
static void run(void *arg) {
    (void)arg;
    CHECK(WAIT_FOR(ink_member_space_open(), 10000), "member space opens");
    test_start_home();
    CHECK(AI("shell.bluetooth", NULL) && g_rc.status == INK_RC_DONE, "AI opens Bluetooth");
    CHECK(AI("settings.set", "{\"id\":\"bt.enabled\",\"value\":0}") && g_rc.status == INK_RC_DONE, "AI disables Bluetooth");
    frame("bt-off", true);
    CHECK(AI("settings.set", "{\"id\":\"bt.enabled\",\"value\":1}") && g_rc.status == INK_RC_DONE, "AI enables Bluetooth");
    frame("bt-on", false);
    CHECK(strstr(page, "\"value\":\"开\"") != NULL, "switch reports enabled");
    CHECK(AI("bt.pair", "{\"addr\":\"00:11:22:33:44:55\",\"name\":\"我的耳机\",\"connect\":true}") &&
          g_rc.status == INK_RC_DONE, "AI pairs and connects the synthetic headset");
    CHECK(WAIT_FOR(connected(), 5000), "headset connects");
    frame("bt-connected", false);
    CHECK(strstr(page, "已连接") != NULL, "page reports the actual connection");
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name = "bt-ai-test", .fn = run, .stack_bytes = 16384,
        .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start Bluetooth boundary test");
}
