/*
 * The calendar's events (calendar-3, see ink/sys/time.h): a PSRAM table kept in the device collection
 * "time.events", run by the time service's one deadline. Repeats, exceptions, the three scopes and the
 * reminder moments are rewritten after Fossify Calendar and Etar, section by section
 * (docs/tasks/calendar-3/SOURCE.md): they are Android apps and cannot be linked.
 */
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "ink/osal.h"
#include "ink/probe.h"
#include "ink/sys/cap.h"
#include "ink/sys/data.h"
#include "ink/sys/members.h"
#include "ink/sys/svc.h"
#include "ink/sys/time.h"
#include "time_internal.h"

#define LATE_OK_S 60
#define NO_LEAD INT32_MIN /* a deadline that only ends a fired single event: its start has come */
#define UNDO_SLOTS 2
#define UNDO_RECORDS (1 + INK_EVENT_EXCEPTIONS)
#define MAX_PERIODS 6000

static const ink_db_def_t EVENTS_DB = {.name = "time.events", .title = "日程", .scope = INK_DB_DEVICE, .schema = 1};

static struct {
    ink_db_t *db;
    ink_event_t e[INK_EVENT_MAX];
    uint32_t rev[INK_EVENT_MAX], next_rev;
    int64_t due[INK_EVENT_MAX], due_start[INK_EVENT_MAX];
    int32_t due_lead[INK_EVENT_MAX];
    int n;
    struct { uint32_t token; int n; char member[16]; ink_event_t rec[UNDO_RECORDS]; } undo[UNDO_SLOTS];
    uint32_t undo_seq;
} E INK_PSRAM_BSS;

/* ------------------------------------------------------------ dates */

static int32_t day_of(int y, int m, int d) { return (int32_t)time_days_from_civil(y, m, d); }
static int wday_of(int32_t day) { return (int)(((day % 7) + 11) % 7); }
static int mdays(int y, int m) { return day_of(m == 12 ? y + 1 : y, m == 12 ? 1 : m + 1, 1) - day_of(y, m, 1); }
int32_t ink_event_day(int64_t utc) { ink_tm_t t; ink_time_to_local(utc, &t); return day_of(t.year, t.month, t.day); }
static int32_t first_day(const ink_event_t *e) { return day_of(e->year, e->month, e->day); }
static int32_t until_day(const ink_event_t *e) { return e->until_year ? day_of(e->until_year, e->until_month, e->until_day) : INT32_MAX; }

/* The nth (1..5) weekday of a month, or the last (nth < 0): Fossify Event.kt:146-180. INT32_MIN: that month has none. */
static int32_t nth_weekday(int y, int m, int nth, int wd) {
    int32_t first = day_of(y, m, 1), last = first + mdays(y, m) - 1;
    if (nth < 0) return last - (wday_of(last) - wd + 7) % 7;
    int32_t d = first + (wd - wday_of(first) + 7) % 7 + (nth - 1) * 7;
    return d <= last ? d : INT32_MIN;
}

static int64_t start_at(const ink_event_t *e, int32_t day) {
    if (e->at_utc && e->freq == INK_FREQ_NONE) return e->at_utc;
    int y, m, d;
    time_civil_from_days(day, &y, &m, &d);
    ink_tm_t tm = {.year = y, .month = m, .day = d, .hour = e->all_day ? 0 : e->hour, .min = e->all_day ? 0 : e->min};
    return ink_time_from_local(&tm);
}

/* ------------------------------------------------------------ repeats (Fossify Event.kt:76-231, EventsHelper.kt:608-758) */

static int32_t by_day(const ink_event_t *e, int y, int m) {
    switch (e->by) {
    case INK_BY_NTH_WEEKDAY: return nth_weekday(y, m, e->nth, e->wday);
    case INK_BY_LAST_WEEKDAY: return nth_weekday(y, m, -1, e->wday);
    case INK_BY_LAST_DAY: return day_of(y, m, mdays(y, m));
    default: return e->rday <= mdays(y, m) ? day_of(y, m, e->rday) : INT32_MIN; /* the 31st, February 29: skipped (Event.kt:112-144) */
    }
}

/* The occurrence days of period p in order; -1 past the last period. */
static int period(const ink_event_t *e, int64_t p, int32_t out[7]) {
    int32_t s = first_day(e);
    int iv = e->interval ? e->interval : 1, y, m, d, n = 0;
    time_civil_from_days(s, &y, &m, &d);
    switch (e->freq) {
    case INK_FREQ_NONE: if (p) return -1; out[0] = s; return 1;
    case INK_FREQ_DAILY: out[0] = (int32_t)(s + p * iv); return 1;
    case INK_FREQ_WEEKLY: { /* weeks start on Monday, as Fossify's isOnProperWeek (Event.kt:215-231) */
        int32_t monday = s - (wday_of(s) + 6) % 7 + (int32_t)(p * 7 * iv);
        for (int i = 0; i < 7; i++) if (e->weekdays & (1u << ((i + 1) % 7))) out[n++] = monday + i;
        return n;
    }
    case INK_FREQ_MONTHLY: { int64_t k = (int64_t)y * 12 + m - 1 + p * iv; y = (int)(k / 12); m = (int)(k % 12) + 1; break; }
    case INK_FREQ_YEARLY: y += (int)(p * iv); m = e->rmonth; break;
    case INK_FREQ_LUNAR_YEARLY: { /* a leap-month date falls in the plain month in most years; the 30th in a short month is its last day */
        ink_lunar_t l;
        if (!ink_lunar_from_solar(y, m, d, &l)) return -1;
        int ly = l.year + (int)(p * iv);
        if (ly > 2100) return -1;
        bool leap = e->lleap && ink_lunar_leap_month(ly) == e->lmonth;
        int last = ink_lunar_month_days(ly, e->lmonth, leap);
        ink_lunar_t want = {ly, e->lmonth, e->lday <= last ? e->lday : last, leap};
        if (last && ink_lunar_to_solar(&want, &y, &m, &d)) out[n++] = day_of(y, m, d);
        return n;
    }
    default: return -1;
    }
    if (y > 2100) return -1;
    int32_t day = by_day(e, y, m);
    if (day != INT32_MIN) out[n++] = day;
    return n;
}

/* Periods that surely end before `from`: skipped when nothing is counted. */
static int64_t skip(const ink_event_t *e, int32_t from) {
    int32_t s = first_day(e);
    int iv = e->interval ? e->interval : 1, sy, sm, sd, fy, fm, fd;
    time_civil_from_days(s, &sy, &sm, &sd);
    time_civil_from_days(from, &fy, &fm, &fd);
    switch (e->freq) {
    case INK_FREQ_DAILY: return (from - s) / iv;
    case INK_FREQ_WEEKLY: return (from - (s - (wday_of(s) + 6) % 7)) / (7 * iv);
    case INK_FREQ_MONTHLY: return ((fy * 12 + fm) - (sy * 12 + sm)) / iv;
    case INK_FREQ_YEARLY: case INK_FREQ_LUNAR_YEARLY: return (fy - sy) / iv > 0 ? (fy - sy) / iv - 1 : 0;
    default: return 0;
    }
}

typedef bool (*walk_fn)(void *ctx, int32_t day, int index);
/* Each occurrence day from `from` on, in order, with its index (right when from <= the first day), by until and
 * count but with the exceptions still in, until fn says stop. */
static void walk(const ink_event_t *e, int32_t from, walk_fn fn, void *ctx) {
    int32_t s = first_day(e), last = until_day(e), days[7];
    int64_t p = !e->count && from > s ? skip(e, from) : 0;
    int index = 0;
    for (int guard = 0; guard < MAX_PERIODS; guard++, p++) {
        int n = period(e, p, days);
        if (n < 0) return;
        for (int i = 0; i < n; i++) {
            if (days[i] < s) continue;
            if (days[i] > last || (e->count && index >= e->count)) return;
            int k = index++;
            if (days[i] >= from && !fn(ctx, days[i], k)) return;
        }
    }
}

static bool excepted(const ink_event_t *e, int32_t day) {
    for (int i = 0; i < e->nex && i < INK_EVENT_EXCEPTIONS; i++) if (e->ex[i] == day) return true;
    return false;
}

struct find { const ink_event_t *e; int64_t earliest, hit; int32_t day; int index; };
static bool find_cb(void *ctx, int32_t day, int index) {
    struct find *f = ctx;
    if (f->day) { if (day < f->day) return true; f->index = day == f->day ? index : -1; return false; }
    if (excepted(f->e, day)) return true;
    int64_t s = start_at(f->e, day);
    if (s < f->earliest) return true;
    f->hit = s;
    return false;
}
static int64_t first_start(const ink_event_t *e, int64_t earliest) {
    struct find f = {e, earliest, 0, 0, -1};
    walk(e, ink_event_day(earliest) - 1, find_cb, &f);
    return f.hit;
}
int64_t ink_event_occurrence(const ink_event_t *e, int64_t from) {
    return first_start(e, from - (e->duration_s > 0 ? e->duration_s - 1 : 0));
}
int ink_event_index(const ink_event_t *e, int32_t day) {
    struct find f = {e, 0, 0, day, -1};
    walk(e, first_day(e), find_cb, &f);
    return f.index;
}

/* ------------------------------------------------------------ reminder moments (Fossify Context.kt:196-226) */

/* The next moment after `after` a reminder goes off: start minus lead (an all-day event starts at its midnight). */
static int64_t next_fire(const ink_event_t *e, int64_t after, int32_t *lead, int64_t *start) {
    int64_t best = 0;
    for (int i = 0; i < e->nleads && i < INK_EVENT_LEADS; i++) {
        int64_t s = first_start(e, after + (int64_t)e->leads[i] * 60 + 1);
        if (s && (!best || s - (int64_t)e->leads[i] * 60 < best)) { best = s - (int64_t)e->leads[i] * 60; *lead = e->leads[i]; *start = s; }
    }
    return best;
}

static bool expires(const ink_event_t *e) { return e->freq == INK_FREQ_NONE && !e->duration_s && e->nleads; }

static void plan(int i, int64_t after) { /* under the lock */
    ink_event_t *e = &E.e[i];
    int32_t lead = 0;
    int64_t start = 0, due = ink_time_valid() ? next_fire(e, after, &lead, &start) : 0;
    if (!due && expires(e) && ink_time_valid() && (start = start_at(e, first_day(e))) > after) { due = start; lead = NO_LEAD; }
    E.due[i] = due; E.due_lead[i] = lead; E.due_start[i] = start;
}

/* ------------------------------------------------------------ the table */

static int find(uint32_t id) { /* under the lock */
    for (int i = 0; i < E.n; i++) if (E.e[i].id == id) return i;
    return -1;
}
static uint32_t changed(int i) { if (!++E.next_rev) ++E.next_rev; return E.rev[i] = E.next_rev; }
static void remove_at(int i) {
    int tail = E.n - i - 1;
    memmove(&E.e[i], &E.e[i + 1], sizeof E.e[0] * (size_t)tail); memmove(&E.rev[i], &E.rev[i + 1], sizeof E.rev[0] * (size_t)tail);
    memmove(&E.due[i], &E.due[i + 1], sizeof E.due[0] * (size_t)tail); memmove(&E.due_start[i], &E.due_start[i + 1], sizeof E.due_start[0] * (size_t)tail);
    memmove(&E.due_lead[i], &E.due_lead[i + 1], sizeof E.due_lead[0] * (size_t)tail);
    E.n--;
}
static int store(const ink_event_t *e) {
    ink_db_write_t w = {.id = e->id, .sort = e->at_utc ? e->at_utc : first_day(e) * 86400LL, .data = e, .len = sizeof *e, .src = e->src};
    return E.db && ink_db_put(E.db, &w, NULL) == INK_DB_OK ? 0 : INK_EVENT_STORAGE;
}
/* Into the table (replacing the same id), planned from now. Under the lock. */
static uint32_t put(const ink_event_t *e) {
    int i = find(e->id);
    if (i < 0) { if (E.n >= INK_EVENT_MAX) return 0; i = E.n++; }
    E.e[i] = *e;
    plan(i, e->done_s > 0 ? e->done_s : ink_time_now_s());
    return changed(i);
}

/* Check and fill in the rule from the start (Fossify EventActivity.kt:1591-1610; Etar RecurrencePickerDialog.java:81-84). */
static int normalize(ink_event_t *e) {
    if (!e->title[0]) return INK_EVENT_NO_TITLE;
    if (e->at_utc && e->freq == INK_FREQ_NONE) {
        ink_tm_t t; ink_time_to_local(e->at_utc, &t);
        e->year = (int16_t)t.year; e->month = (uint8_t)t.month; e->day = (uint8_t)t.day; e->hour = (uint8_t)t.hour; e->min = (uint8_t)t.min;
    } else e->at_utc = 0;
    int y, m, d;
    time_civil_from_days(first_day(e), &y, &m, &d);
    if (e->year < 1901 || e->year > 2100 || y != e->year || m != e->month || d != e->day || e->hour > 23 || e->min > 59) return INK_EVENT_INVALID;
    if (e->all_day) e->hour = e->min = 0;
    if (!e->interval) e->interval = 1;
    if (e->interval > 99 || e->count > 730 || e->nleads > INK_EVENT_LEADS || e->nex > INK_EVENT_EXCEPTIONS || e->freq > INK_FREQ_LUNAR_YEARLY || e->by > INK_BY_LAST_DAY) return INK_EVENT_INVALID;
    if (e->duration_s < 0) return INK_EVENT_END_BEFORE;
    if (e->until_year) {
        time_civil_from_days(until_day(e), &y, &m, &d);
        if (y != e->until_year || m != e->until_month || d != e->until_day) return INK_EVENT_INVALID;
        if (until_day(e) < first_day(e)) return INK_EVENT_UNTIL_BEFORE;
    }
    for (int i = 0; i < e->nleads; i++) if (e->leads[i] < (e->all_day ? -1439 : 0) || e->leads[i] > 40320) return INK_EVENT_INVALID;
    if (!e->rday) e->rday = e->day;
    if (!e->rmonth) e->rmonth = e->month;
    if ((e->by == INK_BY_NTH_WEEKDAY || e->by == INK_BY_LAST_WEEKDAY) && !e->nth) {
        e->wday = (uint8_t)wday_of(first_day(e));
        e->nth = (int8_t)(e->by == INK_BY_NTH_WEEKDAY ? (e->day - 1) / 7 + 1 : -1);
    }
    if (e->by == INK_BY_NTH_WEEKDAY && (e->nth < 1 || e->nth > 5 || e->wday > 6)) return INK_EVENT_INVALID;
    if (e->freq == INK_FREQ_WEEKLY && !(e->weekdays & 0x7F)) e->weekdays = (uint8_t)(1u << wday_of(first_day(e)));
    if (e->freq == INK_FREQ_LUNAR_YEARLY && !e->lmonth) {
        ink_lunar_t l;
        if (!ink_lunar_from_solar(e->year, e->month, e->day, &l)) return INK_EVENT_INVALID;
        e->lmonth = (uint8_t)l.month; e->lday = (uint8_t)l.day; e->lleap = l.leap;
    }
    if (e->freq == INK_FREQ_LUNAR_YEARLY && (e->lmonth < 1 || e->lmonth > 12 || e->lday < 1 || e->lday > 30)) return INK_EVENT_INVALID;
    if (e->freq == INK_FREQ_NONE) { e->count = 0; e->until_year = 0; e->until_month = e->until_day = 0; e->interval = 1; }
    return 0;
}

static bool member_io(const char *member) {
    time_edits_lock();
    if (ink_member_io_begin(member)) return true;
    time_edits_unlock();
    return false;
}
static void member_io_end(const char *member) { time_edits_unlock(); ink_member_io_end(member); time_reschedule(); }

static int add_unlocked(ink_event_t *e, uint32_t *written, const char *member) {
    snprintf(e->owner, sizeof e->owner, "%s", member);
    int64_t now = ink_time_now_s();
    e->modified_s = e->done_s = now;
    e->id = 0;
    int r = normalize(e);
    if (r) return r;
    if (now && expires(e) && start_at(e, first_day(e)) <= now) return INK_EVENT_PAST; /* it would go now and be gone */
    ink_svc_lock();
    bool full = E.n >= INK_EVENT_MAX;
    ink_svc_unlock();
    if (full) return INK_EVENT_FULL;
    e->id = time_new_id();
    if (!e->id) return INK_EVENT_STORAGE;
    if (!e->uid[0]) snprintf(e->uid, sizeof e->uid, "ink-%08x-%u@calendar", (unsigned)(now & 0xffffffff), (unsigned)e->id);
    if ((r = store(e)) != 0) return r;
    ink_svc_lock();
    uint32_t rev = put(e);
    ink_svc_unlock();
    if (written) *written = rev;
    return (int)e->id;
}

int ink_time_event_add_for(ink_event_t *e, uint32_t *written, const char *member) {
    if (written) *written = 0;
    if (!e || !member || !member[0] || !E.db) return INK_EVENT_INVALID;
    if (!member_io(member)) return INK_EVENT_GONE;
    int r = add_unlocked(e, written, member);
    member_io_end(member);
    return r;
}

static int get_unlocked(uint32_t id, ink_event_t *out, const char *member) {
    ink_svc_lock();
    int i = find(id);
    bool ok = i >= 0 && (!member || !strcmp(E.e[i].owner, member));
    if (ok) *out = E.e[i];
    ink_svc_unlock();
    return ok ? 0 : INK_EVENT_NOT_FOUND;
}
int ink_time_event_get_for(uint32_t id, ink_event_t *out, const char *member) { return get_unlocked(id, out, member); }
int ink_event_get(uint32_t id, ink_event_t *out) {
    char member[16];
    ink_svc_lock(); snprintf(member, sizeof member, "%s", ink_member_current()->id); ink_svc_unlock();
    return get_unlocked(id, out, member);
}

int ink_time_event_next_for(int *cursor, ink_event_t *out, const char *member) {
    int hit = 0;
    ink_svc_lock();
    for (int i = *cursor; i < E.n && !hit; i++)
        if (!strcmp(E.e[i].owner, member)) { *out = E.e[i]; *cursor = i + 1; hit = 1; }
    ink_svc_unlock();
    return hit;
}
int ink_event_next(int *cursor, ink_event_t *out) {
    char member[16];
    ink_svc_lock(); snprintf(member, sizeof member, "%s", ink_member_current()->id); ink_svc_unlock();
    return ink_member_exists(member) ? ink_time_event_next_for(cursor, out, member) : -1;
}

/* Write back one record and put it in the table. */
static int rewrite(ink_event_t *e) {
    e->done_s = ink_time_now_s();
    int r = store(e);
    if (r) return r;
    ink_svc_lock(); put(e); ink_svc_unlock();
    return 0;
}
static int erase(uint32_t id, uint8_t src) {
    if (ink_db_del(E.db, id, src) != INK_DB_OK) return INK_EVENT_STORAGE;
    ink_svc_lock();
    int i = find(id);
    if (i >= 0) remove_at(i);
    ink_svc_unlock();
    return 0;
}

static int add_exception(ink_event_t *e, int32_t day) {
    if (excepted(e, day)) return 0;
    if (e->nex >= INK_EVENT_EXCEPTIONS) return INK_EVENT_EXCEPTIONS_FULL;
    e->ex[e->nex++] = day;
    return 0;
}

/* Cut a series before the occurrence on `day` (Fossify EventsHelper.kt:391-416; Etar EditEventHelper.java:414-456):
 * the count split by the occurrences that came before, or else an end date the day before. */
static void cut_before(ink_event_t *e, int32_t day, int index) {
    int y, m, d;
    time_civil_from_days(day - 1, &y, &m, &d);
    if (e->count) e->count = (uint16_t)index;
    else { e->until_year = (int16_t)y; e->until_month = (uint8_t)m; e->until_day = (uint8_t)d; }
    int k = 0;
    for (int i = 0; i < e->nex; i++) if (e->ex[i] < day) e->ex[k++] = e->ex[i];
    e->nex = (uint8_t)k;
}

int ink_time_event_update_for(ink_event_t *e, int64_t occurrence, ink_scope_t scope, const char *member, uint32_t *new_id) {
    if (new_id) *new_id = 0;
    if (!e || !member) return INK_EVENT_INVALID;
    if (!member_io(member)) return INK_EVENT_GONE;
    ink_event_t old;
    int r = get_unlocked(e->id, &old, member);
    int32_t occ = occurrence ? ink_event_day(occurrence) : first_day(&old);
    int index = r ? -1 : ink_event_index(&old, occ);
    if (!r && old.freq == INK_FREQ_NONE) scope = INK_SCOPE_ALL;
    if (!r && scope != INK_SCOPE_ALL && (index < 0 || excepted(&old, occ))) r = INK_EVENT_NOT_FOUND;
    if (!r && scope == INK_SCOPE_FOLLOWING && index == 0) scope = INK_SCOPE_ALL; /* the first one: the whole series (Etar :432-438) */
    snprintf(e->owner, sizeof e->owner, "%s", member);
    e->modified_s = e->done_s = ink_time_now_s();
    if (!r && scope == INK_SCOPE_ALL) {
        if (old.freq != INK_FREQ_NONE && occurrence) { /* keep the series' first day, moved as the occurrence was (EventsHelper.kt:229-240) */
            int32_t moved = first_day(e) - occ + first_day(&old);
            int y, m, d; time_civil_from_days(moved, &y, &m, &d);
            e->year = (int16_t)y; e->month = (uint8_t)m; e->day = (uint8_t)d;
            if (e->freq == old.freq) { e->rday = 0; e->rmonth = 0; e->nth = 0; e->lmonth = 0; }
        }
        e->parent = old.parent; e->orig_day = old.orig_day; e->orig_min = old.orig_min;
        memcpy(e->uid, old.uid, sizeof e->uid);
        if (e->freq == old.freq) { memcpy(e->ex, old.ex, sizeof e->ex); e->nex = old.nex; }
        r = normalize(e);
        if (!r) r = rewrite(e);
    } else if (!r) {
        ink_event_t series = old;
        if (scope == INK_SCOPE_THIS) { /* the series loses that day, the occurrence is kept apart (EventsHelper.kt:242-278) */
            r = add_exception(&series, occ);
            e->parent = old.id; e->freq = INK_FREQ_NONE; e->count = 0; e->until_year = 0; e->nex = 0;
            e->orig_day = occ; e->orig_min = old.all_day ? 0 : old.hour * 60 + old.min;
            memcpy(e->uid, old.uid, sizeof e->uid);
        } else { /* this and the following: the old series ends the day before, a new one starts (EventsHelper.kt:280-305) */
            cut_before(&series, occ, index);
            if (old.count && e->count == old.count) e->count = (uint16_t)(old.count - index);
            e->parent = 0; e->uid[0] = 0;
            int k = 0;
            for (int i = 0; i < old.nex; i++) if (old.ex[i] >= occ) e->ex[k++] = old.ex[i];
            e->nex = (uint8_t)k;
        }
        e->rday = 0; e->rmonth = 0; e->nth = 0; e->lmonth = 0;
        if (!r) r = normalize(e);
        if (!r) { ink_event_t added = *e; r = add_unlocked(&added, NULL, member); if (r > 0) { e->id = added.id; if (new_id) *new_id = added.id; r = 0; } }
        if (!r) r = rewrite(&series);
    }
    member_io_end(member);
    return r;
}

static int keep_undo(const ink_event_t *recs, int n, const char *member) {
    int slot = (int)(E.undo_seq++ % UNDO_SLOTS);
    E.undo[slot].n = n;
    E.undo[slot].token = E.undo_seq;
    snprintf(E.undo[slot].member, sizeof E.undo[slot].member, "%s", member);
    memcpy(E.undo[slot].rec, recs, sizeof recs[0] * (size_t)n);
    return (int)E.undo[slot].token;
}

int ink_time_event_delete_for(uint32_t id, uint32_t revision, int64_t occurrence, ink_scope_t scope, const char *member, bool adult, uint32_t *undo) {
    if (undo) *undo = 0;
    if (!member) return INK_EVENT_INVALID;
    if (!member_io(member)) return INK_EVENT_GONE;
    ink_event_t *before = osal_malloc(sizeof(ink_event_t) * UNDO_RECORDS, OSAL_MEM_EXTERNAL), series = {0};
    int n = 0, r = before ? 0 : INK_EVENT_STORAGE;
    ink_svc_lock();
    int i = find(id);
    if (!r && (i < 0 || (strcmp(E.e[i].owner, member) && !adult))) r = INK_EVENT_NOT_FOUND;
    else if (!r && revision && E.rev[i] != revision) r = INK_EVENT_STALE;
    if (!r) series = before[n++] = E.e[i];
    ink_svc_unlock();
    int32_t occ = !r && occurrence ? ink_event_day(occurrence) : 0;
    int index = !r && occ ? ink_event_index(&series, occ) : 0;
    if (!r && (series.freq == INK_FREQ_NONE || (scope == INK_SCOPE_FOLLOWING && index == 0))) scope = INK_SCOPE_ALL;
    if (!r && scope != INK_SCOPE_ALL && (!occ || index < 0)) r = INK_EVENT_NOT_FOUND;
    if (!r) { /* the edited occurrences that go with it (Fossify deletes a series' children with it) */
        ink_svc_lock();
        for (int k = 0; k < E.n && n < UNDO_RECORDS; k++)
            if (E.e[k].parent == id && E.e[k].id != id && (scope == INK_SCOPE_ALL || (scope == INK_SCOPE_FOLLOWING && E.e[k].orig_day >= occ))) before[n++] = E.e[k];
        ink_svc_unlock();
    }
    if (!r && scope == INK_SCOPE_THIS) { r = add_exception(&series, occ); if (!r) r = rewrite(&series); } /* EventsHelper.kt:428-452 */
    else if (!r && scope == INK_SCOPE_FOLLOWING) { cut_before(&series, occ, index); r = rewrite(&series); }
    else if (!r) r = erase(id, series.src);
    for (int k = 1; !r && k < n; k++) r = erase(before[k].id, before[k].src);
    if (!r && undo) *undo = (uint32_t)keep_undo(before, n, member);
    osal_free(before);
    member_io_end(member);
    return r;
}

/* The inverse of a delete: every record as it was, the same ids. */
int ink_time_event_restore(uint32_t token, const char *member) {
    if (!token || !member) return INK_EVENT_INVALID;
    if (!member_io(member)) return INK_EVENT_GONE;
    int r = INK_EVENT_STALE;
    for (int s = 0; s < UNDO_SLOTS; s++)
        if (E.undo[s].token == token && !strcmp(E.undo[s].member, member)) {
            r = 0;
            for (int k = 0; k < E.undo[s].n && !r; k++) r = rewrite(&E.undo[s].rec[k]);
            if (!r) E.undo[s].token = 0;
        }
    member_io_end(member);
    return r;
}

/* ------------------------------------------------------------ the old reminder calls */

static void cut_title(char *out, size_t cap, const char *in) { /* on a character, never half of one */
    size_t n = strlen(in);
    if (n >= cap) { n = cap - 1; while (n && ((unsigned char)in[n] & 0xC0) == 0x80) n--; }
    memcpy(out, in, n);
    out[n] = 0;
}

static void project(const ink_event_t *e, ink_alarm_t *a, int64_t now) {
    static const uint8_t REPEATS[] = {INK_REPEAT_NONE, INK_REPEAT_WEEKDAYS, INK_REPEAT_WEEKDAYS, INK_REPEAT_MONTHLY, INK_REPEAT_YEARLY, INK_REPEAT_LUNAR_YEARLY};
    memset(a, 0, sizeof *a);
    a->id = e->id; a->kind = INK_REMINDER; a->src = e->src; a->enabled = true;
    a->repeat = REPEATS[e->freq % 6]; a->weekdays = e->freq == INK_FREQ_DAILY ? 0x7F : e->weekdays;
    a->hour = e->hour; a->min = e->min; a->year = e->year; a->month = e->freq == INK_FREQ_YEARLY ? e->rmonth : e->month;
    a->day = e->freq == INK_FREQ_MONTHLY || e->freq == INK_FREQ_YEARLY ? e->rday : e->day;
    a->lmonth = e->lmonth; a->lday = e->lday; a->lleap = e->lleap; a->at_utc = e->at_utc;
    a->next_s = now ? first_start(e, now + 1) : 0;
    memcpy(a->owner, e->owner, sizeof a->owner);
    cut_title(a->title, sizeof a->title, e->title);
    a->silent = !e->nleads; a->all_day = e->all_day; a->duration_s = e->duration_s;
}

int time_events_project(int *cursor, ink_alarm_t *out, const char *member) {
    ink_event_t e;
    int hit = ink_time_event_next_for(cursor, &e, member);
    if (hit == 1) project(&e, out, ink_time_now_s());
    return hit;
}

bool time_events_occurrence(uint32_t id, int64_t from, int64_t *out) {
    ink_event_t e;
    if (get_unlocked(id, &e, NULL)) return false;
    *out = ink_event_occurrence(&e, from);
    return true;
}

/* An ink_alarm_t reminder as an event: the same time, repeat, ring, all-day and end. */
int time_events_from_alarm(const ink_alarm_t *a, ink_event_t *e) {
    memset(e, 0, sizeof *e);
    e->id = a->id; e->src = a->src; e->all_day = a->all_day; e->duration_s = a->duration_s; e->at_utc = a->repeat == INK_REPEAT_NONE ? a->at_utc : 0;
    memcpy(e->owner, a->owner, sizeof a->owner); e->owner[sizeof e->owner - 1] = 0;
    cut_title(e->title, sizeof e->title, a->title);
    if (!e->title[0]) snprintf(e->title, sizeof e->title, "%s", "提醒");
    if (!a->silent) e->nleads = 1;
    e->hour = a->hour; e->min = a->min; e->year = a->year; e->month = a->month; e->day = a->day;
    switch (a->repeat) {
    case INK_REPEAT_WEEKDAYS: e->freq = (a->weekdays & 0x7F) == 0x7F ? INK_FREQ_DAILY : INK_FREQ_WEEKLY; e->weekdays = a->weekdays & 0x7F; break;
    case INK_REPEAT_MONTHLY: e->freq = INK_FREQ_MONTHLY; e->rday = a->day; break;
    case INK_REPEAT_YEARLY: e->freq = INK_FREQ_YEARLY; e->rmonth = a->month; e->rday = a->day; break;
    case INK_REPEAT_LUNAR_YEARLY: e->freq = INK_FREQ_LUNAR_YEARLY; e->lmonth = a->lmonth; e->lday = a->lday; e->lleap = a->lleap; break;
    default: break;
    }
    if (e->at_utc || a->year) return 0;
    /* no date: the next time it comes after now (or after its saved next time) */
    int64_t now = a->next_s ? a->next_s - 1 : ink_time_now_s();
    ink_tm_t t; ink_time_to_local(now, &t);
    e->year = (int16_t)t.year; e->month = (uint8_t)t.month; e->day = (uint8_t)t.day;
    if (e->freq == INK_FREQ_MONTHLY || e->freq == INK_FREQ_YEARLY) e->day = 1;
    if (e->freq == INK_FREQ_YEARLY) e->month = 1;
    if (!e->rday) e->rday = a->day;
    if (!e->rmonth) e->rmonth = a->month;
    int64_t first = first_start(e, now + 1);
    if (!first && e->freq == INK_FREQ_NONE) { /* the hour has gone today: tomorrow */
        time_civil_from_days(first_day(e) + 1, &t.year, &t.month, &t.day);
        e->year = (int16_t)t.year; e->month = (uint8_t)t.month; e->day = (uint8_t)t.day;
        first = first_start(e, now + 1);
    }
    if (!first) return INK_EVENT_PAST;
    ink_time_to_local(first, &t);
    e->year = (int16_t)t.year; e->month = (uint8_t)t.month; e->day = (uint8_t)t.day;
    return 0;
}

/* Boot: a reminder still in time.alarms moves here with its id (D-138). Again after a power cut: the same. */
bool time_events_adopt(const ink_alarm_t *legacy) {
    ink_event_t e;
    ink_svc_lock();
    bool there = find(legacy->id) >= 0;
    ink_svc_unlock();
    if (there) return true;
    if (time_events_from_alarm(legacy, &e) || normalize(&e)) { ink_probe_count("time.event_migrate_skipped"); return false; }
    e.done_s = legacy->next_s > 0 ? legacy->next_s - 1 : ink_time_now_s();
    snprintf(e.uid, sizeof e.uid, "ink-00000000-%u@calendar", (unsigned)e.id);
    if (store(&e)) return false;
    ink_svc_lock();
    bool ok = put(&e) != 0;
    ink_svc_unlock();
    if (ok) ink_probe_count("time.event_migrated");
    return ok;
}

/* ------------------------------------------------------------ the service's side */

int time_events_open(void) {
    E.db = ink_db_open(&EVENTS_DB, NULL);
    if (!E.db) { ink_probe_count("time.events_unavailable"); return -1; }
    ink_db_cursor_t cur = {0};
    ink_rec_t r;
    while (E.n < INK_EVENT_MAX) {
        ink_event_t *e = &E.e[E.n];
        memset(e, 0, sizeof *e);
        if (ink_db_next(E.db, &cur, &r, e, sizeof *e) != 1) break;
        if (r.len != sizeof *e || e->id != r.id) { ink_probe_count("time.event_record_skipped"); continue; }
        changed(E.n++);
    }
    return 0;
}

uint32_t time_events_max_id(void) {
    uint32_t max = 0;
    ink_svc_lock();
    for (int i = 0; i < E.n; i++) if (E.e[i].id > max) max = E.e[i].id;
    ink_svc_unlock();
    return max;
}

void time_events_replan(int64_t now, bool all) {
    ink_svc_lock();
    for (int i = 0; i < E.n; i++) { /* boot: from where each left off (missed ones come up); the clock moved: never before what already went off */
        int64_t done = E.e[i].done_s;
        if (all) plan(i, done > 0 && done < now ? done : now);
        else if (!E.due[i] || E.due[i] > now) plan(i, done > now ? done : now);
    }
    ink_svc_unlock();
}

int64_t time_events_earliest(void) {
    int64_t next = 0;
    ink_svc_lock();
    for (int i = 0; i < E.n; i++) if (E.due[i] && (!next || E.due[i] < next)) next = E.due[i];
    ink_svc_unlock();
    return next;
}

static void emit(const char *event, const ink_event_t *e, int64_t start, int32_t lead, const char *why) {
    if (!ink_member_exists(e->owner)) return;
    ink_args_t *args = osal_malloc(sizeof *args, OSAL_MEM_EXTERNAL);
    if (!args) return;
    char title[48], at[24];
    ink_tm_t t;
    ink_time_to_local(start, &t);
    if (e->all_day) snprintf(at, sizeof at, "%04d-%02d-%02d", t.year, t.month, t.day);
    else snprintf(at, sizeof at, "%04d-%02d-%02d %02d:%02d", t.year, t.month, t.day, t.hour, t.min);
    cut_title(title, sizeof title, e->title);
    ink_args_init(args);
    ink_args_int(args, "id", e->id);
    ink_args_str(args, "title", title);
    ink_args_str(args, "full_title", e->title);
    ink_args_str(args, "at", at);
    ink_args_int(args, "lead_min", lead);
    ink_args_str(args, "owner", e->owner);
    if (e->owner[0]) ink_args_str(args, "member", e->owner);
    if (why) ink_args_str(args, "why", why);
    time_emit_for(event, args, e->owner);
    osal_free(args);
}

/* What is due goes off (or is reported missed), one at a time; each plans its next. Timer task. */
void time_events_due(int64_t now) {
    for (int round = 0; round < 4 * INK_EVENT_MAX; round++) {
        ink_event_t e = {0};
        int32_t lead = 0;
        int64_t start = 0, due = 0;
        bool hit = false, gone = false, late = false;
        ink_svc_lock();
        for (int i = 0; i < E.n && !hit; i++) {
            if (!E.due[i] || E.due[i] > now) continue;
            hit = true; due = E.due[i]; lead = E.due_lead[i]; start = E.due_start[i];
            late = now - due > LATE_OK_S;
            E.e[i].done_s = late ? now : due;
            if (lead != NO_LEAD) plan(i, E.e[i].done_s);
            gone = lead == NO_LEAD || (expires(&E.e[i]) && !E.due[i] && start <= now);
            e = E.e[i];
            if (gone) remove_at(i);
            else changed(i);
        }
        ink_svc_unlock();
        if (!hit) return;
        if (gone) { if (E.db) ink_db_del(E.db, e.id, e.src); }
        else if (store(&e)) ink_probe_count("time.save_failed");
        if (lead == NO_LEAD) continue;
        ink_probe_value("time.reminder_late_ms", (double)(osal_wall_us() - due * 1000000) / 1000.0);
        ink_probe_count(late ? "time.missed" : "time.reminders_fired");
        emit(late ? "reminder.missed" : "reminder.fired", &e, start, lead, late ? "the device was off or the clock moved" : NULL);
    }
}

int time_events_forget(const char *member) {
    for (;;) {
        uint32_t id = 0;
        ink_svc_lock();
        for (int i = 0; i < E.n && !id; i++) if (!strcmp(E.e[i].owner, member)) id = E.e[i].id;
        ink_svc_unlock();
        if (!id) break;
        if (erase(id, INK_SRC_SYSTEM)) return -1;
    }
    for (int s = 0; s < UNDO_SLOTS; s++) if (!strcmp(E.undo[s].member, member)) memset(&E.undo[s], 0, sizeof E.undo[s]);
    return E.db && ink_db_wait(E.db, ink_db_last_ver(E.db), 2000000) == 0 ? 0 : -1;
}
