/* Exercise the production search and existing input/navigation/data paths. */
#include "../../system/ui/search.c"
#define ink_main product_main
#include "../../system/boot/main.c"
#undef ink_main
#include "../integration/whole_product.h"
#include <stdlib.h>

static char scene[24], snap[4096], old_args[160];
static uint32_t saved_version;
static const char *wanted;
static lv_obj_t *found;
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void quiet(void) { CHECK(WAIT_FOR(settled(), 10000), "the search display settles"); }
static void read_scene(void *arg) {
    const char *s = getenv("INK_SEARCH_SCENE");
    snprintf(scene, sizeof scene, "%s", s ? s : "Search");
}
static void take_frame(void *arg) {
    CHECK(settled(), "capture a settled search frame");
    ink_probe_count("test.snapshot.settled");
    ink_probe_count(!strcmp(scene, "Search") ? "test.snapshot.Search" : "test.snapshot.SearchEmpty");
    sim_display_snapshot("scene");
}
static void capture(const char *name) {
    if (strcmp(scene, name)) return;
    quiet(); vtk_event_at(vtk_now() + SIM_MS(1), take_frame, NULL); delay_ms(5);
}
static void read_snapshot(void) {
    CHECK(ink_ui_snapshot(snap, sizeof snap, NULL) > 0, "search has a complete AI snapshot");
    CHECK(ink_ui_context_snapshot(g_snap, sizeof g_snap, NULL) > 0, "search fits the production AI context");
}
static void find_label(lv_obj_t *o) {
    if (lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN)) return;
    if (lv_obj_check_type(o, &lv_label_class) && !strcmp(lv_label_get_text(o), wanted)) found = o;
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) find_label(lv_obj_get_child(o, i));
}
static void locate(void) {
    found = NULL; find_label(lv_screen_active());
    CHECK(found, "the original result label is visible");
    if (!found) return;
    lv_area_t a; lv_obj_update_layout(lv_obj_get_parent(found)); lv_obj_get_coords(lv_obj_get_parent(found), &a);
    g_btn_x[0] = (a.x1 + a.x2) / 2; g_btn_y[0] = (a.y1 + a.y2) / 2;
}
static void check_first(void) {
    read_snapshot();
    CHECK(S.total == 7 && S.position.page == 0 && strstr(snap, "\"pages\":3") && strstr(snap, "联系人"), "first page identifies contact type and pagination");
    CHECK(strstr(snap, "合成检索") && strstr(snap, "query_version") && !S.pending, "snapshot contains committed query and query version");
    snprintf(old_args, sizeof old_args, "%s", S.args[0]); saved_version = S.version;
}
static void check_second(void) {
    read_snapshot();
    CHECK(S.position.page == 1 && S.total == 7 && S.version > saved_version && strstr(snap, "备忘"), "second page changes cursor/version and includes memo type");
    wanted = S.items[0].title; locate();
}
static void check_return(void) {
    read_snapshot();
    CHECK(S.position.page == 1 && !strcmp(S.position.selected, S.items[0].id) && strstr(snap, S.items[0].title), "back restores the original result page and selected stable object");
}
static void check_empty(void) {
    read_snapshot();
    CHECK(!strcmp(ink_ui_page_name(), "SearchEmpty") && !S.total && !S.list && !S.error[0], "no match displays the real SearchEmpty activity");
    CHECK(strstr(snap, "没有找到相关内容"), "AI reads the same empty-state text as the person");
    wanted = "没有找到相关内容"; found = NULL; find_label(lv_screen_active());
    CHECK(found, "the empty-state text is visible");
}
static void late_query(void) {
    ink_receipt_t rc = {.status = INK_RC_DONE, .result = "{\"total\":1,\"results\":[]}"};
    size_t before = S.total;
    loaded((void *)(uintptr_t)(S.version - 1), &rc);
    CHECK(S.total == before && !strcmp(ink_ui_page_name(), "SearchEmpty"), "an obsolete query reply cannot replace newer results");
}
static void focus_field(void) {
    lv_area_t a; lv_obj_update_layout(S.field); lv_obj_get_coords(S.field, &a);
    g_btn_x[0] = (a.x1 + a.x2) / 2; g_btn_y[0] = (a.y1 + a.y2) / 2;
}
static void show(const char *q, bool compatibility) {
    char args[256]; snprintf(args, sizeof args, "{\"query\":\"%s\"}", q);
    CHECK(AI("search.show", args) && g_rc.status == INK_RC_DONE, "search entry accepts the committed keyword");
    CHECK(WAIT_FOR(!S.pending, 15000), "background search returns without blocking UI"); quiet();
}
static void query_has(const char *q, const char *type) {
    char args[256]; snprintf(args, sizeof args, "{\"query\":\"%s\",\"version\":7}", q);
    CHECK(AI("search.query", args) && g_rc.status == INK_RC_DONE && strstr(g_result, type) && strstr(g_result, "\"version\":7"), "AI uses search.query for type %s", type);
}
static void fixture(void) {
    for (unsigned i = 0; i < 5; i++) {
        char args[192]; snprintf(args, sizeof args, "{\"name\":\"合成检索联系人%u\",\"number\":\"0000000000%u\"}", i, i);
        CHECK(AI("contacts.put", args) && g_rc.status == INK_RC_DONE, "create synthetic owner contact %u", i);
    }
    ink_db_t *db = ink_db_open_member(ink_db_find_def("memo.notes"), "owner");
    const char *body = "{\"body\":\"合成检索备忘\\n仅供测试\",\"deleted\":false}";
    uint32_t version = 0;
    ink_db_write_t wr = {.data = body, .len = strlen(body), .src = INK_SRC_TOUCH, .written_ver = &version};
    CHECK(db && ink_db_put(db, &wr, NULL) == 0 && ink_db_wait(db, version, 10000000) == 0, "create a real synthetic memo record");
    ink_db_close(db);
    CHECK(ink_fs_write_atomic("/me/search/合成检索文件.txt", "SYNTHETIC", 9) == 0, "create synthetic owner file");
}
static void check_file(void) {
    read_snapshot();
    CHECK(S.position.page == 2 && strstr(snap, "文件") && strstr(snap, "\"visible\":1"), "last page has only the typed file result");
    ink_receipt_t rc;
    CHECK(ink_cap_invoke_json("search.open", S.args[0], INK_SRC_AI, 0, &rc) == INK_RC_DONE, "AI opens the same search.open action");
    read_snapshot();
    CHECK(!strcmp(ink_ui_page_name(), "SearchFile") && strstr(snap, "合成检索文件.txt"), "file result opens its actual stable-index information");
}
static void check_member(void) {
    CHECK(ink_member_switch("kid") == 0, "switch from owner to child");
    char hidden[4096] = "old";
    CHECK(snapshot(hidden, sizeof hidden) < 0 && !hidden[0], "new member cannot read old result snapshot before UI drains");
    ink_receipt_t rc;
    CHECK(ink_cap_invoke_json("search.open", old_args, INK_SRC_AI, 0, &rc) != INK_RC_DONE, "new member cannot open an old result");
}
static void run(void *arg) {
    vtk_host_call(read_scene, NULL);
    CHECK(WAIT_FOR(ink_member_space_open(), 10000), "owner space opens"); test_start_home(); quiet(); fixture();
    query_has("电话", "应用"); query_has("音量", "设置"); query_has("00000000000", "联系人");
    query_has("仅供测试", "备忘"); query_has("合成检索文件", "文件");
    CHECK(AI("search.query", "{\"query\":\"合成检索\",\"cursor\":3,\"version\":9}") && g_rc.status == INK_RC_DONE &&
        strstr(g_result, "\"cursor\":3") && strstr(g_result, "\"next_cursor\":6") && strstr(g_result, "\"total\":7"), "AI search cursor returns a real partial page");
    show("合成检索", true); on_ui(check_first);
    key(INK_KEY_NEXT); CHECK(WAIT_FOR(!S.pending, 15000), "physical next waits for its requested page"); quiet(); on_ui(check_second);
    CHECK(AI("search.open", old_args) && g_rc.status == INK_RC_STALE, "old query version cannot open new page results");
    tap(g_btn_x[0], g_btn_y[0]); quiet();
    CHECK(!strcmp(ink_ui_page_name(), "ContactEdit"), "physical touch opens the real contact");
    CHECK(BY_HAND("shell.back", NULL) && g_rc.status == INK_RC_DONE, "return through normal navigation"); quiet(); on_ui(check_return);
    CHECK(AI("search.next", NULL) && g_rc.status == INK_RC_DONE, "AI uses the same next-page capability");
    CHECK(WAIT_FOR(!S.pending, 15000), "last page query completes"); quiet(); on_ui(check_file);
    CHECK(BY_HAND("shell.back", NULL) && g_rc.status == INK_RC_DONE, "file info returns to results"); quiet();
    show("", false);
    CHECK(AI("search.focus", NULL) && g_rc.status == INK_RC_DONE, "AI can focus the F2 search field");
    CHECK(AI("text.insert", "{\"text\":\"合成检索\"}") && g_rc.status == INK_RC_DONE && AI("text.submit", NULL) && g_rc.status == INK_RC_DONE, "AI commits through the production text service");
    CHECK(WAIT_FOR(!S.pending, 15000), "AI-edited query completes"); quiet(); on_ui(check_first);
    show("", false); on_ui(focus_field); tap(g_btn_x[0], g_btn_y[0]); quiet();
    CHECK(ink_text_focused(), "physical tap focuses the same F2 search field");
    CHECK(BY_HAND("clipboard.write", "{\"text\":\"合成检索\"}") && g_rc.status == INK_RC_DONE &&
        BY_HAND("keyboard.edit", "{\"action\":4}") && g_rc.status == INK_RC_DONE &&
        BY_HAND("keyboard.key", "{\"code\":10}") && g_rc.status == INK_RC_DONE, "person pastes and submits with real keyboard capabilities");
    CHECK(WAIT_FOR(!S.pending, 15000), "human-edited query completes"); quiet(); on_ui(check_first);
    show("设", false);
    CHECK(AI("search.focus", NULL) && g_rc.status == INK_RC_DONE, "show fixed keyboard for Search reference"); capture("Search");
    show("夜邮", false); on_ui(check_empty); on_ui(late_query); capture("SearchEmpty");
    show("合成检索", false); on_ui(check_member);
    CHECK(AI("search.query", "{\"query\":\"合成检索\"}") && g_rc.status == INK_RC_DONE &&
        strstr(g_result, "\"member\":\"kid\"") && strstr(g_result, "\"total\":0") && !strstr(g_result, "合成检索联系人"), "child query cannot see owner contacts, memos or files");
    CHECK(AI("search.query", "{\"query\":\"音量\"}") && g_rc.status == INK_RC_DONE && strstr(g_result, "\"total\":0"), "child search hides adult settings");
    show("合成检索", false); on_ui(check_empty);
    CHECK(ink_fs_write_atomic("/me/search/孩子检索.txt", "SYNTHETIC", 9) == 0, "create child-owned synthetic file");
    query_has("孩子检索", "文件");
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name = "search-test", .fn = run, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start search acceptance driver");
}
