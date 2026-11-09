/* Calendar views share the time service, F4 lists and the capability bus. */
/* ui-scale-runtime: dimension w,x,y,cell,rows Theme-derived grid and list geometry. */
#include "ink/osal.h"
#include "ink/sys/appreg.h"
#include "ink/sys/json.h"
#include "ink/sys/members.h"
#include "ink/sys/state.h"
#include "ink/sys/time.h"
#include "ink/sys/ui_list.h"
#include "ink/ui_shell.h"
#include <stdio.h>
#include <string.h>

static const ink_activity_t UPCOMING, CALENDAR;
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
    char ids[4][32], values[4][128];
    bool calendar;
} C INK_PSRAM_BSS;
static const ink_ui_action_spec_t PAGING[] = {
    {.id = "prev", .label = "上一页", .capability = "calendar.prev"},
    {.id = "next", .label = "下一页", .capability = "calendar.next"}
};
static int month_days(int year, int month) {
    static const int DAYS[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    return DAYS[month - 1] + (month == 2 && year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
}
static void lunar_text(int day, bool short_name, char *out, size_t cap) {
    ink_lunar_t lunar; out[0] = 0;
    if (!ink_lunar_from_solar(C.date.year, C.date.month, day, &lunar)) return;
    ink_lunar_name(&lunar, out, cap);
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
    (void)ctx; unsigned n = index % 4; ink_alarm_t *a = &C.rows[index]; ink_tm_t t;
    ink_time_to_local(a->next_s, &t);
    snprintf(C.ids[n], sizeof C.ids[n], "%s:%u", a->kind == INK_ALARM ? "alarm" : "reminder", (unsigned)a->id);
    snprintf(C.values[n], sizeof C.values[n], "%d月%d日 %02d:%02d %s · %s", t.month, t.day, t.hour, t.min,
        a->kind == INK_ALARM ? "闹钟" : "提醒", a->title);
    *field = (ink_ui_field_spec_t){.id = C.ids[n], .role = "listitem", .label = a->kind == INK_ALARM ? "闹钟" : "提醒", .value = C.values[n], .object = {C.ids[n], 1}};
    *action = (ink_ui_action_spec_t){.id = C.ids[n], .label = "查看", .capability = "calendar.open", .enabled = false};
    if (!lv_obj_get_child_count(slot)) {
        ink_ui_line(slot, "", INK_FONT_LIST, INK_WEIGHT_REGULAR, INK_CONTROL, INK_GAP_S, INK_CANVAS_W - 2 * INK_MARGIN - INK_CONTROL - INK_GAP_M);
        if (!C.calendar) ink_ui_line(slot, "", INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_CONTROL, INK_LINE_LIST + INK_GAP_S, INK_CANVAS_W - 2 * INK_MARGIN - INK_CONTROL - INK_GAP_M);
    }
    unsigned labels = C.calendar ? 1 : 2;
    if (lv_obj_get_child_count(slot) > labels) lv_obj_delete(lv_obj_get_child(slot, (int32_t)labels));
    ink_ui_icon(slot, a->kind == INK_ALARM ? "clock" : "list", INK_ICON_LIST, INK_ICON_LINE, INK_GAP_M, INK_GAP_L, false);
    char title[96]; snprintf(title, sizeof title, "%02d:%02d %s", t.hour, t.min, a->title);
    lv_label_set_text(lv_obj_get_child(slot, 0), title);
    if (!C.calendar) { snprintf(title, sizeof title, "%d月%d日 · %s", t.month, t.day, a->kind == INK_ALARM ? "闹钟" : "提醒"); lv_label_set_text(lv_obj_get_child(slot, 1), title); }
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
    char title[64]; snprintf(title, sizeof title, "%d年%d月", C.date.year, C.date.month);
    ink_shell_topbar(screen, C.calendar ? title : "接下来的事");
    int y = INK_STATUS_BAR + INK_PRIMARY + INK_GAP_XS, w = INK_CANVAS_W - 2 * INK_MARGIN, capacity = 4;
    memset(C.actions, 0, sizeof C.actions);
    if (C.calendar && ink_time_valid()) {
        int x = INK_CANVAS_W - INK_MARGIN - 2 * INK_CONTROL - INK_GAP_XS;
        ink_shell_bind(ink_ui_circle(screen, "back", x, INK_STATUS_BAR + INK_GAP_S, INK_CONTROL, INK_ICON_MEDIUM, INK_SURFACE_WHITE, NULL, NULL), "calendar.month_prev", "{}");
        ink_shell_bind(ink_ui_circle(screen, "next", x + INK_CONTROL + INK_GAP_XS, INK_STATUS_BAR + INK_GAP_S, INK_CONTROL, INK_ICON_MEDIUM, INK_SURFACE_WHITE, NULL, NULL), "calendar.month_next", "{}");
        ink_tm_t first = {.year = C.date.year, .month = C.date.month, .day = 1}; ink_time_to_local(ink_time_from_local(&first), &first);
        int days = month_days(C.date.year, C.date.month), rows = (first.wday + days + 6) / 7, cell = w / 7;
        static const char *const WEEK[] = {"日", "一", "二", "三", "四", "五", "六"};
        for (int i = 0; i < 7; i++) ink_ui_line(screen, WEEK[i], INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_MARGIN + i * cell + INK_GAP_M, y, cell);
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
        snprintf(C.date_text, sizeof C.date_text, "%d年%d月%d日 · %s", C.date.year, C.date.month, C.date.day, lunar);
        ink_ui_notice(screen, C.date_text, y); y += INK_LINE_NOTE + INK_GAP_S;
        snprintf(C.select_args, sizeof C.select_args, "{\"day\":%d}", C.date.day);
        control(0, "month-prev", "上月", "calendar.month_prev", "{}"); control(1, "month-next", "下月", "calendar.month_next", "{}");
        control(2, "select", "选一天", "calendar.select", C.select_args);
    } else {
        snprintf(C.date_text, sizeof C.date_text, "%s", ink_time_valid() ? "今天和之后" : "请先设置时间");
        ink_ui_notice(screen, C.date_text, y); y += INK_LINE_NOTE + INK_GAP;
        control(0, "month", "看月历", "calendar.open", "{}");
        control(1, "add", "加一件", "ai.panel.listen", "{}");
        ink_shell_bind(ink_ui_pill(screen, "看月历", INK_CANVAS_W - INK_MARGIN - INK_CIRCLE_LARGE, INK_STATUS_BAR + INK_GAP_S, INK_CIRCLE_LARGE, INK_CONTROL, false, NULL, NULL), "calendar.open", "{}");
        ink_shell_bind(ink_ui_pill(screen, "加一件", INK_MARGIN, INK_CANVAS_H - INK_MARGIN - INK_PRIMARY, w, INK_PRIMARY, false, NULL, NULL), "ai.panel.listen", "{}");
    }
    C.date_field = (ink_ui_field_spec_t){.id = "date", .role = "text", .label = C.calendar ? "所选日期" : "范围", .value = C.date_text};
    C.controls = (ink_ui_page_spec_t){.id = ink_ui_page_name(), .title = C.calendar ? "月历" : "接下来", .object = {"calendar", 1}, .fields = &C.date_field, .field_count = 1, .actions = C.actions, .action_count = 3};
    control(3, "back", "返回", "shell.back", "{}"); C.controls.action_count = 4;
    load_rows();
    ink_ui_page_spec_t model = {.id = ink_ui_page_name(), .title = C.controls.title, .object = {"calendar", 1}, .actions = PAGING, .action_count = 2};
    int cell_h = C.calendar ? INK_CONTROL : INK_ROW_DOUBLE;
    lv_obj_t *body = ink_ui_box(screen, 0, y, INK_CANVAS_W, capacity * (cell_h + INK_GAP_M) + INK_GAP + INK_CONTROL, 0, INK_SURFACE_WHITE, false);
    C.list = ink_ui_list_create(body, &model, &C.position, (unsigned)capacity, 1, C.calendar ? INK_CONTROL : INK_ROW_DOUBLE, ink_time_valid() ? (C.calendar ? "这一天没有提醒和闹钟" : "还没有接下来的事") : "请先设置时间", bind_row, NULL);
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
static const ink_cap_t CAPS[] = {
    {.id = "calendar.open", .title = "看月历", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "calendar.prev", .title = "当天事项或接下来上一页", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "calendar.next", .title = "当天事项或接下来下一页", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "calendar.month_prev", .title = "月历上个月", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "calendar.month_next", .title = "月历下个月", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "calendar.select", .title = "选择当月一天查看闹钟和提醒", .input = "{\"type\":\"object\",\"properties\":{\"day\":{\"type\":\"integer\",\"minimum\":1,\"maximum\":31}},\"required\":[\"day\"]}", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate}
};
static const ink_app_def_t APP = {.id = "upcoming", .title = "接下来", .icon = INK_UI_ICON(calendar), .entry = &UPCOMING, .system_app = true, .child_ok = true};
static void changed(void *ctx, int handle, const char *id) {
    (void)ctx; (void)handle; (void)id;
    if (ink_ui_page_current() && (!strcmp(ink_ui_page_name(), "Upcoming") || !strcmp(ink_ui_page_name(), "Calendar"))) ink_ui_redraw();
}
int ink_calendar_init(void) {
    return ink_cap_register(CAPS, sizeof CAPS / sizeof *CAPS) || ink_app_register(&APP) || ink_state_watch("alarm.revision", changed, NULL, INK_EXEC_UI) ? -1 : 0;
}
