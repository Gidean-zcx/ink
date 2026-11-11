#include "common.h"
#ifndef INK_TEST_SOURCE
#define INK_TEST_SOURCE "../../system/ui/shell/time_settings.c"
#endif
#define key test_source_key
#include INK_TEST_SOURCE
#undef key

static void test_disabled(void) {
    CHECK(test_label(P.screen, "先关闭自动设置日期和时间") && test_label(P.screen, "先关闭自动设置时区"), "UX10 disabled time controls explain why");
    CHECK(!P.action[1].enabled && !P.action[3].enabled, "UX10 automatic settings keep manual actions disabled");
}
static void test_choices(void) {
    CHECK(!strcmp(P.model.title, "重新验证"), "UX10 return page has a short title");
    CHECK(!test_label(P.screen, "已选择"), "UX10 selection is a check in the option row");
    CHECK(test_fits(test_label(P.screen, "15 分钟")), "UX10 last return choice fits");
}
static int test_part;
static const char *const test_numbers[] = {"2032", "12", "25", "23", "59"};
_Static_assert(sizeof test_numbers / sizeof *test_numbers == TIME_FIELDS_MAX, "numeric fixtures cover all date parts");
static void test_numeric(void) {
    CHECK(ink_text_type() == INK_TEXT_NUMBER && ink_text_focused(), "UX10 direct entry uses the existing numeric input");
    ink_text_select(0, 4); ink_text_commit_utf8(test_numbers[test_part]); ink_text_enter();
}
static void test_run(void *arg) {
    (void)arg; test_init();
    CHECK(AI("settings.set", "{\"id\":\"time.auto\",\"value\":1}"), "enable automatic time for disabled-state fixture");
    CHECK(AI("settings.set", "{\"id\":\"time.zone_auto\",\"value\":1}"), "enable automatic zone for disabled-state fixture"); test_quiet();
    CHECK(AI("shell.time", NULL), "open time settings"); test_quiet(); on_ui(test_disabled); test_frame("time-auto");
    CHECK(AI("settings.set", "{\"id\":\"time.auto\",\"value\":0}"), "disable automatic time"); test_quiet();
    CHECK(AI("shell.time_edit", NULL), "open date editor"); test_quiet(); test_frame("time-edit");
    CHECK(P.actions == 3 * TIME_FIELDS_MAX + 1, "UX10 date fields have direct input actions");
    if (P.actions == 3 * TIME_FIELDS_MAX + 1) {
        for (test_part = 0; test_part < TIME_FIELDS_MAX; test_part++) {
            char args[64]; snprintf(args, sizeof args, "{\"field\":%d,\"delta\":0}", test_part);
            CHECK(AI("shell.time_step", args) && g_rc.status == INK_RC_DONE, "open numeric field through the draft capability");
            test_quiet();
            if (!test_part) test_frame("time-input");
            on_ui(test_numeric); test_quiet();
            CHECK(P.part[test_part] == atoi(test_numbers[test_part]), "UX10 numeric submit updates each date/time draft field");
        }
        CHECK(AI("shell.time_step", "{\"field\":0,\"delta\":1}") && g_rc.status == INK_RC_DONE && P.part[0] == 2033, "UX10 plus adjustment remains available");
        CHECK(AI("shell.time_step", "{\"field\":1,\"delta\":0,\"value\":13}") && g_rc.status == INK_RC_REJECTED, "UX10 rejects out-of-range numeric draft");
    }
    CHECK(AI("shell.return", NULL), "open return choices"); test_quiet(); on_ui(test_choices); test_frame("return");
    CHECK(AI("shell.time_zones", NULL), "open zone choices"); test_quiet(); test_frame("zones");
    ink_probe_count("test.finished");
}
