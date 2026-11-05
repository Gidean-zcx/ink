/*
 * Notifications and the interruption channel (system services design §6), on the whole product: the inbox and its
 * capabilities; interruptions that do not wait for the UI, take the input and come first; and the events that lead
 * to them, from the modem, the RTC, the USB cable and the battery, as the simulated hardware makes them happen.
 * INK_NOTIFY_PHASE picks the run: main, lowbat1 and lowbat2 (a nearly empty battery saves and powers off, and what
 * was waiting to be written is there at the next boot).
 * Results are the probe counters "test.pass" / "test.fail", as in systest.c.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ink/hal_input.h"
#include "ink/osal.h"
#include "ink/probe.h"
#include "ink/sys/audio.h"
#include "ink/sys/cap.h"
#include "ink/sys/confirm.h"
#include "ink/sys/data.h"
#include "ink/sys/exec.h"
#include "ink/sys/file.h"
#include "ink/sys/interrupt.h"
#include "ink/sys/members.h"
#include "ink/sys/notify.h"
#include "ink/sys/power.h"
#include "ink/sys/settings.h"
#include "ink/sys/state.h"
#include "ink/sys/time.h"
#include "ink/sys/ui.h"
#include "sim.h"

#define ink_main product_main
#include "../../system/boot/main.c"
#undef ink_main

#include "whole_product.h"

static int unread_state(void) {
    int64_t v = -1;
    ink_state_get_int(ink_state_find("notify.unread"), &v);
    return (int)v;
}

static int inbox_count(void) {
    ink_db_cursor_t c = {0};
    ink_notify_view_t v;
    int n = 0;
    while (ink_notify_next(&c, &v) == 1) n++;
    return n;
}

/* The newest with this title. */
static bool inbox_find(const char *title, ink_notify_view_t *out) {
    ink_db_cursor_t c = {0};
    while (ink_notify_next(&c, out) == 1)
        if (!strcmp(out->title, title)) return true;
    return false;
}

/* A page of our own, to see what reaches the foreground. */
static volatile int g_page_keys, g_page_swipes, g_page_clicks;
static bool page_key(ink_key_t k, bool lng) {
    (void)k;
    (void)lng;
    g_page_keys++;
    return true;
}
static bool page_swipe(ink_swipe_dir_t d) {
    (void)d;
    g_page_swipes++;
    return true;
}
static void page_click(lv_event_t *e) {
    (void)e;
    g_page_clicks++;
}
static void page_create(lv_obj_t *scr, void *arg) {
    (void)arg;
    lv_obj_t *b = lv_button_create(scr);
    ink_ui_style_button(b);
    lv_obj_set_size(b, LV_PCT(100), LV_PCT(100));
    lv_obj_add_event_cb(b, page_click, LV_EVENT_CLICKED, NULL);
}
static const ink_activity_t PAGE = {.name = "testpage", .create = page_create, .on_key = page_key, .on_swipe = page_swipe};
static void open_page(void) { ink_ui_push(&PAGE, NULL); }
static void close_page(void) { ink_ui_pop(); }

/* A capability behind a button, and one that only works on our page. */
static volatile int g_act_runs;
static volatile int g_act_src;
static void act_run(ink_call_t *c) {
    g_act_runs++;
    g_act_src = c->src;
}
static void page_cap_run(ink_call_t *c) { (void)c; }
static const ink_cap_t TEST_CAPS[] = {
    {.id = "test.act", .title = "按钮", .effect = INK_FX_NAVIGATE, .exec = INK_EXEC_DIRECT, .run = act_run},
    {.id = "test.page_cap", .title = "只在这一页", .activity = "testpage", .effect = INK_FX_READ, .exec = INK_EXEC_DIRECT, .run = page_cap_run},
};

static ink_interrupt_t card(ink_int_kind_t kind, const char *title, bool ringing) {
    ink_interrupt_t it = {.kind = kind, .ringing = ringing, .nactions = 2};
    snprintf(it.title, sizeof it.title, "%s", title);
    snprintf(it.body, sizeof it.body, "内容");
    it.actions[0] = (ink_int_action_t){.label = "第一", .cap = "test.act"};
    it.actions[1] = (ink_int_action_t){.label = "关掉"};
    return it;
}

/* Local time in `min` minutes, rounded to a minute. */
static ink_alarm_t alarm_in(ink_alarm_kind_t kind, int min, const char *title) {
    ink_tm_t t;
    ink_time_to_local(ink_time_now_s() + min * 60 + 59, &t);
    ink_alarm_t a = {.kind = kind, .hour = (uint8_t)t.hour, .min = (uint8_t)t.min};
    snprintf(a.title, sizeof a.title, "%s", title);
    return a;
}

/* ------------------------------------------------------------ the inbox */

static void inbox(void) {
    ink_notify_t a = {.channel = "message", .title = "第一条", .body = "正文", .group = "g1", .prio = -1, .src = INK_SRC_SYSTEM};
    uint32_t id1 = 0, id2 = 0;
    CHECK(ink_notify_post(&a, &id1) == 0 && id1, "posted");
    CHECK(unread_state() == 1 && ink_notify_unread() == 1, "one not read (%d)", unread_state());
    ink_db_cursor_t c = {0};
    ink_notify_view_t v;
    CHECK(ink_notify_next(&c, &v) == 1 && v.id == id1 && !strcmp(v.channel, "message") && !strcmp(v.title, "第一条") && !strcmp(v.body, "正文") && !strcmp(v.group, "g1") && !v.read && v.prio == INK_NOTIFY_NORMAL,
          "what it says comes back (%s %s %s)", v.channel, v.title, v.body);
    delay_ms(1100);
    ink_notify_t b = {.title = "第二条", .prio = INK_NOTIFY_LOW, .src = INK_SRC_SYSTEM};
    CHECK(ink_notify_post(&b, &id2) == 0, "another");
    memset(&c, 0, sizeof c);
    CHECK(ink_notify_next(&c, &v) == 1 && v.id == id2 && !strcmp(v.channel, "system"), "newest first, in the system channel when none is named (%s)", v.title);
    CHECK(ink_notify_next(&c, &v) == 1 && v.id == id1, "then the older");
    CHECK(ink_notify_next(&c, &v) == 0, "that is all");

    ink_notify_t t1 = {.channel = "download", .title = "下载中", .body = "10%", .tag = "dl", .prio = -1};
    uint32_t tid = 0, tid2 = 0;
    CHECK(ink_notify_post(&t1, &tid) == 0 && unread_state() == 3, "a tagged one");
    t1.body = "90%";
    CHECK(ink_notify_post(&t1, &tid2) == 0 && tid2 == tid && unread_state() == 3 && inbox_count() == 3, "the same tag replaces it (%u %u, %d)", (unsigned)tid, (unsigned)tid2, inbox_count());
    CHECK(inbox_find("下载中", &v) && !strcmp(v.body, "90%"), "with what it says now");

    CHECK(ink_notify_mark_read(id1) == 0 && unread_state() == 2, "read");
    CHECK(inbox_find("第一条", &v) && v.read && !strcmp(v.body, "正文"), "marked, nothing else changed");
    CHECK(ink_notify_mark_read(id1) == 0 && unread_state() == 2, "twice is once");
    CHECK(ink_notify_mark_read(0) == 0 && unread_state() == 0, "all of them");
    CHECK(ink_notify_cancel(id2) == 0 && inbox_count() == 2, "one gone");
    CHECK(ink_notify_cancel(id2) != 0, "not there any more");
    CHECK(ink_notify_cancel_tag("dl") == 0 && inbox_count() == 1, "by its tag");
    CHECK(ink_notify_cancel_tag("dl") != 0, "and that tag is gone");
    CHECK(ink_notify_clear() == 0 && inbox_count() == 0 && unread_state() == 0, "cleared");

    char title[16];
    ink_notify_t many = {.title = title, .prio = INK_NOTIFY_LOW};
    for (int i = 0; i < 205; i++) {
        snprintf(title, sizeof title, "n%d", i);
        ink_notify_post(&many, NULL);
    }
    CHECK(inbox_count() == 200 && unread_state() == 200, "200 at most (%d, %d)", inbox_count(), unread_state());
    CHECK(!inbox_find("n4", &v) && inbox_find("n5", &v) && inbox_find("n204", &v), "the oldest went");
    ink_notify_clear();

    char longer[1200];
    memset(longer, 'x', sizeof longer - 1);
    longer[sizeof longer - 1] = 0;
    ink_notify_t too = {.title = "太长", .body = longer, .prio = INK_NOTIFY_LOW};
    CHECK(ink_notify_post(&too, NULL) != 0 && inbox_count() == 0, "too long for a record is refused, nothing half written");
    ink_notify_t no_title = {.body = "x"};
    CHECK(ink_notify_post(&no_title, NULL) != 0, "no title");
}

static const ink_notify_channel_t APP_CH = {"app.loud", "一个应用的渠道", INK_NOTIFY_HIGH};

static void channels(void) {
    CHECK(BY_HAND("notify.set_channel", "{\"id\":\"message\",\"enabled\":false}") && g_rc.status == INK_RC_DONE, "a channel off (%s)", g_rc.detail);
    CHECK(!ink_notify_channel_on("message") && ink_notify_channel_on("system"), "only that one");
    ink_notify_t m = {.channel = "message", .title = "别来", .prio = -1};
    CHECK(ink_notify_post(&m, NULL) == 0 && inbox_count() == 0, "what it posts goes nowhere");
    CHECK(AI("notify.channels", NULL) && strstr(g_result, "\"message\":false") && strstr(g_result, "\"system\":true"), "the AI can see them (%s)", g_result);
    CHECK(BY_HAND("notify.set_channel", "{\"id\":\"nope\",\"enabled\":false}") && g_rc.status == INK_RC_INVALID, "no such channel");

    /* another member has their own */
    CHECK(ink_member_switch("kid") == 0, "switch");
    CHECK(WAIT_FOR(ink_notify_channel_on("message"), 2000), "theirs is on");
    ink_member_test_reset(); /* back to the adult; by hand that takes the PIN */
    CHECK(WAIT_FOR(!ink_notify_channel_on("message"), 2000), "and the choice was kept");
    CHECK(BY_HAND("notify.set_channel", "{\"id\":\"message\",\"enabled\":true}") && ink_notify_channel_on("message"), "on again");

    CHECK(ink_notify_channel_define(&APP_CH) == 0 && ink_notify_channel_define(&APP_CH) != 0, "an app's channel, once");
    CHECK(AI("notify.channels", NULL) && strstr(g_result, "\"app.loud\":true"), "the AI sees it too (%s)", g_result);
    int alerted = (int)sim_metric_value("notify.alerted");
    double played = sim_audio_played_ms();
    ink_notify_t loud = {.channel = "app.loud", .title = "高优先级", .prio = -1};
    uint32_t id = 0;
    CHECK(ink_notify_post(&loud, &id) == 0 && (int)sim_metric_value("notify.alerted") == alerted + 1, "a high priority one has a sound");
    CHECK(WAIT_FOR(sim_audio_played_ms() > played, 2000), "and it is heard");
    ink_receipt_t rc;
    ink_setting_set("notify.dnd", 1, INK_SRC_TOUCH, &rc);
    CHECK(ink_notify_post(&loud, NULL) == 0 && (int)sim_metric_value("notify.alerted") == alerted + 1 && inbox_count() == 2, "do not disturb: only into the inbox");
    ink_setting_set("notify.dnd", 0, INK_SRC_TOUCH, &rc);
    ink_notify_t soft = {.channel = "app.loud", .title = "普通", .prio = INK_NOTIFY_NORMAL};
    CHECK(ink_notify_post(&soft, NULL) == 0 && (int)sim_metric_value("notify.alerted") == alerted + 1, "normal priority is quiet");
    ink_notify_clear();
}

static void members(void) {
    ink_notify_t forkid = {.title = "给孩子的", .prio = INK_NOTIFY_HIGH, .member = "kid", .src = INK_SRC_SYSTEM};
    int alerted = (int)sim_metric_value("notify.alerted");
    CHECK(ink_notify_post(&forkid, NULL) == 0, "posted for the other member");
    delay_ms(500);
    CHECK(unread_state() == 0 && inbox_count() == 0 && (int)sim_metric_value("notify.alerted") == alerted, "it does not disturb the one using the device");
    ink_notify_t nobody = {.title = "x", .member = "../etc"};
    CHECK(ink_notify_post(&nobody, NULL) != 0, "an impossible member");
    CHECK(ink_member_switch("kid") == 0, "switch");
    ink_notify_view_t v;
    CHECK(WAIT_FOR(unread_state() == 1, 3000) && inbox_find("给孩子的", &v), "there it is (%d)", unread_state());
    ink_notify_clear();
    ink_member_test_reset();
    ink_notify_t mine = {.title = "只给大人", .prio = INK_NOTIFY_LOW};
    ink_notify_post(&mine, NULL);
    CHECK(WAIT_FOR(strcmp(ink_member_current()->id, "kid") != 0 && inbox_find("只给大人", &v) && unread_state() == 1, 3000), "the adult's inbox again (%d)", unread_state());
    ink_notify_clear();
}

static void capabilities(void) {
    CHECK(AI("notify.post", "{\"title\":\"AI 说\",\"body\":\"提醒一下\",\"channel\":\"message\",\"action\":\"time.now\"}") && g_rc.status == INK_RC_DONE && g_rc.undo_ms > 0, "the AI posts (%s %s)", ink_rc_name(g_rc.status), g_rc.detail);
    char object[48], args[80];
    snprintf(object, sizeof object, "%s", g_rc.object);
    ink_notify_view_t v;
    CHECK(inbox_find("AI 说", &v) && !strcmp(v.channel, "ai") && v.src == INK_SRC_AI && !strcmp(v.action, "time.now"), "in its own channel whatever it asks, and it says who (%s, %d)", v.channel, v.src);
    CHECK(AI("notify.list", "{\"limit\":5}") && strstr(g_result, "\"unread\":1") && strstr(g_result, "\"title\":\"AI 说\"") && strstr(g_result, "\"action\":\"time.now\""), "the AI reads the list (%s)", g_result);
    snprintf(args, sizeof args, "{\"object\":\"%s\"}", object);
    CHECK(AI("system.undo", args) && g_rc.status == INK_RC_UNDONE && inbox_count() == 0, "and takes it back (%s)", g_rc.detail);

    ink_notify_t two = {.title = "两条之一", .prio = INK_NOTIFY_LOW};
    uint32_t id = 0;
    ink_notify_post(&two, &id);
    two.title = "两条之二";
    ink_notify_post(&two, NULL);
    char byid[40];
    snprintf(byid, sizeof byid, "{\"id\":%u}", (unsigned)id);
    CHECK(AI("notify.read", byid) && g_rc.status == INK_RC_DONE && unread_state() == 1, "marks one read");
    CHECK(AI("notify.list", "{\"unread_only\":true}") && strstr(g_result, "两条之二") && !strstr(g_result, "两条之一"), "only what was not read (%s)", g_result);
    CHECK(AI("notify.dismiss", byid) && inbox_count() == 1, "removes one");
    CHECK(AI("notify.dismiss", "{\"id\":999999}") && g_rc.status == INK_RC_FAILED, "not there");
    CHECK(AI("notify.clear", NULL) && g_rc.status == INK_RC_PENDING_CONFIRMATION && inbox_count() == 1, "clearing all of it needs a yes (%s)", ink_rc_name(g_rc.status));
    unsigned token = 0;
    sscanf(g_rc.object, "confirm:%u", &token);
    CHECK(ink_confirm_withdraw(token, INK_SRC_AI) == 0, "the AI takes that back");
    delay_ms(1200);
    CHECK(BY_HAND("notify.clear", NULL) && g_rc.status == INK_RC_PENDING_CONFIRMATION && inbox_count() == 1, "D-105: bulk deletion asks by hand too");
    sscanf(g_rc.object, "confirm:%u", &token);
    CHECK(ink_confirm_decide(token, true, INK_SRC_TOUCH) == 0 && WAIT_FOR(inbox_count() == 0, 1000), "cleared after the person accepts");
}

/* ------------------------------------------------------------ interruptions */

static void stuck_job(void) { delay_ms(3000); }
static ink_exec_job_t g_stuck;
static void stuck_ui(void *arg) {
    (void)arg;
    stuck_job();
}

static void not_waiting_for_the_ui(void) {
    ink_exec_post(INK_EXEC_UI, &g_stuck, stuck_ui, NULL);
    delay_ms(100); /* it is in the UI task now, for three seconds */
    double played = sim_audio_played_ms();
    ink_interrupt_t it = card(INK_INT_CALL, "来电", true);
    int id = ink_interrupt_raise(&it);
    CHECK(id > 0 && !strcmp(active_state(), "call"), "raised (%d, %s)", id, active_state());
    delay_ms(1500);
    CHECK(sim_audio_played_ms() >= played + 1000, "the ringing starts although the UI is stuck (%.0f ms)", sim_audio_played_ms() - played);
    delay_ms(2000);
    on_ui(read_screen);
    CHECK(!strcmp(g_top, "interrupt") && g_btn_n == 2, "when the UI is back it shows the card (%s, %d)", g_top, g_btn_n);
    ink_interrupt_end(id);
    delay_ms(400);
    on_ui(read_screen);
    CHECK(strcmp(g_top, "interrupt") && !strcmp(active_state(), "none"), "and it goes away");
    double after = sim_audio_played_ms();
    delay_ms(1000);
    CHECK(sim_audio_played_ms() <= after + 300, "the ringing stops (%.0f ms more)", sim_audio_played_ms() - after);
}

static void how_fast(void) {
    delay_ms(2000); /* nothing is being refreshed */
    double before = sim_metric_value("interrupt.to_visible_ms");
    ink_interrupt_t it = card(INK_INT_CALL, "来电", true);
    int id = ink_interrupt_raise(&it);
    CHECK(WAIT_FOR(sim_metric_value("interrupt.to_visible_ms") > before, 3000), "it was seen");
    double ms = sim_metric_last("interrupt.to_visible_ms");
    CHECK(ms > 0 && ms < 2000, "within two seconds of the ring (%.0f ms)", ms);
    ink_interrupt_end(id);
    delay_ms(1500);
}

static void order(void) {
    double played = sim_audio_played_ms();
    ink_interrupt_t low = card(INK_INT_LOW_BATTERY, "低电量", false);
    int lid = ink_interrupt_raise(&low);
    CHECK(!strcmp(active_state(), "low_battery"), "low battery on screen");
    delay_ms(500);
    CHECK(sim_audio_played_ms() <= played + 100, "without a ring");
    ink_interrupt_t al = card(INK_INT_ALARM, "闹钟", true);
    int aid = ink_interrupt_raise(&al);
    CHECK(!strcmp(active_state(), "alarm"), "an alarm comes first (%s)", active_state());
    ink_interrupt_t call = card(INK_INT_CALL, "来电", true);
    int cid = ink_interrupt_raise(&call);
    CHECK(!strcmp(active_state(), "call"), "a call before that (%s)", active_state());
    delay_ms(400);
    on_ui(read_screen);
    CHECK(strstr(g_snap, "来电"), "the call is what is on screen (%s)", g_snap);
    ink_interrupt_t again = card(INK_INT_ALARM, "闹钟 2", false);
    int id4 = ink_interrupt_raise(&again);
    CHECK(id4 > 0 && !strcmp(active_state(), "call"), "something less important stays behind it");
    double fulls = sim_metric_value("interrupt.table_full");
    ink_interrupt_t fifth = card(INK_INT_LOW_BATTERY, "第五个", false);
    CHECK(ink_interrupt_raise(&fifth) == -1 && sim_metric_value("interrupt.table_full") == fulls + 1, "four at a time");
    CHECK(AI("interrupt.list", NULL) && strstr(g_result, "\"kind\":\"call\"") && strstr(g_result, "\"kind\":\"alarm\"") && strstr(g_result, "\"kind\":\"low_battery\"") && strstr(g_result, "\"cap\":\"test.act\""),
          "the AI sees all of them and their buttons (%s)", g_result);
    ink_interrupt_end(id4);
    ink_interrupt_end(cid);
    CHECK(!strcmp(active_state(), "alarm"), "the call ends, the alarm is next");
    delay_ms(600);
    on_ui(read_screen);
    CHECK(strstr(g_snap, "闹钟"), "and shows (%s)", g_snap);
    played = sim_audio_played_ms();
    delay_ms(800);
    CHECK(sim_audio_played_ms() > played, "and rings");
    ink_interrupt_end(aid);
    CHECK(!strcmp(active_state(), "low_battery"), "then the battery");
    delay_ms(600);
    played = sim_audio_played_ms();
    delay_ms(800);
    CHECK(sim_audio_played_ms() <= played + 300, "which does not ring");
    CHECK(AI("interrupt.ack", NULL) && g_rc.status == INK_RC_DONE, "the AI can close that one");
    CHECK(!strcmp(active_state(), "none"), "nothing is left");
    CHECK(AI("interrupt.ack", NULL) && g_rc.status == INK_RC_FAILED, "nothing to close");
    (void)lid;
    delay_ms(1200);
}

static void modal(void) {
    on_ui(open_page);
    delay_ms(1500);
    g_page_keys = g_page_swipes = g_page_clicks = 0;
    key(HAL_KEY_NEXT);
    CHECK(g_page_keys == 1, "a key reaches the page (%d)", g_page_keys);
    CHECK(ink_cap_available("test.page_cap"), "the page's capability is there");
    g_page_keys = 0;

    ink_interrupt_t it = card(INK_INT_ALARM, "闹钟", false);
    int id = ink_interrupt_raise(&it);
    delay_ms(400);
    on_ui(read_screen);
    CHECK(!strcmp(g_top, "interrupt"), "the card is on top (%s)", g_top);
    CHECK(strstr(g_snap, "闹钟") && strstr(g_snap, "内容") && strstr(g_snap, "第一") && strstr(g_snap, "关掉"), "the AI reads the card, not the page under it (%s)", g_snap);
    CHECK(!ink_cap_available("test.page_cap"), "what works on the page does not while it is covered");
    int bx = g_btn_x[0], by = g_btn_y[0], cx = g_btn_x[1], cy = g_btn_y[1];

    key(HAL_KEY_NEXT);
    key(HAL_KEY_VOL_UP);
    key(HAL_KEY_HOME);
    on_ui(read_screen);
    CHECK(g_page_keys == 0 && !strcmp(g_top, "interrupt"), "keys do not get past it (%d, %s)", g_page_keys, g_top);
    hw(HW_PRESS, 500, 100);
    delay_ms(40);
    hw(HW_RELEASE, 80, 100);
    delay_ms(300);
    CHECK(g_page_swipes == 0 && g_page_clicks == 0, "nor swipes");
    tap(20, 20);
    CHECK(g_page_clicks == 0 && g_act_runs == 0, "and a touch off the buttons does nothing");

    tap(bx, by);
    CHECK(g_act_runs == 1 && g_act_src == INK_SRC_TOUCH, "the button runs its capability, by the person's hand (%d, %d)", g_act_runs, g_act_src);
    CHECK(!strcmp(active_state(), "alarm"), "its capability decides when this ends");
    tap(cx, cy);
    CHECK(!strcmp(active_state(), "none"), "a button with none just closes it");
    delay_ms(1200);
    on_ui(read_screen);
    CHECK(!strcmp(g_top, "testpage") && ink_cap_available("test.page_cap"), "the page is back (%s)", g_top);
    key(HAL_KEY_NEXT);
    CHECK(g_page_keys == 1, "with its keys");
    (void)id;

    /* The finger was already down when the card came, over where a button appears. */
    g_act_runs = 0;
    g_page_clicks = 0;
    hw(HW_PRESS, bx, by);
    delay_ms(60);
    it = card(INK_INT_ALARM, "闹钟", false);
    id = ink_interrupt_raise(&it);
    delay_ms(400);
    hw(HW_PRESS, bx + 4, by); /* still down, and it moves a little, as a finger does */
    delay_ms(40);
    hw(HW_RELEASE, bx + 4, by);
    delay_ms(300);
    CHECK(g_act_runs == 0, "lifting it does not press the button under it (%d)", g_act_runs);
    CHECK(g_page_clicks == 0, "nor the page that was covered meanwhile (%d)", g_page_clicks);
    tap(bx, by);
    CHECK(g_act_runs == 1, "the next touch does");

    /* The other way round: the finger on the card as it goes away, over the page. */
    g_page_clicks = 0;
    hw(HW_PRESS, 20, 20);
    delay_ms(60);
    ink_interrupt_end(id);
    delay_ms(400);
    hw(HW_PRESS, 24, 20);
    delay_ms(40);
    hw(HW_RELEASE, 24, 20);
    delay_ms(300);
    CHECK(g_page_clicks == 0, "a touch that began on the card does not press the page (%d)", g_page_clicks);
    delay_ms(1200);
    tap(20, 20);
    CHECK(g_page_clicks == 1, "the next one does");

    on_ui(close_page);
    delay_ms(1200);
}

static void locked(void) {
    ink_power_lock();
    CHECK(WAIT_FOR(!strcmp(power_state(), "locked"), 2000), "locked");
    int sleeps = sim_power_sleeps();
    CHECK(WAIT_FOR(sim_power_sleeps() > sleeps, 5000), "asleep");

    ink_interrupt_t it = card(INK_INT_LOW_BATTERY, "低电量", false);
    int id = ink_interrupt_raise(&it);
    CHECK(WAIT_FOR(strcmp(power_state(), "locked"), 500), "the screen comes on for it (%s)", power_state());
    delay_ms(500);
    on_ui(read_screen);
    CHECK(!strcmp(g_top, "interrupt"), "and shows it (%s)", g_top);
    sleeps = sim_power_sleeps();
    delay_ms(90000);
    CHECK(sim_power_sleeps() == sleeps && strcmp(power_state(), "locked"), "it stays up, the chip does not sleep");

    /* something locks it meanwhile */
    double undone = sim_metric_value("interrupt.lock_undone");
    ink_power_lock();
    CHECK(WAIT_FOR(sim_metric_value("interrupt.lock_undone") == undone + 1 && strcmp(power_state(), "locked"), 2000), "a lock does not stick while one is up (%s)", power_state());

    ink_interrupt_end(id);
    CHECK(WAIT_FOR(!strcmp(power_state(), "locked"), 2000), "when it ends it locks again (%s)", power_state());
    sleeps = sim_power_sleeps();
    CHECK(WAIT_FOR(sim_power_sleeps() > sleeps, 5000), "and the chip sleeps again");

    /* unlocked before: stays unlocked */
    CHECK(ink_power_unlock(osal_now_us()), "unlock by hand");
    delay_ms(1500);
    it = card(INK_INT_LOW_BATTERY, "低电量", false);
    id = ink_interrupt_raise(&it);
    delay_ms(300);
    ink_interrupt_end(id);
    delay_ms(1500);
    CHECK(strcmp(power_state(), "locked"), "not locked by the card going away (%s)", power_state());

    /* automatic locking waits */
    ink_receipt_t rc;
    ink_setting_set("power.lock_after_s", 60, INK_SRC_TOUCH, &rc);
    it = card(INK_INT_LOW_BATTERY, "低电量", false);
    id = ink_interrupt_raise(&it);
    delay_ms(70000);
    CHECK(strcmp(power_state(), "locked"), "no automatic lock while one is up (%s)", power_state());
    ink_interrupt_end(id);
    CHECK(WAIT_FOR(!strcmp(power_state(), "locked"), 70000), "it comes after (%s)", power_state());
    ink_setting_set("power.lock_after_s", 600, INK_SRC_TOUCH, &rc);
    ink_power_unlock(osal_now_us());
    delay_ms(1500);
}

static void power_key(void) {
    /* a ringing call: silenced, nothing else */
    ink_interrupt_t it = card(INK_INT_CALL, "来电", true);
    int id = ink_interrupt_raise(&it);
    delay_ms(600);
    key(HAL_KEY_POWER);
    double played = sim_audio_played_ms();
    delay_ms(1000);
    CHECK(sim_audio_played_ms() <= played + 300, "the power key silences a call (%.0f more)", sim_audio_played_ms() - played);
    CHECK(!strcmp(active_state(), "call") && strcmp(power_state(), "locked"), "it stays on screen, unlocked (%s, %s)", active_state(), power_state());
    key(HAL_KEY_POWER);
    CHECK(!strcmp(active_state(), "call") && strcmp(power_state(), "locked"), "again: still nothing happens to the call");
    ink_interrupt_end(id);
    delay_ms(800);

    /* the battery: closed */
    it = card(INK_INT_LOW_BATTERY, "低电量", false);
    it.nactions = 0;
    ink_interrupt_raise(&it);
    delay_ms(300);
    key(HAL_KEY_POWER);
    CHECK(!strcmp(active_state(), "none") && strcmp(power_state(), "locked"), "it closes the battery's (%s, %s)", active_state(), power_state());
    delay_ms(800);

    /* nothing up: it locks, as ever */
    key(HAL_KEY_POWER);
    CHECK(WAIT_FOR(!strcmp(power_state(), "locked"), 1000), "with nothing up it locks");
    ink_power_unlock(osal_now_us());
    delay_ms(1500);
}

/* ------------------------------------------------------------ what leads to them */

static void text_message(void) {
    snprintf(g_hw.num, sizeof g_hw.num, "10086");
    snprintf(g_hw.text, sizeof g_hw.text, "余额 88 元");
    hw(HW_SMS, 0, 0);
    ink_notify_view_t v;
    CHECK(WAIT_FOR(inbox_find("10086", &v), 30000), "a text message is a notification");
    CHECK(!strcmp(v.channel, "message") && strstr(v.body, "余额") && !strcmp(v.group, "10086") && v.prio == INK_NOTIFY_NORMAL, "from them, in their group (%s, %s)", v.channel, v.body);
    CHECK(!strcmp(active_state(), "none"), "no interruption");
    ink_notify_clear();
}

static void incoming_call(void) {
    snprintf(g_hw.num, sizeof g_hw.num, "13800000000");
    hw(HW_CALL, 0, 0);
    CHECK(WAIT_FOR(!strcmp(active_state(), "call"), 15000), "a call comes up by itself (%s)", active_state());
    delay_ms(400);
    on_ui(read_screen);
    CHECK(strstr(g_snap, "13800000000") && strstr(g_snap, "接听") && strstr(g_snap, "挂断") && g_btn_n == 2, "who, and what can be done (%s, %d)", g_snap, g_btn_n);
    double played = sim_audio_played_ms();
    delay_ms(1000);
    CHECK(sim_audio_played_ms() > played + 200, "it rings");
    hw(HW_HANGUP, 0, 0);
    CHECK(WAIT_FOR(!strcmp(active_state(), "none"), 15000), "they give up (%s)", active_state());
    ink_notify_view_t v;
    CHECK(WAIT_FOR(inbox_find("未接来电", &v), 2000) && strstr(v.body, "13800000000") && !strcmp(v.channel, "call"), "a missed call stays in the list");
    ink_notify_clear();
    delay_ms(1500);

    /* answered, by the button */
    hw(HW_CALL, 0, 0);
    CHECK(WAIT_FOR(!strcmp(active_state(), "call"), 15000), "again");
    delay_ms(400);
    on_ui(read_screen);
    tap(g_btn_x[0], g_btn_y[0]);
    delay_ms(300);
    bool shown = false;
    for (int i = 0; i < 100 && !shown; i++) {
        on_ui(read_screen);
        shown = strstr(g_snap, "通话中") != NULL;
        if (!shown) delay_ms(100);
    }
    CHECK(shown && g_btn_n == 1 && strstr(g_snap, "挂断"), "answered: in a call, only hanging up left (%s, %d)", g_snap, g_btn_n);
    played = sim_audio_played_ms();
    delay_ms(1000);
    CHECK(sim_audio_played_ms() <= played + 300, "no more ringing (%.0f more)", sim_audio_played_ms() - played);
    hw(HW_HANGUP, 0, 0);
    CHECK(WAIT_FOR(!strcmp(active_state(), "none"), 15000), "the far end hangs up (%s)", active_state());
    delay_ms(500);
    CHECK(inbox_count() == 0, "a call that was answered is not missed (%d)", inbox_count());
    delay_ms(1500);

    /* do not disturb: on screen without a sound */
    ink_receipt_t rc;
    ink_setting_set("notify.dnd", 1, INK_SRC_TOUCH, &rc);
    hw(HW_CALL, 0, 0);
    CHECK(WAIT_FOR(!strcmp(active_state(), "call"), 15000), "with do not disturb it still shows");
    played = sim_audio_played_ms();
    delay_ms(1000);
    CHECK(sim_audio_played_ms() <= played + 100, "but does not ring (%.0f more)", sim_audio_played_ms() - played);
    hw(HW_HANGUP, 0, 0);
    CHECK(WAIT_FOR(!strcmp(active_state(), "none"), 15000), "gone");
    ink_setting_set("notify.dnd", 0, INK_SRC_TOUCH, &rc);
    delay_ms(500);
    ink_notify_clear();
    delay_ms(1500);
}

static void an_alarm(void) {
    /* rings, the button stops it */
    ink_alarm_t a = alarm_in(INK_ALARM, 1, "起床");
    int id = ink_alarm_add(&a);
    CHECK(id > 0, "an alarm");
    CHECK(WAIT_FOR(!strcmp(active_state(), "alarm"), 130000), "it goes off by itself (%s)", active_state());
    delay_ms(600);
    on_ui(read_screen);
    CHECK(strstr(g_snap, "起床") && strstr(g_snap, "关闭") && strstr(g_snap, "稍后") && g_btn_n == 2, "what it is, and the buttons (%s)", g_snap);
    double played = sim_audio_played_ms();
    delay_ms(1000);
    CHECK(sim_audio_played_ms() > played + 200, "it rings");
    tap(g_btn_x[0], g_btn_y[0]);
    CHECK(WAIT_FOR(!strcmp(active_state(), "none"), 2000), "the button stops it (%s)", active_state());
    delay_ms(300);
    played = sim_audio_played_ms();
    delay_ms(1000);
    CHECK(sim_audio_played_ms() <= played + 200, "the sound too");
    CHECK(inbox_count() == 0, "nothing was missed");
    ink_alarm_cancel((uint32_t)id);
    delay_ms(1500);

    /* snoozed by the second button; do not disturb does not silence it */
    ink_receipt_t rc;
    ink_setting_set("notify.dnd", 1, INK_SRC_TOUCH, &rc);
    a = alarm_in(INK_ALARM, 1, "再睡会");
    id = ink_alarm_add(&a);
    CHECK(WAIT_FOR(!strcmp(active_state(), "alarm"), 130000), "the next one (%s)", active_state());
    delay_ms(600);
    played = sim_audio_played_ms();
    delay_ms(1000);
    CHECK(sim_audio_played_ms() > played + 200, "it rings with do not disturb on");
    ink_setting_set("notify.dnd", 0, INK_SRC_TOUCH, &rc);
    on_ui(read_screen);
    tap(g_btn_x[1], g_btn_y[1]);
    CHECK(WAIT_FOR(!strcmp(active_state(), "none"), 2000), "snoozed (%s)", active_state());
    delay_ms(1500);
    CHECK(WAIT_FOR(!strcmp(active_state(), "alarm"), 10 * 60000), "and back after the snooze (%s)", active_state());
    delay_ms(300);
    /* the power key snoozes it too */
    key(HAL_KEY_POWER);
    CHECK(WAIT_FOR(!strcmp(active_state(), "none"), 2000) && strcmp(power_state(), "locked"), "the power key snoozes (%s, %s)", active_state(), power_state());
    ink_alarm_cancel((uint32_t)id);
    delay_ms(1500);

    /* nobody comes */
    a = alarm_in(INK_ALARM, 1, "没人管");
    id = ink_alarm_add(&a);
    CHECK(WAIT_FOR(!strcmp(active_state(), "alarm"), 130000), "again (%s)", active_state());
    CHECK(WAIT_FOR(!strcmp(active_state(), "none"), 130000), "it stops by itself (%s)", active_state());
    ink_notify_view_t v;
    CHECK(WAIT_FOR(inbox_find("错过的闹钟", &v), 2000) && strstr(v.body, "没人管"), "and leaves a note");
    ink_notify_clear();
    ink_alarm_cancel((uint32_t)id);
    delay_ms(1500);

    /* asleep: the chip wakes for it, the screen comes on, it rings, afterwards it sleeps again */
    a = alarm_in(INK_ALARM, 2, "在睡觉");
    id = ink_alarm_add(&a);
    ink_power_lock();
    CHECK(WAIT_FOR(!strcmp(power_state(), "locked"), 2000), "locked");
    int sleeps = sim_power_sleeps();
    CHECK(WAIT_FOR(sim_power_sleeps() > sleeps, 5000), "asleep");
    CHECK(WAIT_FOR(!strcmp(active_state(), "alarm"), 200000), "it goes off (%s)", active_state());
    CHECK(WAIT_FOR(strcmp(power_state(), "locked"), 500), "the screen is on (%s)", power_state());
    delay_ms(1000);
    on_ui(read_screen);
    CHECK(!strcmp(g_top, "interrupt") && g_btn_n == 2, "with the card (%s)", g_top);
    played = sim_audio_played_ms();
    delay_ms(1000);
    CHECK(sim_audio_played_ms() > played + 200, "and the sound");
    tap(g_btn_x[0], g_btn_y[0]);
    CHECK(WAIT_FOR(!strcmp(power_state(), "locked"), 3000), "stopped: it locks again (%s)", power_state());
    sleeps = sim_power_sleeps();
    CHECK(WAIT_FOR(sim_power_sleeps() > sleeps, 5000), "and sleeps");
    ink_power_unlock(osal_now_us());
    ink_alarm_cancel((uint32_t)id);
    delay_ms(1500);

    /* a reminder is a notification with a sound */
    int alerted = (int)sim_metric_value("notify.alerted");
    a = alarm_in(INK_REMINDER, 1, "吃药");
    id = ink_alarm_add(&a);
    CHECK(WAIT_FOR(inbox_find("提醒", &v), 130000) && strstr(v.body, "吃药") && !strcmp(v.channel, "reminder"), "a reminder arrives");
    CHECK((int)sim_metric_value("notify.alerted") == alerted + 1, "with a sound");
    CHECK(!strcmp(active_state(), "none"), "without an interruption");
    ink_notify_clear();
    ink_alarm_cancel((uint32_t)id);
}

static void other_events(void) {
    ink_cap_emit("data.write_failed", NULL);
    ink_notify_view_t v;
    CHECK(WAIT_FOR(inbox_find("有内容还没保存好", &v), 2000) && !strcmp(v.channel, "system"), "a failed write is told");
    ink_cap_emit("data.write_failed", NULL);
    delay_ms(300);
    CHECK(inbox_count() == 1, "once, not again and again (%d)", inbox_count());
    ink_notify_clear();

    hw(HW_USB_ON, 0, 0);
    CHECK(WAIT_FOR(inbox_find("连接了电脑", &v), 3000) && !strcmp(v.action, "usb.mount"), "a computer: offered as a disk, by a notification");
    hw(HW_USB_OFF, 0, 0);
    CHECK(WAIT_FOR(inbox_count() == 0, 3000), "cable gone, offer gone (%d)", inbox_count());
}

static bool battery_low_state(void) {
    int64_t b = 0;
    ink_state_get_int(ink_state_find("battery.low"), &b);
    return b != 0;
}

static void battery(void) {
    ink_notify_view_t v;
    CHECK(!battery_low_state(), "the battery is fine");
    hw(HW_BATTERY, 14, 0);
    CHECK(WAIT_FOR(inbox_find("电量低", &v), 70000) && strstr(v.body, "14%"), "15%%: a notification (%s)", v.body);
    CHECK(battery_low_state() && !strcmp(active_state(), "none"), "state, no interruption");
    hw(HW_BATTERY, 13, 0);
    delay_ms(70000);
    CHECK(inbox_count() == 1, "not told again on the way down (%d)", inbox_count());
    /* the flash work that can wait does */
    static const ink_db_def_t HEAP = {.name = "test.heap", .scope = INK_DB_DEVICE, .schema = 1};
    ink_db_register(&HEAP);
    ink_db_t *heap = ink_db_open(&HEAP, NULL);
    char filler[400];
    memset(filler, 'x', sizeof filler);
    int foldings = (int)sim_metric_value("data.compactions");
    for (int i = 0; i < 120; i++) {
        ink_db_write_t w = {.id = 1, .data = filler, .len = sizeof filler};
        ink_db_put(heap, &w, NULL);
        delay_ms(20);
    }
    delay_ms(45000);
    CHECK((int)sim_metric_value("data.compactions") == foldings, "a low battery does not fold the log (%d)", (int)sim_metric_value("data.compactions") - foldings);
    hw(HW_BATTERY, 5, 0);
    CHECK(WAIT_FOR(!strcmp(active_state(), "low_battery"), 70000), "5%%: an interruption (%s)", active_state());
    delay_ms(500);
    on_ui(read_screen);
    CHECK(strstr(g_snap, "5%") && strstr(g_snap, "知道了") && g_btn_n == 1, "it says so (%s)", g_snap);
    hw(HW_BATTERY_CHARGING, 5, 0);
    CHECK(WAIT_FOR(!strcmp(active_state(), "none"), 70000), "charging: it goes away (%s)", active_state());
    CHECK(WAIT_FOR(inbox_count() == 0, 2000) && !battery_low_state(), "with the notification (%d)", inbox_count());
    CHECK(WAIT_FOR((int)sim_metric_value("data.compactions") > foldings, 60000), "the folding comes after");
    ink_db_close(heap);
    delay_ms(1500);

    hw(HW_BATTERY, 5, 0);
    CHECK(WAIT_FOR(!strcmp(active_state(), "low_battery"), 70000), "off the charger again: told again");
    delay_ms(600);
    on_ui(read_screen);
    tap(g_btn_x[0], g_btn_y[0]);
    CHECK(!strcmp(active_state(), "none"), "acknowledged");
    delay_ms(70000);
    CHECK(!strcmp(active_state(), "none"), "and it does not come back at the same level");
    hw(HW_BATTERY_CHARGING, 80, 0);
    CHECK(WAIT_FOR(!battery_low_state(), 70000), "charged");
    ink_notify_clear();
}

/* Locked and kept awake for hours by something in the background: the battery is still looked at (audit AUD15-011). */
static void battery_while_locked(void) {
    ink_power_hold(INK_NEED_AWAKE);
    ink_power_lock();
    delay_ms(1000);
    CHECK(!strcmp(power_state(), "locked") && !battery_low_state(), "locked, not sleeping, the battery fine");
    hw(HW_BATTERY, 13, 0);
    CHECK(WAIT_FOR(battery_low_state(), 70000), "it notices");
    int64_t level = 0;
    CHECK(ink_state_get_int(ink_state_find("battery.level"), &level) && level == 13, "and says how much (%lld)", (long long)level);
    ink_power_release(INK_NEED_AWAKE);
    ink_power_unlock(osal_now_us());
    hw(HW_BATTERY_CHARGING, 80, 0);
    CHECK(WAIT_FOR(!battery_low_state(), 70000), "charged");
    ink_notify_clear();
}

/* ------------------------------------------------------------ a nearly empty battery */

static void lowbat1(void) {
    ink_notify_t n = {.title = "关机前的", .prio = INK_NOTIFY_LOW};
    CHECK(ink_notify_post(&n, NULL) == 0, "something is waiting to be written");
    /* the script takes the battery down; power off ends this run */
    delay_ms(600000);
    CHECK(0, "it should have powered off");
}

static void lowbat2(void) {
    ink_notify_view_t v;
    CHECK(inbox_find("关机前的", &v), "what was waiting is there after the next boot");
    CHECK(unread_state() >= 1, "and counted (%d)", unread_state());
}

static void test_task(void *arg) {
    (void)arg;
    delay_ms(1500);
    const char *phase = getenv("INK_NOTIFY_PHASE");
    if (phase && !strcmp(phase, "lowbat1")) lowbat1();
    else if (phase && !strcmp(phase, "lowbat2")) lowbat2();
    else {
        CHECK(ink_cap_register(TEST_CAPS, 2) == 0, "register");
        inbox();
        channels();
        members();
        capabilities();
        not_waiting_for_the_ui();
        how_fast();
        order();
        modal();
        locked();
        power_key();
        text_message();
        incoming_call();
        an_alarm();
        other_events();
        battery();
        battery_while_locked();
    }
    ink_probe_count("test.finished");
}

void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name = "notifytest", .fn = test_task, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    if (osal_task_create(&cfg, NULL) != OSAL_OK) osal_panic("test task");
}
