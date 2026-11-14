/*
 * The calendar (calendar-3): rewritten after Fossify Calendar and Etar for this device (docs/tasks/calendar-3/对照表.md,
 * SOURCE.md). Month on one screen, agenda and search, a day with the almanac, read-only details, the editor with
 * repeats and up to three reminders, the three scopes, iCalendar files. Events live in the time service (time.h).
 */
/* ui-scale-runtime: dimension x,y,w Theme-derived control geometry. */
#include <stdlib.h>
#include "cal.h"
#include "ink/sys/appreg.h"
#include "ink/sys/state.h"
#include "ink/sys/file_picker.h"

cal_t K INK_PSRAM_BSS;

static const char *const WEEK_START[] = {"monday", "sunday", NULL};
static const char *const DEFAULT_LEAD[] = {"none", "0", "5", "10", "30", "60", "1440", NULL};
static const char *const VIEWS[] = {"month", "agenda", NULL};
static const ink_setting_def_t SETTINGS[] = {
    {.id = "calendar.week_start", .title = TR_NOOP("一周从哪天开始"), .type = INK_SET_ENUM, .def = 0, .choices = WEEK_START, .child_may_change = true, .per_member = true},
    {.id = "calendar.default_reminder", .title = TR_NOOP("新建时的提醒"), .type = INK_SET_ENUM, .def = 3, .choices = DEFAULT_LEAD, .child_may_change = true, .per_member = true},
    {.id = "calendar.last_view", .title = TR_NOOP("日历上次的视图"), .type = INK_SET_ENUM, .def = 0, .choices = VIEWS, .child_may_change = true, .per_member = true},
};

/* ------------------------------------------------------------ dates and words */

int32_t cal_day(int y, int m, int d) {
    y -= m <= 2;
    int era = (y >= 0 ? y : y - 399) / 400, yoe = y - era * 400, doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    return era * 146097 + yoe * 365 + yoe / 4 - yoe / 100 + doy - 719468;
}
void cal_civil(int32_t z, int *y, int *m, int *d) {
    z += 719468;
    int era = (z >= 0 ? z : z - 146096) / 146097, doe = z - era * 146097, yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    int doy = doe - (365 * yoe + yoe / 4 - yoe / 100), mp = (5 * doy + 2) / 153;
    *d = doy - (153 * mp + 2) / 5 + 1;
    *m = mp < 10 ? mp + 3 : mp - 9;
    *y = yoe + era * 400 + (*m <= 2);
}
int cal_wday(int32_t day) { return ((day % 7) + 11) % 7; }
int cal_month_days(int y, int m) { return cal_day(m == 12 ? y + 1 : y, m == 12 ? 1 : m + 1, 1) - cal_day(y, m, 1); }
const char *cal_weekday_name(int wday) {
    static const char *const W[] = {TR_NOOP("周日"), TR_NOOP("周一"), TR_NOOP("周二"), TR_NOOP("周三"), TR_NOOP("周四"), TR_NOOP("周五"), TR_NOOP("周六")};
    return TR(W[wday % 7]);
}

/* Two characters under a day: the lunar day, a month's name on its first, a solar term on its day (lunar.c). */
void cal_lunar_short(int y, int m, int d, char *out, size_t cap) {
    static const char *const MONTHS[] = {TR_NOOP("正月"), TR_NOOP("二月"), TR_NOOP("三月"), TR_NOOP("四月"), TR_NOOP("五月"), TR_NOOP("六月"), TR_NOOP("七月"),
                                         TR_NOOP("八月"), TR_NOOP("九月"), TR_NOOP("十月"), TR_NOOP("冬月"), TR_NOOP("腊月")};
    static const char *const LEAP[] = {TR_NOOP("闰正"), TR_NOOP("闰二"), TR_NOOP("闰三"), TR_NOOP("闰四"), TR_NOOP("闰五"), TR_NOOP("闰六"), TR_NOOP("闰七"),
                                       TR_NOOP("闰八"), TR_NOOP("闰九"), TR_NOOP("闰十"), TR_NOOP("闰冬"), TR_NOOP("闰腊")};
    ink_lunar_t l;
    out[0] = 0;
    const char *term = ink_lunar_solar_term(y, m, d);
    if (term) { snprintf(out, cap, "%s", term); return; }
    if (!ink_lunar_from_solar(y, m, d, &l)) return;
    if (l.day == 1) { snprintf(out, cap, "%s", TR((l.leap ? LEAP : MONTHS)[(l.month - 1) % 12])); return; }
    if (strcmp(ink_lang_code(), "zh")) { snprintf(out, cap, "%d", l.day); return; } /* other languages: the lunar day's number */
    char name[32];
    ink_lunar_name(&l, name, sizeof name);
    const char *p = strstr(name, "月");
    snprintf(out, cap, "%s", p ? p + strlen("月") : name);
}
void cal_lunar_full(int y, int m, int d, char *out, size_t cap) {
    ink_lunar_t l;
    out[0] = 0;
    if (ink_lunar_from_solar(y, m, d, &l)) ink_lunar_name(&l, out, cap);
}

void cal_lead_text(int32_t lead, bool all_day, char *out, size_t cap) {
    if (all_day && lead <= 0) snprintf(out, cap, TR("当天 %02d:%02d"), -lead / 60, -lead % 60);
    else if (!lead) snprintf(out, cap, "%s", TR("准时"));
    else if (lead % 1440 == 0) snprintf(out, cap, TR("提前 %d 天"), lead / 1440);
    else if (lead % 60 == 0) snprintf(out, cap, TR("提前 %d 小时"), lead / 60);
    else snprintf(out, cap, TR("提前 %d 分钟"), lead);
}

/* "每 2 周的周一、周三，到 12月31日为止" (after Etar's EventRecurrenceFormatter). */
void cal_rule_text(const ink_event_t *e, char *out, size_t cap) {
    static const char *const UNIT[] = {"", TR_NOOP("天"), TR_NOOP("周"), TR_NOOP("个月"), TR_NOOP("年"), TR_NOOP("年")};
    static const char *const ONE[] = {TR_NOOP("不重复"), TR_NOOP("每天"), TR_NOOP("每周"), TR_NOOP("每月"), TR_NOOP("每年"), TR_NOOP("农历每年")};
    size_t n = 0;
    if (e->freq == INK_FREQ_WEEKLY && e->weekdays == 0x3E && e->interval <= 1) n = (size_t)snprintf(out, cap, "%s", TR("工作日"));
    else if (e->interval > 1) n = (size_t)snprintf(out, cap, TR("每 %d %s"), e->interval, TR(UNIT[e->freq % 6]));
    else n = (size_t)snprintf(out, cap, "%s", TR(ONE[e->freq % 6]));
    if (n >= cap) return;
    if (e->freq == INK_FREQ_WEEKLY && e->weekdays != 0x3E) {
        for (int i = 1, first = 1; i <= 7 && n < cap; i++)
            if (e->weekdays & (1u << (i % 7))) { n += (size_t)snprintf(out + n, cap - n, "%s%s", first ? TR("的") : "、", cal_weekday_name(i % 7)); first = 0; }
    } else if (e->freq == INK_FREQ_MONTHLY || e->freq == INK_FREQ_YEARLY) {
        char what[48];
        if (e->by == INK_BY_NTH_WEEKDAY) snprintf(what, sizeof what, TR("第 %d 个%s"), e->nth, cal_weekday_name(e->wday));
        else if (e->by == INK_BY_LAST_WEEKDAY) snprintf(what, sizeof what, TR("最后一个%s"), cal_weekday_name(e->wday));
        else if (e->by == INK_BY_LAST_DAY) snprintf(what, sizeof what, "%s", TR("最后一天"));
        else snprintf(what, sizeof what, TR("%d 日"), e->rday);
        if (e->freq == INK_FREQ_YEARLY) n += (size_t)snprintf(out + n, cap - n, TR("的 %d 月%s"), e->rmonth, what);
        else n += (size_t)snprintf(out + n, cap - n, TR("的%s"), what);
    } else if (e->freq == INK_FREQ_LUNAR_YEARLY) {
        char name[32];
        ink_lunar_t l = {2000, e->lmonth, e->lday, e->lleap};
        ink_lunar_name(&l, name, sizeof name);
        n += (size_t)snprintf(out + n, cap - n, " %s", name);
    }
    if (n >= cap) return;
    if (e->until_year) snprintf(out + n, cap - n, TR("，到 %d月%d日为止"), e->until_month, e->until_day);
    else if (e->count) snprintf(out + n, cap - n, TR("，共 %d 次"), e->count);
}

/* The almanac through one small door the tests can replace (calendar-3 §4): false with why. */
__attribute__((weak)) bool cal_almanac(int y, int m, int d, ink_almanac_t *out, const char **why) {
    if (y < 2000 || y > 2099) { *why = TR("老黄历只有 2000–2099 年"); return false; }
    if (!ink_almanac_date(y, m, d, out)) { *why = TR("这天的老黄历查不到"); return false; }
    return true;
}

/* ------------------------------------------------------------ page parts */

/* A cut at the end of a buffer never leaves half a character. */
void cal_utf8_trim(char *s) {
    size_t n = strlen(s), k = n;
    while (k && ((unsigned char)s[k - 1] & 0xC0) == 0x80) k--;
    if (!k || ((unsigned char)s[k - 1] & 0xC0) != 0xC0) return;
    unsigned char b = (unsigned char)s[k - 1];
    if (n - (k - 1) < (size_t)(b >= 0xF0 ? 4 : b >= 0xE0 ? 3 : 2)) s[k - 1] = 0;
}

bool cal_mine(void) { return !strcmp(K.member, ink_member_current()->id); }

void cal_begin(lv_obj_t *screen, const char *title) {
    if (!cal_mine()) {
        memset(&K.position, 0, sizeof K.position); K.query[0] = 0; K.past = K.searched = K.undo_valid = false; K.y = 0;
        snprintf(K.member, sizeof K.member, "%s", ink_member_current()->id);
    }
    if (!K.y && ink_time_valid()) { ink_tm_t t; ink_time_to_local(ink_time_now_s(), &t); K.y = t.year; K.m = t.month; K.d = t.day; }
    memset(K.fields, 0, sizeof K.fields); memset(K.actions, 0, sizeof K.actions);
    snprintf(K.title, sizeof K.title, "%s", title); /* the page's own copy: callers pass a local */
    K.page = (ink_ui_page_spec_t){.id = ink_ui_page_name(), .title = K.title, .object = {"calendar", 1}, .fields = K.fields, .actions = K.actions};
    K.list = NULL; K.field = NULL;
    ink_shell_statusbar(screen, true);
    ink_shell_topbar(screen, title);
    cal_action(0, "back", TR("返回"), "shell.back", "{}");
}
/* Fields and actions go in the order they are declared (`hint` only reads as the page's order); an id seen again replaces. */
ink_ui_field_spec_t *cal_field(int hint, const char *id, const char *role, const char *label, const char *value) {
    (void)hint;
    size_t i = 0;
    while (i < K.page.field_count && strcmp(K.fields[i].id, id)) i++;
    if (i >= CAL_FIELDS) i = CAL_FIELDS - 1;
    K.fields[i] = (ink_ui_field_spec_t){.id = id, .role = role, .label = label, .value = value};
    if (i + 1 > K.page.field_count) K.page.field_count = i + 1;
    return &K.fields[i];
}
ink_ui_action_spec_t *cal_action(int hint, const char *id, const char *label, const char *cap, const char *args) {
    (void)hint;
    size_t i = 0;
    while (i < K.page.action_count && strcmp(K.actions[i].id, id)) i++;
    if (i >= CAL_ACTIONS) i = CAL_ACTIONS - 1;
    char copy[sizeof K.args[0]];
    snprintf(copy, sizeof copy, "%s", args ? args : "{}");
    memcpy(K.args[i], copy, sizeof copy);
    K.actions[i] = (ink_ui_action_spec_t){.id = id, .label = label, .capability = cap, .args_json = K.args[i], .enabled = true};
    if (i + 1 > K.page.action_count) K.page.action_count = i + 1;
    return &K.actions[i];
}
/* A pill as wide as its words at this text size, never under 108: nothing cut at the largest size. */
int cal_pill_w(const char *label) {
    int w = ink_ui_text_width(label, INK_FONT_LIST, INK_WEIGHT_MEDIUM) + 2 * (INK_GAP_XS + INK_STROKE);
    return w > INK_CONTROL ? w : INK_CONTROL;
}
lv_obj_t *cal_pill(lv_obj_t *parent, const char *label, int x, int y, int w, bool primary, const char *cap, const char *args, int action) {
    lv_obj_t *pill = ink_ui_pill(parent, label, x, y, w, INK_CONTROL, primary, NULL, NULL);
    ink_shell_bind(pill, cap, args ? args : "{}");
    if (action >= 0) cal_action(action, cap, label, cap, args);
    return pill;
}
/* After a delete: a button that stays until the next thing done here (gallery.c:566), never a vanishing bar. */
lv_obj_t *cal_undo_button(lv_obj_t *parent, int right, int y, int action) { /* right: where its right edge goes */
    int w = cal_pill_w(TR("撤销"));
    return K.undo_valid ? cal_pill(parent, TR("撤销"), right - w, y, w, false, "calendar.undo", "{}", action) : NULL;
}

int cal_snapshot(char *out, size_t cap) {
    char *list = K.list ? osal_malloc(cap, OSAL_MEM_EXTERNAL) : NULL, *controls = osal_malloc(cap, OSAL_MEM_EXTERNAL);
    int n = -1;
    size_t actions = K.page.action_count;
    if (K.field && ink_text_focused()) K.page.action_count = 0;
    int c = !controls ? -1 : K.field ? ink_textfield_short_snapshot(K.field, controls, cap) : ink_ui_semantics_snapshot(&K.page, controls, cap); /* F2: the input's rectangle too */
    if (c >= 0 && (!K.list || (list && ink_ui_list_snapshot(K.list, list, cap) >= 0))) {
        ink_jw_t w; ink_jw_init(&w, out, cap); /* a page without a list is its controls alone, as the shared input's snapshot is */
        if (list) { ink_jw_raw(&w, "{\"controls\":"); ink_jw_raw(&w, controls); ink_jw_raw(&w, ",\"content\":"); ink_jw_raw(&w, list); ink_jw_raw(&w, "}"); }
        else ink_jw_raw(&w, controls);
        n = w.overflow ? -1 : (int)w.len;
    }
    K.page.action_count = actions;
    osal_free(list); osal_free(controls);
    if (n < 0 && cap) out[0] = 0;
    return n;
}

bool cal_key(ink_key_t key, bool lng) { return K.list && ink_ui_list_key(K.list, key, lng); }

/* ------------------------------------------------------------ what the buttons and the AI do */

static bool on_page(const char *name) { return !strcmp(ink_ui_page_name(), name); }
static bool read_date(ink_call_t *call, int *y, int *m, int *d) {
    char text[16] = "";
    return ink_args_get_str(&call->args, "date", text, sizeof text) > 0 && sscanf(text, "%d-%d-%d", y, m, d) == 3 && *m >= 1 && *m <= 12 && *d >= 1 && *y >= 1901 && *y <= 2100 &&
           *d <= cal_month_days(*y, *m);
}

static void picked(void *ctx, uint32_t id) { /* the system file picker's answer: the file goes in (BG), the result page shows it */
    (void)ctx;
    ink_fs_entry_t entry;
    if (!id || ink_fs_lookup(id, &entry)) return;
    char args[INK_FS_PATH_MAX + 16];
    ink_jw_t w; ink_jw_init(&w, args, sizeof args); ink_jw_raw(&w, "{\"path\":"); ink_jw_str(&w, entry.path); ink_jw_raw(&w, "}");
    ink_receipt_t rc;
    ink_cap_invoke_json("calendar.import_run", args, INK_SRC_TOUCH, 0, &rc);
}

static void transfer_done(void *ctx, const ink_receipt_t *rc) { /* UI: export's line on the settings page, import's own page */
    bool import = ctx != NULL;
    if (import) snprintf(K.result, sizeof K.result, "%s", rc->result ? rc->result : "");
    else if (rc->status == INK_RC_DONE && rc->result) {
        int count = 0; const char *p = strstr(rc->result, "\"count\":"), *path = strstr(rc->result, "\"path\":\"");
        if (p) count = atoi(p + 8);
        char where[96] = ""; if (path) { snprintf(where, sizeof where, "%s", path + 8); char *q = strchr(where, '"'); if (q) *q = 0; }
        snprintf(K.result, sizeof K.result, TR("已导出 %d 件到 %s"), count, where);
    } else snprintf(K.result, sizeof K.result, "%s", rc->status == INK_RC_REJECTED ? TR("还没有日程可以导出") :
                    strstr(rc->detail, "card") || strstr(rc->detail, "卡") ? TR("没有存储卡，导不出") : strstr(rc->detail, "computer") || strstr(rc->detail, "电脑") ? TR("存储卡借给电脑了，先断开") :
                    strstr(rc->detail, "space") ? TR("存储卡空间不够") : TR("导出没成功，请稍后再试"));
    if (!cal_mine()) return;
    if (import && !on_page("ImportResult")) ink_ui_push(&CAL_IMPORT, NULL);
    else if (on_page("CalendarSettings") || on_page("ImportResult")) ink_ui_redraw();
}

static void nav(ink_call_t *call) {
    const char *id = call->cap->id;
    int64_t v = 0;
    int y, m, d;
    if (strcmp(id, "calendar.undo")) K.undo_valid = false; /* the undo button stays until the next thing done */
    if (!strcmp(id, "calendar.open")) {
        ink_tm_t t;
        if (on_page("Calendar") && ink_time_valid()) { ink_time_to_local(ink_time_now_s(), &t); cal_month_select(t.year, t.month, t.day); return; }
        K.y = 0;
        ink_ui_push(&CAL_MONTH, NULL); return;
    }
    if (!cal_mine()) { call->rc.status = INK_RC_STALE; return; }
    if (!strcmp(id, "calendar.view")) {
        char view[16] = ""; ink_args_get_str(&call->args, "view", view, sizeof view);
        bool agenda = !strcmp(view, "agenda");
        ink_receipt_t rc; ink_setting_set("calendar.last_view", agenda, call->src, &rc);
        if (agenda && on_page("Calendar")) { memset(&K.position, 0, sizeof K.position); K.past = false; ink_ui_push(&CAL_AGENDA, NULL); }
        else if (!agenda && on_page("Agenda")) ink_ui_pop();
        else if (!on_page(agenda ? "Agenda" : "Calendar")) call->rc.status = INK_RC_STALE;
        return;
    }
    if (!strcmp(id, "calendar.today")) {
        ink_tm_t t; if (!ink_time_valid()) { call->rc.status = INK_RC_STALE; return; }
        ink_time_to_local(ink_time_now_s(), &t);
        if (on_page("Calendar")) cal_month_select(t.year, t.month, t.day);
        else { K.y = t.year; K.m = t.month; K.d = t.day; memset(&K.position, 0, sizeof K.position); K.past = false; K.query[0] = 0; K.searched = false; ink_ui_redraw(); }
        return;
    }
    if (!strcmp(id, "calendar.month_prev") || !strcmp(id, "calendar.month_next")) {
        int k = K.y * 12 + K.m - 1 + (!strcmp(id, "calendar.month_next") ? 1 : -1);
        if (k < 1901 * 12 || k >= 2101 * 12 || !on_page("Calendar")) { call->rc.status = INK_RC_INVALID; return; }
        y = k / 12; m = k % 12 + 1;
        cal_month_select(y, m, 1); return; /* a new month starts on its first day */
    }
    if (!strcmp(id, "calendar.select")) {
        if (!on_page("Calendar")) { call->rc.status = INK_RC_STALE; return; }
        if (read_date(call, &y, &m, &d)) { cal_month_select(y, m, d); return; }
        if (!ink_args_get_int(&call->args, "day", &v) || v < 1 || v > cal_month_days(K.y, K.m)) { call->rc.status = INK_RC_INVALID; return; }
        cal_month_select(K.y, K.m, (int)v); return;
    }
    if (!strcmp(id, "calendar.goto")) {
        if (read_date(call, &y, &m, &d)) {
            if (on_page("Calendar")) { cal_month_select(y, m, d); return; } /* another month: one new page */
            K.y = y; K.m = m; K.d = d;
            if (on_page("ReminderTime")) ink_ui_pop(); /* the page below is built again with the date */
            else ink_ui_redraw();
            return;
        }
        memset(&E, 0, sizeof E); E.part_target = 3; E.pick = (ink_tm_t){.year = K.y, .month = K.m, .day = K.d};
        snprintf(E.owner, sizeof E.owner, "%s", K.member);
        ink_ui_push(&CAL_TIME, NULL); return;
    }
    if (!strcmp(id, "calendar.prev") || !strcmp(id, "calendar.next")) {
        if (!K.list || !ink_ui_list_turn(K.list, !strcmp(id, "calendar.next") ? 1 : -1)) call->rc.status = INK_RC_INVALID;
        return;
    }
    if (!strcmp(id, "calendar.search") || !strcmp(id, "calendar.search_clear") || !strcmp(id, "calendar.earlier")) {
        if (!on_page("Agenda")) { call->rc.status = INK_RC_STALE; return; }
        if (!strcmp(id, "calendar.earlier")) K.past = !K.past;
        else ink_args_get_str(&call->args, "query", K.query, sizeof K.query);
        if (!strcmp(id, "calendar.search_clear")) K.query[0] = 0;
        K.searched = K.query[0] != 0;
        memset(&K.position, 0, sizeof K.position);
        ink_ui_redraw(); return;
    }
    if (!strcmp(id, "calendar.search_focus")) { if (!K.field || ink_textfield_short_focus(K.field)) call->rc.status = INK_RC_STALE; return; }
    if (!strcmp(id, "calendar.day") || !strcmp(id, "calendar.day_prev") || !strcmp(id, "calendar.day_next")) {
        if (read_date(call, &y, &m, &d)) { K.y = y; K.m = m; K.d = d; }
        else if (strcmp(id, "calendar.day")) { cal_civil(cal_day(K.y, K.m, K.d) + (!strcmp(id, "calendar.day_next") ? 1 : -1), &K.y, &K.m, &K.d); }
        if (K.y < 1901 || K.y > 2100) { call->rc.status = INK_RC_INVALID; return; }
        memset(&K.position, 0, sizeof K.position);
        if (on_page("Day")) ink_ui_redraw(); else ink_ui_push(&CAL_DAY, NULL);
        return;
    }
    if (!strcmp(id, "calendar.almanac")) { ink_ui_push(&CAL_ALMANAC, NULL); return; }
    if (!strcmp(id, "calendar.settings")) { /* the page, or one of its two settings changed in place (T1, T2) */
        const char *ids[] = {"calendar.week_start", "calendar.default_reminder"}, *keys[] = {"week_start", "default_reminder"};
        bool set = false;
        for (int i = 0; i < 2; i++) if (ink_args_get_int(&call->args, keys[i], &v)) { ink_setting_set(ids[i], v, call->src, &call->rc); set = true; }
        if (set && call->rc.status == INK_RC_DONE && on_page("CalendarSettings")) ink_ui_redraw();
        else if (!set) { K.result[0] = 0; ink_ui_push(&CAL_SETTINGS, NULL); }
        return;
    }
    if (!strcmp(id, "calendar.import_pick")) { if (ink_file_picker_open(INK_FS_OTHER, picked, NULL) < 0) call->rc.status = INK_RC_FAILED; return; }
    if (!strcmp(id, "calendar.export_now") || !strcmp(id, "calendar.import_run")) {
        bool import = !strcmp(id, "calendar.import_run");
        ink_call_opts_t o = {.src = call->src, .reply_exec = INK_EXEC_UI, .done = transfer_done, .ctx = import ? &K : NULL};
        char args[INK_FS_PATH_MAX + 16] = "{}";
        if (import) ink_args_to_json(&call->args, args, sizeof args);
        o.args_json = args;
        if (ink_cap_call(import ? "calendar.import" : "calendar.export", NULL, &o)) call->rc.status = INK_RC_FAILED;
        return;
    }
    if (!strcmp(id, "calendar.undo")) {
        if (!K.undo_valid) { call->rc.status = INK_RC_STALE; return; }
        ink_cap_invoke_json("system.undo", K.undo, call->src, 0, &call->rc);
        if (call->rc.status == INK_RC_UNDONE) call->rc.status = INK_RC_DONE;
        K.undo_valid = false;
        if (on_page("Calendar") && K.undo_button) { lv_obj_add_flag(K.undo_button, LV_OBJ_FLAG_HIDDEN); for (size_t i = 0; i < K.page.action_count; i++) if (!strcmp(K.actions[i].capability, "calendar.undo")) K.actions[i].enabled = false; ink_ui_content_changed(); } /* the cells follow the events */
        else ink_ui_redraw();
        return;
    }
    call->rc.status = INK_RC_INVALID;
}

/* ------------------------------------------------------------ registration */

#define NAV(id_, title_) {.id = id_, .app = "upcoming", .title = title_, .effect = INK_FX_NAVIGATE, .child_ok = true, .run = nav}
#define NAV_IN(id_, title_, input_) {.id = id_, .app = "upcoming", .title = title_, .input = input_, .effect = INK_FX_NAVIGATE, .child_ok = true, .run = nav}
#define EDIT(id_, title_, input_) {.id = id_, .app = "clock", .title = title_, .input = input_, .effect = INK_FX_NAVIGATE, .child_ok = true, .run = cal_edit_cap}
#define EDIT_UP(id_, title_, input_) {.id = id_, .app = "upcoming", .title = title_, .input = input_, .effect = INK_FX_NAVIGATE, .child_ok = true, .run = cal_edit_cap}
#define DATE_ARG "{\"type\":\"object\",\"properties\":{\"date\":{\"type\":\"string\",\"maxLength\":10}}}"
static const ink_cap_t CAPS[] = {
    NAV("calendar.open", "打开日历（整月，选中今天）"),
    NAV_IN("calendar.view", "日历切换视图：month 月，agenda 议程", "{\"type\":\"object\",\"properties\":{\"view\":{\"type\":\"string\",\"enum\":[\"month\",\"agenda\"]}},\"required\":[\"view\"]}"),
    NAV("calendar.today", "日历回到今天"),
    NAV("calendar.prev", "日历当前页列表的上一页"),
    NAV("calendar.next", "日历当前页列表的下一页"),
    NAV("calendar.month_prev", "月历上个月"),
    NAV("calendar.month_next", "月历下个月"),
    NAV_IN("calendar.select", "月历选一天（day 当月几号；给了 date YYYY-MM-DD 就选那天，前后月的也行）", "{\"type\":\"object\",\"properties\":{\"day\":{\"type\":\"integer\",\"minimum\":1,\"maximum\":31},\"date\":{\"type\":\"string\",\"maxLength\":10}},\"required\":[\"day\"]}"),
    NAV_IN("calendar.goto", "日历跳到某天（不给 date 就打开日期选择）", DATE_ARG),
    NAV_IN("calendar.search", "议程里按标题搜日程（一个字起）", "{\"type\":\"object\",\"properties\":{\"query\":{\"type\":\"string\",\"minLength\":1,\"maxLength\":30}},\"required\":[\"query\"]}"),
    NAV("calendar.search_focus", "议程的搜索框输入"),
    NAV("calendar.search_clear", "清空搜索，回到议程"),
    NAV("calendar.earlier", "议程看更早的或回到接下来的"),
    NAV_IN("calendar.day", "打开某一天（日程、农历、老黄历；不给 date 就是所选那天）", DATE_ARG),
    NAV("calendar.day_prev", "当天页的前一天"),
    NAV("calendar.day_next", "当天页的后一天"),
    NAV("calendar.almanac", "更多老黄历（冲煞、神位、值神、建除）"),
    NAV_IN("calendar.settings", "日历设置（不给参数就打开；week_start 0 周一 1 周日，default_reminder 0 不提醒 1 准时 2 五分钟 3 十分钟 4 半小时 5 一小时 6 一天）",
           "{\"type\":\"object\",\"properties\":{\"week_start\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":1},\"default_reminder\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":6}}}"),
    NAV("calendar.import_pick", "选一个 .ics 文件导入日程"),
    {.id = "calendar.export_now", .app = "upcoming", .title = "导出我的日程（页面上写结果）", .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .run = nav},
    {.id = "calendar.import_run", .app = "upcoming", .title = "导入选好的 .ics 文件并显示结果页", .input = "{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\",\"maxLength\":127}},\"required\":[\"path\"]}",
     .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .expose = INK_EXPOSE_OWNER, .run = nav},
    {.id = "calendar.undo", .app = "upcoming", .title = "撤销刚才的删除", .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .run = nav},
    EDIT("calendar.event", "看一件日程的详情（只读，可编辑和删除）", "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"integer\",\"minimum\":1,\"maximum\":4294967295},\"occurrence\":{\"type\":\"string\",\"maxLength\":20}},\"required\":[\"id\"]}"),
    EDIT("reminder.new", "手动加日程（date 默认所选那天）", DATE_ARG),
    EDIT("reminder.open", "修改我的日程（打开编辑页；重复的给 occurrence）", "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"integer\",\"minimum\":1,\"maximum\":4294967295},\"occurrence\":{\"type\":\"string\",\"maxLength\":20}},\"required\":[\"id\"]}"),
    EDIT("reminder.time", "选开始、结束或到期日（end 结束，until 到期日）", "{\"type\":\"object\",\"properties\":{\"end\":{\"type\":\"boolean\"},\"until\":{\"type\":\"boolean\"}}}"),
    EDIT("reminder.focus", "输入日程标题", NULL),
    EDIT("reminder.draft", "改编辑中的日程：时间各部分（year month day hour minute，或 step=year/month/day/hour/minute/interval/count/custom 加 delta ±1）、all_day、clear_end、repeat（0 不重复 1 每天 2 每周 3 工作日 4 每月 5 每年 6 农历每年）、interval 1-99、weekday 0-6 开关、monthly 0-3、end_kind 0 一直 2 共几次、count 1-730、lead 提前分钟数、remove_lead 0-2、custom 单位 0-3 或 -1 加上",
         "{\"type\":\"object\",\"properties\":{\"year\":{\"type\":\"integer\"},\"month\":{\"type\":\"integer\"},\"day\":{\"type\":\"integer\"},\"hour\":{\"type\":\"integer\"},"
         "\"minute\":{\"type\":\"integer\"},\"step\":{\"type\":\"string\"},\"delta\":{\"type\":\"integer\"},\"all_day\":{\"type\":\"boolean\"},\"clear_end\":{\"type\":\"boolean\"},"
         "\"repeat\":{\"type\":\"integer\"},\"interval\":{\"type\":\"integer\"},\"weekday\":{\"type\":\"integer\"},\"monthly\":{\"type\":\"integer\"},\"end_kind\":{\"type\":\"integer\"},"
         "\"count\":{\"type\":\"integer\"},\"lead\":{\"type\":\"integer\"},\"remove_lead\":{\"type\":\"integer\"},\"custom\":{\"type\":\"integer\"}}}"),
    EDIT_UP("calendar.repeat", "编辑页的重复（频率）", NULL),
    EDIT("calendar.repeat_detail", "编辑页的重复细项（间隔、星期几、每月哪天）", NULL),
    EDIT("calendar.repeat_end", "编辑页的重复结束（一直、到某天、共几次）", NULL),
    EDIT_UP("calendar.reminders", "编辑页的提醒列表", NULL),
    EDIT_UP("calendar.reminder_add", "编辑页加一个提醒（选项）", NULL),
    EDIT_UP("calendar.edit_back", "离开编辑页（有没保存的改动就问保存不保存）", NULL),
    EDIT_UP("calendar.edit_discard", "不保存，离开编辑页", NULL),
    {.id = "reminder.save", .app = "clock", .title = "保存编辑中的日程（重复的给 scope：this following all）", .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .run = cal_edit_cap,
     .input = "{\"type\":\"object\",\"properties\":{\"scope\":{\"type\":\"string\",\"enum\":[\"this\",\"following\",\"all\"]}}}"},
    {.id = "calendar.delete", .app = "upcoming", .title = "删除正在看的日程（重复的给 scope：this following all；能撤销）", .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .run = cal_edit_cap,
     .input = "{\"type\":\"object\",\"properties\":{\"scope\":{\"type\":\"string\",\"enum\":[\"this\",\"following\",\"all\"]}}}"},
};
static const ink_app_def_t APP = {.id = "upcoming", .title = TR_NOOP("日历"), .icon = INK_UI_ICON(calendar), .entry = &CAL_MONTH, .system_app = true, .child_ok = true};

static void changed(void *ctx, int handle, const char *id) { /* the events or the minute moved: the month redraws what changed */
    (void)ctx; (void)handle; (void)id;
    if (!ink_ui_page_current() || !cal_mine()) return;
    if (on_page("Calendar")) ink_ui_after_press(cal_month_refresh, NULL);
    else if (!strcmp(id, "alarm.revision") && (on_page("Agenda") || on_page("Day"))) ink_ui_redraw();
    else if (!strcmp(id, "alarm.revision") && on_page("EventInfo")) ink_ui_redraw();
}
int cal_init(void) {
    return ink_settings_define(SETTINGS, (int)(sizeof SETTINGS / sizeof *SETTINGS)) || ink_cap_register(CAPS, sizeof CAPS / sizeof *CAPS) || ink_app_register(&APP) ||
                   ink_state_watch("alarm.revision", changed, NULL, INK_EXEC_UI) || cal_edit_init() /* two watches, as before: the shared table is nearly full */
               ? -1 : 0;
}
