/*
 * The power key (system UI spec §3.4): pressed it locks, held down it is the power menu; a call or an alarm does not answer to
 * that; locked it wakes first. INK_MENU_PHASE picks the run: main, or off and restart, which end with the device doing it. Results
 * are the probe counters "test.pass" / "test.fail", as in systest.c.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ink/sys/data.h"
#include "ink/sys/interrupt.h"

#define ink_main product_main
#include "../../system/boot/main.c"
#undef ink_main

#define INK_TEST_BUTTONS 5
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
static bool problem_recorded(void) {
    on_ui(read_screen);
    return strstr(g_snap, "已记录问题 #") != NULL;
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
    look_at_mark(lv_layer_sys());
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
    CHECK(menu_up() && g_btn_n == 5, "held down: the menu (%s, %d)", g_top, g_btn_n);
    CHECK(strstr(g_snap, "关机") && strstr(g_snap, "重启"), "the AI sees what it says (%s)", g_snap);
    release_power();
    delay_ms(300);
    CHECK(menu_up() && strcmp(power_state(), "locked"), "let go: it stays, nothing locks (%s)", power_state());
    tap(g_btn_x[2], g_btn_y[2]);
    CHECK(WAIT_FOR(problem_recorded(), 5000), "the menu shows the durable problem number: %.150s", g_snap);
    tap(g_btn_x[4], g_btn_y[4]);
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
    tap(g_btn_x[4], g_btn_y[4]);
    delay_ms(600);
    CHECK(AI("system.shutdown", NULL) && g_rc.status == INK_RC_REJECTED, "the AI does not do it for a child (%s)", ink_rc_name(g_rc.status));
    ink_member_test_reset();
}

/* What may wait (a reading position, say): it has to go before the power does. */
static const ink_db_def_t KEPT = {.name = "test.kept", .scope = INK_DB_DEVICE, .schema = 1};

/* What is on the panel when it goes: the run ends with the power, so what was seen is a counter the script around it looks at. */
extern void (*sim_power_before_shutdown)(void);
static uint32_t g_commits_before;

static void at_shutdown(void) { /* all the way onto the panel, the moment the power is asked to go */
    if (ink_display_commits() > g_commits_before && !ink_ui_busy() && ink_display_busy() <= 0) ink_probe_count("test.off_picture_drawn");
}

static void watch_off_picture(const char *must, const char *must_not) {
    sim_power_before_shutdown = at_shutdown;
    for (int i = 0; i < 4000; i++) {
        g_commits_before = ink_display_commits();
        on_ui(read_screen);
        if (!strcmp(g_top, "off")) {
            CHECK(strstr(g_snap, "已关机") && (!must || strstr(g_snap, must)) && (!must_not || !strstr(g_snap, must_not)), "what it says (%s)", g_snap);
            ink_probe_count("test.saw_off_picture");
            delay_ms(60000);
        }
        delay_ms(10);
    }
    CHECK(0, "it should have gone by now");
}

static void test_off(int button, const char *title) {
    ink_db_register(&KEPT);
    ink_db_t *db = ink_db_open(&KEPT, NULL);
    ink_db_write_t w = {.id = (uint32_t)button + 1, .data = title, .len = strlen(title) + 1, .flags = INK_DB_LAZY};
    CHECK(db && ink_db_put(db, &w, NULL) == INK_DB_OK && ink_db_durable_ver(db) < ink_db_last_ver(db), "something that may wait");
    if (button == 0) {
        CHECK(AI("reader.open_book", "{\"title\":\"星星邮差\"}") && g_rc.status == INK_RC_DONE, "reading (%s)", g_rc.detail);
        delay_ms(1500);
    }
    hold_power();
    release_power();
    delay_ms(600);
    CHECK(menu_up(), "the menu");
    ink_probe_count("test.finished");
    tap(g_btn_x[button], g_btn_y[button]);
    if (button == 0) watch_off_picture("星星邮差", NULL); /* where the reading was */
    delay_ms(60000);
    CHECK(0, "it should have gone by now");
}

/* The battery: what waits is written, then the picture, then the power goes. */
static void test_battery(void) {
    ink_db_register(&KEPT);
    ink_db_t *db = ink_db_open(&KEPT, NULL);
    static const char TITLE[] = "没电前的";
    ink_db_write_t w = {.id = 3, .data = TITLE, .len = sizeof TITLE, .flags = INK_DB_LAZY};
    CHECK(db && ink_db_put(db, &w, NULL) == INK_DB_OK && ink_db_durable_ver(db) < ink_db_last_ver(db), "something that may wait");
    ink_probe_count("test.finished");
    hw(HW_BATTERY, 2, 0);
    watch_off_picture("没电了", NULL);
}

static void test_battery_after(void) {
    ink_db_register(&KEPT);
    ink_db_t *db = ink_db_open(&KEPT, NULL);
    char got[32] = "";
    CHECK(db && ink_db_get(db, 3, NULL, got, sizeof got) == INK_DB_OK && !strcmp(got, "没电前的"), "what was waiting is there after (%s)", got);
    ink_db_close(db);
}

/* Written before it went. */
static void test_after(int button, const char *title) {
    ink_db_register(&KEPT);
    ink_db_t *db = ink_db_open(&KEPT, NULL);
    char got[32] = "";
    CHECK(db && ink_db_get(db, (uint32_t)button + 1, NULL, got, sizeof got) == INK_DB_OK && !strcmp(got, title), "what was waiting is there after the next boot (%s)", got);
    ink_db_close(db);
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
    watch_off_picture(NULL, NULL);
}

/* The button with this label, on the system cards: there may be more buttons than read_screen keeps. */
static const char *g_label;
static int g_label_x, g_label_y;
static void label_in(lv_obj_t *o) {
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) {
        lv_obj_t *c = lv_obj_get_child(o, (int32_t)i);
        if (lv_obj_check_type(c, &lv_label_class) && !strcmp(lv_label_get_text(c), g_label)) {
            lv_area_t a;
            lv_obj_update_layout(c);
            lv_obj_get_coords(c, &a);
            g_label_x = (a.x1 + a.x2) / 2;
            g_label_y = (a.y1 + a.y2) / 2;
        }
        label_in(c);
    }
}
static void find_label(void) {
    g_label_x = -1;
    label_in(lv_layer_sys());
}

static volatile int g_pin_done;
static void pin_told(void *ctx, int result, uint32_t wait_s) {
    (void)ctx;
    (void)result;
    (void)wait_s;
    g_pin_done++;
}

/* An app that has a place to leave, whatever else goes on. */
static int always_card(int index, ink_card_t *out) {
    if (index) return 0;
    snprintf(out->id, sizeof out->id, "place");
    snprintf(out->title, sizeof out->title, "在读");
    snprintf(out->text, sizeof out->text, "别人的书");
    out->bookmark = true;
    return 1;
}

static const ink_app_def_t ALWAYS_APP = {.id = "test.always", .title = "测试", .card = always_card};

static void test_closed(void) {
    CHECK(ink_app_register(&ALWAYS_APP) == 0, "something to leave");
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
    g_label = "关机";
    on_ui(find_label);
    CHECK(g_label_x >= 0, "its button");
    tap(g_label_x, g_label_y);
    watch_off_picture(NULL, "在读"); /* the space is closed: nothing of what was behind it */
}

static void test_task(void *arg) {
    (void)arg;
    delay_ms(1500);
    const char *phase = getenv("INK_MENU_PHASE");
    if (!strcmp(phase, "off")) test_off(0, "关机前的");
    else if (!strcmp(phase, "off_after")) test_after(0, "关机前的");
    else if (!strcmp(phase, "restart")) test_off(1, "重启前的");
    else if (!strcmp(phase, "restart_after")) test_after(1, "重启前的");
    else if (!strcmp(phase, "child")) test_child();
    else if (!strcmp(phase, "closed")) test_closed();
    else if (!strcmp(phase, "battery")) test_battery();
    else if (!strcmp(phase, "battery_after")) test_battery_after();
    else test_main();
    ink_probe_count("test.finished");
}

void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name = "menutest", .fn = test_task, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    if (osal_task_create(&cfg, NULL) != OSAL_OK) osal_panic("test task");
}
