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
/** The time zone's name, e.g. "Asia/Shanghai". */
const char *ink_time_zone(void);

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
} ink_alarm_t;

/** Add: fills id and next_s. > 0 the id, < 0 refused (table full, not valid). */
int ink_alarm_add(ink_alarm_t *a);
int ink_alarm_cancel(uint32_t id);
/** The next of the current member's, after *cursor (start from 0): 1, 0 at the end. */
int ink_alarm_next(int *cursor, ink_alarm_kind_t kind, ink_alarm_t *out);
/** Stop a ringing alarm (id 0: all). Repeats stay scheduled. */
int ink_alarm_dismiss(uint32_t id);
/** Ring again in `minutes` (0: 9). */
int ink_alarm_snooze(uint32_t id, int minutes);

#ifdef __cplusplus
}
#endif
#endif
