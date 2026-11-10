/* Real file pages and services, synthetic content, two independent boots. */
#include "../../system/apps/files/files.c"
#define ink_main product_main
#include "../../system/boot/main.c"
#undef ink_main
#include "../integration/whole_product.h"
#include <stdlib.h>

static char phase[16], scene[32], saved[4096], args_json[1024];
static const char *wanted;
static lv_obj_t *found;
static uint32_t ids[3];
static void options(void *ctx) { (void)ctx; snprintf(phase, sizeof phase, "%s", getenv("INK_FILES_OPS_PHASE")); }
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void quiet(void) { CHECK(WAIT_FOR(settled(), 15000), "display settles"); }
static void command(const char *cap, const char *args) { CHECK(AI(cap, args) && g_rc.status == INK_RC_DONE, "%s: %d %.90s", cap, g_rc.status, g_rc.detail); }
static void wait_page(const char *page) { CHECK(WAIT_FOR(!F.busy && !strcmp(ink_ui_page_name(), page), 15000), "loaded %s (%.40s)", page, ink_ui_page_name()); quiet(); }
static void locate(lv_obj_t *obj) {
    if (lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN)) return;
    if (lv_obj_check_type(obj, &lv_label_class) && !strcmp(lv_label_get_text(obj), wanted)) found = obj;
    for (uint32_t i = 0; i < lv_obj_get_child_count(obj); i++) locate(lv_obj_get_child(obj, (int)i));
}
static void target(void) {
    found = NULL; lv_obj_update_layout(lv_screen_active()); locate(lv_screen_active()); CHECK(found, "visible %.80s", wanted);
    if (found) { lv_area_t a; lv_obj_get_coords(found, &a); g_btn_x[0] = (a.x1 + a.x2) / 2; g_btn_y[0] = (a.y1 + a.y2) / 2; }
}
static void touch(const char *label) {
    quiet(); wanted = label; on_ui(target);
    if (found) {
        double delivered = sim_metric_value("input.touch_delivery_ms"); hw(HW_PRESS, g_btn_x[0], g_btn_y[0]);
        CHECK(WAIT_FOR(sim_metric_value("input.touch_delivery_ms") > delivered, 3000), "touch driver received %.80s", label);
        delivered = sim_metric_value("input.touch_delivery_ms"); hw(HW_RELEASE, g_btn_x[0], g_btn_y[0]);
        CHECK(WAIT_FOR(sim_metric_value("input.touch_delivery_ms") > delivered, 3000), "touch driver released %.80s", label); quiet();
    }
}
static void frame(void *ctx) { (void)ctx; sim_display_snapshot(scene); }
static void capture(const char *name) { quiet(); snprintf(scene, sizeof scene, "%s", name); vtk_event_at(vtk_now() + SIM_MS(1), frame, NULL); delay_ms(5); }
static void browse(const char *path) { snprintf(args_json, sizeof args_json, "{\"path\":\"%s\"}", path); command("files.browse", args_json); wait_page("FileList"); }
static void roots(void) { command("system.home", NULL); command("app.open", "{\"id\":\"files\"}"); wait_page("Files"); }
static uint32_t lookup(const char *path) {
    ink_fs_entry_t e; uint32_t cursor = 0;
    while (ink_fs_index("/me", INK_FS_ANY, &cursor, &e) == 1) if (!strcmp(path, e.path)) return e.id;
    return 0;
}
static bool exists(const char *path) { ink_fs_stat_t st; return !ink_fs_stat(path, &st); }
static void select_file(const char *path) { uint32_t id = lookup(path); CHECK(id, "indexed %.100s", path); snprintf(args_json, sizeof args_json, "{\"id\":%u}", (unsigned)id); command("files.select", args_json); wait_page("FileActions"); }
static void all_text(void) { ink_text_select(0, ink_text_char_offset(ink_text_value(), strlen(ink_text_value()))); }
static void name_text(const char *name) {
    command("files.name", NULL); quiet(); on_ui(all_text); snprintf(args_json, sizeof args_json, "{\"text\":\"%s\"}", name); command("text.insert", args_json); command("text.submit", NULL); quiet();
}
static void check_name(void) {
    CHECK(!F.actions[1].enabled && strstr(lv_label_get_text(F.notice), "不能"), "empty or slash name has an explicit notice");
}
static void check_snapshot(void) {
    CHECK(ink_ui_snapshot(g_snap, sizeof g_snap, NULL) > 0 && strstr(g_snap, "\"selected_count\":3") && strstr(g_snap, "file.delete_many") && strstr(g_snap, "files.move"), "snapshot exposes selection count and batch actions");
}
static void check_recovered(void) { CHECK(!strcmp(ink_text_value(), "SYNTH_SAVED_AND_UNSAVED草稿"), "power cut restores edited draft"); }
static void check_partial(void) { CHECK(F.notice && strstr(lv_label_get_text(F.notice), "SYNTH_1.txt"), "partial result names the failed file in the UI"); }
static void read_file(const char *path) {
    ink_fs_file_t *f = NULL; memset(saved, 0, sizeof saved);
    CHECK(!ink_fs_open(path, INK_FS_READ, &f), "read %.100s", path);
    if (f) { CHECK(ink_fs_read(f, saved, sizeof saved - 1) >= 0, "read text bytes"); CHECK(!ink_fs_close(f), "close text"); }
}
static void folder_and_editor(void) {
    CHECK(!ink_fs_mkdir("/me/SYNTH_OPS"), "create fixture root"); roots(); browse("/me/SYNTH_OPS");
    touch("新建文件夹"); wait_page("FileName"); on_ui(check_name); name_text("bad/name"); on_ui(check_name);
    name_text("SYNTH_FOLDER"); capture("NewFolder"); touch("新建文件夹"); wait_page("FileList"); CHECK(exists("/me/SYNTH_OPS/SYNTH_FOLDER"), "folder created by touch through file.mkdir");
    touch("新建文件夹"); wait_page("FileName"); name_text("SYNTH_FOLDER"); touch("新建文件夹"); quiet(); CHECK(!strcmp(ink_ui_page_name(), "FileName"), "duplicate stays on name page");
    CHECK(strstr(lv_label_get_text(F.notice), "已存在"), "duplicate notice"); command("files.up", NULL); wait_page("FileList");
    CHECK(AI("file.mkdir", "{\"path\":\"/me/SYNTH_OPS/SYNTH_AI\"}") && g_rc.status == INK_RC_DONE, "AI same mkdir"); wait_page("FileList");
    CHECK(AI("file.mkdir", "{\"path\":\"/me/../bad\"}") && g_rc.status != INK_RC_DONE, "AI invalid path rejected");
    touch("新建文本"); wait_page("FileName"); name_text("SYNTH_NOTE.txt"); touch("新建文本"); wait_page("FileList"); select_file("/me/SYNTH_OPS/SYNTH_NOTE.txt");
    touch("编辑文本"); wait_page("FileEdit"); command("text.insert", "{\"text\":\"SYNTH_SAVED\"}");
    CHECK(WAIT_FOR(ink_text_draft_saved() == 1 && ink_db_idle(), 10000), "draft durable before save"); capture("WriteText"); touch("保存"); wait_page("FileActions");
    read_file("/me/SYNTH_OPS/SYNTH_NOTE.txt"); CHECK(!strcmp(saved, "SYNTH_SAVED"), "saved text on card");
    touch("编辑文本"); wait_page("FileEdit"); CHECK(!strcmp(ink_text_value(), "SYNTH_SAVED"), "reopened text matches");
    command("text.insert", "{\"text\":\"_AND_UNSAVED草稿\"}"); CHECK(WAIT_FOR(ink_text_draft_saved() == 1 && ink_db_idle() && !ink_storage_pending(), 15000), "reference and unfinished edit durable before power cut");
}
static void batch_fixture(const char *directory_path) {
    CHECK(!ink_fs_mkdir(directory_path), "batch folder");
    for (int i = 0; i < 3; i++) { char path[128]; snprintf(path, sizeof path, "%s/SYNTH_%d.txt", directory_path, i); CHECK(!ink_fs_write_atomic(path, "SYNTH", 5), "batch file"); ids[i] = lookup(path); CHECK(ids[i], "file id"); }
}
static void batch_args_test(const char *op, const char *to) { snprintf(args_json, sizeof args_json, "{\"ids\":[%u,%u,%u],\"op\":\"%s\",\"to\":\"%s\"}", (unsigned)ids[0], (unsigned)ids[1], (unsigned)ids[2], op, to); }
static void batch_ops(void) {
    roots(); batch_fixture("/me/SYNTH_BATCH"); CHECK(!ink_fs_mkdir("/me/SYNTH_COPY") && !ink_fs_mkdir("/me/SYNTH_MOVE"), "destinations"); browse("/me/SYNTH_BATCH"); touch("多选"); wait_page("FileList"); touch("全选"); wait_page("FileList"); on_ui(check_snapshot); capture("MultiSelect");
    touch("拷贝"); wait_page("FileMove"); command("files.browse", "{\"path\":\"/me/SYNTH_COPY\"}"); wait_page("FileMove"); touch("拷贝到这里");
    CHECK(WAIT_FOR(exists("/me/SYNTH_COPY/SYNTH_2.txt"), 15000), "touch copies all three"); wait_page("FileMove");
    for (int i = 0; i < 3; i++) { char path[128]; snprintf(path, sizeof path, "/me/SYNTH_COPY/SYNTH_%d.txt", i); CHECK(exists(path), "copy %.90s", path); }
    roots(); browse("/me/SYNTH_BATCH"); touch("多选"); wait_page("FileList");
    for (int i = 0; i < 3; i++) { snprintf(args_json, sizeof args_json, "{\"id\":%u}", (unsigned)ids[i]); command("files.mark", args_json); wait_page("FileList"); }
    touch("移动"); wait_page("FileMove"); command("files.browse", "{\"path\":\"/me/SYNTH_MOVE\"}"); wait_page("FileMove"); touch("放到这里"); CHECK(WAIT_FOR(!exists("/me/SYNTH_BATCH/SYNTH_2.txt") && exists("/me/SYNTH_MOVE/SYNTH_2.txt"), 15000), "touch moves three"); wait_page("FileMove");
    batch_args_test("copy", "/me/SYNTH_COPY"); CHECK(AI("file.batch", args_json) && g_rc.status == INK_RC_PARTIAL && strstr(g_result, "SYNTH_0.txt") && strstr(g_result, "exists"), "AI partial collision names failed files: %.150s", g_result);
    CHECK(!ink_fs_mkdir("/me/SYNTH_PARTIAL") && !ink_fs_write_atomic("/me/SYNTH_PARTIAL/SYNTH_1.txt", "OLD", 3), "partial fixture"); batch_args_test("copy", "/me/SYNTH_PARTIAL"); CHECK(AI("file.batch", args_json) && g_rc.status == INK_RC_PARTIAL && strstr(g_result, "SYNTH_1.txt"), "one failure with two successes");
    CHECK(exists("/me/SYNTH_PARTIAL/SYNTH_0.txt") && exists("/me/SYNTH_PARTIAL/SYNTH_2.txt"), "partial successes are real");
    quiet(); on_ui(check_partial); capture("PartialFailure");
    command("file.write", "{\"path\":\"/me/SYNTH_AI_TXT.txt\",\"text\":\"SYNTH_AI_NEW\"}"); command("file.write", "{\"path\":\"/me/SYNTH_AI_TXT.txt\",\"text\":\"SYNTH_AI_EDIT\",\"replace\":true}");
    read_file("/me/SYNTH_AI_TXT.txt"); CHECK(!strcmp(saved, "SYNTH_AI_EDIT"), "AI creates and edits through file.write");
    snprintf(args_json, sizeof args_json, "{\"all_path\":\"/me/SYNTH_COPY\"}"); CHECK(AI("file.delete_many", args_json) && g_rc.status == INK_RC_PENDING_CONFIRMATION, "AI batch deletion has same confirmation");
    ink_confirm_view_t ai_views[4]; int ai_n = ink_confirm_list(ai_views, 4); CHECK(ai_n == 1, "AI delete confirmation exists");
    if (ai_n) CHECK(!ink_confirm_decide(ai_views[0].token, true, INK_SRC_TOUCH), "human approves AI batch");
    CHECK(WAIT_FOR(!exists("/me/SYNTH_COPY/SYNTH_0.txt") && !exists("/me/SYNTH_COPY/SYNTH_1.txt") && !exists("/me/SYNTH_COPY/SYNTH_2.txt"), 15000), "AI batch deletes three files");
    roots(); browse("/me/SYNTH_MOVE"); touch("多选"); wait_page("FileList"); touch("全选"); wait_page("FileList"); touch("删除"); quiet(); ink_confirm_view_t views[4]; int n = ink_confirm_list(views, 4);
    CHECK(n == 1 && !strcmp(views[0].cap, "file.delete_many") && exists("/me/SYNTH_MOVE/SYNTH_0.txt"), "batch deletion waits for existing human confirmation");
    if (n) { CHECK(ink_confirm_decide(views[0].token, true, INK_SRC_AI) != 0, "AI cannot accept deletion"); CHECK(!ink_confirm_decide(views[0].token, true, INK_SRC_TOUCH), "person confirms"); }
    CHECK(WAIT_FOR(!exists("/me/SYNTH_MOVE/SYNTH_0.txt") && !exists("/me/SYNTH_MOVE/SYNTH_1.txt") && !exists("/me/SYNTH_MOVE/SYNTH_2.txt"), 15000), "three deletes finish");
    CHECK(ink_member_switch("kid") == 0, "switch child"); roots(); browse("/me");
    CHECK(!exists("/me/SYNTH_OPS/SYNTH_NOTE.txt"), "member files isolated");
    CHECK(AI("file.write", "{\"path\":\"/family/SYNTH_FORBIDDEN.txt\",\"text\":\"SYNTH\",\"replace\":true}") && g_rc.status == INK_RC_REJECTED, "child cannot edit family data");
    CHECK(AI("file.batch", "{\"op\":\"copy\",\"all_path\":\"/members/owner/SYNTH_COPY\",\"to\":\"/me\"}") && g_rc.status != INK_RC_DONE, "child cannot batch adult files");
}
static void run(void *unused) {
    (void)unused; vtk_host_call(options, NULL); CHECK(WAIT_FOR(ink_member_space_open(), 10000), "member space opens"); test_start_home(); quiet();
    if (!strcmp(phase, "write")) { folder_and_editor(); ink_probe_count("test.finished"); hal_power_reboot(); return; }
    roots(); browse("/me/SYNTH_OPS"); select_file("/me/SYNTH_OPS/SYNTH_NOTE.txt"); touch("编辑文本"); wait_page("FileEdit"); on_ui(check_recovered); capture("RecoveredDraft"); touch("保存"); wait_page("FileActions");
    read_file("/me/SYNTH_OPS/SYNTH_NOTE.txt"); CHECK(!strcmp(saved, "SYNTH_SAVED_AND_UNSAVED草稿"), "recovered draft saved");
    batch_ops(); ink_probe_count("test.finished");
}
void ink_main(void) { product_main(); osal_task_cfg_t cfg = {.name = "files-ops-test", .fn = run, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL}; CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start file operations test"); }
