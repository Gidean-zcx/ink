/* Production presenters and synthetic member/device data. */
#include <stdlib.h>
#ifndef INK_GAPS_SETTINGS_SOURCE
#define INK_GAPS_SETTINGS_SOURCE "../../system/ui/shell/settings_pages.c"
#endif
#define key settings_page_key
#include INK_GAPS_SETTINGS_SOURCE
#undef key
#define ink_main product_main
#include "../../system/boot/main.c"
#undef ink_main
#include "../integration/whole_product.h"

static char test_snap[4096], test_name[80];
static const char *test_label;
static lv_obj_t *test_found;
static int test_lang;
static bool test_captured;
static bool test_settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void test_quiet(void) { CHECK(WAIT_FOR(test_settled(), 12000), "page settles"); }
static void test_snapshot(void) {
    CHECK(ink_ui_snapshot(test_snap, sizeof test_snap, NULL) > 0, "visible page has a complete snapshot");
    CHECK(ink_ui_context_snapshot(g_snap, sizeof g_snap, NULL) > 0, "page fits the actual AI context");
}
static void test_find(lv_obj_t *o) {
    if (lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN)) return;
    if (lv_obj_check_type(o, &lv_label_class) && !strcmp(lv_label_get_text(o), test_label)) test_found = o;
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) test_find(lv_obj_get_child(o, (int32_t)i));
}
static void test_locate(void) {
    test_found = NULL; lv_obj_update_layout(lv_screen_active()); lv_obj_update_layout(lv_layer_sys());
    test_find(lv_screen_active()); test_find(lv_layer_sys());
    if (test_found) {
        lv_area_t a; lv_obj_get_coords(test_found, &a); g_btn_x[0] = (a.x1 + a.x2) / 2; g_btn_y[0] = (a.y1 + a.y2) / 2;
    }
}
static void test_click(const char *text) {
    test_label = text; on_ui(test_locate);
    for (int i = 0; !test_found && i < 8; i++) {
        CHECK(AI("shell.settings_next", NULL), "find settings row on later pages"); test_quiet(); on_ui(test_locate);
    }
    CHECK(test_found, "visible control %.120s", text);
    if (test_found) tap(g_btn_x[0], g_btn_y[0]);
    test_quiet();
}
static void test_save(void *ctx) {
    (void)ctx; char path[640];
    int n = snprintf(path, sizeof path, "%s/%s.json", g_sim.out_dir, test_name);
    if (n < 0 || (size_t)n >= sizeof path) abort();
    FILE *f = fopen(path, "w");
    if (!f || fputs(test_snap, f) == EOF || fclose(f)) abort();
}
static void test_frame(void *ctx) { sim_display_snapshot(ctx); test_captured = true; }
static void test_capture(const char *name) {
    test_quiet(); on_ui(test_snapshot); snprintf(test_name, sizeof test_name, "%s", name); vtk_host_call(test_save, NULL);
    test_captured = false; vtk_event_at(vtk_now() + SIM_MS(1), test_frame, test_name);
    CHECK(WAIT_FOR(test_captured, 1000), "capture the actual settled panel");
}
static void test_open(const char *cap, const char *name) {
    CHECK(AI(cap, NULL) && g_rc.status == INK_RC_DONE, "AI opens %.80s", name); test_quiet();
    CHECK(!strcmp(ink_ui_page_name(), name), "requested page is current");
}
static void test_details(void) {
    test_snapshot();
    hal_device_info_t info; hal_device_get_info(&info);
    CHECK(strstr(test_snap, info.display_name), "model is the HAL value");
    char version[64]; ink_state_get_str(ink_state_find("update.version"), version, sizeof version);
    CHECK(strstr(test_snap, version), "version is the update service value");
    CHECK(strstr(test_snap, "\"id\":\"battery.level\"") && strstr(test_snap, "73%"), "battery is the power service value");
    CHECK(strstr(test_snap, "\"id\":\"storage.total\"") && S.total > 0, "storage capacity is available from file.free");
    char size[32]; storage_size(S.total, size, sizeof size);
    CHECK(strstr(test_snap, size), "displayed total matches file.free");
    CHECK(strstr(test_snap, test_lang ? "4.0 GB free" : "还能放 4.0 GB"), "free space is the current service value");
    CHECK(!strstr(test_snap, "synthetic-serial") && !strstr(g_snap, "synthetic-serial") && !strstr(test_snap, "00:11:22:33:44:55"), "device identifiers never enter either snapshot");
}
static void test_state(void) {
    static const ink_state_def_t serial = {.id = "device.serial", .type = INK_ST_STR};
    static const ink_state_def_t mac = {.id = "device.mac", .type = INK_ST_STR};
    ink_state_set_str(ink_state_define(&serial), "synthetic-serial"); ink_state_set_str(ink_state_define(&mac), "00:11:22:33:44:55");
    ink_state_set_int(ink_state_find("battery.level"), 73);
}
static void test_all_license_chunks(void) {
    for (size_t i = 0; i < sizeof LICENSE_TEXTS / sizeof *LICENSE_TEXTS; i++) {
        L.index = (int)i; size_t start = 0, n;
        while ((n = license_chunk(start))) {
            CHECK(!memcmp(L.text, LICENSE_TEXTS[i] + start, n), "license chunk preserves every source byte");
            CHECK(ink_ui_text_height(L.text, INK_FONT_BODY, INK_WEIGHT_REGULAR, INK_CANVAS_W - 2 * INK_MARGIN) <= L.height, "legal text fits at the selected font size");
            start += n;
        }
        CHECK(start == strlen(LICENSE_TEXTS[i]), "all of license %zu is reachable", i);
    }
    L.index = 0;
    size_t start = 0; for (size_t k = 0; k < L.position.page; k++) start += license_chunk(start);
    license_chunk(start);
}
static void test_env(void *ctx) { (void)ctx; test_lang = getenv("INK_GAPS_EN") != NULL; }
static void test_battery_control(void) {
    test_label = test_lang ? "Battery Saver: lock after 1 min" : "省电：1 分钟锁屏"; on_ui(test_locate);
    CHECK((test_found != NULL) == (ink_member_current()->role != INK_ROLE_CHILD), "battery controls match the current member");
    on_ui(read_screen);
    CHECK((strstr(g_snap, "power.set_saving") != NULL) == (ink_member_current()->role != INK_ROLE_CHILD), "battery snapshot matches its controls");
}
static void run(void *ctx) {
    (void)ctx; vtk_host_call(test_env, NULL);
    CHECK(WAIT_FOR(ink_member_space_open(), 10000), "member space opens"); test_start_home(); test_quiet();
    if (test_lang) CHECK(AI("settings.set", "{\"id\":\"system.language\",\"value\":1}"), "select English");
    CHECK(ink_setting_get("voice.interrupt") == 1, "interrupt defaults on");
    test_open("shell.voice", "SetVoice"); test_capture("voice");
    test_click(test_lang ? "Interrupt spoken answers" : "说话能打断回答");
    CHECK(ink_setting_get("voice.interrupt") == 0, "real toggle uses settings.set");
    CHECK(AI("settings.set", "{\"id\":\"voice.interrupt\",\"value\":1}") && ink_setting_get("voice.interrupt") == 1, "AI uses the same reversible setting");
    on_ui(test_state); test_open("shell.about", "SetAbout"); test_click(test_lang ? "Device Details" : "设备详情");
    CHECK(!strcmp(ink_ui_page_name(), "DeviceDetails"), "about entry opens device details by touch");
    CHECK(WAIT_FOR(S.total > 0, 3000), "asynchronous file.free reply arrives"); test_quiet(); on_ui(test_details); test_capture("device");
    CHECK(AI("shell.back", NULL), "back returns to About"); test_quiet();
    test_click(test_lang ? "Licenses" : "开源许可");
    CHECK(!strcmp(ink_ui_page_name(), "Licenses"), "about entry opens licenses by touch"); test_capture("licenses");
    CHECK(strstr(test_snap, "LVGL") && strstr(test_snap, "9.5.0") && strstr(test_snap, "MIT"), "list has a registered component, version and license");
    test_click("LVGL"); CHECK(!strcmp(ink_ui_page_name(), "LicenseText"), "component row opens full text"); test_capture("license-text");
    CHECK(strstr(test_snap, "Source:") && strstr(test_snap, "Copyright"), "full notice includes source and copyright");
    char *all = osal_calloc(1, strlen(LICENSE_TEXTS[0]) + 1, OSAL_MEM_EXTERNAL); size_t at = 0;
    CHECK(all, "allocate test reconstruction in PSRAM");
    if (all) for (;;) {
        on_ui(test_snapshot); size_t n = strlen(L.text); memcpy(all + at, L.text, n); at += n;
        if (at >= strlen(LICENSE_TEXTS[0])) break;
        size_t before = L.position.page; key(HAL_KEY_NEXT); test_quiet();
        CHECK(L.position.page == before + 1, "physical next key turns the legal text");
        if (L.position.page == before) break;
    }
    CHECK(all && !strcmp(all, LICENSE_TEXTS[0]), "paged full text is lossless"); osal_free(all);
    CHECK(AI("shell.settings_prev", NULL), "AI can turn the same legal text back"); test_quiet();
    CHECK(AI("settings.set", "{\"id\":\"display.font_size\",\"value\":3}"), "use largest legal-text size"); test_quiet(); on_ui(test_all_license_chunks);
    test_capture("license-large");
    CHECK(AI("settings.set", "{\"id\":\"display.font_size\",\"value\":1}"), "restore font size"); test_quiet();
    CHECK(BY_HAND("system.home", NULL), "leave settings before battery test"); test_quiet();
    hw(HW_BATTERY, 5, 0); ink_power_battery_policy_start();
    CHECK(WAIT_FOR(!strcmp(active_state(), "low_battery"), 3000), "real battery service raises the low-battery page"); test_quiet(); test_battery_control(); test_capture("battery-adult");
    CHECK(ink_member_switch("kid") == 0, "switch to synthetic child while low-battery page stays open"); test_quiet(); test_battery_control(); test_capture("battery-child");
    CHECK(ink_member_switch("owner") == 0, "return to adult with battery page open"); test_quiet(); test_battery_control();
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name = "small-gaps-pages", .fn = run, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start page acceptance test");
}
