/* Synthetic inputs exercise the real input, UI, settings and motor services. */
#include <stdlib.h>
#include "ink/ui_shell.h"
#include "ink/sys/i18n.h"
#define ink_main product_main
#include "../system/boot/main.c"
#undef ink_main
#include "integration/whole_product.h"

extern int ink_test_pulse_ms;
extern unsigned ink_test_pulses;
static int clicks;
static lv_point_t target;
static const char *wanted;
static bool found, captured;
static bool restarting;
static char page[3072];
static void read_phase(void *arg) {
    (void)arg;
    const char *phase = getenv("INK_VENDOR_HAPTIC_RESTART");
    restarting = phase && !strcmp(phase, "1");
}
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void quiet(void) { CHECK(WAIT_FOR(settled(), 12000), "frame settles"); }
static void call_ok(const char *id, const char *args) {
    CHECK(AI(id, args) && g_rc.status == INK_RC_DONE, "%s succeeds (%s)", id, ink_rc_name(g_rc.status));
    quiet();
}
static void set(const char *id, int value) {
    char args[100]; snprintf(args, sizeof args, "{\"id\":\"%s\",\"value\":%d}", id, value);
    call_ok("settings.set", args);
    CHECK(ink_setting_get(id) == value, "setting applied: %s", id);
}
static void clicked(lv_event_t *e) { (void)e; clicks++; }
static void fixture_create(lv_obj_t *s, void *arg) {
    (void)arg;
    lv_obj_t *b = ink_ui_pill(s, "合成按钮", 80, 160, 220, 64, false, clicked, NULL);
    lv_area_t a; lv_obj_get_coords(b, &a);
    lv_obj_update_layout(s); lv_obj_get_coords(b, &a);
    target = (lv_point_t){(a.x1 + a.x2) / 2, (a.y1 + a.y2) / 2};
    b = ink_ui_pill(s, "合成禁用", 80, 300, 220, 64, false, clicked, NULL);
    lv_obj_add_state(b, LV_STATE_DISABLED);
}
static bool fixture_key(ink_key_t k, bool held) { (void)k; (void)held; return true; }
static const ink_activity_t FIXTURE = {.name = "SyntheticHaptic", .create = fixture_create, .on_key = fixture_key};
static void open_fixture(void) { ink_ui_push(&FIXTURE, NULL); }
static void find_label(lv_obj_t *o) {
    if (!lv_obj_is_visible(o)) return;
    if (lv_obj_check_type(o, &lv_label_class) && !strcmp(lv_label_get_text(o), wanted)) {
        lv_area_t a; lv_obj_get_coords(o, &a);
        target = (lv_point_t){(a.x1 + a.x2) / 2, (a.y1 + a.y2) / 2}; found = true;
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) find_label(lv_obj_get_child(o, (int32_t)i));
}
static void locate(void) { found = false; lv_obj_update_layout(lv_screen_active()); find_label(lv_screen_active()); }
static void press(const char *label) {
    wanted = label;
    for (int i = 0; i < 4; i++) {
        on_ui(locate);
        if (found) break;
        call_ok("shell.page_next", NULL);
    }
    CHECK(found, "visible control: %s", label);
    if (found) tap(target.x, target.y);
    quiet();
}
static void inspect(void) {
    CHECK(ink_ui_snapshot(page, sizeof page, NULL) > 0, "page has a snapshot");
    CHECK(ink_ui_context_snapshot(g_snap, sizeof g_snap, NULL) > 0 && !strcmp(page, g_snap), "AI gets the same snapshot");
}
static void snapshot(void *arg) { sim_display_snapshot(arg); captured = true; }
static void frame(const char *name) {
    quiet(); captured = false; vtk_event_at(vtk_now() + SIM_MS(1), snapshot, (void *)name);
    CHECK(WAIT_FOR(captured, 1000), "capture settled frame");
}
static void pulse_touch(int expected_ms) {
    unsigned before = ink_test_pulses;
    int clicked_before = clicks;
    hw(HW_PRESS, target.x, target.y);
    CHECK(WAIT_FOR(ink_test_pulses > before, 2000), "finger down pulses before release");
    CHECK(ink_test_pulses == before + 1 && ink_test_pulse_ms == expected_ms,
          "one pulse of %d ms (actual %d)", expected_ms, ink_test_pulse_ms);
    CHECK(clicks == clicked_before, "haptic does not wait for a click");
    CHECK(WAIT_FOR(sim_board_motor_on(), 1000), "real simulated P03 output turns on");
    hw(HW_RELEASE, target.x, target.y); quiet();
    CHECK(ink_test_pulses == before + 1, "release adds no pulse");
    CHECK(WAIT_FOR(!sim_board_motor_on(), 2000), "real simulated P03 output turns off");
}
static void input_checks(void) {
    CHECK(ink_setting_get("audio.vibration") == 1 && ink_setting_get("audio.vibration_level") == 0,
          "vendor defaults: enabled and weak");
    set("voice.handsfree", 2); set("power.lock_after_s", 3600);
    on_ui(open_fixture); quiet();
    pulse_touch(35);
    unsigned before = ink_test_pulses;
    tap(10, 100); quiet();
    CHECK(ink_test_pulses == before, "blank space does not pulse");
    wanted = "合成禁用"; on_ui(locate); CHECK(found, "disabled control located");
    tap(target.x, target.y); quiet();
    CHECK(ink_test_pulses == before, "disabled control does not pulse");
    wanted = "合成按钮"; on_ui(locate);
    hw(HW_PRESS, target.x, target.y);
    CHECK(WAIT_FOR(ink_test_pulses == before + 1, 2000), "held touch is delivered");
    delay_ms(1100);
    CHECK(ink_test_pulses == before + 1, "long press does not repeat");
    hw(HW_RELEASE, target.x, target.y); quiet();
    before = ink_test_pulses;
    hw(HW_PRESS, target.x, target.y);
    CHECK(WAIT_FOR(ink_test_pulses == before + 1, 2000), "drag start is delivered");
    hw(HW_PRESS, target.x + 150, target.y + 20);
    hw(HW_RELEASE, target.x + 150, target.y + 20); quiet();
    CHECK(ink_test_pulses == before + 1, "drag and release do not repeat");
    for (int level = 0; level < 3; level++) {
        set("audio.vibration_level", level);
        pulse_touch(level == 0 ? 35 : level == 1 ? 50 : 70);
    }
    set("audio.vibration", 0); before = ink_test_pulses;
    tap(target.x, target.y); key(HAL_KEY_VOL_UP); quiet();
    CHECK(ink_test_pulses == before, "switch off suppresses touch and key output");
    set("audio.vibration", 1); set("audio.volume", 0); set("notify.dnd", 1);
    pulse_touch(70);
    const hal_key_t keys[] = {HAL_KEY_VOL_UP, HAL_KEY_VOL_DOWN, HAL_KEY_AI, HAL_KEY_HOME, HAL_KEY_PREV, HAL_KEY_NEXT};
    for (unsigned i = 0; i < sizeof keys / sizeof *keys; i++) {
        before = ink_test_pulses; key(keys[i]); quiet();
        /* the AI key starts push-to-talk: no buzz into the recording (input_service.c) */
        unsigned want = keys[i] == HAL_KEY_AI ? 0 : 1;
        CHECK(ink_test_pulses == before + want, "physical key %d pulses %u times under DND", keys[i], want);
    }
    before = ink_test_pulses; hw(HW_KEY_DOWN, HAL_KEY_NEXT, 0);
    CHECK(WAIT_FOR(ink_test_pulses == before + 1, 2000), "held cover key reaches the driver");
    delay_ms(1100); hw(HW_KEY_UP, HAL_KEY_NEXT, 0); quiet();
    CHECK(ink_test_pulses == before + 1, "held physical key does not repeat");
    before = ink_test_pulses; hw(HW_KEY_DOWN, HAL_KEY_POWER, 0);
    CHECK(WAIT_FOR(ink_test_pulses == before + 1, 2000), "power down pulses before power policy");
    hw(HW_KEY_UP, HAL_KEY_POWER, 0);
    CHECK(WAIT_FOR(ink_power_state() == INK_PWR_LOCKED, 5000), "power release locks"); quiet();
    before = ink_test_pulses; tap(10, 100); quiet();
    CHECK(ink_test_pulses == before, "locked blank ignores underlying controls");
    key(HAL_KEY_VOL_DOWN); quiet();
    CHECK(ink_test_pulses == before + 1 && ink_power_state() == INK_PWR_LOCKED, "locked volume key still pulses");
    key(HAL_KEY_POWER);
    CHECK(WAIT_FOR(ink_power_state() != INK_PWR_LOCKED, 5000), "wake key unlocks"); quiet();
    CHECK(ink_member_switch("kid") == 0, "switch to synthetic child"); quiet();
    set("audio.vibration_level", 1); on_ui(open_fixture); quiet(); pulse_touch(50);
    before = ink_test_pulses; key(HAL_KEY_VOL_UP); quiet();
    CHECK(ink_test_pulses == before + 1, "child key pulses under DND");
    CHECK(ink_member_switch("owner") == 0, "return to synthetic adult"); quiet();
}
static void ui_checks(void) {
    set("notify.dnd", 0); set("audio.vibration_level", 0);
    call_ok("shell.sound", NULL); press("振动强度");
    CHECK(ink_setting_get("audio.vibration_level") == 1, "touch cycles weak to medium via settings.set");
    press("振动强度"); CHECK(ink_setting_get("audio.vibration_level") == 2, "touch cycles medium to strong");
    press("振动强度"); CHECK(ink_setting_get("audio.vibration_level") == 0, "touch cycles strong to weak");
    call_ok("settings.get", "{\"id\":\"audio.vibration_level\"}");
    CHECK(strstr(g_rc.say, "弱"), "AI reads weak strength");
    call_ok("settings.get", "{\"id\":\"audio.vibration\"}");
    press("振动"); CHECK(ink_setting_get("audio.vibration") == 0, "touch disables existing switch");
    press("振动"); CHECK(ink_setting_get("audio.vibration") == 1, "touch enables existing switch");
    on_ui(inspect); CHECK(strstr(page, "audio.vibration_level") && strstr(page, "settings.get") && strstr(page, "70ms"), "snapshot exposes values, mapping and actions");
    frame("vibration-zh");
    set("system.language", 1); press("Strength");
    CHECK(ink_setting_get("audio.vibration_level") == 1, "English strength control uses settings.set");
    on_ui(inspect); CHECK(strstr(page, "strength(audio.vibration_level)") && strstr(page, "settings.set"), "English snapshot translated");
    frame("vibration-en");
    set("audio.vibration_level", 2);
    CHECK(WAIT_FOR(!ink_storage_pending(), 10000), "settings writes settle");
}
static void run(void *arg) {
    (void)arg; CHECK(WAIT_FOR(ink_member_space_open(), 10000), "member space opens"); test_start_home(); quiet();
    if (restarting) {
        CHECK(ink_setting_get("audio.vibration") == 1 && ink_setting_get("audio.vibration_level") == 2, "vibration preferences persist after restart");
        on_ui(open_fixture); quiet(); pulse_touch(70);
    } else { input_checks(); ui_checks(); }
    CHECK(WAIT_FOR(sim_metric_value("settings.saved") > 0 || ink_setting_get("system.language") == 1, 10000), "preferences reached storage");
    ink_probe_count("test.finished");
}
void ink_main(void) {
    vtk_host_call(read_phase, NULL);
    product_main();
    osal_task_cfg_t cfg = {.name = "haptic-test", .fn = run, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start haptic test");
}
