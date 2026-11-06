/* D-95: lock grace, wake keys and the same page after authenticating, on the whole product. */
#include <stdio.h>
#include <string.h>

#include "ink/sys/appreg.h"
#include "ink/sys/members.h"
#include "ink/sys/power.h"

#define ink_main product_main
#include "../../system/boot/main.c"
#undef ink_main

#include "whole_product.h"

static volatile int pin_done, pin_result;
static void pin_told(void *ctx, int result, uint32_t wait_s) {
    (void)ctx;
    (void)wait_s;
    pin_result = result;
    pin_done++;
}

static bool set_pin(const char *old_pin, const char *new_pin) {
    int before = pin_done;
    return ink_member_set_pin("owner", old_pin, new_pin, pin_told, NULL) == 0 &&
           WAIT_FOR(pin_done > before, 5000) && pin_result == INK_PIN_OK;
}

static char page[32], saved_snapshot[sizeof g_snap];
static uint32_t version, saved_version;
static lv_obj_t *screen, *saved_screen;
static bool wrong_message;
static uint64_t locked_at;

static void observe_lock(void *ctx, int h, const char *id) {
    (void)ctx;
    (void)h;
    (void)id;
    if (!strcmp(power_state(), "locked")) locked_at = osal_now_us();
}

static bool contains_label(lv_obj_t *root, const char *text, bool click) {
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); i++) {
        lv_obj_t *child = lv_obj_get_child(root, (int32_t)i);
        if (lv_obj_check_type(child, &lv_label_class) && !strcmp(lv_label_get_text(child), text) &&
            (!click || lv_obj_check_type(root, &lv_button_class))) {
            if (click) lv_obj_send_event(root, LV_EVENT_CLICKED, NULL);
            return true;
        }
        if (contains_label(child, text, click)) return true;
    }
    return false;
}

static void inspect(void) {
    read_screen();
    snprintf(page, sizeof page, "%s", ink_ui_page_name());
    screen = lv_screen_active();
    version = ink_ui_content_version();
    wrong_message = contains_label(lv_layer_sys(), "密码不对", false);
}

static void enter_wrong_pin(void) {
    CHECK(contains_label(lv_layer_sys(), "我", true), "select the adult on MembersBack");
    for (int i = 0; i < 4; i++) CHECK(contains_label(lv_layer_sys(), "0", true), "enter a wrong digit by hand");
    CHECK(contains_label(lv_layer_sys(), "确定", true), "submit the wrong PIN by hand");
}

static void enter_right_pin(void) {
    CHECK(contains_label(lv_layer_sys(), "我", true), "select the adult");
    for (int i = 0; i < 4; i++) CHECK(contains_label(lv_layer_sys(), "1", true), "enter a digit by hand");
    CHECK(contains_label(lv_layer_sys(), "确定", true), "submit the PIN by hand");
}

static void open_page(void) {
    CHECK(BY_HAND("reader.open_book", "{\"title\":\"测试长书\"}") && g_rc.status == INK_RC_DONE, "open the real reader (%s)", g_rc.detail);
    CHECK(BY_HAND("reader.goto_page", "{\"page\":4}") && g_rc.status == INK_RC_DONE && g_rc.revision == 4, "read page four");
    delay_ms(3000); /* until the next page's start is known: the snapshot ends there */
    on_ui(inspect);
    CHECK(!strcmp(g_top, "book"), "a book is open (%s)", g_top);
    snprintf(saved_snapshot, sizeof saved_snapshot, "%s", g_snap);
    saved_screen = screen;
    saved_version = version;
}

static void same_page(void) {
    on_ui(inspect);
    CHECK(!strcmp(g_top, "book") && !strcmp(g_snap, saved_snapshot), "the same visible page and text (%s)", g_top);
    CHECK(screen == saved_screen && version == saved_version, "the page object and content version were kept");
    CHECK(AI("reader.read_page", NULL) && g_rc.status == INK_RC_DONE && g_rc.revision == 4 && strstr(g_rc.object, "#page=4"), "the AI sees the same fourth page (%s)", g_rc.object);
    CHECK(ink_app_state_of("reader") == INK_APP_FOREGROUND, "the reader resumes after the gate");
}

static uint64_t lock_now(void) {
    key(HAL_KEY_POWER);
    CHECK(WAIT_FOR(!strcmp(power_state(), "locked"), 2000), "the power key locked it");
    CHECK(ink_member_space_open(), "locking starts the grace, not a PIN prompt");
    return locked_at;
}

static void wait_until(uint64_t at) {
    uint64_t now = osal_now_us();
    if (at > now) osal_task_delay_us(at - now);
}

static void wake_down(void *arg) { sim_input_key((hal_key_t)(intptr_t)arg, true); }
static void wake_up(void *arg) { sim_input_key((hal_key_t)(intptr_t)arg, false); }

static void wake_at(uint64_t at, hal_key_t wake_key) {
    /* Hardware events can wake light sleep; a simulated task's delay cannot. */
    vtk_event_at(at * 1000ull, wake_down, (void *)(intptr_t)wake_key);
    vtk_event_at((at + 80000) * 1000ull, wake_up, (void *)(intptr_t)wake_key);
    wait_until(at + 300000);
}

static void authenticate(bool wrong_first) {
    on_ui(inspect);
    CHECK(!strcmp(g_top, "member_gate") && !strcmp(page, "book"), "MembersBack covers the original book (%s, %s)", g_top, page);
    CHECK(strstr(g_snap, "密码") && !strstr(g_snap, "1111"), "the AI snapshot describes the gate without credentials");
    CHECK(AI("state.list", NULL) && g_rc.status == INK_RC_REJECTED, "the closed space rejects AI calls");
    CHECK(AI("reader.read_page", NULL) && g_rc.status == INK_RC_INVALID, "the page is unavailable behind the overlay");
    key(HAL_KEY_HOME);
    on_ui(inspect);
    CHECK(!strcmp(g_top, "member_gate") && !strcmp(page, "book"), "keys cannot escape MembersBack");
    if (wrong_first) {
        on_ui(enter_wrong_pin);
        CHECK(WAIT_FOR((on_ui(inspect), wrong_message), 5000), "wrong PIN shows an error");
        CHECK(!ink_member_space_open() && !strcmp(g_top, "member_gate"), "a wrong PIN keeps the space closed");
    }
    on_ui(enter_right_pin);
    CHECK(WAIT_FOR(ink_member_space_open(), 5000), "the person's right PIN opens the space");
    CHECK(WAIT_FOR((on_ui(inspect), !strcmp(g_top, "book")), 2000), "the member gate goes away");
    same_page();
}

static void adult_wake(hal_key_t wake_key, int seconds) {
    uint64_t locked = lock_now();
    double wakes = sim_metric_value("ai.wakes");
    wake_at(locked + (uint64_t)seconds * 1000000ull, wake_key);
    CHECK(WAIT_FOR(!strcmp(power_state(), "active"), 2000), "the wake key unlocks the screen");
    CHECK(sim_metric_value("ai.wakes") == wakes, "the wake key is consumed, without starting AI");
    if (seconds < 120) {
        CHECK(ink_member_space_open(), "119 seconds returns without a PIN");
        same_page();
        wait_until(locked + 121000000ull);
        CHECK(ink_member_space_open(), "a short wake cancels the old expiry");
    } else authenticate(true);
}

static osal_sem_t *bg_started, *bg_release;
static ink_exec_job_t bg_job;
static void hold_background(void *arg) {
    (void)arg;
    osal_sem_give(bg_started);
    osal_sem_take(bg_release, OSAL_WAIT_FOREVER);
}

static void busy_background(void) {
    bg_started = osal_sem_create(0, 1);
    bg_release = osal_sem_create(0, 1);
    CHECK(bg_started && bg_release && ink_exec_post(INK_EXEC_BG, &bg_job, hold_background, NULL) == 0, "hold the background executor");
    CHECK(osal_sem_take(bg_started, 2000000) == OSAL_OK, "the background executor is held");
    ink_power_hold(INK_NEED_AWAKE);
    uint64_t locked = lock_now();
    wait_until(locked + 121000000ull);
    int64_t published = 0;
    CHECK(ink_state_get_int(ink_state_find("member.authed"), &published) && published && !ink_member_space_open(),
          "the deadline rejects access before its queued state update runs");
    wake_at(osal_now_us() + 10000, HAL_KEY_AI);
    CHECK(WAIT_FOR(!strcmp(power_state(), "active"), 2000), "AI key wakes while background work waits");
    on_ui(inspect);
    CHECK(!ink_member_space_open() && !strcmp(g_top, "member_gate"), "wake cannot bypass the pending deadline");
    osal_sem_give(bg_release);
    authenticate(false);
    ink_power_release(INK_NEED_AWAKE);
    osal_sem_delete(bg_started);
    osal_sem_delete(bg_release);
}

static void automatic_lock(void) {
    ink_receipt_t rc;
    int64_t previous = ink_setting_get("power.lock_after_s");
    CHECK(ink_setting_set("power.lock_after_s", 60, INK_SRC_TOUCH, &rc) == INK_RC_DONE, "set an automatic lock interval");
    ink_power_activity(osal_now_us());
    uint64_t activity = ink_power_last_activity();
    locked_at = 0;
    wake_at(activity + 179000000ull, HAL_KEY_POWER);
    CHECK(locked_at >= activity + 60000000ull && locked_at < activity + 61000000ull, "automatic lock starts after the reading interval");
    CHECK(ink_member_space_open() && !strcmp(power_state(), "active"), "119 seconds after automatic lock returns directly");
    same_page();
    CHECK(ink_setting_set("power.lock_after_s", previous, INK_SRC_TOUCH, &rc) == INK_RC_DONE, "restore the automatic lock interval");
}

static void test_task(void *arg) {
    (void)arg;
    delay_ms(1500);
    CHECK(ink_state_watch("power.state", observe_lock, NULL, INK_EXEC_DIRECT) == 0, "observe the real lock entry time");
    CHECK(ink_setting_get("member.idle_return_s") == 120, "D-95 defaults to a provisional 120 seconds");
    CHECK(set_pin(NULL, "1111"), "set a synthetic adult PIN");
    open_page();
    delay_ms(121000);
    CHECK(ink_member_space_open() && !strcmp(power_state(), "idle"), "reading for 121 seconds never asks for a PIN");
    same_page();
    adult_wake(HAL_KEY_POWER, 119);
    adult_wake(HAL_KEY_AI, 119);
    adult_wake(HAL_KEY_POWER, 121);
    adult_wake(HAL_KEY_AI, 121);
    automatic_lock();
    busy_background();

    CHECK(setting_by_hand("member.idle_return_s", 30), "the single grace setting can be changed by a person");
    uint64_t locked = lock_now();
    wake_at(locked + 31000000ull, HAL_KEY_POWER);
    CHECK(WAIT_FOR(!ink_member_space_open(), 2000), "the configured 30 seconds is used");
    authenticate(false);

    CHECK(BY_HAND("system.switch_member", "{\"id\":\"kid\"}") && g_rc.status == INK_RC_DONE, "switch to the child through the bus");
    open_page();
    const hal_key_t keys[] = {HAL_KEY_POWER, HAL_KEY_AI};
    for (size_t i = 0; i < sizeof keys / sizeof keys[0]; i++) {
        locked = lock_now();
        wake_at(locked + 121000000ull, keys[i]);
        CHECK(ink_member_space_open(), "the child needs no PIN after either wake key");
        same_page();
        CHECK(ink_member_switch("owner") == -2, "the child still cannot switch into the adult's space");
    }
    int before = pin_done;
    CHECK(ink_member_unlock("owner", "1111", pin_told, NULL) == 0 && WAIT_FOR(pin_done > before, 5000) && pin_result == INK_PIN_OK, "return to the adult with their PIN");
    CHECK(set_pin("1111", ""), "remove the synthetic PIN");
    open_page();
    locked = lock_now();
    wake_at(locked + 121000000ull, HAL_KEY_POWER);
    CHECK(ink_member_space_open(), "an adult without a PIN also remains unaffected");
    same_page();
    ink_probe_count("test.finished");
}

void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name = "waketest", .fn = test_task, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    if (osal_task_create(&cfg, NULL) != OSAL_OK) osal_panic("test task");
}
