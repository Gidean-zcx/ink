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
static bool saving(void) {
    int64_t value = 0;
    return ink_state_get_int(ink_state_find("power.saving"), &value) && value;
}
static void toggle_saving(void) {
    quiet();
    tap(INK_CANVAS_W - INK_MARGIN - INK_CONTROL / 2,
        INK_CANVAS_H - INK_MARGIN - INK_PRIMARY - INK_GAP_L - INK_CONTROL / 2);
}
static bool bookmark_heading;
static lv_area_t heading_area, save_area;
static char bookmark_text[96];
static void inspect_heading(lv_obj_t *root) {
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); i++) {
        lv_obj_t *child = lv_obj_get_child(root, (int32_t)i);
        if (lv_obj_check_type(child, &lv_label_class) &&
            lv_obj_get_style_text_font(child, 0) == ink_ui_font_px(INK_FONT_HEADING, INK_WEIGHT_BOLD) &&
            !strcmp(lv_label_get_text(child), !strcmp(scene, "PowerOffNoBookmark") ? "已关机" : bookmark_text)) {
            bookmark_heading = true;
            lv_obj_get_coords(child, &heading_area);
        }
        if (lv_obj_check_type(child, &lv_label_class) && strstr(lv_label_get_text(child), unsaved ? "有些内容没保存" : "东西都存好了")) lv_obj_get_coords(child, &save_area);
        inspect_heading(child);
    }
}
static void check_heading(void) {
    /* The reader's card as it is now (reader-one-1: 《书名》章名 · 读到 N%), the off frame's heading. */
    ink_card_t card = {0};
    for (int i = 0; i < ink_app_count(); i++) if (!strcmp(ink_app_at(i)->id, "reader") && ink_app_at(i)->card) ink_app_at(i)->card(0, &card);
    snprintf(bookmark_text, sizeof bookmark_text, "%s", card.text);
    CHECK(!strcmp(scene, "PowerOffNoBookmark") || strstr(bookmark_text, "《星星邮差》第一章"), "the reader's card is the open book: %s", bookmark_text);
    inspect_heading(lv_layer_sys());
    CHECK(bookmark_heading, "the primary off-screen heading is the actual bookmark, or off when none exists");
    CHECK(bookmark_heading && heading_area.y2 < save_area.y1, "the wrapped bookmark heading stays above the save result");
}
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
    CHECK(!strcmp(g_top, "off") && strstr(g_snap, "PowerOffEmpty"), "AI sees the empty-battery off frame: %.200s", g_snap);
    CHECK(ink_display_busy() == 0 && !ink_ui_busy() && sim_metric_value("ui.off_picture_on_panel") == 1, "shutdown waits for the actual panel commit");
    CHECK(strstr(g_snap, unsaved ? "没保存" : "东西都存好了") != NULL, "the off frame reports the actual save result: %.200s", g_snap);
    if (unsaved) CHECK(!strstr(g_snap, "都存好了"), "a failed save never claims success");
    else if (strcmp(scene, "PowerOffNoBookmark")) CHECK(strstr(g_snap, "星星邮差"), "the retained bookmark comes from the reader");
    on_ui(check_heading);
    if (!unsaved) capture();
    ink_probe_count("test.finished");
}
static void low(void) {
    CHECK(BY_HAND("settings.set", "{\"id\":\"power.lock_after_s\",\"value\":120}") && g_rc.status == INK_RC_DONE, "set a custom two-minute preference");
    battery(15, false);
    CHECK(WAIT_FOR(sim_metric_value("power.battery_low_told") >= 1, 3000), "the 15 percent service threshold is sampled");
    CHECK(!ink_interrupt_active(), "15 percent is a notification, not a full-screen interruption");
    battery(5, false);
    CHECK(WAIT_FOR(page("BatteryLow"), 3000), "5 percent enters the real interruption channel");
    CHECK(strstr(g_snap, "power.set_saving") && strstr(g_snap, "interrupt.ack") && strstr(g_snap, "120"), "AI sees the same real actions and original timeout: %.200s", g_snap);
    CHECK(ink_cap_find("power.set_saving") && ink_cap_find("interrupt.ack"), "both controls are registered atomic capabilities");
    capture();
    toggle_saving();
    CHECK(WAIT_FOR(saving(), 2000), "touch enables the one-minute runtime limit");
    CHECK(ink_setting_get("power.lock_after_s") == 120, "enabling saving preserves the custom preference");
    CHECK(WAIT_FOR(page("BatteryLow") && strstr(g_snap, "锁屏=开"), 2000), "the snapshot reflects the enabled switch");
    toggle_saving();
    CHECK(WAIT_FOR(!saving(), 2000), "touch disables saving");
    CHECK(ink_setting_get("power.lock_after_s") == 120, "touching off preserves the original two-minute preference");
    CHECK(AI("power.set_saving", "{\"enabled\":true}") && g_rc.status == INK_RC_DONE, "AI enables the same saving switch");
    CHECK(AI("settings.set", "{\"id\":\"power.lock_after_s\",\"value\":180}") && g_rc.status == INK_RC_DONE, "the user may change the preference while saving is enabled");
    CHECK(saving() && ink_setting_get("power.lock_after_s") == 180, "the runtime limit does not replace the updated preference");
    CHECK(AI("power.set_saving", "{\"enabled\":false}") && g_rc.status == INK_RC_DONE && ink_setting_get("power.lock_after_s") == 180, "disabling keeps the latest user preference");
    CHECK(AI("settings.set", "{\"id\":\"power.lock_after_s\",\"value\":60}") && g_rc.status == INK_RC_DONE, "a one-minute preference is independent of saving");
    CHECK(WAIT_FOR(page("BatteryLow") && strstr(g_snap, "锁屏=关"), 2000) && !saving(), "a one-minute user setting does not turn saving on");
    CHECK(AI("power.set_saving", "{\"enabled\":true}") && g_rc.status == INK_RC_DONE, "enable with an already-short preference");
    CHECK(AI("power.set_saving", "{\"enabled\":false}") && g_rc.status == INK_RC_DONE && ink_setting_get("power.lock_after_s") == 60, "off never lengthens an already-short preference");
    CHECK(AI("settings.set", "{\"id\":\"power.lock_after_s\",\"value\":120}") && g_rc.status == INK_RC_DONE, "restore the custom two-minute preference");
    CHECK(AI("power.set_saving", "{\"enabled\":true}") && g_rc.status == INK_RC_DONE, "AI enables saving for the timer check");
    CHECK(AI("interrupt.ack", NULL) && g_rc.status == INK_RC_DONE, "AI acknowledges the same interruption");
    CHECK(WAIT_FOR(!ink_interrupt_active(), 1000), "acknowledge clears the interruption");
    ink_power_activity(osal_now_us());
    delay_ms(59000);
    CHECK(strcmp(power_state(), "locked"), "saving does not lock before one minute");
    CHECK(WAIT_FOR(!strcmp(power_state(), "locked"), 3000), "saving actually locks after one minute");
    ink_power_unlock(osal_now_us());
    CHECK(WAIT_FOR(strcmp(power_state(), "locked"), 2000), "wake returns to the underlying page");
    CHECK(AI("power.set_saving", "{\"enabled\":false}") && g_rc.status == INK_RC_DONE, "turn saving off");
    CHECK(ink_setting_get("power.lock_after_s") == 120, "the two-minute preference survives the whole toggle cycle");
    ink_power_activity(osal_now_us());
    delay_ms(61000);
    CHECK(strcmp(power_state(), "locked"), "disabled saving leaves the device awake beyond one minute");
    delay_ms(58000);
    CHECK(strcmp(power_state(), "locked"), "the restored preference does not lock before two minutes");
    CHECK(WAIT_FOR(!strcmp(power_state(), "locked"), 3000), "saving off actually restores the custom two-minute deadline");
    ink_power_unlock(osal_now_us());
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
    CHECK(strstr(g_snap, "正在充电") && strstr(g_snap, "62%") && strstr(g_snap, "AI 键"), "AI has charge and wake context: %.200s", g_snap);
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
    if (strcmp(scene, "PowerOffNoBookmark")) CHECK(BY_HAND("reader.open_book", "{\"title\":\"星星邮差\"}") && g_rc.status == INK_RC_DONE, "open a synthetic book to retain its real reading position");
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
