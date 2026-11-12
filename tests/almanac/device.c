/* Whole-product soft-bus checks with synthetic dates and existing members. */
#include "ink/sys/almanac.h"
#include "ink/sys/time.h"
#define ink_main product_main
#include "../../system/boot/main.c"
#undef ink_main
#include "../integration/whole_product.h"

static const char *case_name;
static void report(void *arg) {
    (void)arg;
    printf("ALMANAC %s status=%d say=%s detail=%s json=%s\n", case_name, g_rc.status, g_rc.say, g_rc.detail, g_result);
}
static void compare(int y, int m, int day) {
    ink_almanac_t d;
    CHECK(ink_almanac_date(y, m, day, &d), "C interface accepts date");
    ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL);
    CHECK(j && ink_json_parse(j, g_result, strlen(g_result)) == 0, "result is valid JSON: %.150s", g_result);
    if (!j) return;
    const char *numbers[] = {"year", "month", "day", "weekday", "days", "first_weekday", "lunar_year", "lunar_month", "lunar_day", "year_cycle", "month_cycle", "day_cycle"};
    const int values[] = {d.year, d.month, d.day, d.weekday, d.days, d.first_weekday, d.lunar_year, d.lunar_month, d.lunar_day, d.year_cycle, d.month_cycle, d.day_cycle};
    _Static_assert(sizeof numbers / sizeof *numbers == sizeof values / sizeof *values, "number fields match");
    for (size_t i = 0; i < sizeof values / sizeof *values; i++) {
        int64_t v = -1;
        CHECK(ink_json_int(j, ink_json_find(j, 0, numbers[i]), &v) && v == values[i], "same numeric field: %s", numbers[i]);
    }
    const char *names[] = {"yi", "ji", "wealth", "clash", "sha", "joy", "fortune", "auspicious", "inauspicious", "officer", "guardian", "guardian_type", "year_ganzhi", "month_ganzhi", "day_ganzhi", "solar_term", "lunar_text", "folk_notice"};
    const char *texts[] = {d.yi, d.ji, d.wealth, d.clash, d.sha, d.joy, d.fortune, d.auspicious, d.inauspicious, d.officer, d.guardian, d.guardian_type, d.year_ganzhi, d.month_ganzhi, d.day_ganzhi, d.solar_term, d.lunar_text, "这些是民俗说法"};
    _Static_assert(sizeof names / sizeof *names == sizeof texts / sizeof *texts, "text fields match");
    for (size_t i = 0; i < sizeof texts / sizeof *texts; i++) {
        char value[512];
        CHECK(ink_json_str(j, ink_json_find(j, 0, names[i]), value, sizeof value) >= 0 && !strcmp(value, texts[i]), "same text field: %s", names[i]);
    }
    osal_free(j);
}
static void query(const char *name, const char *args, int y, int m, int day) {
    case_name = name;
    CHECK(AI("time.almanac", args) && g_rc.status == INK_RC_DONE, "AI reads without confirmation: %s (%s)", name, g_rc.detail);
    compare(y, m, day);
    CHECK(strstr(g_rc.say, "这些是民俗说法"), "receipt explains folk information");
    vtk_host_call(report, NULL);
}
static void run(void *arg) {
    (void)arg;
    CHECK(WAIT_FOR(ink_member_space_open(), 10000), "member space opens"); test_start_home();
    const ink_cap_t *cap = ink_cap_find("time.almanac");
    CHECK(cap && cap->child_ok && cap->effect == INK_FX_READ && ink_cap_ai_callable(cap), "registered read-only AI and child capability");
    ink_receipt_t rc;
    CHECK(ink_setting_set("time.zone", 1, INK_SRC_SYSTEM, &rc) == INK_RC_DONE, "set UTC");
    ink_time_set(ink_time_from_local(&(ink_tm_t){.year=2026, .month=2, .day=16, .hour=20}), false);
    query("today-UTC", NULL, 2026, 2, 16);
    CHECK(ink_setting_set("time.zone", 0, INK_SRC_SYSTEM, &rc) == INK_RC_DONE, "set Shanghai");
    query("today-Shanghai", "{}", 2026, 2, 17);
    query("new-year", "{\"date\":\"2026-02-17\"}", 2026, 2, 17);
    query("first-date", "{\"date\":\"2000-01-01\"}", 2000, 1, 1);
    query("last-date", "{\"date\":\"2099-12-31\"}", 2099, 12, 31);
    case_name = "outside";
    CHECK(AI("time.almanac", "{\"date\":\"1999-12-31\"}") && g_rc.status == INK_RC_INVALID && strstr(g_rc.say, "不在范围"), "out of range is explicit");
    vtk_host_call(report, NULL);
    CHECK(AI("time.almanac", "{\"date\":\"2026-02-29\"}") && g_rc.status == INK_RC_INVALID, "reject invalid day");
    CHECK(AI("time.almanac", "{\"date\":\"2026-2-17\"}") && g_rc.status == INK_RC_INVALID, "reject malformed date");
    CHECK(BY_HAND("system.switch_member", "{\"id\":\"kid\"}") && g_rc.status == INK_RC_DONE, "person switches to child");
    query("child-new-year", "{\"date\":\"2026-02-17\"}", 2026, 2, 17);
    ink_confirm_view_t pending;
    CHECK(ink_confirm_list(&pending, 1) == 0, "no confirmation card was created");
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name = "almanac-test", .fn = run, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    if (osal_task_create(&cfg, NULL) != OSAL_OK) osal_panic("almanac test");
}
