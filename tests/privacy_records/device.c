/* Synthetic member records through the real summary, policy and settings pages. */
#include <stdlib.h>
#include "ink/sys/data.h"
#include "ink/ui_shell.h"
#define ink_main product_main
#include "../../system/boot/main.c"
#undef ink_main
#include "../integration/whole_product.h"
static char snapshot[3072];
static const char *label, *scene;
static int x, y;
static bool captured;
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void quiet(void) { CHECK(WAIT_FOR(settled(), 12000), "frame settles"); }
static void read_snapshot(void) { CHECK(ink_ui_snapshot(snapshot, sizeof snapshot, NULL) > 0, "summary fits the real AI snapshot"); }
static void find_label(lv_obj_t *o) {
    if (lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN)) return;
    if (lv_obj_check_type(o, &lv_label_class) && !strcmp(lv_label_get_text(o), label)) {
        lv_area_t a; lv_obj_update_layout(o); lv_obj_get_coords(o, &a); x = (a.x1 + a.x2) / 2; y = (a.y1 + a.y2) / 2;
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) find_label(lv_obj_get_child(o, i));
}
static void find(void) { x = y = -1; find_label(lv_screen_active()); }
static void press(const char *text) {
    label = text; on_ui(find); CHECK(x >= 0 && y >= 0, "visible control: %s", text);
    if (x >= 0 && y >= 0) tap(x, y);
    quiet();
}
static void save(void *ctx) {
    (void)ctx; char path[600]; int n = snprintf(path, sizeof path, "%s/%s.json", g_sim.out_dir, scene);
    if (n < 0 || (size_t)n >= sizeof path) abort();
    FILE *f = fopen(path, "wb"); if (!f || fputs(snapshot, f) == EOF || fclose(f)) abort();
}
static void frame(void *ctx) {
    (void)ctx; char metric[80]; snprintf(metric, sizeof metric, "test.snapshot.%s", scene);
    CHECK(settled(), "capture settled page"); ink_probe_count(metric); sim_display_snapshot(scene); captured = true;
}
static void capture(const char *name) {
    quiet(); scene = name; on_ui(read_snapshot); vtk_host_call(save, NULL); captured = false;
    vtk_event_at(vtk_now() + SIM_MS(1), frame, NULL); CHECK(WAIT_FOR(captured, 1000), "capture frame");
}
static void fixture(const char *member, int count, const char *cap) {
    ink_db_t *db = ink_db_open_member(ink_db_find_def("activity.log"), member);
    CHECK(db, "open real member collection"); if (!db) return;
    for (int i = 0; i <= count; i++) {
        char json[160]; int64_t when = osal_wall_us() / 1000000 - (i == count ? 366ll * 86400 : i * 60);
        if (i == count && !strcmp(member, "kid")) when = osal_wall_us() / 1000000 - 181ll * 86400;
        int n = snprintf(json, sizeof json, "{\"cap\":\"%s\",\"status\":\"done\",\"when\":%lld}", cap, (long long)when);
        ink_db_write_t wr = {.data = json, .len = (size_t)n, .sort = when, .src = INK_SRC_AI};
        CHECK(ink_db_put(db, &wr, NULL) == INK_DB_OK, "seed synthetic activity");
    }
    CHECK(ink_db_wait(db, ink_db_last_ver(db), 5000000) == INK_DB_OK, "fixture persists"); ink_db_close(db);
}
static int raw_count(const char *member) {
    ink_db_t *db = ink_db_open_cached_member(ink_db_find_def("activity.log"), member);
    int n = db ? ink_db_count(db) : -1; if (db) ink_db_close(db); return n;
}
static bool summary_count(const char *word) { on_ui(read_snapshot); return strstr(snapshot, word) != NULL; }
static void seek(const char *text) {
    for (int i = 0; i < 8; i++) {
        label = text; on_ui(find);
        if (x >= 0) { press(text); return; }
        CHECK(AI("shell.settings_next", NULL), "turn privacy page"); quiet();
    }
    CHECK(false, "privacy entry reachable: %s", text);
}
static void run(void *arg) {
    (void)arg;
    CHECK(WAIT_FOR(ink_member_space_open(), 10000), "member space opens"); test_start_home(); quiet();
    fixture("owner", 2, "synthetic.parent"); fixture("kid", 3, "synthetic.child");
    CHECK(AI("activity.list", "{\"limit\":1}") && g_rc.status == INK_RC_DONE && strstr(g_result, "\"total\":2") && strstr(g_result, "\"keep_days\":365"), "backend returns adult count and retention");
    CHECK(AI("activity.list", "{\"member\":\"kid\",\"limit\":1}") && g_rc.status == INK_RC_DONE && strstr(g_result, "\"total\":3") && strstr(g_result, "\"keep_days\":180"), "adult reads child's count using child's retention");
    CHECK(AI("shell.records", NULL) && g_rc.status == INK_RC_DONE, "open real summary"); quiet();
    CHECK(WAIT_FOR(summary_count("2 件"), 5000), "summary count comes from backend"); capture("Records");
    CHECK(strstr(snapshot, "和 AI 说过的话") && strstr(snapshot, "AI 记得的事") && strstr(snapshot, "运行记录"), "summary has four rows");
    CHECK(strstr(snapshot, "暂时读不到次数") && strstr(snapshot, "暂不能删除"), "missing backends do not invent counts or deletion");
    CHECK(strstr(snapshot, "1 年") && strstr(snapshot, "180 天"), "summary states real retention");
    press("AI 替你做过的事"); CHECK(!strcmp(ink_ui_page_name(), "AiLog"), "activity row opens AiLog"); capture("AiLog");
    CHECK(strstr(snapshot, "synthetic.parent") && strstr(snapshot, "\"total\":2"), "AiLog count and retention match summary");
    CHECK(AI("shell.back", NULL), "return to summary"); quiet(); press("和 AI 说过的话");
    CHECK(!strcmp(ink_ui_page_name(), "AiHome"), "conversation row opens ai app");
    CHECK(AI("shell.back", NULL), "return from conversation"); quiet(); press("AI 记得的事");
    CHECK(!strcmp(ink_ui_page_name(), "AiMemory"), "memory row opens AiMemory"); capture("AiMemory");
    CHECK(strstr(snapshot, "暂时看不到"), "memory remains truthful empty state");
    CHECK(AI("shell.back", NULL), "return from memory"); quiet(); press("运行记录");
    CHECK(!strcmp(ink_ui_page_name(), "RuntimeRecords"), "runtime row opens its unavailable page"); capture("RuntimeRecords");
    CHECK(strstr(snapshot, "不含内容") && strstr(snapshot, "暂时读不到"), "runtime absence disclosed");
    CHECK(AI("shell.back", NULL), "return from runtime"); quiet(); press("孩子");
    CHECK(WAIT_FOR(summary_count("3 件"), 5000), "child segment uses child's backend count"); capture("RecordsChild");
    CHECK(strstr(snapshot, "\"value\":\"kid\""), "snapshot identifies child target");
    press("AI 替你做过的事"); quiet(); on_ui(read_snapshot);
    CHECK(!strcmp(ink_ui_page_name(), "AiLog") && strstr(snapshot, "synthetic.child") && !strstr(snapshot, "synthetic.parent") && strstr(snapshot, "\"total\":3"), "adult AiLog shows only child's unexpired records");
    CHECK(ink_member_switch("kid") == 0, "switch to child"); quiet();
    CHECK(AI("activity.list", NULL) && g_rc.status == INK_RC_DONE && strstr(g_result, "\"total\":3"), "child reads own records");
    CHECK(AI("activity.list", "{\"member\":\"owner\"}") && g_rc.status == INK_RC_REJECTED, "child cannot request adult records");
    CHECK(AI("shell.ai_log", "{\"member\":\"owner\"}") && g_rc.status == INK_RC_REJECTED, "child cannot navigate to adult log");
    CHECK(AI("shell.ai_log", NULL) && g_rc.status == INK_RC_DONE, "child opens own AiLog"); quiet(); on_ui(read_snapshot);
    CHECK(strstr(snapshot, "synthetic.child") && !strstr(snapshot, "synthetic.parent"), "child UI and AI snapshot exclude adult data");
    CHECK(AI("activity.clear", "{\"member\":\"owner\"}") && g_rc.status == INK_RC_REJECTED, "child cannot delete adult records");
    CHECK(ink_member_switch("owner") == 0, "return to adult"); quiet();
    CHECK(AI("shell.privacy", NULL), "open privacy"); quiet(); capture("SetPrivacy");
    bool seen_retention = false;
    for (int i = 0; i < 8; i++) {
        on_ui(read_snapshot);
        if (strstr(snapshot, "留多久")) { seen_retention = strstr(snapshot, "1 年") && strstr(snapshot, "180 天"); break; }
        CHECK(AI("shell.settings_next", NULL), "find retention paragraph"); quiet();
    }
    CHECK(seen_retention, "privacy retention agrees with backend");
    CHECK(AI("shell.privacy", NULL), "reopen child privacy entry"); quiet(); seek("孩子的记录");
    CHECK(!strcmp(ink_ui_page_name(), "Records") && WAIT_FOR(summary_count("3 件"), 5000), "child privacy entry opens child segment");
    CHECK(AI("shell.privacy", NULL), "reopen general privacy entry"); quiet(); seek("查看和删除记录");
    CHECK(!strcmp(ink_ui_page_name(), "Records") && WAIT_FOR(summary_count("2 件"), 5000), "general privacy entry opens self segment");
    press("全部删除");
    ink_confirm_view_t v = {0};
    CHECK(ink_confirm_list(&v, 1) == 1 && raw_count("owner") == 3, "delete waits for human confirmation");
    CHECK(ink_confirm_decide(v.token, true, INK_SRC_AI) < 0, "AI cannot confirm deletion");
    CHECK(ink_confirm_decide(v.token, false, INK_SRC_TOUCH) == 0 && raw_count("owner") == 3, "cancel preserves all records"); quiet();
    press("全部删除"); CHECK(ink_confirm_list(&v, 1) == 1, "delete can be requested again");
    CHECK(ink_confirm_decide(v.token, true, INK_SRC_TOUCH) == 0, "human confirms deletion");
    CHECK(WAIT_FOR(raw_count("owner") == 0, 10000), "real deletion clears retained and expired records"); quiet();
    CHECK(WAIT_FOR(summary_count("0 件"), 5000), "summary returns to zero after confirmation"); capture("RecordsDeleted");
    CHECK(raw_count("kid") == 4, "self deletion preserves child's records");
    press("孩子"); quiet(); press("全部删除"); CHECK(ink_confirm_list(&v, 1) == 1, "child records also require confirmation");
    CHECK(ink_confirm_decide(v.token, true, INK_SRC_TOUCH) == 0, "adult confirms deleting child's records");
    CHECK(WAIT_FOR(raw_count("kid") == 0, 10000), "adult deletion clears child backend"); quiet();
    CHECK(WAIT_FOR(summary_count("0 件"), 5000), "child summary count returns to zero");
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name = "test-privacy-records", .fn = run, .stack_bytes = 16384,
        .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start privacy records test");
}
