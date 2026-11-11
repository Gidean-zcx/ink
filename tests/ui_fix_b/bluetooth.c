#include "common.h"
#ifndef INK_TEST_SOURCE
#define INK_TEST_SOURCE "../../system/ui/shell/bluetooth_page.c"
#endif
#define key test_source_key
#include INK_TEST_SOURCE
#undef key

static void test_devices(void) {
    B.count = 2;
    snprintf(B.devices[0].name, sizeof B.devices[0].name, "合成已配对耳机");
    snprintf(B.devices[0].addr, sizeof B.devices[0].addr, "00:11:22:33:44:55"); B.devices[0].paired = true;
    snprintf(B.devices[1].name, sizeof B.devices[1].name, "合成发现耳机");
    snprintf(B.devices[1].addr, sizeof B.devices[1].addr, "00:11:22:33:44:66"); B.devices[1].paired = false;
    draw();
    CHECK(test_label(B.body, "已配对") && test_label(B.body, "发现的设备"), "UX24 paired and discovered devices have separate headings");
    CHECK(test_fits(test_label(B.body, "忘记")), "UX24 paired row has a visible forget action");
}
static void test_connecting(void) {
#ifndef INK_UI_FIX_BASELINE
    B.devices[0].connecting = true;
#endif
    B.busy = true; draw();
    CHECK(test_label(B.body, "连接中"), "UX24 pending connection has a separate group");
    CHECK(test_label(B.body, "正在配对并连接"), "UX24 pending row describes its operation");
}
static void test_empty(void) {
    B.busy = false; B.count = 0; B.position.page = 0; draw();
    CHECK(test_fits(test_label(B.body, "搜索耳机")), "UX24 empty list has a visible manual scan");
}
static void test_run(void *arg) {
    (void)arg; test_init(); CHECK(AI("shell.bluetooth", NULL), "open Bluetooth"); test_quiet();
    on_ui(test_devices); test_frame("bluetooth-groups"); on_ui(test_connecting); test_frame("bluetooth-connecting");
    on_ui(test_empty); test_frame("bluetooth-empty"); ink_probe_count("test.finished");
}
