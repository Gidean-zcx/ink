/*
 * The time service on the soft bus: what the clock and calendar apps' buttons do, and what the AI can
 * do. None of it blocks: the table is in memory and writes are queued.
 */
#include <stdio.h>
#include <string.h>

#include "ink/osal.h"
#include "ink/sys/cap.h"
#include "ink/sys/json.h"
#include "ink/sys/members.h"
#include "ink/sys/state.h"
#include "ink/sys/time.h"
#include "time_internal.h"

#define UNDO_MS 10000

static void fail(ink_call_t *c, ink_rc_t st, const char *why) {
    c->rc.status = st;
    snprintf(c->rc.detail, sizeof c->rc.detail, "%s", why);
}

static void local_text(int64_t utc, char *out, size_t cap) {
    ink_tm_t t;
    ink_time_to_local(utc, &t);
    snprintf(out, cap, "%04d-%02d-%02d %02d:%02d:%02d", t.year, t.month, t.day, t.hour, t.min, t.sec);
}

static void cap_now(ink_call_t *c) {
    if (!ink_time_valid()) {
        fail(c, INK_RC_FAILED, "the clock is not known yet");
        return;
    }
    int64_t now = ink_time_now_s();
    ink_tm_t t;
    ink_time_to_local(now, &t);
    char text[24], lunar[32] = "", gz[12] = "", out[256];
    local_text(now, text, sizeof text);
    ink_lunar_t l;
    bool has_lunar = ink_lunar_from_solar(t.year, t.month, t.day, &l);
    if (has_lunar) {
        ink_lunar_name(&l, lunar, sizeof lunar);
        ink_lunar_ganzhi(l.year, gz, sizeof gz);
    }
    int64_t synced = 0;
    ink_state_get_int(ink_state_find("time.synced"), &synced);
    snprintf(out, sizeof out, "{\"utc_s\":%lld,\"local\":\"%s\",\"weekday\":%d,\"zone\":\"%s\",\"offset_min\":%d,\"lunar\":\"%s\",\"ganzhi\":\"%s\",\"synced\":%s}",
             (long long)now, text, t.wday, ink_time_zone(), t.offset_min, lunar, gz, synced ? "true" : "false");
    ink_cap_result(c, out);
    snprintf(c->rc.say, sizeof c->rc.say, "现在是 %.16s", text);
}

static void cap_set(ink_call_t *c) {
    int64_t s = 0;
    ink_args_get_int(&c->args, "utc_s", &s);
    ink_time_set(s, false);
    snprintf(c->rc.say, sizeof c->rc.say, "时间改好了");
}

static void cap_lunar(ink_call_t *c) {
    int64_t y = 0, m = 0, d = 0, ly = 0, lm = 0, ld = 0;
    bool leap = false;
    ink_args_get_bool(&c->args, "leap", &leap);
    char out[160], name[32] = "", gz[12] = "";
    if (ink_args_get_int(&c->args, "lunar_year", &ly) && ink_args_get_int(&c->args, "lunar_month", &lm) && ink_args_get_int(&c->args, "lunar_day", &ld)) {
        ink_lunar_t l = {(int)ly, (int)lm, (int)ld, leap};
        int sy, sm, sd;
        if (!ink_lunar_to_solar(&l, &sy, &sm, &sd)) {
            fail(c, INK_RC_INVALID, "no such lunar date");
            return;
        }
        snprintf(out, sizeof out, "{\"year\":%d,\"month\":%d,\"day\":%d}", sy, sm, sd);
    } else {
        ink_tm_t t = {0};
        if (!ink_args_get_int(&c->args, "year", &y) || !ink_args_get_int(&c->args, "month", &m) || !ink_args_get_int(&c->args, "day", &d)) {
            if (!ink_time_valid()) {
                fail(c, INK_RC_FAILED, "the clock is not known yet");
                return;
            }
            ink_time_to_local(ink_time_now_s(), &t);
            y = t.year;
            m = t.month;
            d = t.day;
        }
        ink_lunar_t l;
        if (!ink_lunar_from_solar((int)y, (int)m, (int)d, &l)) {
            fail(c, INK_RC_INVALID, "outside the calendar (1901-2100)");
            return;
        }
        ink_lunar_name(&l, name, sizeof name);
        ink_lunar_ganzhi(l.year, gz, sizeof gz);
        snprintf(out, sizeof out, "{\"lunar_year\":%d,\"lunar_month\":%d,\"lunar_day\":%d,\"leap\":%s,\"name\":\"%s\",\"ganzhi\":\"%s\",\"zodiac\":\"%s\"}", l.year, l.month, l.day,
                 l.leap ? "true" : "false", name, gz, ink_lunar_zodiac(l.year));
    }
    ink_cap_result(c, out);
}

static bool parse_at(const char *s, ink_alarm_t *a) { /* "2026-11-05 15:00" */
    int y, mo, d, h, mi;
    if (sscanf(s, "%d-%d-%d %d:%d", &y, &mo, &d, &h, &mi) != 5 || h < 0 || h > 23 || mi < 0 || mi > 59) return false;
    a->year = (int16_t)y;
    a->month = (uint8_t)mo;
    a->day = (uint8_t)d;
    a->hour = (uint8_t)h;
    a->min = (uint8_t)mi;
    return true;
}

static void answer_add(ink_call_t *c, ink_alarm_t *a, const char *undo_cap) {
    int id = ink_alarm_add(a);
    if (id <= 0) {
        fail(c, id == -3 ? INK_RC_REJECTED : INK_RC_INVALID, id == -2 ? "that time has passed" : id == -3 ? "too many alarms" : "not a valid time");
        return;
    }
    snprintf(c->rc.object, sizeof c->rc.object, "%s:%d", a->kind == INK_ALARM ? "alarm" : "reminder", id);
    char next[24];
    if (a->next_s) {
        local_text(a->next_s, next, sizeof next);
        snprintf(c->rc.say, sizeof c->rc.say, "好的，%.16s", next);
    }
    ink_args_t undo;
    ink_args_init(&undo);
    ink_args_int(&undo, "id", id);
    ink_cap_undoable(c, undo_cap, &undo, UNDO_MS);
}

static void cap_alarm_set(ink_call_t *c) {
    ink_alarm_t a = {.kind = INK_ALARM, .src = (uint8_t)c->src};
    int64_t h = 0, m = 0, wd = 0;
    ink_args_get_int(&c->args, "hour", &h);
    ink_args_get_int(&c->args, "minute", &m);
    a.hour = (uint8_t)h;
    a.min = (uint8_t)m;
    if (ink_args_get_int(&c->args, "weekdays", &wd) && wd) {
        a.repeat = INK_REPEAT_WEEKDAYS;
        a.weekdays = (uint8_t)wd;
    }
    ink_args_get_str(&c->args, "title", a.title, sizeof a.title);
    answer_add(c, &a, "alarm.cancel");
}

static void cap_reminder_add(ink_call_t *c) {
    ink_alarm_t a = {.kind = INK_REMINDER, .src = (uint8_t)c->src};
    char at[32] = "", repeat[16] = "";
    ink_args_get_str(&c->args, "at", at, sizeof at);
    ink_args_get_str(&c->args, "title", a.title, sizeof a.title);
    ink_args_get_str(&c->args, "repeat", repeat, sizeof repeat);
    if (!parse_at(at, &a)) {
        fail(c, INK_RC_INVALID, "at: YYYY-MM-DD HH:MM");
        return;
    }
    if (!strcmp(repeat, "monthly")) a.repeat = INK_REPEAT_MONTHLY;
    else if (!strcmp(repeat, "yearly")) a.repeat = INK_REPEAT_YEARLY;
    else if (!strcmp(repeat, "lunar_yearly")) {
        ink_lunar_t l;
        if (!ink_lunar_from_solar(a.year, a.month, a.day, &l)) {
            fail(c, INK_RC_INVALID, "outside the calendar (1901-2100)");
            return;
        }
        a.repeat = INK_REPEAT_LUNAR_YEARLY;
        a.lmonth = (uint8_t)l.month;
        a.lday = (uint8_t)l.day;
        a.lleap = l.leap;
    } else if (!strcmp(repeat, "weekly")) {
        a.repeat = INK_REPEAT_WEEKDAYS;
        int64_t days = time_days_from_civil(a.year, a.month, a.day);
        a.weekdays = (uint8_t)(1u << (((days % 7) + 11) % 7));
    } else if (!strcmp(repeat, "daily")) {
        a.repeat = INK_REPEAT_WEEKDAYS;
        a.weekdays = 0x7F;
    }
    answer_add(c, &a, "reminder.cancel");
}

static void list_kind(ink_call_t *c, ink_alarm_kind_t kind) {
    char *out = osal_malloc(4096, OSAL_MEM_EXTERNAL);
    ink_alarm_t *a = osal_malloc(sizeof *a, OSAL_MEM_EXTERNAL);
    if (!out || !a) {
        osal_free(out);
        osal_free(a);
        fail(c, INK_RC_FAILED, "out of memory");
        return;
    }
    ink_jw_t w;
    ink_jw_init(&w, out, 4096);
    ink_jw_raw(&w, "[");
    int cursor = 0;
    bool first = true;
    while (ink_alarm_next(&cursor, kind, a) == 1) {
        char next[24] = "";
        if (a->next_s) local_text(a->next_s, next, sizeof next);
        size_t before = w.len;
        ink_jw_raw(&w, first ? "{\"id\":" : ",{\"id\":");
        ink_jw_int(&w, a->id);
        ink_jw_raw(&w, ",\"title\":");
        ink_jw_str(&w, a->title);
        ink_jw_raw(&w, ",\"enabled\":");
        ink_jw_raw(&w, a->enabled ? "true" : "false");
        ink_jw_raw(&w, ",\"next\":");
        ink_jw_str(&w, next);
        ink_jw_raw(&w, ",\"repeat\":");
        ink_jw_int(&w, a->repeat);
        ink_jw_raw(&w, "}");
        if (w.overflow) {
            w.overflow = false;
            w.len = before;
            break;
        }
        first = false;
    }
    ink_jw_raw(&w, "]");
    ink_cap_result(c, out);
    osal_free(out);
    osal_free(a);
}

static void cap_alarm_list(ink_call_t *c) { list_kind(c, INK_ALARM); }
static void cap_reminder_list(ink_call_t *c) { list_kind(c, INK_REMINDER); }

static void cap_cancel(ink_call_t *c) {
    int64_t id = 0;
    ink_args_get_int(&c->args, "id", &id);
    if (ink_alarm_cancel((uint32_t)id) != 0) fail(c, INK_RC_FAILED, "no such one, or not yours");
}

static void cap_dismiss(ink_call_t *c) {
    int64_t id = 0;
    ink_args_get_int(&c->args, "id", &id);
    if (ink_alarm_dismiss((uint32_t)id) != 0) fail(c, INK_RC_FAILED, "nothing is ringing");
}

static void cap_snooze(ink_call_t *c) {
    int64_t id = 0, minutes = 0;
    ink_args_get_int(&c->args, "id", &id);
    ink_args_get_int(&c->args, "minutes", &minutes);
    if (ink_alarm_snooze((uint32_t)id, (int)minutes) != 0) fail(c, INK_RC_FAILED, "nothing is ringing");
}

#define ID_ARG "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"integer\",\"minimum\":1}},\"required\":[\"id\"]}"

static const ink_cap_t TIME_CAPS[] = {
    {.id = "time.now", .title = "现在几点（含时区和农历）", .output = "{\"type\":\"object\"}", .effect = INK_FX_READ, .child_ok = true, .exec = INK_EXEC_DIRECT, .run = cap_now},
    {.id = "time.set",
     .title = "改系统时间",
     .ask = "把系统时间改成 {utc_s}（UTC 秒）",
     .input = "{\"type\":\"object\",\"properties\":{\"utc_s\":{\"type\":\"integer\",\"minimum\":1000000000}},\"required\":[\"utc_s\"]}",
     .effect = INK_FX_SYSTEM, /* children cannot: it would get around time limits (foundation §7.1) */
     .exec = INK_EXEC_BG,
     .run = cap_set},
    {.id = "time.lunar",
     .title = "公历和农历互查（不给日期就是今天）",
     .input = "{\"type\":\"object\",\"properties\":{\"year\":{\"type\":\"integer\"},\"month\":{\"type\":\"integer\"},\"day\":{\"type\":\"integer\"},"
              "\"lunar_year\":{\"type\":\"integer\"},\"lunar_month\":{\"type\":\"integer\"},\"lunar_day\":{\"type\":\"integer\"},\"leap\":{\"type\":\"boolean\"}}}",
     .output = "{\"type\":\"object\"}",
     .effect = INK_FX_READ,
     .child_ok = true,
     .exec = INK_EXEC_DIRECT,
     .run = cap_lunar},
    {.id = "alarm.set",
     .title = "设闹钟（weekdays 每周几：位 0 是周日；不给就只响一次）",
     .input = "{\"type\":\"object\",\"properties\":{\"hour\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":23},\"minute\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":59},"
              "\"weekdays\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":127},\"title\":{\"type\":\"string\",\"maxLength\":15}},\"required\":[\"hour\",\"minute\"]}",
     .effect = INK_FX_LOCAL_WRITE,
     .child_ok = true,
     .exec = INK_EXEC_DIRECT,
     .run = cap_alarm_set},
    {.id = "alarm.list", .title = "我的闹钟", .output = "{\"type\":\"array\"}", .effect = INK_FX_READ, .child_ok = true, .exec = INK_EXEC_DIRECT, .run = cap_alarm_list},
    {.id = "alarm.cancel", .title = "删闹钟", .input = ID_ARG, .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .exec = INK_EXEC_DIRECT, .run = cap_cancel},
    {.id = "alarm.dismiss",
     .title = "关掉正在响的闹钟",
     .input = "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"integer\",\"minimum\":0}}}",
     .effect = INK_FX_LOCAL_WRITE,
     .child_ok = true,
     .while_locked = true,
     .exec = INK_EXEC_DIRECT,
     .run = cap_dismiss},
    {.id = "alarm.snooze",
     .title = "闹钟稍后再响",
     .input = "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"integer\",\"minimum\":0},\"minutes\":{\"type\":\"integer\",\"minimum\":1,\"maximum\":120}}}",
     .effect = INK_FX_LOCAL_WRITE,
     .child_ok = true,
     .while_locked = true,
     .exec = INK_EXEC_DIRECT,
     .run = cap_snooze},
    {.id = "reminder.add",
     .title = "加提醒（at 本地时间 YYYY-MM-DD HH:MM；repeat: daily weekly monthly yearly lunar_yearly）",
     .input = "{\"type\":\"object\",\"properties\":{\"at\":{\"type\":\"string\",\"maxLength\":20},\"title\":{\"type\":\"string\",\"maxLength\":15},"
              "\"repeat\":{\"type\":\"string\",\"enum\":[\"daily\",\"weekly\",\"monthly\",\"yearly\",\"lunar_yearly\"]}},\"required\":[\"at\",\"title\"]}",
     .effect = INK_FX_LOCAL_WRITE,
     .child_ok = true,
     .exec = INK_EXEC_DIRECT,
     .run = cap_reminder_add},
    {.id = "reminder.list", .title = "我的提醒", .output = "{\"type\":\"array\"}", .effect = INK_FX_READ, .child_ok = true, .exec = INK_EXEC_DIRECT, .run = cap_reminder_list},
    {.id = "reminder.cancel", .title = "删提醒", .input = ID_ARG, .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .exec = INK_EXEC_DIRECT, .run = cap_cancel},
    {.id = "alarm.fired", .title = "闹钟响了", .form = INK_FORM_EVENT, .effect = INK_FX_READ, .child_ok = true},
    {.id = "alarm.ended", .title = "闹钟停了", .form = INK_FORM_EVENT, .effect = INK_FX_READ, .child_ok = true},
    {.id = "alarm.missed", .title = "错过了闹钟", .form = INK_FORM_EVENT, .effect = INK_FX_READ, .child_ok = true},
    {.id = "reminder.fired", .title = "提醒到时间了", .form = INK_FORM_EVENT, .effect = INK_FX_READ, .child_ok = true},
    {.id = "reminder.missed", .title = "错过了提醒", .form = INK_FORM_EVENT, .effect = INK_FX_READ, .child_ok = true},
    {.id = "time.changed", .title = "系统时间改了", .form = INK_FORM_EVENT, .effect = INK_FX_READ, .child_ok = true},
};

int ink_time_caps_init(void) { return ink_cap_register(TIME_CAPS, (int)(sizeof TIME_CAPS / sizeof TIME_CAPS[0])); }
