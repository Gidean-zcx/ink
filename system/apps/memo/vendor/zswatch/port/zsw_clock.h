/* Shim for the copied ZSWatch voice memo UI only (see SOURCE.md): the watch clock is the system time service. */
#pragma once
#include <string.h>
#include <time.h>
#include "ink/sys/time.h"

typedef struct { int64_t utc_s; } zsw_timeval_t;

static inline void zsw_clock_get_time(zsw_timeval_t *tv) { tv->utc_s = ink_time_now_s(); }
static inline void zsw_timeval_to_tm(const zsw_timeval_t *tv, struct tm *out) {
    ink_tm_t t;
    ink_time_to_local(tv->utc_s, &t);
    memset(out, 0, sizeof *out);
    out->tm_year = t.year - 1900; out->tm_mon = t.month - 1; out->tm_mday = t.day;
    out->tm_hour = t.hour; out->tm_min = t.min; out->tm_sec = t.sec; out->tm_wday = t.wday;
}
