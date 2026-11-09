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
static lv_obj_t *text_label(lv_obj_t *root, const char *text) {
    if (lv_obj_has_flag(root, LV_OBJ_FLAG_HIDDEN)) return NULL;
    if (lv_obj_check_type(root, &lv_label_class) && strstr(lv_label_get_text(root), text)) return root;
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); i++) {
        lv_obj_t *o = text_label(lv_obj_get_child(root, (int32_t)i), text); if (o) return o;
    }
    return NULL;
}
static void readable_label(const char *text) {
    lv_obj_t *label = text_label(lv_screen_active(), text);
    CHECK(label, "receipt text is present on screen: %s", text);
    if (!label) return;
    lv_obj_update_layout(label);
    CHECK(lv_label_get_long_mode(label) == LV_LABEL_LONG_WRAP, "receipt text wraps without ellipsis");
    lv_point_t need;
    lv_text_get_size(&need, lv_label_get_text(label), lv_obj_get_style_text_font(label, 0), 0, 0,
        lv_obj_get_width(label), LV_TEXT_FLAG_NONE);
    CHECK(lv_obj_get_height(label) >= need.y, "all receipt lines fit the label");
    lv_area_t a, parent; lv_obj_get_coords(label, &a); lv_obj_get_coords(lv_obj_get_parent(label), &parent);
    CHECK(a.x1 >= parent.x1 && a.x2 <= parent.x2 && a.y1 >= parent.y1 && a.y2 <= parent.y2,
        "receipt text stays inside its F4 row");
    lv_obj_t *next = lv_obj_get_child(lv_obj_get_parent(label), 1);
    if (label != next) { lv_area_t b; lv_obj_get_coords(next, &b); CHECK(a.y2 < b.y1, "receipt title and metadata do not overlap"); }
}
static void check_real_receipt(void) {
    read_snapshot(); readable_label("setting:display.light"); readable_label("做好了");
    CHECK(strstr(snapshot, "system.undo") && strstr(snapshot, "setting:display.light"), "undo is offered to AI with the same affected object");
}
static void check_long_receipt(void) { read_snapshot(); readable_label("页面或家人已变，操作已停止"); readable_label("synthetic:6"); }
static void hide_panel(void) {
    ink_receipt_t rc; ink_cap_invoke("ai.panel.hide", NULL, INK_SRC_AI, 0, &rc);
    CHECK(rc.status == INK_RC_DONE, "generic panel hide succeeds");
    read_snapshot(); CHECK(!strstr(snapshot, "AiOffline"), "hidden error is absent from AI snapshot");
}
static void reveal_panel(void) { ink_shell_ai_reveal(); }
static bool has_snapshot(const char *text) { on_ui(read_snapshot); return strstr(snapshot, text) != NULL; }
static void open_cold_log(void) {
    ink_receipt_t rc; ink_cap_invoke("shell.ai_log", NULL, INK_SRC_AI, 0, &rc);
    CHECK(rc.status == INK_RC_DONE, "open cold member log"); read_snapshot();
    CHECK(strstr(snapshot, "正在读取记录") && !strstr(snapshot, "\"total\":0"), "loading is visible to AI, not a false empty result");
    CHECK(text_label(lv_screen_active(), "正在读取记录"), "cold log shows loading on screen");
}
static void cold_records(void) {
    CHECK(BY_HAND("member.add", "{\"id\":\"synthetic-child\",\"role\":\"child\"}") && g_rc.status == INK_RC_PENDING_CONFIRMATION, "request a synthetic new member");
    unsigned token = 0; sscanf(g_rc.object, "confirm:%u", &token);
    CHECK(ink_confirm_decide(token, true, INK_SRC_TOUCH) == 0 && WAIT_FOR(ink_member_exists("synthetic-child"), 12000), "confirm synthetic member creation");
    CHECK(ink_member_switch("synthetic-child") == 0, "enter new member"); quiet();
    ink_db_t *cached = ink_db_open_cached_member(ink_db_find_def("activity.log"), ink_member_current()->id);
    CHECK(!cached, "new member activity collection is cold"); if (cached) ink_db_close(cached);
    static ink_db_def_t defs[40]; static ink_db_t *held[40]; static char names[40][32];
    bool failing = !strcmp(mode, "loadfail");
    if (failing) for (unsigned i = 0; i < 40; i++) {
        snprintf(names[i], sizeof names[i], "test.ai_pages_%u", i);
        defs[i] = (ink_db_def_t){.name = names[i], .scope = INK_DB_MEMBER, .schema = 1, .json = true};
        held[i] = ink_db_open(&defs[i], NULL); if (!held[i]) break;
    }
    on_ui(open_cold_log);
    CHECK(WAIT_FOR(has_snapshot(failing ? "记录暂时读不了" : "\"total\":0"), 12000), "one-shot BG load reports its real result"); quiet();
    if (failing) {
        CHECK(!strstr(snapshot, "\"total\":0"), "load failure is not an empty successful log");
        for (unsigned i = 0; i < 40; i++) if (held[i]) ink_db_close(held[i]);
    } else {
        CHECK(AI("activity.list", NULL) && g_rc.status == INK_RC_DONE, "existing activity read agrees with cold-load result");
    }
    CHECK(ink_cap_register(&EFFECT, 1) == 0, "register new member operation");
    CHECK(AI(EFFECT.id, "{\"n\":1,\"status\":0}") && g_rc.status == INK_RC_DONE, "first new member operation succeeds");
    CHECK(WAIT_FOR(log_count() == 1, 12000), "first activity is stored");
    CHECK(WAIT_FOR(has_snapshot("\"total\":1"), 12000), "open page recovers when activity becomes available");
    capture(failing ? "LoadRecovered" : "NewMemberLoaded");
    CHECK(strstr(snapshot, "合成测试操作"), "first operation appears without reopening");
    CHECK(ink_member_switch("owner") == 0, "switch away from newly loaded page"); quiet();
    CHECK(AI("shell.ai_log", NULL), "open original member log"); quiet(); on_ui(check_child_screen);
}
static void real_receipts(void) {
    int64_t before = ink_setting_get("display.light");
    CHECK(AI("settings.set", "{\"id\":\"display.light\",\"value\":3}") && g_rc.status == INK_RC_DONE && g_rc.undo_ms == 10000, "real brightness change returns undo window");
    char undo[160]; snprintf(undo, sizeof undo, "{\"object\":\"%s\",\"revision\":%u}", g_rc.object, g_rc.revision);
    CHECK(WAIT_FOR(log_count() == 1, 12000), "real operation is in activity log");
    CHECK(AI("shell.ai_receipts", "{\"id\":1}"), "open real receipt"); capture("RealReceipt"); on_ui(check_real_receipt);
    press("system.undo");
    CHECK(WAIT_FOR(ink_setting_get("display.light") == before, 5000), "touch undo restores the original setting");
    CHECK(AI("system.undo", undo) && g_rc.status != INK_RC_UNDONE, "the same undo cannot execute twice");
    CHECK(AI("settings.set", "{\"id\":\"display.light\",\"value\":3}") && g_rc.status == INK_RC_DONE, "create a fresh undo for AI");
    CHECK(WAIT_FOR(log_count() == 3, 12000), "undo result and new change are logged");
    CHECK(AI("shell.ai_receipts", "{\"id\":3}"), "open fresh undo receipt"); quiet(); on_ui(read_snapshot);
    CHECK(strstr(snapshot, "system.undo") && strstr(snapshot, "setting:display.light"), "fresh receipt exposes undo again");
    ink_json_t json; CHECK(ink_json_parse(&json, snapshot, strlen(snapshot)) == 0, "parse AI page actions");
    int actions = ink_json_find(&json, 0, "actions"), undo_args = -1;
    for (int t = actions + 1; t < json.ntok; t++) if (json.tok[t].type == 1 && ink_json_eq(&json, ink_json_find(&json, t, "capability"), "system.undo")) undo_args = ink_json_find(&json, t, "args_json");
    CHECK(undo_args >= 0 && json.tok[undo_args].end - json.tok[undo_args].start < (int)sizeof undo, "AI snapshot carries exact undo arguments");
    if (undo_args >= 0) {
        CHECK(ink_json_str(&json, undo_args, undo, sizeof undo) > 0, "decode exact AI undo arguments");
        CHECK(AI("system.undo", undo) && g_rc.status == INK_RC_UNDONE && ink_setting_get("display.light") == before, "AI uses the receipt's object and revision to undo");
    }
    CHECK(AI("settings.set", "{\"id\":\"display.light\",\"value\":3}") && g_rc.status == INK_RC_DONE, "create expiring undo");
    uint64_t deadline = osal_now_us() + (uint64_t)g_rc.undo_ms * 1000;
    CHECK(WAIT_FOR(osal_now_us() > deadline, 12000), "existing undo window expires");
    CHECK(AI("shell.ai_receipts", NULL), "reopen expired receipts"); quiet(); on_ui(read_snapshot);
    CHECK(!strstr(snapshot, "system.undo"), "expired receipts offer no undo");
    CHECK(WAIT_FOR(log_count() == 6, 12000), "all real AI actions are logged");
    CHECK(ink_cap_register(&EFFECT, 1) == 0 && AI(EFFECT.id, "{\"n\":6,\"status\":5}"), "long receipt fixture");
    CHECK(WAIT_FOR(log_count() == 7, 12000), "long receipt persisted");
    CHECK(AI("settings.set", "{\"id\":\"display.font_size\",\"value\":3}"), "largest supported text size");
    CHECK(AI("shell.ai_receipts", "{\"id\":7}"), "open long receipt at largest text size"); capture("ReceiptsLarge"); on_ui(check_long_receipt);
}
static void run(void *ctx) {
    (void)ctx; vtk_host_call(get_mode, NULL); delay_ms(1800); quiet(); test_start_home(); quiet();
    if (!strcmp(mode, "newmember") || !strcmp(mode, "loadfail")) cold_records();
    else if (!strcmp(mode, "real")) real_receipts();
    else if (strcmp(mode, "records")) {
        CHECK(BY_HAND("reader.open_book", "{\"title\":\"星星邮差\"}") && g_rc.status == INK_RC_DONE, "open the real reader below the error panel");
        quiet(); on_ui(read_snapshot); snprintf(background, sizeof background, "%s", snapshot);
        on_ui(ask);
        CHECK(WAIT_FOR(sim_metric_value("ai.failures") >= 1, 55000), "real AI service reaches an error");
        const char *board = !strcmp(mode, "offline") || !strcmp(mode, "hide") || !strcmp(mode, "lost") ? "AiOffline" : !strcmp(mode, "timeout") ? "AiTimeout" : "AiBudget";
        capture(board); CHECK(strstr(snapshot, board), "correct AI error page: %s", board);
        CHECK(!strcmp(ink_ui_top_name(), board), "error overlay has its own page name");
        CHECK(strstr(snapshot, "ai.pages.close") && (strcmp(mode, "budget") || !strstr(snapshot, "ai.pages.retry")), "error snapshot offers only valid actions");
        if (!strcmp(mode, "lost")) CHECK(strstr(snapshot, "回答没收全") && strstr(snapshot, "不会自动重试"), "mid-request network loss preserves its truthful explanation");
        if (!strcmp(mode, "hide")) {
            on_ui(hide_panel); quiet();
            CHECK(!strcmp(ink_ui_top_name(), "book"), "generic hide releases the error overlay");
            key(HAL_KEY_NEXT); quiet(); on_ui(read_snapshot);
            CHECK(strcmp(background, snapshot), "reader next key works while AI error is hidden");
            on_ui(reveal_panel); quiet(); on_ui(read_snapshot);
            CHECK(!strcmp(ink_ui_top_name(), "AiOffline") && strstr(snapshot, "AiOffline"), "reveal restores the same error snapshot");
            key(HAL_KEY_HOME); quiet(); key(HAL_KEY_PREV);
        } else if (!strcmp(mode, "offline") || !strcmp(mode, "lost")) press("ai.pages.close");
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
