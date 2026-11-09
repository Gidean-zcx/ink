/* Synthetic files, real pages, real service and a second-process restart. */
#include "../../system/apps/files/files.c"
#define ink_main product_main
#include "../../system/boot/main.c"
#undef ink_main
#include "../integration/whole_product.h"
#include "image_fixtures.h"
#include <stdlib.h>

static char phase[16], capture_scene[32], scene[32], snapshot_json[12000];
static const char *wanted, *expected_page;
static lv_obj_t *found;
static bool ready;
static uint32_t kept, removed, picture_id;
static unsigned expected_total = 10;
static void host_options(void *unused) {
    (void)unused;
    snprintf(phase, sizeof phase, "%s", getenv("INK_FILES_PHASE"));
    snprintf(capture_scene, sizeof capture_scene, "%s", getenv("INK_FILES_SCENE"));
}
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void quiet(void) { CHECK(WAIT_FOR(settled(), 15000), "file display settles"); }
static void command(const char *cap, const char *args) {
    CHECK(AI(cap, args) && g_rc.status == INK_RC_DONE, "%s completes (%d: %s)", cap, g_rc.status, g_rc.detail);
}
static void poll(void) { ready = !F.busy && !strcmp(ink_ui_page_name(), expected_page); }
static void wait_page(const char *name) {
    expected_page = name; ready = false;
    for (unsigned i = 0; i < 750 && !ready; i++) { on_ui(poll); if (!ready) delay_ms(20); }
    CHECK(ready, "page %s has finished reading", name); quiet();
}
static void locate(lv_obj_t *obj) {
    if (lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN)) return;
    if (lv_obj_check_type(obj, &lv_label_class) && !strcmp(lv_label_get_text(obj), wanted)) found = obj;
    for (uint32_t i = 0; i < lv_obj_get_child_count(obj); i++) locate(lv_obj_get_child(obj, (int)i));
}
static void target(void) {
    found = NULL; lv_obj_update_layout(lv_screen_active()); locate(lv_screen_active());
    CHECK(found != NULL, "visible control: %s", wanted);
    if (found) { lv_area_t a; lv_obj_get_coords(found, &a); g_btn_x[0] = (a.x1 + a.x2) / 2; g_btn_y[0] = (a.y1 + a.y2) / 2; }
}
static void touch(const char *label) { wanted = label; on_ui(target); if (found) tap(g_btn_x[0], g_btn_y[0]); }
static void host_snapshot(void *unused) {
    (void)unused; char path[768];
    int n = snprintf(path, sizeof path, "%s/snapshots.ndjson", g_sim.out_dir);
    if (n < 0 || (size_t)n >= sizeof path) abort();
    FILE *f = fopen(path, "ab");
    if (!f || fprintf(f, "{\"scene\":\"%s\",\"snapshot\":%s}\n", scene, snapshot_json) < 0 || fclose(f)) abort();
}
static void snapshot_page(void) {
    const char *page = !strcmp(scene, "FileActionsPreview") ? "FileActions" : !strcmp(scene, "FileMoveLong") ? "FileMove" :
                       !strcmp(scene, "FileListLong") ? "FileList" : scene;
    CHECK(ink_ui_snapshot(snapshot_json, sizeof snapshot_json, NULL) > 0 && strstr(snapshot_json, page), "%s has a semantic snapshot", scene);
    CHECK(ink_ui_context_snapshot(g_snap, sizeof g_snap, NULL) > 0, "%s fits the production AI buffer", scene);
    vtk_host_call(host_snapshot, NULL);
}
static void frame(void *unused) {
    (void)unused; CHECK(settled(), "capture a settled file frame");
    char name[80]; snprintf(name, sizeof name, "test.snapshot.%s", scene);
    ink_probe_count("test.snapshot.settled"); ink_probe_count(name); sim_display_snapshot("scene");
}
static void snap(const char *name) {
    snprintf(scene, sizeof scene, "%s", name); quiet(); on_ui(snapshot_page);
    if (!strcmp(name, capture_scene)) { vtk_event_at(vtk_now() + SIM_MS(1), frame, NULL); delay_ms(5); }
}
static uint32_t file_id(const char *path) {
    uint32_t cursor = 0; ink_fs_entry_t e; int r;
    while ((r = ink_fs_index("/me/SYNTH_FILES", INK_FS_ANY, &cursor, &e)) == 1) if (!strcmp(e.path, path)) return e.id;
    CHECK(r == 0, "synthetic file index ends"); return 0;
}
static void select_id(uint32_t id) {
    char args[64]; snprintf(args, sizeof args, "{\"id\":%u}", (unsigned)id);
    command("files.select", args); wait_page("FileActions");
}
static void roots(void) { command("system.home", NULL); command("app.open", "{\"id\":\"files\"}"); wait_page("Files"); }
static void browse(void) { command("files.browse", "{\"path\":\"/me/SYNTH_FILES\"}"); wait_page("FileList"); }
static void fixture(void) {
    CHECK(ink_fs_mkdir("/me/SYNTH_FILES/SYNTH_DEST") == 0, "create synthetic destination");
    CHECK(ink_fs_mkdir("/me/SYNTH_FILES/SYNTH_SUB") == 0, "create synthetic nested folder");
    CHECK(ink_fs_write_atomic("/family/SYNTH_SHARED.txt", "SYNTH_SHARED", 12) == 0, "family fixture");
    CHECK(ink_fs_write_atomic("/me/SYNTH_FILES/SYNTH_A.txt", "SYNTHETIC_FILE_PREVIEW", 22) == 0, "private text fixture");
    CHECK(ink_fs_write_atomic("/me/SYNTH_FILES/SYNTH_B.png", black_png, sizeof black_png) == 0, "valid synthetic PNG");
    CHECK(ink_fs_write_atomic("/me/SYNTH_FILES/SYNTH_C.txt", "SYNTH_DELETE", 12) == 0, "delete fixture");
    for (int i = 0; i < 5; i++) {
        char path[128]; snprintf(path, sizeof path, "/me/SYNTH_FILES/SYNTH_%d.txt", i);
        CHECK(ink_fs_write_atomic(path, "SYNTHETIC", 9) == 0, "paged synthetic file %d", i);
    }
    CHECK(ink_fs_write_atomic("/me/SYNTH_FILES/SYNTH_SUB/SYNTH_NESTED.txt", "SYNTHETIC", 9) == 0, "nested files stay in their own folder");
    kept = file_id("/me/SYNTH_FILES/SYNTH_A.txt"); removed = file_id("/me/SYNTH_FILES/SYNTH_C.txt"); picture_id = file_id("/me/SYNTH_FILES/SYNTH_B.png");
    CHECK(kept && removed && picture_id, "stable service identifiers");
}
static void first_page(void) {
    CHECK(F.count == expected_total && F.list && F.position.page == 0, "only %u direct children, not the nested file", expected_total);
    char pages[32]; snprintf(pages, sizeof pages, "\"pages\":%u", (expected_total + 3) / 4);
    CHECK(ink_ui_snapshot(g_snap, sizeof g_snap, NULL) > 0 && strstr(g_snap, pages) && strstr(g_snap, "\"visible\":4"), "F4 pages only four visible rows");
    CHECK(strstr(g_snap, "files.select") && strstr(g_snap, "files.browse") && strstr(g_snap, "files.next"), "AI sees the same row and paging capabilities");
}
static lv_obj_t *ai_panel;
static void show_ai(void) {
    lv_obj_t *caption, *question, *answer;
    ink_shell_ai_open(&ai_panel, &caption, &question, &answer);
    CHECK(ai_panel && !strcmp(ink_ui_top_name(), "Ai"), "use the real AI panel above the file page");
}
static void close_ai(void) { ink_shell_ai_closed(); lv_obj_delete(ai_panel); ai_panel = NULL; }
static void ai_page(void) {
    CHECK(F.position.page == 1, "AI can turn the underlying file page with its panel open");
    CHECK(ink_ui_context_snapshot(g_snap, sizeof g_snap, NULL) > 0 && strstr(g_snap, "FileList"), "AI context still describes its underlying file page");
}
static void third_page(void) { CHECK(F.position.page == 2 && ink_ui_snapshot(g_snap, sizeof g_snap, NULL) > 0 && strstr(g_snap, "\"visible\":2"), "last page has two entries"); }
static void thumb_check(void) {
    bool visible = false;
    for (unsigned i = 0; i < 4; i++) if (F.images[i] && !lv_obj_has_flag(F.images[i], LV_OBJ_FLAG_HIDDEN) && lv_image_get_src(F.images[i]) == &F.thumbs[i]) {
        visible = true;
        for (size_t p = 0; p < sizeof F.pixels[i]; p++) if (F.pixels[i][p] != 0) { CHECK(false, "decoded thumbnail is the synthetic black image"); return; }
        CHECK(F.thumbs[i].header.w == INK_ICON_LIST && F.thumbs[i].header.cf == LV_COLOR_FORMAT_L8, "thumbnail is a real L8 LVGL image");
    }
    CHECK(visible, "a picture row displays file.thumb pixels");
}
static unsigned picture_page;
static void find_picture_page(void) { for (size_t i = 0; i < F.count; i++) if (F.items[i].id == picture_id) picture_page = (unsigned)i / 4; }
static void check_list_thumb(void) {
    picture_page = 0; on_ui(find_picture_page);
    for (unsigned i = 0; i < picture_page; i++) { command("files.next", NULL); quiet(); }
    on_ui(thumb_check);
    for (unsigned i = 0; i < picture_page; i++) { command("files.prev", NULL); quiet(); }
}
static void action_contract(void) {
    CHECK(F.selected.id == kept && F.field, "actions resolve the selected stable id");
    CHECK(ink_ui_snapshot(g_snap, sizeof g_snap, NULL) > 0 && strstr(g_snap, "file.move") && strstr(g_snap, "file.delete") && strstr(g_snap, "files.preview"), "rename, delete and open use advertised capabilities");
    CHECK(!strcmp(F.actions[1].capability, "file.move") && !strcmp(F.actions[4].capability, "file.delete"), "touch bindings are the existing AI file capabilities");
}
static void preview_check(void) { CHECK(!strcmp(F.preview_text, "SYNTHETIC_FILE_PREVIEW") && !strcmp(lv_label_get_text(F.preview), F.preview_text), "opening reads actual text into the visible preview"); }
static void notice_check(void) {
    lv_obj_update_layout(F.screen);
    lv_area_t notice; lv_obj_get_coords(F.notice, &notice);
    CHECK(lv_label_get_text(F.notice)[0] && !lv_obj_has_flag(F.notice, LV_OBJ_FLAG_HIDDEN) &&
          notice.y1 >= 0 && notice.y2 < ink_ui_height(), "operation result is visible inside the screen");
    for (uint32_t i = 0; i < lv_obj_get_child_count(F.screen); i++) {
        lv_obj_t *obj = lv_obj_get_child(F.screen, (int)i);
        if (obj == F.notice || lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN) || !lv_obj_has_flag(obj, LV_OBJ_FLAG_CLICKABLE)) continue;
        lv_area_t control; lv_obj_get_coords(obj, &control);
        CHECK(notice.x2 < control.x1 || control.x2 < notice.x1 || notice.y2 < control.y1 || control.y2 < notice.y1,
              "operation result does not intersect an action control");
    }
}
static void name_selected(void) { ink_text_select(0, ink_text_char_offset(ink_text_value(), strlen(ink_text_value()))); }
static void move_contract(void) {
    CHECK(F.mode == MOVE && !strcmp(F.actions[2].capability, "file.move") && F.actions[2].enabled, "destination button calls the same file.move as AI");
    CHECK(ink_ui_snapshot(g_snap, sizeof g_snap, NULL) > 0 && strstr(g_snap, "SYNTH_DEST") && strstr(g_snap, "file.move"), "move snapshot describes the actual target");
}
static void child_roots(void) {
    CHECK(ink_ui_snapshot(g_snap, sizeof g_snap, NULL) > 0 && !strstr(g_snap, "/members/"), "child roots hide every other member's private directory");
    for (size_t i = 0; i < F.count; i++) CHECK(!strncmp(F.items[i].path, "/me", 3) || !strncmp(F.items[i].path, "/family", 7), "all child entry paths are their own or shared");
}
static const char *expected_directory;
static void parent_check(void) {
    ready = !strcmp(ink_ui_page_name(), expected_page) && !strcmp(directory(), expected_directory);
    CHECK(ready,
          "up returns to %s at %s, got %s at %s", expected_page, expected_directory, ink_ui_page_name(), directory());
}
static bool up_to(const char *page, const char *path) {
    command("files.up", NULL); quiet();
    CHECK(WAIT_FOR(!F.busy, 10000), "parent directory finishes reading");
    expected_page = page; expected_directory = path; on_ui(parent_check);
    return ready;
}
static void browse_path(const char *path, const char *page) {
    char args[384]; snprintf(args, sizeof args, "{\"path\":\"%s\"}", path);
    command("files.browse", args); wait_page(page);
}
static void long_rows(void) {
    CHECK(F.count == 4, "all four legal long directory names are listed");
    CHECK(ink_ui_context_snapshot(g_snap, sizeof g_snap, NULL) > 0, "long directory names fit the 3072-byte production AI buffer");
    CHECK(strstr(g_snap, "名称已截断"), "AI snapshot explains shortened directory names");
    ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL);
    CHECK(j != NULL, "allocate action argument parser");
    if (!j) return;
    for (size_t i = 0; i < F.count && i < 4; i++) {
        char path[INK_FS_PATH_MAX];
        CHECK(ink_json_parse(j, F.args[i], strlen(F.args[i])) == 0 &&
              ink_json_str(j, ink_json_find(j, 0, "path"), path, sizeof path) > 0 && !strcmp(path, F.items[i].path),
              "shortened row retains its complete browse path");
    }
    osal_free(j);
}
static void review_regressions(void) {
    fixture(); roots();
    CHECK(ink_fs_mkdir("/me/2026/notes") == 0 && ink_fs_mkdir("/family/2026/notes") == 0 &&
          ink_fs_mkdir("/members/kid/SYNTH_NEST") == 0, "create synthetic parent-boundary directories");
    browse_path("/me", "FileList"); browse_path("/me/2026", "FileList"); browse_path("/me/2026/notes", "FileList");
    if (up_to("FileList", "/me/2026") && up_to("FileList", "/me")) up_to("Files", "");
    roots();
    browse_path("/family/2026/notes", "FileList");
    if (up_to("FileList", "/family/2026") && up_to("FileList", "/family")) up_to("Files", "");
    roots();
    browse_path("/members/kid/SYNTH_NEST", "FileList"); if (up_to("FileList", "/members/kid")) up_to("Files", ""); roots();
    browse_path("/members/owner", "FileList"); up_to("Files", ""); roots();
    browse(); select_id(kept); touch("打开"); quiet(); on_ui(preview_check); on_ui(notice_check); snap("FileActionsPreview");
    command("settings.set", "{\"id\":\"display.font_size\",\"value\":3}"); quiet(); wait_page("FileActions");
    touch("打开"); quiet(); on_ui(preview_check); on_ui(notice_check);
    touch("移动"); wait_page("FileMove"); browse_path("/me/2026/notes", "FileMove");
    if (up_to("FileMove", "/me/2026") && up_to("FileMove", "/me")) up_to("FileMove", "");
    browse_path("/members/kid/SYNTH_NEST", "FileMove"); if (up_to("FileMove", "/members/kid")) up_to("FileMove", "");
    const char *parent = "/me/SYNTH_资料目录";
    for (int i = 0; i < 4; i++) {
        char path[INK_FS_PATH_MAX]; snprintf(path, sizeof path, "%s/SYNTH_测测测测测测测测测测测测测测测测测%d", parent, i);
        CHECK(ink_fs_mkdir(path) == 0, "create legal synthetic long directory %d", i);
    }
    browse_path(parent, "FileMove"); on_ui(long_rows); snap("FileMoveLong");
    roots(); browse_path(parent, "FileList"); on_ui(long_rows); snap("FileListLong");
    on_ui(show_ai); quiet(); on_ui(long_rows); on_ui(close_ai);
    touch("SYNTH_测…"); wait_page("FileList");
    CHECK(!strncmp(F.path, parent, strlen(parent)) && strlen(F.path) > strlen(parent), "touch opens the full long directory path");
}
static void verify_restart(void) {
    ink_fs_file_t *f = NULL; char ids[64] = {0};
    CHECK(ink_fs_open("/me/SYNTH_IDS.txt", INK_FS_READ, &f) == 0, "restart reads synthetic expected ids");
    if (f) { CHECK(ink_fs_read(f, ids, sizeof ids - 1) > 0, "read expected ids"); CHECK(ink_fs_close(f) == 0, "close expected ids"); }
    CHECK(sscanf(ids, "%u %u", &kept, &removed) == 2, "parse synthetic expected ids");
    ink_fs_entry_t e;
    CHECK(ink_fs_lookup(kept, &e) == 0 && !strcmp(e.path, "/me/SYNTH_FILES/SYNTH_DEST/SYNTH_FINAL.txt"), "rename and move survive a new process with the same id");
    CHECK(ink_fs_lookup(removed, &e) == INK_FS_ERR_NOT_FOUND, "confirmed deletion survives restart");
    ink_fs_stat_t st;
    CHECK(ink_fs_stat("/me/SYNTH_FILES/SYNTH_A.txt", &st) == INK_FS_ERR_NOT_FOUND && ink_fs_stat("/me/SYNTH_FILES/SYNTH_C.txt", &st) == INK_FS_ERR_NOT_FOUND, "old and deleted names stay absent");
    roots(); browse(); command("files.browse", "{\"path\":\"/me/SYNTH_FILES/SYNTH_DEST\"}"); wait_page("FileList");
    select_id(kept); on_ui(action_contract); touch("打开"); quiet(); on_ui(preview_check);
}
static void exercise(void) {
    fixture(); roots(); snap("Files");
    touch("我的"); wait_page("FileList"); browse(); on_ui(first_page); snap("FileList");
    on_ui(show_ai); quiet(); command("files.next", NULL); quiet(); on_ui(ai_page); on_ui(close_ai); command("files.prev", NULL); quiet();
    check_list_thumb(); key(INK_KEY_NEXT); quiet(); command("files.next", NULL); quiet(); on_ui(third_page);
    command("files.prev", NULL); key(INK_KEY_PREV); quiet(); on_ui(first_page);
    select_id(picture_id); on_ui(thumb_check); command("files.up", NULL); wait_page("FileList");
    select_id(kept); on_ui(action_contract); snap("FileActions"); touch("打开"); quiet(); on_ui(preview_check);
    char args[128]; snprintf(args, sizeof args, "{\"id\":%u}", (unsigned)kept); command("files.preview", args); quiet(); on_ui(preview_check);
    command("files.name", NULL); quiet(); on_ui(name_selected); command("text.insert", "{\"text\":\"SYNTH_RENAMED.txt\"}"); command("text.submit", NULL); quiet(); touch("改名"); wait_page("FileActions");
    ink_fs_entry_t e;
    CHECK(ink_fs_lookup(kept, &e) == 0 && !strcmp(e.path, "/me/SYNTH_FILES/SYNTH_RENAMED.txt"), "real rename button preserves stable id");
    touch("移动"); wait_page("FileMove"); command("files.browse", "{\"path\":\"/me/SYNTH_FILES/SYNTH_DEST\"}"); wait_page("FileMove"); on_ui(move_contract); snap("FileMove"); touch("放到这里"); wait_page("FileActions");
    CHECK(ink_fs_lookup(kept, &e) == 0 && !strcmp(e.path, "/me/SYNTH_FILES/SYNTH_DEST/SYNTH_RENAMED.txt"), "real target button moves the same object");
    command("file.move", "{\"from\":\"/me/SYNTH_FILES/SYNTH_DEST/SYNTH_RENAMED.txt\",\"to\":\"/me/SYNTH_FILES/SYNTH_DEST/SYNTH_FINAL.txt\"}"); wait_page("FileActions");
    CHECK(ink_fs_lookup(kept, &e) == 0 && !strcmp(e.path, "/me/SYNTH_FILES/SYNTH_DEST/SYNTH_FINAL.txt"), "AI uses the same mutation capability");
    command("files.up", NULL); wait_page("FileList"); select_id(removed); touch("删除"); quiet();
    ink_confirm_view_t confirm[4]; int count = ink_confirm_list(confirm, 4); ink_fs_stat_t st;
    CHECK(count == 1 && !strcmp(confirm[0].cap, "file.delete") && strstr(confirm[0].detail, "SYNTH_C.txt"), "touch deletion asks exactly what will be lost");
    CHECK(ink_fs_stat("/me/SYNTH_FILES/SYNTH_C.txt", &st) == 0, "pending confirmation has not deleted anything");
    if (count) { CHECK(ink_confirm_decide(confirm[0].token, true, INK_SRC_AI) != 0, "AI cannot say yes for the person"); CHECK(ink_confirm_decide(confirm[0].token, false, INK_SRC_TOUCH) == 0, "person cancels deletion"); }
    quiet(); CHECK(ink_fs_stat("/me/SYNTH_FILES/SYNTH_C.txt", &st) == 0, "cancel keeps the file");
    CHECK(AI("file.delete", "{\"path\":\"/me/SYNTH_FILES/SYNTH_C.txt\"}") && g_rc.status == INK_RC_PENDING_CONFIRMATION, "AI deletion uses the same confirmation gate");
    count = ink_confirm_list(confirm, 4); CHECK(count == 1, "one real delete confirmation");
    if (count) CHECK(ink_confirm_decide(confirm[0].token, true, INK_SRC_TOUCH) == 0, "person confirms deletion");
    CHECK(WAIT_FOR(ink_fs_stat("/me/SYNTH_FILES/SYNTH_C.txt", &st) == INK_FS_ERR_NOT_FOUND, 10000), "confirmed delete job removes the file");
    wait_page("FileList");
    char ids[64]; int n = snprintf(ids, sizeof ids, "%u %u", (unsigned)kept, (unsigned)removed);
    CHECK(n > 0 && ink_fs_write_atomic("/me/SYNTH_IDS.txt", ids, (size_t)n) == 0, "persist only synthetic expectations for the next process");
    CHECK(ink_member_switch("kid") == 0, "switch to the real child"); roots(); on_ui(child_roots);
    CHECK(AI("files.browse", "{\"path\":\"/members/owner/SYNTH_FILES\"}") && g_rc.status == INK_RC_REJECTED, "forged adult directory is rejected");
    CHECK(ink_fs_lookup(kept, &e) == INK_FS_ERR_DENIED, "service also rejects the adult's stable id");
    select_id(kept); on_ui(read_screen);
    CHECK(!strstr(g_snap, "SYNTH_FINAL") && !strstr(g_snap, "SYNTH_DEST") && !strstr(g_snap, "file.move"), "a forged private id leaves no previous file details or mutation actions");
    CHECK(AI("files.move", NULL) && g_rc.status == INK_RC_REJECTED, "failed lookup cannot reuse a prior selection"); roots();
    CHECK(ink_fs_write_atomic("/me/SYNTH_CHILD.txt", "SYNTHETIC_CHILD", 15) == 0, "child owns a separate fixture");
    command("files.browse", "{\"path\":\"/me\"}"); wait_page("FileList"); on_ui(read_screen);
    CHECK(strstr(g_snap, "SYNTH_CHILD.txt") && !strstr(g_snap, "SYNTH_FINAL") && !strstr(g_snap, "SYNTH_DEST"), "child list contains only their own files");
    command("files.up", NULL); wait_page("Files"); command("files.browse", "{\"path\":\"/family\"}"); wait_page("FileList");
    uint32_t cursor = 0; CHECK(ink_fs_index("/family", INK_FS_TEXT, &cursor, &e) == 1, "child can read family content"); select_id(e.id); on_ui(read_screen);
    CHECK(!strstr(g_snap, "file.delete") && !strstr(g_snap, "file.move") && !strstr(g_snap, "files.name"), "child sees no write actions for shared content");
    CHECK(ink_member_switch("owner") == 0, "return to the owner"); roots();
    expected_total = 8;
    command("settings.set", "{\"id\":\"display.font_size\",\"value\":3}"); quiet(); browse(); on_ui(first_page); check_list_thumb();
}
static void run(void *unused) {
    (void)unused; vtk_host_call(host_options, NULL);
    CHECK(WAIT_FOR(ink_member_space_open(), 10000), "member space opens"); test_start_home(); quiet();
    if (!strcmp(phase, "verify")) verify_restart(); else if (!strcmp(phase, "review")) review_regressions(); else exercise();
    CHECK(WAIT_FOR(ink_db_idle(), 10000), "file index is durable before stopping"); quiet(); ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main(); osal_task_cfg_t cfg = {.name = "files-app-test", .fn = run, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start synthetic file-app test");
}
