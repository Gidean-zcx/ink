/* calendar-3: the reference path through the capability bus (AI and touch sources), the time service's events,
 * reminders going off, the old reminder readers, undo, iCalendar out and back, migration from time.alarms. */
#include <stdlib.h>
#include <stddef.h>
#include "ink/ui_shell.h"
#include "ink/sys/notify.h"
#define ink_main product_main
#include "../../system/boot/main.c"
#undef ink_main
#include "../integration/whole_product.h"

static char phase[16], owner[16], last_at[32], last_full[128], undo[160];
static volatile int fired;
static int64_t last_lead;
static ink_source_t src = INK_SRC_AI;

static void on_fired(void *ctx, const char *id, const ink_args_t *args) {
    (void)ctx; (void)id;
    ink_args_get_str(args, "at", last_at, sizeof last_at);
    ink_args_get_str(args, "full_title", last_full, sizeof last_full);
    ink_args_get_int(args, "lead_min", &last_lead);
    __atomic_add_fetch(&fired, 1, __ATOMIC_RELEASE);
}
static bool call(const char *id, const char *args) { return call_as(id, args, src); }
static void ok(const char *id, const char *args) {
    bool undo_call = !strcmp(id, "system.undo");
    CHECK(call(id, args) && g_rc.status == (undo_call ? INK_RC_UNDONE : INK_RC_DONE), "%s done (%s): %.90s", id, ink_rc_name(g_rc.status), g_rc.detail);
    if (!strstr(id, ".list")) delay_ms(200); /* the AI activity log takes a moment per write (16 may wait) */
}
static int64_t at(int y, int mo, int d, int h, int mi) { return ink_time_from_local(&(ink_tm_t){.year = y, .month = mo, .day = d, .hour = h, .min = mi}); }
static uint32_t id_of(const char *title) {
    ink_event_t e; int cursor = 0;
    while (ink_event_next(&cursor, &e) == 1) if (!strcmp(e.title, title) && !e.parent) return e.id;
    return 0;
}
static bool listed(const char *from, const char *until, const char *text) {
    char args[96]; snprintf(args, sizeof args, "{\"from\":\"%s\",\"until\":\"%s\"}", from, until);
    ok("calendar.list", args);
    return strstr(g_result, text) != NULL;
}
/* Occurrences of a title in [from, until) through the public calls (calendar.list's text is cut at 4 KB here). */
static int occurrences(const char *title, int64_t from, int64_t until) {
    ink_event_t e; int cursor = 0, n = 0;
    while (ink_event_next(&cursor, &e) == 1) {
        if (strcmp(e.title, title)) continue;
        for (int64_t t = ink_event_occurrence(&e, from); t && t < until; t = ink_event_occurrence(&e, t + (e.duration_s > 0 ? e.duration_s : 1))) n++;
    }
    return n;
}
/* Every occurrence in [from, until) as sorted text lines: title, start, length, reminders, rule, edited. */
static int line_cmp(const void *a, const void *b) { return strcmp(*(char *const *)a, *(char *const *)b); }
static void dump(char *out, size_t cap, int64_t from, int64_t until) {
    static char lines[400][256]; static char *order[400];
    ink_event_t e; int cursor = 0, n = 0;
    while (ink_event_next(&cursor, &e) == 1)
        for (int64_t t = ink_event_occurrence(&e, from); t && t < until && n < 400; t = ink_event_occurrence(&e, t + (e.duration_s > 0 ? e.duration_s : 1))) {
            snprintf(lines[n], sizeof lines[n], "%s|%lld|%lld|%d:%d,%d,%d|%d/%d/%d-%02d-%02d/%d|%d|%d", e.title, (long long)t, (long long)e.duration_s, e.nleads, (int)e.leads[0], (int)e.leads[1], (int)e.leads[2],
                     e.freq, e.interval, e.until_year, e.until_month, e.until_day, e.count, e.parent != 0, e.all_day);
            order[n] = lines[n]; n++;
        }
    qsort(order, (size_t)n, sizeof order[0], line_cmp);
    size_t len = 0; out[0] = 0;
    for (int i = 0; i < n && len + 170 < cap; i++) len += (size_t)snprintf(out + len, cap - len, "%s\n", order[i]);
}
static int count_of(const char *hay, const char *needle) { int n = 0; for (const char *p = hay; (p = strstr(p, needle)) != NULL; p++) n++; return n; }
static void wait_fire(int64_t moment, int want, const char *what) {
    ink_time_set(moment - 2, false);
    CHECK(WAIT_FOR(__atomic_load_n(&fired, __ATOMIC_ACQUIRE) >= want, 6000), "%s goes off (%d)", what, fired);
}
static void quiet_until(int64_t moment, const char *what) {
    int before = fired;
    ink_time_set(moment - 2, false); delay_ms(4000);
    CHECK(fired == before, "%s does not go off", what);
}
static int64_t projected(uint32_t id, int64_t from) { /* what the lock screen's next line and do-not-disturb read */
    int cursor = 0; ink_alarm_t a;
    while (ink_alarm_next(&cursor, INK_REMINDER, &a) == 1) if (a.id == id) return ink_alarm_occurrence(&a, from);
    return -1;
}

static void trip(void) {
    ok("reminder.add", "{\"at\":\"2026-11-20 20:00\",\"end\":\"2026-11-22 18:00\",\"title\":\"出差\",\"reminders\":[1440,60]}");
    CHECK(listed("2026-11-21", "2026-11-22", "出差") && listed("2026-11-22", "2026-11-23", "出差") && !listed("2026-11-23", "2026-11-24", "出差"), "the trip covers 20-22 and ends on the 22nd");
    CHECK(strstr(g_result, "出差") == NULL, "nothing after its end");
    ok("calendar.list", "{\"from\":\"2026-11-20\",\"until\":\"2026-11-21\"}");
    CHECK(strstr(g_result, "\"reminders\":[1440,60]") && strstr(g_result, "\"end\":\"2026-11-22 18:00"), "two reminders and the end are kept: %.200s", g_result);
}
static void weekly(void) {
    ok("reminder.add", "{\"at\":\"2026-11-16 10:00\",\"end\":\"2026-11-16 11:00\",\"title\":\"周会\",\"reminders\":[10],\"rule\":{\"freq\":\"weekly\",\"until\":\"2026-12-31\"}}");
    CHECK(occurrences("周会", at(2026, 11, 16, 0, 0), at(2027, 1, 12, 0, 0)) == 7 && listed("2026-12-28", "2026-12-29", "2026-12-28 10:00") && !listed("2027-01-04", "2027-01-05", "周会"),
          "seven Mondays up to 12-31, none after: %d", occurrences("周会", at(2026, 11, 16, 0, 0), at(2027, 1, 12, 0, 0)));
}
static void this_one(void) {
    char args[200]; uint32_t id = id_of("周会");
    snprintf(args, sizeof args, "{\"id\":%u,\"at\":\"2026-11-24 15:00\",\"end\":\"2026-11-24 16:00\"}", (unsigned)id);
    CHECK(call("reminder.update", args) && g_rc.status == INK_RC_REJECTED && strstr(g_rc.detail, "scope"), "a change to a repeating one without scope is refused: %s", g_rc.detail);
    snprintf(args, sizeof args, "{\"id\":%u,\"at\":\"2026-11-24 15:00\",\"end\":\"2026-11-24 16:00\",\"scope\":\"this\",\"occurrence\":\"2026-11-23 10:00\"}", (unsigned)id);
    ok("reminder.update", args);
    ok("calendar.list", "{\"from\":\"2026-11-23\",\"until\":\"2026-11-25\"}");
    CHECK(!strstr(g_result, "2026-11-23 10:00\",\"repeat") && strstr(g_result, "\"at\":\"2026-11-24 15:00") && strstr(g_result, "\"edited\":true") &&
          strstr(g_result, "\"occurrence\":\"2026-11-23 10:00\""), "11-23 moved to 11-24 15:00 and says where it came from: %.150s", g_result);
    CHECK(listed("2026-11-30", "2026-12-01", "2026-11-30 10:00") && listed("2026-11-16", "2026-11-17", "2026-11-16 10:00"), "the other Mondays stay");
}
static void delete_one(void) {
    char args[200]; uint32_t id = id_of("周会");
    snprintf(args, sizeof args, "{\"id\":%u}", (unsigned)id);
    CHECK(call("reminder.cancel", args) && g_rc.status == INK_RC_REJECTED && strstr(g_rc.detail, "scope"), "a delete without scope is refused");
    snprintf(args, sizeof args, "{\"id\":%u,\"scope\":\"this\",\"occurrence\":\"2026-11-30 10:00\"}", (unsigned)id);
    ok("reminder.cancel", args);
    CHECK(g_rc.undo_ms > 0, "the delete can be undone");
    snprintf(undo, sizeof undo, "{\"object\":\"%.48s\",\"revision\":%u}", g_rc.object, (unsigned)g_rc.revision);
    CHECK(!listed("2026-11-30", "2026-12-01", "周会"), "calendar.list has no 11-30");
    CHECK(projected(id, at(2026, 11, 30, 0, 0)) == at(2026, 12, 7, 10, 0), "the lock screen and do-not-disturb skip it");
    ok("system.undo", undo);
    CHECK(listed("2026-11-30", "2026-12-01", "周会") && projected(id, at(2026, 11, 30, 0, 0)) == at(2026, 11, 30, 10, 0), "undo brings it back in all three");
    ok("reminder.cancel", args); /* deleted again: that day does not go off */
}
static void following(void) {
    char args[240]; uint32_t id = id_of("周会");
    snprintf(args, sizeof args, "{\"id\":%u,\"at\":\"2026-12-14 14:00\",\"end\":\"2026-12-14 15:00\",\"scope\":\"following\",\"occurrence\":\"2026-12-14 10:00\"}", (unsigned)id);
    ok("reminder.update", args);
    CHECK(listed("2026-12-07", "2026-12-08", "2026-12-07 10:00") && listed("2026-12-14", "2026-12-15", "2026-12-14 14:00") && listed("2026-12-28", "2026-12-29", "2026-12-28 14:00"),
          "12-07 stays at 10:00, from 12-14 on it is 14:00");
    CHECK(occurrences("周会", at(2026, 11, 16, 0, 0), at(2027, 1, 12, 0, 0)) == 6, "still up to 12-31: six left after one deleted (%d)", occurrences("周会", at(2026, 11, 16, 0, 0), at(2027, 1, 12, 0, 0)));
    ok("reminder.add", "{\"at\":\"2026-11-17 08:00\",\"end\":\"2026-11-17 08:30\",\"title\":\"晨跑\",\"rule\":{\"freq\":\"weekly\",\"count\":8}}");
    snprintf(args, sizeof args, "{\"id\":%u,\"at\":\"2026-12-08 07:00\",\"end\":\"2026-12-08 07:30\",\"scope\":\"following\",\"occurrence\":\"2026-12-08 08:00\"}", (unsigned)id_of("晨跑"));
    ok("reminder.update", args);
    ink_event_t e; int cursor = 0, counts = 0;
    while (ink_event_next(&cursor, &e) == 1) if (!strcmp(e.title, "晨跑")) counts = counts * 10 + e.count;
    CHECK(occurrences("晨跑", at(2026, 11, 16, 0, 0), at(2027, 3, 1, 0, 0)) == 8 && (counts == 35 || counts == 53), "eight in all, split 3 + 5 (%d)", counts);
}
static void lunar(void) {
    ok("reminder.add", "{\"at\":\"2026-09-25 09:00\",\"title\":\"妈妈生日\",\"repeat\":\"lunar_yearly\",\"reminders\":[0]}");
    CHECK(listed("2027-09-15", "2027-09-16", "妈妈生日"), "lunar 8-15 falls on 2027-09-15");
    ink_event_t e; CHECK(ink_event_get(id_of("妈妈生日"), &e) == 0 && e.lmonth == 8 && e.lday == 15 && !e.lleap, "stored as the eighth month's 15th");
    /* the 30th of a month that has 29 days falls on its last day; a leap-month date falls in the plain month (time_service.c:266-277) */
    ok("reminder.add", "{\"at\":\"2026-03-18 09:00\",\"title\":\"三十生日\",\"repeat\":\"lunar_yearly\"}");
    ink_event_get(id_of("三十生日"), &e);
    int64_t next = ink_event_occurrence(&e, at(2027, 1, 1, 0, 0));
    ink_tm_t t; ink_time_to_local(next, &t); ink_lunar_t l; ink_lunar_from_solar(t.year, t.month, t.day, &l);
    CHECK(e.lday == 30 && l.month == e.lmonth && l.day == ink_lunar_month_days(l.year, l.month, false), "the 30th where there are %d days", ink_lunar_month_days(l.year, l.month, false));
}
static void month_rules(void) {
    ok("reminder.add", "{\"at\":\"2026-01-31 09:00\",\"title\":\"月底\",\"ring\":false,\"rule\":{\"freq\":\"monthly\"}}");
    CHECK(!listed("2026-02-01", "2026-03-01", "月底") && listed("2026-03-31", "2026-04-01", "月底") && !listed("2026-04-01", "2026-05-01", "月底"), "the 31st skips months without one (Event.kt:112-144)");
    ok("reminder.add", "{\"at\":\"2026-11-26 09:00\",\"title\":\"感恩\",\"ring\":false,\"rule\":{\"freq\":\"yearly\",\"monthly\":\"weekday\"}}");
    CHECK(listed("2027-11-25", "2027-11-26", "感恩"), "the fourth Thursday of November");
    ok("reminder.add", "{\"at\":\"2026-11-30 09:00\",\"title\":\"最后一天\",\"ring\":false,\"rule\":{\"freq\":\"monthly\",\"monthly\":\"last_day\",\"interval\":2}}");
    CHECK(listed("2027-01-31", "2027-02-01", "最后一天") && !listed("2026-12-31", "2027-01-01", "最后一天"), "every second month's last day");
}
static void fires(void) {
    CHECK(ink_cap_subscribe("reminder.fired", on_fired, NULL, INK_EXEC_DIRECT) == 0, "listen to reminder.fired");
    wait_fire(at(2026, 11, 19, 20, 0), 1, "the trip, a day before");
    CHECK(!strcmp(last_full, "出差") && !strcmp(last_at, "2026-11-20 20:00") && last_lead == 1440, "full_title %s at %s lead %lld", last_full, last_at, (long long)last_lead);
    wait_fire(at(2026, 11, 20, 19, 0), 2, "the trip, an hour before");
    CHECK(!strcmp(last_at, "2026-11-20 20:00") && last_lead == 60, "the same start, lead %lld", (long long)last_lead);
    quiet_until(at(2026, 11, 23, 9, 50), "the moved Monday at its old time");
    wait_fire(at(2026, 11, 24, 14, 50), 3, "the moved occurrence");
    CHECK(!strcmp(last_at, "2026-11-24 15:00") && !strcmp(last_full, "周会"), "it says 11-24 15:00 (%s)", last_at);
    quiet_until(at(2026, 11, 30, 9, 50), "the deleted Monday");
    ok("notify.dnd_set", "{\"calendar\":true}");
    ink_time_set(at(2026, 12, 7, 10, 30), false); delay_ms(300);
    ink_notify_dnd_t d; ink_notify_dnd_state(&d);
    CHECK(d.active && d.reason && !strcmp(d.reason, "calendar"), "do-not-disturb sees the weekly meeting at 12-07 10:30");
}
static void ics(void) {
    static char before[32768], after[32768];
    dump(before, sizeof before, at(2026, 1, 1, 0, 0), at(2028, 12, 31, 0, 0));
    ok("calendar.export", NULL);
    char path[96] = ""; const char *p = strstr(g_result, "\"path\":\"");
    if (p) { snprintf(path, sizeof path, "%s", p + 8); char *q = strchr(path, '"'); if (q) *q = 0; }
    CHECK(strstr(path, "/me/日历导出-") && strstr(g_result, "\"count\":"), "exported to %s", path);
    ink_event_t e; int cursor = 0; uint32_t ids[64]; int n = 0;
    while (ink_event_next(&cursor, &e) == 1 && n < 64) if (!e.parent) ids[n++] = e.id;
    for (int i = 0; i < n; i++) { char args[64]; snprintf(args, sizeof args, "{\"id\":%u,\"scope\":\"all\"}", (unsigned)ids[i]); ok("reminder.cancel", args); }
    cursor = 0; CHECK(ink_event_next(&cursor, &e) == 0, "everything deleted");
    char args[160]; snprintf(args, sizeof args, "{\"path\":\"%s\"}", path);
    ok("calendar.import", args);
    CHECK(strstr(g_result, "\"skipped\":0") && strstr(g_result, "\"existing\":0") && strstr(g_result, "\"full\":0"), "all came in: %.150s", g_result);
    dump(after, sizeof after, at(2026, 1, 1, 0, 0), at(2028, 12, 31, 0, 0));
    size_t k = 0; while (before[k] && before[k] == after[k]) k++;
    while (k && before[k - 1] != '\n') k--;
    CHECK(before[0] && !strcmp(before, after), "every occurrence back as before (moved and deleted Mondays, reminders, ends, lunar): before %.60s after %.60s", before + k, after + k);
    CHECK(strstr(after, "妈妈生日") && strstr(after, "|1|0\n") && occurrences("周会", at(2026, 11, 30, 0, 0), at(2026, 12, 1, 0, 0)) == 0, "the edit and the deletion are still there");
    ok("calendar.import", args);
    CHECK(strstr(g_result, "\"imported\":0") && !strstr(g_result, "\"existing\":0"), "again: already there: %.150s", g_result);
}
static void hand_and_ai(void) { /* the same capability from a touch is the same entry */
    src = INK_SRC_TOUCH;
    ok("reminder.add", "{\"at\":\"2026-12-21 09:00\",\"title\":\"手动周会\",\"rule\":{\"freq\":\"weekly\",\"count\":3}}");
    char args[200]; snprintf(args, sizeof args, "{\"id\":%u,\"scope\":\"this\",\"occurrence\":\"2026-12-28 09:00\"}", (unsigned)id_of("手动周会"));
    ok("reminder.cancel", args);
    CHECK(occurrences("手动周会", at(2026, 12, 20, 0, 0), at(2027, 1, 10, 0, 0)) == 2, "touch deletes one of three");
    src = INK_SRC_AI;
}
static void isolation(void) {
    CHECK(ink_member_switch("kid") == 0, "the child"); delay_ms(300);
    ok("calendar.list", "{\"from\":\"2026-11-01\",\"until\":\"2028-01-01\"}");
    CHECK(!strstr(g_result, "周会") && !strstr(g_result, "出差"), "the child sees none of the adult's");
    ok("reminder.add", "{\"at\":\"2026-12-20 10:00\",\"title\":\"孩子的事\",\"ring\":false}");
    CHECK(call("calendar.export", NULL) && g_rc.status == INK_RC_DONE && strstr(g_result, "\"count\":1"), "the child exports only its own: %.100s", g_result);
    CHECK(ink_member_switch(owner) == 0, "back"); delay_ms(300);
    CHECK(listed("2026-12-01", "2027-01-01", "周会") && !strstr(g_result, "孩子的事"), "the adult sees none of the child's");
}
static void legacy_records(void) {
    static const ink_db_def_t def = {.name = "time.alarms", .title = "闹钟和提醒", .scope = INK_DB_DEVICE, .schema = 1};
    static const ink_db_def_t events = {.name = "time.events", .title = "日程", .scope = INK_DB_DEVICE, .schema = 1};
    ink_db_t *db = ink_db_open(&def, NULL), *edb = ink_db_open(&events, NULL);
    CHECK(db && edb, "open both collections");
    if (!db || !edb) return;
    ink_alarm_t alarm = {.kind = INK_ALARM, .enabled = true, .hour = 7, .min = 15, .repeat = INK_REPEAT_WEEKDAYS, .weekdays = 0x3E, .src = INK_SRC_TOUCH};
    ink_alarm_t full = {.kind = INK_REMINDER, .enabled = true, .year = 2026, .month = 12, .day = 3, .hour = 9, .min = 30, .repeat = INK_REPEAT_MONTHLY, .src = INK_SRC_AI, .silent = true, .duration_s = 1800};
    ink_alarm_t old = {.kind = INK_REMINDER, .enabled = true, .year = 2026, .month = 12, .day = 5, .hour = 18, .src = INK_SRC_TOUCH};
    ink_alarm_t cut = {.kind = INK_REMINDER, .enabled = true, .year = 2026, .month = 12, .day = 6, .hour = 8, .all_day = true, .duration_s = 86400, .silent = true, .src = INK_SRC_TOUCH};
    ink_alarm_t *recs[] = {&alarm, &full, &old, &cut};
    const char *titles[] = {"起床闹钟", "旧的月会", "旧长度提醒", "断电那条"};
    for (int i = 0; i < 4; i++) {
        snprintf(recs[i]->owner, sizeof recs[i]->owner, "%s", owner); snprintf(recs[i]->title, sizeof recs[i]->title, "%s", titles[i]);
        if (i == 2) recs[i]->next_s = at(2026, 12, 5, 18, 0);
        recs[i]->id = ink_db_new_id(db);
        ink_db_write_t w = {.id = recs[i]->id, .sort = recs[i]->next_s, .data = recs[i], .len = i == 2 ? offsetof(ink_alarm_t, silent) : sizeof(ink_alarm_t), .src = recs[i]->src};
        CHECK(ink_db_put(db, &w, NULL) == INK_DB_OK, "legacy record %s", titles[i]);
    }
    /* a power cut after the event was written and before the old record went: both are there */
    ink_event_t e = {.id = cut.id, .year = 2026, .month = 12, .day = 6, .all_day = true, .duration_s = 86400, .src = INK_SRC_TOUCH, .interval = 1};
    snprintf(e.owner, sizeof e.owner, "%s", owner); snprintf(e.title, sizeof e.title, "断电那条"); snprintf(e.uid, sizeof e.uid, "ink-00000000-%u@calendar", (unsigned)cut.id);
    ink_db_write_t w = {.id = e.id, .data = &e, .len = sizeof e, .src = e.src};
    CHECK(ink_db_put(edb, &w, NULL) == INK_DB_OK, "the half-migrated event");
    char ids[96]; snprintf(ids, sizeof ids, "%u %u %u %u", (unsigned)alarm.id, (unsigned)full.id, (unsigned)old.id, (unsigned)cut.id);
    CHECK(ink_db_put(db, &(ink_db_write_t){.id = 0x7ffffff0u, .data = ids, .len = strlen(ids) + 1, .src = INK_SRC_SYSTEM}, NULL) == INK_DB_OK, "remember the ids");
    ink_db_close(db); ink_db_close(edb);
}
static void migrated(void) {
    static const ink_db_def_t def = {.name = "time.alarms", .title = "闹钟和提醒", .scope = INK_DB_DEVICE, .schema = 1};
    ink_db_t *db = ink_db_open(&def, NULL);
    CHECK(db != NULL, "open alarms");
    if (!db) return;
    char ids[96] = ""; ink_rec_t r; unsigned a = 0, f = 0, o = 0, c = 0;
    CHECK(ink_db_get(db, 0x7ffffff0u, &r, ids, sizeof ids) == 0 && sscanf(ids, "%u %u %u %u", &a, &f, &o, &c) == 4, "the ids written before");
    ink_alarm_t got; ink_event_t e;
    CHECK(ink_db_get(db, a, &r, &got, sizeof got) == 0 && r.len == sizeof got && got.kind == INK_ALARM && got.hour == 7 && got.min == 15 && got.weekdays == 0x3E && !strcmp(got.title, "起床闹钟"), "the alarm record is untouched");
    CHECK(ink_db_get(db, f, &r, &got, sizeof got) != 0 && ink_db_get(db, o, &r, &got, sizeof got) != 0 && ink_db_get(db, c, &r, &got, sizeof got) != 0, "reminders left time.alarms");
    CHECK(ink_event_get(f, &e) == 0 && e.freq == INK_FREQ_MONTHLY && e.rday == 3 && e.hour == 9 && e.min == 30 && !e.nleads && e.duration_s == 1800 && !strcmp(e.title, "旧的月会"), "new-length reminder: same id, time, repeat, silent, end");
    CHECK(ink_event_get(o, &e) == 0 && e.freq == INK_FREQ_NONE && e.year == 2026 && e.month == 12 && e.day == 5 && e.hour == 18 && e.nleads == 1 && e.leads[0] == 0 && !e.all_day && !e.duration_s, "old-length reminder: rings at its start");
    int seen = 0, cursor = 0; ink_event_t any;
    while (ink_event_next(&cursor, &any) == 1) seen += any.id == c;
    CHECK(seen == 1 && ink_event_get(c, &e) == 0 && e.all_day && e.duration_s == 86400, "the half-migrated one once, all-day kept");
    int cursor2 = 0; ink_alarm_t alarm; bool alarm_listed = false;
    while (ink_alarm_next(&cursor2, INK_ALARM, &alarm) == 1) alarm_listed |= alarm.id == a;
    CHECK(alarm_listed, "the alarm still rings as before");
    ink_alarm_t next = {.kind = INK_ALARM, .hour = 6, .min = 0};
    int nid = ink_alarm_add(&next);
    CHECK(nid > 0 && (unsigned)nid != f && (unsigned)nid != o && (unsigned)nid != c, "a new alarm takes no event's id");
    ink_alarm_cancel((uint32_t)nid);
    ink_db_close(db);
}
static void host_phase(void *arg) { (void)arg; const char *s = getenv("INK_CALENDAR_PHASE"); snprintf(phase, sizeof phase, "%s", s ? s : "path"); }
static void run(void *arg) {
    (void)arg;
    CHECK(WAIT_FOR(ink_member_space_open(), 10000), "member space opens");
    test_start_home();
    ink_time_set(at(2026, 11, 14, 8, 0), false);
    snprintf(owner, sizeof owner, "%s", ink_member_current()->id);
    if (!strcmp(phase, "path")) { trip(); weekly(); this_one(); delete_one(); following(); lunar(); month_rules(); hand_and_ai(); isolation(); fires(); ics(); }
    else if (!strcmp(phase, "legacy")) legacy_records();
    else migrated();
    ink_db_flush_all(); CHECK(WAIT_FOR(ink_db_idle() && !ink_storage_pending(), 10000), "writes reach flash");
    ink_probe_count("test.finished");
}
void ink_main(void) {
    vtk_host_call(host_phase, NULL); product_main();
    osal_task_cfg_t cfg = {.name = "calendar-3-service", .fn = run, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start the test driver");
}
