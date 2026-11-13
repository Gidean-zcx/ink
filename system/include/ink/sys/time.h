/*
 * Time service (foundation §7.1, system services design §5): the clock and where
 * it comes from, time zones, the lunar calendar, alarms and reminders. Records
 * keep UTC; people see local time. Durations and timeouts use the monotonic
 * clock (ink_now_us), which changing the time does not touch.
 */
#ifndef INK_SYS_TIME_H
#define INK_SYS_TIME_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int year, month, day;    /* 1..12, 1..31 */
    int hour, min, sec;
    int wday;                /* 0 = Sunday */
    int offset_min;          /* from UTC, daylight saving included */
    bool dst;
} ink_tm_t;

/** After the state and settings tables, the data store and the power service; hub optional. */
int ink_time_init(void);
/** Is the wall clock known? False after the RTC lost its time, until the hub or a person says. */
bool ink_time_valid(void);
/** UTC seconds; 0 while it is not known. */
int64_t ink_time_now_s(void);
void ink_time_to_local(int64_t utc_s, ink_tm_t *out);
/** Local time to UTC. A time the clocks skip moves forward by the skip; one that happens twice is the first. */
int64_t ink_time_from_local(const ink_tm_t *tm);
/** Set the clock (and the RTC). A person's choice or the hub's: alarms move with it. */
void ink_time_set(int64_t utc_s, bool from_hub);
int ink_time_set_if_revision(int64_t utc_s, bool from_hub, uint32_t expected, uint32_t *written);
/** The time zone's name, e.g. "Asia/Shanghai". */
const char *ink_time_zone(void);
/** Stable setting indices; NULL outside the compact modern-zone table. Cities are Chinese i18n keys. */
int ink_time_zone_count(void);
const char *ink_time_zone_name(int index);
const char *ink_time_zone_city(int index);
/** World clocks can convert without changing the device's zone. Invalid index uses Shanghai. */
void ink_time_to_zone(int index, int64_t utc_s, ink_tm_t *out);

/* ---- the lunar calendar, 1901 to 2100 ---- */
typedef struct {
    int year;  /* the Gregorian number of the year it mostly falls in */
    int month; /* 1..12 */
    int day;   /* 1..30 */
    bool leap; /* the leap month after `month` */
} ink_lunar_t;

bool ink_lunar_from_solar(int y, int m, int d, ink_lunar_t *out);
/** False if that lunar date does not exist. */
bool ink_lunar_to_solar(const ink_lunar_t *l, int *y, int *m, int *d);
/** "闰四月初一". Length, or -1. */
int ink_lunar_name(const ink_lunar_t *l, char *out, size_t cap);
const char *ink_lunar_zodiac(int year);
/** "丙午". Length, or -1. */
int ink_lunar_ganzhi(int year, char *out, size_t cap);
/** The leap month of a lunar year (0 none), the length of a month. */
int ink_lunar_leap_month(int year);
int ink_lunar_month_days(int year, int month, bool leap);
/** The solar term that starts on that day ("立春"), or NULL (lunar.c, 1901-2100). */
const char *ink_lunar_solar_term(int year, int month, int day);

/* ---- alarms and reminders ---- */
typedef enum { INK_ALARM = 0, INK_REMINDER } ink_alarm_kind_t;
typedef enum {
    INK_REPEAT_NONE = 0,
    INK_REPEAT_WEEKDAYS,    /* `weekdays`: bit 0 = Sunday */
    INK_REPEAT_MONTHLY,     /* the same day of each month that has it */
    INK_REPEAT_YEARLY,
    INK_REPEAT_LUNAR_YEARLY /* the same lunar date: birthdays */
} ink_repeat_t;

/* What is kept for an alarm (one fixed record, schema 1, in the device's data area). */
typedef struct {
    uint32_t id;
    uint8_t kind, repeat, weekdays, src; /* src: who set it (ink_source_t) */
    bool enabled;
    uint8_t hour, min;     /* local time of day */
    uint8_t lmonth, lday;  /* lunar repeat */
    bool lleap;
    int16_t year;          /* local date of a single or a dated one; 0 = the next time that hour and minute comes */
    uint8_t month, day;
    int64_t at_utc;        /* a reminder at a moment, whatever the zone becomes; 0 = local time above */
    int64_t next_s;        /* when it goes off next (0 = not scheduled) */
    int64_t snooze_until;
    char owner[16];        /* whose it is; it rings whoever is using the device */
    char title[48];
    bool silent, all_day;  /* zero keeps legacy reminders audible */
    int64_t duration_s;    /* exclusive end, relative to each occurrence; 0 = a point */
} ink_alarm_t;

/** Add: fills id and next_s. > 0 the id, < 0 refused (table full, not valid). */
int ink_alarm_add(ink_alarm_t *a);
/** Runtime mutation version for an inverse; the stored alarm format is unchanged. */
int ink_alarm_add_versioned(ink_alarm_t *a, uint32_t *written);
/** Erase owned schedules and ringing state durably before releasing a member id. */
int ink_time_forget_member(const char *member, int64_t timeout_us);
int ink_alarm_cancel(uint32_t id);
/** Cancel only the unchanged alarm: -2 if its runtime version is stale. */
int ink_alarm_cancel_if_revision(uint32_t id, uint32_t revision);
/** The next of the current member's, after *cursor (start from 0): 1, 0 at the end. */
int ink_alarm_next(int *cursor, ink_alarm_kind_t kind, ink_alarm_t *out);
/** Stop a ringing alarm (id 0: all). Repeats stay scheduled. */
int ink_alarm_dismiss(uint32_t id);
/** Ring again in `minutes` (0: 9). */
int ink_alarm_snooze(uint32_t id, int minutes);

/** First enabled occurrence overlapping or following from_s; ends are exclusive. */
int64_t ink_alarm_occurrence(const ink_alarm_t *alarm, int64_t from_s);

/*
 * Calendar events (calendar-3): what a reminder is since then, in their own collection "time.events" so the
 * alarm record keeps every byte. INK_REMINDER through the ink_alarm_* calls above reads and writes these
 * (ink_alarm_next gives a projection: next_s is the next start, the title cut to 47 bytes on a character).
 * Repeats follow Fossify Calendar and Etar (docs/tasks/calendar-3/SOURCE.md).
 */
enum { INK_EVENT_TITLE = 96, INK_EVENT_UID = 64, INK_EVENT_LEADS = 3, INK_EVENT_EXCEPTIONS = 16, INK_EVENT_MAX = 128 };
typedef enum { INK_FREQ_NONE = 0, INK_FREQ_DAILY, INK_FREQ_WEEKLY, INK_FREQ_MONTHLY, INK_FREQ_YEARLY, INK_FREQ_LUNAR_YEARLY } ink_freq_t;
/* Which day of a month (monthly) or of rmonth (yearly). */
typedef enum { INK_BY_DAY = 0, INK_BY_NTH_WEEKDAY, INK_BY_LAST_WEEKDAY, INK_BY_LAST_DAY } ink_by_t;
typedef enum { INK_SCOPE_ALL = 0, INK_SCOPE_THIS, INK_SCOPE_FOLLOWING } ink_scope_t;
enum {
    INK_EVENT_INVALID = -1, INK_EVENT_PAST = -2, INK_EVENT_FULL = -3, INK_EVENT_STORAGE = -4, INK_EVENT_GONE = -5,
    INK_EVENT_EXCEPTIONS_FULL = -6, INK_EVENT_NOT_FOUND = -7, INK_EVENT_STALE = -8, INK_EVENT_UNTIL_BEFORE = -9,
    INK_EVENT_END_BEFORE = -10, INK_EVENT_NO_TITLE = -11
};

typedef struct {
    uint32_t id, parent;     /* parent: the series an edited occurrence came out of (0 none) */
    char owner[16];
    char title[INK_EVENT_TITLE];
    char uid[INK_EVENT_UID]; /* iCalendar UID: kept from an import, made up for our own */
    uint8_t src, freq, by, interval;  /* interval 1..99 */
    uint8_t weekdays;        /* weekly: bit 0 = Sunday */
    uint8_t rmonth, rday, wday;       /* the rule's month, day of the month, weekday (0 Sunday) */
    int8_t nth;              /* INK_BY_NTH_WEEKDAY: 1..5 */
    uint8_t lmonth, lday;    /* lunar yearly */
    bool lleap, all_day;
    uint8_t nleads, nex, reserved;
    int16_t year;            /* local start of the first occurrence */
    uint8_t month, day, hour, min;
    int16_t until_year;      /* last day it may happen, local, inclusive; 0 = no end date */
    uint8_t until_month, until_day;
    uint16_t count;          /* how many times in all; 0 = no count */
    int32_t leads[INK_EVENT_LEADS]; /* minutes before the start; on an all-day event a negative one is that many minutes after its midnight */
    int32_t ex[INK_EVENT_EXCEPTIONS]; /* local days (since 1970) of occurrences taken out (deleted or edited apart) */
    int32_t orig_day, orig_min; /* an edited occurrence: where it was, local day and minute */
    int64_t at_utc;          /* a single event at a moment, whatever the zone becomes; 0 = local time above */
    int64_t duration_s;      /* exclusive end after each start; 0 = a point */
    int64_t modified_s;
    int64_t done_s;          /* reminders up to this moment went off or were reported missed */
} ink_event_t;

/** The current member's events after *cursor (start from 0): 1, 0 at the end. */
int ink_event_next(int *cursor, ink_event_t *out);
/** One of the current member's: 0, or INK_EVENT_NOT_FOUND. */
int ink_event_get(uint32_t id, ink_event_t *out);
/** Start of the first occurrence overlapping or following from_s (ends exclusive, exceptions out); 0 none. */
int64_t ink_event_occurrence(const ink_event_t *e, int64_t from_s);
/** How many occurrences come before the one starting on local day `day` (that one's index), or -1. */
int ink_event_index(const ink_event_t *e, int32_t day);
/** Local days since 1970 of a UTC moment, in the device's zone. */
int32_t ink_event_day(int64_t utc_s);

#ifdef __cplusplus
}
#endif
#endif
