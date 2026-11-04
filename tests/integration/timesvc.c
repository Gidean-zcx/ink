/*
 * The time service (system services design §5). INK_TIME_PHASE picks the run: main (zones, alarms, sleep,
 * the clock moving, capabilities), rtc_lost (the RTC has no time until the hub says), line_low (the RTC's
 * interrupt does not work: alarms are on time anyway), reboot1 and reboot2 (alarms across a power-off).
 * Results are the probe counters "test.pass" / "test.fail", as in systest.c.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ink/hal_device.h"
#include "ink/osal.h"
#include "ink/probe.h"
#include "ink/sys/cap.h"
#include "ink/sys/data.h"
#include "ink/sys/display.h"
#include "ink/sys/exec.h"
#include "ink/sys/hub.h"
#include "ink/sys/members.h"
#include "ink/sys/net.h"
#include "ink/sys/power.h"
#include "ink/sys/res.h"
#include "ink/sys/settings.h"
#include "ink/sys/state.h"
#include "ink/sys/storage.h"
#include "ink/sys/svc.h"
#include "ink/sys/time.h"
#include "sim.h"

#define CHECK(cond, ...)                                                 \
    do {                                                                 \
        if (cond) {                                                      \
            ink_probe_count("test.pass");                                \
        } else {                                                         \
            char _m[300];                                                \
            snprintf(_m, sizeof _m, __VA_ARGS__);                        \
            sim_warn("CHECK FAILED (%s:%d) %s", __FILE__, __LINE__, _m); \
            ink_probe_count("test.fail");                                \
        }                                                                \
    } while (0)

/* What happened, as the events say it. */
static volatile int g_fired, g_ended, g_missed, g_rem_fired, g_rem_missed, g_changed;
static uint32_t g_last_id;
static char g_last_why[32], g_last_title[48];
static int64_t g_fired_wall_us;

static void on_time_event(void *ctx, const char *id, const ink_args_t *a) {
    (void)ctx;
    int64_t v = 0;
    if (a) ink_args_get_int(a, "id", &v);
    g_last_id = (uint32_t)v;
    g_last_why[0] = 0;
    if (a) {
        ink_args_get_str(a, "why", g_last_why, sizeof g_last_why);
        ink_args_get_str(a, "title", g_last_title, sizeof g_last_title);
    }
    if (!strcmp(id, "alarm.fired")) {
        g_fired_wall_us = osal_wall_us();
        g_fired++;
    } else if (!strcmp(id, "alarm.ended")) g_ended++;
    else if (!strcmp(id, "alarm.missed")) g_missed++;
    else if (!strcmp(id, "reminder.fired")) {
        g_fired_wall_us = osal_wall_us();
        g_rem_fired++;
    } else if (!strcmp(id, "reminder.missed")) g_rem_missed++;
    else if (!strcmp(id, "time.changed")) g_changed++;
}

static bool wait_for(volatile int *v, int want, uint64_t max_s) {
    for (uint64_t i = 0; i < max_s * 20 && *v < want; i++) osal_task_delay_us(50000);
    return *v >= want;
}

/* Local time in `min` minutes, rounded to a minute. */
static ink_alarm_t alarm_in(int min, const char *title) {
    ink_tm_t t;
    ink_time_to_local(ink_time_now_s() + min * 60 + 59, &t);
    ink_alarm_t a = {.kind = INK_ALARM, .hour = (uint8_t)t.hour, .min = (uint8_t)t.min};
    snprintf(a.title, sizeof a.title, "%s", title);
    return a;
}

static bool state_int(const char *id, int64_t *v) { return ink_state_get_int(ink_state_find(id), v); }

static void set_zone(const char *name) {
    ink_receipt_t rc;
    static const char *const Z[] = {"Asia/Shanghai", "UTC", "Asia/Tokyo", "Asia/Hong_Kong", "Asia/Singapore", "Europe/London", "Europe/Paris", "America/New_York", "America/Los_Angeles"};
    for (int i = 0; i < 9; i++)
        if (!strcmp(Z[i], name)) ink_setting_set("time.zone", i, INK_SRC_TOUCH, &rc);
}

/* 2026-11-02 00:00:00 UTC, as sim_main boots */
#define BOOT_S 1793577600ll

static void zones(void) {
    char z[32];
    CHECK(ink_time_valid() && ink_time_now_s() >= BOOT_S && ink_time_now_s() < BOOT_S + 60, "the clock comes from the RTC (%lld)", (long long)ink_time_now_s());
    CHECK(ink_state_get_str(ink_state_find("time.zone"), z, sizeof z) > 0 && !strcmp(z, "Asia/Shanghai"), "China by default (%s)", z);
    ink_tm_t t;
    ink_time_to_local(BOOT_S, &t);
    CHECK(t.year == 2026 && t.month == 11 && t.day == 2 && t.hour == 8 && t.min == 0 && t.wday == 1 && t.offset_min == 480 && !t.dst, "local time %d-%d-%d %d:%d wday %d", t.year, t.month, t.day, t.hour, t.min, t.wday);
    int64_t v = 0;
    CHECK(state_int("time.minute", &v) && v == 8 * 60, "the minute of the day is state (%lld)", (long long)v);
    int64_t back = ink_time_from_local(&t);
    CHECK(back == BOOT_S, "and back (%lld)", (long long)back);

    set_zone("America/New_York");
    CHECK(ink_state_get_str(ink_state_find("time.zone"), z, sizeof z) > 0 && !strcmp(z, "America/New_York"), "the zone is a setting and state");
    ink_time_to_local(BOOT_S, &t);
    CHECK(t.day == 1 && t.hour == 19 && t.offset_min == -300 && !t.dst, "after the autumn change: EST (%d %d:%d, %d)", t.day, t.hour, t.min, t.offset_min);
    ink_tm_t july = {.year = 2026, .month = 7, .day = 1, .hour = 12};
    int64_t js = ink_time_from_local(&july);
    ink_time_to_local(js, &t);
    CHECK(t.hour == 12 && t.offset_min == -240 && t.dst, "in July: EDT (%d, %d)", t.hour, t.offset_min);
    /* 02:30 does not exist on 2026-03-08; 01:30 happens twice on 2026-11-01 */
    ink_tm_t gap = {.year = 2026, .month = 3, .day = 8, .hour = 2, .min = 30};
    ink_time_to_local(ink_time_from_local(&gap), &t);
    CHECK(t.hour == 3 && t.min == 30 && t.dst, "a time the clocks skip moves on by the skip (%d:%d)", t.hour, t.min);
    ink_tm_t twice = {.year = 2026, .month = 11, .day = 1, .hour = 1, .min = 30};
    int64_t first = ink_time_from_local(&twice);
    ink_time_to_local(first, &t);
    CHECK(t.hour == 1 && t.min == 30 && t.dst, "one that happens twice is the first (dst %d)", t.dst);
    ink_time_to_local(first + 3600, &t);
    CHECK(t.hour == 1 && t.min == 30 && !t.dst, "an hour later it is that time again");
    set_zone("Europe/Paris");
    ink_tm_t paris = {.year = 2026, .month = 7, .day = 1, .hour = 12};
    ink_time_to_local(ink_time_from_local(&paris), &t);
    CHECK(t.offset_min == 120 && t.dst, "Paris in July (%d)", t.offset_min);
    ink_tm_t winter = {.year = 2026, .month = 12, .day = 1, .hour = 12};
    ink_time_to_local(ink_time_from_local(&winter), &t);
    CHECK(t.offset_min == 60 && !t.dst, "and in December (%d)", t.offset_min);
    set_zone("Asia/Shanghai");
}

static void rules(void) {
    /* a daily-looking repeat: Monday to Friday. Boot is Monday 08:00 local. */
    ink_alarm_t a = {.kind = INK_ALARM, .repeat = INK_REPEAT_WEEKDAYS, .weekdays = 0x3E, .hour = 7, .min = 30};
    snprintf(a.title, sizeof a.title, "workdays");
    int id = ink_alarm_add(&a);
    ink_tm_t t;
    ink_time_to_local(a.next_s, &t);
    CHECK(id > 0 && t.day == 3 && t.hour == 7 && t.min == 30, "07:30 has passed today: tomorrow (%d %d:%d)", t.day, t.hour, t.min);
    CHECK(ink_alarm_cancel((uint32_t)id) == 0 && ink_alarm_cancel((uint32_t)id) != 0, "cancelled once");
    ink_alarm_t sat = {.kind = INK_ALARM, .repeat = INK_REPEAT_WEEKDAYS, .weekdays = 1u << 6, .hour = 9};
    id = ink_alarm_add(&sat);
    ink_time_to_local(sat.next_s, &t);
    CHECK(id > 0 && t.day == 7 && t.wday == 6, "Saturdays: the 7th (%d)", t.day);
    ink_alarm_cancel((uint32_t)id);
    ink_alarm_t bad = {.kind = INK_ALARM, .hour = 25};
    CHECK(ink_alarm_add(&bad) < 0, "not a time");
    ink_alarm_t none = {.kind = INK_ALARM, .repeat = INK_REPEAT_WEEKDAYS, .hour = 9};
    CHECK(ink_alarm_add(&none) < 0, "no day chosen");

    /* a day some months do not have */
    ink_alarm_t mon = {.kind = INK_REMINDER, .repeat = INK_REPEAT_MONTHLY, .day = 31, .hour = 9};
    id = ink_alarm_add(&mon);
    ink_time_to_local(mon.next_s, &t);
    CHECK(id > 0 && t.month == 12 && t.day == 31, "the 31st: the next month that has one (%d-%d)", t.month, t.day);
    ink_alarm_cancel((uint32_t)id);
    ink_alarm_t feb = {.kind = INK_REMINDER, .repeat = INK_REPEAT_YEARLY, .month = 2, .day = 29, .hour = 9};
    id = ink_alarm_add(&feb);
    ink_time_to_local(feb.next_s, &t);
    CHECK(id > 0 && t.year == 2028 && t.month == 2 && t.day == 29, "February 29: 2028 (%d)", t.year);
    ink_alarm_cancel((uint32_t)id);
    /* lunar: new year's day, next is 2027-02-06 */
    ink_alarm_t ny = {.kind = INK_REMINDER, .repeat = INK_REPEAT_LUNAR_YEARLY, .lmonth = 1, .lday = 1, .hour = 9};
    id = ink_alarm_add(&ny);
    ink_time_to_local(ny.next_s, &t);
    CHECK(id > 0 && t.year == 2027 && t.month == 2 && t.day == 6, "a lunar birthday (%d-%d-%d)", t.year, t.month, t.day);
    ink_alarm_cancel((uint32_t)id);
    /* the 30th of a lunar month that has only 29 days this year: its last day. 2027's twelfth month has 30, the eleventh 29 */
    ink_alarm_t last = {.kind = INK_REMINDER, .repeat = INK_REPEAT_LUNAR_YEARLY, .lmonth = 2, .lday = 30, .hour = 9};
    id = ink_alarm_add(&last);
    int want = ink_lunar_month_days(2027, 2, false);
    ink_lunar_t got;
    ink_time_to_local(last.next_s, &t);
    CHECK(id > 0 && ink_lunar_from_solar(t.year, t.month, t.day, &got) && got.month == 2 && got.day == want, "the 30th where there are %d days (%d-%d)", want, got.month, got.day);
    ink_alarm_cancel((uint32_t)id);
    /* a date that is over and does not repeat */
    ink_alarm_t over = {.kind = INK_REMINDER, .year = 2026, .month = 1, .day = 1, .hour = 9};
    CHECK(ink_alarm_add(&over) == -2, "a date that has passed is refused");
}

static void awake(void) {
    /* rings at its minute */
    ink_alarm_t a = alarm_in(1, "soon");
    int id = ink_alarm_add(&a);
    int due = (int)(a.next_s);
    CHECK(id > 0 && g_fired == 0, "added (%d)", id);
    CHECK(wait_for(&g_fired, 1, 130), "it goes off");
    int64_t late = g_fired_wall_us - (int64_t)due * 1000000;
    CHECK(late >= 0 && late < 200000, "on time (%lld us late)", (long long)late);
    int64_t v = 0;
    CHECK(state_int("alarm.ringing", &v) && v == 1 && g_last_id == (uint32_t)id && !strcmp(g_last_title, "soon"), "state says it rings");
    CHECK(ink_alarm_dismiss((uint32_t)id) == 0 && g_ended == 1 && !strcmp(g_last_why, "dismissed"), "dismissed");
    CHECK(state_int("alarm.ringing", &v) && v == 0, "not ringing any more");
    CHECK(ink_alarm_dismiss(0) != 0, "nothing else rings");
    int cursor = 0;
    ink_alarm_t got = {0}, it;
    bool found = false;
    while (ink_alarm_next(&cursor, INK_ALARM, &it) == 1)
        if (it.id == (uint32_t)id) {
            got = it;
            found = true;
        }
    CHECK(found && !got.enabled && !got.next_s, "a single alarm stays in the list, off");
    ink_alarm_cancel((uint32_t)id);

    /* snooze: rings again */
    a = alarm_in(1, "snooze");
    id = ink_alarm_add(&a);
    CHECK(wait_for(&g_fired, 2, 130), "the next goes off");
    uint64_t t0 = osal_now_us();
    CHECK(ink_alarm_snooze(0, 1) == 0 && !strcmp(g_last_why, "snoozed"), "snooze");
    CHECK(wait_for(&g_fired, 3, 70), "it rings again");
    uint64_t waited = (osal_now_us() - t0) / 1000000;
    CHECK(waited >= 59 && waited <= 61, "after the minute asked for (%llu s)", (unsigned long long)waited);
    /* nobody comes: it stops by itself */
    int missed0 = g_missed;
    uint64_t t1 = osal_now_us();
    CHECK(wait_for(&g_missed, missed0 + 1, 130), "nobody came: it stops and counts as missed");
    CHECK((osal_now_us() - t1) / 1000000 >= 119, "after two minutes");
    CHECK(state_int("alarm.ringing", &v) && v == 0, "not ringing");
    ink_alarm_cancel((uint32_t)id);

    /* a reminder goes off and is over */
    ink_alarm_t r = {.kind = INK_REMINDER, .at_utc = ink_time_now_s() + 20};
    snprintf(r.title, sizeof r.title, "reminder");
    id = ink_alarm_add(&r);
    CHECK(id > 0 && wait_for(&g_rem_fired, 1, 25) && g_last_id == (uint32_t)id, "a reminder fires");
    cursor = 0;
    CHECK(ink_alarm_next(&cursor, INK_REMINDER, &got) == 0, "a single reminder is over and gone");

    /* whose: a child's alarm is not in the adult's list, and rings all the same */
    CHECK(ink_member_switch("kid") == 0, "the child");
    a = alarm_in(1, "kid's");
    id = ink_alarm_add(&a);
    CHECK(id > 0 && !strcmp(a.owner, "kid"), "belongs to the child");
    ink_member_test_reset();
    cursor = 0;
    bool seen = false;
    while (ink_alarm_next(&cursor, INK_ALARM, &got) == 1) seen = seen || got.id == (uint32_t)id;
    CHECK(!seen, "not listed for the adult");
    int f0 = g_fired;
    CHECK(wait_for(&g_fired, f0 + 1, 130), "it rings whoever is using the device");
    ink_alarm_dismiss(0);
    ink_alarm_cancel((uint32_t)id);
}

/* Asleep: the RTC's alarm (a minute's resolution, rounded down) wakes the chip, and the exact time holds. */
static void asleep(void) {
    int f0 = g_rem_fired;
    int64_t now = ink_time_now_s();
    int64_t minute = (now / 60 + 3) * 60;
    ink_alarm_t r = {.kind = INK_REMINDER, .at_utc = minute + 30}; /* the RTC can only say the minute */
    snprintf(r.title, sizeof r.title, "asleep");
    int id = ink_alarm_add(&r);
    CHECK(id > 0, "a reminder 3 minutes ahead");
    osal_task_delay_us(1000000);
    double woke0 = sim_metric_value("power.wake.timer") + sim_metric_value("power.wake.other");
    ink_power_lock();
    CHECK(wait_for(&g_rem_fired, f0 + 1, 240), "it goes off while locked");
    CHECK(sim_metric_value("power.wake.timer") + sim_metric_value("power.wake.other") > woke0, "the chip slept meanwhile, and woke for it");
    int64_t late = g_fired_wall_us - (minute + 30) * 1000000;
    CHECK(late >= 0 && late < 700000, "on time (%lld ms late)", (long long)late / 1000);
    CHECK(sim_rtc_alarms() >= 1, "the RTC's alarm came (%u)", sim_rtc_alarms());
    ink_power_unlock(osal_now_us());
    osal_task_delay_us(500000);
    int64_t v = 0;
    CHECK(state_int("time.minute", &v) && v >= 0, "the minute is fresh after the unlock");
}

static void moved_clock(void) {
    /* forward past an alarm: not rung late, reported */
    ink_alarm_t a = alarm_in(30, "skipped");
    int id = ink_alarm_add(&a);
    int ch0 = g_changed, m0 = g_missed, f0 = g_fired;
    int64_t now = ink_time_now_s();
    ink_time_set(now + 3600, false);
    CHECK(g_changed == ch0 + 1, "the change is told");
    int64_t rtc = 0;
    CHECK(hal_rtc_get(&rtc) && rtc >= now + 3600 && rtc < now + 3610, "the RTC follows (%lld)", (long long)(rtc - now));
    CHECK(wait_for(&g_missed, m0 + 1, 5) && g_fired == f0 && !strcmp(g_last_title, "skipped"), "an alarm the clock jumped over is reported missed, not rung");
    ink_alarm_cancel((uint32_t)id);

    /* a repeating one moves with the zone; a moment in UTC does not */
    ink_alarm_t daily = {.kind = INK_ALARM, .repeat = INK_REPEAT_WEEKDAYS, .weekdays = 0x7F, .hour = 23, .min = 0};
    ink_alarm_t moment = {.kind = INK_REMINDER, .at_utc = ink_time_now_s() + 86400 * 2};
    int d = ink_alarm_add(&daily), m = ink_alarm_add(&moment);
    int64_t before = daily.next_s;
    set_zone("Asia/Tokyo");
    int cursor = 0;
    ink_alarm_t got;
    int64_t after_daily = 0, after_moment = 0;
    while (ink_alarm_next(&cursor, INK_ALARM, &got) == 1)
        if (got.id == (uint32_t)d) after_daily = got.next_s;
    cursor = 0;
    while (ink_alarm_next(&cursor, INK_REMINDER, &got) == 1)
        if (got.id == (uint32_t)m) after_moment = got.next_s;
    CHECK(after_daily == before - 3600 || after_daily == before - 3600 + 86400, "23:00 stays 23:00 in the new zone (%lld)", (long long)(after_daily - before));
    CHECK(after_moment == moment.at_utc, "a moment stays that moment");
    set_zone("Asia/Shanghai");
    ink_alarm_cancel((uint32_t)d);
    ink_alarm_cancel((uint32_t)m);
    /* back: nothing fires twice, the schedule is right */
    int64_t n2 = ink_time_now_s();
    ink_alarm_t ahead = alarm_in(10, "ahead");
    int aid = ink_alarm_add(&ahead);
    ink_time_set(n2 - 3600, false);
    cursor = 0;
    int64_t next = 0;
    while (ink_alarm_next(&cursor, INK_ALARM, &got) == 1)
        if (got.id == (uint32_t)aid) next = got.next_s;
    CHECK(next > ink_time_now_s() && next <= ink_time_now_s() + 86400, "after going back the next time is still ahead");
    ink_alarm_cancel((uint32_t)aid);
    ink_time_set(n2, false);
}

/* The hub's clock: the script sets it 100 s off, the connection tells. */
static void hub_time(void) {
    int64_t synced = 1;
    CHECK(state_int("time.synced", &synced) && synced == 0, "not checked against the hub yet");
    int64_t before = ink_time_now_s();
    ink_receipt_t rc;
    ink_hub_demand();
    for (int i = 0; i < 400 && !(state_int("time.synced", &synced) && synced); i++) osal_task_delay_us(50000);
    int64_t after = ink_time_now_s();
    CHECK(synced == 1, "connected: checked");
    CHECK(after - before >= 100 && after - before < 130, "and 100 s off: moved (%lld)", (long long)(after - before));
    int64_t rtc = 0;
    CHECK(hal_rtc_get(&rtc) && llabs(rtc - after) <= 1, "the RTC too");
    CHECK(ink_setting_set("time.auto", 0, INK_SRC_TOUCH, &rc) == INK_RC_DONE, "automatic off");
}

static volatile int g_done;
static ink_receipt_t g_rc;
static char g_result[1024];

static void done_cb(void *ctx, const ink_receipt_t *rc) {
    (void)ctx;
    g_rc = *rc;
    g_result[0] = 0;
    if (rc->result) snprintf(g_result, sizeof g_result, "%s", rc->result);
    g_rc.result = NULL;
    g_done++;
}

static bool call_as(const char *id, const char *args, ink_source_t src) {
    int before = g_done;
    ink_call_opts_t o = {.src = src, .args_json = args, .reply_exec = INK_EXEC_DIRECT, .done = done_cb};
    if (ink_cap_call(id, NULL, &o) != 0) return false;
    for (int i = 0; i < 300 && g_done == before; i++) osal_task_delay_us(10000);
    return g_done > before;
}
#define AI(id, args) call_as(id, args, INK_SRC_AI)

static void capabilities(void) {
    CHECK(AI("time.now", NULL) && g_rc.status == INK_RC_DONE && strstr(g_result, "\"zone\":\"Asia/Shanghai\"") && strstr(g_result, "\"lunar\":") && strstr(g_result, "\"ganzhi\":\"丙午\"") && strstr(g_rc.say, "现在是"),
          "what time it is, for the AI (%s)", g_result);
    CHECK(AI("time.lunar", "{\"year\":2026,\"month\":11,\"day\":4}") && strstr(g_result, "\"name\":\"九月廿六\"") && strstr(g_result, "\"zodiac\":\"马\""), "the lunar date (%s)", g_result);
    CHECK(AI("time.lunar", "{\"lunar_year\":2023,\"lunar_month\":2,\"lunar_day\":1,\"leap\":true}") && strstr(g_result, "\"month\":3") && strstr(g_result, "\"day\":22"), "and back (%s)", g_result);
    CHECK(AI("time.lunar", "{\"lunar_year\":2026,\"lunar_month\":2,\"lunar_day\":30}") && g_rc.status == INK_RC_INVALID, "a date that does not exist");
    CHECK(AI("alarm.set", "{\"hour\":7,\"minute\":15,\"weekdays\":62,\"title\":\"起床\"}") && g_rc.status == INK_RC_DONE && g_rc.undo_ms > 0 && !strncmp(g_rc.object, "alarm:", 6), "the AI sets an alarm, it can be undone (%s %s)", ink_rc_name(g_rc.status), g_rc.detail);
    char object[48], args[80];
    snprintf(object, sizeof object, "%s", g_rc.object);
    CHECK(AI("alarm.list", NULL) && strstr(g_result, "\"title\":\"起床\"") && strstr(g_result, "\"next\":\"2026-"), "it is listed (%s)", g_result);
    int cursor = 0;
    ink_alarm_t got = {0};
    while (ink_alarm_next(&cursor, INK_ALARM, &got) == 1) {}
    CHECK(got.src == INK_SRC_AI, "and it says who set it (%d)", got.src);
    snprintf(args, sizeof args, "{\"object\":\"%s\"}", object);
    CHECK(AI("system.undo", args) && g_rc.status == INK_RC_UNDONE, "undone (%s)", g_rc.detail);
    CHECK(AI("alarm.list", NULL) && !strstr(g_result, "起床"), "gone");
    CHECK(AI("alarm.set", "{\"hour\":24,\"minute\":0}") && g_rc.status == INK_RC_INVALID, "a bad hour is refused by the schema");
    CHECK(AI("reminder.add", "{\"at\":\"2030-05-01 09:30\",\"title\":\"生日\",\"repeat\":\"lunar_yearly\"}") && g_rc.status == INK_RC_DONE, "a reminder (%s)", g_rc.detail);
    CHECK(AI("reminder.list", NULL) && strstr(g_result, "生日"), "listed (%s)", g_result);
    uint32_t rid = (uint32_t)atoi(strchr(g_result, ':') + 1);
    snprintf(args, sizeof args, "{\"id\":%u}", rid);
    CHECK(AI("reminder.cancel", args) && g_rc.status == INK_RC_DONE, "cancelled");
    CHECK(AI("reminder.add", "{\"at\":\"tomorrow\",\"title\":\"x\"}") && g_rc.status == INK_RC_INVALID && strstr(g_rc.detail, "YYYY"), "a time it cannot read is said so");
    CHECK(AI("time.set", "{\"utc_s\":1800000000}") && g_rc.status == INK_RC_PENDING_CONFIRMATION, "the AI changing the clock waits for a person");
    CHECK(AI("alarm.dismiss", "{}") && g_rc.status == INK_RC_FAILED, "nothing rings");
    CHECK(ink_member_switch("kid") == 0, "the child");
    CHECK(call_as("time.set", "{\"utc_s\":1800000000}", INK_SRC_TOUCH) && g_rc.status == INK_RC_REJECTED, "a child cannot change the time: it would get around time limits (%s)", ink_rc_name(g_rc.status));
    ink_receipt_t rc;
    CHECK(ink_setting_set("time.zone", 1, INK_SRC_TOUCH, &rc) == INK_RC_REJECTED, "nor the zone");
    CHECK(ink_setting_set("time.auto", 1, INK_SRC_TOUCH, &rc) == INK_RC_REJECTED, "nor automatic time");
    ink_member_test_reset();
}

/* ---- the other runs ---- */

static void rtc_lost(void) {
    CHECK(!ink_time_valid() && ink_time_now_s() == 0, "the RTC lost its time: the clock is not known");
    ink_receipt_t rc;
    CHECK(ink_cap_invoke("time.now", NULL, INK_SRC_AI, 0, &rc) == INK_RC_FAILED, "and that is what the AI hears (%s)", rc.detail);
    ink_alarm_t a = {.kind = INK_ALARM, .repeat = INK_REPEAT_WEEKDAYS, .weekdays = 0x7F, .hour = 8, .min = 5};
    int id = ink_alarm_add(&a);
    CHECK(id > 0 && !a.next_s, "an alarm can be set; it waits for the clock");
    /* the service asked for a connection by itself */
    int64_t synced = 0;
    for (int i = 0; i < 400 && !(state_int("time.synced", &synced) && synced); i++) osal_task_delay_us(50000);
    CHECK(synced == 1 && ink_time_valid() && llabs(ink_time_now_s() - (BOOT_S + (int64_t)(osal_now_us() / 1000000))) <= 2, "the hub says what time it is (%lld)", (long long)ink_time_now_s());
    int64_t rtc = 0;
    CHECK(hal_rtc_get(&rtc) && llabs(rtc - ink_time_now_s()) <= 1, "and the RTC is right again");
    int cursor = 0;
    ink_alarm_t got = {0};
    CHECK(ink_alarm_next(&cursor, INK_ALARM, &got) == 1 && got.next_s > ink_time_now_s() && g_changed >= 1, "the alarm has its time now");
    CHECK(wait_for(&g_fired, 1, 330), "and goes off");
    ink_alarm_dismiss(0);
}

static void line_low(void) {
    /* the script holds the RTC's line low from the start */
    int64_t now = ink_time_now_s();
    int64_t minute = (now / 60 + 2) * 60;
    ink_alarm_t r = {.kind = INK_REMINDER, .at_utc = minute};
    snprintf(r.title, sizeof r.title, "no rtc line");
    CHECK(ink_alarm_add(&r) > 0, "a reminder");
    osal_task_delay_us(1000000);
    ink_power_lock();
    CHECK(wait_for(&g_rem_fired, 1, 150), "it goes off");
    int64_t late = g_fired_wall_us - minute * 1000000;
    CHECK(late >= 0 && late < 700000, "on time without the RTC's interrupt (%lld ms)", (long long)late / 1000);
    CHECK(sim_metric_value("power.rtc_wake_unavailable") >= 1 && sim_metric_value("power.wakes.rtc") == 0, "the RTC's line was not used");
    ink_power_unlock(osal_now_us());
}

static void reboot1(void) { /* the wall clock restarts at BOOT_S */
    ink_alarm_t soon = {.kind = INK_ALARM, .year = 2026, .month = 11, .day = 2, .hour = 8, .min = 2};
    snprintf(soon.title, sizeof soon.title, "missed");
    ink_alarm_t later = {.kind = INK_ALARM, .year = 2026, .month = 11, .day = 2, .hour = 8, .min = 12};
    snprintf(later.title, sizeof later.title, "later");
    ink_alarm_t daily = {.kind = INK_ALARM, .repeat = INK_REPEAT_WEEKDAYS, .weekdays = 0x7F, .hour = 8, .min = 3};
    snprintf(daily.title, sizeof daily.title, "daily");
    ink_alarm_t rem = {.kind = INK_REMINDER, .at_utc = BOOT_S + 120};
    snprintf(rem.title, sizeof rem.title, "missed rem");
    int a = ink_alarm_add(&soon), b = ink_alarm_add(&later), c = ink_alarm_add(&daily), d = ink_alarm_add(&rem);
    CHECK(a > 0 && b > 0 && c > 0 && d > 0, "four things to remember");
    ink_db_flush_all();
    for (int i = 0; i < 500 && (!ink_db_idle() || ink_storage_pending() > 0); i++) osal_task_delay_us(10000);
    CHECK(ink_db_idle(), "they are on the flash");
}

static void reboot2(void) { /* booted 5 minutes later: --wall-start */
    CHECK(wait_for(&g_missed, 2, 5), "the alarms that came due while it was off are reported (%d)", g_missed);
    CHECK(g_rem_missed == 1, "the reminder too");
    CHECK(g_fired == 0, "none of them rings late");
    int cursor = 0;
    ink_alarm_t got;
    int enabled_later = 0, daily_ok = 0;
    while (ink_alarm_next(&cursor, INK_ALARM, &got) == 1) {
        if (!strcmp(got.title, "later") && got.enabled && got.next_s == BOOT_S + 12 * 60) enabled_later = 1;
        if (!strcmp(got.title, "daily") && got.enabled && got.next_s == BOOT_S + 86400 + 3 * 60) daily_ok = 1;
    }
    CHECK(enabled_later, "what is still ahead is still ahead");
    CHECK(daily_ok, "a repeating one moved on to tomorrow");
    CHECK(wait_for(&g_fired, 1, 8 * 60), "and the one ahead rings");
    CHECK(!strcmp(g_last_title, "later"), "that one (%s)", g_last_title);
    ink_alarm_dismiss(0);
}

/* A clean flash, for a run that does not continue another. */
static void test_task(void *arg) {
    (void)arg;
    if (ink_display_init() != 0 || ink_storage_init() != 0 || ink_cap_init() != 0 || ink_exec_init() != 0 || ink_res_init() != 0 || ink_state_init() != 0 ||
        ink_settings_init() != 0 || ink_svc_init() != 0 || ink_data_init() != 0 || ink_power_init() != 0 || ink_net_init() != 0 ||
        ink_hub_init("wss://hub.local/v1/device") != 0 || ink_tools_init() != 0) {
        CHECK(0, "the services start");
        return;
    }
    ink_members_init();
    CHECK(ink_cap_subscribe("alarm.", on_time_event, NULL, INK_EXEC_DIRECT) == 0 && ink_cap_subscribe("reminder.", on_time_event, NULL, INK_EXEC_DIRECT) == 0 &&
              ink_cap_subscribe("time.", on_time_event, NULL, INK_EXEC_DIRECT) == 0,
          "subscribe");
    CHECK(ink_time_init() == 0, "the time service starts");
    const char *phase = getenv("INK_TIME_PHASE");
    if (phase && !strcmp(phase, "rtc_lost")) rtc_lost();
    else if (phase && !strcmp(phase, "line_low")) line_low();
    else if (phase && !strcmp(phase, "reboot1")) reboot1();
    else if (phase && !strcmp(phase, "reboot2")) reboot2();
    else {
        zones();
        rules();
        capabilities();
        awake();
        asleep();
        hub_time();
        moved_clock();
    }
    ink_probe_count("test.finished");
}

void ink_main(void) {
    osal_task_cfg_t cfg = {.name = "timetest", .fn = test_task, .stack_bytes = 8192, .priority = OSAL_PRIO_NORMAL, .core = -1};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "create the test task");
}
