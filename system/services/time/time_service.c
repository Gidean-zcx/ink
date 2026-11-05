/*
 * Time service (see ink/sys/time.h).
 *
 * The clock: the RTC at start, the hub's time when a connection comes up (its
 * answer to "hello" carries it; half the round trip is added), a person's
 * setting. It only jumps when it is more than 2 s off. Alarms follow.
 *
 * The schedule: a table in PSRAM, kept in a device collection. The earliest time
 * due is given three ways: a deadline on the monotonic clock, the power service's
 * wake time (a sleep slice ends there, and the chip does not sleep past it), and
 * the RTC's alarm (a minute's resolution, rounded down: it wakes the chip a little
 * early and it sleeps again). If the RTC's interrupt line does not work (the factory
 * log shows P16 low all the time) the first two are on time without it.
 *
 * Going off is an event ("alarm.fired", "reminder.fired"): this service does not
 * know who rings or shows it. The notification service subscribes.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ink/hal_device.h"
#include "ink/osal.h"
#include "ink/probe.h"
#include "ink/sys/cap.h"
#include "ink/sys/data.h"
#include "ink/sys/hub.h"
#include "ink/sys/members.h"
#include "ink/sys/power.h"
#include "ink/sys/settings.h"
#include "ink/sys/state.h"
#include "ink/sys/svc.h"
#include "ink/sys/time.h"
#include "time_internal.h"

#define MAX_ALARMS 64
#define MAX_RINGING 4
#define LATE_OK_S 60          /* later than this it did not go off: it is reported missed */
#define RING_S 120            /* nobody came: it stops by itself and counts as missed */
#define SNOOZE_MIN 9
#define STEP_S 2              /* the hub's time moves the clock only when it is further off than this */
#define MAX_FIRE_BATCH 8

static const ink_db_def_t ALARMS_DB = {.name = "time.alarms", .title = "闹钟和提醒", .scope = INK_DB_DEVICE, .schema = 1};

typedef struct {
    uint32_t id;
    uint64_t until_us;
} ring_t;

static struct {
    ink_alarm_t a[MAX_ALARMS];
    int n;
    ink_db_t *db;
    ring_t rings[MAX_RINGING];
    int h_synced, h_zone, h_minute, h_ringing;
    bool valid, synced;
    ink_svc_deadline_t due_at, minute_at, ring_at;
    ink_exec_job_t rtc_job;
    int64_t rtc_next_s;
    ink_args_t ev; /* the timer task's event arguments: too large for its stack */
} T INK_PSRAM_BSS;

/* ------------------------------------------------------------ time zones */

typedef enum { DST_NONE = 0, DST_US, DST_EU } dst_rule_t;

static const struct {
    const char *name;
    int16_t offset_min;
    uint8_t rule;
} ZONES[] = {
    {"Asia/Shanghai", 480, DST_NONE},    {"UTC", 0, DST_NONE},
    {"Asia/Tokyo", 540, DST_NONE},       {"Asia/Hong_Kong", 480, DST_NONE},
    {"Asia/Singapore", 480, DST_NONE},   {"Europe/London", 0, DST_EU},
    {"Europe/Paris", 60, DST_EU},        {"America/New_York", -300, DST_US},
    {"America/Los_Angeles", -480, DST_US},
};
#define NZONES ((int)(sizeof ZONES / sizeof ZONES[0]))
static const char *const ZONE_NAMES[NZONES + 1] = {"Asia/Shanghai", "UTC", "Asia/Tokyo", "Asia/Hong_Kong", "Asia/Singapore", "Europe/London",
                                                   "Europe/Paris", "America/New_York", "America/Los_Angeles", NULL};

static int zone_index(void) {
    int z = (int)ink_setting_get("time.zone");
    return z >= 0 && z < NZONES ? z : 0;
}

const char *ink_time_zone(void) { return ZONES[zone_index()].name; }

int64_t time_days_from_civil(int y, int m, int d) {
    y -= m <= 2;
    int64_t era = (y >= 0 ? y : y - 399) / 400;
    unsigned yoe = (unsigned)(y - era * 400);
    unsigned doy = (153u * (unsigned)(m + (m > 2 ? -3 : 9)) + 2u) / 5u + (unsigned)d - 1u;
    unsigned doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;
    return era * 146097 + (int64_t)doe - 719468;
}

static void civil_from_days(int64_t z, int *y, int *m, int *d) {
    z += 719468;
    int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    unsigned doe = (unsigned)(z - era * 146097);
    unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    int yy = (int)yoe + (int)(era * 400);
    unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    unsigned mp = (5 * doy + 2) / 153;
    *d = (int)(doy - (153 * mp + 2) / 5 + 1);
    *m = (int)(mp < 10 ? mp + 3 : mp - 9);
    *y = yy + (*m <= 2);
}

static int weekday(int64_t days) { return (int)(((days % 7) + 11) % 7); } /* 1970-01-01 was a Thursday */

/* The n-th Sunday of a month (n > 0), or the last (n = 0): days since 1970. */
static int64_t sunday_of(int y, int m, int n) {
    if (n > 0) {
        int64_t first = time_days_from_civil(y, m, 1);
        return first + (7 - weekday(first)) % 7 + (n - 1) * 7;
    }
    int64_t last = time_days_from_civil(m == 12 ? y + 1 : y, m == 12 ? 1 : m + 1, 1) - 1;
    return last - weekday(last);
}

/* Is daylight saving on at that moment, in that zone? */
static bool dst_on(int z, int64_t utc_s) {
    int rule = ZONES[z].rule;
    if (rule == DST_NONE) return false;
    int y, m, d;
    int64_t std_s = utc_s + ZONES[z].offset_min * 60;
    civil_from_days(std_s >= 0 ? std_s / 86400 : -((-std_s + 86399) / 86400), &y, &m, &d);
    int64_t start, end;
    if (rule == DST_US) { /* second Sunday of March 02:00 standard; first Sunday of November 02:00 daylight */
        start = sunday_of(y, 3, 2) * 86400 + 2 * 3600 - ZONES[z].offset_min * 60;
        end = sunday_of(y, 11, 1) * 86400 + 1 * 3600 - ZONES[z].offset_min * 60;
    } else { /* last Sundays of March and October, 01:00 UTC */
        start = sunday_of(y, 3, 0) * 86400 + 3600;
        end = sunday_of(y, 10, 0) * 86400 + 3600;
    }
    return utc_s >= start && utc_s < end;
}

void ink_time_to_local(int64_t utc_s, ink_tm_t *out) {
    int z = zone_index();
    bool dst = dst_on(z, utc_s);
    int off = ZONES[z].offset_min + (dst ? 60 : 0);
    int64_t s = utc_s + off * 60;
    int64_t days = s >= 0 ? s / 86400 : -((-s + 86399) / 86400), rem = s - days * 86400;
    civil_from_days(days, &out->year, &out->month, &out->day);
    out->hour = (int)(rem / 3600);
    out->min = (int)(rem / 60 % 60);
    out->sec = (int)(rem % 60);
    out->wday = weekday(days);
    out->offset_min = off;
    out->dst = dst;
}

int64_t ink_time_from_local(const ink_tm_t *tm) {
    int z = zone_index();
    int64_t local = time_days_from_civil(tm->year, tm->month, tm->day) * 86400 + tm->hour * 3600 + tm->min * 60 + tm->sec;
    int std_off = ZONES[z].offset_min * 60;
    int64_t as_dst = local - std_off - 3600, as_std = local - std_off;
    if (ZONES[z].rule != DST_NONE && dst_on(z, as_dst)) return as_dst; /* the first one, when it happens twice */
    if (!dst_on(z, as_std) || ZONES[z].rule == DST_NONE) return as_std;
    return as_std; /* a time the clocks skip: this is that much later on the clock */
}

/* ------------------------------------------------------------ the clock */

bool ink_time_valid(void) { return __atomic_load_n(&T.valid, __ATOMIC_ACQUIRE); }

int64_t ink_time_now_s(void) {
    if (!ink_time_valid()) return 0;
    return osal_wall_us() / 1000000;
}

static void publish_minute(void) {
    if (!ink_time_valid()) return;
    ink_tm_t tm;
    ink_time_to_local(ink_time_now_s(), &tm);
    ink_state_set_int(T.h_minute, tm.hour * 60 + tm.min);
}

static void minute_cb(void *arg) { /* timer task; awake only: while locked nobody looks, and it would cut the sleep */
    (void)arg;
    if (ink_power_state() == INK_PWR_LOCKED || !ink_time_valid()) return;
    publish_minute();
    int64_t wall = osal_wall_us();
    ink_svc_at(&T.minute_at, osal_now_us() + (uint64_t)(60000000 - wall % 60000000), minute_cb, NULL);
}

static void on_power(void *ctx, ink_power_state_t from, ink_power_state_t to, uint64_t cause_us) { /* power task */
    (void)ctx;
    (void)cause_us;
    if (to == INK_PWR_LOCKED) ink_svc_cancel(&T.minute_at);
    else if (from == INK_PWR_LOCKED) minute_cb(NULL);
}

/* ------------------------------------------------------------ when it goes off next */

static bool valid_date(int y, int m, int d) {
    if (m < 1 || m > 12 || d < 1 || d > 31) return false;
    int cy, cm, cd;
    civil_from_days(time_days_from_civil(y, m, d), &cy, &cm, &cd);
    return cy == y && cm == m && cd == d;
}

static int64_t at_local(int y, int m, int d, int hour, int min) {
    ink_tm_t tm = {.year = y, .month = m, .day = d, .hour = hour, .min = min};
    return ink_time_from_local(&tm);
}

/* The first time after `now` by its rule; 0 if there is none. */
static int64_t rule_next(const ink_alarm_t *a, int64_t now) {
    ink_tm_t t;
    ink_time_to_local(now, &t);
    int64_t today = time_days_from_civil(t.year, t.month, t.day);
    switch (a->repeat) {
    case INK_REPEAT_WEEKDAYS:
        for (int i = 0; i <= 7; i++) {
            int y, m, d;
            civil_from_days(today + i, &y, &m, &d);
            if (!(a->weekdays & (1u << weekday(today + i)))) continue;
            int64_t s = at_local(y, m, d, a->hour, a->min);
            if (s > now) return s;
        }
        return 0;
    case INK_REPEAT_MONTHLY: {
        int y = t.year, m = t.month;
        for (int i = 0; i < 48; i++) { /* the 31st: the months that have one */
            if (valid_date(y, m, a->day)) {
                int64_t s = at_local(y, m, a->day, a->hour, a->min);
                if (s > now) return s;
            }
            if (++m > 12) {
                m = 1;
                y++;
            }
        }
        return 0;
    }
    case INK_REPEAT_YEARLY:
        for (int y = t.year; y < t.year + 9; y++) /* February 29: the years that have one */
            if (valid_date(y, a->month, a->day)) {
                int64_t s = at_local(y, a->month, a->day, a->hour, a->min);
                if (s > now) return s;
            }
        return 0;
    case INK_REPEAT_LUNAR_YEARLY:
        for (int ly = t.year - 1; ly < t.year + 3; ly++) {
            /* a leap-month date falls in the plain month in most years; the 30th in a short month is its last day */
            bool leap = a->lleap && ink_lunar_leap_month(ly) == a->lmonth;
            int last = ink_lunar_month_days(ly, a->lmonth, leap);
            if (!last) continue;
            ink_lunar_t l = {ly, a->lmonth, a->lday <= last ? a->lday : last, leap};
            int y, m, d;
            if (!ink_lunar_to_solar(&l, &y, &m, &d)) continue;
            int64_t s = at_local(y, m, d, a->hour, a->min);
            if (s > now) return s;
        }
        return 0;
    default:
        if (a->at_utc) return a->at_utc > now ? a->at_utc : 0;
        if (a->year) {
            int64_t s = at_local(a->year, a->month, a->day, a->hour, a->min);
            return s > now ? s : 0;
        }
        for (int i = 0; i < 2; i++) { /* the next time that hour and minute comes */
            int y, m, d;
            civil_from_days(today + i, &y, &m, &d);
            int64_t s = at_local(y, m, d, a->hour, a->min);
            if (s > now) return s;
        }
        return 0;
    }
}

static int64_t next_of(const ink_alarm_t *a, int64_t now) {
    int64_t s = rule_next(a, now);
    if (a->snooze_until > now && (!s || a->snooze_until < s)) return a->snooze_until;
    return s;
}

/* ------------------------------------------------------------ the schedule */

static void due_cb(void *arg);

static void rtc_job(void *arg) { /* exec.bg: the shared I2C bus */
    (void)arg;
    ink_svc_lock();
    int64_t next = T.rtc_next_s;
    ink_svc_unlock();
    hal_rtc_clear_alarm();
    if (next && hal_rtc_set_alarm(next)) ink_probe_count("time.rtc_alarm_set");
}

/* The earliest of the table goes to the deadline, the power service and the RTC. */
static void reschedule(void) {
    if (!ink_time_valid()) return;
    int64_t next = 0;
    ink_svc_lock();
    for (int i = 0; i < T.n; i++)
        if (T.a[i].enabled && T.a[i].next_s && (!next || T.a[i].next_s < next)) next = T.a[i].next_s;
    bool rtc_changed = T.rtc_next_s != next;
    T.rtc_next_s = next;
    ink_svc_unlock();
    if (!next) {
        ink_svc_cancel(&T.due_at);
        ink_power_wake_at(0);
    } else {
        int64_t wall = osal_wall_us(), left = next * 1000000 - wall;
        uint64_t at = osal_now_us() + (uint64_t)(left > 0 ? left : 0);
        ink_svc_at(&T.due_at, at, due_cb, NULL);
        ink_power_wake_at(at);
    }
    if (rtc_changed) ink_exec_post(INK_EXEC_BG, &T.rtc_job, rtc_job, NULL); /* the first alarm starts this executor, not the boot */
}

static void save_alarm(const ink_alarm_t *a) { /* outside the lock */
    ink_db_write_t w = {.id = a->id, .sort = a->next_s, .data = a, .len = sizeof *a, .src = a->src};
    if (T.db && ink_db_put(T.db, &w, NULL) != INK_DB_OK) ink_probe_count("time.save_failed");
}

static int find_alarm(uint32_t id) { /* under the lock */
    for (int i = 0; i < T.n; i++)
        if (T.a[i].id == id) return i;
    return -1;
}

static void remove_at(int i) { /* under the lock */
    memmove(&T.a[i], &T.a[i + 1], sizeof T.a[0] * (size_t)(T.n - i - 1));
    T.n--;
}

static void emit_alarm(const char *event, const ink_alarm_t *a, const char *why) { /* the timer task, or a capability's */
    ink_args_t *args = osal_malloc(sizeof *args, OSAL_MEM_EXTERNAL);
    if (!args) return;
    ink_args_init(args);
    ink_args_int(args, "id", a->id);
    ink_args_str(args, "title", a->title);
    ink_args_str(args, "owner", a->owner);
    if (a->owner[0]) ink_args_str(args, "member", a->owner);
    if (why) ink_args_str(args, "why", why);
    ink_cap_emit(event, args);
    osal_free(args);
}

static void ring_add(uint32_t id, uint64_t now_us) { /* under the lock */
    for (int i = 0; i < MAX_RINGING; i++)
        if (!T.rings[i].id || T.rings[i].id == id) {
            T.rings[i] = (ring_t){id, now_us + (uint64_t)RING_S * 1000000u};
            return;
        }
    T.rings[0] = (ring_t){id, now_us + (uint64_t)RING_S * 1000000u}; /* the oldest gives way: it has rung longest */
}

static void ring_cb(void *arg);

static void rings_changed(void) { /* not under the lock */
    int n = 0;
    uint64_t next = 0;
    ink_svc_lock();
    for (int i = 0; i < MAX_RINGING; i++)
        if (T.rings[i].id) {
            n++;
            if (!next || T.rings[i].until_us < next) next = T.rings[i].until_us;
        }
    ink_svc_unlock();
    ink_state_set_int(T.h_ringing, n);
    if (next) ink_svc_at(&T.ring_at, next, ring_cb, NULL);
    else ink_svc_cancel(&T.ring_at);
}

/* Nobody came: it stops by itself. */
static void ring_cb(void *arg) { /* timer task */
    (void)arg;
    uint64_t now = osal_now_us();
    ink_alarm_t ended[MAX_RINGING];
    int n = 0;
    ink_svc_lock();
    for (int i = 0; i < MAX_RINGING; i++)
        if (T.rings[i].id && T.rings[i].until_us <= now) {
            int k = find_alarm(T.rings[i].id);
            if (k >= 0) ended[n++] = T.a[k];
            T.rings[i].id = 0;
        }
    ink_svc_unlock();
    for (int i = 0; i < n; i++) {
        emit_alarm("alarm.ended", &ended[i], "timeout");
        emit_alarm("alarm.missed", &ended[i], "nobody stopped it");
    }
    rings_changed();
}

/* Whatever is due now goes off, or is reported missed; each moves on. */
static void due_cb(void *arg) { /* timer task */
    (void)arg;
    if (!ink_time_valid()) return;
    int64_t now = ink_time_now_s();
    uint64_t now_us = osal_now_us();
    ink_alarm_t *fired = osal_malloc(sizeof(ink_alarm_t) * MAX_FIRE_BATCH, OSAL_MEM_EXTERNAL);
    bool late[MAX_FIRE_BATCH], saved[MAX_FIRE_BATCH];
    int n = 0;
    if (!fired) {
        ink_svc_at(&T.due_at, now_us + 1000000u, due_cb, NULL);
        return;
    }
    ink_svc_lock();
    for (int i = T.n - 1; i >= 0 && n < MAX_FIRE_BATCH; i--) {
        ink_alarm_t *a = &T.a[i];
        if (!a->enabled || !a->next_s || a->next_s > now) continue;
        late[n] = now - a->next_s > LATE_OK_S;
        ink_probe_value(a->kind == INK_ALARM ? "time.alarm_late_ms" : "time.reminder_late_ms", (double)(osal_wall_us() - a->next_s * 1000000) / 1000.0);
        if (!late[n] && a->kind == INK_ALARM) ring_add(a->id, now_us);
        a->snooze_until = 0;
        a->next_s = a->repeat == INK_REPEAT_NONE ? 0 : rule_next(a, now);
        if (!a->next_s && a->kind == INK_ALARM) a->enabled = false; /* a single alarm stays in the list, off */
        saved[n] = a->kind == INK_ALARM || a->next_s; /* a single reminder is over */
        fired[n] = *a;
        n++;
        if (!a->next_s && a->kind == INK_REMINDER) remove_at(i);
    }
    ink_svc_unlock();
    for (int i = 0; i < n; i++) {
        const ink_alarm_t *a = &fired[i];
        if (saved[i]) save_alarm(a);
        else if (T.db) ink_db_del(T.db, a->id, a->src);
        ink_probe_count(late[i] ? "time.missed" : a->kind == INK_ALARM ? "time.alarms_fired" : "time.reminders_fired");
        if (late[i]) emit_alarm(a->kind == INK_ALARM ? "alarm.missed" : "reminder.missed", a, "the device was off or the clock moved");
        else emit_alarm(a->kind == INK_ALARM ? "alarm.fired" : "reminder.fired", a, NULL);
    }
    osal_free(fired);
    if (n) rings_changed();
    reschedule();
}

/* The clock moved or the zone changed: what is ahead is worked out again; what is due now stays due. */
static void clock_moved(void) {
    int64_t now = ink_time_now_s();
    ink_alarm_t *a = osal_malloc(sizeof *a, OSAL_MEM_EXTERNAL);
    for (int i = 0;; i++) {
        bool any = false, changed = false;
        ink_svc_lock();
        if (i < T.n) {
            any = true;
            if (T.a[i].enabled && (!T.a[i].next_s || T.a[i].next_s > now)) {
                int64_t next = next_of(&T.a[i], now);
                changed = next != T.a[i].next_s;
                T.a[i].next_s = next;
                if (changed && a) *a = T.a[i];
            }
        }
        ink_svc_unlock();
        if (!any) break;
        if (changed && a) save_alarm(a); /* the next boot starts from what holds now */
    }
    osal_free(a);
    publish_minute();
    reschedule();
}

void ink_time_set(int64_t utc_s, bool from_hub) {
    if (from_hub && !ink_setting_get("time.auto") && ink_time_valid()) return;
    osal_set_wall_us(utc_s * 1000000);
    __atomic_store_n(&T.valid, true, __ATOMIC_RELEASE);
    if (!hal_rtc_set(utc_s)) ink_probe_count("time.rtc_write_failed");
    if (from_hub) {
        T.synced = true;
        ink_state_set_bool(T.h_synced, true);
    }
    ink_probe_count("time.clock_moved");
    clock_moved();
    if (!T.minute_at.armed) minute_cb(NULL);
    ink_cap_emit("time.changed", NULL);
}

/* The hub told its time at hello. Hub task. */
static void on_online(void *ctx) {
    (void)ctx;
    int64_t server_ms;
    uint32_t rtt_ms;
    if (!ink_hub_server_time(&server_ms, &rtt_ms)) return;
    int64_t server_s = (server_ms + rtt_ms / 2) / 1000;
    int64_t now = ink_time_now_s();
    int64_t off = ink_time_valid() ? (server_s > now ? server_s - now : now - server_s) : INT32_MAX;
    if (off > STEP_S && (ink_setting_get("time.auto") || !ink_time_valid())) ink_time_set(server_s, true);
    else if (!T.synced) {
        T.synced = true;
        ink_state_set_bool(T.h_synced, true);
    }
}

/* ------------------------------------------------------------ the API */

static bool alarm_valid(const ink_alarm_t *a) {
    if (a->hour > 23 || a->min > 59) return false;
    switch (a->repeat) {
    case INK_REPEAT_NONE: return !a->year || valid_date(a->year, a->month, a->day) || a->at_utc;
    case INK_REPEAT_WEEKDAYS: return (a->weekdays & 0x7F) != 0;
    case INK_REPEAT_MONTHLY: return a->day >= 1 && a->day <= 31;
    case INK_REPEAT_YEARLY: return a->month >= 1 && a->month <= 12 && a->day >= 1 && a->day <= 31;
    case INK_REPEAT_LUNAR_YEARLY: return a->lmonth >= 1 && a->lmonth <= 12 && a->lday >= 1 && a->lday <= 30;
    default: return false;
    }
}

int ink_alarm_add(ink_alarm_t *a) {
    if (!T.db || !alarm_valid(a)) return -1;
    int64_t now = ink_time_now_s();
    a->enabled = true;
    a->snooze_until = 0;
    if (!a->owner[0]) snprintf(a->owner, sizeof a->owner, "%s", ink_member_current()->id);
    a->next_s = now ? next_of(a, now) : 0;
    if (now && !a->next_s) return -2; /* it is in the past, and there is no next */
    ink_svc_lock();
    bool full = T.n >= MAX_ALARMS;
    ink_svc_unlock();
    if (full) return -3;
    a->id = ink_db_new_id(T.db);
    ink_db_write_t w = {.id = a->id, .sort = a->next_s, .data = a, .len = sizeof *a, .src = a->src};
    if (ink_db_put(T.db, &w, NULL) != INK_DB_OK) return -4;
    ink_svc_lock();
    if (T.n < MAX_ALARMS) T.a[T.n++] = *a;
    ink_svc_unlock();
    reschedule();
    return (int)a->id;
}

int ink_alarm_cancel(uint32_t id) {
    ink_svc_lock();
    int i = find_alarm(id);
    ink_alarm_t a = {0};
    bool mine = i >= 0 && (!strcmp(T.a[i].owner, ink_member_current()->id) || ink_member_current()->role == INK_ROLE_ADULT);
    bool ringing = false;
    if (mine) {
        a = T.a[i];
        remove_at(i);
    }
    for (int k = 0; k < MAX_RINGING && mine; k++)
        if (T.rings[k].id == id) {
            T.rings[k].id = 0;
            ringing = true;
        }
    ink_svc_unlock();
    if (!mine) return -1;
    ink_db_del(T.db, id, a.src);
    if (ringing) {
        emit_alarm("alarm.ended", &a, "cancelled");
        rings_changed();
    }
    reschedule();
    return 0;
}

int ink_alarm_next(int *cursor, ink_alarm_kind_t kind, ink_alarm_t *out) {
    const char *me = ink_member_current()->id;
    ink_svc_lock();
    int hit = 0;
    for (int i = *cursor; i < T.n && !hit; i++)
        if (T.a[i].kind == kind && !strcmp(T.a[i].owner, me)) {
            *out = T.a[i];
            *cursor = i + 1;
            hit = 1;
        }
    ink_svc_unlock();
    return hit;
}

/* Stop what rings: id 0 all of it. How many stopped. */
static int stop_rings(uint32_t id, ink_alarm_t *ended, int max) {
    int n = 0;
    ink_svc_lock();
    for (int i = 0; i < MAX_RINGING; i++)
        if (T.rings[i].id && (!id || T.rings[i].id == id)) {
            int k = find_alarm(T.rings[i].id);
            if (k >= 0 && n < max) ended[n++] = T.a[k];
            T.rings[i].id = 0;
        }
    ink_svc_unlock();
    return n;
}

int ink_alarm_dismiss(uint32_t id) {
    ink_alarm_t ended[MAX_RINGING];
    int n = stop_rings(id, ended, MAX_RINGING);
    for (int i = 0; i < n; i++) emit_alarm("alarm.ended", &ended[i], "dismissed");
    rings_changed();
    return n ? 0 : -1;
}

int ink_alarm_snooze(uint32_t id, int minutes) {
    ink_alarm_t ended[MAX_RINGING];
    int n = stop_rings(id, ended, MAX_RINGING);
    if (!n) return -1;
    int64_t until = ink_time_now_s() + (minutes > 0 ? minutes : SNOOZE_MIN) * 60;
    ink_alarm_t updated[MAX_RINGING];
    int nu = 0;
    ink_svc_lock();
    for (int i = 0; i < n; i++) {
        int k = find_alarm(ended[i].id);
        if (k < 0) continue;
        T.a[k].enabled = true;
        T.a[k].snooze_until = until;
        T.a[k].next_s = next_of(&T.a[k], ink_time_now_s());
        updated[nu++] = T.a[k];
    }
    ink_svc_unlock();
    for (int i = 0; i < nu; i++) save_alarm(&updated[i]);
    for (int i = 0; i < n; i++) emit_alarm("alarm.ended", &ended[i], "snoozed");
    rings_changed();
    reschedule();
    return 0;
}

/* ------------------------------------------------------------ start */

static void apply_zone(void *ctx, int64_t v) {
    (void)ctx;
    (void)v;
    ink_state_set_str(T.h_zone, ink_time_zone());
    clock_moved();
}

static const ink_setting_def_t TIME_SETTINGS[] = {
    {.id = "time.zone", .title = "时区", .type = INK_SET_ENUM, .def = 0, .choices = ZONE_NAMES, .apply = apply_zone, .apply_exec = INK_EXEC_DIRECT},
    {.id = "time.auto", .title = "自动校时（听家庭服务器的）", .type = INK_SET_BOOL, .def = 1},
};

static const ink_state_def_t ST_SYNCED = {.id = "time.synced", .type = INK_ST_BOOL, .title = "时间和家庭服务器对过"};
static const ink_state_def_t ST_ZONE = {.id = "time.zone", .type = INK_ST_STR, .title = "时区"};
static const ink_state_def_t ST_MINUTE = {.id = "time.minute", .type = INK_ST_INT, .title = "本地时间（当天第几分钟）"};
static const ink_state_def_t ST_RINGING = {.id = "alarm.ringing", .type = INK_ST_INT, .title = "正在响的闹钟数"};

static void load_alarms(void) {
    T.db = ink_db_open(&ALARMS_DB, NULL);
    if (!T.db) {
        ink_probe_count("time.alarms_unavailable");
        return;
    }
    ink_db_cursor_t cur = {0};
    ink_rec_t r;
    while (T.n < MAX_ALARMS) {
        ink_alarm_t *a = &T.a[T.n];
        if (ink_db_next(T.db, &cur, &r, a, sizeof *a) != 1) break;
        if (r.len != sizeof *a || a->id != r.id) { /* another version's: not guessed at */
            ink_probe_count("time.alarm_record_skipped");
            continue;
        }
        T.n++;
    }
}

int ink_time_init(void) {
    if (ink_settings_define(TIME_SETTINGS, 2) != 0) return -1;
    T.h_synced = ink_state_define(&ST_SYNCED);
    T.h_zone = ink_state_define(&ST_ZONE);
    T.h_minute = ink_state_define(&ST_MINUTE);
    T.h_ringing = ink_state_define(&ST_RINGING);
    ink_state_set_bool(T.h_synced, false);
    ink_state_set_str(T.h_zone, ink_time_zone());
    ink_state_set_int(T.h_ringing, 0);
    if (ink_power_subscribe(on_power, NULL) != 0 || ink_hub_on_online(on_online, NULL) != 0) return -1;
    int64_t rtc_s;
    if (hal_rtc_get(&rtc_s)) {
        osal_set_wall_us(rtc_s * 1000000);
        T.valid = true;
    } else {
        osal_set_wall_us(0); /* not 1970: not known */
        T.valid = false;
        ink_probe_count("time.rtc_invalid");
        ink_hub_demand(); /* the hub knows */
    }
    hal_rtc_clear_alarm(); /* what fired while it was off must not hold the interrupt line down */
    load_alarms();
    if (T.valid) { /* what was due while it was off comes up as late: reported, not rung */
        int64_t now = ink_time_now_s();
        for (int i = 0; i < T.n; i++) /* what is ahead by today's rules and zone: a power cut may have come before it was saved */
            if (T.a[i].enabled && (!T.a[i].next_s || T.a[i].next_s > now)) T.a[i].next_s = next_of(&T.a[i], now);
        publish_minute();
        minute_cb(NULL);
        reschedule();
    }
    return ink_time_caps_init();
}
