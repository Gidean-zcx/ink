/* Synthetic scheduler, release notes and offline first-run content. */
#include <stdlib.h>
#include "cJSON.h"
#include "ink/sys/ai.h"
#include "ink/sys/time.h"
#include "ink/ui_shell.h"
#define ink_main product_main
#include "../../system/boot/main.c"
#undef ink_main
#include "../integration/whole_product.h"

static char mode[24], wanted[128];
static int x, y;
static volatile bool captured;
static int64_t state_int(const char *id) { int64_t n = 0; ink_state_get_int(ink_state_find(id), &n); return n; }
static const char *state_str(const char *id) { static char s[32]; ink_state_get_str(ink_state_find(id), s, sizeof s); return s; }
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static bool at(const char *name) { on_ui(read_screen); return !strcmp(g_top, name); }
static void locate(lv_obj_t *root) {
    if (!lv_obj_is_visible(root)) return;
    if (lv_obj_check_type(root, &lv_label_class) && !strcmp(lv_label_get_text(root), wanted)) {
        lv_area_t a; lv_obj_get_coords(root, &a); x = (a.x1 + a.x2) / 2; y = (a.y1 + a.y2) / 2;
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); i++) locate(lv_obj_get_child(root, (int)i));
}
static void find(void) { x = y = -1; lv_obj_update_layout(lv_screen_active()); locate(lv_screen_active()); locate(lv_layer_sys()); }
static void press(const char *label) {
    CHECK(WAIT_FOR(settled(), 15000), "frame settles before touch");
    snprintf(wanted, sizeof wanted, "%s", label); on_ui(find);
    CHECK(x >= 0 && y >= 0, "visible touch control: %s", label);
    if (x >= 0) tap(x, y);
}
static bool visible(const char *label) { snprintf(wanted, sizeof wanted, "%s", label); on_ui(find); return x >= 0; }
static void frame(void *name) { sim_display_snapshot(name); captured = true; }
static void capture(const char *name) {
    CHECK(WAIT_FOR(settled(), 15000), "screenshot is a settled frame");
    captured = false; vtk_event_at(vtk_now() + SIM_MS(1), frame, (void *)name);
    CHECK(WAIT_FOR(captured, 1000), "screenshot captured");
}
static void link(void *name) { sim_net_set_link(name); }
static void set_link(const char *name) { vtk_event_at(vtk_now() + SIM_MS(1), link, (void *)name); delay_ms(5); }
static bool backup_phase(const char *phase) {
    return AI("backup.status", NULL) && g_rc.status == INK_RC_DONE && strstr(g_result, phase);
}
static void tick(void) {
    double before = sim_metric_value("backup.auto_checks");
    CHECK(WAIT_FOR(sim_metric_value("backup.auto_checks") > before, 65000), "existing backup timer executes its next check");
}
static void no_backup(const char *why) {
    tick();
    CHECK(sim_metric_value("action.user.backup.now") == 0, "no automatic backup: %s", why);
}
static void backup(void) {
    CHECK(ink_setting_get("backup.auto") == 0, "automatic backup defaults off");
    CHECK(AI("backup.open", NULL) && WAIT_FOR(at("Backup") && strstr(g_snap, "自动备份：关"), 5000), "backup page exposes the schedule setting");
    /* the first status reply redraws the page (a tap in progress would land on a deleted button): wait for it */
    CHECK(WAIT_FOR(at("Backup") && strstr(g_snap, "还没有备份"), 15000), "the backup page shows its first status");
    press("自动备份：关");
    CHECK(WAIT_FOR(ink_setting_get("backup.auto") == 1, 5000), "touch selects daily backup through settings.set");
    tick();
    int64_t base = ink_time_now_s();
    ink_time_set(base + 86400 - 300, false);
    no_backup("deadline not reached");
    ink_time_set(base + 86400 + 120, false);
    hw(HW_BATTERY, 80, 0); ink_power_battery_policy_start();
    CHECK(WAIT_FOR(!state_int("battery.charging"), 5000), "charging is absent in the real power state");
    no_backup("not charging");
    hw(HW_BATTERY_CHARGING, 80, 0); ink_power_battery_policy_start();
    CHECK(WAIT_FOR(state_int("battery.charging"), 5000), "charging resumes");
    set_link("none"); set_link("lte");
    CHECK(WAIT_FOR(!strcmp(state_str("net.type"), "lte"), 15000), "synthetic mobile link selected");
    no_backup("mobile network");
    set_link("wifi");
    CHECK(WAIT_FOR(!strcmp(state_str("net.type"), "wifi") && state_int("net.online"), 15000), "Wi-Fi returns; the backup job will open the on-demand peer");
    ink_power_hold(INK_NEED_NET);
    CHECK(WAIT_FOR(ink_power_state() == INK_PWR_ACTIVE, 5000), "device is in active use");
    no_backup("device active");
    ink_power_release(INK_NEED_NET);
    CHECK(WAIT_FOR(backup_phase("\"phase\":\"saved\""), 90000), "due idle charging Wi-Fi backup completes the existing task");
    CHECK(sim_metric_value("action.user.backup.now") == 1, "one due interval starts exactly one backup.now");
    CHECK(WAIT_FOR(at("Backup") && strstr(g_snap, "已备份到家庭服务器"), 5000), "page displays the last successful result");
    capture("automatic-backup");
    press("自动备份：每天");
    CHECK(WAIT_FOR(ink_setting_get("backup.auto") == 2, 5000), "weekly setting reachable");
    tick(); base = ink_time_now_s();
    ink_time_set(base + 86400, false); tick();
    CHECK(sim_metric_value("action.user.backup.now") == 1, "weekly does not run after a day");
    ink_time_set(base + 7 * 86400 + 120, false);
    CHECK(WAIT_FOR(sim_metric_value("action.user.backup.now") == 2 && backup_phase("\"phase\":\"saved\""), 90000), "weekly deadline starts the same backup job");
    press("自动备份：每周");
    CHECK(WAIT_FOR(ink_setting_get("backup.auto") == 0, 5000), "touch turns automatic backup off");
}
static char note_piece[768];
static void parse_note(void *arg) {
    (void)arg;
    note_piece[0] = 0;
    cJSON *j = cJSON_Parse(g_snap);
    const cJSON *field;
    cJSON_ArrayForEach(field, cJSON_GetObjectItemCaseSensitive(j, "fields")) {
        const cJSON *id = cJSON_GetObjectItemCaseSensitive(field, "id");
        const cJSON *value = cJSON_GetObjectItemCaseSensitive(field, "value");
        if (cJSON_IsString(id) && !strcmp(id->valuestring, "notes") && cJSON_IsString(value))
            snprintf(note_piece, sizeof note_piece, "%s", value->valuestring);
    }
    CHECK(j, "notes snapshot is valid JSON");
    cJSON_Delete(j);
}
static void notes(void) {
    CHECK(AI("shell.update", NULL) && WAIT_FOR(at("Update"), 5000), "open update page");
    CHECK(AI("update.check", NULL) && g_rc.status == INK_RC_DONE, "real update check receives a synthetic offer");
    CHECK(WAIT_FOR(at("Update") && strstr(g_snap, "发现新版本"), 5000), "offer is presented");
    bool empty = !strcmp(mode, "empty-notes");
    if (empty) {
        CHECK(!strstr(g_snap, "更新说明") && !strstr(g_snap, "notes_next"), "absent notes have no block or pager");
        CHECK(!visible("更新说明") && !visible("更新说明暂不能查看"), "absent notes render neither a heading nor the old placeholder");
        capture("no-notes"); return;
    }
    CHECK(strstr(g_snap, "更新说明") && strstr(g_snap, "合成开头") && strstr(g_snap, "notes_next"), "long notes show the beginning and pagination");
    capture("notes-first");
    static char all[16000]; all[0] = 0;
    for (int page = 0; page < 50; page++) {
        on_ui(read_screen);
        vtk_host_call(parse_note, NULL);
        const char *text = note_piece;
        size_t at_byte = strlen(all), n = strlen(text);
        CHECK(n && at_byte + n < sizeof all, "every notes page contributes complete bounded text");
        if (!n || at_byte + n >= sizeof all) break;
        memcpy(all + at_byte, text, n + 1);
        if (strstr(text, "合成结尾")) break;
        CHECK(AI("shell.update_notes_page", "{\"delta\":1}") && g_rc.status == INK_RC_DONE, "AI flips the same notes page capability");
    }
    const char *expected = getenv("INK_SIM_UPDATE_NOTES");
    CHECK(expected && !strcmp(all, expected), "all pages reconstruct the entire server notes exactly, including UTF-8 and newlines");
    capture("notes-last");
    CHECK(AI("shell.update_notes_page", "{\"delta\":-1}") && g_rc.status == INK_RC_DONE, "notes can turn backwards");
}
static void sample(void) {
    set_link("none");
    CHECK(WAIT_FOR(!ink_hub_online(), 10000), "peer observes the offline link before tutorial opens");
    CHECK(BY_HAND("shell.launch", "{\"id\":\"boot\"}") && g_rc.status == INK_RC_DONE &&
        BY_HAND("shell.launch", "{\"id\":\"boot.hello\"}") && g_rc.status == INK_RC_DONE, "reach first-run hello offline");
    CHECK(WAIT_FOR(at("BootHello") && strstr(g_snap, "看看示例") && strstr(g_snap, "AI 还要等"), 5000), "offline tutorial offers sample and honest AI availability");
    capture("boot-hello"); press("跳过");
    CHECK(WAIT_FOR(at("home"), 5000), "sample tutorial can be skipped");
    CHECK(BY_HAND("shell.launch", "{\"id\":\"boot\"}") && BY_HAND("shell.launch", "{\"id\":\"boot.hello\"}"), "reopen tutorial");
    double before = sim_metric_value("action.user.reader.open_sample");
    press("看看示例");
    CHECK(WAIT_FOR(at("book") && strstr(g_snap, "小河边") && strstr(g_snap, "CC0 1.0"), 8000), "offline original sample is open in the real reader");
    CHECK(sim_metric_value("action.user.reader.open_sample") == before + 1, "sample touch uses the atomic reader capability");
    capture("sample-first");
    set_link("wifi"); ink_hub_demand();
    CHECK(WAIT_FOR(ink_hub_online(), 15000), "synthetic AI peer online without leaving the sample");
    hw(HW_KEY_DOWN, HAL_KEY_AI, 0); delay_ms(1200); hw(HW_KEY_UP, HAL_KEY_AI, 0);
    CHECK(WAIT_FOR(sim_hub_last_ask(), 5000), "AI receives a question from this reading page");
    const char *ask = sim_hub_last_ask();
    CHECK(ask && strstr(ask, "小河边") && strstr(ask, "夜邮") && strstr(ask, "CC0 1.0"), "actual AI request carries the visible original sample text");
}
static void run(void *arg) {
    (void)arg; test_start_home(); ink_power_hold(INK_NEED_NO_LOCK); ink_power_hold(INK_NEED_AWAKE);
    ink_hub_demand(); CHECK(WAIT_FOR(ink_hub_online(), 15000), "synthetic hub connected");
    if (!strcmp(mode, "backup")) backup();
    else if (!strcmp(mode, "sample")) sample();
    else notes();
    ink_probe_count("test.finished");
}
void ink_main(void) {
    sim_copy_name(mode, sizeof mode, getenv("INK_GAPS_CASE")); product_main();
    osal_task_cfg_t cfg = {.name = "small-gaps-test", .fn = run, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL,
        .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start bounded feature driver");
}
