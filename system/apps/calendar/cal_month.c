/* The month on one screen (Fossify MonthlyCalendarImpl.kt:15, :37-73; MonthDayFragment.kt:115-162): 42 cells, the
 * selected day below. Selecting a day redraws the old cell, the new one and the block under the grid (D-127). */
/* ui-scale-runtime: dimension x,y,w,h,top,seg,line,cw Theme-derived month grid geometry: the canvas in seven columns (M15). */
#include "cal.h"
#include "ink/probe.h"

#define ROW_B (INK_STATUS_BAR + INK_GAP_S + INK_CONTROL + INK_GAP_XS)

static int grid_top(void) { return ROW_B + INK_CONTROL + INK_GAP_XS + ink_ui_line_height(INK_FONT_NOTE); }
static int col_x(int c) { return c * INK_CANVAS_W / 7; }
static bool shown(void) { return !strcmp(ink_ui_page_name(), "Calendar") && K.cells[0]; }

/* Every occurrence in the 42 days, each event walked once from the window's start (not 42 x each event). */
static void compute_marks(void) {
    memset(K.marks, 0, sizeof K.marks);
    int y, m, d;
    cal_civil(K.first, &y, &m, &d);
    int64_t from = ink_time_from_local(&(ink_tm_t){.year = y, .month = m, .day = d});
    cal_civil(K.first + 42, &y, &m, &d);
    int64_t until = ink_time_from_local(&(ink_tm_t){.year = y, .month = m, .day = d});
    ink_event_t e;
    int cursor = 0;
    while (ink_event_next(&cursor, &e) == 1)
        for (int64_t t = ink_event_occurrence(&e, from); t && t < until; t = ink_event_occurrence(&e, t + (e.duration_s > 0 ? e.duration_s : 1))) {
            int32_t a = ink_event_day(t) - K.first, b = ink_event_day(t + (e.duration_s > 0 ? e.duration_s - 1 : 0)) - K.first;
            for (int32_t i = a < 0 ? 0 : a; i <= b && i < 42; i++) K.marks[i] |= e.nleads ? 2 : 1;
        }
}

/* What a day holds, sorted: "10:00 周会" and a grey note ("每周 · 提前 10 分钟", "已改这一次"). */
int cal_events_on(int32_t day, cal_item_t *items, int max) {
    int y, m, d, n = 0;
    cal_civil(day, &y, &m, &d);
    int64_t from = ink_time_from_local(&(ink_tm_t){.year = y, .month = m, .day = d});
    cal_civil(day + 1, &y, &m, &d);
    int64_t until = ink_time_from_local(&(ink_tm_t){.year = y, .month = m, .day = d});
    ink_event_t e;
    int cursor = 0;
    while (n < max && ink_event_next(&cursor, &e) == 1)
        for (int64_t t = ink_event_occurrence(&e, from); n < max && t && t < until; t = ink_event_occurrence(&e, t + (e.duration_s > 0 ? e.duration_s : 1))) {
            cal_item_t *it = &items[n];
            int k = n++;
            while (k && items[k - 1].at > t) { items[k] = items[k - 1]; k--; }
            it = &items[k];
            ink_tm_t lt; ink_time_to_local(t, &lt);
            it->kind = 1; it->id = e.id; it->at = t; it->orig = t;
            if (e.all_day || t < from) snprintf(it->text, sizeof it->text, "%s  %.80s", TR("全天"), e.title);
            else snprintf(it->text, sizeof it->text, "%02d:%02d  %.80s", lt.hour, lt.min, e.title);
            char rule[48] = "", lead[32] = "";
            if (e.freq) cal_rule_text(&e, rule, sizeof rule);
            int32_t first = e.nleads ? e.leads[0] : 0;
            for (int i = 1; i < e.nleads; i++) if (e.leads[i] > first) first = e.leads[i];
            if (e.nleads) cal_lead_text(first, e.all_day, lead, sizeof lead);
            snprintf(it->note, sizeof it->note, "%s%s%s%s%.40s", e.parent ? TR("已改这一次") : "", e.parent && (rule[0] || lead[0]) ? " · " : "", lead,
                     rule[0] && lead[0] ? " · " : "", rule);
            cal_utf8_trim(it->note); cal_utf8_trim(it->text);
        }
    ink_alarm_t a; /* the day's alarms too, as the old page listed them (A9) */
    for (cursor = 0; n < max && ink_alarm_next(&cursor, INK_ALARM, &a) == 1;)
        for (int64_t t = a.enabled ? ink_alarm_occurrence(&a, from) : 0; n < max && t && t < until; t = ink_alarm_occurrence(&a, t + 1)) {
            int k = n++;
            while (k && items[k - 1].at > t) { items[k] = items[k - 1]; k--; }
            ink_tm_t lt; ink_time_to_local(t, &lt);
            items[k] = (cal_item_t){.kind = 2, .id = a.id, .at = t};
            snprintf(items[k].text, sizeof items[k].text, "%02d:%02d  %.47s", lt.hour, lt.min, a.title[0] ? a.title : TR("闹钟"));
            snprintf(items[k].note, sizeof items[k].note, "%s", TR("闹钟"));
        }
    return n;
}

static void fill_cell(int i) {
    lv_obj_t *box = K.cells[i];
    lv_obj_clean(box);
    int y, m, d, cw = col_x(i % 7 + 1) - col_x(i % 7);
    cal_civil(K.first + i, &y, &m, &d);
    bool selected = y == K.y && m == K.m && d == K.d, today = y == K.ty && m == K.tm && d == K.td, other = m != K.m;
    ink_ui_surface_style(box, selected ? INK_SURFACE_BLACK : INK_SURFACE_WHITE, false);
    lv_color_t color = selected ? ink_ui_paper() : other ? ink_ui_secondary() : ink_ui_ink();
    char text[24];
    snprintf(text, sizeof text, "%d", d);
    lv_obj_t *label = ink_ui_line(box, text, INK_FONT_LIST, INK_WEIGHT_MEDIUM, 0, 0, cw);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(label, color, 0);
    cal_lunar_short(y, m, d, text, sizeof text); /* two characters, laid by the glyphs so both fit in 108 at the largest size */
    label = ink_ui_line(box, text, INK_FONT_NOTE, INK_WEIGHT_REGULAR, 0, INK_CONTROL - ink_ui_line_height(INK_FONT_NOTE), cw);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(label, color, 0);
    if (today) { /* today: a heavy line under the number, not a colour (M4) */
        lv_obj_t *bar = ink_ui_box(box, (cw - INK_GAP_L) / 2, ink_ui_line_height(INK_FONT_LIST) - INK_GAP_XS, INK_GAP_L, INK_STROKE_FOCUS, 0, selected ? INK_SURFACE_WHITE : INK_SURFACE_BLACK, false);
        lv_obj_remove_flag(bar, LV_OBJ_FLAG_CLICKABLE);
    }
    for (int bit = 0; bit < 2; bit++) if (K.marks[i] & (1u << bit)) { /* an event: a hollow square; one that rings: a filled one (M6) */
        bool filled = bit == 1;
        lv_obj_t *mark = ink_ui_box(box, cw - INK_GAP_M - bit * INK_GAP_M, INK_GAP_XS, INK_GAP_S, INK_GAP_S, 0, filled != selected ? INK_SURFACE_BLACK : INK_SURFACE_WHITE, !filled);
        lv_obj_set_style_border_color(mark, selected ? ink_ui_paper() : ink_ui_ink(), 0);
        lv_obj_remove_flag(mark, LV_OBJ_FLAG_CLICKABLE);
    }
}

static void fill_block(void) {
    char lunar[32], yi[96] = "";
    cal_lunar_full(K.y, K.m, K.d, lunar, sizeof lunar);
    ink_almanac_t *a = osal_malloc(sizeof *a, OSAL_MEM_EXTERNAL);
    const char *why = NULL;
    if (a && cal_almanac(K.y, K.m, K.d, a, &why)) {
        char words[96]; cal_fmt(words, sizeof words, "%s", a->yi);
        int spaces = 0; for (char *p = words; *p; p++) if (*p == ' ' && ++spaces == 2) *p = 0;
        if (words[0]) cal_fmt(yi, sizeof yi, " · %s %s", TR("宜"), words);
    }
    osal_free(a);
    snprintf(K.texts[1], sizeof K.texts[1], TR("%d月%d日 %s · %s%s"), K.m, K.d, cal_weekday_name(cal_wday(cal_day(K.y, K.m, K.d))), lunar, yi);
    cal_item_t *items = K.items;
    int n = cal_events_on(cal_day(K.y, K.m, K.d), items, 8);
    if (!n) snprintf(K.texts[2], sizeof K.texts[2], "%s", TR("这天没有安排"));
    else if (n == 1) snprintf(K.texts[2], sizeof K.texts[2], "%s", items[0].text);
    else snprintf(K.texts[2], sizeof K.texts[2], TR("这天 %d 件事"), n);
    for (int i = 0; i < 2; i++) /* the same words draw nothing: a new minute leaves the block alone */
        if (K.block_lines[i] && strcmp(lv_label_get_text(K.block_lines[i]), K.texts[1 + i])) lv_label_set_text(K.block_lines[i], K.texts[1 + i]);
    snprintf(K.texts[3], sizeof K.texts[3], "%s", ink_lunar_solar_term(K.y, K.m, K.d) ? ink_lunar_solar_term(K.y, K.m, K.d) : "");
    char args[48]; snprintf(args, sizeof args, "{\"day\":%d}", K.d);
    cal_action(8, "select", TR("选一天"), "calendar.select", args);
}

static void marks_text(void) { /* the snapshot names each day that has something, and which cells are the months around */
    size_t n = 0;
    K.texts[4][0] = 0;
    for (int i = 0; i < 42 && n < sizeof K.texts[4] - 24; i++) {
        int y, m, d; cal_civil(K.first + i, &y, &m, &d);
        if (K.marks[i]) n += (size_t)snprintf(K.texts[4] + n, sizeof K.texts[4] - n, "%s%s%d%s:%s", n ? " " : "", m != K.m ? (i < 7 ? TR("上月") : TR("下月")) : "", d, TR("日"),
                                              K.marks[i] & 2 ? TR("提醒") : TR("日程"));
    }
    int y, m, d; cal_civil(K.first, &y, &m, &d);
    int ly, lm, ld; cal_civil(K.first + 41, &ly, &lm, &ld);
    snprintf(K.texts[5], sizeof K.texts[5], TR("六周 %d月%d日 到 %d月%d日（不是本月的是上月或下月，浅灰）"), m, d, lm, ld);
}

static void create(lv_obj_t *screen, void *arg) {
    (void)arg;
    uint64_t t0 = osal_now_us();
    char title[48];
    if (!K.y && ink_time_valid()) { ink_tm_t t; ink_time_to_local(ink_time_now_s(), &t); K.y = t.year; K.m = t.month; K.d = t.day; }
    snprintf(title, sizeof title, TR("%d年%d月"), K.y, K.m);
    cal_begin(screen, ink_time_valid() ? title : TR("日历"));
    memset(K.cells, 0, sizeof K.cells); K.block = K.block_lines[0] = K.block_lines[1] = NULL;
    const char *labels[] = {TR("今天"), TR("上月"), TR("下月")}, *caps[] = {"calendar.today", "calendar.month_prev", "calendar.month_next"};
    int w = INK_CANVAS_W - 2 * INK_MARGIN, pw[3], seg = w, set = cal_pill_w(TR("设置"));
    for (int i = 0; i < 3; i++) seg -= (pw[i] = cal_pill_w(labels[i])) + INK_GAP_XS;
    cal_pill(screen, TR("设置"), INK_CANVAS_W - INK_MARGIN - set, INK_STATUS_BAR + INK_GAP_S, set, false, "calendar.settings", "{}", 1);
    if (!ink_time_valid()) { /* M12: no grid without the time */
        snprintf(K.texts[0], sizeof K.texts[0], "%s", TR("请先设置时间"));
        ink_ui_notice(screen, K.texts[0], ROW_B);
        cal_pill(screen, TR("去设置时间"), INK_MARGIN, ROW_B + INK_LINE_NOTE + INK_GAP, w, true, "shell.time_edit", "{}", 2);
        cal_field(0, "state", "status", TR("状态"), K.texts[0]);
        return;
    }
    ink_tm_t now; ink_time_to_local(ink_time_now_s(), &now);
    K.ty = now.year; K.tm = now.month; K.td = now.day;
    lv_obj_t *goto_title = ink_ui_box(screen, INK_MARGIN + INK_CONTROL + INK_GAP, INK_STATUS_BAR + INK_GAP_S, INK_CANVAS_W - 2 * INK_MARGIN - INK_CONTROL - set - 2 * INK_GAP, INK_CONTROL, 0, INK_SURFACE_WHITE, false);
    lv_obj_set_style_bg_opa(goto_title, LV_OPA_TRANSP, 0);
    ink_shell_bind(goto_title, "calendar.goto", "{}"); /* M3: the title picks a date */
    cal_action(2, "goto", TR("跳到日期"), "calendar.goto", "{}");
    const char *views[] = {TR("月"), TR("议程")};
    lv_obj_t *segments = ink_ui_segments(screen, views, 2, 0, INK_MARGIN, ROW_B, seg, NULL, NULL);
    ink_shell_bind(lv_obj_get_child(segments, 1), "calendar.view", "{\"view\":\"agenda\"}");
    cal_action(3, "agenda", TR("议程"), "calendar.view", "{\"view\":\"agenda\"}");
    for (int i = 0, x = INK_MARGIN + seg + INK_GAP_XS; i < 3; x += pw[i++] + INK_GAP_XS) cal_pill(screen, labels[i], x, ROW_B, pw[i], false, caps[i], "{}", 4 + i);
    int week_start = ink_setting_get("calendar.week_start") ? 0 : 1, top = grid_top();
    for (int c = 0; c < 7; c++) {
        lv_obj_t *l = ink_ui_line(screen, cal_weekday_name((c + week_start) % 7) + (strcmp(ink_lang_code(), "zh") ? 0 : strlen("周")), INK_FONT_NOTE, INK_WEIGHT_REGULAR,
                                  col_x(c), top - ink_ui_line_height(INK_FONT_NOTE), col_x(c + 1) - col_x(c));
        lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    }
    int32_t first = cal_day(K.y, K.m, 1);
    K.first = first - (cal_wday(first) - week_start + 7) % 7;
    compute_marks();
    for (int i = 0; i < 42; i++) {
        int y, m, d; cal_civil(K.first + i, &y, &m, &d);
        int x = col_x(i % 7);
        K.cells[i] = ink_ui_box(screen, x, top + i / 7 * INK_CONTROL, col_x(i % 7 + 1) - x, INK_CONTROL, 0, INK_SURFACE_WHITE, false);
        char args[48]; snprintf(args, sizeof args, "{\"day\":%d,\"date\":\"%04d-%02d-%02d\"}", d, y, m, d);
        ink_shell_bind(K.cells[i], "calendar.select", args); /* M9: a neighbouring month's day turns to it */
        fill_cell(i);
    }
    int line = top + 6 * INK_CONTROL + INK_GAP_XS;
    K.block = ink_ui_box(screen, 0, line, INK_CANVAS_W, INK_CANVAS_H - line, 0, INK_SURFACE_WHITE, false);
    ink_shell_bind(K.block, "calendar.day", "{}");
    K.block_lines[0] = ink_ui_line(K.block, "", INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_MARGIN, 0, w);
    lv_obj_set_style_text_color(K.block_lines[0], ink_ui_secondary(), 0);
    K.block_lines[1] = ink_ui_line(K.block, "", INK_FONT_LIST, INK_WEIGHT_REGULAR, INK_MARGIN, ink_ui_line_height(INK_FONT_NOTE), w - (K.undo_valid ? cal_pill_w(TR("撤销")) + INK_GAP_XS : 0));
    K.undo_button = cal_undo_button(K.block, INK_CANVAS_W - INK_MARGIN, ink_ui_line_height(INK_FONT_NOTE), 10);
    cal_action(7, "day", TR("打开这一天"), "calendar.day", "{}");
    fill_block();
    marks_text();
    snprintf(K.texts[0], sizeof K.texts[0], TR("%d年%d月"), K.y, K.m);
    cal_field(0, "month", "heading", TR("月份"), K.texts[0]);
    cal_field(1, "selected", "text", TR("所选日期"), K.texts[1]);
    cal_field(2, "day", "text", TR("这天"), K.texts[2]);
    cal_field(3, "term", "text", TR("节气"), K.texts[3]);
    cal_field(4, "marks", "text", TR("有事的日子"), K.texts[4]);
    cal_field(5, "grid", "text", TR("月格"), K.texts[5]);
    ink_probe_value("calendar.month_ms", (double)(osal_now_us() - t0) / 1000.0);
}

/* A day picked: in this month only its cell, the old one and the block redraw; another month is a new page. */
void cal_month_select(int y, int m, int d) {
    if (!shown() || y != K.y || m != K.m) { K.y = y; K.m = m; K.d = d; ink_ui_redraw(); return; }
    int old = cal_day(K.y, K.m, K.d) - K.first, now = cal_day(y, m, d) - K.first;
    K.d = d;
    if (old >= 0 && old < 42) fill_cell(old);
    if (now >= 0 && now < 42) fill_cell(now);
    fill_block();
    ink_ui_content_changed();
}

/* The events or the minute changed while the month is up (after any press): only the cells whose marks or today
 * changed, and the block. */
void cal_month_refresh(void *arg) {
    (void)arg;
    if (!shown() || !cal_mine() || !ink_time_valid()) return;
    uint8_t before[42];
    memcpy(before, K.marks, sizeof before);
    int32_t was = cal_day(K.ty, K.tm, K.td) - K.first;
    ink_tm_t now; ink_time_to_local(ink_time_now_s(), &now);
    K.ty = now.year; K.tm = now.month; K.td = now.day;
    int32_t is = cal_day(K.ty, K.tm, K.td) - K.first;
    compute_marks();
    bool any = was != is;
    for (int i = 0; i < 42; i++) if (before[i] != K.marks[i] || ((i == was || i == is) && was != is)) { fill_cell(i); any = true; }
    char line[sizeof K.texts[2]];
    snprintf(line, sizeof line, "%s", K.texts[2]);
    fill_block();
    marks_text();
    if (any || strcmp(line, K.texts[2])) ink_ui_content_changed();
}

static void destroy(void) { memset(K.cells, 0, sizeof K.cells); K.block = K.block_lines[0] = K.block_lines[1] = K.undo_button = NULL; }
static bool key(ink_key_t k, bool lng) { /* the cover's page keys turn the month (M2) */
    (void)lng;
    if (k != INK_KEY_PREV && k != INK_KEY_NEXT) return false;
    ink_receipt_t rc;
    ink_cap_invoke(k == INK_KEY_NEXT ? "calendar.month_next" : "calendar.month_prev", NULL, INK_SRC_TOUCH, 0, &rc);
    return true;
}
const ink_activity_t CAL_MONTH = {.name = "Calendar", .app = "upcoming", .create = create, .destroy = destroy, .snapshot = cal_snapshot, .on_key = key};
