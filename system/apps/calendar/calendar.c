/* Calendar views share the time service, F4 lists and the capability bus. */
/* ui-scale-runtime: dimension w,x,y,cell,rows Theme-derived grid and list geometry. */
#include "ink/osal.h"
#include "ink/sys/appreg.h"
#include "ink/sys/json.h"
#include "ink/sys/i18n.h"
#include "ink/sys/members.h"
#include "ink/sys/state.h"
#include "ink/sys/textfield.h"
#include "ink/sys/time.h"
#include "ink/sys/ui_list.h"
#include "ink/ui_shell.h"
#include <stdio.h>
#include <string.h>

enum { VISIBLE_ROWS = 4, EDIT_FIELDS = 4, PICKER_PARTS = INK_UI_DATETIME_PARTS, REPEAT_PRESETS = 4, EDIT_ACTIONS = 12 };
static const ink_activity_t UPCOMING, CALENDAR, REMINDER_EDIT, REMINDER_TIME;
static void reminder_action(ink_call_t *call);
static struct {
    ink_alarm_t rows[64];
    size_t count;
    ink_tm_t date;
    ink_ui_list_t *list;
    ink_ui_list_position_t position;
    ink_ui_page_spec_t controls;
    ink_ui_field_spec_t date_field;
    ink_ui_action_spec_t actions[5];
    char member[16], date_text[96], select_args[32];
    char ids[VISIBLE_ROWS][32], values[VISIBLE_ROWS][128], args[VISIBLE_ROWS][32];
    bool calendar;
} C INK_PSRAM_BSS;
static const ink_ui_action_spec_t PAGING[] = {
    {.id = "prev", .label = TR_NOOP("上一页"), .capability = "calendar.prev"},
    {.id = "next", .label = TR_NOOP("下一页"), .capability = "calendar.next"}
};
static int month_days(int year, int month) {
    static const int DAYS[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    return DAYS[month - 1] + (month == 2 && year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
}
static void lunar_text(int day, bool short_name, char *out, size_t cap) {
    ink_lunar_t lunar; out[0] = 0;
    if (!ink_lunar_from_solar(C.date.year, C.date.month, day, &lunar)) return;
    ink_lunar_name(&lunar, out, cap);
    if (short_name && strcmp(ink_lang_code(), "zh")) { snprintf(out, cap, lunar.day == 1 ? TR("农历%d月") : "%d", lunar.day == 1 ? lunar.month : lunar.day); return; }
    char *p = strstr(out, "月");
    if (short_name && p) { p += strlen("月"); if (lunar.day == 1) *p = 0; else memmove(out, p, strlen(p) + 1); }
}
static void load_rows(void) {
    C.count = 0;
    if (!ink_time_valid()) return;
    ink_tm_t start = C.date; start.hour = start.min = start.sec = 0;
    int64_t from = C.calendar ? ink_time_from_local(&start) : ink_time_now_s();
    start.day++; int64_t until = ink_time_from_local(&start);
    for (int kind = INK_ALARM; kind <= INK_REMINDER; kind++) {
        int cursor = 0; ink_alarm_t a;
        while (ink_alarm_next(&cursor, (ink_alarm_kind_t)kind, &a) == 1) {
            int64_t at = C.calendar ? ink_alarm_occurrence(&a, from) : a.next_s;
            if (!a.enabled || at < from || !at || (C.calendar && at >= until)) continue;
            a.next_s = at; size_t i = C.count;
            if (i == sizeof C.rows / sizeof *C.rows) break;
            while (i && (C.rows[i - 1].next_s > at || (C.rows[i - 1].next_s == at && C.rows[i - 1].id > a.id))) {
                C.rows[i] = C.rows[i - 1]; i--;
            }
            C.rows[i] = a; C.count++;
        }
    }
}
static void bind_row(lv_obj_t *slot, size_t index, ink_ui_field_spec_t *field, ink_ui_action_spec_t *action, void *ctx) {
    (void)ctx; unsigned n = index % VISIBLE_ROWS; ink_alarm_t *a = &C.rows[index]; ink_tm_t t;
    ink_time_to_local(a->next_s, &t);
    snprintf(C.ids[n], sizeof C.ids[n], "%s:%u", a->kind == INK_ALARM ? "alarm" : "reminder", (unsigned)a->id);
    char named[sizeof a->title]; /* a copy: gcc refuses snprintf from C into C */
    memcpy(named, a->title, sizeof named); named[sizeof named - 1] = 0;
    snprintf(C.values[n], sizeof C.values[n], TR("%d月%d日 %02d:%02d %s · %s"), t.month, t.day, t.hour, t.min,
        a->kind == INK_ALARM ? TR("闹钟") : TR("提醒"), named);
    *field = (ink_ui_field_spec_t){.id = C.ids[n], .role = "listitem", .label = a->kind == INK_ALARM ? TR("闹钟") : TR("提醒"), .value = C.values[n], .object = {C.ids[n], 1}};
    snprintf(C.args[n], sizeof C.args[n], "{\"id\":%u}", (unsigned)a->id);
    *action = (ink_ui_action_spec_t){.id = C.ids[n], .label = TR("查看"), .capability = "reminder.open", .args_json = C.args[n], .object = field->object, .enabled = a->kind == INK_REMINDER};
    if (!lv_obj_get_child_count(slot)) {
        ink_ui_line(slot, "", INK_FONT_LIST, INK_WEIGHT_REGULAR, INK_CONTROL, INK_GAP_S, INK_CANVAS_W - 2 * INK_MARGIN - INK_CONTROL - INK_GAP_M);
        if (!C.calendar) ink_ui_line(slot, "", INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_CONTROL, INK_LINE_LIST + INK_GAP_S, INK_CANVAS_W - 2 * INK_MARGIN - INK_CONTROL - INK_GAP_M);
    }
    unsigned labels = C.calendar ? 1 : 2;
    if (lv_obj_get_child_count(slot) > labels) lv_obj_delete(lv_obj_get_child(slot, (int32_t)labels));
    ink_ui_icon(slot, a->kind == INK_ALARM ? "clock" : "list", INK_ICON_LIST, INK_ICON_LINE, INK_GAP_M, INK_GAP_L, false);
    char title[96]; snprintf(title, sizeof title, "%02d:%02d %s", t.hour, t.min, a->title);
    lv_label_set_text(lv_obj_get_child(slot, 0), title);
    if (!C.calendar) { snprintf(title, sizeof title, TR("%d月%d日 · %s"), t.month, t.day, a->kind == INK_ALARM ? TR("闹钟") : TR("提醒")); lv_label_set_text(lv_obj_get_child(slot, 1), title); }
}
static void control(size_t n, const char *id, const char *label, const char *cap, const char *args) {
    C.actions[n] = (ink_ui_action_spec_t){.id = id, .label = label, .capability = cap, .args_json = args, .enabled = true};
}
static void create(lv_obj_t *screen, void *arg) {
    (void)arg; C.calendar = !strcmp(ink_ui_page_name(), "Calendar");
    if (strcmp(C.member, ink_member_current()->id) || !C.date.year) {
        memset(&C.position, 0, sizeof C.position); ink_time_to_local(ink_time_now_s(), &C.date);
        snprintf(C.member, sizeof C.member, "%s", ink_member_current()->id);
    }
    ink_shell_statusbar(screen, true);
    char title[64]; snprintf(title, sizeof title, TR("%d年%d月"), C.date.year, C.date.month);
    ink_shell_topbar(screen, C.calendar ? title : TR("接下来的事"));
    int y = INK_STATUS_BAR + INK_PRIMARY + INK_GAP_XS, w = INK_CANVAS_W - 2 * INK_MARGIN, capacity = 4;
    memset(C.actions, 0, sizeof C.actions);
    if (C.calendar && ink_time_valid()) {
        int x = INK_CANVAS_W - INK_MARGIN - 2 * INK_CONTROL - INK_GAP_XS;
        ink_shell_bind(ink_ui_circle(screen, "back", x, INK_STATUS_BAR + INK_GAP_S, INK_CONTROL, INK_ICON_MEDIUM, INK_SURFACE_WHITE, NULL, NULL), "calendar.month_prev", "{}");
        ink_shell_bind(ink_ui_circle(screen, "next", x + INK_CONTROL + INK_GAP_XS, INK_STATUS_BAR + INK_GAP_S, INK_CONTROL, INK_ICON_MEDIUM, INK_SURFACE_WHITE, NULL, NULL), "calendar.month_next", "{}");
        ink_tm_t first = {.year = C.date.year, .month = C.date.month, .day = 1}; ink_time_to_local(ink_time_from_local(&first), &first);
        int days = month_days(C.date.year, C.date.month), rows = (first.wday + days + 6) / 7, cell = w / 7;
        static const char *const WEEK[] = {TR_NOOP("周日"), TR_NOOP("周一"), TR_NOOP("周二"), TR_NOOP("周三"), TR_NOOP("周四"), TR_NOOP("周五"), TR_NOOP("周六")};
        for (int i = 0; i < 7; i++) {
            const char *day = TR(WEEK[i]) + (!strcmp(ink_lang_code(), "zh") ? strlen("周") : 0);
            ink_ui_line(screen, day, INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_MARGIN + i * cell + INK_GAP_M, y, cell);
        }
        y += INK_LINE_NOTE;
        for (int d = 1; d <= days; d++) {
            int pos = first.wday + d - 1; bool selected = d == C.date.day;
            lv_obj_t *box = ink_ui_box(screen, INK_MARGIN + pos % 7 * cell, y + pos / 7 * INK_CONTROL, cell, INK_CONTROL, INK_RADIUS_KEY, selected ? INK_SURFACE_BLACK : INK_SURFACE_WHITE, false);
            char text[32], args[32]; snprintf(text, sizeof text, "%d", d); snprintf(args, sizeof args, "{\"day\":%d}", d);
            ink_shell_bind(box, "calendar.select", args);
            lv_obj_t *label = ink_ui_line(box, text, INK_FONT_LIST, INK_WEIGHT_MEDIUM, 0, 0, cell);
            lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0); lv_obj_set_style_text_color(label, selected ? ink_ui_paper() : ink_ui_ink(), 0);
            lunar_text(d, true, text, sizeof text); label = ink_ui_line(box, text, INK_FONT_NOTE, INK_WEIGHT_REGULAR, 0, INK_LINE_LIST, cell);
            lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0); lv_obj_set_style_text_color(label, selected ? ink_ui_paper() : ink_ui_ink(), 0);
        }
        y += rows * INK_CONTROL; capacity = rows == 6 ? 1 : 2;
        char lunar[32]; lunar_text(C.date.day, false, lunar, sizeof lunar);
        snprintf(C.date_text, sizeof C.date_text, TR("%d年%d月%d日 · %s"), C.date.year, C.date.month, C.date.day, lunar);
        ink_ui_notice(screen, C.date_text, y); y += INK_LINE_NOTE + INK_GAP_S;
        snprintf(C.select_args, sizeof C.select_args, "{\"day\":%d}", C.date.day);
        control(0, "month-prev", TR("上月"), "calendar.month_prev", "{}"); control(1, "month-next", TR("下月"), "calendar.month_next", "{}");
        control(2, "select", TR("选一天"), "calendar.select", C.select_args);
    } else {
        snprintf(C.date_text, sizeof C.date_text, "%s", ink_time_valid() ? TR("今天和之后") : TR("请先设置时间"));
        ink_ui_notice(screen, C.date_text, y); y += INK_LINE_NOTE + INK_GAP;
        control(0, "month", TR("看月历"), "calendar.open", "{}");
        control(1, "add", TR("加一件"), "reminder.new", "{}");
        ink_shell_bind(ink_ui_pill(screen, TR("看月历"), INK_CANVAS_W - INK_MARGIN - INK_CIRCLE_LARGE, INK_STATUS_BAR + INK_GAP_S, INK_CIRCLE_LARGE, INK_CONTROL, false, NULL, NULL), "calendar.open", "{}");
        ink_shell_bind(ink_ui_pill(screen, TR("加一件"), INK_MARGIN, INK_CANVAS_H - INK_MARGIN - INK_PRIMARY, w, INK_PRIMARY, false, NULL, NULL), "reminder.new", "{}");
    }
    C.date_field = (ink_ui_field_spec_t){.id = "date", .role = "text", .label = C.calendar ? TR("所选日期") : TR("范围"), .value = C.date_text};
    C.controls = (ink_ui_page_spec_t){.id = ink_ui_page_name(), .title = C.calendar ? TR("月历") : TR("接下来"), .object = {"calendar", 1}, .fields = &C.date_field, .field_count = 1, .actions = C.actions, .action_count = 3};
    control(3, "back", TR("返回"), "shell.back", "{}"); C.controls.action_count = 4;
    load_rows();
    ink_ui_page_spec_t model = {.id = ink_ui_page_name(), .title = C.controls.title, .object = {"calendar", 1}, .actions = PAGING, .action_count = 2};
    int cell_h = C.calendar ? INK_CONTROL : INK_ROW_DOUBLE;
    lv_obj_t *body = ink_ui_box(screen, 0, y, INK_CANVAS_W, capacity * (cell_h + INK_GAP_M) + INK_GAP + INK_CONTROL, 0, INK_SURFACE_WHITE, false);
    C.list = ink_ui_list_create(body, &model, &C.position, (unsigned)capacity, 1, C.calendar ? INK_CONTROL : INK_ROW_DOUBLE, ink_time_valid() ? (C.calendar ? TR("这一天没有提醒和闹钟") : TR("还没有接下来的事")) : TR("请先设置时间"), bind_row, NULL);
    if (C.list) ink_ui_list_refresh(C.list, C.count);
}
static void destroy(void) { C.list = NULL; }
static int snapshot(char *out, size_t cap) {
    char *content = osal_malloc(cap, OSAL_MEM_EXTERNAL), *controls = osal_malloc(cap, OSAL_MEM_EXTERNAL); int n = -1;
    if (C.list && content && controls && ink_ui_list_snapshot(C.list, content, cap) >= 0 && ink_ui_semantics_snapshot(&C.controls, controls, cap) >= 0) {
        ink_jw_t w; ink_jw_init(&w, out, cap); ink_jw_raw(&w, "{\"content\":"); ink_jw_raw(&w, content);
        ink_jw_raw(&w, ",\"controls\":"); ink_jw_raw(&w, controls); ink_jw_raw(&w, "}"); n = w.overflow ? -1 : (int)w.len;
    }
    osal_free(controls); osal_free(content); if (n < 0 && cap) out[0] = 0; return n;
}
static bool page_key(ink_key_t key, bool lng) { return C.list && ink_ui_list_key(C.list, key, lng); }
static const ink_activity_t UPCOMING = {.name = "Upcoming", .app = "upcoming", .create = create, .destroy = destroy, .snapshot = snapshot, .on_key = page_key};
static const ink_activity_t CALENDAR = {.name = "Calendar", .app = "upcoming", .create = create, .destroy = destroy, .snapshot = snapshot, .on_key = page_key};
static void navigate(ink_call_t *call) {
    const char *id = call->cap->id;
    if (!strcmp(id, "calendar.open")) { memset(&C.position, 0, sizeof C.position); ink_ui_push(&CALENDAR, NULL); return; }
    if (!C.list || strcmp(C.member, ink_member_current()->id)) { call->rc.status = INK_RC_STALE; return; }
    if (!strcmp(id, "calendar.next") || !strcmp(id, "calendar.prev")) { ink_ui_list_turn(C.list, !strcmp(id, "calendar.next") ? 1 : -1); return; }
    if (!C.calendar || !ink_time_valid()) { call->rc.status = INK_RC_STALE; return; }
    if (!strcmp(id, "calendar.select")) {
        int64_t day = 0; ink_args_get_int(&call->args, "day", &day);
        if (day > month_days(C.date.year, C.date.month)) { call->rc.status = INK_RC_INVALID; return; }
        C.date.day = (int)day;
    } else {
        int month = C.date.year * 12 + C.date.month - 1 + (!strcmp(id, "calendar.month_next") ? 1 : -1);
        if (month < 1901 * 12 || month >= 2101 * 12) { call->rc.status = INK_RC_INVALID; return; }
        C.date.year = month / 12; C.date.month = month % 12 + 1; C.date.day = 1;
    }
    memset(&C.position, 0, sizeof C.position); ink_ui_redraw();
}
static struct {
    ink_alarm_t draft;
    ink_tm_t date;
    int repeat;
    lv_obj_t *name;
    char owner[16], at[32], object[24], error[192], ids[EDIT_ACTIONS][12], args[EDIT_ACTIONS][64];
    ink_ui_field_spec_t fields[EDIT_FIELDS];
    ink_ui_action_spec_t actions[EDIT_ACTIONS];
    ink_ui_page_spec_t page;
} E INK_PSRAM_BSS;
static const char *const REPEATS[] = {"none", "daily", "weekly", "workdays", "monthly", "yearly", "lunar_yearly"};
static const char *const REPEAT_LABELS[] = {TR_NOOP("不重复"), TR_NOOP("每天"), TR_NOOP("每周"), TR_NOOP("工作日"), TR_NOOP("每月"), TR_NOOP("每年"), TR_NOOP("农历每年")};
static const char *const PARTS[] = {"year", "month", "day", "hour", "minute"};
_Static_assert(sizeof PARTS / sizeof *PARTS * 2 + 1 <= EDIT_ACTIONS, "picker action slots");
_Static_assert(sizeof PARTS / sizeof *PARTS == PICKER_PARTS, "picker rows");
_Static_assert(REPEAT_PRESETS + 5 <= EDIT_ACTIONS, "editor action slots");
_Static_assert(sizeof REPEATS / sizeof *REPEATS == sizeof REPEAT_LABELS / sizeof *REPEAT_LABELS, "repeat labels");
static bool find_reminder(uint32_t id, ink_alarm_t *a) {
    int cursor = 0;
    while (ink_alarm_next(&cursor, INK_REMINDER, a) == 1) if (a->id == id) return true;
    return false;
}
static void editor_bind(lv_obj_t *button, const char *label, const char *cap, const char *args) {
    size_t i = E.page.action_count++;
    snprintf(E.ids[i], sizeof E.ids[i], "a%u", (unsigned)i); snprintf(E.args[i], sizeof E.args[i], "%s", args);
    E.actions[i] = (ink_ui_action_spec_t){.id = E.ids[i], .label = label, .capability = cap, .args_json = E.args[i], .enabled = true};
    if (button) ink_shell_bind(button, cap, args);
}
static void editor_name(lv_event_t *event) {
    (void)event; snprintf(E.draft.title, sizeof E.draft.title, "%s", ink_textfield_text(E.name));
}
static void editor_at(void) {
    snprintf(E.at, sizeof E.at, "%04d-%02d-%02d %02d:%02d", E.date.year, E.date.month, E.date.day, E.date.hour, E.date.min);
}
static void editor_date_bind(lv_obj_t *b, int part, int delta) {
    const int values[PICKER_PARTS] = {E.date.year, E.date.month, E.date.day, E.date.hour, E.date.min};
    const int lo[PICKER_PARTS] = {1901, 1, 1, 0, 0}, hi[PICKER_PARTS] = {2100, 12, month_days(E.date.year, E.date.month), 23, 59};
    int value = values[part] + delta;
    if (value < lo[part]) value = hi[part];
    if (value > hi[part]) value = lo[part];
    char args[48]; snprintf(args, sizeof args, "{\"%s\":%d}", PARTS[part], value);
    editor_bind(b, delta > 0 ? TR("增加") : TR("减少"), "reminder.draft", args);
}
/* ui-scale-runtime: dimension y,w,x Theme-derived editor and picker geometry. */
static void editor_create(lv_obj_t *screen, void *arg) {
    (void)arg; bool picker = !strcmp(ink_ui_page_name(), "ReminderTime");
    editor_at();
    snprintf(E.object, sizeof E.object, "reminder:%u", (unsigned)E.draft.id);
    E.fields[0] = (ink_ui_field_spec_t){.id = "title", .role = "textbox", .label = TR("内容"), .value = E.draft.title};
    E.fields[1] = (ink_ui_field_spec_t){.id = "at", .role = "text", .label = TR("日期和时间"), .value = E.at};
    E.fields[2] = (ink_ui_field_spec_t){.id = "repeat", .role = "text", .label = TR("重复"), .value = TR(REPEAT_LABELS[E.repeat])};
    E.fields[3] = (ink_ui_field_spec_t){.id = "error", .role = "status", .label = TR("提示"), .value = E.error};
    E.page = (ink_ui_page_spec_t){.id = ink_ui_page_name(), .title = picker ? TR("日期和时间") : E.draft.id ? TR("提醒") : TR("加一件"),
        .object = {E.object, 1}, .fields = E.fields, .field_count = EDIT_FIELDS, .actions = E.actions};
    ink_shell_statusbar(screen, true); ink_shell_topbar(screen, E.page.title);
    int y = INK_STATUS_BAR + INK_PRIMARY + INK_GAP, w = INK_CANVAS_W - 2 * INK_MARGIN;
    if (picker) {
        int values[PICKER_PARTS] = {E.date.year, E.date.month, E.date.day, E.date.hour, E.date.min};
        ink_ui_datetime_picker(screen, values, editor_date_bind);
        editor_bind(ink_ui_pill(screen, TR("选好了"), INK_MARGIN, INK_CANVAS_H - INK_MARGIN - INK_CONTROL, w, INK_CONTROL, true, NULL, NULL), TR("选好了"), "shell.back", "{}");
    } else {
        editor_bind(NULL, TR("输入内容"), "reminder.focus", "{}");
        ink_text_cfg_t cfg = {.type = INK_TEXT_PLAIN, .enter = INK_ENTER_DONE, .initial = E.draft.title, .hint = TR("提醒内容"), .max = sizeof E.draft.title - 1};
        E.name = ink_textfield_create(screen, &cfg, NULL, NULL, false);
        lv_obj_set_pos(E.name, ink_ui_px(INK_MARGIN), ink_ui_px(y)); lv_obj_set_width(E.name, ink_ui_px(w));
        ink_textfield_bind_short(E.name, &E.page, &E.fields[0], &E.actions[0]); lv_obj_add_event_cb(E.name, editor_name, LV_EVENT_VALUE_CHANGED, NULL);
        y += INK_CONTROL + INK_GAP;
        editor_bind(ink_ui_pill(screen, E.at, INK_MARGIN, y, w, INK_CONTROL, false, NULL, NULL), TR("日期和时间"), "reminder.time", "{}");
        y += INK_CONTROL + INK_GAP; ink_ui_notice(screen, TR(REPEAT_LABELS[E.repeat]), y); y += INK_LINE_NOTE + INK_GAP_S;
        for (int i = 0; i < REPEAT_PRESETS; i++) {
            char args[32]; snprintf(args, sizeof args, "{\"repeat\":%d}", i);
            editor_bind(ink_ui_pill(screen, TR(REPEAT_LABELS[i]), INK_MARGIN + i % 2 * (w + INK_GAP_M) / 2, y + i / 2 * (INK_CONTROL + INK_GAP_M), (w - INK_GAP_M) / 2, INK_CONTROL, E.repeat == i, NULL, NULL), TR(REPEAT_LABELS[i]), "reminder.draft", args);
        }
        ink_ui_notice(screen, E.error, y + 2 * (INK_CONTROL + INK_GAP_M) + INK_GAP);
        if (E.draft.id) {
            char args[48]; snprintf(args, sizeof args, "{\"id\":%u}", (unsigned)E.draft.id);
            editor_bind(ink_ui_pill(screen, TR("删除提醒"), INK_MARGIN, INK_CANVAS_H - INK_MARGIN - INK_CONTROL, (w - INK_GAP) / 2, INK_CONTROL, false, NULL, NULL), TR("删除提醒"), "reminder.cancel", args);
        }
        editor_bind(ink_ui_pill(screen, TR("保存提醒"), INK_MARGIN + (E.draft.id ? (w + INK_GAP) / 2 : 0), INK_CANVAS_H - INK_MARGIN - INK_CONTROL, E.draft.id ? (w - INK_GAP) / 2 : w, INK_CONTROL, true, NULL, NULL), TR("保存提醒"), "reminder.save", "{}");
        editor_bind(NULL, TR("返回"), "shell.back", "{}");
    }
}
static void editor_destroy(void) { E.name = NULL; }
static int editor_snapshot(char *out, size_t cap) {
    size_t actions = E.page.action_count; if (ink_text_focused()) E.page.action_count = 0;
    int n = E.name ? ink_textfield_short_snapshot(E.name, out, cap) : ink_ui_semantics_snapshot(&E.page, out, cap);
    E.page.action_count = actions; return n;
}
static const ink_activity_t REMINDER_EDIT = {.name = "ReminderEdit", .app = "upcoming", .create = editor_create, .destroy = editor_destroy, .snapshot = editor_snapshot};
static const ink_activity_t REMINDER_TIME = {.name = "ReminderTime", .app = "upcoming", .create = editor_create, .destroy = editor_destroy, .snapshot = editor_snapshot};
static void reminder_action(ink_call_t *call) {
    const char *cap = call->cap->id;
    if (!strcmp(cap, "reminder.new") || !strcmp(cap, "reminder.open")) {
        ink_alarm_t a = {.kind = INK_REMINDER}; int64_t id = 0; ink_args_get_int(&call->args, "id", &id);
        if (!ink_time_valid() || (id && !find_reminder((uint32_t)id, &a))) { call->rc.status = INK_RC_STALE; return; }
        memset(&E, 0, sizeof E); E.draft = a; snprintf(E.owner, sizeof E.owner, "%s", ink_member_current()->id);
        ink_time_to_local(a.next_s ? a.next_s : ink_time_now_s() + 86400, &E.date);
        if (!id) { E.date.hour = 8; E.date.min = 0; }
        E.repeat = a.repeat == INK_REPEAT_WEEKDAYS ? (a.weekdays == 127 ? 1 : a.weekdays == 62 ? 3 : 2) : a.repeat ? a.repeat + 2 : 0;
        ink_ui_push(&REMINDER_EDIT, NULL); return;
    }
    if (strcmp(E.owner, ink_member_current()->id) || (strcmp(ink_ui_page_name(), "ReminderEdit") && strcmp(ink_ui_page_name(), "ReminderTime"))) { call->rc.status = INK_RC_STALE; return; }
    if (!strcmp(cap, "reminder.time")) { ink_ui_push(&REMINDER_TIME, NULL); return; }
    if (!strcmp(cap, "reminder.focus")) { if (!E.name || ink_textfield_short_focus(E.name)) call->rc.status = INK_RC_STALE; return; }
    if (!strcmp(cap, "reminder.draft")) {
        ink_tm_t date = E.date; int *parts[] = {&date.year, &date.month, &date.day, &date.hour, &date.min}; int64_t v;
        for (size_t i = 0; i < sizeof PARTS / sizeof *PARTS; i++) if (ink_args_get_int(&call->args, PARTS[i], &v)) *parts[i] = (int)v;
        int days = month_days(date.year, date.month);
        if (date.day > days && ink_args_get_int(&call->args, "day", &v)) { call->rc.status = INK_RC_INVALID; return; }
        if (date.day > days) date.day = days;
        E.date = date; if (ink_args_get_int(&call->args, "repeat", &v)) E.repeat = (int)v;
        E.error[0] = 0; ink_ui_redraw(); return;
    }
    ink_args_t *args = osal_calloc(1, sizeof *args, OSAL_MEM_EXTERNAL); if (!args) { call->rc.status = INK_RC_FAILED; return; }
    editor_at(); ink_args_init(args); ink_args_str(args, "at", E.at); ink_args_str(args, "title", E.draft.title); ink_args_str(args, "repeat", REPEATS[E.repeat]);
    if (E.draft.id) ink_args_int(args, "id", E.draft.id);
    ink_cap_invoke(E.draft.id ? "reminder.update" : "reminder.add", args, call->src, 0, &call->rc); osal_free(args);
    if (call->rc.status == INK_RC_DONE) ink_ui_pop();
    else { snprintf(E.error, sizeof E.error, "%s", TR("请填写内容（最多15个字）并选择将来的时间")); ink_ui_redraw(); }
}
#define REMINDER_NAV(id_, title_) {.id = id_, .app = "clock", .title = title_, .effect = INK_FX_NAVIGATE, .child_ok = true, .run = reminder_action}
static const ink_cap_t CAPS[] = {
    REMINDER_NAV("reminder.new", "手动加提醒"),
    {.id = "reminder.open", .app = "clock", .title = "查看和修改我的提醒", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = reminder_action, .input = "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"integer\",\"minimum\":1,\"maximum\":4294967295}},\"required\":[\"id\"]}"},
    REMINDER_NAV("reminder.time", "选择提醒日期和时间"), REMINDER_NAV("reminder.focus", "输入提醒内容"),
    {.id = "reminder.draft", .app = "clock", .title = "选择提醒时间和重复", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = reminder_action,
     .input = "{\"type\":\"object\",\"properties\":{\"year\":{\"type\":\"integer\",\"minimum\":1901,\"maximum\":2100},\"month\":{\"type\":\"integer\",\"minimum\":1,\"maximum\":12},\"day\":{\"type\":\"integer\",\"minimum\":1,\"maximum\":31},\"hour\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":23},\"minute\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":59},\"repeat\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":6}}}"},
    {.id = "reminder.save", .app = "clock", .title = "保存提醒", .activity = "ReminderEdit", .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .run = reminder_action},
    {.id = "calendar.open", .app = "upcoming", .title = "看月历", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "calendar.prev", .app = "upcoming", .title = "当天事项或接下来上一页", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "calendar.next", .app = "upcoming", .title = "当天事项或接下来下一页", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "calendar.month_prev", .app = "upcoming", .title = "月历上个月", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "calendar.month_next", .app = "upcoming", .title = "月历下个月", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "calendar.select", .app = "upcoming", .title = "选择当月一天查看闹钟和提醒", .input = "{\"type\":\"object\",\"properties\":{\"day\":{\"type\":\"integer\",\"minimum\":1,\"maximum\":31}},\"required\":[\"day\"]}", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate}
};
static const ink_app_def_t APP = {.id = "upcoming", .title = TR_NOOP("接下来"), .icon = INK_UI_ICON(calendar), .entry = &UPCOMING, .system_app = true, .child_ok = true};
static void changed(void *ctx, int handle, const char *id) {
    (void)ctx; (void)handle; (void)id;
    if (ink_ui_page_current() && (!strcmp(ink_ui_page_name(), "Upcoming") || !strcmp(ink_ui_page_name(), "Calendar"))) ink_ui_redraw();
    else if (!strcmp(ink_ui_page_name(), "ReminderEdit") && E.draft.id) { ink_alarm_t a; if (!find_reminder(E.draft.id, &a)) ink_ui_pop(); }
}
int ink_calendar_init(void) {
    return ink_cap_register(CAPS, sizeof CAPS / sizeof *CAPS) || ink_app_register(&APP) || ink_state_watch("alarm.revision", changed, NULL, INK_EXEC_UI) ? -1 : 0;
}
