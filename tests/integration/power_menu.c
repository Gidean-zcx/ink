/*
 * The power key (system UI spec §3.4): pressed it locks, held down it is the power menu; a call or an alarm does not answer to
 * that; locked it wakes first. INK_MENU_PHASE picks the run: main, or off and restart, which end with the device doing it. Results
 * are the probe counters "test.pass" / "test.fail", as in systest.c.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ink/sys/interrupt.h"
#include "ink/sys/notify.h"

#define ink_main product_main
#include "../../system/boot/main.c"
#undef ink_main

#include "whole_product.h"

static void hold_power(void) {
    hw(HW_KEY_DOWN, HAL_KEY_POWER, 0);
    delay_ms(1700);
}
static void release_power(void) { hw(HW_KEY_UP, HAL_KEY_POWER, 0); }

static bool menu_up(void) {
    on_ui(read_screen);
    return !strcmp(g_top, "power_menu");
}

/* The lock mark says what wakes it (D-38). */
static bool g_mark_ok;
static void look_at_mark(lv_obj_t *o) {
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) {
        lv_obj_t *c = lv_obj_get_child(o, (int32_t)i);
        if (lv_obj_check_type(c, &lv_label_class) && strstr(lv_label_get_text(c), "电源键") && strstr(lv_label_get_text(c), "AI 键")) g_mark_ok = true;
        look_at_mark(c);
    }
}
static void read_mark(void) {
    g_mark_ok = false;
    look_at_mark(lv_layer_top());
}

static void test_main(void) {
    /* a short press is what it always was */
    key(HAL_KEY_POWER);
    CHECK(WAIT_FOR(!strcmp(power_state(), "locked"), 1000), "pressed: it locks (%s)", power_state());
    delay_ms(500);
    on_ui(read_mark);
    CHECK(g_mark_ok, "the mark says the power key or the AI key wakes it");
    ink_power_unlock(osal_now_us());
    CHECK(WAIT_FOR(strcmp(power_state(), "locked") != 0, 70000), "unlocked");
    ink_power_activity(osal_now_us());
    delay_ms(500);
    CHECK(!menu_up(), "no menu");

    /* held down: the menu; letting go does nothing more */
    hold_power();
    delay_ms(600);
    CHECK(menu_up() && g_btn_n == 3, "held down: the menu (%s, %d)", g_top, g_btn_n);
    CHECK(strstr(g_snap, "关机") && strstr(g_snap, "重启"), "the AI sees what it says (%s)", g_snap);
    release_power();
    delay_ms(300);
    CHECK(menu_up() && strcmp(power_state(), "locked"), "let go: it stays, nothing locks (%s)", power_state());
    tap(g_btn_x[2], g_btn_y[2]);
    delay_ms(600);
    CHECK(!menu_up() && strcmp(power_state(), "locked"), "cancel: it goes, nothing else happens (%s)", g_top);

    /* not long enough */
    hw(HW_KEY_DOWN, HAL_KEY_POWER, 0);
    delay_ms(1000);
    release_power();
    delay_ms(600);
    CHECK(!menu_up() && !strcmp(power_state(), "locked"), "a second is still a press (%s, %s)", g_top, power_state());
    ink_power_unlock(osal_now_us());
    CHECK(WAIT_FOR(strcmp(power_state(), "locked") != 0, 70000), "unlocked");
    ink_power_activity(osal_now_us());
    delay_ms(500);

    /* HOME closes it */
    hold_power();
    release_power();
    delay_ms(600);
    CHECK(menu_up(), "again");
    key(HAL_KEY_HOME);
    delay_ms(600);
    CHECK(!menu_up(), "HOME closes it first (%s)", g_top);

    /* locked: it wakes, then the menu */
    ink_power_lock();
    CHECK(WAIT_FOR(!strcmp(power_state(), "locked"), 1000), "locked");
    delay_ms(2000);
    hold_power();
    CHECK(strcmp(power_state(), "locked"), "held down it wakes (%s)", power_state());
    delay_ms(1000);
    CHECK(menu_up(), "and then the menu (%s)", g_top);
    release_power();
    delay_ms(300);
    CHECK(menu_up() && strcmp(power_state(), "locked"), "let go: it stays (%s)", power_state());
    /* locking takes it away: nobody is looking at it */
    ink_power_lock();
    delay_ms(1500);
    ink_power_unlock(osal_now_us());
    CHECK(WAIT_FOR(strcmp(power_state(), "locked") != 0, 70000), "unlocked");
    ink_power_activity(osal_now_us());
    delay_ms(800);
    CHECK(!menu_up(), "gone (%s)", g_top);

    /* a call or an alarm does not answer to it, and letting go is nothing either */
    ink_interrupt_t it = {.kind = INK_INT_CALL, .ringing = true};
    snprintf(it.title, sizeof it.title, "来电");
    int id = ink_interrupt_raise(&it);
    delay_ms(600);
    hold_power();
    release_power();
    delay_ms(600);
    on_ui(read_screen);
    CHECK(strcmp(g_top, "power_menu") && !strcmp(active_state(), "call") && strcmp(power_state(), "locked"), "over a call nothing happens (%s, %s, %s)", g_top, active_state(), power_state());
    double played = sim_audio_played_ms();
    delay_ms(300);
    CHECK(sim_audio_played_ms() > played, "it is still ringing: a long press does not silence it");
    ink_interrupt_end(id);
    delay_ms(800);
    CHECK(!menu_up(), "and nothing was waiting under it (%s)", g_top);

    /* a child can turn it off too, and a closed space does not stand in the way */
    CHECK(ink_member_switch("kid") == 0, "the child");
    hold_power();
    release_power();
    delay_ms(600);
    CHECK(menu_up(), "the menu for the child too");
    tap(g_btn_x[2], g_btn_y[2]);
    delay_ms(600);
    CHECK(AI("system.shutdown", NULL) && g_rc.status == INK_RC_REJECTED, "the AI does not do it for a child (%s)", ink_rc_name(g_rc.status));
    ink_member_test_reset();
}

static void test_off(int button, const char *title) {
    ink_notify_t n = {.title = title, .prio = INK_NOTIFY_LOW};
    CHECK(ink_notify_post(&n, NULL) == 0, "something to write");
    hold_power();
    release_power();
    delay_ms(600);
    CHECK(menu_up(), "the menu");
    ink_probe_count("test.finished");
    tap(g_btn_x[button], g_btn_y[button]);
    delay_ms(60000);
    CHECK(0, "it should have gone by now");
}

/* Written before it went. */
static void test_after(const char *title) {
    ink_db_cursor_t c = {0};
    ink_notify_view_t v;
    bool found = false;
    while (ink_notify_next(&c, &v) == 1) found = found || !strcmp(v.title, title);
    CHECK(found, "what was waiting is there after the next boot (%s)", title);
}

/* Whoever holds the device can turn it off: a child, an adult's closed space. */
static void test_child(void) {
    CHECK(ink_member_switch("kid") == 0, "the child");
    delay_ms(500);
    hold_power();
    release_power();
    delay_ms(600);
    CHECK(menu_up(), "the menu");
    ink_probe_count("test.finished");
    tap(g_btn_x[0], g_btn_y[0]);
    delay_ms(60000);
    CHECK(0, "it should have gone by now");
}

static volatile int g_pin_done;
static void pin_told(void *ctx, int result, uint32_t wait_s) {
    (void)ctx;
    (void)result;
    (void)wait_s;
    g_pin_done++;
}

static void test_closed(void) {
    CHECK(ink_member_set_pin("owner", NULL, "1234", pin_told, NULL) == 0 && WAIT_FOR(g_pin_done == 1, 5000), "a PIN");
    ink_power_lock();
    CHECK(WAIT_FOR(!ink_member_space_open(), 2000), "locked, closed");
    ink_power_unlock(osal_now_us());
    CHECK(WAIT_FOR(strcmp(power_state(), "locked") != 0, 70000), "unlocked");
    ink_power_activity(osal_now_us());
    delay_ms(800);
    on_ui(read_screen);
    CHECK(!strcmp(g_top, "member_gate"), "the gate (%s)", g_top);
    hold_power();
    release_power();
    delay_ms(600);
    CHECK(menu_up(), "the menu over it (%s)", g_top);
    ink_probe_count("test.finished");
    tap(g_btn_x[g_btn_n - 3], g_btn_y[g_btn_n - 3]); /* the gate's buttons come first */
    delay_ms(60000);
    CHECK(0, "it should have gone by now");
}

static void test_task(void *arg) {
    (void)arg;
    delay_ms(1500);
    const char *phase = getenv("INK_MENU_PHASE");
    if (!strcmp(phase, "off")) test_off(0, "关机前的");
    else if (!strcmp(phase, "off_after")) test_after("关机前的");
    else if (!strcmp(phase, "restart")) test_off(1, "重启前的");
    else if (!strcmp(phase, "restart_after")) test_after("重启前的");
    else if (!strcmp(phase, "child")) test_child();
    else if (!strcmp(phase, "closed")) test_closed();
    else test_main();
    ink_probe_count("test.finished");
}

void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name = "menutest", .fn = test_task, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    if (osal_task_create(&cfg, NULL) != OSAL_OK) osal_panic("test task");
}
