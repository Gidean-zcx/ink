/* Date, zone and return-grace presenters use the existing capability bus. */
/* ui-scale-runtime: dimension x,y,w,h Theme-sized rows, buttons and page body. */
#include <stdio.h>
#include <string.h>
#include "ink/osal.h"
#include "ink/sys/hub.h"
#include "ink/sys/i18n.h"
#include "ink/sys/members.h"
#include "ink/sys/settings.h"
#include "ink/sys/time.h"
#include "ink/sys/ui_list.h"
#include "ink/ui_shell.h"

enum { TIME, EDIT, ZONES, RETURN };
#define TIME_FIELDS_MAX 5
#define TIME_ACTIONS_MAX 12
static const char *const NAMES[] = {"SetTime", "SetDateTime", "SetTimeZone", "SetReturn"};
static const char *const TITLES[] = {TR_NOOP("日期和时间"), TR_NOOP("手动设置"), TR_NOOP("时区"), TR_NOOP("离开多久要重新验证")};
static const char *const PARTS[] = {TR_NOOP("年"), TR_NOOP("月"), TR_NOOP("日"), TR_NOOP("时"), TR_NOOP("分")};
static const int GRACE[] = {60, 120, 300, 900};
static const char *const GRACE_NAMES[] = {TR_NOOP("1 分钟"), TR_NOOP("2 分钟"), TR_NOOP("5 分钟"), TR_NOOP("15 分钟")};
_Static_assert(sizeof PARTS / sizeof *PARTS <= TIME_FIELDS_MAX, "date fields fit");
_Static_assert(sizeof GRACE / sizeof *GRACE <= TIME_FIELDS_MAX, "return choices fit");
_Static_assert(sizeof GRACE_NAMES / sizeof *GRACE_NAMES == sizeof GRACE / sizeof *GRACE, "return labels fit");
static struct {
    int page, part[TIME_FIELDS_MAX], fields, actions;
    lv_obj_t *screen;
    ink_ui_list_t *list;
    ink_ui_list_position_t pos;
    ink_ui_field_spec_t field[TIME_FIELDS_MAX];
    ink_ui_action_spec_t action[TIME_ACTIONS_MAX];
    char values[TIME_FIELDS_MAX][128], args[TIME_ACTIONS_MAX][96], ids[TIME_ACTIONS_MAX][24];
    ink_ui_page_spec_t model;
} P INK_PSRAM_BSS;
static const ink_activity_t PAGES[];
static bool current(void) { return P.screen && ink_ui_page_current() && !strcmp(ink_ui_page_name(), NAMES[P.page]); }
static void field(int i, const char *id, const char *label, const char *value) {
    snprintf(P.values[i], sizeof P.values[i], "%s", value);
    P.field[i] = (ink_ui_field_spec_t){.id = id, .role = "setting", .label = label, .value = P.values[i]};
    P.fields = i + 1;
}
static void button(const char *label, const char *cap, const char *args, bool enabled, int x, int y, int w, bool primary) {
    int i = P.actions++;
    snprintf(P.ids[i], sizeof P.ids[i], "action:%d", i);
    snprintf(P.args[i], sizeof P.args[i], "%s", args ? args : "{}");
    P.action[i] = (ink_ui_action_spec_t){.id = P.ids[i], .label = label, .capability = cap, .args_json = P.args[i], .enabled = enabled};
    lv_obj_t *b = P.page == TIME && !strcmp(cap, "settings.set") ?
        ink_ui_toggle(P.screen, label, ink_setting_get(i == 2 ? "time.zone_auto" : "time.auto"), x, y, w, NULL, NULL) :
        ink_ui_pill(P.screen, label, x, y, w, INK_CONTROL, primary, NULL, NULL);
    if (enabled) ink_shell_bind(b, cap, P.args[i]);
    else lv_obj_add_state(b, LV_STATE_DISABLED);
}
static void bind_zone(lv_obj_t *slot, size_t i, ink_ui_field_spec_t *f, ink_ui_action_spec_t *a, void *ctx) {
    (void)ctx; unsigned k = (unsigned)(i % 4);
    const char *name = ink_time_zone_name((int)i), *city = TR(ink_time_zone_city((int)i));
    snprintf(P.args[k], sizeof P.args[k], "{\"id\":\"time.zone\",\"value\":%u}", (unsigned)i);
    *f = (ink_ui_field_spec_t){.id = name, .role = "option", .label = city, .value = name, .object = {name, 0}};
    *a = (ink_ui_action_spec_t){.id = name, .label = city, .capability = "settings.set", .args_json = P.args[k], .enabled = !ink_setting_get("time.zone_auto"), .object = f->object};
    lv_obj_clean(slot);
    ink_ui_line(slot, city, INK_FONT_LIST, INK_WEIGHT_MEDIUM, INK_GAP, INK_GAP_M, INK_CANVAS_W - 2 * INK_MARGIN - 2 * INK_GAP);
    ink_ui_line(slot, name, INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_GAP, INK_GAP_M + ink_ui_line_height(INK_FONT_LIST), INK_CANVAS_W - 2 * INK_MARGIN - 2 * INK_GAP);
}
static void create(lv_obj_t *screen, void *arg) {
    P.page = (int)(intptr_t)arg; P.screen = screen; P.fields = P.actions = 0; P.list = NULL;
    P.model = (ink_ui_page_spec_t){.id = NAMES[P.page], .title = TR(TITLES[P.page]), .fields = P.field, .actions = P.action};
    ink_shell_statusbar(screen, true); ink_shell_topbar(screen, TR(TITLES[P.page]));
    int y = INK_STATUS_BAR + INK_PRIMARY + INK_GAP, w = INK_CANVAS_W - 2 * INK_MARGIN;
    char text[128], args[96];
    if (P.page == ZONES) {
        static const ink_ui_action_spec_t paging[] = {
            {.id = "prev", .label = "上一页", .capability = "shell.time_prev"}, {.id = "next", .label = "下一页", .capability = "shell.time_next"}};
        ink_ui_page_spec_t page = {.id = "SetTimeZone", .title = TR("时区"), .actions = paging, .action_count = 2};
        lv_obj_t *body = ink_ui_box(screen, 0, y, INK_CANVAS_W, INK_CANVAS_H - y, 0, INK_SURFACE_WHITE, false);
        P.list = ink_ui_list_create(body, &page, &P.pos, 4, 1, ink_ui_row_double(), "", bind_zone, NULL);
        ink_ui_list_refresh(P.list, (size_t)ink_time_zone_count()); return;
    }
    if (P.page == TIME) {
        const char *const labels[] = {TR("自动设置日期和时间"), TR("手动设置"), TR("自动设置时区"), TR("选择城市或地区")};
        for (int i = 0; i < 4; i++) {
            const char *setting = i < 2 ? "time.auto" : "time.zone_auto";
            bool automatic = ink_setting_get(setting) != 0;
            if (!(i % 2)) snprintf(text, sizeof text, "%s", automatic ? TR("自动") : TR("手动"));
            else if (i == 1) {
                ink_tm_t t; ink_time_to_local(ink_time_now_s(), &t);
                snprintf(text, sizeof text, "%04d-%02d-%02d %02d:%02d", t.year, t.month, t.day, t.hour, t.min);
                if (!ink_time_valid()) snprintf(text, sizeof text, "%s", TR("时间尚未设置"));
            } else snprintf(text, sizeof text, "%s", ink_time_zone());
            field(i, i == 1 ? "local" : i == 3 ? "zone" : setting, labels[i], text);
            ink_ui_line(screen, text, INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_MARGIN, y, w);
            snprintf(args, sizeof args, "{\"id\":\"%s\",\"value\":%d}", setting, !automatic);
            button(labels[i], i % 2 ? (i == 1 ? "shell.time_edit" : "shell.time_zones") : "settings.set", i % 2 ? NULL : args,
                !(i % 2) || !automatic, INK_MARGIN, y + ink_ui_line_height(INK_FONT_NOTE), w, false);
            y += INK_CONTROL + ink_ui_line_height(INK_FONT_NOTE) + INK_GAP;
        }
        char zone[40];
        ink_ui_notice(screen, TR(ink_setting_get("time.zone_auto") && !ink_hub_server_zone(zone, sizeof zone) ? "等待网络提供时区" : "使用 24 小时制；夏令时自动调整"), y);
    } else if (P.page == EDIT) {
        for (int i = 0; i < TIME_FIELDS_MAX; i++) {
            snprintf(text, sizeof text, "%s %02d", TR(PARTS[i]), P.part[i]); field(i, PARTS[i], TR(PARTS[i]), text);
            ink_ui_line(screen, text, INK_FONT_LIST, INK_WEIGHT_MEDIUM, INK_MARGIN + INK_PRIMARY + INK_GAP_M, y + INK_GAP_L, w - 2 * (INK_PRIMARY + INK_GAP_M));
            for (int j = 0; j < 2; j++) {
                snprintf(args, sizeof args, "{\"field\":%d,\"delta\":%d}", i, j ? 1 : -1);
                button(j ? "+" : "−", "shell.time_step", args, true, j ? INK_CANVAS_W - INK_MARGIN - INK_PRIMARY : INK_MARGIN, y, INK_PRIMARY, false);
            }
            y += INK_CONTROL + INK_GAP_M;
        }
        ink_tm_t t = {.year = P.part[0], .month = P.part[1], .day = P.part[2], .hour = P.part[3], .min = P.part[4]};
        snprintf(args, sizeof args, "{\"utc_s\":%lld}", (long long)ink_time_from_local(&t));
        button(TR("保存日期和时间"), "time.set", args, !ink_setting_get("time.auto"), INK_MARGIN, INK_CANVAS_H - INK_MARGIN - INK_CONTROL, w, true);
    } else for (int i = 0; i < (int)(sizeof GRACE / sizeof *GRACE); i++) {
        const char *label = TR(GRACE_NAMES[i]);
        field(i, GRACE_NAMES[i], label, ink_setting_get("member.idle_return_s") == GRACE[i] ? TR("已选择") : "");
        snprintf(args, sizeof args, "{\"id\":\"member.idle_return_s\",\"value\":%d}", GRACE[i]);
        button(label, "settings.set", args, true, INK_MARGIN, y, w, false);
        ink_ui_line(screen, P.values[i], INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_MARGIN, y + INK_CONTROL, w);
        y += INK_CONTROL + ink_ui_line_height(INK_FONT_NOTE) + INK_GAP;
    }
    P.model.field_count = (size_t)P.fields; P.model.action_count = (size_t)P.actions;
}
static void destroy(void) { P.screen = NULL; P.list = NULL; }
static int snapshot(char *out, size_t cap) { return P.list ? ink_ui_list_snapshot(P.list, out, cap) : ink_ui_semantics_snapshot(&P.model, out, cap); }
static bool key(ink_key_t k, bool lng) { return P.list && ink_ui_list_key(P.list, k, lng); }
#define PAGE(n) {.name = n, .create = create, .destroy = destroy, .snapshot = snapshot, .on_key = key}
static const ink_activity_t PAGES[] = {PAGE("SetTime"), PAGE("SetDateTime"), PAGE("SetTimeZone"), PAGE("SetReturn")};
static void navigate(ink_call_t *c) {
    if (ink_member_current()->role == INK_ROLE_CHILD || !ink_member_space_open()) { c->rc.status = INK_RC_REJECTED; return; }
    const char *id = c->cap->id;
    if (!strcmp(id, "shell.time_step")) {
        int64_t i = 0, d = 0; ink_args_get_int(&c->args, "field", &i); ink_args_get_int(&c->args, "delta", &d);
        P.part[i] += (int)d;
        const int lo[] = {2002, 1, 1, 0, 0}, hi[] = {2100, 12, 31, 23, 59};
        if (P.part[i] < lo[i]) P.part[i] = hi[i];
        if (P.part[i] > hi[i]) P.part[i] = lo[i];
        int y = P.part[0], m = P.part[1], days = m == 2 ? 28 + (!(y % 4) && (y % 100 || !(y % 400))) : 30 + ((m + (m > 7)) % 2);
        if (P.part[2] > days) P.part[2] = i == 2 && d > 0 ? 1 : days;
        ink_ui_redraw(); return;
    }
    if (strstr(id, "_prev") || strstr(id, "_next")) { if (P.list) ink_ui_list_turn(P.list, strstr(id, "_next") ? 1 : -1); return; }
    int page = !strcmp(id, "shell.time_edit") ? EDIT : !strcmp(id, "shell.time_zones") ? ZONES : !strcmp(id, "shell.return") ? RETURN : TIME;
    if (page == EDIT) {
        if (ink_setting_get("time.auto")) { c->rc.status = INK_RC_REJECTED; return; }
        ink_tm_t t; ink_time_to_local(ink_time_now_s(), &t);
        int parts[] = {t.year < 2002 ? 2002 : t.year, t.month, t.day, t.hour, t.min}; memcpy(P.part, parts, sizeof parts);
    }
    memset(&P.pos, 0, sizeof P.pos); ink_ui_push(&PAGES[page], (void *)(intptr_t)page);
}
static void changed(void *ctx, int h, const char *id) { (void)ctx; (void)h; (void)id; if (current()) ink_ui_redraw(); }
int ink_time_settings_init(void) {
    static const ink_cap_t caps[] = {
        {.id = "shell.time", .title = "打开日期和时间", .effect = INK_FX_NAVIGATE, .run = navigate},
        {.id = "shell.time_edit", .title = "手动设置日期和时间", .effect = INK_FX_NAVIGATE, .run = navigate},
        {.id = "shell.time_zones", .title = "选择城市或地区", .effect = INK_FX_NAVIGATE, .run = navigate},
        {.id = "shell.return", .title = "离开多久要重新验证", .effect = INK_FX_NAVIGATE, .run = navigate},
        {.id = "shell.time_step", .title = "调整日期时间草稿", .activity = "SetDateTime", .effect = INK_FX_NAVIGATE,
         .input = "{\"type\":\"object\",\"properties\":{\"field\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":4},\"delta\":{\"type\":\"integer\",\"enum\":[-1,1]}},\"required\":[\"field\",\"delta\"]}", .run = navigate},
        {.id = "shell.time_prev", .title = "时区上一页", .activity = "SetTimeZone", .effect = INK_FX_NAVIGATE, .run = navigate},
        {.id = "shell.time_next", .title = "时区下一页", .activity = "SetTimeZone", .effect = INK_FX_NAVIGATE, .run = navigate}};
    return ink_cap_register(caps, sizeof caps / sizeof *caps) || ink_settings_watch("time.", changed, NULL, INK_EXEC_UI) ||
        ink_settings_watch("member.idle_return_s", changed, NULL, INK_EXEC_UI) || ink_state_watch("time.", changed, NULL, INK_EXEC_UI);
}
