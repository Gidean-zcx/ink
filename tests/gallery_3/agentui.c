/* agentui_*: each function copied from AgentUI, called from C through system/apps/gallery/repository.cpp.
 * Local time is a fixed UTC+8 here (the time service is not linked); TR() returns its key. */
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "ink/sys/time.h"
#include "repository.h"

static int failures, checks;
#define CHECK(c, ...) do { checks++; if (!(c)) { failures++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

const char *ink_tr(const char *zh) { return zh; }
void ink_time_to_local(int64_t utc_s, ink_tm_t *out) {
    time_t t = (time_t)(utc_s + 8 * 3600); struct tm g; gmtime_r(&t, &g);
    *out = (ink_tm_t){g.tm_year + 1900, g.tm_mon + 1, g.tm_mday, g.tm_hour, g.tm_min, g.tm_sec, g.tm_wday, 480, false};
}
static int64_t local(int y, int mo, int d, int h, int mi, int s) { /* the UTC second of a UTC+8 wall time (days from civil) */
    y -= mo <= 2; int64_t era = (y >= 0 ? y : y - 399) / 400; int64_t yoe = y - era * 400;
    int64_t doy = (153 * (mo + (mo > 2 ? -3 : 9)) + 2) / 5 + d - 1, doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return (era * 146097 + doe - 719468) * 86400 + h * 3600 + mi * 60 + s - 8 * 3600;
}

static void agentui_isjpg(void) {
    CHECK(ink_gallery_is_jpg("a.jpg") && ink_gallery_is_jpg("B.JPG") && ink_gallery_is_jpg("c.jpeg") && ink_gallery_is_jpg("D.JpEg"), "jpg and jpeg in any case");
    CHECK(!ink_gallery_is_jpg("x.png") && !ink_gallery_is_jpg("jpg") && !ink_gallery_is_jpg(".jpg") && !ink_gallery_is_jpg("a.jpgx") && !ink_gallery_is_jpg(NULL), "not a jpeg name");
}
static void agentui_format_date(void) {
    char s[16];
    ink_gallery_date_key(local(2026, 11, 13, 0, 0, 0), '-', s, sizeof s); CHECK(!strcmp(s, "2026-11-13"), "midnight belongs to the new day: %s", s);
    ink_gallery_date_key(local(2026, 11, 12, 23, 59, 59), '-', s, sizeof s); CHECK(!strcmp(s, "2026-11-12"), "a second before is the old day: %s", s);
    ink_gallery_date_key(local(2026, 12, 31, 23, 59, 59), '-', s, sizeof s); CHECK(!strcmp(s, "2026-12-31"), "the year's last second: %s", s);
    ink_gallery_date_key(local(2027, 1, 1, 0, 0, 0), '-', s, sizeof s); CHECK(!strcmp(s, "2027-01-01"), "across the year: %s", s);
    ink_gallery_date_key(local(2027, 1, 1, 0, 0, 0), '/', s, sizeof s); CHECK(!strcmp(s, "2027/01/01"), "the separator is the caller's: %s", s);
    ink_gallery_date_key(local(2026, 9, 5, 7, 0, 0), '-', s, 8); CHECK(!strcmp(s, "2026-09"), "a short buffer keeps the month key: %s", s);
}
static void agentui_day_label(void) {
    char s[32];
    ink_gallery_day_label(local(2026, 11, 13, 9, 30, 0), s, sizeof s); CHECK(!strcmp(s, "11月13日"), "day label: %s", s);
    ink_gallery_day_label(local(2027, 1, 1, 0, 0, 0), s, sizeof s); CHECK(!strcmp(s, "1月1日"), "no leading zeros: %s", s);
}
static void agentui_time_label(void) {
    char s[16];
    ink_gallery_time_label(local(2026, 11, 13, 14, 5, 59), s, sizeof s); CHECK(!strcmp(s, "14:05"), "minutes, seconds dropped: %s", s);
    ink_gallery_time_label(local(2026, 11, 13, 0, 0, 0), s, sizeof s); CHECK(!strcmp(s, "00:00"), "midnight: %s", s);
}
static void agentui_size_label(void) {
    static const struct { int64_t bytes; const char *text; } CASES[] = {
        {0, "大小未知"}, {-5, "大小未知"}, {1, "1 KB"}, {1023, "1 KB"}, {1024, "1 KB"}, {1025, "2 KB"},
        {1048575, "1024 KB"}, {1048576, "1.0 MB"}, {1572864, "1.5 MB"}, {4194304, "4.0 MB"}};
    for (unsigned i = 0; i < sizeof CASES / sizeof *CASES; i++) {
        char s[24]; ink_gallery_size_label(CASES[i].bytes, s, sizeof s);
        CHECK(!strcmp(s, CASES[i].text), "%lld bytes: %s, not %s", (long long)CASES[i].bytes, s, CASES[i].text);
    }
}
static void sort_newest_first(void) {
    ink_gallery_item_t it[6] = {
        {.id = 1, .modified = 100, .name = "b.png"}, {.id = 2, .modified = 300, .name = "a.png"}, {.id = 3, .modified = -1, .name = "z.png"},
        {.id = 4, .modified = 300, .name = "c.png"}, {.id = 5, .modified = 200, .name = "m.png"}, {.id = 6, .modified = 300, .name = "c.png"}};
    ink_gallery_sort(it, 6);
    CHECK(it[0].modified == 300 && it[1].modified == 300 && it[2].modified == 300 && it[3].id == 5 && it[4].id == 1 && it[5].id == 3, "newest first, unknown last");
    CHECK(!strcmp(it[0].name, "c.png") && !strcmp(it[1].name, "c.png") && !strcmp(it[2].name, "a.png"), "equal times: names descending (%s %s %s)", it[0].name, it[1].name, it[2].name);
    ink_gallery_sort(it, 0); CHECK(it[3].id == 5, "an empty list changes nothing");
}
static size_t groups[8][2], ngroups;
static void collect(void *ctx, size_t begin, size_t end) { (void)ctx; if (ngroups < 8) { groups[ngroups][0] = begin; groups[ngroups][1] = end; } ngroups++; }
static void month_groups(void) {
    ink_gallery_item_t it[6] = {{.month = "2026-11"}, {.month = "2026-11"}, {.month = "2026-10"}, {.month = "2025-10"}, {.month = "undated"}, {.month = "undated"}};
    ngroups = 0; ink_gallery_month_groups(it, 6, collect, NULL);
    CHECK(ngroups == 4 && groups[0][0] == 0 && groups[0][1] == 2 && groups[1][1] == 3 && groups[2][1] == 4 && groups[3][0] == 4 && groups[3][1] == 6,
          "neighbours with the same key, the same month of another year apart (%zu groups)", ngroups);
    ngroups = 0; ink_gallery_month_groups(it, 0, collect, NULL); CHECK(ngroups == 0, "no photos, no rows");
}
int main(void) {
    agentui_isjpg(); agentui_format_date(); agentui_day_label(); agentui_time_label(); agentui_size_label(); sort_newest_first(); month_groups();
    printf("agentui: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
