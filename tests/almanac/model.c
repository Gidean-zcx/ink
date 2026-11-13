/* Host-only model driver. The clock, allocator and receipt sink are port stubs. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "ink/osal.h"
#include "ink/sys/almanac.h"
#include "ink/sys/json.h"
#include "ink/sys/time.h"

void *osal_malloc(size_t size, uint32_t caps) { (void)caps; return malloc(size); }
void osal_free(void *p) { free(p); }
void osal_cost_units(const char *unit, uint64_t count) { (void)unit; (void)count; }
const char *ink_tr(const char *s) { return s; }
const char *ink_lang_code(void) { return "zh"; }
bool ink_time_valid(void) { return false; }
int64_t ink_time_now_s(void) { return 0; }
void ink_time_to_local(int64_t s, ink_tm_t *out) { (void)s; memset(out, 0, sizeof *out); }
void ink_cap_result(ink_call_t *c, const char *json) { (void)c; (void)json; abort(); }

static void print_date(int y, int m, int day) {
    ink_almanac_t d;
    if (!ink_almanac_date(y, m, day, &d)) exit(2);
    char out[4096]; ink_jw_t w;
    ink_jw_init(&w, out, sizeof out); ink_jw_raw(&w, "{");
#define NUMBER(field) ink_jw_raw(&w, "\"" #field "\":"); ink_jw_int(&w, d.field); ink_jw_raw(&w, ",")
#define TEXT(field) ink_jw_raw(&w, "\"" #field "\":"); ink_jw_str(&w, d.field); ink_jw_raw(&w, ",")
    NUMBER(year); NUMBER(month); NUMBER(day); NUMBER(weekday); NUMBER(days); NUMBER(first_weekday);
    NUMBER(lunar_year); NUMBER(lunar_month); NUMBER(lunar_day);
    NUMBER(year_cycle); NUMBER(month_cycle); NUMBER(day_cycle);
    TEXT(yi); TEXT(ji); TEXT(wealth); TEXT(clash); TEXT(sha); TEXT(joy); TEXT(fortune);
    TEXT(auspicious); TEXT(inauspicious); TEXT(officer); TEXT(guardian); TEXT(guardian_type);
    TEXT(year_ganzhi); TEXT(month_ganzhi); TEXT(day_ganzhi); TEXT(solar_term); TEXT(lunar_text);
#undef NUMBER
#undef TEXT
    ink_lunar_t l;
    if (!ink_lunar_from_solar(y, m, day, &l)) exit(3);
    char legacy[96];
    snprintf(legacy, sizeof legacy, "\"legacy\":[%d,%d,%d]}", l.year, l.leap ? -l.month : l.month, l.day);
    ink_jw_raw(&w, legacy);
    if (w.overflow || puts(out) == EOF) exit(4);
}

int main(int argc, char **argv) {
    if (argc == 2 && !strcmp(argv[1], "--benchmark")) {
        ink_almanac_t d; volatile int sum = 0;
        struct timespec before, after;
        if (clock_gettime(CLOCK_MONOTONIC, &before)) return 5;
        for (int i = 0; i < 1000000; i++) {
            if (!ink_almanac_date(2000 + i % 100, 1 + i % 12, 1 + i % 28, &d)) return 2;
            sum += d.lunar_day;
        }
        if (clock_gettime(CLOCK_MONOTONIC, &after)) return 5;
        double ns = (after.tv_sec - before.tv_sec) * 1e9 + after.tv_nsec - before.tv_nsec;
        printf("host: 1000000 lookups, %.1f ns/lookup, checksum=%d\n", ns / 1e6, sum);
        return 0;
    }
    ink_almanac_t invalid;
    const int bad[][3] = {{1999,12,31}, {2100,1,1}, {2026,2,29}, {2000,0,1}, {2026,13,1}, {2026,1,0}};
    for (size_t i = 0; i < sizeof bad / sizeof *bad; i++) {
        memset(&invalid, 0xA5, sizeof invalid);
        if (ink_almanac_date(bad[i][0], bad[i][1], bad[i][2], &invalid) || invalid.year || invalid.yi[0]) return 6;
    }
    if (ink_almanac_date(2026, 2, 17, NULL)) return 7;
    char line[80];
    while (fgets(line, sizeof line, stdin)) {
        int y, m, d;
        if (sscanf(line, "%d %d %d", &y, &m, &d) != 3) return 1;
        print_date(y, m, d);
    }
    return ferror(stdin) || ferror(stdout) ? 8 : 0;
}
