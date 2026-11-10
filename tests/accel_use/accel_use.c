/*
 * accel-use (D-122): lift to wake and locking early while lying flat and still, on the whole product with the
 * Plus board model (the SC7A20H register model drives INT1 on the shared GPIO2). INK_ACCEL_PHASE:
 *   main          the behaviour checks
 *   standby_on    locked flat and untouched for the rest of the run, power.lift_wake on (standby current)
 *   standby_off   the same with it off
 *   bumps_on/off  the same with a bump every two minutes
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ink/sys/audio.h"
#include "ink/sys/members.h"

#define ink_main product_main
#include "../../system/boot/main.c"
#undef ink_main

#include "../integration/whole_product.h"

static char phase[16];
static volatile int rec_done;

static void read_env(void *arg) {
    (void)arg;
    const char *value = getenv("INK_ACCEL_PHASE");
    snprintf(phase, sizeof phase, "%s", value ? value : "main");
}

static void on_event(void *ctx, const char *id, const ink_args_t *a) {
    (void)ctx, (void)a;
    if (!strcmp(id, "audio.record_done")) rec_done++;
}

/* The world, in event context as the script does it. While the chip sleeps every task is suspended, the test too:
 * what happens meanwhile is planned ahead as hardware events, relative to plan(). */
static uint64_t T0;
static void plan(void) { T0 = vtk_now(); }
static void orient_ev(void *arg) { sim_accel_orient((const char *)arg); }
static void shake_ev(void *arg) {
    (void)arg;
    sim_accel_shake();
}
static void key_ev(void *arg) { sim_input_key(HAL_KEY_POWER, arg != NULL); }
static void orient_at(int ms, const char *name) { vtk_event_at(T0 + SIM_MS(ms), orient_ev, (void *)name); }
static void shake_at(int ms) { vtk_event_at(T0 + SIM_MS(ms), shake_ev, NULL); }
static void key_at(int ms) {
    vtk_event_at(T0 + SIM_MS(ms), key_ev, (void *)1);
    vtk_event_at(T0 + SIM_MS(ms + 80), key_ev, NULL);
}
/* A power key press later, in case what is tested does not wake it; taken back once it is awake. */
static vtk_event_id_t fallback[2];
static void fallback_key(int ms) {
    fallback[0] = vtk_event_at(T0 + SIM_MS(ms), key_ev, (void *)1);
    fallback[1] = vtk_event_at(T0 + SIM_MS(ms + 80), key_ev, NULL);
}
static void cancel_fallback(void) {
    vtk_lock();
    vtk_event_cancel(fallback[0]);
    vtk_event_cancel(fallback[1]);
    vtk_unlock();
}
static void orient(const char *name) {
    plan();
    orient_at(1, name);
    delay_ms(10);
}
static void shake(void) {
    plan();
    shake_at(1);
    delay_ms(10);
}

static double metric(const char *k) { return sim_metric_value(k); }
static bool locked(void) { return !strcmp(power_state(), "locked"); }
static bool lifted_state(void) {
    int64_t v = 0;
    ink_state_get_int(ink_state_find("power.lifted"), &v);
    return v != 0;
}

static void lock_now(void) {
    key(HAL_KEY_POWER);
    CHECK(WAIT_FOR(locked(), 3000), "the power key locks");
}

/* The planned events run; the test goes on once something unlocked it. */
static bool unlocked_by(int max_ms) { return WAIT_FOR(!locked(), max_ms); }

static char page_before[32];
static void remember_page(void) { snprintf(page_before, sizeof page_before, "%s", ink_ui_page_name()); }
static void read_top(void) { read_screen(); }

static bool press_label(lv_obj_t *root, const char *text) {
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); i++) {
        lv_obj_t *child = lv_obj_get_child(root, (int32_t)i);
        if (lv_obj_check_type(child, &lv_label_class) && !strcmp(lv_label_get_text(child), text)) {
            lv_obj_send_event(root, LV_EVENT_CLICKED, NULL);
            return true;
        }
        if (press_label(child, text)) return true;
    }
    return false;
}
static bool pressed;
static void press_lift_row(void) { pressed = press_label(lv_screen_active(), "拿起亮屏"); }

static volatile int pin_done, pin_result;
static void pin_told(void *ctx, int result, uint32_t wait_s) {
    (void)ctx, (void)wait_s;
    pin_result = result;
    pin_done++;
}

static void lift_wakes(void) {
    CHECK(ink_setting_get("power.lift_wake") == 1, "lift to wake is on by default");
    orient("face_up");
    on_ui(remember_page);
    double lifts = metric("power.lift_wake"), ignored = metric("power.lift_ignored");
    int sleeps = sim_power_sleeps();
    lock_now();
    CHECK(!lifted_state(), "not lifted while locked");
    plan();
    orient_at(8000, "reading");
    fallback_key(20000);
    CHECK(unlocked_by(40000), "woken");
    cancel_fallback();
    CHECK(sim_power_sleeps() > sleeps, "it was asleep");
    CHECK(metric("power.lift_wake") == lifts + 1 && metric("power.lift_ignored") == ignored,
          "lifting it from the table to a reading angle wakes it, nothing ignored");
    CHECK(lifted_state(), "power.lifted tells the AI it was just lifted");
    on_ui(read_top);
    CHECK(ink_member_space_open() && !strcmp(ink_ui_page_name(), page_before) && strcmp(g_top, "member_gate"),
          "within D-95's grace it returns to the same page, as the power key does (%s, %s)", ink_ui_page_name(), page_before);

    /* lifting takes time: still flat at the first look, at a reading angle half a second later */
    orient("face_up");
    lock_now();
    lifts = metric("power.lift_wake");
    plan();
    shake_at(8000);
    orient_at(8500, "reading");
    fallback_key(20000);
    CHECK(unlocked_by(40000), "woken");
    cancel_fallback();
    CHECK(metric("power.lift_wake") == lifts + 1 && metric("power.lift_ignored") == ignored,
          "a lift that reaches the reading angle within the second after the movement wakes it");
}

static void false_wakes(void) {
    orient("face_up");
    lock_now();
    double ignored = metric("power.lift_ignored"), lifts = metric("power.lift_wake");
    int sleeps = sim_power_sleeps();
    plan();
    shake_at(8000); /* a bump on the table */
    key_at(14000);
    CHECK(unlocked_by(30000), "only the power key wakes it");
    CHECK(metric("power.lift_ignored") == ignored + 1 && metric("power.lift_wake") == lifts, "a bump while lying flat leaves it locked");
    CHECK(sim_power_sleeps() >= sleeps + 2, "and the chip sleeps again after looking (%d)", sim_power_sleeps() - sleeps);

    orient("face_up");
    lock_now();
    ignored = metric("power.lift_ignored");
    plan();
    for (int i = 0; i < 10; i++) shake_at(8000 + 3000 * i); /* flat in a bag that keeps moving */
    key_at(40000);
    CHECK(unlocked_by(60000), "woken by the key");
    CHECK(metric("power.lift_ignored") == ignored + 3 && metric("power.lift_wake") == lifts,
          "shaking flat in a bag: three motion wakes, then none (%.0f)", metric("power.lift_ignored") - ignored);

    orient("face_up");
    lock_now();
    ignored = metric("power.lift_ignored");
    plan();
    for (int i = 0; i < 4; i++) shake_at(8000 + 3000 * i);
    shake_at(90000); /* after a slice that ended by its timer */
    key_at(95000);
    CHECK(unlocked_by(120000), "woken by the key");
    CHECK(metric("power.lift_ignored") == ignored + 4, "after a quiet slice it watches again (%.0f)", metric("power.lift_ignored") - ignored);

    orient("face_up");
    lock_now();
    ignored = metric("power.lift_ignored");
    plan();
    orient_at(8000, "portrait"); /* standing up in the bag: a motion wake, not a reading angle */
    for (int i = 0; i < 5; i++) shake_at(12000 + 3000 * i);
    orient_at(30000, "reading"); /* from upright to a reading angle: not a lift from the table */
    key_at(34000);
    CHECK(unlocked_by(60000), "woken by the key");
    CHECK(metric("power.lift_ignored") == ignored + 1 && metric("power.lift_wake") == lifts,
          "not lying flat it does not watch for movement, and nothing lit it (%.0f)", metric("power.lift_ignored") - ignored);
}

static void snap_ev(void *arg) { sim_display_snapshot((const char *)arg); }

static void to_lift_row(void) {
    if (!strstr(g_snap, "拿起亮屏") && strstr(g_snap, "family.next")) { /* the row is on the list's next page */
        CHECK(BY_HAND("family.next", NULL) && g_rc.status == INK_RC_DONE, "next page of the list");
        delay_ms(1500);
        on_ui(read_top);
    }
}

static void setting_off(void) {
    CHECK(AI("settings.set", "{\"id\":\"power.lift_wake\",\"value\":0}") && g_rc.status == INK_RC_DONE, "the AI turns it off");
    orient("face_up");
    lock_now();
    double ignored = metric("power.lift_ignored"), lifts = metric("power.lift_wake");
    plan();
    orient_at(8000, "reading");
    key_at(12000);
    CHECK(unlocked_by(30000), "woken by the key");
    CHECK(metric("power.lift_wake") == lifts && metric("power.lift_ignored") == ignored, "off: lifting does not wake it, nor does the chip wake to look");
    CHECK(BY_HAND("shell.settings", NULL) && g_rc.status == INK_RC_DONE, "open the settings");
    delay_ms(1500);
    on_ui(read_top);
    CHECK(strstr(g_snap, "拿起亮屏(power.lift_wake)=0") != NULL, "the settings snapshot shows it off (%.150s)", g_snap);
    CHECK(BY_HAND("shell.wallpaper", NULL) && g_rc.status == INK_RC_DONE, "open the lock screen settings");
    delay_ms(1500);
    on_ui(read_top);
    to_lift_row();
    CHECK(strstr(g_snap, "拿起亮屏") && strstr(g_snap, "power.lift_wake"), "its row is in that page's snapshot (%.200s)", g_snap);
    on_ui(press_lift_row);
    CHECK(pressed && WAIT_FOR(ink_setting_get("power.lift_wake") == 1, 2000), "the person turns it on again with the same setting");
    delay_ms(1500);
    on_ui(read_top);
    to_lift_row();
    vtk_event_at(vtk_now() + SIM_MS(1), snap_ev, (void *)"wallpaper-lift"); /* PNG for the review */
    delay_ms(50);
    CHECK(strstr(g_snap, "开，平放不动 3 分钟先锁屏") != NULL, "and the row follows (%.200s)", g_snap);
    CHECK(BY_HAND("system.home", NULL) && g_rc.status == INK_RC_DONE, "back home");
}

/* Idle and lying flat and still: locked 5 s + 180 s after the last input instead of 10 minutes. */
static void still_locks(void) {
    orient("face_up");
    delay_ms(1000);
    ink_power_activity(osal_now_us());
    uint64_t t0 = osal_now_us();
    double still = metric("power.still_lock");
    CHECK(WAIT_FOR(locked(), 200000), "lying flat and still it locks early");
    uint64_t dt = osal_now_us() - t0;
    CHECK(dt >= 185000000ull && dt < 187000000ull && metric("power.still_lock") == still + 1, "after 3 minutes of idle (%llu ms)",
          (unsigned long long)(dt / 1000));
    int sleeps = sim_power_sleeps();
    double lifts = metric("power.lift_wake");
    plan();
    orient_at(8000, "reading");
    fallback_key(20000);
    CHECK(unlocked_by(40000), "woken");
    cancel_fallback();
    CHECK(sim_power_sleeps() > sleeps && metric("power.lift_wake") == lifts + 1, "it sleeps, and lifting it brings it back");

    orient("face_up");
    delay_ms(1000);
    ink_power_activity(osal_now_us());
    t0 = osal_now_us();
    delay_ms(100000);
    shake(); /* moved on the table */
    delay_ms(100000);
    CHECK(!locked(), "a movement starts the still time over");
    CHECK(WAIT_FOR(locked(), 200000) && osal_now_us() - t0 >= 300000000ull, "and it locks a full still time later (%llu ms)",
          (unsigned long long)((osal_now_us() - t0) / 1000));
    plan();
    key_at(5000);
    CHECK(unlocked_by(20000), "woken by the key");

    orient("reading"); /* held, not lying flat */
    delay_ms(1000);
    ink_power_activity(osal_now_us());
    delay_ms(200000);
    CHECK(!locked(), "held at a reading angle it keeps the normal lock time");
}

static void busy_keeps_awake(void) {
    orient("face_up");
    delay_ms(1000);
    int done = rec_done;
    CHECK(BY_HAND("audio.record", "{\"path\":\"/me/rec/still.opus\"}") && g_rc.status == INK_RC_DONE, "start recording");
    ink_power_activity(osal_now_us());
    delay_ms(200000);
    CHECK(!locked(), "recording: not locked early");
    CHECK(BY_HAND("audio.record_stop", NULL) && WAIT_FOR(rec_done == done + 1, 5000), "stop recording");
    CHECK(BY_HAND("media.play", "{\"path\":\"/me/rec/still.opus\",\"start_ms\":0}") && g_rc.status == INK_RC_DONE, "play it (%s)",
          g_rc.detail);
    ink_power_activity(osal_now_us());
    delay_ms(195000);
    CHECK(!locked(), "playing: not locked early");
    ink_media_stop();
    delay_ms(1000);

    int token = ink_audio_focus_acquire(INK_AUDIO_PRIO_CALL, NULL, NULL, INK_EXEC_DIRECT); /* what the call service holds */
    CHECK(token > 0, "a call holds the sound");
    ink_power_activity(osal_now_us());
    delay_ms(200000);
    CHECK(!locked(), "in a call: not locked early");
    ink_audio_focus_release(token);

    ink_power_hold(INK_NEED_NET); /* a download */
    ink_power_activity(osal_now_us());
    delay_ms(200000);
    CHECK(!locked(), "downloading: not locked early");
    ink_power_release(INK_NEED_NET);
}

static void gate_after_grace(void) {
    int before = pin_done;
    CHECK(ink_member_set_pin("owner", NULL, "1111", pin_told, NULL) == 0 && WAIT_FOR(pin_done > before, 5000) && pin_result == INK_PIN_OK,
          "a synthetic adult PIN");
    orient("face_up");
    lock_now();
    plan();
    orient_at(122000, "reading");
    double lifts = metric("power.lift_wake");
    fallback_key(135000);
    CHECK(unlocked_by(160000), "woken");
    cancel_fallback();
    CHECK(metric("power.lift_wake") == lifts + 1, "lifted after the grace");
    delay_ms(1500);
    on_ui(read_top);
    CHECK(!ink_member_space_open() && !strcmp(g_top, "member_gate"), "past D-95's grace it asks for the PIN, as the power key does (%s)", g_top);
    before = pin_done;
    CHECK(ink_member_unlock("owner", "1111", pin_told, NULL) == 0 && WAIT_FOR(pin_done > before, 5000) && pin_result == INK_PIN_OK, "the PIN");
    before = pin_done;
    CHECK(ink_member_set_pin("owner", "1111", "", pin_told, NULL) == 0 && WAIT_FOR(pin_done > before, 5000), "remove the PIN");
}

/* The average current from the lock to the key press 2300 s later, from the battery model's charge. */
static void standby(bool on, bool bumps) {
    CHECK(BY_HAND("settings.set", on ? "{\"id\":\"power.lift_wake\",\"value\":1}" : "{\"id\":\"power.lift_wake\",\"value\":0}") &&
              g_rc.status == INK_RC_DONE,
          "set lift to wake");
    orient("face_up");
    delay_ms(5000); /* what the change redraws is drawn */
    lock_now();
    double soc = sim_battery_soc();
    uint64_t t = osal_now_us();
    plan();
    for (int i = 0; bumps && i < 15; i++) shake_at(120000 * (i + 1));
    key_at(2300000);
    CHECK(unlocked_by(2400000), "only the key at the end wakes it");
    double hours = (double)(osal_now_us() - t) / 3.6e9;
    sim_metric_set("test.standby_ma", (soc - sim_battery_soc()) * g_profile.batt_mah / hours);
}

static void run(void *arg) {
    (void)arg;
    vtk_host_call(read_env, NULL);
    CHECK(WAIT_FOR(ink_member_space_open(), 10000), "member space opens");
    test_start_home();
    CHECK(ink_cap_subscribe("audio.record_done", on_event, NULL, INK_EXEC_DIRECT) == 0, "subscribe");
    delay_ms(2000);
    if (!strncmp(phase, "standby_", 8)) standby(!strcmp(phase, "standby_on"), false);
    else if (!strncmp(phase, "bumps_", 6)) standby(!strcmp(phase, "bumps_on"), true);
    else {
        lift_wakes();
        false_wakes();
        setting_off();
        still_locks();
        busy_keeps_awake();
        gate_after_grace();
    }
    ink_probe_count("test.finished");
}

void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name = "acceltest", .fn = run, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    if (osal_task_create(&cfg, NULL) != OSAL_OK) osal_panic("test task");
}
