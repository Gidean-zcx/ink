/*
 * iCalendar files for the calendar (calendar-3): export a member's events, import a file of VEVENTs.
 * Rewritten after Fossify Calendar's IcsExporter.kt, IcsImporter.kt and Parser.kt (SOURCE.md), with
 * RECURRENCE-ID for an edited occurrence (RFC 5545) and RSCALE=CHINESE for lunar yearly (RFC 7529).
 * A rule with a part we do not do is skipped and counted, never guessed. Card I/O: not the UI task.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ink/osal.h"
#include "ink/sys/file.h"
#include "ink/sys/time.h"
#include "time_internal.h"

#define LINE_MAX_B 1024
#define CHUNK 1024
#define PENDING 16

static const char *const DAYS[] = {"SU", "MO", "TU", "WE", "TH", "FR", "SA"};

/* ------------------------------------------------------------ writing (IcsExporter.kt:87-115, :144-196) */

typedef struct { ink_fs_file_t *f; char *buf; size_t len; int err; } out_t;

static void flush_out(out_t *o) {
    if (o->len && !o->err && ink_fs_write(o->f, o->buf, o->len) != (int64_t)o->len) o->err = INK_FS_ERR_IO;
    o->len = 0;
}
/* One content line, folded at 75 bytes on a character (RFC 5545 3.1). */
static void line(out_t *o, const char *text) {
    size_t n = strlen(text), at = 0, width = 75;
    while (at < n) {
        size_t take = n - at > width ? width : n - at;
        while (take < n - at && take && ((unsigned char)text[at + take] & 0xC0) == 0x80) take--;
        if (o->len + take + 3 > CHUNK) flush_out(o);
        if (at) o->buf[o->len++] = ' ';
        memcpy(o->buf + o->len, text + at, take);
        o->len += take;
        o->buf[o->len++] = '\r';
        o->buf[o->len++] = '\n';
        at += take;
        width = 74;
    }
}

static void utc_text(int64_t utc, char *out, size_t cap) {
    int64_t days = utc >= 0 ? utc / 86400 : -((-utc + 86399) / 86400), rem = utc - days * 86400;
    int y, m, d;
    time_civil_from_days(days, &y, &m, &d);
    snprintf(out, cap, "%04d%02d%02dT%02d%02d%02dZ", y, m, d, (int)(rem / 3600), (int)(rem / 60 % 60), (int)(rem % 60));
}
static void day_text(int32_t day, char *out, size_t cap) { int y, m, d; time_civil_from_days(day, &y, &m, &d); snprintf(out, cap, "%04d%02d%02d", y, m, d); }
static int64_t start_of(int32_t day, int min) {
    int y, m, d;
    time_civil_from_days(day, &y, &m, &d);
    ink_tm_t t = {.year = y, .month = m, .day = d, .hour = min / 60, .min = min % 60};
    return ink_time_from_local(&t);
}
/* A DTSTART-like property for the occurrence on local `day` at minute `min`. */
static void when(out_t *o, const char *name, const ink_event_t *e, int32_t day, int min) {
    char text[96], v[24];
    if (e->all_day) { day_text(day, v, sizeof v); snprintf(text, sizeof text, "%s;VALUE=DATE:%s", name, v); }
    else { utc_text(e->at_utc && e->freq == INK_FREQ_NONE && !e->parent ? e->at_utc : start_of(day, min), v, sizeof v); snprintf(text, sizeof text, "%s:%s", name, v); }
    line(o, text);
}

static void rrule(out_t *o, const ink_event_t *e) {
    static const char *const FREQ[] = {"", "DAILY", "WEEKLY", "MONTHLY", "YEARLY", "YEARLY"};
    char text[200];
    int n = snprintf(text, sizeof text, "RRULE:%sFREQ=%s", e->freq == INK_FREQ_LUNAR_YEARLY ? "RSCALE=CHINESE;" : "", FREQ[e->freq % 6]);
    if (e->interval > 1) n += snprintf(text + n, sizeof text - (size_t)n, ";INTERVAL=%d", e->interval);
    if (e->freq == INK_FREQ_WEEKLY) {
        n += snprintf(text + n, sizeof text - (size_t)n, ";BYDAY=");
        bool first = true;
        for (int i = 1; i <= 7; i++) if (e->weekdays & (1u << (i % 7))) { n += snprintf(text + n, sizeof text - (size_t)n, "%s%s", first ? "" : ",", DAYS[i % 7]); first = false; }
    }
    if (e->freq == INK_FREQ_YEARLY) n += snprintf(text + n, sizeof text - (size_t)n, ";BYMONTH=%d", e->rmonth);
    if (e->freq == INK_FREQ_MONTHLY || e->freq == INK_FREQ_YEARLY) {
        if (e->by == INK_BY_NTH_WEEKDAY) n += snprintf(text + n, sizeof text - (size_t)n, ";BYDAY=%d%s", e->nth, DAYS[e->wday % 7]);
        else if (e->by == INK_BY_LAST_WEEKDAY) n += snprintf(text + n, sizeof text - (size_t)n, ";BYDAY=-1%s", DAYS[e->wday % 7]);
        else n += snprintf(text + n, sizeof text - (size_t)n, ";BYMONTHDAY=%d", e->by == INK_BY_LAST_DAY ? -1 : e->rday);
    }
    if (e->freq == INK_FREQ_LUNAR_YEARLY) n += snprintf(text + n, sizeof text - (size_t)n, ";BYMONTH=%d%s;BYMONTHDAY=%d", e->lmonth, e->lleap ? "L" : "", e->lday);
    if (e->count) n += snprintf(text + n, sizeof text - (size_t)n, ";COUNT=%d", e->count);
    else if (e->until_year) n += snprintf(text + n, sizeof text - (size_t)n, ";UNTIL=%04d%02d%02d", e->until_year, e->until_month, e->until_day);
    line(o, text);
}

static void escape(const char *in, char *out, size_t cap) {
    size_t n = 0;
    for (; *in && n + 3 < cap; in++) {
        if (*in == '\\' || *in == ';' || *in == ',') out[n++] = '\\';
        if (*in == '\n') { out[n++] = '\\'; out[n++] = 'n'; continue; }
        out[n++] = *in;
    }
    out[n] = 0;
}

static void vevent(out_t *o, const ink_event_t *e, ink_event_t *all, int count) {
    char text[256], v[24];
    int min = e->hour * 60 + e->min;
    line(o, "BEGIN:VEVENT");
    snprintf(text, sizeof text, "UID:%s", e->uid); line(o, text);
    utc_text(e->modified_s, v, sizeof v);
    snprintf(text, sizeof text, "DTSTAMP:%s", v); line(o, text);
    snprintf(text, sizeof text, "LAST-MODIFIED:%s", v); line(o, text);
    char title[200]; escape(e->title, title, sizeof title);
    snprintf(text, sizeof text, "SUMMARY:%s", title); line(o, text);
    if (e->parent) { ink_event_t parent = {.all_day = e->all_day}; when(o, "RECURRENCE-ID", &parent, e->orig_day, e->orig_min); }
    int32_t first = (int32_t)time_days_from_civil(e->year, e->month, e->day);
    when(o, "DTSTART", e, first, min);
    if (e->all_day && e->duration_s) when(o, "DTEND", e, first + (int32_t)((e->duration_s + 43200) / 86400), 0);
    else if (e->duration_s) { snprintf(text, sizeof text, "DURATION:PT%lldS", (long long)e->duration_s); line(o, text); }
    if (e->freq != INK_FREQ_NONE) rrule(o, e);
    for (int i = 0; i < e->nex; i++) { /* an occurrence edited apart goes as its own VEVENT, not as an EXDATE */
        bool moved = false;
        for (int k = 0; k < count && !moved; k++) moved = all[k].parent == e->id && all[k].orig_day == e->ex[i];
        if (moved) continue;
        ink_event_t at = {.all_day = e->all_day};
        when(o, "EXDATE", &at, e->ex[i], min);
    }
    for (int i = 0; i < e->nleads; i++) {
        int32_t lead = e->leads[i];
        line(o, "BEGIN:VALARM");
        line(o, "ACTION:DISPLAY");
        snprintf(text, sizeof text, "DESCRIPTION:%s", title); line(o, text);
        snprintf(text, sizeof text, "TRIGGER:%sPT%dM", lead > 0 ? "-" : "", lead > 0 ? lead : -lead);
        line(o, text);
        line(o, "END:VALARM");
    }
    line(o, "END:VEVENT");
}

int ink_time_ics_export(const char *member, const char *path, int *count) {
    *count = 0;
    ink_event_t *all = osal_malloc(sizeof(ink_event_t) * INK_EVENT_MAX, OSAL_MEM_EXTERNAL);
    char *buf = osal_malloc(CHUNK, OSAL_MEM_EXTERNAL);
    int n = 0, cursor = 0, r = 0;
    if (!all || !buf) r = INK_FS_ERR_IO;
    while (!r && n < INK_EVENT_MAX && ink_time_event_next_for(&cursor, &all[n], member) == 1) n++;
    if (!r && !n) r = 1; /* nothing to export */
    out_t o = {.buf = buf};
    if (!r) r = ink_fs_open_for(member, path, INK_FS_WRITE | INK_FS_CREATE | INK_FS_EXCL, &o.f);
    if (!r) {
        line(&o, "BEGIN:VCALENDAR");
        line(&o, "VERSION:2.0");
        line(&o, "PRODID:-//ink//calendar//ZH");
        for (int i = 0; i < n; i++) vevent(&o, &all[i], all, n);
        line(&o, "END:VCALENDAR");
        flush_out(&o);
        if (ink_fs_sync(o.f) && !o.err) o.err = INK_FS_ERR_IO;
        int closed = ink_fs_close(o.f);
        r = o.err ? o.err : closed;
        if (r) ink_fs_remove_for(member, path);
        else *count = n;
    }
    osal_free(buf);
    osal_free(all);
    return r;
}

/* ------------------------------------------------------------ reading (IcsImporter.kt:110-318, Parser.kt:17-71, :220-250) */

typedef struct {
    ink_event_t e;
    bool in_event, in_alarm, alarm_ok, bad_rule, has_start, has_end, child;
    int32_t alarm_lead;
    int64_t start_utc, end_utc, duration, modified, rid_utc;
    bool rid_date;
    char rule[256];
} cur_t;

static int zone_of(const char *params) {
    const char *tz = strstr(params, "TZID=");
    if (!tz) return -1;
    tz += 5;
    for (int i = 0; i < ink_time_zone_count(); i++) {
        size_t k = strlen(ink_time_zone_name(i));
        if (!strncmp(tz, ink_time_zone_name(i), k) && (tz[k] == ':' || tz[k] == ';' || !tz[k])) return i;
    }
    return -2; /* a zone we do not have: the device's, as Fossify */
}

/* "20261120", "20261120T200000", "...Z": UTC, and whether it was a date. */
static bool read_when(const char *params, const char *v, int64_t *utc, bool *date) {
    int y, m, d, hh = 0, mm = 0, ss = 0;
    if (sscanf(v, "%4d%2d%2d", &y, &m, &d) != 3) return false;
    *date = strlen(v) == 8 || (strstr(params, "VALUE=DATE") && !strstr(params, "DATE-TIME"));
    if (!*date && sscanf(v + 8, "T%2d%2d%2d", &hh, &mm, &ss) < 2) return false;
    int64_t naive = time_days_from_civil(y, m, d) * 86400 + hh * 3600 + mm * 60 + ss;
    int z = zone_of(params);
    if (!*date && strchr(v, 'Z')) *utc = naive;
    else if (!*date && z >= 0) { ink_tm_t t; ink_time_to_zone(z, naive, &t); ink_time_to_zone(z, naive - t.offset_min * 60, &t); *utc = naive - t.offset_min * 60; }
    else { ink_tm_t t = {.year = y, .month = m, .day = d, .hour = hh, .min = mm, .sec = ss}; *utc = ink_time_from_local(&t); }
    return true;
}

/* "-P1DT2H", "PT10M", "P1W": seconds, or false. */
static bool read_duration(const char *v, int64_t *out) {
    int sign = *v == '-' ? -1 : 1;
    if (*v == '-' || *v == '+') v++;
    if (*v++ != 'P') return false;
    int64_t total = 0;
    bool time = false;
    while (*v) {
        if (*v == 'T') { time = true; v++; continue; }
        char *end;
        long n = strtol(v, &end, 10);
        if (end == v) return false;
        switch (*end) {
        case 'W': total += n * 604800; break;
        case 'D': total += n * 86400; break;
        case 'H': total += n * 3600; break;
        case 'M': total += time ? n * 60 : 0; if (!time) return false; break;
        case 'S': total += n; break;
        default: return false;
        }
        v = end + 1;
    }
    *out = sign * total;
    return true;
}

/* Our rules only; anything else makes the event skipped (Fossify ignores what it does not know and may miscount). */
static bool read_rule(cur_t *c) {
    ink_event_t *e = &c->e;
    char copy[256];
    snprintf(copy, sizeof copy, "%s", c->rule);
    bool lunar = false;
    int bymonthday = 0, bymonth = 0, nth = 0, wday = -1;
    char *save = NULL;
    e->interval = 1;
    for (char *part = strtok_r(copy, ";", &save); part; part = strtok_r(NULL, ";", &save)) {
        char *v = strchr(part, '=');
        if (!v) return false;
        *v++ = 0;
        if (!strcmp(part, "FREQ")) {
            static const char *const F[] = {"DAILY", "WEEKLY", "MONTHLY", "YEARLY"};
            e->freq = 0;
            for (int i = 0; i < 4; i++) if (!strcmp(v, F[i])) e->freq = (uint8_t)(i + 1);
            if (!e->freq) return false;
        } else if (!strcmp(part, "RSCALE")) { if (strcmp(v, "CHINESE")) return false; lunar = true; }
        else if (!strcmp(part, "INTERVAL")) { int n = atoi(v); if (n < 1 || n > 99) return false; e->interval = (uint8_t)n; }
        else if (!strcmp(part, "COUNT")) { int n = atoi(v); if (n < 1 || n > 730) return false; e->count = (uint16_t)n; }
        else if (!strcmp(part, "UNTIL")) {
            int64_t utc; bool date;
            if (!read_when("", v, &utc, &date)) return false;
            ink_tm_t t; ink_time_to_local(utc, &t);
            if (date) sscanf(v, "%4d%2d%2d", &t.year, &t.month, &t.day);
            e->until_year = (int16_t)t.year; e->until_month = (uint8_t)t.month; e->until_day = (uint8_t)t.day;
        } else if (!strcmp(part, "BYDAY")) {
            for (char *s2 = NULL, *d = strtok_r(v, ",", &s2); d; d = strtok_r(NULL, ",", &s2)) {
                char *end; long n = strtol(d, &end, 10);
                int k = -1;
                for (int i = 0; i < 7; i++) if (!strcmp(end, DAYS[i])) k = i;
                if (k < 0) return false;
                if (end != d) { if (wday >= 0 || n == 0 || n < -1 || n > 5) return false; nth = (int)n; wday = k; }
                else e->weekdays |= (uint8_t)(1u << k);
            }
        } else if (!strcmp(part, "BYMONTHDAY")) { if (strchr(v, ',') || bymonthday) return false; bymonthday = atoi(v); if (!bymonthday || bymonthday < -1 || bymonthday > 31) return false; }
        else if (!strcmp(part, "BYMONTH")) {
            if (strchr(v, ',')) return false;
            bymonth = atoi(v);
            if (bymonth < 1 || bymonth > 12) return false;
            if (strchr(v, 'L')) { if (!lunar) return false; e->lleap = true; }
        } else if (strcmp(part, "WKST")) return false; /* BYSETPOS, BYHOUR, BYMINUTE, BYSECOND, BYYEARDAY, BYWEEKNO... */
    }
    if (lunar) {
        if (e->freq != INK_FREQ_YEARLY || !bymonth || bymonthday < 1 || bymonthday > 30 || wday >= 0 || e->weekdays) return false;
        e->freq = INK_FREQ_LUNAR_YEARLY; e->lmonth = (uint8_t)bymonth; e->lday = (uint8_t)bymonthday;
        return true;
    }
    if (e->weekdays && (e->freq != INK_FREQ_WEEKLY || wday >= 0)) return false;
    if (wday >= 0 && e->freq != INK_FREQ_MONTHLY && e->freq != INK_FREQ_YEARLY) return false;
    if ((bymonthday || bymonth) && e->freq != INK_FREQ_MONTHLY && e->freq != INK_FREQ_YEARLY) return false;
    if (bymonth && e->freq != INK_FREQ_YEARLY) return false;
    if (bymonthday && wday >= 0) return false;
    if (wday >= 0) { e->by = (uint8_t)(nth < 0 ? INK_BY_LAST_WEEKDAY : INK_BY_NTH_WEEKDAY); e->nth = (int8_t)nth; e->wday = (uint8_t)wday; }
    else if (bymonthday == -1) e->by = INK_BY_LAST_DAY;
    else if (bymonthday) e->rday = (uint8_t)bymonthday;
    if (bymonth) e->rmonth = (uint8_t)bymonth;
    return true;
}

static void unescape(const char *in, char *out, size_t cap) {
    size_t n = 0;
    for (; *in && n + 1 < cap; in++) {
        if (*in == '\\' && in[1]) { in++; out[n++] = *in == 'n' || *in == 'N' ? ' ' : *in; }
        else out[n++] = *in;
    }
    size_t k = n; /* a multi-byte character cut at the end goes */
    while (k && ((unsigned char)out[k - 1] & 0xC0) == 0x80) k--;
    if (k && ((unsigned char)out[k - 1] & 0xC0) == 0xC0) {
        unsigned char b = (unsigned char)out[k - 1];
        if (n - (k - 1) < (size_t)(b >= 0xF0 ? 4 : b >= 0xE0 ? 3 : 2)) n = k - 1;
    }
    out[n] = 0;
}

static void local_fields(ink_event_t *e, int64_t utc) {
    ink_tm_t t; ink_time_to_local(utc, &t);
    e->year = (int16_t)t.year; e->month = (uint8_t)t.month; e->day = (uint8_t)t.day; e->hour = (uint8_t)t.hour; e->min = (uint8_t)t.min;
}

typedef struct { ink_event_t e[PENDING]; int n; } pending_t;

/* An existing event with this UID (and, for an edited occurrence, the same original day). */
static bool existing(const char *member, const ink_event_t *e, ink_event_t *out) {
    int cursor = 0;
    while (ink_time_event_next_for(&cursor, out, member) == 1)
        if (!strcmp(out->uid, e->uid) && (out->parent != 0) == (e->parent != 0) && (!e->parent || out->orig_day == e->orig_day)) return true;
    return false;
}

static void finish(cur_t *c, const char *member, ink_ics_result_t *r, pending_t *later, ink_event_t *scratch) {
    ink_event_t *e = &c->e;
    if (!c->has_start || !e->title[0]) { r->skipped++; return; } /* IcsImporter.kt:305 */
    if (c->rule[0] && !read_rule(c)) { r->skipped++; return; }
    if (e->all_day) {
        if (c->has_end) e->duration_s = c->end_utc - c->start_utc;
        else if (c->duration) e->duration_s = c->duration;
        else { ink_tm_t t = {.year = e->year, .month = e->month, .day = e->day + 1}; e->duration_s = ink_time_from_local(&t) - c->start_utc; }
    } else e->duration_s = c->has_end ? c->end_utc - c->start_utc : c->duration;
    if (e->duration_s < 0) e->duration_s = 0;
    e->modified_s = c->modified;
    if (c->child) {
        if (c->rid_date) { ink_tm_t t; ink_time_to_local(c->rid_utc, &t); e->orig_day = (int32_t)time_days_from_civil(t.year, t.month, t.day); e->orig_min = 0; }
        else { ink_tm_t t; ink_time_to_local(c->rid_utc, &t); e->orig_day = (int32_t)time_days_from_civil(t.year, t.month, t.day); e->orig_min = t.hour * 60 + t.min; }
        e->parent = 1; /* attached once every series is in (IcsImporter.kt:280-289) */
        e->freq = INK_FREQ_NONE;
        if (later->n < PENDING) later->e[later->n++] = *e;
        else r->skipped++;
        return;
    }
    if (existing(member, e, scratch)) { /* IcsImporter.kt:310-318: not newer is there already; newer replaces it */
        if (scratch->modified_s >= e->modified_s) { r->existing++; return; }
        ink_time_event_delete_for(scratch->id, 0, 0, INK_SCOPE_ALL, member, false, NULL);
    }
    int id = ink_time_event_add_for(e, NULL, member);
    if (id > 0) r->imported++;
    else if (id == INK_EVENT_FULL) r->full++;
    else r->skipped++;
}

static void attach(const char *member, ink_ics_result_t *r, pending_t *later, ink_event_t *series) {
    for (int i = 0; i < later->n; i++) {
        ink_event_t *child = &later->e[i];
        if (existing(member, child, series) && series->modified_s >= child->modified_s) { r->existing++; continue; }
        child->parent = 0;
        int cursor = 0;
        bool found = false;
        while (!found && ink_time_event_next_for(&cursor, series, member) == 1) found = !series->parent && !strcmp(series->uid, child->uid);
        if (!found) { r->skipped++; continue; }
        child->id = series->id;
        ink_tm_t t = {0};
        time_civil_from_days(child->orig_day, &t.year, &t.month, &t.day);
        t.hour = child->orig_min / 60; t.min = child->orig_min % 60;
        int64_t occurrence = ink_time_from_local(&t);
        int rc = ink_time_event_update_for(child, occurrence, INK_SCOPE_THIS, member, NULL);
        if (!rc) r->imported++;
        else if (rc == INK_EVENT_FULL) r->full++;
        else r->skipped++;
    }
}

static void property(cur_t *c, char *text) {
    char *colon = strchr(text, ':');
    if (!colon) return;
    *colon = 0;
    char *name = text, *value = colon + 1, *params = strchr(text, ';');
    if (params) *params++ = 0;
    else params = "";
    ink_event_t *e = &c->e;
    if (c->in_alarm) {
        if (!strcmp(name, "ACTION")) c->alarm_ok = !strcmp(value, "DISPLAY") || !strcmp(value, "AUDIO");
        else if (!strcmp(name, "TRIGGER")) { /* before the start only: RELATED=END and moments are not ours */
            int64_t s;
            if (!strstr(params, "RELATED=END") && !strstr(params, "DATE-TIME") && read_duration(value, &s)) c->alarm_lead = (int32_t)(-s / 60);
            else c->alarm_ok = false;
        }
        return;
    }
    int64_t utc;
    bool date;
    if (!strcmp(name, "SUMMARY")) unescape(value, e->title, sizeof e->title);
    else if (!strcmp(name, "UID")) {
        if (strlen(value) < sizeof e->uid) snprintf(e->uid, sizeof e->uid, "%s", value);
        else { uint64_t h = 1469598103934665603ull; for (const char *p = value; *p; p++) h = (h ^ (unsigned char)*p) * 1099511628211ull; snprintf(e->uid, sizeof e->uid, "h-%016llx", (unsigned long long)h); }
    } else if (!strcmp(name, "DTSTART") && read_when(params, value, &utc, &date)) {
        c->has_start = true; c->start_utc = utc; e->all_day = date; local_fields(e, utc);
    } else if (!strcmp(name, "DTEND") && read_when(params, value, &utc, &date)) { c->has_end = true; c->end_utc = utc; }
    else if (!strcmp(name, "DURATION")) read_duration(value, &c->duration);
    else if (!strcmp(name, "RRULE")) snprintf(c->rule, sizeof c->rule, "%s", value);
    else if (!strcmp(name, "LAST-MODIFIED") && read_when(params, value, &utc, &date)) c->modified = utc;
    else if (!strcmp(name, "RECURRENCE-ID") && read_when(params, value, &utc, &date)) { c->child = true; c->rid_utc = utc; c->rid_date = date; }
    else if (!strcmp(name, "EXDATE")) {
        char *save = NULL;
        for (char *v = strtok_r(value, ",", &save); v; v = strtok_r(NULL, ",", &save))
            if (read_when(params, v, &utc, &date) && e->nex < INK_EVENT_EXCEPTIONS) {
                int32_t day = ink_event_day(utc);
                if (date) { int y, m, d; sscanf(v, "%4d%2d%2d", &y, &m, &d); day = (int32_t)time_days_from_civil(y, m, d); }
                e->ex[e->nex++] = day;
            }
    }
}

/* One whole content line. True at BEGIN:VCALENDAR. */
static bool logical(cur_t *c, char *text, const char *member, ink_ics_result_t *r, pending_t *later, ink_event_t *scratch) { /* text is changed */
    if (!strcmp(text, "BEGIN:VCALENDAR")) return true;
    if (!strcmp(text, "BEGIN:VEVENT")) { memset(c, 0, sizeof *c); c->in_event = true; }
    else if (!strcmp(text, "END:VEVENT") && c->in_event) { finish(c, member, r, later, scratch); c->in_event = false; }
    else if (!strcmp(text, "BEGIN:VALARM") && c->in_event) { c->in_alarm = true; c->alarm_ok = true; c->alarm_lead = 0; }
    else if (!strcmp(text, "END:VALARM") && c->in_alarm) {
        c->in_alarm = false;
        if (c->alarm_ok && c->e.nleads < INK_EVENT_LEADS) c->e.leads[c->e.nleads++] = c->alarm_lead;
    } else if (c->in_event) property(c, text);
    return false;
}

int ink_time_ics_import(const char *member, const char *path, ink_ics_result_t *r) {
    memset(r, 0, sizeof *r);
    const char *dot = strrchr(path, '.');
    if (!dot || (strcmp(dot, ".ics") && strcmp(dot, ".ICS"))) { r->error = 1; return 1; } /* not a calendar file */
    char *chunk = osal_malloc(CHUNK, OSAL_MEM_EXTERNAL), *text = osal_malloc(LINE_MAX_B, OSAL_MEM_EXTERNAL), *next = osal_malloc(LINE_MAX_B, OSAL_MEM_EXTERNAL);
    cur_t *c = osal_malloc(sizeof *c, OSAL_MEM_EXTERNAL);
    pending_t *later = osal_calloc(1, sizeof *later, OSAL_MEM_EXTERNAL);
    ink_event_t *scratch = osal_malloc(sizeof *scratch, OSAL_MEM_EXTERNAL);
    ink_fs_file_t *f = NULL;
    int err = chunk && text && next && c && later && scratch ? ink_fs_open_for(member, path, INK_FS_READ, &f) : INK_FS_ERR_IO;
    size_t tlen = 0, nlen = 0;
    bool calendar = false, have = false;
    if (c) memset(c, 0, sizeof *c);
    for (bool eof = false; !err && !eof;) {
        int64_t got = ink_fs_read(f, chunk, CHUNK);
        if (got < 0) { err = (int)got; break; }
        eof = got == 0;
        for (int64_t i = 0; i < (eof ? 1 : got); i++) {
            char ch = eof ? '\n' : chunk[i];
            if (ch == '\r') continue;
            if (ch != '\n') { if (nlen + 1 < LINE_MAX_B) next[nlen++] = ch; continue; }
            next[nlen] = 0;
            if (nlen && (next[0] == ' ' || next[0] == '\t') && have) { /* a folded line goes on (IcsImporter.kt:115-118) */
                size_t add = nlen - 1;
                if (tlen + add + 1 < LINE_MAX_B) { memcpy(text + tlen, next + 1, add + 1); tlen += add; }
            } else {
                if (have) { text[tlen] = 0; calendar |= logical(c, text, member, r, later, scratch); }
                memcpy(text, next, nlen + 1);
                tlen = nlen;
                have = nlen > 0;
            }
            nlen = 0;
        }
    }
    if (have && !err) { text[tlen] = 0; calendar |= logical(c, text, member, r, later, scratch); }
    if (f) ink_fs_close(f);
    if (!err && !calendar) err = 1;
    if (!err) attach(member, r, later, scratch);
    r->error = err;
    osal_free(scratch); osal_free(later); osal_free(c); osal_free(next); osal_free(text); osal_free(chunk);
    return err;
}
