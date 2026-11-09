/* Real AI failures, activity writes, pagination and member isolation. */
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>
#include "ink/sys/ai.h"
#include "ink/sys/data.h"
#include "ink/sys/time.h"
#include "ink/ui_shell.h"
#define ink_main product_main
#include "../system/boot/main.c"
#undef ink_main
#include "integration/whole_product.h"

static char mode[24], snapshot[3072], background[3072];
static const char *scene;
static bool found;
static int x, y;
static const char *button;
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void quiet(void) { CHECK(WAIT_FOR(settled(), 12000), "frame settles"); }
static void read_snapshot(void) { CHECK(ink_ui_snapshot(snapshot, 3072, NULL) > 0, "complete snapshot within AI request budget"); }
static void get_mode(void *ctx) {
    (void)ctx; const char *value = getenv("INK_AI_PAGES_CASE");
    snprintf(mode, sizeof mode, "%s", value ? value : "records");
}
static lv_obj_t *bound(lv_obj_t *root) {
    if (lv_obj_has_flag(root, LV_OBJ_FLAG_HIDDEN)) return NULL;
    const char *cap = ink_shell_bound_cap(root);
    if (cap && !strcmp(cap, button)) return root;
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); i++) {
        lv_obj_t *o = bound(lv_obj_get_child(root, (int32_t)i)); if (o) return o;
    }
    return NULL;
}
static void locate(void) {
    lv_obj_t *o = bound(lv_layer_sys()); if (!o) o = bound(lv_screen_active());
    found = o != NULL;
    if (o) { lv_area_t a; lv_obj_update_layout(o); lv_obj_get_coords(o, &a); x = (a.x1 + a.x2) / 2; y = (a.y1 + a.y2) / 2; }
}
static void press(const char *cap) {
    button = cap; on_ui(locate); CHECK(found, "visible bound button: %s", cap); if (found) tap(x, y);
}
static void save(void *ctx) {
    (void)ctx; char dir[600], path[650];
    int n = snprintf(dir, sizeof dir, "%s/%s", g_sim.out_dir, scene);
    if (n < 0 || (size_t)n >= sizeof dir || (mkdir(dir, 0700) && errno != EEXIST)) abort();
    n = snprintf(path, sizeof path, "%s/snapshot.json", dir);
    if (n < 0 || (size_t)n >= sizeof path) abort();
    FILE *f = fopen(path, "wb"); if (!f || fputs(snapshot, f) == EOF || fclose(f)) abort();
}
static void snap_event(void *ctx) {
    (void)ctx; CHECK(settled(), "snapshot frame is settled");
    if (settled()) { char metric[80]; snprintf(metric, sizeof metric, "test.snapshot.%s", scene); ink_probe_count(metric); sim_display_snapshot(scene); }
}
static void capture(const char *name) {
    quiet(); scene = name; on_ui(read_snapshot); vtk_host_call(save, NULL);
    vtk_event_at(vtk_now() + SIM_MS(1), snap_event, NULL); delay_ms(5);
}
static void ask(void) { ink_ai_ask_text("合成测试问题"); }
static void effect(ink_call_t *c) {
    int64_t n = 0, status = 0; ink_args_get_int(&c->args, "n", &n); ink_args_get_int(&c->args, "status", &status);
    c->rc.status = (ink_rc_t)status; c->rc.revision = (uint32_t)n;
    snprintf(c->rc.object, sizeof c->rc.object, "synthetic:%lld", (long long)n);
}
static const ink_cap_t EFFECT = {.id = "test.ai_effect", .title = "合成测试操作", .effect = INK_FX_LOCAL_WRITE,
    .child_ok = true, .exec = INK_EXEC_BG, .run = effect,
    .input = "{\"type\":\"object\",\"properties\":{\"n\":{\"type\":\"integer\"},\"status\":{\"type\":\"integer\"}}}"};
static int log_count(void) {
    ink_db_t *db = ink_db_open_cached_member(ink_db_find_def("activity.log"), ink_member_current()->id);
    int count = db ? ink_db_count(db) : 0; if (db) ink_db_close(db); return count;
}
static void all_statuses(void) {
    static const char *const expected[] = {"已撤销", "已取消", "结果不确定，不会自动重试", "操作无效", "页面或家人已变，操作已停止", "不能做", "等你确认", "还没做，等联网", "做好了一部分", "做好了"};
    bool seen[10] = {0};
    for (int page = 0; page < 10; page++) {
        quiet(); on_ui(read_snapshot);
        for (int i = 0; i < 10; i++) {
            char label[160]; snprintf(label, sizeof label, "\"label\":\"%s\"", expected[i]);
            if (strstr(snapshot, label)) seen[i] = true;
        }
        CHECK(strstr(snapshot, "pagination") && strstr(snapshot, "activity:"), "F4 snapshot identifies records and page");
        if (!strstr(snapshot, "\"id\":\"next\"")) break;
        CHECK(AI("ai.pages.next", NULL) && g_rc.status == INK_RC_DONE, "AI can read the next page");
    }
    for (int i = 0; i < 10; i++) CHECK(seen[i], "receipt status visible: %s", expected[i]);
}
static void select_first(void) {
    int px = ink_ui_px(INK_CANVAS_W / 2);
    int py = ink_ui_px(INK_STATUS_BAR + INK_PRIMARY + INK_GAP + ink_ui_row_double() / 2);
    x = px; y = py;
}
static bool private_text(lv_obj_t *root) {
    if (lv_obj_has_flag(root, LV_OBJ_FLAG_HIDDEN)) return false;
    if (lv_obj_check_type(root, &lv_label_class) && (strstr(lv_label_get_text(root), "合成测试操作") || strstr(lv_label_get_text(root), "synthetic:"))) return true;
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); i++) if (private_text(lv_obj_get_child(root, (int32_t)i))) return true;
    return false;
}
static void check_child_screen(void) {
    read_snapshot(); CHECK(!strstr(snapshot, "synthetic:") && !strstr(snapshot, "合成测试操作"), "child cannot see adult receipt text");
    CHECK(!private_text(lv_screen_active()) && !private_text(lv_layer_sys()), "adult receipt text is absent from visible child controls");
}
static void run(void *ctx) {
    (void)ctx; vtk_host_call(get_mode, NULL); delay_ms(1800); quiet(); test_start_home(); quiet();
    if (strcmp(mode, "records")) {
        CHECK(BY_HAND("reader.open_book", "{\"title\":\"星星邮差\"}") && g_rc.status == INK_RC_DONE, "open the real reader below the error panel");
        quiet(); on_ui(read_snapshot); snprintf(background, sizeof background, "%s", snapshot);
        on_ui(ask);
        CHECK(WAIT_FOR(sim_metric_value("ai.failures") >= 1, 55000), "real AI service reaches an error");
        const char *board = !strcmp(mode, "offline") ? "AiOffline" : !strcmp(mode, "timeout") ? "AiTimeout" : "AiBudget";
        capture(board); CHECK(strstr(snapshot, board), "correct AI error page: %s", board);
        CHECK(!strcmp(ink_ui_top_name(), board), "error overlay has its own page name");
        CHECK(strstr(snapshot, "ai.pages.close") && (strcmp(mode, "budget") || !strstr(snapshot, "ai.pages.retry")), "error snapshot offers only valid actions");
        if (!strcmp(mode, "offline")) press("ai.pages.close");
        else if (!strcmp(mode, "timeout")) key(HAL_KEY_HOME);
        else CHECK(AI("ai.pages.close", NULL) && g_rc.status == INK_RC_DONE, "AI uses the same close capability");
        quiet(); CHECK(!strcmp(ink_ui_top_name(), ink_ui_page_name()), "continue reading removes overlay: top=%s page=%s close=%.0f", ink_ui_top_name(), ink_ui_page_name(), sim_metric_value("action.user.ai.pages.close"));
        on_ui(read_snapshot); CHECK(!strcmp(ink_ui_page_name(), "book") && !strcmp(background, snapshot), "reader text and position are preserved");
        if (!strcmp(mode, "budget")) {
            CHECK(ink_member_switch("kid") == 0, "switch to child for quota explanation"); quiet(); on_ui(ask);
            CHECK(WAIT_FOR(sim_metric_value("ai.failures") >= 2, 55000), "child gets real budget response");
            quiet(); on_ui(read_snapshot); CHECK(strstr(snapshot, "可以问爸爸妈妈") && !strstr(snapshot, "手机上加"), "child quota wording");
            CHECK(ink_member_switch("owner") == 0, "switch while error is visible"); quiet(); CHECK(strcmp(ink_ui_top_name(), "AiBudget"), "old member error overlay disappears");
        }
    } else {
        CHECK(AI("shell.ai_log", NULL) && g_rc.status == INK_RC_DONE, "open AI activity app");
        quiet(); on_ui(read_snapshot); CHECK(strstr(snapshot, "\"total\":0"), "honest empty activity list");
        CHECK(ink_cap_register(&EFFECT, 1) == 0, "register synthetic operation fixture");
        for (int i = 0; i < 10; i++) {
            char args[100]; snprintf(args, sizeof args, "{\"n\":%d,\"status\":%d}", i + 1, i);
            CHECK(AI(EFFECT.id, args) && g_rc.status == (ink_rc_t)i, "real bus operation %d writes its receipt", i);
        }
        CHECK(WAIT_FOR(log_count() == 10, 12000), "real activity_flush writes ten records");
        CHECK(AI("system.home", NULL), "reopen after activity writes");
        CHECK(AI("app.open", "{\"id\":\"ai-log\"}") && g_rc.status == INK_RC_DONE, "human app launcher exposes activity records");
        capture("AiLog"); CHECK(strstr(snapshot, "合成测试操作") && strstr(snapshot, "\"total\":10"), "current member records appear");
        CHECK(strstr(snapshot, "activity:10") && !strstr(snapshot, "activity:1\""), "recent records are first and off-page records stay hidden");
        key(HAL_KEY_NEXT); quiet(); on_ui(read_snapshot); CHECK(strstr(snapshot, "\"page\":2"), "hardware paging uses F4");
        CHECK(AI("ai.pages.prev", NULL) && g_rc.status == INK_RC_DONE, "AI can return to the first page");
        quiet(); on_ui(select_first); tap(x, y); capture("AiReceipts");
        CHECK(strstr(snapshot, "AiReceipts") && strstr(snapshot, "synthetic:10") && strstr(snapshot, "已撤销"), "row touch opens the selected operation's receipt");
        all_statuses(); press("shell.back"); quiet(); CHECK(!strcmp(ink_ui_page_name(), "AiLog"), "receipts return to activity list");
        CHECK(AI("shell.ai_receipts", "{\"id\":1}") && g_rc.status == INK_RC_DONE, "open oldest receipt by stable record id");
        quiet(); on_ui(read_snapshot); CHECK(strstr(snapshot, "\"id\":\"activity:1\"") && strstr(snapshot, "synthetic:1\""), "selected receipt is on its page");
        CHECK(ink_member_switch("kid") == 0, "switch away from adult receipts"); quiet(); on_ui(check_child_screen);
        CHECK(AI("shell.ai_log", NULL) && g_rc.status == INK_RC_DONE, "child opens their own log");
        quiet(); on_ui(check_child_screen); CHECK(strstr(snapshot, "\"total\":0"), "child activity list is empty");
        CHECK(AI("shell.ai_receipts", "{\"id\":10}") && g_rc.status == INK_RC_DONE, "try adult record id in child space");
        quiet(); on_ui(check_child_screen); CHECK(strstr(snapshot, "\"total\":0"), "foreign record id does not cross member boundary");
        CHECK(ink_member_switch("owner") == 0, "return to adult"); quiet();
        CHECK(AI("shell.ai_log", NULL) && g_rc.status == INK_RC_DONE, "adult opens unchanged records");
        quiet(); on_ui(read_snapshot); CHECK(strstr(snapshot, "\"total\":10"), "adult records remain isolated and intact");
    }
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name = "ai-pages-test", .fn = run, .stack_bytes = 24576, .priority = OSAL_PRIO_NORMAL,
        .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start scenario driver");
}
