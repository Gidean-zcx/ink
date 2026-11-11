#include "common.h"
#ifndef INK_TEST_SOURCE
#define INK_TEST_SOURCE "../../system/ui/shell/settings_pages.c"
#endif
#define key test_source_key
#include INK_TEST_SOURCE
#undef key

static void test_voice(void) {
    ink_ui_field_spec_t f; ink_ui_action_spec_t a;
    lv_obj_t *slot = ink_ui_box(S.body, 0, 0, INK_CANVAS_W, INK_PRIMARY, 0, INK_SURFACE_WHITE, false);
    bind_row(slot, 0, &f, &a, NULL);
    CHECK(!hal_afe_available() && !a.enabled && strstr(f.value, "不支持"), "UX08 unsupported wake mode is disabled with its reason");
    CHECK(!test_label(slot, "使用时"), "UX08 unsupported wake mode has no live segments");
    lv_obj_delete(slot);
}
static void test_storage_failure(void) {
    U.generation = S.generation; U.state[0] = U.state[1] = 2; refresh();
    ink_ui_action_spec_t a = {0}; storage_action(2, &a);
    CHECK(a.enabled && a.capability && !strcmp(a.capability, "shell.storage"), "UX08 storage failure offers a read retry");
    value(2); CHECK(strstr(S.values[2], "再试一次"), "UX08 failed storage tells the person how to retry");
}
static void test_backup(void) {
    value(0); CHECK(strstr(S.values[0], "每天"), "UX08 backup row shows the automatic backup value");
}
static void test_voice_paging(void) {
    lv_obj_t *label = test_label(S.body, "AI 记得的事");
    CHECK(label && !lv_obj_has_state(lv_obj_get_parent(label), LV_STATE_DISABLED), "UX08 recycled rows restore enabled AI actions");
}
static void test_about(void) {
    value(3); CHECK(strstr(S.values[3], "中文"), "UX08 language row shows the current language");
    for (S.position.page = 0; S.position.page * S.capacity < PAGES[ABOUT].count; S.position.page++) {
        refresh(); lv_obj_update_layout(S.body);
        lv_obj_t *reset = test_label(S.body, "清空这台设备");
        if (reset) {
            CHECK(test_fits(reset), "UX08 reset action fits at both font sizes");
            CHECK(test_fits(test_label(S.body, "清掉所有家人的数据，找不回来")), "UX08 reset consequence fits at both font sizes");
            break;
        }
    }
}
static void test_run(void *arg) {
    (void)arg; test_init();
    CHECK(AI("shell.voice", NULL), "open voice settings"); test_quiet(); on_ui(test_voice); test_frame("voice");
    CHECK(AI("shell.settings_next", NULL), "turn past disabled voice rows"); test_quiet(); on_ui(test_voice_paging);
    CHECK(AI("settings.set", "{\"id\":\"backup.auto\",\"value\":1}"), "set synthetic backup preference");
    CHECK(AI("shell.storage", NULL), "open storage settings"); test_quiet(); on_ui(test_backup); test_frame("storage");
    on_ui(test_storage_failure); test_frame("storage-error");
    CHECK(AI("shell.about", NULL), "open About"); test_quiet(); test_frame("about"); on_ui(test_about); test_frame("about-reset");
    ink_probe_count("test.finished");
}
