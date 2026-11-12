/* Adapter only: the vendor owns the calendar rules and static tables. */
#include <stdio.h>
#include <string.h>

#include "ink/osal.h"
#include "ink/sys/almanac.h"
#include "ink/sys/i18n.h"
#include "ink/sys/json.h"
#include "ink/sys/time.h"
#include "vendor/mogo-calendar/main/time/calendar.h"

bool ink_almanac_date(int year, int month, int day, ink_almanac_t *out) {
    if (!out) return false;
    memset(out, 0, sizeof *out);
    calendar_date_t d;
    if (!calendar_date(year, month, day, &d)) return false;
    *out = (ink_almanac_t){
        .year = d.year, .month = d.month, .day = d.day, .weekday = d.weekday,
        .days = d.days, .first_weekday = d.first_weekday,
        .lunar_year = d.lunar_year, .lunar_month = d.lunar_month, .lunar_day = d.lunar_day,
        .year_cycle = d.year_cycle, .month_cycle = d.month_cycle, .day_cycle = d.day_cycle,
        .yi = d.yi, .ji = d.ji, .wealth = d.wealth, .clash = d.clash, .sha = d.sha,
        .joy = d.joy, .fortune = d.fortune, .auspicious = d.auspicious,
        .inauspicious = d.inauspicious, .officer = d.officer, .guardian = d.guardian,
        .guardian_type = d.guardian_type, .year_ganzhi = calendar_cycle_name(d.year_cycle),
        .month_ganzhi = calendar_cycle_name(d.month_cycle), .day_ganzhi = calendar_cycle_name(d.day_cycle),
        .solar_term = ""};
    calendar_lunar_text(&d, false, out->lunar_text, sizeof out->lunar_text);
    for (int i = 0; i < 2; i++) {
        int term_day = 0;
        const char *term = calendar_term(year, month, i, &term_day);
        if (term_day == day) out->solar_term = term;
    }
    return true;
}

static void almanac_fail(ink_call_t *c, ink_rc_t status, const char *text) {
    c->rc.status = status;
    snprintf(c->rc.detail, sizeof c->rc.detail, "%s", text);
    snprintf(c->rc.say, sizeof c->rc.say, "%s", text);
}

void ink_almanac_cap_run(ink_call_t *c) {
    char date[16] = "";
    int y = 0, m = 0, day = 0;
    bool today = !ink_args_find(&c->args, "date");
    if (today) {
        if (!ink_time_valid()) {
            almanac_fail(c, INK_RC_FAILED, TR("请先校准时间"));
            return;
        }
        ink_tm_t t;
        ink_time_to_local(ink_time_now_s(), &t);
        y = t.year; m = t.month; day = t.day;
    } else {
        ink_args_get_str(&c->args, "date", date, sizeof date);
        bool valid = strlen(date) == 10 && date[4] == '-' && date[7] == '-';
        for (int i = 0; valid && i < 10; i++)
            if (i != 4 && i != 7 && (date[i] < '0' || date[i] > '9')) valid = false;
        if (!valid || sscanf(date, "%4d-%2d-%2d", &y, &m, &day) != 3) {
            almanac_fail(c, INK_RC_INVALID, TR("日期请用 YYYY-MM-DD"));
            return;
        }
    }
    if (y < 2000 || y > 2099) {
        almanac_fail(c, INK_RC_INVALID, TR("不在范围（2000—2099 年）"));
        return;
    }
    ink_almanac_t d;
    if (!ink_almanac_date(y, m, day, &d)) {
        almanac_fail(c, INK_RC_INVALID, TR("日期无效"));
        return;
    }
    char *out = osal_malloc(4096, OSAL_MEM_EXTERNAL);
    if (!out) {
        almanac_fail(c, INK_RC_FAILED, TR("内存不足"));
        return;
    }
    ink_jw_t w;
    ink_jw_init(&w, out, 4096);
    ink_jw_raw(&w, "{");
#define NUMBER(field) ink_jw_raw(&w, "\"" #field "\":"); ink_jw_int(&w, d.field); ink_jw_raw(&w, ",")
#define TEXT(field) ink_jw_raw(&w, "\"" #field "\":"); ink_jw_str(&w, d.field); ink_jw_raw(&w, ",")
    NUMBER(year); NUMBER(month); NUMBER(day); NUMBER(weekday); NUMBER(days); NUMBER(first_weekday);
    NUMBER(lunar_year); NUMBER(lunar_month); NUMBER(lunar_day);
    NUMBER(year_cycle); NUMBER(month_cycle); NUMBER(day_cycle);
    TEXT(yi); TEXT(ji); TEXT(wealth); TEXT(clash); TEXT(sha); TEXT(joy); TEXT(fortune);
    TEXT(auspicious); TEXT(inauspicious); TEXT(officer); TEXT(guardian); TEXT(guardian_type);
    TEXT(year_ganzhi); TEXT(month_ganzhi); TEXT(day_ganzhi); TEXT(solar_term); TEXT(lunar_text);
#undef TEXT
#undef NUMBER
    ink_jw_raw(&w, "\"folk_notice\":"); ink_jw_str(&w, TR("这些是民俗说法")); ink_jw_raw(&w, "}");
    if (w.overflow) almanac_fail(c, INK_RC_FAILED, TR("历法结果过长"));
    else {
        ink_cap_result(c, out);
        int yi = (int)strcspn(d.yi, " "), ji = (int)strcspn(d.ji, " ");
        snprintf(c->rc.say, sizeof c->rc.say, TR("%s%.36s，宜%.*s，忌%.*s。这些是民俗说法。"),
                 today ? TR("今天") : TR("这天"), d.lunar_text, yi < 24 ? yi : 24, d.yi, ji < 24 ? ji : 24, d.ji);
    }
    osal_free(out);
}
