/* Real presenters, member policy, settings persistence and F1/F4 snapshots. */
#include "ink/ui_shell.h"
#define ink_main product_main
#include "../system/boot/main.c"
#undef ink_main
#include "integration/whole_product.h"
#include <stdlib.h>

static bool restarting;
static const char *wanted, *opening;
static int x, y;
static char snapshot[4096], bound[128];
static const char *const PAGES[] = {"SetVoice", "SetStorage", "SetAbout", "SetPrivacy"};
static const char *const CAPS[] = {"shell.voice", "shell.storage", "shell.about", "shell.privacy"};
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void quiet(void) { CHECK(WAIT_FOR(settled(), 10000), "display settles"); }
static void save_snapshot(void *arg) {
    (void)arg;
    char path[600];
    int n = snprintf(path, sizeof path, "%s/snapshots.ndjson", g_sim.out_dir);
    if (n < 0 || (size_t)n >= sizeof path) abort();
    FILE *f = fopen(path, "ab");
    if (!f || fputs(snapshot, f) == EOF || fputc('\n', f) == EOF || fclose(f)) abort();
}
static void read_snapshot(void) {
    CHECK(ink_ui_snapshot(snapshot, sizeof snapshot, NULL) > 0, "the production page has a complete snapshot");
    CHECK(ink_ui_context_snapshot(g_snap, sizeof g_snap, NULL) > 0, "the page fits the real AI context buffer");
    CHECK(strstr(snapshot, "\"pagination\"") && strstr(snapshot, "\"fields\"") && strstr(snapshot, "\"actions\""),
        "new pages use both F1 semantics and F4 pagination");
    vtk_host_call(save_snapshot, NULL);
}
static void find_label_at(lv_obj_t *o) {
    if (lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN)) return;
    if (lv_obj_check_type(o, &lv_label_class) && !strcmp(lv_label_get_text(o), wanted)) {
        lv_area_t a; lv_obj_update_layout(o); lv_obj_get_coords(o, &a);
        x = (a.x1 + a.x2) / 2; y = (a.y1 + a.y2) / 2;
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) find_label_at(lv_obj_get_child(o, i));
}
static void find_label(void) { x = y = -1; find_label_at(lv_screen_active()); }
static void press(const char *label) {
    wanted = label; on_ui(find_label);
    CHECK(x >= 0 && y >= 0, "visible button: %s", label);
    if (x >= 0 && y >= 0) tap(x, y);
    quiet();
}
static void open_direct(void) { ink_shell_open_page(opening); }
static void inspect_backend(void) {
    read_snapshot();
    CHECK(strstr(snapshot, ink_ui_page_name()), "snapshot names its real activity");
    if (!strcmp(ink_ui_page_name(), "SetVoice"))
        CHECK(strstr(snapshot, "voice.handsfree") && strstr(snapshot, "settings.set"), "voice exposes the existing setting capability");
    if (!strcmp(ink_ui_page_name(), "SetStorage")) {
        int64_t bytes = -1; char expected[64];
        CHECK(ink_state_get_int(ink_state_find("storage.free"), &bytes), "storage backend has a real value");
        snprintf(expected, sizeof expected, "还能放 %lld KiB", (long long)bytes / 1024);
        CHECK(strstr(snapshot, expected), "storage page reads the real service rather than design example sizes");
    }
    if (!strcmp(ink_ui_page_name(), "SetAbout")) {
        char version[64];
        CHECK(ink_state_get_str(ink_state_find("update.version"), version, sizeof version) > 0 && strstr(snapshot, version),
            "about reads the update service's current version");
    }
    if (!strcmp(ink_ui_page_name(), "SetPrivacy"))
        CHECK(strstr(snapshot, "密码框里的字看不到") && strstr(snapshot, "ai.read_clipboard"), "privacy exports its text and live permission");
}
static void extract_action(void) {
    read_snapshot();
    ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL);
    const char *actions = strstr(snapshot, "\"actions\":[");
    const char *start = actions ? strstr(actions, "{\"id\":\"voice.handsfree\"") : NULL;
    const char *end = start ? strstr(start, "}}") : NULL;
    size_t len = end ? (size_t)(end - start) + 2 : 0;
    CHECK(j && len && !ink_json_parse(j, start, len), "the voice action fits the device JSON parser");
    bound[0] = 0;
    if (j && len) {
        CHECK(ink_json_eq(j, ink_json_find(j, 0, "capability"), "settings.set"), "touch binding and AI both target settings.set");
        CHECK(ink_json_str(j, ink_json_find(j, 0, "args_json"), bound, sizeof bound) > 0, "AI gets the same setting arguments");
    }
    osal_free(j);
    CHECK(bound[0], "the setting action is present");
}
static void backend_changes(void) {
    ink_state_set_int(ink_state_find("storage.free"), 4321 * 1024);
    ink_state_set_str(ink_state_find("update.version"), "0.0.7-test");
}
static void unavailable_card(void) {
    ink_state_set_bool(ink_state_find("storage.card"), false);
    ink_state_set_str(ink_state_find("storage.mode"), "usb");
}
static void check_no_card(void) {
    read_snapshot(); CHECK(strstr(snapshot, "存储卡正借给电脑"), "unavailable storage never displays a stale free-space number");
}
static void check_hidden(void) {
    CHECK(!strcmp(ink_ui_page_name(), "home"), "child cannot open a private settings page through the direct presenter path");
}
static void run(void *arg) {
    (void)arg;
    CHECK(WAIT_FOR(ink_member_space_open(), 10000), "member space opens");
    test_start_home(); quiet();
    if (restarting) {
        CHECK(ink_setting_get("voice.handsfree") == 2, "voice setting survives a fresh process and persisted flash");
        CHECK(ink_setting_get("update.auto_download") == 1, "about setting survives restart");
        CHECK(ink_setting_get("ai.read_clipboard") == 1, "confirmed privacy setting survives restart");
    } else {
        for (int i = 0; i < 4; i++) {
            CHECK(AI(CAPS[i], NULL) && g_rc.status == INK_RC_DONE, "AI opens %s", PAGES[i]); quiet();
            CHECK(!strcmp(ink_ui_page_name(), PAGES[i]), "the requested real page opens"); on_ui(inspect_backend);
            CHECK(BY_HAND("shell.back", NULL) && g_rc.status == INK_RC_DONE, "back closes only one page"); quiet();
        }
        CHECK(AI("shell.voice", NULL), "open voice controls"); quiet(); press("什么时候能叫名字");
        CHECK(ink_setting_get("voice.handsfree") == 1, "a real touch uses settings.set");
        on_ui(extract_action);
        CHECK(AI("settings.set", bound) && g_rc.status == INK_RC_DONE && ink_setting_get("voice.handsfree") == 2,
            "AI executes the exact action exported for the same control"); quiet();
        press("使用时"); CHECK(ink_setting_get("voice.handsfree") == 0, "the segment button uses the same settings capability");
        CHECK(AI("settings.set", "{\"id\":\"voice.handsfree\",\"value\":2}") && g_rc.status == INK_RC_DONE, "restore the persisted voice value"); quiet();
        CHECK(AI("shell.settings_next", NULL) && g_rc.status == INK_RC_DONE, "AI uses the F4 next capability"); quiet();
        key(INK_KEY_PREV); quiet(); on_ui(read_snapshot);
        CHECK(strstr(snapshot, "voice.handsfree"), "the physical previous key restores the first F4 page");
        CHECK(BY_HAND("system.home", NULL), "close voice before opening about");
        CHECK(AI("shell.about", NULL), "open about controls"); quiet(); press("自动下载新版本");
        CHECK(ink_setting_get("update.auto_download") == 1, "about toggle reaches the existing backend");
        CHECK(AI("shell.privacy", NULL), "open privacy controls"); quiet(); press("AI 可以读剪贴板");
        ink_confirm_view_t confirmation;
        CHECK(ink_confirm_list(&confirmation, 1) == 1 && !strcmp(confirmation.cap, "settings.set") &&
            ink_setting_get("ai.read_clipboard") == 0, "privacy permission waits for the existing human confirmation");
        CHECK(ink_confirm_decide(confirmation.token, true, INK_SRC_TOUCH) == 0, "a person confirms the permission");
        CHECK(WAIT_FOR(ink_setting_get("ai.read_clipboard") == 1, 5000), "confirmed privacy toggle reaches its backend"); quiet();
        on_ui(backend_changes);
        CHECK(BY_HAND("system.home", NULL), "close privacy");
        CHECK(AI("shell.storage", NULL), "open updated storage"); quiet(); on_ui(inspect_backend);
        on_ui(unavailable_card); quiet(); on_ui(check_no_card);
        CHECK(AI("shell.about", NULL), "open updated version"); quiet(); on_ui(inspect_backend);
        CHECK(BY_HAND("system.home", NULL), "return home before switching members"); quiet();
        CHECK(WAIT_FOR(ink_storage_pending() == 0, 10000), "writes settle before member tests");
        CHECK(ink_member_switch("kid") == 0, "enter the child member space"); quiet();
        for (int i = 0; i < 4; i++) {
            CHECK(AI(CAPS[i], NULL) && g_rc.status == INK_RC_REJECTED, "child AI cannot open %s", PAGES[i]);
            opening = PAGES[i]; on_ui(open_direct); quiet(); on_ui(check_hidden);
            on_ui(ink_ui_home); quiet();
        }
        CHECK(AI("settings.set", "{\"id\":\"voice.handsfree\",\"value\":0}") && g_rc.status == INK_RC_REJECTED,
            "child cannot write the adult voice setting via AI");
        CHECK(BY_HAND("settings.set", "{\"id\":\"update.auto_download\",\"value\":0}") && g_rc.status == INK_RC_REJECTED,
            "child cannot write the adult about setting via touch");
        CHECK(AI("settings.set", "{\"id\":\"ai.read_clipboard\",\"value\":0}") && g_rc.status == INK_RC_REJECTED,
            "child cannot change privacy permission");
        CHECK(ink_setting_get("voice.handsfree") == 2 && ink_setting_get("update.auto_download") == 1 && ink_setting_get("ai.read_clipboard") == 1,
            "child attempts leave all adult settings unchanged");
    }
    CHECK(WAIT_FOR(ink_storage_pending() == 0, 10000), "all accepted writes finish");
    ink_probe_count("test.finished");
}
void ink_main(void) {
    restarting = getenv("INK_SETTINGS_RESTART") != NULL;
    product_main();
    osal_task_cfg_t cfg = {.name = "settings-a-test", .fn = run, .stack_bytes = 16384,
        .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start the test driver");
}
