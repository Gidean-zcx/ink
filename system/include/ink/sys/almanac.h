/* Offline civil-day almanac; folk information uses Mogo's pinned rules. */
#ifndef INK_SYS_ALMANAC_H
#define INK_SYS_ALMANAC_H

#include <stdbool.h>
#include "ink/sys/cap.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int year, month, day, weekday, days, first_weekday;
    int lunar_year, lunar_month, lunar_day; /* Negative month means leap month. */
    int year_cycle, month_cycle, day_cycle; /* 0..59, starting at Jiazi. */
    const char *yi, *ji, *wealth, *clash, *sha;
    const char *joy, *fortune, *auspicious, *inauspicious, *officer, *guardian, *guardian_type;
    const char *year_ganzhi, *month_ganzhi, *day_ganzhi, *solar_term;
    char lunar_text[48];
} ink_almanac_t;

/* Local Gregorian date, 2000..2099. False clears out; strings live in flash. */
bool ink_almanac_date(int year, int month, int day, ink_almanac_t *out);
/* Time service capability handler; clients use time.almanac on the soft bus. */
void ink_almanac_cap_run(ink_call_t *call);

#ifdef __cplusplus
}
#endif
#endif
