/*
 * The lunar table against its source (Apple's Chinese calendar, generated 2026-11-04), against dates known
 * from public almanacs, and every day from 1901 to 2100 there and back.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ink/sys/time.h"
const char *ink_tr(const char *text) { return text; }
const char *ink_lang_code(void) { return "zh"; }

static int fails;
#define CHECK(c, ...)                 \
    do {                              \
        if (!(c)) {                   \
            printf("FAILED: ");       \
            printf(__VA_ARGS__);      \
            printf("\n");             \
            fails++;                  \
        }                             \
    } while (0)

static int days_in(int y, int m) {
    static const int D[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (m == 2 && ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0)) return 29;
    return D[m - 1];
}

int main(int argc, char **argv) {
    /* spring festivals and leap months from almanacs */
    static const struct { int y, m, d, leap; } KNOWN[] = {
        {2000, 2, 5, 0}, {2020, 1, 25, 4}, {2023, 1, 22, 2}, {2024, 2, 10, 0}, {2025, 1, 29, 6},
        {2026, 2, 17, 0}, {2027, 2, 6, 0}, {2028, 1, 26, 5}, {2033, 1, 31, 11}, {2034, 2, 19, 0},
    };
    for (size_t i = 0; i < sizeof KNOWN / sizeof KNOWN[0]; i++) {
        ink_lunar_t l;
        CHECK(ink_lunar_from_solar(KNOWN[i].y, KNOWN[i].m, KNOWN[i].d, &l) && l.year == KNOWN[i].y && l.month == 1 && l.day == 1 && !l.leap, "new year %d", KNOWN[i].y);
        CHECK(ink_lunar_leap_month(KNOWN[i].y) == KNOWN[i].leap, "leap month of %d: %d", KNOWN[i].y, ink_lunar_leap_month(KNOWN[i].y));
    }
    /* a known day: the ninth month of 2026 begins on October 10, so November 4 is its twenty-sixth; 丙午, horse */
    ink_lunar_t l;
    char name[32], gz[16];
    CHECK(ink_lunar_from_solar(2026, 11, 4, &l) && l.year == 2026 && l.month == 9 && l.day == 26 && !l.leap, "2026-11-04 is %d-%d-%d", l.year, l.month, l.day);
    CHECK(ink_lunar_name(&l, name, sizeof name) > 0 && !strcmp(name, "九月廿六"), "name %s", name);
    CHECK(ink_lunar_ganzhi(2026, gz, sizeof gz) > 0 && !strcmp(gz, "丙午") && !strcmp(ink_lunar_zodiac(2026), "马"), "2026 is %s %s", gz, ink_lunar_zodiac(2026));
    ink_lunar_t leap = {2023, 2, 1, true};
    int y, m, d;
    CHECK(ink_lunar_to_solar(&leap, &y, &m, &d) && y == 2023 && m == 3 && d == 22, "the leap second month of 2023 begins %d-%d-%d", y, m, d);
    CHECK(ink_lunar_name(&leap, name, sizeof name) > 0 && !strcmp(name, "闰二月初一"), "name %s", name);
    ink_lunar_t nope = {2026, 2, 30, false};
    CHECK(!ink_lunar_to_solar(&nope, &y, &m, &d), "a day a month does not have");
    ink_lunar_t noleap = {2026, 3, 1, true};
    CHECK(!ink_lunar_to_solar(&noleap, &y, &m, &d), "a leap month a year does not have");
    CHECK(!ink_lunar_from_solar(1899, 12, 31, &l) && !ink_lunar_from_solar(2200, 1, 1, &l), "outside the table");

    /* the source, line by line */
    FILE *f = argc > 1 ? fopen(argv[1], "r") : NULL;
    CHECK(f != NULL, "the fixture opens");
    int lines = 0;
    static char line[256];
    while (f && fgets(line, sizeof line, f)) {
        int yy, mm, dd;
        char *rest = NULL;
        yy = (int)strtol(line, &rest, 10);
        mm = (int)strtol(rest, &rest, 10);
        dd = (int)strtol(rest, &rest, 10);
        lines++;
        ink_lunar_t ny;
        CHECK(ink_lunar_from_solar(yy, mm, dd, &ny) && ny.year == yy && ny.month == 1 && ny.day == 1, "new year of %d", yy);
        int leapm = 0;
        for (char *p = strtok(rest, " ,\n"); p; p = strtok(NULL, " ,\n")) {
            int mo = atoi(p);
            bool is_leap = strchr(p, 'L') != NULL;
            int len = atoi(strchr(p, ':') + 1);
            if (is_leap) leapm = mo;
            CHECK(ink_lunar_month_days(yy, mo, is_leap) == len, "%d month %d%s: %d", yy, mo, is_leap ? " leap" : "", len);
        }
        CHECK(ink_lunar_leap_month(yy) == leapm, "%d's leap month", yy);
    }
    if (f) fclose(f);
    CHECK(lines == 200, "200 years (%d)", lines);

    /* every day: there and back, one day after another */
    ink_lunar_t prev = {0};
    int days = 0;
    for (int yy = 1901; yy <= 2100; yy++)
        for (int mm = 1; mm <= 12; mm++)
            for (int dd = 1; dd <= days_in(yy, mm); dd++) {
                ink_lunar_t cur;
                if (yy == 1901 && mm == 1) continue; /* before the first new year of the table */
                if (yy == 1901 && mm == 2 && dd < 19) continue;
                if (!ink_lunar_from_solar(yy, mm, dd, &cur)) {
                    CHECK(yy == 2100 || yy == 2101, "%d-%d-%d has no lunar date", yy, mm, dd); /* past the table's end */
                    continue;
                }
                int ry, rm, rd;
                CHECK(ink_lunar_to_solar(&cur, &ry, &rm, &rd) && ry == yy && rm == mm && rd == dd, "%d-%d-%d does not come back (%d-%d-%d)", yy, mm, dd, ry, rm, rd);
                if (prev.year) CHECK(cur.day == prev.day + 1 || cur.day == 1, "%d-%d-%d jumps", yy, mm, dd);
                prev = cur;
                days++;
            }
    CHECK(days >= 73000, "days checked: %d", days);
    if (fails) printf("%d failures\n", fails);
    else printf("lunar: %d days and 200 years are right\n", days);
    return fails ? 1 : 0;
}
