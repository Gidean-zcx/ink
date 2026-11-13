/* Adapter only: the vendor owns the calendar rules and static tables. */
#include <stdio.h>
#include <string.h>

#include "ink/osal.h"
#include "ink/sys/almanac.h"
#include "ink/sys/i18n.h"
#include "ink/sys/json.h"
#include "ink/sys/time.h"
#include "vendor/mogo-calendar/main/time/calendar.h"

_Static_assert(sizeof ((ink_almanac_t *)0)->yi == sizeof ((calendar_date_t *)0)->yi &&
               sizeof ((ink_almanac_t *)0)->ji == sizeof ((calendar_date_t *)0)->ji &&
               sizeof ((ink_almanac_t *)0)->auspicious == sizeof ((calendar_date_t *)0)->auspicious &&
               sizeof ((ink_almanac_t *)0)->inauspicious == sizeof ((calendar_date_t *)0)->inauspicious,
               "almanac lists must match Mogo's calendar_date_t");

/* d holds Mogo's ~650 B result: on the caller's stack for ink_almanac_date, in PSRAM for time.almanac. */
static bool almanac_fill(int year, int month, int day, ink_almanac_t *out, calendar_date_t *d) {
    if (!out) return false;
    memset(out, 0, sizeof *out);
    if (!calendar_date(year, month, day, d)) return false;
#define COPY(field) out->field = d->field
#define LIST(field) memcpy(out->field, d->field, sizeof out->field)
    COPY(year); COPY(month); COPY(day); COPY(weekday); COPY(days); COPY(first_weekday);
    COPY(lunar_year); COPY(lunar_month); COPY(lunar_day); COPY(year_cycle); COPY(month_cycle); COPY(day_cycle);
    LIST(yi); LIST(ji); COPY(wealth); COPY(clash); COPY(sha); COPY(joy); COPY(fortune);
    LIST(auspicious); LIST(inauspicious); COPY(officer); COPY(guardian); COPY(guardian_type);
#undef LIST
#undef COPY
    out->year_ganzhi = calendar_cycle_name(d->year_cycle);
    out->month_ganzhi = calendar_cycle_name(d->month_cycle);
    out->day_ganzhi = calendar_cycle_name(d->day_cycle);
    out->solar_term = "";
    calendar_lunar_text(d, false, out->lunar_text, sizeof out->lunar_text);
    for (int i = 0; i < 2; i++) {
        int term_day = 0;
        const char *term = calendar_term(year, month, i, &term_day);
        if (term_day == day) out->solar_term = term;
    }
    return true;
}

bool ink_almanac_date(int year, int month, int day, ink_almanac_t *out) {
    calendar_date_t d;
    return almanac_fill(year, month, day, out, &d);
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
    /* One temporary PSRAM block holds Mogo's result, ours and the JSON; nothing large on the stack. */
    struct almanac_work { calendar_date_t mogo; ink_almanac_t d; char json[4096]; } *work =
        osal_malloc(sizeof *work, OSAL_MEM_EXTERNAL);
    if (!work) {
        almanac_fail(c, INK_RC_FAILED, TR("内存不足"));
        return;
    }
    const ink_almanac_t *d = &work->d;
    char *out = work->json;
    if (!almanac_fill(y, m, day, &work->d, &work->mogo)) {
        almanac_fail(c, INK_RC_INVALID, TR("日期无效"));
        osal_free(work);
        return;
    }
    ink_jw_t w;
    ink_jw_init(&w, out, sizeof work->json);
    ink_jw_raw(&w, "{");
#define NUMBER(field) ink_jw_raw(&w, "\"" #field "\":"); ink_jw_int(&w, d->field); ink_jw_raw(&w, ",")
#define TEXT(field) ink_jw_raw(&w, "\"" #field "\":"); ink_jw_str(&w, d->field); ink_jw_raw(&w, ",")
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
        int yi = (int)strcspn(d->yi, " "), ji = (int)strcspn(d->ji, " ");
        snprintf(c->rc.say, sizeof c->rc.say, TR("%s%.36s，宜%.*s，忌%.*s。这些是民俗说法。"),
                 today ? TR("今天") : TR("这天"), d->lunar_text, yi < 24 ? yi : 24, d->yi, ji < 24 ? ji : 24, d->ji);
    }
    osal_free(work);
}
