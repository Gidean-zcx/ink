/* files-3: the reference walk (APPS.md A10) by touch and by the AI, sorting, deletion and replacement cards,
 * favourites through Flipper's code, member isolation, refresh counts and pictures. Synthetic files only. */
#include "../../system/apps/files/files.c"
#define ink_main product_main
#include "../../system/boot/main.c"
#undef ink_main
#include "../integration/whole_product.h"
#include <stdlib.h>
#include "ink/rotation.h"

static char which[16], text_size[4], scene[48], args_json[512];
static const char *wanted, *expected_page;
static lv_obj_t *found;
static bool ready;
static void options(void *ctx) { (void)ctx; snprintf(which, sizeof which, "%s", getenv("INK_FILES3_CASE")); snprintf(text_size, sizeof text_size, "%s", getenv("INK_FILES3_TEXT") ? getenv("INK_FILES3_TEXT") : ""); }
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void quiet(void) { CHECK(WAIT_FOR(settled(), 15000), "display settles"); }
#include "../child_app_fixture.h"
static void command(const char *cap, const char *args) { CHECK(AI(cap, args) && g_rc.status == INK_RC_DONE, "%s %.80s: %d %.80s", cap, args ? args : "", g_rc.status, g_rc.detail); }
static void poll(void) { ready = !F.busy && !strcmp(ink_ui_page_name(), expected_page); }
static void wait_page(const char *name) {
    expected_page = name; ready = false;
    for (unsigned i = 0; i < 750 && !ready; i++) { on_ui(poll); if (!ready) delay_ms(20); }
    CHECK(ready, "page %s is ready (actual %s, busy %d)", name, ink_ui_page_name(), F.busy); quiet();
}
static void locate(lv_obj_t *obj) {
    if (lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN)) return;
    if (lv_obj_check_type(obj, &lv_label_class) && !strcmp(lv_label_get_text(obj), wanted)) found = obj;
    for (uint32_t i = 0; i < lv_obj_get_child_count(obj); i++) locate(lv_obj_get_child(obj, (int)i));
}
static void target(void) {
    found = NULL; lv_obj_update_layout(lv_screen_active()); locate(lv_screen_active()); CHECK(found != NULL, "visible %.80s", wanted);
    if (found) { lv_area_t a; lv_obj_get_coords(found, &a); g_btn_x[0] = (a.x1 + a.x2) / 2; g_btn_y[0] = (a.y1 + a.y2) / 2; }
}
static void touch(const char *label) {
    quiet(); wanted = label; on_ui(target);
    if (found) { double before = sim_metric_value("input.touch_delivery_ms"); tap(g_btn_x[0], g_btn_y[0]); CHECK(WAIT_FOR(sim_metric_value("input.touch_delivery_ms") > before, 3000), "driver received %.60s", label); }
    quiet();
}
static bool shown_now;
static void look(void) { found = NULL; lv_obj_update_layout(lv_screen_active()); locate(lv_screen_active()); shown_now = found != NULL; }
static void touch_paged(const char *label) { /* the cover's next-page key until the row is on screen */
    wanted = label; for (int i = 0; i < 4; i++) { quiet(); on_ui(look); if (shown_now) break; key(INK_KEY_NEXT); }
    touch(label);
}
static void frame(void *ctx) { (void)ctx; sim_display_snapshot(scene); }
static void shot(const char *name) {
    quiet(); snprintf(scene, sizeof scene, "%s%s", name, which[0] == 'c' ? "-child" : text_size[0] == '3' ? "-max" : "-default");
    vtk_event_at(vtk_now() + SIM_MS(1), frame, NULL); delay_ms(5);
}
static bool exists(const char *path) { ink_fs_stat_t st; return !ink_fs_stat(path, &st); }
static uint32_t id_of(const char *path) { ink_fs_entry_t e; uint32_t cursor = 0; while (ink_fs_index("/me", INK_FS_ANY, &cursor, &e) == 1) if (!strcmp(e.path, path)) return e.id; return 0; }
static void write_file(const char *path, size_t n) {
    static char body[2048]; memset(body, 'S', sizeof body); CHECK(n <= sizeof body && !ink_fs_write_atomic(path, body, n), "synthetic %.80s", path);
}
static void fixture(void) {
    CHECK(!ink_fs_mkdir("/me/SYNTH_文档/SYNTH_子") && !ink_fs_mkdir("/me/SYNTH_目标") && !ink_fs_mkdir("/me/SYNTH_章") && !ink_fs_mkdir("/me/SYNTH_翻页"), "synthetic folders");
    write_file("/me/SYNTH_文档/报告.txt", 1500); write_file("/me/SYNTH_文档/草稿.txt", 1200); write_file("/me/SYNTH_文档/计划.txt", 1400);
    write_file("/me/SYNTH_文档/SYNTH_子/报告2.txt", 300); write_file("/me/SYNTH_目标/报告.txt", 100); write_file("/me/SYNTH_目标/草稿.txt", 100);
    write_file("/me/SYNTH_章/第10章.txt", 100); write_file("/me/SYNTH_章/第2章.txt", 900); write_file("/me/SYNTH_章/第1章.txt", 500);
    for (int i = 1; i <= 7; i++) { char path[64]; snprintf(path, sizeof path, "/me/SYNTH_翻页/SYNTH_%d.txt", i); write_file(path, 100 * (size_t)i); }
    CHECK(id_of("/me/SYNTH_文档/计划.txt") && id_of("/me/SYNTH_文档/SYNTH_子/报告2.txt"), "the index has the synthetic files");
}
static void roots(void) { command("system.home", NULL); command("app.open", "{\"id\":\"files\"}"); wait_page("Files"); }
static void browse(const char *path) { snprintf(args_json, sizeof args_json, "{\"path\":\"%s\"}", path); command("files.browse", args_json); wait_page("FileList"); }
static void snap(void) { CHECK(ink_ui_snapshot(g_snap, sizeof g_snap, NULL) > 0, "page snapshot"); CHECK(ink_ui_context_snapshot(g_snap, sizeof g_snap, NULL) > 0, "fits the AI buffer"); }
static double dispatches(void) {
    static const char *const keys[] = {"A2", "FAST", "UI", "TEXT", "FLASH_UI", "FULL"}; double n = 0; char key[48];
    for (int i = 0; i < 6; i++) { snprintf(key, sizeof key, "display.dispatch.%s", keys[i]); n += sim_metric_value(key); snprintf(key, sizeof key, "display.dispatch.%s+clean", keys[i]); n += sim_metric_value(key); }
    return n;
}
static double mark_at;
static void count_begin(const char *name) { quiet(); char m[64]; snprintf(m, sizeof m, "files3.%s.before", name); ink_probe_mark(m); mark_at = dispatches(); }
static void count_end(const char *name) {
    quiet(); delay_ms(300); quiet(); char m[64]; snprintf(m, sizeof m, "files3.%s.after", name); ink_probe_mark(m);
    snprintf(m, sizeof m, "files3.refresh.%s", name); ink_probe_value(m, dispatches() - mark_at);
}
static void row_bounds(void) { /* the marked row and the count line, for the trace check in run.py */
    int marked_slot = 0;
    for (int i = 0; i < 4; i++) if (F.slots[i] && !strcmp(lv_label_get_text(lv_obj_get_child(F.slots[i], 0)), "SYNTH_2.txt")) marked_slot = i;
    lv_area_t a, b; lv_obj_get_coords(F.slots[marked_slot], &a); lv_obj_get_coords(F.notice, &b);
    int w, h, x0, y0, x1, y1; hal_panel_info_t info; hal_display_get_info(&info); ink_display_native_size(&w, &h);
    ink_logical_to_native(w, h, info.rotation_deg, a.x1 < b.x1 ? a.x1 : b.x1, b.y1 < a.y1 ? b.y1 : a.y1, &x0, &y0);
    ink_logical_to_native(w, h, info.rotation_deg, a.x2 > b.x2 ? a.x2 : b.x2, a.y2 > b.y2 ? a.y2 : b.y2, &x1, &y1);
    ink_probe_value("test.mark.x1", x0 < x1 ? x0 : x1); ink_probe_value("test.mark.y1", y0 < y1 ? y0 : y1);
    ink_probe_value("test.mark.x2", x0 < x1 ? x1 : x0); ink_probe_value("test.mark.y2", y0 < y1 ? y1 : y0);
}
static uint32_t found_id;
static void search_results(void) {
    snap(); CHECK(F.rows == 2 && strstr(g_snap, "在 我的 > SYNTH_文档 > SYNTH_子") && strstr(g_snap, "在 我的 > SYNTH_文档\""), "two results, each with its place: %.300s", g_snap);
}
static void jumped(void) {
    snap(); CHECK(!strcmp(F.path, "/me/SYNTH_文档/SYNTH_子") && F.found == found_id && strstr(g_snap, "找到 · ") && strstr(F.position.selected, "file:"), "the result's folder, its page, marked as found");
}
static void walk_marks(void) { CHECK(F.multi && F.marked_count == 3 && F.position.page == 1, "back in the folder on the same page, three files still selected (page %zu, %zu marked)", F.position.page, F.marked_count); }
static void space_page(void) { snap(); CHECK(F.bstate == B_SPACE && strstr(lv_label_get_text(F.notice), "") && strstr(g_snap, "放得下的先做"), "the space check says what fits goes first"); }
static void question(void) { snap(); CHECK(F.bstate == B_ASK && strstr(g_snap, "\"of\":2") && strstr(g_snap, F.conf_at ? "草稿.txt" : "报告.txt"), "question %zu of 2: %.200s", F.conf_at + 1, g_snap); }
static void result_page(void) {
    snap(); CHECK(F.bstate == B_DONE && F.succeeded == 1 && F.skipped == 1 && F.failed == 1 && strstr(lv_label_get_text(F.notice), "成功 1 个，跳过 1 个，失败 1 个"), "result: %zu done, %zu skipped, %zu failed (%s)", F.succeeded, F.skipped, F.failed, lv_label_get_text(F.notice));
    CHECK(F.fail_n == 1 && strstr(F.fails[0], "计划.txt") && strstr(F.fails[0], "空间不够"), "the failure names the file and why: %s", F.fails[0]);
    CHECK(strstr(F.renamed, "报告 (1).txt") != NULL, "keep both shows the new name: %s", F.renamed);
}
static void select_three(bool by_touch) {
    if (by_touch) { touch("多选"); wait_page("FileList"); touch_paged("报告.txt"); touch_paged("草稿.txt"); touch_paged("计划.txt"); return; }
    command("files.multi", NULL); wait_page("FileList");
    const char *names[] = {"/me/SYNTH_文档/报告.txt", "/me/SYNTH_文档/草稿.txt", "/me/SYNTH_文档/计划.txt"};
    for (int i = 0; i < 3; i++) { snprintf(args_json, sizeof args_json, "{\"id\":%u}", (unsigned)id_of(names[i])); command("files.mark", args_json); }
    command("files.next", NULL); quiet();
}
static void copy_walk(bool by_touch) {
    roots(); browse("/me/SYNTH_文档");
    if (by_touch) { shot("list"); touch("搜索"); wait_page("FileSearch"); command("files.name", NULL); command("text.insert", "{\"text\":\"报告\"}"); command("text.submit", NULL); quiet();
        count_begin("search"); touch("搜索"); wait_page("FileSearch"); count_end("search"); }
    else { command("files.search", NULL); wait_page("FileSearch"); command("files.search", "{\"query\":\"报告\"}"); wait_page("FileSearch"); }
    on_ui(search_results); if (by_touch) shot("search");
    found_id = id_of("/me/SYNTH_文档/SYNTH_子/报告2.txt");
    if (by_touch) touch("报告2.txt"); else { snprintf(args_json, sizeof args_json, "{\"path\":\"/me/SYNTH_文档/SYNTH_子\",\"find\":%u}", (unsigned)found_id); command("files.browse", args_json); }
    wait_page("FileList"); on_ui(jumped); if (by_touch) shot("found");
    command("files.up", NULL); wait_page("FileList"); CHECK(!strcmp(F.path, "/me/SYNTH_文档"), "back where the search began");
    select_three(by_touch); wait_page("FileList"); if (by_touch) shot("multi");
    sim_sd_set_free(1500 + 400); /* room for one of the three */
    if (by_touch) { touch("拷贝"); wait_page("FileMove"); shot("dest-root"); touch("我的"); wait_page("FileMove"); touch("SYNTH_目标"); wait_page("FileMove"); shot("dest"); touch("拷贝到这里"); }
    else { command("files.move", "{\"copy\":true}"); wait_page("FileMove"); command("files.browse", "{\"path\":\"/me/SYNTH_目标\"}"); wait_page("FileMove"); command("files.place", NULL); }
    wait_page("FileBatch"); CHECK(WAIT_FOR(F.bstate == B_SPACE, 10000), "the check finishes"); on_ui(space_page); if (by_touch) shot("space");
    if (by_touch) touch("开始"); else command("files.start", NULL);
    wait_page("FileBatch"); on_ui(question); if (by_touch) { shot("conflict"); count_begin("question"); touch("保留两个"); wait_page("FileBatch"); count_end("question"); } else command("files.conflict", "{\"choice\":\"keep_both\"}");
    wait_page("FileBatch"); on_ui(question);
    if (by_touch) { count_begin("result"); touch("跳过"); } else command("files.conflict", "{\"choice\":\"skip\"}");
    CHECK(WAIT_FOR(F.bstate == B_DONE, 20000), "the batch finishes"); wait_page("FileBatch"); if (by_touch) count_end("result");
    on_ui(result_page); if (by_touch) shot("result");
    CHECK(exists("/me/SYNTH_目标/报告 (1).txt") && !exists("/me/SYNTH_目标/计划.txt"), "keep both made 报告 (1).txt; 计划.txt did not fit");
    sim_sd_set_free(-1);
    if (by_touch) touch("完成"); else command("files.up", NULL);
    wait_page("FileList"); on_ui(walk_marks);
}
static void move_listed(void) { CHECK(F.rows == 1 && F.marked_count == 0, "only the subfolder is left (%zu rows, %zu marked)", F.rows, F.marked_count); }
static void move_walk(bool by_touch) {
    if (by_touch) touch("移动"); else command("files.move", NULL);
    wait_page("FileMove"); CHECK(!strcmp(F.dest, "/me/SYNTH_目标"), "starts where the member last put files");
    if (by_touch) touch("放到这里"); else command("files.place", NULL);
    wait_page("FileBatch"); CHECK(WAIT_FOR(F.bstate == B_ASK, 10000), "names are taken in the target"); on_ui(question);
    if (by_touch) { touch("后面的同名都这样"); wait_page("FileBatch"); touch("保留两个"); } else command("files.conflict", "{\"choice\":\"keep_both\",\"all\":true}");
    CHECK(WAIT_FOR(F.bstate == B_DONE, 20000), "the move finishes"); wait_page("FileBatch");
    CHECK(F.succeeded == 3 && exists("/me/SYNTH_目标/报告 (2).txt") && exists("/me/SYNTH_目标/草稿 (1).txt") && exists("/me/SYNTH_目标/计划.txt"), "all three moved, keeping both names (%zu)", F.succeeded);
    if (by_touch) touch("完成"); else command("files.up", NULL);
    wait_page("FileList"); on_ui(move_listed);
}
static void refresh_checks(void) {
    roots(); count_begin("enter"); touch("我的"); wait_page("FileList"); count_end("enter");
    browse("/me/SYNTH_翻页"); command("files.multi", NULL); wait_page("FileList");
    touch("SYNTH_1.txt"); count_begin("mark"); touch("SYNTH_2.txt"); count_end("mark"); on_ui(row_bounds);
    count_begin("turn"); key(INK_KEY_NEXT); count_end("turn"); command("files.multi", NULL); wait_page("FileList");
    count_begin("sort"); command("files.sort", "{\"by\":\"size\",\"desc\":true}"); count_end("sort");
    count_begin("menu"); snprintf(args_json, sizeof args_json, "{\"id\":%u}", (unsigned)id_of("/me/SYNTH_翻页/SYNTH_1.txt")); command("files.select", args_json); wait_page("FileActions"); count_end("menu");
    shot("menu"); command("files.up", NULL); wait_page("FileList"); command("files.sort", "{\"by\":\"name\",\"desc\":false}");
}
static const char *order[3];
static void sorted(void) { for (int i = 0; i < 3; i++) CHECK(F.rows == 3 && !strcmp(F.items[F.view[i]].name, order[i]), "row %d is %s (%s)", i, order[i], F.rows > (size_t)i ? F.items[F.view[i]].name : "-"); }
static void sort_case(void) {
    roots(); browse("/me/SYNTH_章");
    order[0] = "第1章.txt"; order[1] = "第2章.txt"; order[2] = "第10章.txt"; on_ui(sorted);
    touch("排序"); wait_page("FileSort"); shot("sort"); touch("大小"); wait_page("FileList");
    order[0] = "第10章.txt"; order[1] = "第1章.txt"; order[2] = "第2章.txt"; on_ui(sorted);
    touch("排序"); wait_page("FileSort"); touch("倒序"); wait_page("FileList");
    order[0] = "第2章.txt"; order[1] = "第1章.txt"; order[2] = "第10章.txt"; on_ui(sorted);
    permit_child_files(); CHECK(ink_member_switch("kid") == 0, "another member"); roots(); CHECK(ink_member_switch("owner") == 0, "back to the owner");
    roots(); browse("/me/SYNTH_章"); on_ui(sorted); CHECK(F.sort == 2 && F.desc, "the member's sort is remembered");
}
static void delete_case(void) {
    roots(); browse("/me/SYNTH_文档"); CHECK(AI("files.all", NULL) && g_rc.status == INK_RC_INVALID, "select-all needs the multi mode");
    command("files.multi", NULL); wait_page("FileList"); command("files.all", NULL); quiet(); touch("删除"); quiet();
    ink_confirm_view_t v[4]; int n = 0; WAIT_FOR((n = ink_confirm_list(v, 4)) > 0, 3000);
    CHECK(n == 1 && !strcmp(v[0].cap, "file.delete_many") && strstr(v[0].detail, "删除这 3 个文件") && strstr(v[0].detail, "报告.txt") && strstr(v[0].detail, "……"), "the card counts and names the files: %s", n ? v[0].detail : "-");
    if (n) CHECK(!ink_confirm_decide(v[0].token, false, INK_SRC_TOUCH), "the person cancels");
    quiet(); CHECK(exists("/me/SYNTH_文档/报告.txt") && exists("/me/SYNTH_文档/草稿.txt") && exists("/me/SYNTH_文档/计划.txt"), "all three are still there");
    command("files.move", "{\"copy\":true}"); wait_page("FileMove"); command("files.browse", "{\"path\":\"/me/SYNTH_目标\"}"); wait_page("FileMove"); command("files.place", NULL);
    wait_page("FileBatch"); CHECK(WAIT_FOR(F.bstate == B_ASK, 10000), "two names taken");
    command("files.conflict", "{\"choice\":\"replace\",\"all\":true}"); quiet();
    WAIT_FOR((n = ink_confirm_list(v, 4)) > 0, 3000);
    CHECK(n == 1 && !strcmp(v[0].cap, "file.batch_replace") && strstr(v[0].detail, "替换 2 个同名文件"), "one card for the replacement: %s", n ? v[0].detail : "-");
    if (n) { CHECK(ink_confirm_decide(v[0].token, true, INK_SRC_AI) != 0, "the AI cannot say yes"); CHECK(!ink_confirm_decide(v[0].token, false, INK_SRC_TOUCH), "the person cancels"); }
    quiet(); ink_fs_stat_t st; CHECK(!ink_fs_stat("/me/SYNTH_目标/报告.txt", &st) && st.size == 100 && !exists("/me/SYNTH_目标/计划.txt"), "nothing was replaced or copied");
}
static void all_text(void) { ink_text_select(0, ink_text_char_offset(ink_text_value(), strlen(ink_text_value()))); }
static bool fav_has(const char *path) { files_prefs_t *p = files_prefs_begin(); bool on = strstr(p->fav, path) != NULL; files_prefs_end(false); return on; }
static void home_fav(void) { snap(); CHECK(strstr(g_snap, "收藏 · ") != NULL, "home lists the favourite first"); }
static void fav_case(void) {
    roots(); browse("/me/SYNTH_文档"); snprintf(args_json, sizeof args_json, "{\"id\":%u}", (unsigned)id_of("/me/SYNTH_文档/草稿.txt")); command("files.select", args_json); wait_page("FileActions");
    touch("收藏"); wait_page("FileList"); CHECK(fav_has("/me/SYNTH_文档/草稿.txt"), "pinned through Flipper's Pin");
    roots(); on_ui(home_fav); shot("home");
    command("files.select", "{\"path\":\"/me/SYNTH_文档/草稿.txt\"}"); wait_page("FileActions"); shot("menu-fav");
    command("files.up", NULL); wait_page("FileList");
    command("files.select", args_json); wait_page("FileActions");
    command("files.name", NULL); quiet(); on_ui(all_text); command("text.insert", "{\"text\":\"新稿.txt\"}"); command("text.submit", NULL); quiet(); touch("改名"); wait_page("FileActions");
    CHECK(exists("/me/SYNTH_文档/新稿.txt") && fav_has("/me/SYNTH_文档/新稿.txt") && !fav_has("草稿.txt"), "the favourite follows the rename");
    touch("删除"); quiet(); ink_confirm_view_t v[4]; int n = 0; WAIT_FOR((n = ink_confirm_list(v, 4)) > 0, 3000);
    CHECK(n == 1 && strstr(v[0].detail, "新稿.txt"), "deleting asks first");
    if (n) CHECK(!ink_confirm_decide(v[0].token, true, INK_SRC_TOUCH), "the person confirms");
    CHECK(WAIT_FOR(!exists("/me/SYNTH_文档/新稿.txt"), 10000), "removed"); quiet();
    CHECK(WAIT_FOR(!fav_has("新稿.txt"), 5000), "Flipper's delete unpinned it");
    command("files.fav", "{\"path\":\"/me/SYNTH_文档/计划.txt\"}"); CHECK(fav_has("/me/SYNTH_文档/计划.txt"), "the AI pins with the same capability");
    command("files.fav", "{\"path\":\"/me/SYNTH_文档/SYNTH_子\"}"); CHECK(fav_has("/me/SYNTH_文档/SYNTH_子/\n"), "a folder is pinned by its own line");
    command("file.move", "{\"from\":\"/me/SYNTH_文档/计划.txt\",\"to\":\"/me/SYNTH_目标/计划.txt\"}");
    roots(); CHECK(WAIT_FOR(!fav_has("计划.txt"), 10000), "the rescan drops what moved away"); quiet();
    CHECK(fav_has("SYNTH_子/"), "the folder stays");
}
static void child_case(void) {
    write_file("/me/SYNTH_文档/SYNTH_只给大人.txt", 10); CHECK(!ink_fs_write_atomic("/family/SYNTH_家庭.txt", "SYNTH", 5), "family file");
    permit_child_files(); CHECK(ink_member_switch("kid") == 0, "the child");
    roots(); on_ui(snap); CHECK(!strstr(g_snap, "/members/") && !strstr(g_snap, "的文件"), "no other member's files for a child"); shot("home");
    CHECK(AI("files.browse", "{\"path\":\"/members/owner/SYNTH_文档\"}") && g_rc.status == INK_RC_REJECTED, "the adult's folder is out of reach");
    CHECK(AI("files.fav", "{\"path\":\"/members/owner/SYNTH_文档/报告.txt\"}") && g_rc.status == INK_RC_REJECTED, "and cannot be pinned");
    write_file("/me/SYNTH_孩子.txt", 10); browse("/me"); command("files.search", NULL); wait_page("FileSearch"); command("files.search", "{\"query\":\"SYNTH\"}"); wait_page("FileSearch");
    on_ui(snap); CHECK(strstr(g_snap, "SYNTH_孩子.txt") && !strstr(g_snap, "只给大人"), "search stays in the child's own space"); shot("search");
    command("files.up", NULL); wait_page("FileList"); command("files.up", NULL); wait_page("Files"); browse("/family"); on_ui(snap); shot("list");
    CHECK(!strstr(g_snap, "files.new") && !strstr(g_snap, "files.folder"), "the family folder is read-only for a child");
    ink_fs_entry_t e; uint32_t cursor = 0; CHECK(ink_fs_index("/family", INK_FS_TEXT, &cursor, &e) == 1, "family file indexed");
    snprintf(args_json, sizeof args_json, "{\"id\":%u}", (unsigned)e.id); command("files.select", args_json); wait_page("FileActions"); on_ui(snap); shot("menu");
    CHECK(!strstr(g_snap, "file.delete") && !strstr(g_snap, "file.move") && strstr(g_snap, "files.move"), "a child can copy family files but not change them");
    CHECK(ink_member_switch("owner") == 0, "back to the owner");
}
static void states_case(void) { /* empty folder and a card lent to a computer */
    CHECK(!ink_fs_mkdir("/me/SYNTH_空"), "empty folder"); roots(); browse("/me/SYNTH_空"); on_ui(snap); CHECK(strstr(g_snap, "这里还没有文件") || F.rows == 0, "empty state"); shot("empty");
    command("files.folder", NULL); wait_page("FileActions"); shot("folder-menu"); command("files.up", NULL); wait_page("FileList");
    sim_sd_set_present(false); command("files.browse", "{\"path\":\"/me/SYNTH_空\"}"); quiet(); CHECK(WAIT_FOR(!F.busy, 10000) && F.error == 1, "no card: one sentence (%d)", F.error); shot("error");
    sim_sd_set_present(true); quiet();
}
/* Flipper's copied code through the shims: the type table, what the menu holds, the favourites kept in step, and
 * every browse event reaching the page action or capability it stands for. */
static const char *menu_items[FILES_MENU_MAX]; static int menu_n;
static bool menu_has(const char *item) { for (int i = 0; i < menu_n; i++) if (!strcmp(menu_items[i], item)) return true; return false; }
static void menu_of(const char *path, bool dir, bool favorites) { files_fl_select(path, dir, favorites); menu_n = files_fl_menu(menu_items); }
static void flipper_tables(void) {
    static const struct { const char *path; int type; } types[] = {{"/me/a.txt", FILES_TYPE_TEXT}, {"/me/a.MD", FILES_TYPE_TEXT}, {"/me/b.epub", FILES_TYPE_BOOK},
        {"/me/c.opus", FILES_TYPE_AUDIO}, {"/me/d.JPG", FILES_TYPE_IMAGE}, {"/me/e.zip", FILES_TYPE_UNKNOWN}, {"/me/f.bin", FILES_TYPE_UNKNOWN}, {"/me/f.pdf", FILES_TYPE_UNKNOWN}};
    for (size_t i = 0; i < sizeof types / sizeof *types; i++) CHECK(files_fl_type(types[i].path, false) == types[i].type, "flipper-types: %s", types[i].path);
    CHECK(files_fl_type("/me/a.txt", true) == FILES_TYPE_TEXT && files_fl_type("/me/dir", true) == FILES_TYPE_FOLDER, "flipper-types: folders, unless named like a known file (original rule)");
    menu_of("/me/SYNTH_文档/报告.txt", false, false);
    CHECK(menu_n == 9 && !strcmp(menu_items[0], "Run in app") && menu_has("Pin") && menu_has("Rename") && menu_has("Delete") && menu_has("Copy") && menu_has("Edit") && menu_has("Read"), "flipper-menu: a text file (%d)", menu_n);
    menu_of("/family/SYNTH_家庭.txt", false, false); CHECK(!menu_has("Read") && menu_has("Edit"), "flipper-menu: reading only from the member's own space");
    menu_of("/me/SYNTH_文档/SYNTH_子", true, false);
    CHECK(!menu_has("Run in app") && !menu_has("Pin") && menu_has("Rename") && menu_has("Delete") && menu_has("Pin folder") && !menu_has("Edit"), "flipper-menu: a folder (Run and Pin are --- in the original; our appended Pin folder)");
    menu_of("/me/SYNTH_文档/f.bin", false, false); CHECK(!menu_has("Run in app") && !menu_has("Pin") && menu_has("Rename") && menu_has("Delete"), "flipper-menu: an unknown type keeps rename and delete");
    menu_of("/me/SYNTH_文档/报告.txt", false, true); CHECK(menu_has("Unpin") && !menu_has("Delete") && !menu_has("Rename"), "flipper-menu: the favourites tab (Move is the unwired reorder, Delete ---)");
}
static const char *ev_path; static bool ev_dir; static int ev; static bool ev_removed;
static void ev_ui(void) { if (ev_path) files_fl_select(ev_path, ev_dir, false); files_fl_event(ev); }
static void send_event(const char *path, bool dir, int event) { ev_path = path; ev_dir = dir; ev = event; on_ui(ev_ui); quiet(); }
static void pick_ui(void) { for (size_t i = 0; i < F.count; i++) if (!strcmp(F.items[i].name, "报告.txt")) { F.selected = F.items[i]; F.selected_id = F.items[i].id; F.known = true; } }
static void renamed_ui(void) { files_fl_renamed("/me/SYNTH_文档/报告.txt", "/me/SYNTH_文档/改名.txt"); }
static void deleted_ui(void) { files_fl_deleted("/me/SYNTH_文档/改名.txt", ev_removed); }
static char sel_path[INK_FS_PATH_MAX];
static void sel_ui(void) { snprintf(sel_path, sizeof sel_path, "%s", F.selected.path); }
static void flipper_events(void) {
    roots(); browse("/me/SYNTH_文档");
    send_event("/me/SYNTH_文档/SYNTH_子", true, FILES_EV_ENTER); wait_page("FileList"); CHECK(!strcmp(F.path, "/me/SYNTH_文档/SYNTH_子"), "flipper-events: EnterDir is files.browse");
    send_event(NULL, false, FILES_EV_EXIT); wait_page("FileList"); CHECK(!strcmp(F.path, "/me/SYNTH_文档"), "flipper-events: Exit is files.up");
    send_event(NULL, false, FILES_EV_NEXT); CHECK(F.position.page == 1, "flipper-events: LoadNextItems is files.next");
    send_event(NULL, false, FILES_EV_PREV); CHECK(F.position.page == 0, "flipper-events: LoadPrevItems is files.prev");
    on_ui(pick_ui); on_ui(sel_ui);
    send_event(sel_path, false, FILES_EV_MENU); wait_page("FileActions"); CHECK(!strcmp(ink_ui_page_name(), "FileActions"), "flipper-events: FileMenuOpen is files.select");
    send_event(sel_path, false, FILES_EV_RUN); CHECK(WAIT_FOR(F.preview_text[0] == 'S', 5000), "flipper-events: FileMenuRun of text is files.preview");
    send_event(sel_path, false, FILES_EV_PIN); wait_page("FileList"); CHECK(fav_has("/me/SYNTH_文档/报告.txt"), "flipper-events: FileMenuPin is files.fav and closes the menu");
    on_ui(renamed_ui); CHECK(fav_has("/me/SYNTH_文档/改名.txt") && !fav_has("/me/SYNTH_文档/报告.txt"), "flipper-fav: rename keeps the favourite");
    ev_removed = false; on_ui(deleted_ui); CHECK(fav_has("改名.txt"), "flipper-fav: a refused removal keeps the favourite (our change)");
    ev_removed = true; on_ui(deleted_ui); quiet(); CHECK(!fav_has("改名.txt"), "flipper-fav: a removal unpins it");
    wait_page("FileList"); snprintf(args_json, sizeof args_json, "{\"id\":%u}", (unsigned)id_of("/me/SYNTH_文档/计划.txt")); command("files.select", args_json); wait_page("FileActions");
    on_ui(sel_ui); send_event(sel_path, false, FILES_EV_DELETE);
    ink_confirm_view_t v[4]; int n = 0; WAIT_FOR((n = ink_confirm_list(v, 4)) > 0, 3000);
    CHECK(n == 1 && !strcmp(v[0].cap, "file.delete") && strstr(v[0].detail, "计划.txt"), "flipper-events: FileMenuDelete is file.delete with its card");
    if (n) CHECK(!ink_confirm_decide(v[0].token, false, INK_SRC_TOUCH), "cancel");
    quiet(); send_event(NULL, false, FILES_EV_REFRESH); wait_page("FileList"); CHECK(exists("/me/SYNTH_文档/计划.txt"), "flipper-events: ListRefresh reads the folder again");
}
static void flipper_case(void) { on_ui(flipper_tables); flipper_events(); }
static void run(void *unused) {
    (void)unused; vtk_host_call(options, NULL); CHECK(WAIT_FOR(ink_member_space_open(), 10000), "member space opens"); test_start_home(); quiet();
    if (text_size[0]) { snprintf(args_json, sizeof args_json, "{\"id\":\"display.font_size\",\"value\":%s}", text_size); command("settings.set", args_json); quiet(); }
    fixture();
    if (!strcmp(which, "walk")) { copy_walk(true); move_walk(true); refresh_checks(); }
    else if (!strcmp(which, "ai")) { copy_walk(false); move_walk(false); }
    else if (!strcmp(which, "sort")) sort_case();
    else if (!strcmp(which, "delete")) delete_case();
    else if (!strcmp(which, "fav")) fav_case();
    else if (!strcmp(which, "child")) child_case();
    else if (!strcmp(which, "states")) states_case();
    else if (!strcmp(which, "flipper")) flipper_case();
    CHECK(WAIT_FOR(ink_db_idle(), 10000), "data durable before stopping"); quiet(); ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main(); osal_task_cfg_t cfg = {.name = "files3-test", .fn = run, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start files-3 test");
}
