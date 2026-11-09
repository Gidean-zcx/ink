/* Whole product battery transitions, retained frames and AI context; synthetic inputs only. */
#include <stdlib.h>
#include "ink/sys/interrupt.h"
#include "ink/ui_shell.h"
#define ink_main product_main
#include "configured_boot.c"
#undef ink_main
#include "integration/whole_product.h"

static char scene[32];
static bool unsaved;
static const ink_db_def_t DATA = {.name = "test.power-pages", .scope = INK_DB_DEVICE, .schema = 1};
extern void (*sim_power_before_shutdown)(void);
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void quiet(void) { CHECK(WAIT_FOR(settled(), 10000), "the battery frame settles"); }
static bool page(const char *name) { on_ui(read_screen); return !strcmp(g_top, name); }
static void screenshot(void *arg) {
    (void)arg;
    CHECK(settled(), "only compare a settled physical frame");
    ink_probe_count("test.snapshot.settled");
    char metric[80]; snprintf(metric, sizeof metric, "test.snapshot.%s", scene); ink_probe_count(metric);
    sim_display_snapshot("scene");
}
static void capture(void) {
    quiet(); vtk_event_at(vtk_now() + SIM_MS(1), screenshot, NULL); delay_ms(5);
}
static void battery(int percent, bool charging) {
    hw(charging ? HW_BATTERY_CHARGING : HW_BATTERY, percent, 0);
    ink_power_battery_policy_start(); /* Ask the normal sampler to run now, without a minute's wait. */
}
static void shutdown_check(void) {
    on_ui(read_screen);
    CHECK(!strcmp(g_top, "off") && strstr(g_snap, "PowerOffEmpty"), "AI sees the empty-battery off frame: %s", g_snap);
    CHECK(ink_display_busy() == 0 && !ink_ui_busy() && sim_metric_value("ui.off_picture_on_panel") == 1, "shutdown waits for the actual panel commit");
    CHECK(strstr(g_snap, unsaved ? "没保存" : "东西都存好了") != NULL, "the off frame reports the actual save result: %s", g_snap);
    if (unsaved) CHECK(!strstr(g_snap, "都存好了"), "a failed save never claims success");
    else CHECK(strstr(g_snap, "星星邮差"), "the retained bookmark comes from the reader");
    if (!strcmp(scene, "PowerOffEmpty")) capture();
    ink_probe_count("test.finished");
}
static void low(void) {
    battery(15, false);
    CHECK(WAIT_FOR(sim_metric_value("power.battery_low_told") >= 1, 3000), "the 15 percent service threshold is sampled");
    CHECK(!ink_interrupt_active(), "15 percent is a notification, not a full-screen interruption");
    battery(5, false);
    CHECK(WAIT_FOR(page("BatteryLow"), 3000), "5 percent enters the real interruption channel");
    CHECK(strstr(g_snap, "settings.set") && strstr(g_snap, "interrupt.ack"), "AI sees the same real actions: %s", g_snap);
    CHECK(ink_cap_find("settings.set") && ink_cap_find("interrupt.ack"), "both controls are registered atomic capabilities");
    capture();
    tap(INK_CANVAS_W - INK_MARGIN - INK_CONTROL / 2,
        INK_CANVAS_H - INK_MARGIN - INK_PRIMARY - INK_GAP_L - INK_CONTROL / 2);
    CHECK(WAIT_FOR(ink_setting_get("power.lock_after_s") == 60, 2000), "touch enables the real one-minute auto-lock policy");
    CHECK(AI("settings.set", "{\"id\":\"power.lock_after_s\",\"value\":600}") && g_rc.status == INK_RC_DONE, "AI disables the same setting");
    CHECK(ink_setting_get("power.lock_after_s") == 600, "off returns to the ten-minute policy");
    CHECK(AI("settings.set", "{\"id\":\"power.lock_after_s\",\"value\":60}") && g_rc.status == INK_RC_DONE, "AI enables saving");
    CHECK(AI("interrupt.ack", NULL) && g_rc.status == INK_RC_DONE, "AI acknowledges the same interruption");
    CHECK(WAIT_FOR(!ink_interrupt_active(), 1000), "acknowledge clears the interruption");
    ink_power_activity(osal_now_us());
    delay_ms(59000);
    CHECK(strcmp(power_state(), "locked"), "saving does not lock before one minute");
    CHECK(WAIT_FOR(!strcmp(power_state(), "locked"), 3000), "saving actually locks after one minute");
    ink_power_unlock(osal_now_us());
    CHECK(WAIT_FOR(strcmp(power_state(), "locked"), 2000), "wake returns to the underlying page");
    CHECK(AI("settings.set", "{\"id\":\"power.lock_after_s\",\"value\":600}") && g_rc.status == INK_RC_DONE, "turn saving off");
    delay_ms(61000);
    CHECK(strcmp(power_state(), "locked"), "disabled saving leaves the device awake beyond one minute");
    battery(80, true); delay_ms(200);
    battery(5, false);
    CHECK(WAIT_FOR(page("BatteryLow"), 2000), "charging resets the one-shot low-battery reminder");
    quiet(); on_ui(read_screen);
    tap(INK_CANVAS_W / 2, INK_CANVAS_H - INK_MARGIN - INK_PRIMARY / 2);
    CHECK(WAIT_FOR(!ink_interrupt_active(), 1000), "the visible acknowledge button clears the service");
    ink_probe_count("test.finished");
}
static void charging(void) {
    ink_power_lock(); CHECK(WAIT_FOR(!strcmp(power_state(), "locked"), 2000), "lock before showing the charging surface");
    battery(62, true);
    CHECK(WAIT_FOR(page("Charging"), 3000), "plugging in shows the charging surface on the lock screen");
    CHECK(strstr(g_snap, "正在充电") && strstr(g_snap, "62%") && strstr(g_snap, "AI 键"), "AI has charge and wake context: %s", g_snap);
    capture();
    battery(72, true);
    CHECK(WAIT_FOR(page("Charging") && strstr(g_snap, "72%"), 2000), "a ten-percent increase updates the retained percentage");
    quiet();
    key(HAL_KEY_POWER);
    CHECK(WAIT_FOR(strcmp(power_state(), "locked"), 2000), "the physical key wakes the device");
    CHECK(WAIT_FOR(!page("Charging"), 2000), "waking removes the charging surface");
    ink_power_lock(); delay_ms(200);
    CHECK(WAIT_FOR(page("Charging"), 2000), "relocking restores the charging surface");
    battery(72, false);
    CHECK(WAIT_FOR(!page("Charging"), 2000), "unplugging restores the ordinary lock screen");
    ink_probe_count("test.finished");
}
static void empty(void) {
    CHECK(BY_HAND("reader.open_book", "{\"title\":\"星星邮差\"}") && g_rc.status == INK_RC_DONE, "open a synthetic book to retain its real reading position");
    quiet();
    CHECK(ink_db_register(&DATA) == 0, "register the synthetic pending record");
    ink_db_t *db = ink_db_open(&DATA, NULL); CHECK(db != NULL, "open the pending collection");
    ink_storage_hold_writes(unsaved);
    ink_db_write_t w = {.data = "SYNTHETIC", .len = 9, .flags = INK_DB_LAZY};
    CHECK(ink_db_put(db, &w, NULL) == INK_DB_OK, "accept a real lazy write before battery exhaustion");
    sim_power_before_shutdown = shutdown_check;
    battery(2, false);
    if (unsaved) {
        CHECK(WAIT_FOR(page("BatterySaving"), 2000), "2 percent shows the saving surface while data waits");
        CHECK(strstr(g_snap, "正在存东西") && !strstr(g_snap, "settings.set"), "saving snapshot reports progress without invented actions");
        capture();
    }
    delay_ms(20000);
    CHECK(false, "exhaustion must end with real simulated shutdown");
}
static void run(void *arg) {
    (void)arg; delay_ms(2000); test_start_home(); ink_power_hold(INK_NEED_AWAKE);
    if (!strcmp(scene, "BatteryLow")) low();
    else if (!strcmp(scene, "Charging")) charging();
    else empty();
}
void ink_main(void) {
    sim_copy_name(scene, sizeof scene, getenv("INK_POWER_SCENE"));
    unsaved = !strcmp(scene, "BatterySaving");
    product_main();
    osal_task_cfg_t cfg = {.name = "test-power-pages", .fn = run, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL,
                          .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start the test driver");
}
