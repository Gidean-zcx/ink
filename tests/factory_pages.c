/* Real reset, backup and boot flow on synthetic flash, card and peer. */
#include <stdlib.h>
#include "ink/ui_shell.h"
#define ink_main product_main
#include "../system/boot/main.c"
#undef ink_main
#include "integration/whole_product.h"

static const char *label, *picture;
static int x, y;
static bool captured;
static char context[6144];
static volatile int pin_done, pin_result;
static void pin_reply(void *ctx, int result, uint32_t wait) { (void)ctx; (void)wait; pin_result = result; pin_done++; }
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void read_page(void) { read_screen(); snprintf(g_top, sizeof g_top, "%s", ink_ui_page_name()); }
static bool at(const char *name) { on_ui(read_page); return !strcmp(g_top, name); }
static bool ready(const char *name) {
    bool ok = WAIT_FOR(at(name), 12000) && WAIT_FOR(settled(), 10000);
    CHECK(ok, "page %s is presented (actual %s)", name, g_top); return ok;
}
static void locate(lv_obj_t *o) {
    if (!lv_obj_is_visible(o)) return;
    if (lv_obj_check_type(o, &lv_label_class) && !strcmp(lv_label_get_text(o), label)) {
        lv_area_t a; lv_obj_get_coords(label[0] ? o : lv_obj_get_parent(o), &a);
        x = (a.x1 + a.x2) / 2; y = (a.y1 + a.y2) / 2;
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) locate(lv_obj_get_child(o, (int32_t)i));
}
static void find(void) { x = y = -1; lv_obj_update_layout(lv_screen_active()); locate(lv_screen_active()); }
static void press(const char *text) {
    CHECK(WAIT_FOR(settled(), 10000), "touch starts on a settled frame");
    label = text; on_ui(find); CHECK(x >= 0 && y >= 0, "real touch control: %s", text);
    if (x >= 0 && y >= 0) tap(x, y);
}
static void save(void *arg) {
    char path[600]; int n = snprintf(path, sizeof path, "%s/%s.json", g_sim.out_dir, (const char *)arg);
    if (n < 0 || (size_t)n >= sizeof path) abort();
    FILE *f = fopen(path, "wb");
    if (!f || fputs(context, f) == EOF || fclose(f)) abort();
}
static void inspect(void) {
    const char *name = NULL;
    CHECK(ink_ui_snapshot(context, sizeof context, &name) > 0 && name && !strcmp(name, picture), "production AI snapshot: %s", picture);
    CHECK(ink_ui_context_snapshot(g_snap, sizeof g_snap, NULL) > 0, "snapshot fits real AI context");
    CHECK(!strstr(context, "1234") && !strstr(context, "test-only-photo-content"), "AI never receives password or file contents");
    const char *caps[] = {"factory.media", "backup.now", "system.factory_reset", "factory.finish"};
    for (unsigned i = 0; i < sizeof caps / sizeof *caps; i++)
        if (strstr(context, caps[i])) CHECK(ink_cap_find(caps[i]) && ink_cap_ai_callable(ink_cap_find(caps[i])), "AI and touch share capability %s", caps[i]);
}
static void frame(void *arg) { CHECK(settled(), "frame is fully presented"); sim_display_snapshot(arg); captured = true; }
static void capture(const char *name) {
    if (!ready(name)) return;
    picture = name; on_ui(inspect); vtk_host_call(save, (void *)name);
    captured = false; vtk_event_at(vtk_now() + SIM_MS(1), frame, (void *)name);
    CHECK(WAIT_FOR(captured, 2000), "capture %s", name);
    char metric[96]; snprintf(metric, sizeof metric, "test.snapshot.%s", name); ink_probe_count(metric);
    ink_probe_count("test.snapshot.settled");
}
static void file(const char *path, const char *value) { CHECK(!ink_fs_write_atomic(path, value, strlen(value)), "synthetic file %s", path); }
static void exists(const char *path, bool expected) {
    ink_fs_stat_t st; int r = ink_fs_stat(path, &st);
    CHECK(expected ? r == INK_FS_OK : r == INK_FS_ERR_NOT_FOUND, "file %s present=%d actual=%d", path, expected, r);
}
static void memo(const char *body) {
    ink_db_t *db = ink_db_open(ink_db_find_def("memo.notes"), NULL);
    ink_db_write_t w = {.id = 1, .data = body, .len = strlen(body), .src = INK_SRC_TOUCH};
    CHECK(db && ink_db_put(db, &w, NULL) == INK_DB_OK, "real memo collection changes"); ink_db_close(db);
}
static int64_t saved_time(void) {
    if (!AI("backup.status", "{\"changes\":true}") || g_rc.status != INK_RC_DONE) return 0;
    ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL); int64_t saved = 0;
    if (j && !ink_json_parse(j, g_result, strlen(g_result))) ink_json_int(j, ink_json_find(j, 0, "saved_s"), &saved);
    osal_free(j); return saved;
}
static bool time_shown(int64_t saved) {
    ink_tm_t tm; ink_time_to_local(saved, &tm); char text[80];
    snprintf(text, sizeof text, "上次备份：%d月%d日 %02d:%02d", tm.month, tm.day, tm.hour, tm.min);
    return at("FactoryReset") && strstr(g_snap, text);
}
static uint32_t confirmation(void) {
    ink_confirm_view_t v;
    CHECK(WAIT_FOR(ink_confirm_list(&v, 1) == 1, 3000) && !strcmp(v.cap, "system.factory_reset") && v.parent,
        "reset uses existing parent password confirmation"); return v.token;
}
static void prepare(bool clear) {
    test_start_home();
    CHECK(ink_member_set_pin("owner", NULL, "1234", pin_reply, NULL) == 0 && WAIT_FOR(pin_done, 5000) && pin_result == INK_PIN_OK, "set synthetic parent password");
    CHECK(AI("member.add", "{\"id\":\"testguest\",\"name\":\"测试家人\",\"role\":\"adult\"}") && g_rc.status == INK_RC_PENDING_CONFIRMATION, "add a persisted synthetic family member");
    ink_confirm_view_t add; CHECK(ink_confirm_list(&add, 1) == 1 && !strcmp(add.cap, "member.add"), "family confirmation");
    CHECK(!ink_confirm_accept_with_pin(add.token, "1234", NULL, NULL) && WAIT_FOR(ink_member_exists("testguest"), 5000), "parent adds the synthetic family member");
    CHECK(AI("settings.set", "{\"id\":\"audio.volume\",\"value\":30}") && g_rc.status == INK_RC_DONE, "nondefault real setting");
    file("/me/photos/test-only.png", "test-only-photo-content"); file("/me/recordings/test-only.opus", "test-only-recording");
    file("/family/books/test-only.txt", "test-only-book"); file("/family/books/nested/another.txt", "test-only-nested-book");
    file("/family/audio/test-only.opus", "test-only-audio"); file("/family/other/test-only.txt", "test-only-other");
    memo("{\"body\":\"测试备忘\"}");
    CHECK(AI("shell.about", NULL) && ready("SetAbout"), "about page opens");
    for (int i = 0; i < 5 && !strstr(g_snap, "factory.open"); i++) {
        CHECK(AI("shell.settings_next", NULL) && g_rc.status == INK_RC_DONE && ready("SetAbout"), "find real about footer by paging");
    }
    CHECK(strstr(g_snap, "factory.open") && !strstr(g_snap, "system.factory_reset"), "about page opens review instead of destructive backend");
    press("清空这台设备");
    CHECK(ready("FactoryReset"), "review page opens");
    CHECK(WAIT_FOR(at("FactoryReset") && strstr(g_snap, "还没有备份"), 6000), "empty backup state is honest");
    press("先备份");
    CHECK(WAIT_FOR(saved_time() > 0, 120000), "backup button really commits to the synthetic peer");
    int64_t first = saved_time();
    CHECK(strstr(g_result, "\"unbacked\":false"), "unchanged collections and files are reported as backed up (%.24s)", strstr(g_result, "\"unbacked\"") ? strstr(g_result, "\"unbacked\"") : g_result);
    CHECK(WAIT_FOR(time_shown(first), 6000), "page follows real backup timestamp");
    delay_ms(61000); /* Advance a displayed minute before the second backup. */
    memo("{\"body\":\"备份后修改的测试备忘\"}");
    CHECK(WAIT_FOR(at("FactoryReset") && strstr(g_snap, "之后有东西还没备份"), 6000), "page reports actual unbacked changes");
    press("先备份");
    CHECK(WAIT_FOR(saved_time() > first, 120000), "second real backup advances saved time");
    int64_t second = saved_time();
    CHECK(strstr(g_result, "\"unbacked\":false"), "second receipt clears the actual unbacked changes (%.24s)", strstr(g_result, "\"unbacked\"") ? strstr(g_result, "\"unbacked\"") : g_result);
    CHECK(WAIT_FOR(time_shown(second), 6000) && second - first >= 60, "visible backup minute follows the second actual receipt");
    file("/me/after-backup.txt", "test-only-unbacked-file");
    CHECK(WAIT_FOR(at("FactoryReset") && strstr(g_snap, "之后有东西还没备份"), 6000), "new file after backup is unbacked");
    capture("FactoryReset");
    press("继续"); uint32_t token = confirmation();
    CHECK(ink_confirm_decide(token, true, INK_SRC_AI) != 0, "AI cannot approve reset");
    int before = pin_done;
    CHECK(!ink_confirm_accept_with_pin(token, "0000", pin_reply, NULL) && WAIT_FOR(pin_done > before, 5000) && pin_result == INK_PIN_WRONG, "wrong parent password is rejected");
    CHECK(sim_metric_value("system.factory_resets") == 0 && ink_storage_size("/user/sys/reset.pending") < 0, "wrong password never marks or clears");
    exists("/me/photos/test-only.png", true); exists("/family/books/test-only.txt", true);
    CHECK(!ink_confirm_decide(token, false, INK_SRC_TOUCH), "cancel failed password request");
    CHECK(ink_member_switch("kid") == 0, "enter synthetic child space");
    CHECK(AI("factory.open", NULL) && g_rc.status == INK_RC_REJECTED, "child cannot open review");
    CHECK(AI("factory.media", "{\"clear_media\":true}") && g_rc.status == INK_RC_REJECTED, "child cannot choose card wipe");
    CHECK(AI("system.factory_reset", "{\"clear_media\":true}") && g_rc.status == INK_RC_REJECTED, "child cannot call destructive backend");
    before = pin_done;
    CHECK(!ink_member_unlock("owner", "1234", pin_reply, NULL) && WAIT_FOR(pin_done > before, 5000) && pin_result == INK_PIN_OK, "parent returns through real password gate");
    CHECK(AI("factory.open", NULL) && ready("FactoryReset"), "parent reopens review");
    if (clear) {
        CHECK(AI("factory.media", "{\"clear_media\":true}") && g_rc.status == INK_RC_DONE, "AI selects explicit card clearing");
        press("");
        CHECK(at("FactoryReset") && strstr(g_snap, "\\\"clear_media\\\":false"), "real switch and AI use the same selection path");
        CHECK(AI("factory.media", "{\"clear_media\":true}") && g_rc.status == INK_RC_DONE, "select card wipe for this reset");
    }
    press("继续"); token = confirmation();
    ink_db_flush_all(); CHECK(WAIT_FOR(ink_db_idle() && !ink_storage_pending(), 10000), "fixtures are durable before reset");
    ink_probe_count("test.finished");
    CHECK(!ink_confirm_accept_with_pin(token, "1234", NULL, NULL), "correct parent password authorizes reset");
    delay_ms(30000); CHECK(false, "reset must reboot");
}
static void after(bool clear) {
    CHECK(ready("FactoryDone"), "successful reboot starts on completion page");
    CHECK(!ink_member_exists("testguest") && !ink_members_configured() && !ink_member_has_pin("owner"), "family and password are cleared");
    CHECK(ink_setting_get("audio.volume") == 70, "settings are back to factory values");
    ink_db_t *db = ink_db_open(ink_db_find_def("memo.notes"), NULL);
    CHECK(db && ink_db_count(db) == 0, "memo collection is empty"); ink_db_close(db);
    exists("/me/photos/test-only.png", false); exists("/me/recordings/test-only.opus", false); exists("/me/after-backup.txt", false);
    exists("/family/books/test-only.txt", !clear); exists("/family/books/nested/another.txt", !clear); exists("/family/audio/test-only.opus", !clear);
    exists("/family/other/test-only.txt", true);
    capture("FactoryDone");
    press("知道了"); CHECK(ready("FirstBoot"), "acknowledgement enters existing boot flow");
    ink_probe_count("test.finished");
}
static void run(void *arg) {
    (void)arg;
    const char *phase = getenv("INK_FACTORY_PHASE");
    bool clear = phase && strstr(phase, "clear");
    if (phase && strstr(phase, "after")) after(clear); else prepare(clear);
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name = "factory-pages-test", .fn = run, .stack_bytes = 16384,
        .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start synthetic reset driver");
}
