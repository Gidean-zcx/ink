/* Details first (Etar EventInfoFragment.java:1149-1152), then the editor (Fossify EventActivity.kt:817-1010, :1591-1610,
 * :1732-1801, :274-301): start and end on the shared picker, up to three reminders, repeats with their details and end,
 * the three scopes (Etar EditEventHelper.java:398-470, DeleteEventHelper.java:73-75) as pages of rows, not dialogs. */
/* ui-scale-runtime: dimension x,y,w,half,top,row Theme-derived editor geometry. */
#include <stdlib.h>
#include "cal.h"

cal_edit_t E INK_PSRAM_BSS;
static const char *const PARTS[] = {"year", "month", "day", "hour", "minute"};
static const char *const REPEATS[] = {TR_NOOP("不重复"), TR_NOOP("每天"), TR_NOOP("每周"), TR_NOOP("工作日"), TR_NOOP("每月"), TR_NOOP("每年"), TR_NOOP("农历每年")};
static const char *const UNITS[] = {TR_NOOP("分钟"), TR_NOOP("小时"), TR_NOOP("天"), TR_NOOP("当天几点")};
static lv_obj_t *row_values[4], *footer, *body;
#define TOP (INK_STATUS_BAR + INK_GAP_S + INK_CONTROL + INK_GAP)

static bool editor_page(void) {
    static const char *const NAMES[] = {"EventInfo", "ReminderEdit", "ReminderTime", "RepeatRule", "RepeatDetail", "RepeatEnd", "Reminders", "ReminderAdd", "Scope", "Leave", NULL};
    for (int i = 0; NAMES[i]; i++) if (!strcmp(ink_ui_page_name(), NAMES[i])) return true;
    return false;
}
static void leave_editor(void) { while (editor_page() && ink_ui_depth() > 1) ink_ui_pop(); }
static int repeat_index(const ink_event_t *e) {
    static const int OF[] = {0, 1, 2, 4, 5, 6};
    return e->freq == INK_FREQ_WEEKLY && e->weekdays == 0x3E ? 3 : OF[e->freq % 6];
}
static void when_text(const ink_tm_t *t, bool all_day, char *out, size_t cap) {
    if (all_day) snprintf(out, cap, "%04d-%02d-%02d", t->year, t->month, t->day);
    else snprintf(out, cap, "%04d-%02d-%02d %02d:%02d", t->year, t->month, t->day, t->hour, t->min);
}
static void leads_text(char *out, size_t cap) {
    size_t n = 0;
    out[0] = 0;
    if (!E.e.nleads) snprintf(out, cap, "%s", TR("不提醒"));
    for (int i = 0; i < E.e.nleads && n < cap; i++) {
        char one[32]; cal_lead_text(E.e.leads[i], E.e.all_day, one, sizeof one);
        n += (size_t)snprintf(out + n, cap - n, "%s%s", i ? "、" : "", one);
    }
}
static void end_text(char *out, size_t cap) {
    if (!E.has_end) { snprintf(out, cap, "%s", TR("未设置")); return; }
    when_text(&E.end, E.e.all_day, out, cap);
}
/* The draft's start, end and rule written into the event fields the service reads. */
static void draft_to_event(void) {
    E.e.year = (int16_t)E.start.year; E.e.month = (uint8_t)E.start.month; E.e.day = (uint8_t)E.start.day;
    E.e.hour = (uint8_t)(E.e.all_day ? 0 : E.start.hour); E.e.min = (uint8_t)(E.e.all_day ? 0 : E.start.min);
}

/* ------------------------------------------------------------ the read-only details (I1, I3) */

static void info_create(lv_obj_t *screen, void *arg) {
    (void)arg;
    cal_begin(screen, TR("日程"));
    ink_event_t e;
    int y = TOP, w = INK_CANVAS_W - 2 * INK_MARGIN;
    if (ink_event_get(E.e.id, &e)) { /* deleted elsewhere: said so, nothing jumps away */
        snprintf(K.texts[0], sizeof K.texts[0], "%s", TR("这件事已经删掉了"));
        ink_ui_notice(screen, K.texts[0], y);
        cal_field(0, "state", "status", TR("状态"), K.texts[0]);
        cal_pill(screen, TR("返回"), INK_MARGIN, INK_CANVAS_H - INK_MARGIN - INK_PRIMARY, w, true, "shell.back", "{}", 1);
        return;
    }
    E.e = e;
    int64_t start = E.occurrence ? ink_event_occurrence(&e, E.occurrence) : ink_event_occurrence(&e, 0);
    if (!start) start = ink_event_occurrence(&e, 0);
    ink_tm_t a, b; ink_time_to_local(start, &a); ink_time_to_local(start + (e.duration_s > 0 ? e.duration_s - (e.all_day ? 1 : 0) : 0), &b);
    snprintf(K.texts[0], sizeof K.texts[0], "%s", e.title);
    lv_obj_t *t = ink_ui_label(screen, K.texts[0], INK_FONT_SUBTITLE, INK_WEIGHT_BOLD, INK_MARGIN, y, w);
    (void)t;
    y += ink_ui_text_height(K.texts[0], INK_FONT_SUBTITLE, INK_WEIGHT_BOLD, w) + INK_GAP_S;
    bool same = a.year == b.year && a.month == b.month && a.day == b.day;
    if (e.all_day) snprintf(K.texts[1], sizeof K.texts[1], same ? TR("%d月%d日 %s 全天") : TR("%d月%d日 %s 起，全天\n到 %d月%d日"), a.month, a.day, cal_weekday_name(a.wday), b.month, b.day);
    else if (!e.duration_s) snprintf(K.texts[1], sizeof K.texts[1], TR("%d月%d日 %s %02d:%02d"), a.month, a.day, cal_weekday_name(a.wday), a.hour, a.min);
    else if (same) snprintf(K.texts[1], sizeof K.texts[1], TR("%d月%d日 %s %02d:%02d–%02d:%02d"), a.month, a.day, cal_weekday_name(a.wday), a.hour, a.min, b.hour, b.min);
    else snprintf(K.texts[1], sizeof K.texts[1], TR("%d月%d日 %02d:%02d 起\n到 %d月%d日 %02d:%02d"), a.month, a.day, a.hour, a.min, b.month, b.day, b.hour, b.min);
    E.e = e; leads_text(K.texts[3], sizeof K.texts[3]);
    if (e.freq) cal_rule_text(&e, K.texts[2], sizeof K.texts[2]); else snprintf(K.texts[2], sizeof K.texts[2], "%s", TR("不重复"));
    snprintf(K.texts[4], sizeof K.texts[4], "%s", e.parent ? TR("已改这一次") : "");
    const char *labels[] = {TR("时间"), TR("重复"), TR("提醒"), TR("改过")}, *ids[] = {"time", "repeat", "reminders", "edited"};
    cal_field(0, "title", "heading", TR("标题"), K.texts[0]);
    for (int i = 1; i <= 4; i++) {
        if (!K.texts[i][0]) continue;
        char line[224]; snprintf(line, sizeof line, "%s：%s", labels[i - 1], K.texts[i]);
        cal_fmt(K.texts[4 + i], sizeof K.texts[4 + i], "%s", line);
        ink_ui_label(screen, K.texts[4 + i], INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_MARGIN, y, w);
        y += ink_ui_text_height(K.texts[4 + i], INK_FONT_NOTE, INK_WEIGHT_REGULAR, w) + INK_GAP_XS;
        cal_field(i, ids[i - 1], "text", labels[i - 1], K.texts[i]);
    }
    ink_tm_t o; ink_time_to_local(start, &o);
    char args[96]; snprintf(args, sizeof args, "{\"id\":%u,\"occurrence\":\"%04d-%02d-%02d %02d:%02d\"}", (unsigned)e.id, o.year, o.month, o.day, o.hour, o.min);
    int half = (w - INK_GAP) / 2, bottom = INK_CANVAS_H - INK_MARGIN - INK_PRIMARY;
    cal_pill(screen, TR("删除"), INK_MARGIN, bottom, half, false, "calendar.delete", "{}", 1);
    cal_pill(screen, TR("编辑"), INK_MARGIN + half + INK_GAP, bottom, half, true, "reminder.open", args, 2);
    E.occurrence = start;
}
const ink_activity_t CAL_INFO = {.name = "EventInfo", .app = "upcoming", .create = info_create, .snapshot = cal_snapshot};

/* ------------------------------------------------------------ the editor */

static void editor_name(lv_event_t *event) {
    const char *text = ink_textfield_text(lv_event_get_current_target(event));
    if (!text || !strcmp(text, E.e.title)) return;
    snprintf(E.e.title, sizeof E.e.title, "%s", text);
    E.dirty = true;
}
static void focus_changed(void *ctx, int handle, const char *id) { /* the keyboard up: only the title and the save row stay */
    (void)ctx; (void)handle; (void)id;
    if (!E.name || !footer || strcmp(ink_ui_page_name(), "ReminderEdit")) return;
    bool focused = ink_text_focused();
    lv_obj_set_flag(body, LV_OBJ_FLAG_HIDDEN, focused);
    lv_obj_update_layout(footer);
    lv_area_t keyboard; ink_shell_keyboard_area(&keyboard);
    lv_obj_set_y(footer, (focused ? keyboard.y1 - ink_ui_px(INK_GAP) : ink_ui_height() - ink_ui_px(INK_MARGIN)) - lv_obj_get_height(footer));
    lv_obj_set_y(E.name, ink_ui_px(focused ? INK_STATUS_BAR : TOP)); /* the title field up by the bar, clear of the message */
}

static void edit_create(lv_obj_t *screen, void *arg) {
    (void)arg;
    cal_begin(screen, E.e.id ? TR("修改日程") : TR("加一件"));
    int w = INK_CANVAS_W - 2 * INK_MARGIN, y = TOP;
    lv_obj_t *back = ink_ui_box(screen, INK_MARGIN, INK_STATUS_BAR + INK_GAP_S, INK_CONTROL, INK_CONTROL, 0, INK_SURFACE_WHITE, false);
    lv_obj_set_style_bg_opa(back, LV_OPA_TRANSP, 0);
    ink_shell_bind(back, "calendar.edit_back", "{}"); /* E14: leaving asks when something is not saved */
    cal_action(0, "back", TR("返回"), "calendar.edit_back", "{}");
    int tw = INK_CANVAS_W / 3;
    ink_shell_bind(ink_ui_toggle(screen, TR("全天"), E.e.all_day, INK_CANVAS_W - INK_MARGIN - tw, INK_STATUS_BAR + INK_GAP_S, tw, NULL, NULL), "reminder.draft", E.e.all_day ? "{\"all_day\":false}" : "{\"all_day\":true}");
    cal_action(1, "all_day", TR("全天"), "reminder.draft", E.e.all_day ? "{\"all_day\":false}" : "{\"all_day\":true}");
    ink_text_cfg_t cfg = {.type = INK_TEXT_PLAIN, .enter = INK_ENTER_DONE, .initial = E.e.title, .hint = TR("标题，最多 30 个字"), .max = 90};
    E.name = ink_textfield_create(screen, &cfg, NULL, NULL, false);
    K.field = E.name;
    lv_obj_set_y(E.name, ink_ui_px(y));
    ink_textfield_style(E.name, INK_FIELD_SINGLE, TR("标题"));
    ink_textfield_bind_short(E.name, &K.page, cal_field(0, "title", "textbox", TR("标题"), E.e.title), cal_action(2, "title", TR("输入标题"), "reminder.focus", "{}"));
    lv_obj_add_event_cb(E.name, editor_name, LV_EVENT_VALUE_CHANGED, NULL);
    y += ink_textfield_height(INK_FIELD_SINGLE) + INK_GAP_S;
    int footer_h = INK_PRIMARY + (E.error[0] ? ink_ui_text_height(E.error, INK_FONT_NOTE, INK_WEIGHT_REGULAR, w) + INK_GAP_L : 0);
    body = ink_ui_box(screen, 0, y, INK_CANVAS_W, INK_CANVAS_H - y - INK_MARGIN - footer_h, 0, INK_SURFACE_WHITE, false);
    when_text(&E.start, E.e.all_day, K.texts[1], sizeof K.texts[1]);
    end_text(K.texts[2], sizeof K.texts[2]);
    leads_text(K.texts[3], sizeof K.texts[3]);
    draft_to_event(); cal_rule_text(&E.e, K.texts[4], sizeof K.texts[4]);
    const char *labels[] = {TR("开始"), TR("结束"), TR("提醒"), TR("重复")}, *caps[] = {"reminder.time", "reminder.time", "calendar.reminders", "calendar.repeat"};
    const char *args[] = {"{}", "{\"end\":true}", "{}", "{}"}, *ids[] = {"start", "end", "reminders", "repeat"};
    for (int i = 0, ry = 0; i < 4; i++, ry += ink_ui_row_double()) {
        static const char *const ICONS[] = {"calendar", "clock", "bell", "rotate"};
        lv_obj_t *row = ink_ui_list_row(body, ICONS[i], labels[i], K.texts[1 + i], INK_MARGIN, ry, w, true, NULL, NULL);
        row_values[i] = lv_obj_get_child(row, (int32_t)lv_obj_get_child_count(row) - 1);
        ink_shell_bind(row, caps[i], args[i]);
        cal_action(3 + i, ids[i], labels[i], caps[i], args[i]);
        cal_field(1 + i, ids[i], "text", labels[i], K.texts[1 + i]);
        if (i) ink_ui_group_divider(body, ry, w);
    }
    footer = ink_ui_box(screen, 0, INK_CANVAS_H - INK_MARGIN - footer_h, INK_CANVAS_W, footer_h, 0, INK_SURFACE_WHITE, false);
    snprintf(K.texts[5], sizeof K.texts[5], "%s", E.error);
    if (E.error[0]) ink_ui_label(footer, K.texts[5], INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_MARGIN, 0, w);
    cal_field(5, "error", "status", TR("提示"), K.texts[5]);
    int half = (w - INK_GAP) / 2, fy = footer_h - INK_PRIMARY;
    if (E.e.id) cal_pill(footer, TR("删除"), INK_MARGIN, fy, half, false, "calendar.delete", "{}", 7);
    cal_pill(footer, TR("保存"), INK_MARGIN + (E.e.id ? half + INK_GAP : 0), fy, E.e.id ? half : w, true, "reminder.save", "{}", 8);
}
static bool edit_key(ink_key_t key, bool lng) {
    (void)lng;
    if (key != INK_KEY_BACK) return false;
    ink_receipt_t rc;
    ink_cap_invoke("calendar.edit_back", NULL, INK_SRC_TOUCH, 0, &rc);
    return true;
}
static void edit_destroy(void) { E.name = NULL; footer = body = NULL; memset(row_values, 0, sizeof row_values); }
const ink_activity_t CAL_EDIT = {.name = "ReminderEdit", .app = "upcoming", .create = edit_create, .destroy = edit_destroy, .snapshot = cal_snapshot, .on_key = edit_key};

/* ------------------------------------------------------------ the shared picker, one row redrawn at a time (E5, §6) */

static ink_tm_t *picked_tm(void) { return E.part_target == 1 ? &E.end : E.part_target == 0 ? &E.start : &E.pick; }
static bool date_only(void) { return E.part_target >= 2 || E.e.all_day; }
static void bind_part(lv_obj_t *b, int part, int delta) { /* the line before the − button is the value (memo.c:647-662) */
    if (delta < 0) E.part_labels[part] = lv_obj_get_child(lv_obj_get_parent(b), (int32_t)lv_obj_get_index(b) - 1);
    char args[48]; snprintf(args, sizeof args, "{\"step\":\"%s\",\"delta\":%d}", PARTS[part], delta);
    ink_shell_bind(b, "reminder.draft", args);
    if (!date_only() || part < 3) cal_action(2 + part * 2 + (delta > 0), PARTS[part], delta > 0 ? TR("增加") : TR("减少"), "reminder.draft", args);
}
static void part_texts(void) {
    static const char *const NAMES[] = {TR_NOOP("年"), TR_NOOP("月"), TR_NOOP("日"), TR_NOOP("时"), TR_NOOP("分")};
    ink_tm_t *t = picked_tm();
    int v[] = {t->year, t->month, t->day, t->hour, t->min};
    for (int i = 0; i < INK_UI_DATETIME_PARTS; i++) {
        snprintf(K.texts[1 + i], sizeof K.texts[1 + i], "%s %02d", TR(NAMES[i]), v[i]);
        if (E.part_labels[i] && strcmp(lv_label_get_text(E.part_labels[i]), K.texts[1 + i])) lv_label_set_text(E.part_labels[i], K.texts[1 + i]);
    }
    when_text(t, date_only(), K.texts[6], sizeof K.texts[6]); /* the whole value, for the AI */
}
static void time_create(lv_obj_t *screen, void *arg) {
    (void)arg;
    static const char *const TITLES[] = {TR_NOOP("开始时间"), TR_NOOP("结束时间"), TR_NOOP("到期日"), TR_NOOP("跳到日期")};
    cal_begin(screen, TR(TITLES[E.part_target % 4]));
    memset(E.part_labels, 0, sizeof E.part_labels);
    lv_obj_t *box = ink_ui_box(screen, 0, 0, INK_CANVAS_W, INK_CANVAS_H, 0, INK_SURFACE_WHITE, false);
    lv_obj_set_style_bg_opa(box, LV_OPA_TRANSP, 0);
    lv_obj_remove_flag(box, LV_OBJ_FLAG_CLICKABLE);
    ink_tm_t *t = picked_tm();
    int v[] = {t->year, t->month, t->day, t->hour, t->min};
    ink_ui_datetime_picker(box, v, bind_part);
    for (int i = 3; date_only() && i < INK_UI_DATETIME_PARTS; i++) /* a date only: the hour and minute rows are not there */
        for (int k = 0; k < 3 && E.part_labels[i]; k++) lv_obj_add_flag(lv_obj_get_child(box, (int32_t)lv_obj_get_index(E.part_labels[i]) + k), LV_OBJ_FLAG_HIDDEN);
    part_texts();
    cal_field(0, "value", "text", TR("选的时间"), K.texts[6]);
    for (int i = 0; i < (date_only() ? 3 : 5); i++) cal_field(i, PARTS[i], "text", PARTS[i], K.texts[1 + i]);
    int w = INK_CANVAS_W - 2 * INK_MARGIN, half = (w - INK_GAP) / 2, bottom = INK_CANVAS_H - INK_MARGIN - INK_CONTROL;
    snprintf(K.texts[7], sizeof K.texts[7], "{\"date\":\"%04d-%02d-%02d\"}", t->year, t->month, t->day);
    if (E.part_target == 1) cal_pill(screen, TR("不要结束时间"), INK_MARGIN, bottom, half, false, "reminder.draft", "{\"clear_end\":true}", 12);
    cal_pill(screen, TR("选好了"), INK_MARGIN + (E.part_target == 1 ? half + INK_GAP : 0), bottom, E.part_target == 1 ? half : w, true,
             E.part_target == 3 ? "calendar.goto" : "shell.back", E.part_target == 3 ? K.texts[7] : "{}", 13);
}
static void time_destroy(void) { memset(E.part_labels, 0, sizeof E.part_labels); }
const ink_activity_t CAL_TIME = {.name = "ReminderTime", .app = "upcoming", .create = time_create, .destroy = time_destroy, .snapshot = cal_snapshot};

/* ------------------------------------------------------------ repeats, reminders, scope, leaving: pages of rows */

static const char *row_icon = "next";
static void row(const char *label, const char *cap, const char *args, const char *value, bool on) {
    if (K.nrows >= CAL_ROWS) return;
    cal_row_t *r = &K.rows[K.nrows++];
    *r = (cal_row_t){.label = label, .cap = cap, .on = on, .icon = row_icon};
    snprintf(r->args, sizeof r->args, "%s", args);
    snprintf(r->value, sizeof r->value, "%s", value ? value : "");
}
static void stepper(lv_obj_t *screen, int y, const char *step, const char *text, int action) { /* − value +, the value redrawn alone */
    int w = INK_CANVAS_W - 2 * INK_MARGIN;
    char args[48];
    snprintf(args, sizeof args, "{\"step\":\"%s\",\"delta\":-1}", step); cal_pill(screen, "−", INK_MARGIN, y, INK_CONTROL, false, "reminder.draft", args, action);
    snprintf(args, sizeof args, "{\"step\":\"%s\",\"delta\":1}", step); cal_pill(screen, "+", INK_CANVAS_W - INK_MARGIN - INK_CONTROL, y, INK_CONTROL, false, "reminder.draft", args, action + 1);
    snprintf(E.step_value, sizeof E.step_value, "%s", text);
    E.step_label = ink_ui_line(screen, E.step_value, INK_FONT_LIST, INK_WEIGHT_MEDIUM, INK_MARGIN + INK_CONTROL + INK_GAP_M, y + (INK_CONTROL - ink_ui_line_height(INK_FONT_LIST)) / 2, w - 2 * (INK_CONTROL + INK_GAP_M));
    lv_obj_set_style_text_align(E.step_label, LV_TEXT_ALIGN_CENTER, 0);
    cal_field(CAL_FIELDS - 2, step, "text", step, E.step_value);
}
static void interval_text(char *out, size_t cap) {
    static const char *const UNIT[] = {"", TR_NOOP("天"), TR_NOOP("周"), TR_NOOP("个月"), TR_NOOP("年"), TR_NOOP("年")};
    snprintf(out, cap, TR("每 %d %s"), E.e.interval ? E.e.interval : 1, TR(UNIT[E.e.freq % 6]));
}
static void choices_create(lv_obj_t *screen, void *arg) {
    (void)arg;
    const char *name = ink_ui_page_name();
    char value[64], args[64], note[96] = "";
    K.nrows = 0; E.step_label = NULL;
    draft_to_event();
    row_icon = !strcmp(name, "Reminders") || !strcmp(name, "ReminderAdd") ? "bell" : !strcmp(name, "Scope") ? (E.deleting ? "trash" : "edit") : !strcmp(name, "Leave") ? "memo" : "rotate";
    if (!strcmp(name, "RepeatRule")) { /* R1: the frequencies, with 农历每年 and 工作日 */
        cal_begin(screen, TR("重复"));
        for (int i = 0; i < 7; i++) { snprintf(args, sizeof args, "{\"repeat\":%d}", i); row(TR(REPEATS[i]), "reminder.draft", args, NULL, repeat_index(&E.e) == i); }
    } else if (!strcmp(name, "RepeatDetail")) { /* R2-R5, R8 */
        cal_begin(screen, TR("重复细项"));
        interval_text(value, sizeof value);
        int y = TOP;
        if (E.e.freq != INK_FREQ_LUNAR_YEARLY) { stepper(screen, y, "interval", value, 14); y += INK_CONTROL + INK_GAP_S; }
        if (E.e.freq == INK_FREQ_WEEKLY) { /* seven written toggles of 108, in the week's order */
            int start = ink_setting_get("calendar.week_start") ? 0 : 1, dw = cal_pill_w(cal_weekday_name(3));
            for (int i = 0; i < 7; i++) {
                int wd = (i + start) % 7, x = INK_MARGIN + (i % 4) * (dw + INK_GAP_XS);
                snprintf(args, sizeof args, "{\"weekday\":%d}", wd);
                cal_pill(screen, cal_weekday_name(wd), x, y + i / 4 * (INK_CONTROL + INK_GAP_XS), dw, (E.e.weekdays >> wd) & 1, "reminder.draft", args, 16 + i);
            }
            snprintf(K.texts[6], sizeof K.texts[6], "%u", E.e.weekdays);
            cal_field(CAL_FIELDS - 3, "weekdays", "text", TR("每周哪几天（位 0 周日）"), K.texts[6]);
            y += 2 * (INK_CONTROL + INK_GAP_XS);
        } else if (E.e.freq == INK_FREQ_MONTHLY || E.e.freq == INK_FREQ_YEARLY) { /* only the ones that apply (EventActivity.kt:1012-1016) */
            int32_t day = cal_day(E.start.year, E.start.month, E.start.day);
            int nth = (E.start.day - 1) / 7 + 1, last_week = E.start.day + 7 > cal_month_days(E.start.year, E.start.month);
            snprintf(value, sizeof value, TR("%d 日"), E.start.day); row(value, "reminder.draft", "{\"monthly\":0}", NULL, E.e.by == INK_BY_DAY);
            if (nth <= 4) { snprintf(value, sizeof value, TR("第 %d 个%s"), nth, cal_weekday_name(cal_wday(day))); row(value, "reminder.draft", "{\"monthly\":1}", NULL, E.e.by == INK_BY_NTH_WEEKDAY); }
            if (last_week) { snprintf(value, sizeof value, TR("最后一个%s"), cal_weekday_name(cal_wday(day))); row(value, "reminder.draft", "{\"monthly\":2}", NULL, E.e.by == INK_BY_LAST_WEEKDAY); }
            if (E.start.day == cal_month_days(E.start.year, E.start.month) && E.e.freq == INK_FREQ_MONTHLY) row(TR("最后一天"), "reminder.draft", "{\"monthly\":3}", NULL, E.e.by == INK_BY_LAST_DAY);
        } else if (E.e.freq == INK_FREQ_LUNAR_YEARLY) { cal_rule_text(&E.e, note, sizeof note); }
        cal_rule_text(&E.e, value, sizeof value);
        row(TR("结束"), "calendar.repeat_end", "{}", E.e.until_year || E.e.count ? value : TR("一直重复"), false);
        K.rows[K.nrows] = (cal_row_t){.label = TR("好"), .cap = "shell.back", .primary = true}; snprintf(K.rows[K.nrows++].args, sizeof K.rows[0].args, "{}");
        cal_choices(screen, NULL, note, y);
        return;
    } else if (!strcmp(name, "RepeatEnd")) { /* R7 */
        cal_begin(screen, TR("重复到什么时候"));
        snprintf(value, sizeof value, TR("%d月%d日"), E.e.until_month, E.e.until_day);
        row(TR("一直重复"), "reminder.draft", "{\"end_kind\":0}", NULL, !E.e.until_year && !E.e.count);
        row(TR("到某天为止"), "reminder.time", "{\"until\":true}", E.e.until_year ? value : TR("选日期"), E.e.until_year != 0);
        row(TR("共几次"), "reminder.draft", "{\"end_kind\":2}", NULL, E.e.count != 0);
        K.rows[K.nrows] = (cal_row_t){.label = TR("好"), .cap = "shell.back", .primary = true}; snprintf(K.rows[K.nrows++].args, sizeof K.rows[0].args, "{}");
        int y = TOP + 3 * INK_CONTROL + INK_GAP;
        if (E.e.count) { snprintf(value, sizeof value, TR("共 %d 次"), E.e.count); stepper(screen, y, "count", value, 14); }
        cal_choices(screen, NULL, NULL, TOP);
        return;
    } else if (!strcmp(name, "Reminders")) { /* E8 */
        cal_begin(screen, TR("提醒"));
        for (int i = 0; i < E.e.nleads; i++) {
            cal_lead_text(E.e.leads[i], E.e.all_day, value, sizeof value);
            snprintf(args, sizeof args, "{\"remove_lead\":%d}", i);
            snprintf(K.texts[8 + i % 3], sizeof K.texts[8], "%s", value);
            row(K.texts[8 + i % 3], "reminder.draft", args, TR("点一下删掉"), false);
        }
        if (E.e.nleads < INK_EVENT_LEADS) row(TR("加一个提醒"), "calendar.reminder_add", "{}", NULL, false);
        if (!E.e.nleads) snprintf(note, sizeof note, "%s", TR("没有提醒：到时候不响，只在日历里"));
        K.rows[K.nrows] = (cal_row_t){.label = TR("好"), .cap = "shell.back", .primary = true}; snprintf(K.rows[K.nrows++].args, sizeof K.rows[0].args, "{}");
    } else if (!strcmp(name, "ReminderAdd")) { /* E9: Commons' choices for this device, and one made with − and + */
        cal_begin(screen, TR("加一个提醒"));
        static const int TIMED[] = {0, 5, 10, 30, 60, 1440}, ALLDAY[] = {-540, 0, 1440, 2880, 10080};
        const int *v = E.e.all_day ? ALLDAY : TIMED;
        int n = E.e.all_day ? 5 : 6;
        for (int i = 0; i < n; i++) { cal_lead_text(v[i], E.e.all_day, K.texts[6 + i], sizeof K.texts[6]); snprintf(args, sizeof args, "{\"lead\":%d}", v[i]); row(K.texts[6 + i], "reminder.draft", args, NULL, false); }
        snprintf(value, sizeof value, TR("单位：%s（点一下换）"), TR(UNITS[E.custom_unit % 4]));
        snprintf(args, sizeof args, "{\"custom\":%d}", (E.custom_unit + 1) % (E.e.all_day ? 4 : 3));
        row(TR("自定义"), "reminder.draft", args, value, false);
        int y = TOP + n * INK_CONTROL + ink_ui_row_double() + INK_GAP_S;
        snprintf(value, sizeof value, "%d %s", E.custom, TR(UNITS[E.custom_unit % 4]));
        stepper(screen, y, "custom", value, 20);
        K.rows[K.nrows] = (cal_row_t){.label = TR("加上这个"), .cap = "reminder.draft", .primary = true}; snprintf(K.rows[K.nrows++].args, sizeof K.rows[0].args, "{\"custom\":-1}");
    } else if (!strcmp(name, "Scope")) { /* C1, C5 */
        cal_begin(screen, E.deleting ? TR("删掉哪几次") : TR("改哪几次"));
        static const char *const SAVE[] = {TR_NOOP("只改这一次"), TR_NOOP("这次和以后"), TR_NOOP("全部")}, *DEL[] = {TR_NOOP("只删这一次"), TR_NOOP("删掉这次和以后"), TR_NOOP("全部删掉")}, *SC[] = {"this", "following", "all"};
        for (int i = 0; i < 3; i++) { snprintf(args, sizeof args, "{\"scope\":\"%s\"}", SC[i]); row(TR((E.deleting ? DEL : SAVE)[i]), E.deleting ? "calendar.delete" : "reminder.save", args, NULL, false); }
        snprintf(note, sizeof note, "%s", TR("这是一组重复的日程"));
    } else { /* Leave (E14) */
        cal_begin(screen, TR("改动还没保存"));
        row(TR("保存"), "reminder.save", "{}", NULL, false);
        row(TR("不保存"), "calendar.edit_discard", "{}", NULL, false);
    }
    cal_choices(screen, NULL, note, TOP);
}
static void choices_destroy(void) { E.step_label = NULL; }
#define CHOICES(var, name_) const ink_activity_t var = {.name = name_, .app = "upcoming", .create = choices_create, .destroy = choices_destroy, .snapshot = cal_snapshot}
CHOICES(CAL_REPEAT, "RepeatRule"); CHOICES(CAL_DETAIL, "RepeatDetail"); CHOICES(CAL_END, "RepeatEnd"); CHOICES(CAL_REMINDERS, "Reminders");
CHOICES(CAL_REMINDER_ADD, "ReminderAdd"); CHOICES(CAL_SCOPE, "Scope"); CHOICES(CAL_LEAVE, "Leave");

/* ------------------------------------------------------------ what the editor's buttons and the AI do */

void cal_edit_open(const ink_event_t *e, int64_t occurrence, int y, int m, int d) {
    memset(&E, 0, sizeof E);
    snprintf(E.owner, sizeof E.owner, "%s", ink_member_current()->id);
    E.custom = 15;
    if (e) {
        E.e = *e;
        int64_t start = occurrence ? ink_event_occurrence(e, occurrence) : 0;
        if (!start) start = ink_event_occurrence(e, 0);
        E.occurrence = e->freq ? start : 0;
        E.series = e->freq != INK_FREQ_NONE;
        if (start) ink_time_to_local(start, &E.start);
        else E.start = (ink_tm_t){.year = e->year, .month = e->month, .day = e->day, .hour = e->hour, .min = e->min};
        E.has_end = e->duration_s > 0;
        ink_time_to_local((start ? start : ink_time_from_local(&E.start)) + (e->duration_s > 0 ? e->duration_s - (e->all_day ? 86400 : 0) : 3600), &E.end);
        return;
    }
    ink_tm_t now; ink_time_to_local(ink_time_now_s(), &now); /* E17: the chosen day at the next whole hour, an hour long */
    E.start = (ink_tm_t){.year = y, .month = m, .day = d, .hour = now.hour + 1 > 23 ? 23 : now.hour + 1};
    E.has_end = true;
    ink_time_to_local(ink_time_from_local(&E.start) + 3600, &E.end);
    E.e.interval = 1;
    static const int LEADS[] = {-1, 0, 5, 10, 30, 60, 1440};
    int lead = LEADS[ink_setting_get("calendar.default_reminder") % 7];
    if (lead >= 0) { E.e.nleads = 1; E.e.leads[0] = lead; }
}

static int64_t local_of(const char *text) {
    ink_tm_t t = {0};
    if (sscanf(text, "%d-%d-%d %d:%d", &t.year, &t.month, &t.day, &t.hour, &t.min) < 3) return 0;
    return ink_time_from_local(&t);
}

/* The service's words to the field that is wrong (E2, E6, E16, R7). */
static void say_error(const ink_receipt_t *rc) {
    const char *d = rc->detail;
    snprintf(E.error, sizeof E.error, "%s", strstr(d, "title") || strstr(d, "at:") ? TR("标题不能空") : strstr(d, "end must") ? TR("结束要晚于开始") :
             strstr(d, "until") ? TR("到期日要晚于开始") : strstr(d, "full") ? TR("日程已满（最多 128 条）") : strstr(d, "occurrences already") ? TR("这组重复的例外已满 16 个，请改成“这次和以后”") :
             strstr(d, "passed") ? TR("这个时间已经过了") : TR("存储写不进，请稍后再试"));
}

static void show_error(void) { /* back on the editor, the message by the field */
    if (!strcmp(ink_ui_page_name(), "ReminderEdit")) { ink_ui_redraw(); return; }
    while (editor_page() && strcmp(ink_ui_page_name(), "ReminderEdit") && ink_ui_depth() > 1) ink_ui_pop();
}
static void save(ink_call_t *call, const char *scope) {
    if (!E.e.title[0]) { snprintf(E.error, sizeof E.error, "%s", TR("标题不能空")); show_error(); call->rc.status = INK_RC_INVALID; return; }
    if (E.series && !scope) { E.deleting = false; ink_ui_push(&CAL_SCOPE, NULL); return; }
    ink_args_t *a = osal_calloc(1, sizeof *a, OSAL_MEM_EXTERNAL);
    if (!a) { call->rc.status = INK_RC_FAILED; return; }
    char at[24], end[24], leads[48] = "[", rule[160];
    draft_to_event();
    when_text(&E.start, E.e.all_day, at, sizeof at);
    ink_tm_t stop = E.end;
    if (E.e.all_day) { stop.hour = stop.min = 0; ink_time_to_local(ink_time_from_local(&stop) + 86400 + 3600, &stop); stop.hour = stop.min = 0; }
    when_text(&stop, false, end, sizeof end);
    for (int i = 0; i < E.e.nleads; i++) snprintf(leads + strlen(leads), sizeof leads - strlen(leads), "%s%d", i ? "," : "", (int)E.e.leads[i]);
    strncat(leads, "]", sizeof leads - strlen(leads) - 1);
    static const char *const FREQ[] = {"none", "daily", "weekly", "monthly", "yearly", "lunar_yearly"}, *BY[] = {"day", "weekday", "last_weekday", "last_day"};
    snprintf(rule, sizeof rule, "{\"freq\":\"%s\",\"interval\":%d,\"weekdays\":%d,\"monthly\":\"%s\",\"count\":%d,\"until\":\"", FREQ[E.e.freq % 6], E.e.interval ? E.e.interval : 1,
             E.e.weekdays ? E.e.weekdays : 1 << cal_wday(cal_day(E.start.year, E.start.month, E.start.day)), BY[E.e.by % 4], E.e.count);
    if (E.e.until_year) snprintf(rule + strlen(rule), sizeof rule - strlen(rule), "%04d-%02d-%02d", E.e.until_year, E.e.until_month, E.e.until_day);
    strncat(rule, "\"}", sizeof rule - strlen(rule) - 1);
    ink_args_init(a);
    ink_args_str(a, "at", at); ink_args_str(a, "title", E.e.title); ink_args_str(a, "end", E.has_end || E.e.all_day ? end : "");
    ink_args_json(a, "reminders", leads, strlen(leads)); ink_args_json(a, "rule", rule, strlen(rule));
    if (E.e.id) {
        ink_args_int(a, "id", E.e.id);
        if (scope) {
            ink_tm_t o; ink_time_to_local(E.occurrence, &o);
            char occ[24]; snprintf(occ, sizeof occ, "%04d-%02d-%02d %02d:%02d", o.year, o.month, o.day, o.hour, o.min);
            ink_args_str(a, "scope", scope); ink_args_str(a, "occurrence", occ);
        }
    }
    ink_cap_invoke(E.e.id ? "reminder.update" : "reminder.add", a, call->src, 0, &call->rc);
    osal_free(a);
    if (call->rc.status == INK_RC_DONE) { E.dirty = false; leave_editor(); return; }
    say_error(&call->rc);
    show_error();
}

static void delete_now(ink_call_t *call, const char *scope) {
    if (E.series && !scope) { E.deleting = true; ink_ui_push(&CAL_SCOPE, NULL); return; }
    char args[160];
    ink_tm_t o; ink_time_to_local(E.occurrence, &o);
    snprintf(args, sizeof args, "{\"id\":%u,\"scope\":\"%s\",\"occurrence\":\"%04d-%02d-%02d %02d:%02d\"}", (unsigned)E.e.id, scope ? scope : "all", o.year, o.month, o.day, o.hour, o.min);
    ink_cap_invoke_json("reminder.cancel", args, call->src, 0, &call->rc);
    if (call->rc.status != INK_RC_DONE) { say_error(&call->rc); return; }
    if (call->rc.undo_ms) { /* C6: a written 撤销 stays on the page below until the next thing done */
        snprintf(K.undo, sizeof K.undo, "{\"object\":\"%s\",\"revision\":%u}", call->rc.object, (unsigned)call->rc.revision);
        K.undo_valid = true;
    }
    leave_editor();
}

static void step(const char *what, int delta) {
    ink_tm_t *t = picked_tm();
    int *parts[] = {&t->year, &t->month, &t->day, &t->hour, &t->min}, lo[] = {1901, 1, 1, 0, 0}, hi[] = {2100, 12, cal_month_days(t->year, t->month), 23, 59};
    for (int i = 0; i < 5; i++) if (!strcmp(what, PARTS[i])) { *parts[i] += delta; if (*parts[i] < lo[i]) *parts[i] = hi[i]; if (*parts[i] > hi[i]) *parts[i] = lo[i]; }
    if (!strcmp(what, "interval")) E.e.interval = (uint8_t)((E.e.interval ? E.e.interval : 1) + delta < 1 ? 99 : (E.e.interval ? E.e.interval : 1) + delta > 99 ? 1 : (E.e.interval ? E.e.interval : 1) + delta);
    if (!strcmp(what, "count")) E.e.count = (uint16_t)(E.e.count + delta < 1 ? 730 : E.e.count + delta > 730 ? 1 : E.e.count + delta);
    if (!strcmp(what, "custom")) E.custom = E.custom + delta < 1 ? (E.custom_unit == 3 ? 23 : 99) : E.custom + delta;
    if (E.custom_unit == 3 && E.custom > 23) E.custom = 0;
}

void cal_edit_cap(ink_call_t *call) {
    const char *id = call->cap->id;
    int64_t v = 0;
    char text[32] = "";
    K.undo_valid = false;
    if (!strcmp(id, "calendar.event") || !strcmp(id, "reminder.open") || !strcmp(id, "reminder.new")) {
        ink_event_t e;
        ink_args_get_str(&call->args, "occurrence", text, sizeof text);
        if (!ink_time_valid()) { call->rc.status = INK_RC_STALE; return; }
        if (!strcmp(id, "reminder.new")) {
            int y = K.y, m = K.m, d = K.d;
            char date[16] = "";
            if (!y) { ink_tm_t t; ink_time_to_local(ink_time_now_s() + 86400, &t); y = t.year; m = t.month; d = t.day; }
            if (ink_args_get_str(&call->args, "date", date, sizeof date) > 0 && sscanf(date, "%d-%d-%d", &y, &m, &d) != 3) { call->rc.status = INK_RC_INVALID; return; }
            cal_edit_open(NULL, 0, y, m, d);
            ink_ui_push(&CAL_EDIT, NULL); return;
        }
        ink_args_get_int(&call->args, "id", &v);
        if (ink_event_get((uint32_t)v, &e)) { call->rc.status = INK_RC_STALE; return; }
        cal_edit_open(&e, local_of(text), 0, 0, 0);
        ink_ui_push(!strcmp(id, "calendar.event") ? &CAL_INFO : &CAL_EDIT, NULL); return;
    }
    if (strcmp(E.owner, ink_member_current()->id) || !editor_page()) { call->rc.status = INK_RC_STALE; return; }
    if (!strcmp(id, "calendar.delete")) { ink_args_get_str(&call->args, "scope", text, sizeof text); delete_now(call, text[0] ? text : NULL); return; }
    if (!strcmp(id, "reminder.save")) { ink_args_get_str(&call->args, "scope", text, sizeof text); save(call, text[0] ? text : NULL); return; }
    if (!strcmp(id, "calendar.edit_back")) { if (E.dirty && !strcmp(ink_ui_page_name(), "ReminderEdit")) ink_ui_push(&CAL_LEAVE, NULL); else ink_ui_pop(); return; }
    if (!strcmp(id, "calendar.edit_discard")) { E.dirty = false; leave_editor(); return; }
    if (!strcmp(id, "reminder.focus")) { if (!E.name || ink_textfield_short_focus(E.name)) call->rc.status = INK_RC_STALE; return; }
    const struct { const char *cap; const ink_activity_t *page; } OPEN[] = {{"calendar.repeat", &CAL_REPEAT}, {"calendar.repeat_detail", &CAL_DETAIL}, {"calendar.repeat_end", &CAL_END},
                                                                        {"calendar.reminders", &CAL_REMINDERS}, {"calendar.reminder_add", &CAL_REMINDER_ADD}};
    for (unsigned i = 0; i < sizeof OPEN / sizeof *OPEN; i++) if (!strcmp(id, OPEN[i].cap)) { draft_to_event(); ink_ui_push(OPEN[i].page, NULL); return; }
    if (!strcmp(id, "reminder.time")) {
        bool end = false, until = false;
        ink_args_get_bool(&call->args, "end", &end); ink_args_get_bool(&call->args, "until", &until);
        E.part_target = until ? 2 : end ? 1 : 0;
        if (end) E.has_end = true;
        if (until) { if (!E.e.until_year) { E.pick = E.start; E.pick.month += 1; if (E.pick.month > 12) { E.pick.month = 1; E.pick.year++; } if (E.pick.day > 28) E.pick.day = 28; } else E.pick = (ink_tm_t){.year = E.e.until_year, .month = E.e.until_month, .day = E.e.until_day}; }
        ink_ui_push(&CAL_TIME, NULL); return;
    }
    /* reminder.draft: the time's parts, absolute or a step, and everything else being edited */
    bool redraw = false, flag;
    const char *page = ink_ui_page_name();
    ink_tm_t *t = !strcmp(page, "ReminderTime") ? picked_tm() : &E.start;
    int *parts[] = {&t->year, &t->month, &t->day, &t->hour, &t->min};
    int64_t start_before = ink_time_from_local(&E.start); /* a moved start takes the end along (Fossify keeps the length) */
    bool day_given = ink_args_get_int(&call->args, "day", &v);
    const int lo[] = {1901, 1, 1, 0, 0}, hi[] = {2100, 12, 31, 23, 59};
    for (int i = 0; i < 5; i++) if (ink_args_get_int(&call->args, PARTS[i], &v)) {
        if (v < lo[i] || v > hi[i]) { call->rc.status = INK_RC_INVALID; return; }
        *parts[i] = (int)v;
    }
    if (t->day > cal_month_days(t->year, t->month)) { if (day_given) { call->rc.status = INK_RC_INVALID; return; } t->day = cal_month_days(t->year, t->month); }
    if (ink_args_get_str(&call->args, "step", text, sizeof text) > 0 && ink_args_get_int(&call->args, "delta", &v)) {
        step(text, (int)v);
        if (t->day > cal_month_days(t->year, t->month)) t->day = cal_month_days(t->year, t->month);
    }
    int64_t moved = ink_time_from_local(&E.start) - start_before;
    if (moved && E.has_end) ink_time_to_local(ink_time_from_local(&E.end) + moved, &E.end);
    if (E.part_target == 2 && !strcmp(page, "ReminderTime")) { E.e.until_year = (int16_t)E.pick.year; E.e.until_month = (uint8_t)E.pick.month; E.e.until_day = (uint8_t)E.pick.day; E.e.count = 0; }
    if (ink_args_get_bool(&call->args, "all_day", &flag)) { /* an all-day end is its last day; a timed end is exclusive */
        if (E.e.all_day && !flag && E.has_end) ink_time_to_local(ink_time_from_local(&E.end) + 86400, &E.end);
        E.e.all_day = flag; redraw = true;
    }
    if (ink_args_get_bool(&call->args, "clear_end", &flag) && flag) { E.has_end = false; if (!strcmp(page, "ReminderTime")) { E.dirty = true; ink_ui_pop(); return; } redraw = true; }
    int64_t check;
    if ((ink_args_get_int(&call->args, "repeat", &check) && (check < 0 || check > 6)) || (ink_args_get_int(&call->args, "weekday", &check) && (check < 0 || check > 6)) ||
        (ink_args_get_int(&call->args, "monthly", &check) && (check < 0 || check > 3)) || (ink_args_get_int(&call->args, "interval", &check) && (check < 1 || check > 99)) ||
        (ink_args_get_int(&call->args, "count", &check) && (check < 1 || check > 730)) || (ink_args_get_int(&call->args, "custom", &check) && (check < -1 || check > 3)) ||
        (ink_args_get_int(&call->args, "lead", &check) && (check < -1439 || check > 40320)) || (ink_args_get_int(&call->args, "delta", &check) && (check < -1 || check > 1))) {
        call->rc.status = INK_RC_INVALID; return;
    }
    if (ink_args_get_int(&call->args, "repeat", &v)) {
        static const uint8_t FREQ[] = {INK_FREQ_NONE, INK_FREQ_DAILY, INK_FREQ_WEEKLY, INK_FREQ_WEEKLY, INK_FREQ_MONTHLY, INK_FREQ_YEARLY, INK_FREQ_LUNAR_YEARLY};
        E.e.freq = FREQ[v % 7]; E.e.weekdays = v == 3 ? 0x3E : v == 2 ? (uint8_t)(1u << cal_wday(cal_day(E.start.year, E.start.month, E.start.day))) : 0;
        E.e.by = INK_BY_DAY; E.e.interval = 1; E.e.lmonth = 0;
        if (E.e.freq == INK_FREQ_LUNAR_YEARLY) { ink_lunar_t l; if (ink_lunar_from_solar(E.start.year, E.start.month, E.start.day, &l)) { E.e.lmonth = (uint8_t)l.month; E.e.lday = (uint8_t)l.day; E.e.lleap = l.leap; } }
        E.dirty = true;
        if (!strcmp(page, "RepeatRule")) { ink_ui_pop(); if (v && v != 3) ink_ui_push(&CAL_DETAIL, NULL); return; }
        redraw = true;
    }
    if (ink_args_get_int(&call->args, "interval", &v)) E.e.interval = (uint8_t)v;
    if (ink_args_get_int(&call->args, "weekday", &v)) { E.e.weekdays ^= (uint8_t)(1u << v); if (!E.e.weekdays) E.e.weekdays = (uint8_t)(1u << v); redraw = true; }
    if (ink_args_get_int(&call->args, "monthly", &v)) { E.e.by = (uint8_t)v; E.e.nth = 0; redraw = true; }
    if (ink_args_get_int(&call->args, "end_kind", &v)) { E.e.until_year = 0; E.e.count = v == 2 ? (E.e.count ? E.e.count : 10) : 0; redraw = true; }
    if (ink_args_get_int(&call->args, "count", &v)) { E.e.count = (uint16_t)v; E.e.until_year = 0; redraw = true; }
    if (ink_args_get_int(&call->args, "remove_lead", &v) && v < E.e.nleads) {
        for (int i = (int)v; i + 1 < E.e.nleads; i++) E.e.leads[i] = E.e.leads[i + 1];
        E.e.nleads--; redraw = true;
    }
    int64_t lead = INT32_MIN;
    if (ink_args_get_int(&call->args, "lead", &v)) lead = v;
    if (ink_args_get_int(&call->args, "custom", &v)) {
        if (v >= 0) { E.custom_unit = (int)v; if (v == 3 && E.custom > 23) E.custom = 9; redraw = true; }
        else lead = E.custom_unit == 3 ? -E.custom * 60 : E.custom * (E.custom_unit == 2 ? 1440 : E.custom_unit == 1 ? 60 : 1);
    }
    if (lead != INT32_MIN) {
        bool dup = false;
        for (int i = 0; i < E.e.nleads; i++) dup |= E.e.leads[i] == lead;
        if (E.e.nleads >= INK_EVENT_LEADS && !dup) { call->rc.status = INK_RC_REJECTED; snprintf(call->rc.detail, sizeof call->rc.detail, "three reminders at most"); return; }
        if (!dup) E.e.leads[E.e.nleads++] = (int32_t)lead;
        E.dirty = true;
        if (!strcmp(page, "ReminderAdd")) { ink_ui_pop(); return; }
        redraw = true;
    }
    E.dirty = true; E.error[0] = 0;
    if (!strcmp(page, "ReminderTime")) { part_texts(); ink_ui_content_changed(); return; } /* one row (or the day too) */
    if (E.step_label && !redraw) { /* the stepper's value alone */
        if (!strcmp(page, "RepeatDetail")) interval_text(E.step_value, sizeof E.step_value);
        else if (!strcmp(page, "RepeatEnd")) snprintf(E.step_value, sizeof E.step_value, TR("共 %d 次"), E.e.count);
        else snprintf(E.step_value, sizeof E.step_value, "%d %s", E.custom, TR(UNITS[E.custom_unit % 4]));
        lv_label_set_text(E.step_label, E.step_value); ink_ui_content_changed(); return;
    }
    if (!strcmp(page, "ReminderEdit") && !redraw && row_values[0]) { /* the AI's new start: its row alone */
        when_text(&E.start, E.e.all_day, K.texts[1], sizeof K.texts[1]);
        lv_label_set_text(row_values[0], K.texts[1]); ink_ui_content_changed(); return;
    }
    ink_ui_redraw();
}

int cal_edit_init(void) { return ink_state_watch("text.focused", focus_changed, NULL, INK_EXEC_UI); }
