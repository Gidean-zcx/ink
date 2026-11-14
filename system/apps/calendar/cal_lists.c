/* Agenda and search (Fossify EventListFragment.kt:103-254, MainActivity.kt:1369-1450; Etar AgendaByDayAdapter.java:213-238),
 * the day (DayFragment.kt:47-127) with the almanac (content after Mogo ui_calendar.c:196-283), the settings and the
 * import result, and one shared page of rows for the small choices (scope, leave, repeat, reminders). */
/* ui-scale-runtime: dimension x,y,w,h,top,seg,used Theme-derived list and text geometry. */
#include "cal.h"

#define ROW_B (INK_STATUS_BAR + INK_GAP_S + INK_CONTROL + INK_GAP_XS)

static cal_item_t found[CAL_ITEMS] INK_PSRAM_BSS;

/* Occurrences in [from, until) that match, earliest first, at most CAL_ITEMS (an event stops once it is past the last kept). */
static int collect(int64_t from, int64_t until, const char *query, cal_item_t *out) {
    int n = 0;
    ink_event_t e;
    int cursor = 0;
    while (ink_event_next(&cursor, &e) == 1) {
        if (query && query[0] && !strstr(e.title, query)) continue;
        for (int64_t t = ink_event_occurrence(&e, from); t && t < until; t = ink_event_occurrence(&e, t + (e.duration_s > 0 ? e.duration_s : 1))) {
            if (n == CAL_ITEMS && t >= out[n - 1].at) break;
            int k = n < CAL_ITEMS ? n++ : n - 1;
            while (k && out[k - 1].at > t) { out[k] = out[k - 1]; k--; }
            cal_item_t *it = &out[k];
            ink_tm_t lt; ink_time_to_local(t, &lt);
            it->kind = 1; it->id = e.id; it->at = t;
            it->orig = e.parent ? 0 : t;
            if (e.all_day) snprintf(it->text, sizeof it->text, "%s  %.80s", TR("全天"), e.title);
            else snprintf(it->text, sizeof it->text, "%02d:%02d  %.80s", lt.hour, lt.min, e.title);
            char rule[48] = "", lead[32] = "";
            if (e.freq) cal_rule_text(&e, rule, sizeof rule);
            int32_t most = e.nleads ? e.leads[0] : 0;
            for (int i = 1; i < e.nleads; i++) if (e.leads[i] > most) most = e.leads[i];
            if (e.nleads) cal_lead_text(most, e.all_day, lead, sizeof lead);
            snprintf(it->note, sizeof it->note, "%s%s%s%s%.40s", e.parent ? TR("已改这一次") : "", e.parent && (rule[0] || lead[0]) ? " · " : "", lead, rule[0] && lead[0] ? " · " : "", rule);
            cal_utf8_trim(it->note); cal_utf8_trim(it->text);
        }
    }
    return n;
}

/* The grey date heading rides on the first row of each day ("今天 · 11月13日 周五 · 每周"), a month's name where it turns
 * (A2, A3): a whole row for a heading would halve what a page holds on this screen. */
static void with_headings(const cal_item_t *in, int n) {
    K.count = 0;
    int32_t today = ink_time_valid() ? ink_event_day(ink_time_now_s()) : 0, last = INT32_MIN, last_month = -1;
    for (int i = 0; i < n && K.count < CAL_ITEMS; i++) {
        cal_item_t *it = &K.items[K.count++];
        *it = in[i];
        int32_t day = ink_event_day(in[i].at);
        if (day == last) continue;
        int y, m, d; cal_civil(day, &y, &m, &d);
        char month[24] = "", note[sizeof it->note], date[48];
        if (last_month >= 0 && y * 12 + m != last_month) snprintf(month, sizeof month, TR("%d年%d月 · "), y, m);
        snprintf(note, sizeof note, "%s", it->note);
        snprintf(date, sizeof date, TR("%d月%d日 %s"), m, d, cal_weekday_name(cal_wday(day)));
        cal_fmt(it->note, sizeof it->note, "%s%s%s%s%s", month, day == today ? TR("今天 · ") : day == today + 1 ? TR("明天 · ") : "", date, note[0] ? " · " : "", note);
        cal_utf8_trim(it->note);
        last = day; last_month = y * 12 + m;
    }
}

void cal_build_agenda(void) {
    int y, m, d, n;
    cal_civil(ink_event_day(ink_time_now_s()), &y, &m, &d);
    int64_t today = ink_time_from_local(&(ink_tm_t){.year = y, .month = m, .day = d}), year = 366 * 86400LL;
    if (K.searched) { /* two years either way, then all of it when few came (MainActivity.kt:1388-1404) */
        n = collect(today - 2 * year, today + 2 * year, K.query, found);
        if (n < 30) n = collect(ink_time_from_local(&(ink_tm_t){.year = 1901, .month = 1, .day = 1}), ink_time_from_local(&(ink_tm_t){.year = 2100, .month = 12, .day = 31}), K.query, found);
    } else n = collect(K.past ? today - 90 * 86400LL : today, K.past ? today : today + year, NULL, found);
    if (!K.searched && !K.past) { /* alarms stay listed as the old 接下来 page did (A9) */
        int cursor = 0; ink_alarm_t a;
        while (n < CAL_ITEMS && ink_alarm_next(&cursor, INK_ALARM, &a) == 1) {
            if (!a.enabled || !a.next_s || a.next_s >= today + year) continue;
            int k = n++;
            while (k && found[k - 1].at > a.next_s) { found[k] = found[k - 1]; k--; }
            ink_tm_t lt; ink_time_to_local(a.next_s, &lt);
            found[k] = (cal_item_t){.kind = 2, .id = a.id, .at = a.next_s};
            snprintf(found[k].text, sizeof found[k].text, "%02d:%02d  %.47s", lt.hour, lt.min, a.title[0] ? a.title : TR("闹钟"));
            snprintf(found[k].note, sizeof found[k].note, "%s", TR("闹钟"));
        }
    }
    with_headings(found, n);
}

static void bind_row(lv_obj_t *slot, size_t index, ink_ui_field_spec_t *field, ink_ui_action_spec_t *action, void *ctx) {
    (void)ctx;
    unsigned s = (unsigned)(index % CAL_SLOTS);
    const cal_item_t *it = &K.items[index];
    int w = INK_CANVAS_W - 2 * INK_MARGIN;
    if (!lv_obj_get_child_count(slot)) {
        ink_ui_line(slot, "", INK_FONT_LIST, INK_WEIGHT_REGULAR, 0, INK_GAP_XS, w);
        ink_ui_line(slot, "", INK_FONT_NOTE, INK_WEIGHT_REGULAR, 0, INK_GAP_XS + ink_ui_line_height(INK_FONT_LIST), w);
    }
    lv_obj_t *title = lv_obj_get_child(slot, 0), *note = lv_obj_get_child(slot, 1);
    lv_label_set_text(title, it->text);
    lv_label_set_text(note, it->note);
    lv_obj_set_style_text_color(title, it->kind ? ink_ui_ink() : ink_ui_secondary(), 0);
    lv_obj_set_style_text_color(note, ink_ui_secondary(), 0);
    lv_obj_set_style_text_font(title, ink_ui_font_px(it->kind ? INK_FONT_LIST : INK_FONT_NOTE, INK_WEIGHT_REGULAR), 0);
    ink_tm_t t; ink_time_to_local(it->at, &t);
    snprintf(K.slot_ids[s], sizeof K.slot_ids[s], it->kind == 2 ? "alarm:%u" : it->kind ? "reminder:%u" : "day:%u", it->kind ? (unsigned)it->id : (unsigned)index);
    cal_fmt(K.slot_text[s][0], sizeof K.slot_text[s][0], "%s%s%s", it->text, it->note[0] ? " · " : "", it->note);
    if (it->kind == 2) snprintf(K.slot_args[s], sizeof K.slot_args[s], "{}");
    else snprintf(K.slot_args[s], sizeof K.slot_args[s], "{\"id\":%u,\"occurrence\":\"%04d-%02d-%02d %02d:%02d\"}", (unsigned)it->id, t.year, t.month, t.day, t.hour, t.min);
    *field = (ink_ui_field_spec_t){.id = K.slot_ids[s], .role = it->kind ? "listitem" : "heading", .label = it->kind ? TR("日程") : TR("日期"), .value = K.slot_text[s][0], .object = {K.slot_ids[s], 1}};
    *action = (ink_ui_action_spec_t){.id = K.slot_ids[s], .label = TR("查看"), .capability = it->kind == 2 ? "clock.open" : "calendar.event", .args_json = K.slot_args[s],
                                     .object = field->object, .enabled = it->kind != 0}; /* A8: the details first, read-only */
}

static const ink_ui_action_spec_t PAGING[] = {{.id = "prev", .label = TR_NOOP("上一页"), .capability = "calendar.prev"}, {.id = "next", .label = TR_NOOP("下一页"), .capability = "calendar.next"}};
static void list(lv_obj_t *screen, int y, int bottom, const char *empty) {
    lv_obj_t *body = ink_ui_box(screen, 0, y, INK_CANVAS_W, bottom - y, 0, INK_SURFACE_WHITE, false);
    ink_ui_page_spec_t model = {.id = ink_ui_page_name(), .title = K.page.title, .object = {"calendar", 1}, .actions = PAGING, .action_count = 2};
    int row = ink_ui_row_double(), slots = (bottom - y - INK_CONTROL) / row;
    K.list = ink_ui_list_create(body, &model, &K.position, (unsigned)(slots < 1 ? 1 : slots > CAL_SLOTS ? CAL_SLOTS : slots), 1, row, empty, bind_row, NULL);
    if (K.list) ink_ui_list_refresh(K.list, K.count);
}

static void search_entered(void *ctx, const char *value) { /* the keyboard's search key, never each letter (S2) */
    (void)ctx;
    char args[160];
    ink_jw_t w; ink_jw_init(&w, args, sizeof args);
    ink_jw_raw(&w, value && value[0] ? "{\"query\":" : "{");
    if (value && value[0]) { ink_jw_str(&w, value); }
    ink_jw_raw(&w, "}");
    ink_receipt_t rc;
    ink_cap_invoke_json(value && value[0] ? "calendar.search" : "calendar.search_clear", args, INK_SRC_TOUCH, 0, &rc);
}

static void bound_here(lv_event_t *e) { (void)e; } /* the shared empty state draws its action only with a callback */
static void agenda_create(lv_obj_t *screen, void *arg) {
    (void)arg;
    cal_begin(screen, TR("日历"));
    const char *last = K.undo_valid ? TR("撤销") : K.past ? TR("接下来") : TR("更早的");
    int w = INK_CANVAS_W - 2 * INK_MARGIN, today = cal_pill_w(TR("今天")), lw = cal_pill_w(last), seg = w - today - lw - 2 * INK_GAP_XS, set = cal_pill_w(TR("设置"));
    cal_pill(screen, TR("设置"), INK_CANVAS_W - INK_MARGIN - set, INK_STATUS_BAR + INK_GAP_S, set, false, "calendar.settings", "{}", 1);
    const char *views[] = {TR("月"), TR("议程")};
    lv_obj_t *segments = ink_ui_segments(screen, views, 2, 1, INK_MARGIN, ROW_B, seg, NULL, NULL);
    ink_shell_bind(lv_obj_get_child(segments, 0), "calendar.view", "{\"view\":\"month\"}");
    cal_action(2, "month", TR("月"), "calendar.view", "{\"view\":\"month\"}");
    cal_pill(screen, TR("今天"), INK_MARGIN + seg + INK_GAP_XS, ROW_B, today, false, "calendar.today", "{}", 3);
    if (K.undo_valid) cal_undo_button(screen, INK_CANVAS_W - INK_MARGIN, ROW_B, 4);
    else cal_pill(screen, last, INK_CANVAS_W - INK_MARGIN - lw, ROW_B, lw, false, "calendar.earlier", "{}", 4);
    int y = ROW_B + INK_CONTROL + INK_GAP_S;
    ink_text_cfg_t cfg = {.type = INK_TEXT_PLAIN, .enter = INK_ENTER_SEARCH, .max = 30, .hint = TR("按标题搜索"), .initial = K.query};
    K.field = ink_textfield_create(screen, &cfg, search_entered, NULL, false);
    lv_obj_set_y(K.field, ink_ui_px(y));
    ink_textfield_bind_short(K.field, &K.page, cal_field(0, "query", "searchbox", TR("按标题搜索（calendar.search）"), K.query), cal_action(5, "query", TR("输入搜索"), "calendar.search_focus", "{}"));
    ink_textfield_style(K.field, INK_FIELD_SEARCH, TR("按标题搜索"));
    y += ink_textfield_height(INK_FIELD_SEARCH) + INK_GAP_XS;
    if (K.searched) cal_action(6, "clear", TR("清空搜索"), "calendar.search_clear", "{}");
    cal_build_agenda();
    snprintf(K.texts[1], sizeof K.texts[1], K.searched ? TR("搜索“%s”") : K.past ? TR("最近 90 天") : TR("从今天起"), K.query);
    cal_field(1, "range", "status", TR("范围"), K.texts[1]);
    if (!K.count && !K.searched && !K.past) { /* A7: the shared empty state with 加一件 */
        lv_obj_t *box = ink_ui_box(screen, 0, y, INK_CANVAS_W, INK_CANVAS_H - y, 0, INK_SURFACE_WHITE, false);
        ink_shell_bind(lv_obj_get_child(ink_ui_empty(box, TR("还没有接下来的事"), TR("加一件"), bound_here, NULL), 1), "reminder.new", "{}");
        cal_action(7, "add", TR("加一件"), "reminder.new", "{}");
        cal_field(2, "empty", "status", TR("状态"), TR("还没有接下来的事"));
        return;
    }
    snprintf(K.texts[2], sizeof K.texts[2], TR("没有找到“%s”"), K.query);
    if (K.searched && !K.count) cal_field(2, "result", "status", TR("结果"), K.texts[2]); /* S6 */
    list(screen, y, INK_CANVAS_H - INK_MARGIN, K.searched ? K.texts[2] : TR("这段时间没有安排"));
}
const ink_activity_t CAL_AGENDA = {.name = "Agenda", .app = "upcoming", .create = agenda_create, .snapshot = cal_snapshot, .on_key = cal_key};

/* ------------------------------------------------------------ the day */

static void text_lines(lv_obj_t *screen, int *y, int index, const char *id, const char *label, const char *text, bool grey) {
    int w = INK_CANVAS_W - 2 * INK_MARGIN;
    snprintf(K.texts[index], sizeof K.texts[index], "%s", text);
    lv_obj_t *l = ink_ui_label(screen, K.texts[index], INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_MARGIN, *y, w);
    if (grey) lv_obj_set_style_text_color(l, ink_ui_secondary(), 0);
    *y += ink_ui_text_height(K.texts[index], INK_FONT_NOTE, INK_WEIGHT_REGULAR, w) + INK_GAP_XS;
    cal_field(index, id, "text", label, K.texts[index]);
}

static void day_create(lv_obj_t *screen, void *arg) {
    (void)arg;
    char title[48], lunar[32], line[192];
    int32_t day = cal_day(K.y, K.m, K.d);
    snprintf(title, sizeof title, TR("%d月%d日 %s"), K.m, K.d, cal_weekday_name(cal_wday(day)));
    cal_begin(screen, title);
    int w = INK_CANVAS_W - 2 * INK_MARGIN, half = (w - INK_GAP_XS - (K.undo_valid ? cal_pill_w(TR("撤销")) + INK_GAP_XS : 0)) / 2;
    lv_obj_t *goto_title = ink_ui_box(screen, INK_MARGIN + INK_CONTROL + INK_GAP, INK_STATUS_BAR + INK_GAP_S, w - INK_CONTROL - INK_GAP, INK_CONTROL, 0, INK_SURFACE_WHITE, false);
    lv_obj_set_style_bg_opa(goto_title, LV_OPA_TRANSP, 0);
    ink_shell_bind(goto_title, "calendar.goto", "{}");
    cal_action(1, "goto", TR("跳到日期"), "calendar.goto", "{}");
    cal_pill(screen, TR("前一天"), INK_MARGIN, ROW_B, half, false, "calendar.day_prev", "{}", 2);
    cal_pill(screen, TR("后一天"), INK_MARGIN + half + INK_GAP_XS, ROW_B, half, false, "calendar.day_next", "{}", 3);
    cal_undo_button(screen, INK_CANVAS_W - INK_MARGIN, ROW_B, 4);
    int y = ROW_B + INK_CONTROL + INK_GAP_S;
    cal_lunar_full(K.y, K.m, K.d, lunar, sizeof lunar);
    ink_almanac_t *a = osal_malloc(sizeof *a, OSAL_MEM_EXTERNAL);
    const char *why = NULL, *term = ink_lunar_solar_term(K.y, K.m, K.d);
    bool have = a && cal_almanac(K.y, K.m, K.d, a, &why);
    char gz[16] = ""; ink_lunar_t l;
    if (ink_lunar_from_solar(K.y, K.m, K.d, &l)) ink_lunar_ganzhi(l.year, gz, sizeof gz);
    if (have) snprintf(line, sizeof line, TR("农历%s · %s年 %s月 %s日%s%s"), lunar, a->year_ganzhi, a->month_ganzhi, a->day_ganzhi, term ? " · " : "", term ? term : "");
    else snprintf(line, sizeof line, TR("农历%s · %s年%s%s"), lunar, gz, term ? " · " : "", term ? term : "");
    text_lines(screen, &y, 0, "lunar", TR("农历"), line, false);
    if (have) {
        cal_fmt(line, sizeof line, "%s %s", TR("宜"), a->yi); text_lines(screen, &y, 1, "yi", TR("宜"), line, false);
        snprintf(line, sizeof line, "%s %s", TR("忌"), a->ji); text_lines(screen, &y, 2, "ji", TR("忌"), line, false);
    } else text_lines(screen, &y, 1, "almanac", TR("老黄历"), why ? why : TR("这天的老黄历查不到"), true); /* D6, D7: the rest stays */
    osal_free(a);
    if (have) {
        lv_obj_t *more = ink_ui_list_row(screen, "book", TR("更多老黄历"), NULL, INK_MARGIN, y, w, false, NULL, NULL);
        ink_shell_bind(more, "calendar.almanac", "{}");
        cal_action(5, "almanac", TR("更多老黄历"), "calendar.almanac", "{}");
        y += INK_CONTROL;
    }
    int bottom = INK_CANVAS_H - INK_MARGIN - INK_PRIMARY - INK_GAP_S;
    char add[48]; snprintf(add, sizeof add, "{\"date\":\"%04d-%02d-%02d\"}", K.y, K.m, K.d);
    ink_shell_bind(ink_ui_pill(screen, TR("加一件"), INK_MARGIN, bottom + INK_GAP_S, w, INK_PRIMARY, true, NULL, NULL), "reminder.new", add);
    cal_action(6, "add", TR("加一件"), "reminder.new", add);
    K.count = (size_t)cal_events_on(day, K.items, CAL_ITEMS);
    list(screen, y, bottom, TR("这天没有安排"));
}
const ink_activity_t CAL_DAY = {.name = "Day", .app = "upcoming", .create = day_create, .snapshot = cal_snapshot, .on_key = cal_key};

static void almanac_create(lv_obj_t *screen, void *arg) {
    (void)arg;
    char title[48], line[192];
    snprintf(title, sizeof title, TR("老黄历 · %d月%d日"), K.m, K.d);
    cal_begin(screen, title);
    int y = INK_STATUS_BAR + INK_GAP_S + INK_CONTROL + INK_GAP;
    ink_almanac_t *a = osal_malloc(sizeof *a, OSAL_MEM_EXTERNAL);
    const char *why = NULL;
    if (!a || !cal_almanac(K.y, K.m, K.d, a, &why)) text_lines(screen, &y, 0, "almanac", TR("老黄历"), why ? why : TR("这天的老黄历查不到"), true);
    else { /* Mogo's fields and order (ui_calendar.c:236-262), our own words; no lunar_text: the lunar date is lunar.c's (H6) */
        snprintf(line, sizeof line, TR("冲%s · 煞%s"), a->clash, a->sha); text_lines(screen, &y, 0, "clash", TR("冲煞"), line, false);
        snprintf(line, sizeof line, TR("财神 %s · 喜神 %s · 福神 %s"), a->wealth, a->joy, a->fortune); text_lines(screen, &y, 1, "gods", TR("三神方位"), line, false);
        snprintf(line, sizeof line, "%s %s", TR("吉神宜趋"), a->auspicious); text_lines(screen, &y, 2, "auspicious", TR("吉神宜趋"), line, false);
        snprintf(line, sizeof line, "%s %s", TR("凶煞宜忌"), a->inauspicious); text_lines(screen, &y, 3, "inauspicious", TR("凶煞宜忌"), line, false);
        snprintf(line, sizeof line, TR("值神 %s（%s）"), a->guardian, a->guardian_type); text_lines(screen, &y, 4, "guardian", TR("值神"), line, false);
        snprintf(line, sizeof line, TR("建除 %s日"), a->officer); text_lines(screen, &y, 5, "officer", TR("建除"), line, false);
        cal_fmt(line, sizeof line, "%s %s", TR("宜"), a->yi); text_lines(screen, &y, 6, "yi", TR("宜"), line, false);
        snprintf(line, sizeof line, "%s %s", TR("忌"), a->ji); text_lines(screen, &y, 7, "ji", TR("忌"), line, false);
    }
    osal_free(a);
    text_lines(screen, &y, 8, "notice", TR("说明"), TR("这些是民俗说法"), true); /* H5 */
}
const ink_activity_t CAL_ALMANAC = {.name = "Almanac", .app = "upcoming", .create = almanac_create, .snapshot = cal_snapshot};

/* ------------------------------------------------------------ one page of rows for the small choices */

/* K.rows filled by the page: each a row bound to its capability (a two-line row when it has a value). */
void cal_choices(lv_obj_t *screen, const char *title, const char *note, int y) {
    (void)title;
    int w = INK_CANVAS_W - 2 * INK_MARGIN;
    if (note && note[0]) { text_lines(screen, &y, CAL_FIELDS - 1, "note", TR("说明"), note, true); y += INK_GAP_XS; }
    for (int i = 0; i < K.nrows && i < CAL_ROWS; i++) {
        cal_row_t *r = &K.rows[i];
        int h = r->value[0] ? ink_ui_row_double() : INK_CONTROL;
        if (r->primary) { cal_pill(screen, r->label, INK_MARGIN, INK_CANVAS_H - INK_MARGIN - INK_PRIMARY, w, true, r->cap, r->args, 1 + i); continue; }
        lv_obj_t *row = ink_ui_list_row(screen, r->on ? "check" : r->icon ? r->icon : "next", r->label, r->value[0] ? r->value : NULL, INK_MARGIN, y, w, r->value[0] != 0, NULL, NULL);
        if (r->cap) ink_shell_bind(row, r->cap, r->args);
        if (r->cap) cal_action(1 + i, r->label, r->label, r->cap, r->args);
        if (i < CAL_FIELDS - 1) cal_field(i, r->label, r->on ? "option-selected" : "option", r->label, r->value);
        y += h;
        if (i + 1 < K.nrows) ink_ui_group_divider(screen, y, w);
    }
}

static void settings_create(lv_obj_t *screen, void *arg) { /* T1, T2 and the files (Fossify SettingsActivity.kt:501-506, :747-751, :1294-1382) */
    (void)arg;
    cal_begin(screen, TR("日历设置"));
    static const char *const LEADS[] = {TR_NOOP("不提醒"), TR_NOOP("准时"), TR_NOOP("提前 5 分钟"), TR_NOOP("提前 10 分钟"), TR_NOOP("提前 30 分钟"), TR_NOOP("提前 1 小时"), TR_NOOP("提前 1 天")};
    int64_t start = ink_setting_get("calendar.week_start"), lead = ink_setting_get("calendar.default_reminder");
    K.nrows = 0;
    cal_row_t *r = &K.rows[K.nrows++];
    *r = (cal_row_t){.label = TR("一周从哪天开始"), .cap = "calendar.settings", .icon = "calendar"};
    snprintf(r->value, sizeof r->value, "%s", start ? TR("周日（点一下换成周一）") : TR("周一（点一下换成周日）"));
    snprintf(r->args, sizeof r->args, "{\"week_start\":%d}", start ? 0 : 1);
    r = &K.rows[K.nrows++];
    *r = (cal_row_t){.label = TR("新建时的提醒"), .cap = "calendar.settings", .icon = "bell"};
    snprintf(r->value, sizeof r->value, TR("%s（点一下换下一个）"), TR(LEADS[lead % 7]));
    snprintf(r->args, sizeof r->args, "{\"default_reminder\":%d}", (int)((lead + 1) % 7));
    { /* P11: children too, within their own space and what the picker lets them read */
        r = &K.rows[K.nrows++]; *r = (cal_row_t){.label = TR("导出日程（.ics）"), .cap = "calendar.export_now", .icon = "upload"}; snprintf(r->args, sizeof r->args, "{}");
        cal_fmt(r->value, sizeof r->value, "%s", K.result[0] ? K.result : TR("写到我的空间，含过去的"));
        r = &K.rows[K.nrows++]; *r = (cal_row_t){.label = TR("导入日程（.ics）"), .cap = "calendar.import_pick", .icon = "download"}; snprintf(r->args, sizeof r->args, "{}");
        snprintf(r->value, sizeof r->value, "%s", TR("从文件里选"));
    }
    cal_choices(screen, NULL, NULL, INK_STATUS_BAR + INK_GAP_S + INK_CONTROL + INK_GAP);
}
const ink_activity_t CAL_SETTINGS = {.name = "CalendarSettings", .app = "upcoming", .create = settings_create, .snapshot = cal_snapshot};

static void import_create(lv_obj_t *screen, void *arg) { /* P9: a page that stays, with the four numbers */
    (void)arg;
    cal_begin(screen, TR("导入结果"));
    int y = INK_STATUS_BAR + INK_GAP_S + INK_CONTROL + INK_GAP, imported = 0, skipped = 0, existing = 0, full = 0;
    char error[64] = "", line[192];
    ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL);
    if (j && !ink_json_parse(j, K.result, strlen(K.result))) {
        int64_t v;
        if (ink_json_int(j, ink_json_find(j, 0, "imported"), &v)) imported = (int)v;
        if (ink_json_int(j, ink_json_find(j, 0, "skipped"), &v)) skipped = (int)v;
        if (ink_json_int(j, ink_json_find(j, 0, "existing"), &v)) existing = (int)v;
        if (ink_json_int(j, ink_json_find(j, 0, "full"), &v)) full = (int)v;
        ink_json_str(j, ink_json_find(j, 0, "error"), error, sizeof error);
    }
    osal_free(j);
    if (error[0]) snprintf(line, sizeof line, TR("没有导入：%s"), strstr(error, "calendar") ? TR("不是日历文件（.ics）") : strstr(error, "card") ? TR("没有存储卡") :
                           strstr(error, "computer") ? TR("存储卡借给电脑了") : TR("文件读不出来"));
    else snprintf(line, sizeof line, TR("导入 %d 件；跳过 %d 件看不懂的规则；%d 件已存在；日程满了，%d 件没导入"), imported, skipped, existing, full);
    text_lines(screen, &y, 0, "result", TR("结果"), line, false);
    K.nrows = 1;
    K.rows[0] = (cal_row_t){.label = TR("好"), .cap = "shell.back", .primary = true};
    snprintf(K.rows[0].args, sizeof K.rows[0].args, "{}");
    cal_choices(screen, NULL, NULL, y);
}
const ink_activity_t CAL_IMPORT = {.name = "ImportResult", .app = "upcoming", .create = import_create, .snapshot = cal_snapshot};
