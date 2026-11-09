/* Real capabilities and immediate power cuts on synthetic member/card data. */
#include "../services_finish/product.h"
#include "ink/ui_shell.h"

unsigned ink_test_index_path_id(const char *path);
unsigned ink_test_index_count(void);
static char descriptions[3][160];
static volatile bool pin_done;
static void pin_reply(void *ctx, int result, uint32_t wait) {
    (void)ctx; (void)wait; CHECK(result == INK_PIN_OK, "set synthetic parent password"); pin_done = true;
}
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void quiet(void) { CHECK(WAIT_FOR(settled(), 12000), "UI settles"); }
static void confirmation(void) {
    CHECK(ink_member_set_pin("owner", NULL, "1234", pin_reply, NULL) == 0 && WAIT_FOR(pin_done, 5000),
        "install synthetic parent password before checking the gate");
    const char *args[] = {"{\"clear_media\":true}", "{\"clear_media\":false}", NULL};
    for (int i = 0; i < 3; i++) {
        ink_confirm_view_t v = {0};
        CHECK(AI("system.factory_reset", args[i]) && g_rc.status == INK_RC_PENDING_CONFIRMATION,
            "direct AI reset waits for confirmation");
        CHECK(ink_confirm_list(&v, 1) == 1 && !strcmp(v.cap, "system.factory_reset") && v.parent,
            "real pending card keeps parent password gate");
        snprintf(descriptions[i], sizeof descriptions[i], "%s", v.detail);
        CHECK(strstr(v.detail, i ? "存储卡上的书和音频保留" : "同时清空存储卡上的书和音频"),
            "confirmation states actual media scope: %.150s", v.detail);
        CHECK(ink_confirm_decide(v.token, true, INK_SRC_AI) < 0, "AI cannot confirm reset");
        CHECK(ink_confirm_decide(v.token, false, INK_SRC_TOUCH) == 0, "cancel without deleting anything");
    }
    CHECK(strcmp(descriptions[0], descriptions[1]) && !strcmp(descriptions[1], descriptions[2]),
        "true differs from false; omitted media flag means preserve");
    CHECK(sim_metric_value("system.factory_resets") == 0, "no reset executed");
}
static void fixture(const char *member, const char *cap) {
    ink_db_t *db = ink_db_open_member(ink_db_find_def("activity.log"), member);
    char row[160];
    int n = snprintf(row, sizeof row, "{\"cap\":\"%s\",\"status\":\"done\"}", cap);
    ink_db_write_t w = {.data = row, .len = (size_t)n, .sort = osal_wall_us() / 1000000, .src = INK_SRC_AI};
    CHECK(db && ink_db_put(db, &w, NULL) == 0 && ink_db_wait(db, ink_db_last_ver(db), 5000000) == 0,
        "seed distinct synthetic member record");
    ink_db_close(db);
}
static int x, y;
static void locate(lv_obj_t *o) {
    if (!lv_obj_is_visible(o)) return;
    if (lv_obj_check_type(o, &lv_label_class) && !strcmp(lv_label_get_text(o), "SYNTHETIC_CHILD")) {
        lv_area_t a; lv_obj_get_coords(o, &a); x = (a.x1 + a.x2) / 2; y = (a.y1 + a.y2) / 2;
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) locate(lv_obj_get_child(o, i));
}
static void find_child_row(void) { x = y = -1; lv_obj_update_layout(lv_screen_active()); locate(lv_screen_active()); }
static void expect_receipts(bool child) {
    quiet(); on_ui(read_screen);
    CHECK(!strcmp(ink_ui_page_name(), "AiReceipts"), "real receipt page opens");
    CHECK(strstr(g_snap, child ? "SYNTHETIC_CHILD" : "SYNTHETIC_OWNER") &&
        !strstr(g_snap, child ? "SYNTHETIC_OWNER" : "SYNTHETIC_CHILD"),
        "receipt snapshot contains only the requested member");
    if (child && strcmp(ink_member_current()->id, "kid")) CHECK(strstr(g_snap, "孩子"), "receipt page states child target");
}
static void receipts(void) {
    fixture("owner", "SYNTHETIC_OWNER"); fixture("kid", "SYNTHETIC_CHILD");
    CHECK(AI("shell.ai_log", "{\"member\":\"kid\"}") && g_rc.status == INK_RC_DONE, "adult opens child log"); quiet();
    CHECK(AI("system.home", NULL) && g_rc.status == INK_RC_DONE, "leave child log for home"); quiet();
    CHECK(AI("shell.ai_receipts", NULL) && g_rc.status == INK_RC_DONE, "independent receipt entry"); expect_receipts(false);
    CHECK(AI("shell.ai_log", "{\"member\":\"kid\"}") && g_rc.status == INK_RC_DONE, "reopen child log"); quiet();
    on_ui(find_child_row); CHECK(x >= 0 && y >= 0, "child log row is visible");
    if (x >= 0 && y >= 0) tap(x, y);
    expect_receipts(true);
    CHECK(AI("shell.ai_receipts", "{\"member\":\"kid\"}") && g_rc.status == INK_RC_DONE,
        "AI can explicitly select permitted child"); expect_receipts(true);
    CHECK(ink_member_switch("kid") == 0, "enter child space"); quiet();
    CHECK(AI("shell.ai_receipts", "{\"member\":\"owner\"}") && g_rc.status == INK_RC_REJECTED,
        "child cannot open adult receipts");
    CHECK(AI("shell.ai_receipts", NULL) && g_rc.status == INK_RC_DONE, "child opens own receipts"); expect_receipts(true);
}
static bool ready(void) {
    char state[32] = "";
    return ink_state_get_str(ink_state_find("file.index_state"), state, sizeof state) && !strcmp(state, "ready");
}
static int64_t books(void) {
    ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL); int64_t n = -1;
    if (j && !ink_json_parse(j, g_result, strlen(g_result)))
        ink_json_int(j, ink_json_find(j, ink_json_find(j, 0, "device"), "books"), &n);
    osal_free(j); return n;
}
static void cut(void *arg) { (void)arg; ink_probe_count("test.power_cut"); sim_storage_power_cut(); }
static void index_case(const char *mode) {
    bool create = strstr(mode, "create") != NULL, reboot = strstr(mode, "reboot") != NULL;
    CHECK(WAIT_FOR(ready(), 30000), "card automatically reaches ready");
    unsigned old_id = ink_test_index_path_id("/content/SYNTHETIC_OLD.txt");
    if (!reboot) {
        CHECK(old_id != 0, "old file already has a stable id");
        ink_db_flush_all(); CHECK(WAIT_FOR(ink_db_idle() && !ink_storage_pending(), 10000), "initial index and completion are durable");
        ink_probe_value("test.old_id", old_id);
        CHECK(create ? ink_fs_write_atomic("/family/SYNTHETIC_NEW.txt", "XXXXX", 5) == 0 :
            ink_fs_move("/family/SYNTHETIC_OLD.txt", "/family/SYNTHETIC_RENAMED.txt") == 0, "file mutation returns success");
        ink_probe_count("test.finished");
        vtk_event_after(SIM_US(1), cut, NULL);
        delay_ms(1000); CHECK(false, "power cut must stop process before background repair");
    } else {
        CHECK(AI("file.usage", NULL) && g_rc.status == INK_RC_DONE && books() == (create ? 8 : 3),
            "usage immediately after ready includes all bytes: %.150s", g_result);
        CHECK(ink_test_index_count() == (create ? 2u : 1u), "reboot restores complete file count");
        if (create) CHECK(ink_test_index_path_id("/content/SYNTHETIC_NEW.txt") != 0, "new file gets id without manual directory query");
        else {
            unsigned expected = (unsigned)strtoul(getenv("INK_AUDIT5_OLD_ID"), NULL, 10);
            ink_fs_entry_t e;
            CHECK(expected && ink_fs_lookup(expected, &e) == 0 && !strcmp(e.path, "/family/SYNTHETIC_RENAMED.txt"),
                "original id resolves to renamed path after reboot");
        }
    }
}
static void run(void *arg) {
    (void)arg;
    CHECK(WAIT_FOR(ink_member_space_open(), 10000), "member space opens"); test_start_home(); quiet();
    const char *mode = getenv("INK_AUDIT5_MODE");
    if (!strcmp(mode, "confirmation")) confirmation();
    else if (!strcmp(mode, "receipts")) receipts();
    else index_case(mode);
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name = "audit5-test", .fn = run, .stack_bytes = 16384,
        .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start audit5 regression");
}
