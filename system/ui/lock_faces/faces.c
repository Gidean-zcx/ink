/* CrossMux layout adaptation; see SOURCE.md. Dates come only from the time service. */
/* ui-scale-runtime: dimension y Forwarded row positions advance by theme line heights; checked at 100 and 120 percent. */
/* ui-scale-runtime: font font Centered labels receive theme font tokens. */
#include <stdio.h>
#include <string.h>
#include "faces.h"
#include "ink/osal.h"
#include "ink/sys/app.h"
#include "ink/sys/appreg.h"
#include "ink/sys/i18n.h"
#include "ink/sys/json.h"
#include "ink/sys/members.h"
#include "ink/sys/power.h"
#include "ink/sys/settings.h"
#include "ink/sys/state.h"
#include "ink/sys/textfield.h"
#include "ink/sys/time.h"
#include "ink/sys/text.h"

const char *ink_lunar_solar_term(int year, int month, int day);

/* The one list of styles (settings_body.c uses it too). New ones only go at the end: a saved number keeps its style. */
const char *const ink_lock_face_choices[] = {TR_NOOP("时钟和通知"), TR_NOOP("时钟和待办"), TR_NOOP("壁纸"), TR_NOOP("简约时钟"), TR_NOOP("待机时钟"), TR_NOOP("月历"),
    TR_NOOP("票根"), TR_NOOP("拼贴"), TR_NOOP("数字时钟"), TR_NOOP("老黄历"), TR_NOOP("万年历"), TR_NOOP("迷你书库"), NULL};
const char *const ink_lock_face_notes[] = {TR_NOOP("时间、今天的安排、未读通知和家人"), TR_NOOP("时间、下一个闹钟和通知数"),
    TR_NOOP("选好的图片铺满屏幕"), TR_NOOP("上下两行的大时间"), TR_NOOP("时间、农历、待办和继续读"), TR_NOOP("这个月的日历和农历"),
    TR_NOOP("正在读的书和今天读了多久"), TR_NOOP("书架上的封面拼在一起"), TR_NOOP("大号数字时间、日期和电量"),
    TR_NOOP("今天的宜忌、冲煞和农历"), TR_NOOP("整月日历、农历和宜忌"), TR_NOOP("最近读的五本书，点一本接着读"), NULL};
_Static_assert(sizeof ink_lock_face_choices / sizeof *ink_lock_face_choices == INK_LOCK_FACE_COUNT + 1, "face choices match");
_Static_assert(sizeof ink_lock_face_notes / sizeof *ink_lock_face_notes == INK_LOCK_FACE_COUNT + 1, "face notes match");
void ink_lock_drawn_draw(lv_obj_t *parent);
void ink_lock_drawn_clear(void);
bool ink_lock_drawn_update(int style);
void ink_lock_drawn_resume(void);
size_t ink_lock_drawn_fields(ink_ui_field_spec_t *fields);
static struct {
    lv_obj_t *parent, *body, *clock, *date, *lunar, *todo, *reading;
    int style, day;
    char owner[16], month[1536];
} F INK_PSRAM_BSS;
static struct { bool pending; char owner[16]; } R;
static bool visible(void) { return ink_member_space_open() && !strcmp(F.owner, ink_member_current()->id); }
static lv_obj_t *center(lv_obj_t *p, const char *s, int font, int y) {
    lv_obj_t *l = ink_ui_label(p, s, font, INK_WEIGHT_REGULAR, INK_MARGIN, y, INK_CANVAS_W - 2 * INK_MARGIN);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_bg_color(l, ink_ui_paper(), 0); lv_obj_set_style_bg_opa(l, LV_OPA_COVER, 0);
    return l;
}
static void read_clicked(lv_event_t *e) {
    lv_event_stop_processing(e);
    if (strcmp(F.owner, ink_member_current()->id)) return;
    snprintf(R.owner, sizeof R.owner, "%s", F.owner); R.pending = true;
    ink_power_unlock(ink_now_us());
}
void ink_lock_face_resume(void) {
    ink_lock_drawn_resume();
    if (!R.pending) return;
    if (strcmp(R.owner, ink_member_current()->id)) { R.pending = false; return; }
    if (ink_power_state() == INK_PWR_LOCKED || !ink_member_space_open()) return;
    R.pending = false;
    ink_call_opts_t opts = {.src = INK_SRC_TOUCH}; ink_cap_call("reader.continue", NULL, &opts);
}
static void memos(void *ctx, ink_db_t *db) {
    (void)ctx;
    char *raw = osal_malloc(INK_DB_RECORD_MAX + 1, OSAL_MEM_EXTERNAL);
    ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL);
    ink_db_cursor_t cur = {.desc = true}; ink_rec_t rec;
    while (db && raw && j && F.todo && visible() && ink_db_next(db, &cur, &rec, raw, INK_DB_RECORD_MAX) == 1) {
        raw[rec.len] = 0;
        if (ink_json_parse(j, raw, rec.len) || !ink_json_eq(j, ink_json_find(j, 0, "checklist"), "true") ||
            ink_json_eq(j, ink_json_find(j, 0, "deleted"), "true")) continue;
        int64_t checked = 0; ink_json_int(j, ink_json_find(j, 0, "checked"), &checked);
        if (ink_json_str(j, ink_json_find(j, 0, "body"), raw, INK_DB_RECORD_MAX + 1) < 0) continue;
        char *line = raw;
        for (unsigned i = 0; *line && i < 32; i++) {
            size_t n = strcspn(line, "\n");
            if (n && !((uint32_t)checked & (UINT32_C(1) << i))) {
                if (n > ink_text_byte_offset(line, 24)) n = ink_text_byte_offset(line, 24);
                line[n] = 0; lv_label_set_text(F.todo, line); ink_ui_hint_obj(F.todo, INK_REFRESH_TEXT);
                goto done;
            }
            line += n;
            if (*line) line++;
        }
    }
done:
    osal_free(j); osal_free(raw); ink_db_close(db);
}
static void widgets(int y) {
    if (F.style == INK_LOCK_SIMPLE) return;
    int h = ink_ui_line_height(INK_FONT_NOTE);
    if (ink_setting_get("display.lock_todo")) {
        ink_ui_box(F.body, INK_MARGIN, y, INK_CANVAS_W - 2 * INK_MARGIN, h, 0, INK_SURFACE_WHITE, false);
        ink_ui_line(F.body, TR("待办"), INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_MARGIN, y, INK_BLOCK_L);
        F.todo = ink_ui_line(F.body, visible() ? TR("还没有未完成的待办") : TR("解锁后查看"), INK_FONT_NOTE, INK_WEIGHT_REGULAR,
                             INK_MARGIN + INK_BLOCK_L, y, INK_CANVAS_W - 2 * INK_MARGIN - INK_BLOCK_L);
        const ink_db_def_t *def = ink_db_find_def("memo.notes");
        if (visible() && def && ink_member_app_mode(NULL, "memo")) ink_app_db_open_async("memo", def, F.body, memos, NULL);
        y += h + INK_GAP;
    }
    if (ink_setting_get("display.lock_reading")) {
        ink_ui_box(F.body, INK_MARGIN, y, INK_CANVAS_W - 2 * INK_MARGIN, h, 0, INK_SURFACE_WHITE, false);
        ink_card_t card = {0};
        for (int i = 0; visible() && i < ink_app_count(); i++) {
            const ink_app_def_t *app = ink_app_at(i);
            if (!strcmp(app->id, "reader") && ink_app_visible(app) && app->card) app->card(0, &card);
        }
        ink_ui_line(F.body, TR("继续读"), INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_MARGIN, y, INK_BLOCK_L);
        F.reading = ink_ui_line(F.body, visible() && ink_app_card_visible(&card) && card.text[0] ? card.text : TR("解锁后继续读"),
                                INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_MARGIN + INK_BLOCK_L, y,
                                INK_CANVAS_W - 2 * INK_MARGIN - INK_BLOCK_L);
        lv_obj_add_flag(F.reading, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(F.reading, read_clicked, LV_EVENT_CLICKED, NULL);
        ink_shell_bind(F.reading, "reader.continue", NULL);
    }
}
static void calendar(const ink_tm_t *t, int y) {
    const int w = (INK_CANVAS_W - 2 * INK_MARGIN) / 7, h = ink_ui_line_height(INK_FONT_STATUS), row = 2 * h + INK_GAP_XS;
    static const char *const weekdays[] = {TR_NOOP("周日"), TR_NOOP("周一"), TR_NOOP("周二"), TR_NOOP("周三"), TR_NOOP("周四"), TR_NOOP("周五"), TR_NOOP("周六")};
    ink_ui_box(F.body, INK_MARGIN, y, INK_CANVAS_W - 2 * INK_MARGIN, h, 0, INK_SURFACE_WHITE, false);
    for (int i = 0; i < 7; i++) {
        const char *label = TR(weekdays[i]);
        lv_obj_t *l = ink_ui_line(F.body, !strcmp(ink_lang_code(), "zh") ? label + strlen("周") : label, INK_FONT_STATUS, INK_WEIGHT_MEDIUM, INK_MARGIN + i * w, y, w);
        lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    }
    y += h;
    static const int days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    int count = days[t->month - 1] + (t->month == 2 && t->year % 4 == 0 && (t->year % 100 != 0 || t->year % 400 == 0));
    int first = (t->wday - (t->day - 1) % 7 + 7) % 7; size_t used = 0; F.month[0] = 0;
    for (int d = 1; d <= count; d++) {
        ink_lunar_t lunar; char name[64], text[96];
        snprintf(name, sizeof name, "%s", TR("暂不可用"));
        if (ink_lunar_from_solar(t->year, t->month, d, &lunar)) {
            ink_lunar_name(&lunar, name, sizeof name);
            char *month = strstr(name, "月");
            if (month && lunar.day == 1) month[strlen("月")] = 0;
            else if (month) memmove(name, month + strlen("月"), strlen(month + strlen("月")) + 1);
            if (strcmp(ink_lang_code(), "zh")) {
                if (lunar.day == 1) snprintf(name, sizeof name, lunar.leap ? TR("闰%d月") : TR("%d月"), lunar.month);
                else snprintf(name, sizeof name, "%d", lunar.day);
            }
        }
        int slot = first + d - 1;
        lv_obj_t *cell = ink_ui_box(F.body, INK_MARGIN + slot % 7 * w, y + slot / 7 * row, w, row, 0,
                                    d == t->day ? INK_SURFACE_BLACK : INK_SURFACE_WHITE, false);
        snprintf(text, sizeof text, "%d\n%s", d, name);
        lv_obj_t *l = ink_ui_label(cell, text, INK_FONT_STATUS, INK_WEIGHT_REGULAR, 0, 0, w);
        lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
        int n = snprintf(F.month + used, sizeof F.month - used, "%d:%s%s;", d, name, d == t->day ? TR("（今天）") : "");
        if (n > 0 && (size_t)n < sizeof F.month - used) used += (size_t)n;
    }
    widgets(y + 6 * row + INK_GAP);
}
void ink_lock_face_draw(lv_obj_t *parent, int style) {
    F.parent = parent; F.style = style;
    if (style >= INK_LOCK_DRAWN) { ink_lock_drawn_draw(parent); return; }
    snprintf(F.owner, sizeof F.owner, "%s", ink_member_current()->id);
    F.body = ink_ui_box(parent, 0, 0, INK_CANVAS_W, INK_CANVAS_H, 0, INK_SURFACE_WHITE, false);
    lv_obj_set_style_bg_opa(F.body, LV_OPA_TRANSP, 0);
    ink_tm_t t; ink_time_to_local(ink_time_now_s(), &t); F.day = t.year * 10000 + t.month * 100 + t.day;
    int y = INK_STATUS_BAR + INK_GAP + (style == INK_LOCK_CALENDAR ? 0 : INK_CONTROL + INK_GAP_M);
    char time[24], date[96], name[64], text[128], ganzhi[24] = "";
    snprintf(name, sizeof name, "%s", TR("农历暂不可用"));
    ink_shell_time_strings(time, sizeof time, date, sizeof date);
    char *separator = strstr(date, " · "); if (separator) *separator = 0;
    ink_lunar_t lunar;
    if (ink_time_valid() && ink_lunar_from_solar(t.year, t.month, t.day, &lunar)) {
        ink_lunar_name(&lunar, name, sizeof name); ink_lunar_ganzhi(lunar.year, ganzhi, sizeof ganzhi);
    }
    if (style != INK_LOCK_CALENDAR) {
        if (style == INK_LOCK_SIMPLE && strchr(time, ':')) *strchr(time, ':') = '\n';
        F.clock = center(F.body, time, INK_FONT_CLOCK, y);
        y += (style == INK_LOCK_SIMPLE ? 2 : 1) * ink_ui_line_height(INK_FONT_CLOCK) + INK_GAP;
    }
    F.date = center(F.body, date, INK_FONT_LIST, y); y += ink_ui_line_height(INK_FONT_LIST);
    if (ganzhi[0]) snprintf(text, sizeof text, TR("%s  %s%s年"), name, ganzhi, ink_lunar_zodiac(lunar.year));
    else snprintf(text, sizeof text, "%s", name);
    const char *term = style == INK_LOCK_CALENDAR && ink_time_valid() ? ink_lunar_solar_term(t.year, t.month, t.day) : NULL;
    if (term) {
        size_t used = strlen(text);
        snprintf(text + used, sizeof text - used, "  %s", term);
    }
    F.lunar = center(F.body, text, INK_FONT_NOTE, y);
    y += ink_ui_text_height(text, INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_CANVAS_W - 2 * INK_MARGIN) + INK_GAP;
    if (style == INK_LOCK_CALENDAR && ink_time_valid()) calendar(&t, y);
    else widgets(y + INK_GAP_L);
}
bool ink_lock_face_update(void) {
    int chosen = (int)ink_setting_get("display.lock_style");
    if (chosen >= INK_LOCK_DRAWN) return ink_power_state() == INK_PWR_LOCKED && ink_lock_drawn_update(chosen);
    if (!F.body) return false;
    ink_tm_t t; ink_time_to_local(ink_time_now_s(), &t);
    int day = t.year * 10000 + t.month * 100 + t.day;
    if (day != F.day) {
        lv_obj_t *parent = F.parent; int style = F.style;
        ink_lock_face_clear(); ink_lock_face_draw(parent, style);
        ink_ui_hint_obj(F.body, INK_REFRESH_TEXT); return true;
    }
    char time[24], date[96]; ink_shell_time_strings(time, sizeof time, date, sizeof date);
    if (F.style == INK_LOCK_SIMPLE && strchr(time, ':')) *strchr(time, ':') = '\n';
    if (!F.clock || !strcmp(lv_label_get_text(F.clock), time)) return false;
    lv_label_set_text(F.clock, time); ink_ui_hint_obj(F.clock, INK_REFRESH_TEXT); return true;
}
void ink_lock_face_clear(void) {
    ink_lock_drawn_clear();
    if (F.body) lv_obj_delete(F.body);
    memset(&F, 0, sizeof F);
}
size_t ink_lock_face_fields(ink_ui_field_spec_t *fields) {
    if (F.parent && F.style >= INK_LOCK_DRAWN) return ink_lock_drawn_fields(fields);
    if (!F.body) return 0;
    lv_obj_t *objects[] = {F.clock, F.date, F.lunar, NULL, F.todo, F.reading};
    static const char *const names[] = {"time", "date", "lunar", "calendar", "todo", "reading"};
    _Static_assert(sizeof objects / sizeof *objects == sizeof names / sizeof *names, "face fields match");
    _Static_assert(sizeof names / sizeof *names <= INK_LOCK_FACE_FIELDS, "face field capacity");
    for (size_t i = 0; i < sizeof names / sizeof *names; i++) fields[i] = (ink_ui_field_spec_t){.id = names[i], .role = "text", .label = names[i],
        .value = i >= 4 && !visible() ? "" : i == 3 ? F.month : objects[i] ? lv_label_get_text(objects[i]) : ""};
    return sizeof names / sizeof *names;
}
