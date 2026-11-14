/*
 * The time service on the soft bus: what the clock and calendar apps' buttons do, and what the AI can
 * do. None of it blocks: the table is in memory and writes are queued.
 */
#include <stdio.h>
#include <string.h>

#include "ink/sys/i18n.h"
#include "ink/osal.h"
#include "ink/sys/cap.h"
#include "ink/sys/json.h"
#include "ink/sys/members.h"
#include "ink/sys/state.h"
#include "ink/sys/time.h"
#include "ink/sys/almanac.h"
#include "ink/sys/svc.h"
#include "ink/sys/file.h"
#include "time_internal.h"

#define UNDO_MS 10000

static void fail(ink_call_t *c, ink_rc_t st, const char *why) {
    c->rc.status = st;
    snprintf(c->rc.detail, sizeof c->rc.detail, "%s", why);
}

static void local_text(int64_t utc, char *out, size_t cap) {
    ink_tm_t t;
    ink_time_to_local(utc, &t);
    snprintf(out, cap, "%04d-%02d-%02d %02d:%02d:%02d", t.year, t.month, t.day, t.hour, t.min, t.sec);
}

static void cap_now(ink_call_t *c) {
    if (!ink_time_valid()) {
        fail(c, INK_RC_FAILED, "the clock is not known yet");
        return;
    }
    int64_t now = ink_time_now_s();
    ink_tm_t t;
    ink_time_to_local(now, &t);
    char text[24], lunar[32] = "", gz[12] = "", out[256];
    local_text(now, text, sizeof text);
    ink_lunar_t l;
    bool has_lunar = ink_lunar_from_solar(t.year, t.month, t.day, &l);
    if (has_lunar) {
        ink_lunar_name(&l, lunar, sizeof lunar);
        ink_lunar_ganzhi(l.year, gz, sizeof gz);
    }
    int64_t synced = 0;
    ink_state_get_int(ink_state_find("time.synced"), &synced);
    snprintf(out, sizeof out, "{\"utc_s\":%lld,\"local\":\"%s\",\"weekday\":%d,\"zone\":\"%s\",\"offset_min\":%d,\"lunar\":\"%s\",\"ganzhi\":\"%s\",\"synced\":%s}",
             (long long)now, text, t.wday, ink_time_zone(), t.offset_min, lunar, gz, synced ? "true" : "false");
    ink_cap_result(c, out);
    snprintf(c->rc.say, sizeof c->rc.say, TR("现在是 %.16s"), text);
}

static void cap_set(ink_call_t *c) {
    int64_t s = 0;
    ink_args_get_int(&c->args, "utc_s", &s);
    int64_t before = ink_time_now_s();
    bool valid = ink_time_valid();
    int64_t expected = 0;
    ink_args_get_int(&c->args, "if_revision", &expected);
    if (ink_time_set_if_revision(s, false, (uint32_t)expected, &c->rc.revision) != 0) { c->rc.status = INK_RC_STALE; return; }
    snprintf(c->rc.object, sizeof c->rc.object, "time:clock");
    if (valid) {
        ink_args_t *undo = osal_malloc(sizeof *undo, OSAL_MEM_EXTERNAL);
        if (undo) {
            ink_args_init(undo);
            ink_args_int(undo, "utc_s", before);
            ink_args_int(undo, "if_revision", c->rc.revision);
            ink_cap_undoable(c, "time.set", undo, 10000);
            osal_free(undo);
        }
    }
    snprintf(c->rc.say, sizeof c->rc.say, TR("时间改好了"));
}

static void cap_lunar(ink_call_t *c) {
    int64_t y = 0, m = 0, d = 0, ly = 0, lm = 0, ld = 0;
    bool leap = false;
    ink_args_get_bool(&c->args, "leap", &leap);
    char out[160], name[32] = "", gz[12] = "";
    if (ink_args_get_int(&c->args, "lunar_year", &ly) && ink_args_get_int(&c->args, "lunar_month", &lm) && ink_args_get_int(&c->args, "lunar_day", &ld)) {
        ink_lunar_t l = {(int)ly, (int)lm, (int)ld, leap};
        int sy, sm, sd;
        if (!ink_lunar_to_solar(&l, &sy, &sm, &sd)) {
            fail(c, INK_RC_INVALID, "no such lunar date");
            return;
        }
        snprintf(out, sizeof out, "{\"year\":%d,\"month\":%d,\"day\":%d}", sy, sm, sd);
    } else {
        ink_tm_t t = {0};
        if (!ink_args_get_int(&c->args, "year", &y) || !ink_args_get_int(&c->args, "month", &m) || !ink_args_get_int(&c->args, "day", &d)) {
            if (!ink_time_valid()) {
                fail(c, INK_RC_FAILED, "the clock is not known yet");
                return;
            }
            ink_time_to_local(ink_time_now_s(), &t);
            y = t.year;
            m = t.month;
            d = t.day;
        }
        ink_lunar_t l;
        if (!ink_lunar_from_solar((int)y, (int)m, (int)d, &l)) {
            fail(c, INK_RC_INVALID, "outside the calendar (1901-2100)");
            return;
        }
        ink_lunar_name(&l, name, sizeof name);
        ink_lunar_ganzhi(l.year, gz, sizeof gz);
        snprintf(out, sizeof out, "{\"lunar_year\":%d,\"lunar_month\":%d,\"lunar_day\":%d,\"leap\":%s,\"name\":\"%s\",\"ganzhi\":\"%s\",\"zodiac\":\"%s\"}", l.year, l.month, l.day,
                 l.leap ? "true" : "false", name, gz, ink_lunar_zodiac(l.year));
    }
    ink_cap_result(c, out);
}

static bool parse_at(const char *s, ink_alarm_t *a) { /* "2026-11-05 15:00" */
    int y, mo, d, h, mi;
    if (sscanf(s, "%d-%d-%d %d:%d", &y, &mo, &d, &h, &mi) != 5 || h < 0 || h > 23 || mi < 0 || mi > 59) return false;
    ink_lunar_t date;
    if (!ink_lunar_from_solar(y, mo, d, &date)) return false;
    a->year = (int16_t)y;
    a->month = (uint8_t)mo;
    a->day = (uint8_t)d;
    a->hour = (uint8_t)h;
    a->min = (uint8_t)mi;
    return true;
}

static void answer_add(ink_call_t *c, ink_alarm_t *a, const char *undo_cap) {
    const char *member = ink_cap_call_member(c);
    int id = ink_time_alarm_add_for(a, &c->rc.revision, member);
    if (id <= 0) {
        fail(c, id == -5 ? INK_RC_STALE : id == -3 ? INK_RC_REJECTED : INK_RC_INVALID, id == -2 ? "that time has passed" : id == -3 ? "too many alarms" : "not a valid time");
        return;
    }
    snprintf(c->rc.object, sizeof c->rc.object, "%s:%d", a->kind == INK_ALARM ? "alarm" : "reminder", id);
    char next[24];
    if (a->next_s) {
        local_text(a->next_s, next, sizeof next);
        snprintf(c->rc.say, sizeof c->rc.say, TR("好的，%.16s"), next);
    }
    ink_args_t undo;
    ink_args_init(&undo);
    ink_args_int(&undo, "id", id);
    ink_args_int(&undo, "if_revision", c->rc.revision);
    ink_cap_undoable(c, undo_cap, &undo, UNDO_MS);
}

static void cap_alarm_set(ink_call_t *c) {
    ink_alarm_t a = {.kind = INK_ALARM, .src = (uint8_t)c->src};
    int64_t h = 0, m = 0, wd = 0;
    ink_args_get_int(&c->args, "hour", &h);
    ink_args_get_int(&c->args, "minute", &m);
    a.hour = (uint8_t)h;
    a.min = (uint8_t)m;
    if (ink_args_get_int(&c->args, "weekdays", &wd) && wd) {
        a.repeat = INK_REPEAT_WEEKDAYS;
        a.weekdays = (uint8_t)wd;
    }
    ink_args_get_str(&c->args, "title", a.title, sizeof a.title);
    answer_add(c, &a, "alarm.cancel");
}

/* ---- calendar-3: reminders are events (time.h); the old arguments keep working ---- */

static const char *const FREQS[] = {"none", "daily", "weekly", "monthly", "yearly", "lunar_yearly", "workdays", NULL};
static const char *const BYS[] = {"day", "weekday", "last_weekday", "last_day", NULL};
static int word(const char *const *list, const char *s) { for (int i = 0; list[i]; i++) if (!strcmp(list[i], s)) return i; return -1; }

/* "YYYY-MM-DD HH:MM", or "YYYY-MM-DD" (all day): 1 timed, 2 a date, 0 not readable. */
static int parse_when(const char *s, ink_tm_t *t) {
    memset(t, 0, sizeof *t);
    int n = sscanf(s, "%d-%d-%d %d:%d", &t->year, &t->month, &t->day, &t->hour, &t->min);
    ink_lunar_t check;
    if ((n != 5 && n != 3) || t->hour < 0 || t->hour > 23 || t->min < 0 || t->min > 59 || !ink_lunar_from_solar(t->year, t->month, t->day, &check)) return 0;
    return n == 5 ? 1 : 2;
}

static void set_rule(ink_event_t *e, int freq) {
    e->freq = (uint8_t)(freq == 6 ? INK_FREQ_WEEKLY : freq);
    e->weekdays = freq == 6 ? 0x3E : 0;
    e->interval = 1; e->count = 0; e->until_year = 0; e->by = INK_BY_DAY;
    e->rday = 0; e->rmonth = 0; e->nth = 0; e->lmonth = 0; e->nex = 0;
}

/* The `rule` object: freq, interval, weekdays (bit 0 Sunday), monthly, until, count. NULL or why it is not readable. */
static const char *read_rule(ink_event_t *e, const char *json, ink_json_t *j) {
    if (ink_json_parse(j, json, strlen(json)) || j->tok[0].type != 1) return "rule: an object";
    char text[24];
    int64_t v;
    int t = ink_json_find(j, 0, "freq");
    if (t >= 0) { int f = ink_json_str(j, t, text, sizeof text) > 0 ? word(FREQS, text) : -1; if (f < 0) return "rule.freq: none daily weekly workdays monthly yearly lunar_yearly"; set_rule(e, f); }
    if ((t = ink_json_find(j, 0, "interval")) >= 0) { if (!ink_json_int(j, t, &v) || v < 1 || v > 99) return "rule.interval: 1-99"; e->interval = (uint8_t)v; }
    if ((t = ink_json_find(j, 0, "weekdays")) >= 0) { if (!ink_json_int(j, t, &v) || v < 1 || v > 127) return "rule.weekdays: 1-127, bit 0 Sunday"; e->weekdays = (uint8_t)v; }
    if ((t = ink_json_find(j, 0, "monthly")) >= 0) { int by = ink_json_str(j, t, text, sizeof text) > 0 ? word(BYS, text) : -1; if (by < 0) return "rule.monthly: day weekday last_weekday last_day"; e->by = (uint8_t)by; e->nth = 0; }
    if ((t = ink_json_find(j, 0, "count")) >= 0) { if (!ink_json_int(j, t, &v) || v < 0 || v > 730) return "rule.count: 0-730"; e->count = (uint16_t)v; if (v) e->until_year = 0; }
    if ((t = ink_json_find(j, 0, "until")) >= 0) {
        ink_tm_t u;
        if (ink_json_str(j, t, text, sizeof text) < 0) return "rule.until: YYYY-MM-DD";
        if (!text[0]) e->until_year = 0;
        else if (parse_when(text, &u) != 2) return "rule.until: YYYY-MM-DD";
        else { e->until_year = (int16_t)u.year; e->until_month = (uint8_t)u.month; e->until_day = (uint8_t)u.day; e->count = 0; }
    }
    return NULL;
}

static const char *event_error(int r) {
    switch (r) {
    case INK_EVENT_PAST: return "that time has passed";
    case INK_EVENT_FULL: return "the calendar is full (128 events)";
    case INK_EVENT_EXCEPTIONS_FULL: return "16 changed or deleted occurrences already: use scope following";
    case INK_EVENT_UNTIL_BEFORE: return "until must not come before the start";
    case INK_EVENT_END_BEFORE: return "end must follow at (exclusive)";
    case INK_EVENT_NO_TITLE: return "title: not empty";
    case INK_EVENT_STORAGE: return "could not be stored, try again";
    case INK_EVENT_NOT_FOUND: return "no such reminder or occurrence";
    default: return "not a valid time";
    }
}
static ink_rc_t event_status(int r) {
    return r == INK_EVENT_GONE || r == INK_EVENT_STALE || r == INK_EVENT_NOT_FOUND ? INK_RC_STALE : r == INK_EVENT_FULL || r == INK_EVENT_EXCEPTIONS_FULL ? INK_RC_REJECTED : INK_RC_INVALID;
}

/* occurrence and scope of a change to one that repeats: refused without them (calendar-3). */
static bool read_scope(ink_call_t *c, const ink_event_t *e, int64_t *occurrence, ink_scope_t *scope) {
    char text[24] = "";
    *occurrence = 0; *scope = INK_SCOPE_ALL;
    if (e->freq == INK_FREQ_NONE) return true;
    static const char *const SCOPES[] = {"all", "this", "following", NULL};
    int k = ink_args_get_str(&c->args, "scope", text, sizeof text) > 0 ? word(SCOPES, text) : -1;
    if (k < 0) { fail(c, INK_RC_REJECTED, "repeats: give scope (this, following or all) and occurrence (its YYYY-MM-DD HH:MM)"); return false; }
    *scope = (ink_scope_t)k;
    ink_tm_t t;
    if (ink_args_get_str(&c->args, "occurrence", text, sizeof text) > 0 && parse_when(text, &t)) {
        if (e->all_day) t.hour = t.min = 0;
        *occurrence = ink_time_from_local(&t);
    } else if (k) { fail(c, INK_RC_REJECTED, "scope this or following: give occurrence (its YYYY-MM-DD HH:MM)"); return false; }
    return true;
}

static void cap_reminder_add(ink_call_t *c) {
    bool update = !strcmp(c->cap->id, "reminder.update");
    const char *member = ink_cap_call_member(c);
    ink_event_t *e = osal_calloc(1, sizeof *e, OSAL_MEM_EXTERNAL);
    ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL);
    char at[32] = "", end[32] = "", repeat[16] = "", json[160] = "";
    int64_t id = 0, occurrence = 0;
    ink_scope_t scope = INK_SCOPE_ALL;
    ink_tm_t start = {0};
    if (!e || !j) { fail(c, INK_RC_FAILED, "out of memory"); goto done; }
    ink_args_get_int(&c->args, "id", &id);
    if (update && ink_time_event_get_for((uint32_t)id, e, member)) { fail(c, INK_RC_STALE, "no such reminder"); goto done; }
    if (update && !read_scope(c, e, &occurrence, &scope)) goto done;
    if (!update) { e->nleads = 1; e->interval = 1; }
    bool timed_given = ink_args_get_str(&c->args, "at", at, sizeof at) >= 0;
    int kind = timed_given ? parse_when(at, &start) : 0;
    if (!timed_given && update) { /* not given: where it is (the occurrence's day for one of a series) */
        kind = e->all_day ? 2 : 1;
        start = (ink_tm_t){.year = e->year, .month = e->month, .day = e->day, .hour = e->hour, .min = e->min};
        if (occurrence && scope != INK_SCOPE_ALL) { ink_tm_t o; ink_time_to_local(occurrence, &o); start.year = o.year; start.month = o.month; start.day = o.day; }
    }
    if (ink_args_get_str(&c->args, "title", e->title, sizeof e->title) >= 0 && !e->title[0]) kind = 0;
    if (!kind || !e->title[0]) { fail(c, INK_RC_INVALID, "at: YYYY-MM-DD HH:MM"); goto done; }
    bool was_all_day = e->all_day, ring = e->nleads > 0;
    if (timed_given) e->all_day = kind == 2;
    ink_args_get_bool(&c->args, "all_day", &e->all_day);
    if (e->all_day) start.hour = start.min = 0;
    if (e->all_day && !was_all_day) e->duration_s = 0;
    e->at_utc = 0; e->year = (int16_t)start.year; e->month = (uint8_t)start.month; e->day = (uint8_t)start.day;
    e->hour = (uint8_t)start.hour; e->min = (uint8_t)start.min; e->src = (uint8_t)c->src;
    if (ink_args_get_bool(&c->args, "ring", &ring)) { if (!ring) e->nleads = 0; else if (!e->nleads) { e->nleads = 1; e->leads[0] = 0; } }
    if (ink_args_get_str(&c->args, "reminders", json, sizeof json) >= 0) { /* minutes before; negative on all-day: minutes after its midnight */
        if (ink_json_parse(j, json, strlen(json)) || j->tok[0].type != 2 || j->tok[0].size > INK_EVENT_LEADS) { fail(c, INK_RC_INVALID, "reminders: up to 3 minutes before"); goto done; }
        e->nleads = (uint8_t)j->tok[0].size;
        for (int i = 0; i < e->nleads; i++) { int64_t v = 0; ink_json_int(j, i + 1, &v); e->leads[i] = (int32_t)v; }
    }
    if (ink_args_get_str(&c->args, "end", end, sizeof end) >= 0) {
        ink_tm_t stop = {0};
        int ek = end[0] ? parse_when(end, &stop) : 0;
        if (end[0] && !ek) { fail(c, INK_RC_INVALID, "end: YYYY-MM-DD HH:MM"); goto done; }
        if (e->all_day) stop.hour = stop.min = 0;
        e->duration_s = end[0] ? ink_time_from_local(&stop) - ink_time_from_local(&start) : 0;
        if (end[0] && e->duration_s <= 0) { fail(c, INK_RC_INVALID, "end must follow at (exclusive)"); goto done; }
    }
    if (e->all_day && !e->duration_s) { ink_tm_t next = start; next.day++; e->duration_s = ink_time_from_local(&next) - ink_time_from_local(&start); }
    if (ink_args_get_str(&c->args, "repeat", repeat, sizeof repeat) >= 0) {
        int f = word(FREQS, repeat);
        if (f < 0) { fail(c, INK_RC_INVALID, "repeat: none daily weekly workdays monthly yearly lunar_yearly"); goto done; }
        if (!update || f != (e->freq == INK_FREQ_WEEKLY && e->weekdays == 0x3E ? 6 : e->freq)) set_rule(e, f);
    }
    if (ink_args_get_str(&c->args, "rule", json, sizeof json) >= 0) {
        const char *why = read_rule(e, json, j);
        if (why) { fail(c, INK_RC_INVALID, why); goto done; }
    }
    if (!update) {
        int r = ink_time_event_add_for(e, &c->rc.revision, member);
        if (r <= 0) { fail(c, event_status(r), event_error(r)); goto done; }
        snprintf(c->rc.object, sizeof c->rc.object, "reminder:%d", r);
        int64_t next = ink_event_occurrence(e, ink_time_now_s() + 1);
        char when[24];
        if (next) { local_text(next, when, sizeof when); snprintf(c->rc.say, sizeof c->rc.say, TR("好的，%.16s"), when); }
        ink_args_t *undo = osal_malloc(sizeof *undo, OSAL_MEM_EXTERNAL);
        if (undo) {
            ink_args_init(undo);
            ink_args_int(undo, "id", r);
            ink_args_int(undo, "if_revision", c->rc.revision);
            ink_args_str(undo, "scope", "all");
            ink_cap_undoable(c, "reminder.cancel", undo, UNDO_MS);
            osal_free(undo);
        }
    } else {
        uint32_t made = 0;
        int r = ink_time_event_update_for(e, occurrence, scope, member, &made);
        if (r) { fail(c, event_status(r), event_error(r)); goto done; }
        snprintf(c->rc.object, sizeof c->rc.object, "reminder:%u", (unsigned)(made ? made : e->id));
    }
done:
    osal_free(j);
    osal_free(e);
}

static bool member_adult(const char *member) {
    bool adult = false;
    ink_svc_lock();
    const ink_member_t *m;
    for (int i = 0; (m = ink_member_at(i)) != NULL; i++) if (!strcmp(m->id, member)) adult = m->role == INK_ROLE_ADULT;
    ink_svc_unlock();
    return adult;
}

static void cap_reminder_cancel(ink_call_t *c) {
    int64_t id = 0, revision = 0, occurrence = 0;
    const char *member = ink_cap_call_member(c);
    ink_args_get_int(&c->args, "id", &id);
    if (ink_args_get_int(&c->args, "if_revision", &revision) && (revision <= 0 || revision > UINT32_MAX)) { fail(c, INK_RC_STALE, "alarm changed or absent"); return; }
    ink_event_t *e = osal_malloc(sizeof *e, OSAL_MEM_EXTERNAL);
    ink_scope_t scope = INK_SCOPE_ALL;
    bool adult = member_adult(member);
    int r = !e ? INK_EVENT_STORAGE : ink_time_event_get_for((uint32_t)id, e, NULL);
    if (!r && strcmp(e->owner, member) && !adult) r = INK_EVENT_NOT_FOUND;
    bool asked = !r && read_scope(c, e, &occurrence, &scope);
    osal_free(e);
    if (!r && !asked) return;
    uint32_t token = 0;
    if (!r) r = ink_time_event_delete_for((uint32_t)id, (uint32_t)revision, occurrence, scope, member, adult, &token);
    if (r) { fail(c, r == INK_EVENT_GONE || r == INK_EVENT_STALE ? INK_RC_STALE : r == INK_EVENT_EXCEPTIONS_FULL ? INK_RC_REJECTED : INK_RC_FAILED, r == INK_EVENT_EXCEPTIONS_FULL ? event_error(r) : "no such one, or changed"); return; }
    snprintf(c->rc.object, sizeof c->rc.object, "reminder:%u", (unsigned)id);
    ink_args_t *undo = token ? osal_malloc(sizeof *undo, OSAL_MEM_EXTERNAL) : NULL; /* calendar-3: a delete can be undone */
    if (undo) {
        ink_args_init(undo);
        ink_args_int(undo, "token", token);
        ink_cap_undoable(c, "calendar.restore", undo, UNDO_MS);
    }
    osal_free(undo);
}

static void cap_reminder_restore(ink_call_t *c) {
    int64_t token = 0;
    ink_args_get_int(&c->args, "token", &token);
    if (ink_time_event_restore((uint32_t)token, ink_cap_call_member(c))) fail(c, INK_RC_STALE, "nothing to restore");
}

static void rule_json(ink_jw_t *w, const ink_event_t *e) {
    ink_jw_raw(w, ",\"rule\":{\"freq\":");
    ink_jw_str(w, FREQS[e->freq % 6]);
    ink_jw_raw(w, ",\"interval\":"); ink_jw_int(w, e->interval);
    if (e->freq == INK_FREQ_WEEKLY) { ink_jw_raw(w, ",\"weekdays\":"); ink_jw_int(w, e->weekdays); }
    if (e->freq == INK_FREQ_MONTHLY || e->freq == INK_FREQ_YEARLY) { ink_jw_raw(w, ",\"monthly\":"); ink_jw_str(w, BYS[e->by % 4]); }
    char until[16] = "";
    if (e->until_year) snprintf(until, sizeof until, "%04d-%02d-%02d", e->until_year, e->until_month, e->until_day);
    ink_jw_raw(w, ",\"until\":"); ink_jw_str(w, until);
    ink_jw_raw(w, ",\"count\":"); ink_jw_int(w, e->count);
    ink_jw_raw(w, "},\"reminders\":[");
    for (int i = 0; i < e->nleads; i++) { if (i) ink_jw_raw(w, ","); ink_jw_int(w, e->leads[i]); }
    ink_jw_raw(w, "]");
}

/* One entry: an event's next start (reminder.list) or one occurrence in a range (calendar.list). */
static void event_json(ink_jw_t *w, const ink_event_t *e, int64_t at, bool first) {
    char next[24] = "", end[24] = "", orig[24] = "";
    if (at) local_text(at, next, sizeof next);
    if (at && e->duration_s) local_text(at + e->duration_s, end, sizeof end);
    if (e->parent) {
        int y, m, d;
        time_civil_from_days(e->orig_day, &y, &m, &d);
        snprintf(orig, sizeof orig, "%04d-%02d-%02d %02d:%02d", y, m, d, (int)(e->orig_min / 60 % 24), (int)(e->orig_min % 60));
    } else snprintf(orig, sizeof orig, "%.16s", next);
    static const int REPEATS[] = {INK_REPEAT_NONE, INK_REPEAT_WEEKDAYS, INK_REPEAT_WEEKDAYS, INK_REPEAT_MONTHLY, INK_REPEAT_YEARLY, INK_REPEAT_LUNAR_YEARLY};
    ink_jw_raw(w, first ? "{\"id\":" : ",{\"id\":"); ink_jw_int(w, e->id);
    ink_jw_raw(w, ",\"series\":"); ink_jw_int(w, e->parent ? e->parent : e->id);
    ink_jw_raw(w, ",\"title\":"); ink_jw_str(w, e->title);
    ink_jw_raw(w, ",\"enabled\":true,\"next\":"); ink_jw_str(w, next);
    ink_jw_raw(w, ",\"repeat\":"); ink_jw_int(w, REPEATS[e->freq % 6]);
    ink_jw_raw(w, ",\"at\":"); ink_jw_str(w, next);
    ink_jw_raw(w, ",\"end\":"); ink_jw_str(w, end);
    ink_jw_raw(w, ",\"occurrence\":"); ink_jw_str(w, orig);
    ink_jw_raw(w, e->nleads ? ",\"ring\":true" : ",\"ring\":false");
    ink_jw_raw(w, e->all_day ? ",\"all_day\":true" : ",\"all_day\":false");
    ink_jw_raw(w, e->parent ? ",\"edited\":true" : ",\"edited\":false");
    rule_json(w, e);
    ink_jw_raw(w, "}");
}

static void cap_event_list(ink_call_t *c) {
    const char *member = ink_cap_call_member(c);
    bool ranged = !strcmp(c->cap->id, "calendar.list");
    int64_t bounds[2] = {0, 0};
    for (int i = 0; ranged && i < 2; i++) {
        char date[16] = "";
        ink_tm_t t;
        ink_args_get_str(&c->args, i ? "until" : "from", date, sizeof date);
        if (strlen(date) != 10 || parse_when(date, &t) != 2) { fail(c, INK_RC_INVALID, "range: YYYY-MM-DD"); return; }
        bounds[i] = ink_time_from_local(&t);
    }
    if (ranged && bounds[1] <= bounds[0]) { fail(c, INK_RC_INVALID, "until must follow from (exclusive)"); return; }
    enum { OUT = 8192 };
    char *out = osal_malloc(OUT, OSAL_MEM_EXTERNAL);
    ink_event_t *e = osal_malloc(sizeof *e, OSAL_MEM_EXTERNAL);
    if (!out || !e) { osal_free(out); osal_free(e); fail(c, INK_RC_FAILED, "out of memory"); return; }
    ink_jw_t w;
    ink_jw_init(&w, out, OUT);
    ink_jw_raw(&w, "[");
    int cursor = 0, got;
    bool first = true;
    int64_t now = ink_time_now_s();
    while ((got = ink_time_event_next_for(&cursor, e, member)) == 1) {
        int64_t at = ranged ? ink_event_occurrence(e, bounds[0]) : now ? ink_event_occurrence(e, now + 1) : 0;
        do {
            if (ranged && (!at || at >= bounds[1])) break;
            size_t before = w.len;
            event_json(&w, e, at, first);
            if (w.overflow) {
                if (ranged) { fail(c, INK_RC_FAILED, "too many occurrences; narrow the range"); goto done; }
                w.overflow = false; w.len = before; break;
            }
            first = false;
            if (!ranged) break;
            at = ink_event_occurrence(e, at + (e->duration_s > 0 ? e->duration_s : 1));
        } while (at && at < bounds[1]);
    }
    ink_jw_raw(&w, "]");
    if (got < 0) c->rc.status = INK_RC_STALE;
    else if (w.overflow) fail(c, INK_RC_FAILED, "too many occurrences; narrow the range");
    else ink_cap_result(c, out);
done:
    osal_free(out);
    osal_free(e);
}

static void cap_alarm_update(ink_call_t *c) {
    ink_alarm_t a; int cursor = 0; int64_t id = 0, v;
    const char *member = ink_cap_call_member(c); ink_args_get_int(&c->args, "id", &id);
    while (ink_time_alarm_next_for(&cursor, INK_ALARM, &a, member) == 1) if (a.id == (uint32_t)id) {
        if (ink_args_get_int(&c->args, "hour", &v)) a.hour = (uint8_t)v;
        if (ink_args_get_int(&c->args, "minute", &v)) a.min = (uint8_t)v;
        if (ink_args_get_int(&c->args, "weekdays", &v)) { a.weekdays = (uint8_t)v; a.repeat = v ? INK_REPEAT_WEEKDAYS : INK_REPEAT_NONE; }
        ink_args_get_bool(&c->args, "enabled", &a.enabled);
        ink_args_get_str(&c->args, "title", a.title, sizeof a.title); a.src = (uint8_t)c->src;
        if (ink_time_alarm_update_for(&a, member)) fail(c, INK_RC_FAILED, "alarm update failed");
        else snprintf(c->rc.object, sizeof c->rc.object, "alarm:%u", (unsigned)a.id);
        return;
    }
    fail(c, INK_RC_STALE, "no such alarm");
}

static void list_kind(ink_call_t *c, ink_alarm_kind_t kind) {
    const char *member = ink_cap_call_member(c);
    bool ranged = !strcmp(c->cap->id, "calendar.list");
    int64_t from = 0, until = 0;
    if (ranged) {
        const char *keys[] = {"from", "until"};
        int64_t *bounds[] = {&from, &until};
        for (int i = 0; i < 2; i++) {
            char date[16] = "", at[32]; ink_alarm_t a = {0};
            ink_args_get_str(&c->args, keys[i], date, sizeof date);
            snprintf(at, sizeof at, "%.10s 00:00", date);
            if (strlen(date) != 10 || !parse_at(at, &a)) { fail(c, INK_RC_INVALID, "range: YYYY-MM-DD"); return; }
            *bounds[i] = ink_time_from_local(&(ink_tm_t){.year = a.year, .month = a.month, .day = a.day});
        }
        if (until <= from) { fail(c, INK_RC_INVALID, "until must follow from (exclusive)"); return; }
    }
    char *out = osal_malloc(4096, OSAL_MEM_EXTERNAL);
    ink_alarm_t *a = osal_malloc(sizeof *a, OSAL_MEM_EXTERNAL);
    if (!out || !a) {
        osal_free(out);
        osal_free(a);
        fail(c, INK_RC_FAILED, "out of memory");
        return;
    }
    ink_jw_t w;
    ink_jw_init(&w, out, 4096);
    ink_jw_raw(&w, "[");
    int cursor = 0;
    bool first = true;
    int got;
    while ((got = ink_time_alarm_next_for(&cursor, kind, a, member)) == 1) {
      int64_t at = ranged ? ink_alarm_occurrence(a, from) : a->next_s;
      do {
        if (ranged && (!at || at >= until)) break;
        char next[24] = "";
        if (at) local_text(at, next, sizeof next);
        size_t before = w.len;
        ink_jw_raw(&w, first ? "{\"id\":" : ",{\"id\":");
        ink_jw_int(&w, a->id);
        ink_jw_raw(&w, ",\"title\":");
        ink_jw_str(&w, a->title);
        ink_jw_raw(&w, ",\"enabled\":");
        ink_jw_raw(&w, a->enabled ? "true" : "false");
        ink_jw_raw(&w, ",\"next\":");
        ink_jw_str(&w, next);
        ink_jw_raw(&w, ",\"repeat\":");
        ink_jw_int(&w, a->repeat);
        if (kind == INK_REMINDER) {
            char end[24] = "";
            if (at && a->duration_s) local_text(at + a->duration_s, end, sizeof end);
            ink_jw_raw(&w, ",\"at\":"); ink_jw_str(&w, next);
            ink_jw_raw(&w, ",\"end\":"); ink_jw_str(&w, end);
            ink_jw_raw(&w, a->silent ? ",\"ring\":false" : ",\"ring\":true");
            ink_jw_raw(&w, a->all_day ? ",\"all_day\":true" : ",\"all_day\":false");
        }
        ink_jw_raw(&w, "}");
        if (w.overflow) {
            if (ranged) { fail(c, INK_RC_FAILED, "too many occurrences; narrow the range"); goto done; }
            w.overflow = false;
            w.len = before;
            break;
        }
        first = false;
        if (!ranged) break;
        at = ink_alarm_occurrence(a, at + (a->duration_s > 0 ? a->duration_s : 1));
      } while (at && at < until);
    }
    ink_jw_raw(&w, "]");
    if (got < 0) c->rc.status = INK_RC_STALE;
    else if (w.overflow) fail(c, INK_RC_FAILED, "too many occurrences; narrow the range");
    else ink_cap_result(c, out);
done:
    osal_free(out);
    osal_free(a);
}

static void cap_alarm_list(ink_call_t *c) { list_kind(c, INK_ALARM); }

static void cap_cancel(ink_call_t *c) {
    int64_t id = 0, revision = 0;
    ink_args_get_int(&c->args, "id", &id);
    if (ink_args_get_int(&c->args, "if_revision", &revision) && (revision <= 0 || revision > UINT32_MAX)) {
        fail(c, INK_RC_STALE, "alarm changed or absent");
        return;
    }
    const char *member = ink_cap_call_member(c);
    int r = ink_time_alarm_cancel_for((uint32_t)id, (uint32_t)revision, member);
    if (r != 0) fail(c, r == -2 || r == -5 ? INK_RC_STALE : INK_RC_FAILED, "no such one, or changed");
}

static void cap_dismiss(ink_call_t *c) {
    int64_t id = 0;
    ink_args_get_int(&c->args, "id", &id);
    if (ink_alarm_dismiss((uint32_t)id) != 0) fail(c, INK_RC_FAILED, "nothing is ringing");
}

static void cap_snooze(ink_call_t *c) {
    int64_t id = 0, minutes = 0;
    ink_args_get_int(&c->args, "id", &id);
    ink_args_get_int(&c->args, "minutes", &minutes);
    if (ink_alarm_snooze((uint32_t)id, (int)minutes) != 0) fail(c, INK_RC_FAILED, "nothing is ringing");
}

/* calendar-3: the member's events to /me/日历导出-YYYYMMDD.ics ("-2" and on when taken), and a file of them in. */
static void cap_export(ink_call_t *c) {
    const char *member = ink_cap_call_member(c);
    ink_tm_t t;
    ink_time_to_local(ink_time_now_s(), &t);
    char path[64];
    int count = 0, r = INK_FS_ERR_EXISTS;
    for (int k = 1; k < 10 && r == INK_FS_ERR_EXISTS; k++) {
        if (k == 1) snprintf(path, sizeof path, "/me/日历导出-%04d%02d%02d.ics", t.year, t.month, t.day);
        else snprintf(path, sizeof path, "/me/日历导出-%04d%02d%02d-%d.ics", t.year, t.month, t.day, k);
        r = ink_time_ics_export(member, path, &count);
    }
    if (r == 1) { fail(c, INK_RC_REJECTED, "no events to export"); return; }
    if (r) { fail(c, INK_RC_FAILED, ink_fs_err_name(r)); return; }
    char out[160];
    snprintf(out, sizeof out, "{\"path\":\"%s\",\"count\":%d}", path, count);
    ink_cap_result(c, out);
    snprintf(c->rc.object, sizeof c->rc.object, "file:%.40s", path);
}

static void cap_import(ink_call_t *c) {
    char path[INK_FS_PATH_MAX] = "", out[160];
    ink_args_get_str(&c->args, "path", path, sizeof path);
    ink_ics_result_t r;
    int err = ink_time_ics_import(ink_cap_call_member(c), path, &r);
    snprintf(out, sizeof out, "{\"imported\":%d,\"skipped\":%d,\"existing\":%d,\"full\":%d,\"error\":\"%s\"}", r.imported, r.skipped, r.existing, r.full,
             err == 1 ? "not a calendar file" : err ? ink_fs_err_name(err) : "");
    ink_cap_result(c, out);
    if (err) fail(c, err == 1 ? INK_RC_INVALID : INK_RC_FAILED, err == 1 ? "not a calendar file" : ink_fs_err_name(err));
}

#define ID_ARG "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"integer\",\"minimum\":1}},\"required\":[\"id\"]}"
#define SCOPE_FIELDS "\"occurrence\":{\"type\":\"string\",\"maxLength\":20},\"scope\":{\"type\":\"string\",\"enum\":[\"this\",\"following\",\"all\"]}"
#define REMINDER_FIELDS "\"at\":{\"type\":\"string\",\"maxLength\":20},\"title\":{\"type\":\"string\",\"minLength\":1,\"maxLength\":30},\"reminders\":{\"type\":\"array\"},\"rule\":{\"type\":\"object\"},\"repeat\":{\"type\":\"string\",\"enum\":[\"none\",\"daily\",\"weekly\",\"workdays\",\"monthly\",\"yearly\",\"lunar_yearly\"]},\"ring\":{\"type\":\"boolean\"},\"all_day\":{\"type\":\"boolean\"},\"end\":{\"type\":\"string\",\"maxLength\":20}"

static const ink_cap_t TIME_CAPS[] = {
    {.id = "time.almanac", .title = "老黄历（日期不给就是系统时区的今天，民俗说法）",
     .input = "{\"type\":\"object\",\"properties\":{\"date\":{\"type\":\"string\",\"minLength\":10,\"maxLength\":10}},\"additionalProperties\":false}",
     .output = "{\"type\":\"object\"}", .effect = INK_FX_READ, .child_ok = true,
     .exec = INK_EXEC_DIRECT, .run = ink_almanac_cap_run},
    {.id = "time.now", .title = "现在几点（含时区和农历）", .output = "{\"type\":\"object\"}", .effect = INK_FX_READ, .child_ok = true, .exec = INK_EXEC_DIRECT, .run = cap_now},
    {.id = "time.set",
     .title = "改系统时间",
     .ask = TR_NOOP("把系统时间改成 {utc_s}（UTC 秒）"),
     .input = "{\"type\":\"object\",\"properties\":{\"utc_s\":{\"type\":\"integer\",\"minimum\":1000000000}},\"required\":[\"utc_s\"]}",
     .effect = INK_FX_SYSTEM, /* children cannot: it would get around time limits (foundation §7.1) */
     .exec = INK_EXEC_BG,
     .run = cap_set},
    {.id = "time.lunar",
     .title = "公历和农历互查（不给日期就是今天）",
     .input = "{\"type\":\"object\",\"properties\":{\"year\":{\"type\":\"integer\"},\"month\":{\"type\":\"integer\"},\"day\":{\"type\":\"integer\"},"
              "\"lunar_year\":{\"type\":\"integer\"},\"lunar_month\":{\"type\":\"integer\"},\"lunar_day\":{\"type\":\"integer\"},\"leap\":{\"type\":\"boolean\"}}}",
     .output = "{\"type\":\"object\"}",
     .effect = INK_FX_READ,
     .child_ok = true,
     .exec = INK_EXEC_DIRECT,
     .run = cap_lunar},
    {.id = "alarm.set", .app = "clock",
     .title = "设闹钟（weekdays 每周几：位 0 是周日；不给就只响一次）",
     .input = "{\"type\":\"object\",\"properties\":{\"hour\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":23},\"minute\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":59},"
              "\"weekdays\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":127},\"title\":{\"type\":\"string\",\"maxLength\":15}},\"required\":[\"hour\",\"minute\"]}",
     .effect = INK_FX_LOCAL_WRITE,
     .child_ok = true,
     .exec = INK_EXEC_DIRECT,
     .run = cap_alarm_set},
    {.id = "alarm.list", .app = "clock", .title = "我的闹钟", .output = "{\"type\":\"array\"}", .effect = INK_FX_READ, .child_ok = true, .exec = INK_EXEC_DIRECT, .run = cap_alarm_list},
    {.id = "alarm.update", .app = "clock", .title = "修改或开关我的闹钟", .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .exec = INK_EXEC_DIRECT, .run = cap_alarm_update,
     .input = "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"integer\",\"minimum\":1,\"maximum\":4294967295},\"hour\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":23},\"minute\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":59},"
              "\"weekdays\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":127},\"title\":{\"type\":\"string\",\"maxLength\":15},\"enabled\":{\"type\":\"boolean\"}},\"required\":[\"id\"]}"},
    {.id = "alarm.cancel", .app = "clock", .title = "删闹钟", .input = ID_ARG, .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .exec = INK_EXEC_DIRECT, .run = cap_cancel},
    {.id = "alarm.dismiss", .app = "clock",
     .title = "关掉正在响的闹钟",
     .input = "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"integer\",\"minimum\":0}}}",
     .effect = INK_FX_LOCAL_WRITE,
     .child_ok = true,
     .while_locked = true,
     .exec = INK_EXEC_DIRECT,
     .run = cap_dismiss},
    {.id = "alarm.snooze", .app = "clock",
     .title = "闹钟稍后再响",
     .input = "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"integer\",\"minimum\":0},\"minutes\":{\"type\":\"integer\",\"minimum\":1,\"maximum\":120}}}",
     .effect = INK_FX_LOCAL_WRITE,
     .child_ok = true,
     .while_locked = true,
     .exec = INK_EXEC_DIRECT,
     .run = cap_snooze},
    {.id = "reminder.add", .app = "clock",
     .title = "加提醒或日程（at/end 本地时间 YYYY-MM-DD HH:MM，只写日期就是全天；end 不含；reminders 提前分钟数最多 3 个，全天事件负数是当天几点后多少分钟，不给就准时响，[] 为不响的日程；ring 旧参数；repeat: none daily weekly workdays monthly yearly lunar_yearly；rule 对象：freq interval(1-99) weekdays(位 0 周日) monthly(day weekday last_weekday last_day) until(YYYY-MM-DD) count(1-730)）",
     .input = "{\"type\":\"object\",\"properties\":{" REMINDER_FIELDS "},\"required\":[\"at\",\"title\"]}",
     .effect = INK_FX_LOCAL_WRITE,
     .child_ok = true,
     .exec = INK_EXEC_DIRECT,
     .run = cap_reminder_add},
    {.id = "reminder.update", .app = "clock", .title = "修改我的提醒或日程（只给要改的；end 空串清除，不传则保留时长；重复的必须给 scope：this 只改这一次、following 这次和以后、all 全部，this 和 following 还要 occurrence 那一次原来的 YYYY-MM-DD HH:MM）", .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .exec = INK_EXEC_DIRECT, .run = cap_reminder_add,
     .input = "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"integer\",\"minimum\":1,\"maximum\":4294967295}," REMINDER_FIELDS "," SCOPE_FIELDS "},\"required\":[\"id\"]}"},
    {.id = "reminder.list", .app = "clock", .title = "我的提醒和日程（每件的下一次）", .output = "{\"type\":\"array\"}", .effect = INK_FX_READ, .child_ok = true, .exec = INK_EXEC_DIRECT, .run = cap_event_list},
    {.id = "calendar.list", .app = "upcoming", .title = "按本地日期列出我的日程和提醒的每一次（from 含当天，until 不含当天；日程 ring=false；end 为结束时间，不含；series 系列 id，occurrence 这一次原来的开始，edited 改过的那一次，rule 重复，reminders 提前分钟数）", .output = "{\"type\":\"array\"}",
     .input = "{\"type\":\"object\",\"properties\":{\"from\":{\"type\":\"string\",\"maxLength\":10},\"until\":{\"type\":\"string\",\"maxLength\":10}},\"required\":[\"from\",\"until\"]}",
     .effect = INK_FX_READ, .child_ok = true, .exec = INK_EXEC_DIRECT, .run = cap_event_list},
    {.id = "reminder.cancel", .app = "clock", .title = "删提醒或日程（重复的必须给 scope：this 只删这一次、following 删掉这次和以后、all 全部删掉，this 和 following 还要 occurrence）", .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .exec = INK_EXEC_DIRECT, .run = cap_reminder_cancel,
     .input = "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"integer\",\"minimum\":1}," SCOPE_FIELDS "},\"required\":[\"id\"]}"},
    {.id = "calendar.restore", .app = "upcoming", .title = "恢复刚删掉的日程（撤销用）", .input = "{\"type\":\"object\",\"properties\":{\"token\":{\"type\":\"integer\"}},\"required\":[\"token\"]}",
     .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .expose = INK_EXPOSE_OWNER, .exec = INK_EXEC_DIRECT, .run = cap_reminder_restore},
    {.id = "calendar.export", .app = "upcoming", .title = "把我的日程导出成 .ics 文件（到我的空间，含过去的）", .output = "{\"type\":\"object\"}", .effect = INK_FX_LOCAL_WRITE,
     .child_ok = true, .timeout_ms = 30000, .exec = INK_EXEC_BG, .run = cap_export},
    {.id = "calendar.import", .app = "upcoming", .title = "从 .ics 文件导入日程（path 是 /me 或 /family 下的文件）", .output = "{\"type\":\"object\"}",
     .input = "{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\",\"maxLength\":127}},\"required\":[\"path\"]}",
     .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .timeout_ms = 60000, .exec = INK_EXEC_BG, .run = cap_import},
    {.id = "alarm.fired", .app = "clock", .title = "闹钟响了", .form = INK_FORM_EVENT, .effect = INK_FX_READ, .child_ok = true},
    {.id = "alarm.ended", .app = "clock", .title = "闹钟停了", .form = INK_FORM_EVENT, .effect = INK_FX_READ, .child_ok = true},
    {.id = "alarm.missed", .app = "clock", .title = "错过了闹钟", .form = INK_FORM_EVENT, .effect = INK_FX_READ, .child_ok = true},
    {.id = "reminder.fired", .app = "clock", .title = "提醒到时间了", .form = INK_FORM_EVENT, .effect = INK_FX_READ, .child_ok = true},
    {.id = "reminder.missed", .app = "clock", .title = "错过了提醒", .form = INK_FORM_EVENT, .effect = INK_FX_READ, .child_ok = true},
    {.id = "time.changed", .title = "系统时间改了", .form = INK_FORM_EVENT, .effect = INK_FX_READ, .child_ok = true},
};

int ink_time_caps_init(void) { return ink_cap_register(TIME_CAPS, (int)(sizeof TIME_CAPS / sizeof TIME_CAPS[0])); }
