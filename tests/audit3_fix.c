/* Real bus and pages; bounded synthetic regressions for B3-02 through B3-09. */
#include <stdlib.h>
#include "ink/sys/file.h"
#include "ink/sys/textfield.h"
#include "ink/ui_shell.h"
#define ink_main product_main
#include "../system/boot/main.c"
#undef ink_main
#include "integration/whole_product.h"
#include "settings_b_fixture.h"

static int case_id;
static char snap[8192], args[256];
static const char *label;
static int x, y;
static bool card_present;
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void quiet(void) { CHECK(WAIT_FOR(settled(), 12000), "display settles"); }
static void read_snapshot(void) { CHECK(ink_ui_snapshot(snap, sizeof snap, NULL) > 0, "complete page snapshot"); }
static void read_case(void *ctx) { (void)ctx; case_id = atoi(getenv("INK_AUDIT3_CASE")); }
static void locate(lv_obj_t *o) {
    if (!lv_obj_is_visible(o)) return;
    if (lv_obj_check_type(o, &lv_label_class) && !strcmp(lv_label_get_text(o), label)) {
        lv_area_t a; lv_obj_get_coords(o, &a); x = (a.x1 + a.x2) / 2; y = (a.y1 + a.y2) / 2;
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) locate(lv_obj_get_child(o, (int32_t)i));
}
static void locate_label(void) { x = y = -1; lv_obj_update_layout(lv_screen_active()); locate(lv_screen_active()); }
static void press(const char *text) {
    label = text; on_ui(locate_label); CHECK(x >= 0 && y >= 0, "visible touch control: %s", text);
    if (x >= 0 && y >= 0) tap(x, y);
    quiet();
}
static void ok(const char *cap, const char *json) {
    CHECK(AI(cap, json) && g_rc.status == INK_RC_DONE, "%s succeeds (%s)", cap, ink_rc_name(g_rc.status)); quiet();
}
static void confirm(void) {
    ink_confirm_view_t v;
    CHECK(ink_confirm_list(&v, 1) == 1, "identity change requires human confirmation");
    CHECK(ink_confirm_decide(v.token, true, INK_SRC_TOUCH) == 0, "person confirms identity change"); quiet();
}
static void set_card(void *ctx) { (void)ctx; sim_sd_set_present(card_present); }
static void card(bool present) {
    card_present = present; vtk_event_at(vtk_now() + SIM_MS(1), set_card, NULL); delay_ms(10); ink_fs_refresh_state();
}
static uint32_t image(const char *path) {
    CHECK(ink_fs_write_atomic(path, SYNTH_PNG, sizeof SYNTH_PNG) == 0, "write synthetic image");
    uint32_t at = 0; ink_fs_entry_t e;
    while (ink_fs_index("/me", INK_FS_IMAGE, &at, &e) == 1) if (!strcmp(e.path, path)) return e.id;
    CHECK(false, "image receives a file ID"); return 0;
}
static void query_has(const char *query, const char *expected) {
    snprintf(args, sizeof args, "{\"query\":\"%s\"}", query);
    CHECK(AI("search.query", args) && g_rc.status == INK_RC_DONE && strstr(g_result, expected),
        "B3-02: available source remains searchable: %s", query);
}
static void search_case(void) {
    CHECK(ink_fs_write_atomic("/me/合成拔卡.txt", "SYNTHETIC", 9) == 0, "write indexed search fixture");
    query_has("合成拔卡", "\"type\":\"文件\"");
    card(false);
    query_has("电话", "\"title\":\"电话\"");
    CHECK(strstr(g_result, "\"unavailable\":[") && strstr(g_result, "文件"), "B3-02: response identifies unavailable file category");
    query_has("音量", "\"type\":\"设置\"");
    ok("search.show", "{\"query\":\"电话\"}");
    bool shown = false;
    for (int i = 0; i < 300 && !shown; i++) {
        on_ui(read_snapshot); shown = strstr(snap, "电话") && strstr(snap, "文件") && strstr(snap, "暂时搜不了");
        if (!shown) delay_ms(20);
    }
    CHECK(shown, "B3-02: page preserves results and names unavailable categories");
    card(true); query_has("合成拔卡", "\"type\":\"文件\"");
    CHECK(strstr(g_result, "\"unavailable\":[]"), "B3-02: reinsertion restores all categories");
}
static int log_count(void) {
    ink_db_t *db = ink_db_open_cached_member(ink_db_find_def("activity.log"), ink_member_current()->id);
    int n = db ? ink_db_count(db) : 0; ink_db_close(db); return n;
}
static void page(unsigned n, const char *why) {
    on_ui(read_snapshot); char expected[32]; snprintf(expected, sizeof expected, "\"page\":%u", n);
    CHECK(strstr(snap, expected), "B3-03: %s", why);
}
static void pagination_case(void) {
    for (unsigned i = 0; i < 18; i++) {
        snprintf(args, sizeof args, "{\"id\":\"display.light\",\"value\":%u}", i);
        CHECK(AI("settings.set", args) && g_rc.status == INK_RC_DONE, "create real AI activity");
    }
    CHECK(WAIT_FOR(log_count() >= 18, 10000), "activity writes complete");
    ok("shell.ai_log", NULL); ok("ai.pages.next", NULL); page(2, "log reaches its second page");
    ok("shell.ai_receipts", NULL); page(1, "receipts start independently");
    ok("ai.pages.next", NULL); page(2, "receipts reach their second page");
    ok("shell.settings", NULL); ok("shell.back", NULL); page(2, "receipts retain their own page on return");
    ok("shell.back", NULL); page(2, "return from receipts restores the original log page");
    ok("shell.ai_receipts", NULL); page(1, "explicit receipt reopen resets only receipts");
    ok("shell.back", NULL); page(2, "receipt reopen keeps log position");
    ok("shell.ai_log", NULL); page(1, "explicit log reopen resets log");
    CHECK(ink_member_switch("kid") == 0, "switch member"); quiet();
    ok("shell.ai_log", NULL); page(1, "new member starts at first page");
}
static bool backup_failed(void) {
    on_ui(read_snapshot); return strstr(snap, "\"page\":\"BackupFail\"") && strstr(snap, "请插入存储卡后备份");
}
static void backup_case(void) {
    ok("backup.open", NULL); card(false);
    CHECK(AI("backup.now", NULL) && g_rc.status == INK_RC_FAILED, "missing card rejects AI backup");
    CHECK(AI("backup.status", NULL) && g_rc.status == INK_RC_DONE && strstr(g_result, "\"phase\":\"failed\"") &&
        strstr(g_result, "请插入存储卡后备份"), "B3-04: service status publishes immediate backup failure");
    CHECK(WAIT_FOR(backup_failed(), 6000), "B3-04: AI failure updates the open backup page"); quiet();
    ok("backup.open", NULL); CHECK(WAIT_FOR(backup_failed(), 6000), "B3-04: reopening reads the same service failure");
    press("再试一次"); CHECK(WAIT_FOR(backup_failed(), 6000), "B3-04: touch uses the same failed service state");
}
static unsigned images(void) {
    uint32_t at = 0; ink_fs_entry_t e; unsigned n = 0;
    while (ink_fs_index("/me", INK_FS_IMAGE, &at, &e) == 1) n++;
    return n;
}
static void capture_case(void) {
    unsigned before = images();
    const ink_cap_t *capture = ink_cap_find("camera.capture");
    CHECK(capture && strstr(capture->title, "不保存"), "B3-05: capture title explicitly says it does not save");
    CHECK(AI("camera.capture", NULL) && g_rc.status == INK_RC_DONE && strstr(g_rc.say, "没有存到相册") &&
        !strncmp(g_rc.object, "photo:", 6), "B3-05: frame receipt reports memory-only capture");
    CHECK(images() == before, "B3-05: capture does not create a photo file");
    ok("app.open", "{\"id\":\"camera\"}"); ok("camera.photo", NULL);
    CHECK(!strncmp(g_rc.object, "file:", 5) && images() == before + 1, "B3-05: photo capability saves exactly one indexed file");
}
static const char *photo_path;
static bool photo_loaded(void) { on_ui(read_snapshot); return strstr(snap, "\"page\":\"Photo\"") && strstr(snap, photo_path); }
static void photo_case(void) {
    const char *path = "/me/合成文件图.png";
    uint32_t owner_id = image(path);
    ok("app.open", "{\"id\":\"files\"}");
    snprintf(args, sizeof args, "{\"id\":%u}", (unsigned)owner_id); ok("files.select", args);
    bool ready = false;
    for (int i = 0; i < 300 && !ready; i++) { on_ui(read_snapshot); ready = strstr(snap, "合成文件图.png") && strstr(snap, "打开"); if (!ready) delay_ms(20); }
    CHECK(ready, "file actions finish loading"); quiet(); press("打开"); photo_path = path;
    CHECK(WAIT_FOR(photo_loaded(), 10000), "B3-06: touching file open displays this file in Photo");
    ok("system.home", NULL);
    uint32_t second = image("/me/合成未列图.png");
    snprintf(args, sizeof args, "{\"id\":%u}", (unsigned)second); ok("photo.open", args); photo_path = "/me/合成未列图.png";
    CHECK(WAIT_FOR(photo_loaded(), 10000), "B3-06: Photo accepts a permitted image outside gallery's current list");
    CHECK(ink_member_switch("kid") == 0, "switch to child"); quiet();
    snprintf(args, sizeof args, "{\"id\":%u}", (unsigned)owner_id);
    CHECK(AI("photo.open", args) && g_rc.status == INK_RC_REJECTED, "B3-06: child cannot open adult image by known ID");
    uint32_t child_id = image("/me/合成孩子图.png");
    snprintf(args, sizeof args, "{\"id\":%u}", (unsigned)child_id); ok("photo.open", args); photo_path = "/me/合成孩子图.png";
    CHECK(WAIT_FOR(photo_loaded(), 10000), "B3-06: child can view their own image");
}
static bool renamed_list(void) { on_ui(read_snapshot); return strstr(snap, "合成新称呼") != NULL; }
static void members_case(void) {
    ok("shell.family", NULL); ok("member.rename", "{\"id\":\"kid\",\"name\":\"合成新称呼\"}");
    CHECK(WAIT_FOR(renamed_list(), 5000), "B3-07: open member list and snapshot update after rename");
    label = "合成新称呼"; on_ui(locate_label); CHECK(x >= 0, "B3-07: renamed label is visible on screen");
    ok("family.edit", "{\"id\":\"kid\"}"); ok("family.focus", NULL); ok("text.insert", "{\"text\":\"合成未提交\"}");
    ok("member.rename", "{\"id\":\"kid\",\"name\":\"合成再改名\"}"); on_ui(read_snapshot);
    CHECK(strstr(snap, "合成未提交"), "B3-07: member event preserves an edited input");
    CHECK(AI("member.add", "{\"id\":\"synthetic\",\"name\":\"合成访客\"}") && g_rc.status == INK_RC_PENDING_CONFIRMATION, "request synthetic member"); confirm();
    CHECK(WAIT_FOR(ink_member_exists("synthetic"), 10000), "synthetic member is added");
    ok("system.home", NULL); ok("shell.family", NULL); ok("family.edit", "{\"id\":\"synthetic\"}");
    CHECK(AI("member.remove", "{\"id\":\"synthetic\"}") && g_rc.status == INK_RC_PENDING_CONFIRMATION, "request target removal"); confirm();
    CHECK(WAIT_FOR(!ink_member_exists("synthetic"), 10000), "target removed"); quiet();
    CHECK(AI("family.save", NULL) && g_rc.status == INK_RC_STALE, "B3-07: deleted edit target invalidates save");
}
static const ink_member_t *member(const char *id) {
    for (int i = 0; ink_member_at(i); i++) if (!strcmp(ink_member_at(i)->id, id)) return ink_member_at(i);
    return NULL;
}
static char add_status[32], add_say[64];
static void add_result(void *ctx, const char *id, const ink_args_t *payload) {
    (void)ctx; (void)id; char capability[32] = "";
    ink_args_get_str(payload, "cap", capability, sizeof capability);
    if (strcmp(capability, "member.add")) return;
    ink_args_get_str(payload, "status", add_status, sizeof add_status);
    ink_args_get_str(payload, "say", add_say, sizeof add_say);
}
static void name_case(void) {
    static const char *const ten = "一二三四五六七八九十";
    CHECK(ink_cap_subscribe("confirm.result", add_result, NULL, INK_EXEC_DIRECT) == 0, "observe confirmed add receipt");
    CHECK(AI("member.rename", "{\"id\":\"kid\",\"name\":\"一二三四五六七八九十甲\"}") && g_rc.status == INK_RC_INVALID &&
        !strcmp(g_rc.say, "称呼太长"), "B3-08: rename rejects eleven Chinese characters explicitly");
    ok("member.rename", "{\"id\":\"kid\",\"name\":\"一二三四五六七八九十\"}");
    CHECK(member("kid") && !strcmp(member("kid")->name, ten), "B3-08: rename preserves all ten Chinese characters");
    CHECK(AI("member.add", "{\"id\":\"synthetic\",\"name\":\"一二三四五六七八九十甲\"}") && g_rc.status == INK_RC_PENDING_CONFIRMATION, "request long member name"); confirm();
    CHECK(WAIT_FOR(!strcmp(add_say, "称呼太长"), 10000) && !strcmp(add_status, "invalid") && !member("synthetic"),
        "B3-08: confirmed add rejects eleven Chinese characters explicitly");
    CHECK(AI("member.add", "{\"id\":\"synthetic10\",\"name\":\"一二三四五六七八九十\"}") && g_rc.status == INK_RC_PENDING_CONFIRMATION, "request ten-character member name"); confirm();
    CHECK(WAIT_FOR(member("synthetic10") != NULL, 10000) && !strcmp(member("synthetic10")->name, ten),
        "B3-08: confirmed add preserves all ten Chinese characters");
    ok("shell.family", NULL); ok("family.edit", "{\"id\":\"kid\"}"); ok("family.focus", NULL);
    ok("text.insert", "{\"text\":\"一二三四五六七八九十\"}");
    ok("text.insert", "{\"text\":\"甲\"}");
    on_ui(read_snapshot); CHECK(strstr(snap, ten) && !strstr(snap, "一二三四五六七八九十甲"), "B3-08: editor's 31-byte limit preserves complete Chinese input");
}
static bool wallpaper_recorded(void) {
    return AI("activity.list", NULL) && g_rc.status == INK_RC_DONE && strstr(g_result, "settings.set");
}
static void wallpaper_case(void) {
    uint32_t id = image("/me/合成壁纸.png");
    ok("shell.wallpaper", NULL); ok("wallpaper.choose", NULL);
    snprintf(args, sizeof args, "{\"id\":%u}", (unsigned)id); ok("file_picker.select", args);
    CHECK(WAIT_FOR(ink_setting_get("display.wallpaper_file") == id, 10000), "wallpaper setting is applied");
    CHECK(WAIT_FOR(wallpaper_recorded(), 5000), "B3-09: AI picker setting is recorded as AI activity");
    CHECK(WAIT_FOR(ink_wallpaper_ready(), 5000), "chosen wallpaper loads real pixels");
}
static void run(void *ctx) {
    (void)ctx; vtk_host_call(read_case, NULL);
    CHECK(WAIT_FOR(ink_member_space_open(), 10000), "owner space opens"); test_start_home(); quiet();
    switch (case_id) {
    case 2: search_case(); break;
    case 3: pagination_case(); break;
    case 4: backup_case(); break;
    case 5: capture_case(); break;
    case 6: photo_case(); break;
    case 7: members_case(); break;
    case 8: name_case(); break;
    case 9: wallpaper_case(); break;
    default: CHECK(false, "unknown audit case"); break;
    }
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name = "audit3-test", .fn = run, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start bounded audit regression");
}
