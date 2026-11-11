/* Family settings use the existing member, settings and activity capabilities. */
/* ui-scale-runtime: dimension y,w,h Layout derives from theme rows and the available page body. */
#include <stdio.h>
#include <string.h>
#include "ink/osal.h"
#include "ink/sys/file_picker.h"
#include "ink/sys/i18n.h"
#include "ink/sys/json.h"
#include "ink/sys/members.h"
#include "ink/sys/appreg.h"
#include "ink/sys/settings.h"
#include "ink/sys/textfield.h"
#include "ink/sys/ui_list.h"
#include "ink/ui_shell.h"
#include "../lock_faces/faces.h"

enum { MEMBERS, KID, WALLPAPER, RECORDS, MEMORY, RUNTIME, KID_APPS, KID_PERIODS };
#define ROWS_MAX 80
_Static_assert(2 * 40 <= ROWS_MAX && INK_MEMBERS_MAX + 3 <= ROWS_MAX, "family rows fit (two switches per app)");
static const char *const NAMES[] = {"SetMembers", "SetKid", "Wallpaper", "Records", "AiMemory", "RuntimeRecords", "SetKidApps", "SetKidPeriods"};
static const char *const TITLES[] = {TR_NOOP("家人"), TR_NOOP("孩子的设置"), TR_NOOP("锁屏和壁纸"), TR_NOOP("记录"), TR_NOOP("AI 记得的事"), TR_NOOP("运行记录"), TR_NOOP("应用名单"), TR_NOOP("禁用时段")};
static struct {
    int page, count, period; char kid[16];
    uint32_t generation;
    lv_obj_t *body, *field, *role;
    bool adult;
    ink_ui_list_t *list;
    ink_ui_list_position_t pos;
    char target[16], titles[ROWS_MAX][96], values[ROWS_MAX][160], ids[ROWS_MAX][48], caps[ROWS_MAX][48], args[ROWS_MAX][192];
    ink_ui_field_spec_t input[2];
    ink_ui_action_spec_t form[5];
    ink_ui_page_spec_t model;
} S INK_PSRAM_BSS;
static bool current(void) { return S.body && ink_ui_page_current() && !strcmp(ink_ui_page_name(), NAMES[S.page]); }
static void row(const char *id, const char *title, const char *value, const char *cap, const char *args) {
    int i = S.count++;
    snprintf(S.ids[i], sizeof S.ids[i], "%s", id); snprintf(S.titles[i], sizeof S.titles[i], "%s", title);
    snprintf(S.values[i], sizeof S.values[i], "%s", value ? value : "");
    snprintf(S.caps[i], sizeof S.caps[i], "%s", cap ? cap : ""); snprintf(S.args[i], sizeof S.args[i], "%s", args ? args : "{}");
}
static void setting(const char *id, const char *title, int64_t next) {
    char args[128], value[48]; snprintf(args, sizeof args, "{\"id\":\"%s\",\"value\":%lld}", id, (long long)next);
    int64_t v = ink_setting_get(id);
    snprintf(value, sizeof value, "%lld", (long long)v);
    if (!strcmp(id, "power.lock_after_s")) snprintf(value, sizeof value, TR("%lld 分钟"), (long long)v / 60);
    if (!strcmp(id, "display.wallpaper_file")) snprintf(value, sizeof value, "%s", v ? TR("已选择") : TR("还没有选壁纸"));
    if (!strcmp(id, "power.lift_wake")) snprintf(value, sizeof value, "%s", v ? TR("开，平放不动 3 分钟先锁屏") : TR("关"));
    row(id, title, value, "settings.set", args);
}
static void time_reply(void *ctx, const ink_receipt_t *rc) {
    if ((uint32_t)(uintptr_t)ctx != S.generation || !current() || S.page != KID) return;
    ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL); int64_t minutes = 0; char id[16];
    if (j && rc->result && !ink_json_parse(j, rc->result, strlen(rc->result)) && ink_json_int(j, ink_json_find(j, 0, "minutes"), &minutes)) {
        ink_json_str(j, ink_json_find(j, 0, "id"), id, sizeof id);
        if (!minutes) snprintf(S.values[2], sizeof S.values[2], TR("不限"));
        else snprintf(S.values[2], sizeof S.values[2], minutes % 60 ? TR("%lld 分钟") : TR("%lld 小时"), (long long)(minutes % 60 ? minutes : minutes / 60));
        snprintf(S.args[2], sizeof S.args[2], "{\"id\":\"%s\",\"minutes\":%d}", id, minutes == 0 ? 30 : minutes <= 30 ? 60 : minutes <= 60 ? 120 : 0);
        snprintf(S.caps[2], sizeof S.caps[2], "member.time_set"); ink_ui_list_refresh(S.list, S.count);
    }
    osal_free(j);
}
static void time_changed(void *ctx, const char *id, const ink_args_t *args) {
    (void)ctx; (void)id; (void)args;
    if (current() && (S.page == KID || S.page == KID_APPS || S.page == KID_PERIODS)) ink_ui_redraw();
}
static void bind_row(lv_obj_t *slot, size_t i, ink_ui_field_spec_t *f, ink_ui_action_spec_t *a, void *ctx) {
    (void)ctx;
    *f = (ink_ui_field_spec_t){.id = S.ids[i], .role = "text", .label = S.titles[i], .value = S.values[i], .object = {S.ids[i], 0}};
    *a = (ink_ui_action_spec_t){.id = S.ids[i], .label = S.titles[i], .capability = S.caps[i], .args_json = S.args[i], .object = f->object, .enabled = S.caps[i][0]};
    lv_obj_clean(slot); ink_ui_surface_style(slot, INK_SURFACE_STATIC, false);
    int w = INK_CANVAS_W - 2 * INK_MARGIN;
    if ((S.page == KID_APPS || S.page == KID_PERIODS) && S.caps[i][0] && strcmp(S.ids[i], "period")) {
        f->role = "switch";
        ink_ui_toggle(slot, S.titles[i], !strcmp(S.values[i], TR("开")), INK_GAP_L, (ink_ui_row_double() - INK_CONTROL) / 2, w - 2 * INK_GAP_L, NULL, NULL);
        return;
    }
    ink_ui_line(slot, S.titles[i], INK_FONT_LIST, INK_WEIGHT_MEDIUM, INK_GAP_L, INK_GAP_M, w - 2 * INK_GAP_L);
    ink_ui_line(slot, S.values[i], INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_GAP_L, INK_GAP_M + ink_ui_line_height(INK_FONT_LIST), w - 2 * INK_GAP_L);
}
static struct {
    char member[16], args[64], child_args[64], count[48];
    lv_obj_t *count_label;
    ink_ui_field_spec_t fields[6];
    ink_ui_action_spec_t actions[7];
    ink_ui_page_spec_t model;
} R INK_PSRAM_BSS;
static const char RETENTION[] = TR_NOOP("活动记录：大人 1 年，孩子 180 天。");
static const char DELETE_NOTE[] = TR_NOOP("其余记录暂不能删除。");
static void records_reply(void *ctx, const ink_receipt_t *rc) {
    if ((uint32_t)(uintptr_t)ctx != S.generation || !current() || S.page != RECORDS) return;
    ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL); int64_t total = 0;
    bool ok = j && rc->status == INK_RC_DONE && rc->result && !ink_json_parse(j, rc->result, strlen(rc->result)) &&
        ink_json_int(j, ink_json_find(j, 0, "total"), &total);
    if (ok) snprintf(R.count, sizeof R.count, TR("%lld 件"), (long long)total);
    else snprintf(R.count, sizeof R.count, TR("记录暂时读不出来"));
    osal_free(j); lv_label_set_text(R.count_label, R.count); ink_ui_content_changed(); ink_ui_hint_obj(S.body, INK_REFRESH_UI);
}
static void records_load(void) {
    char args[96]; snprintf(args, sizeof args, "{\"member\":\"%s\",\"limit\":1}", R.member);
    ink_call_opts_t o = {.src = INK_SRC_APP, .args_json = args, .reply_exec = INK_EXEC_UI,
        .done = records_reply, .ctx = (void *)(uintptr_t)S.generation};
    if (ink_cap_call("activity.list", NULL, &o)) records_reply(o.ctx, &(ink_receipt_t){.status = INK_RC_FAILED});
}
static void records_changed(void *ctx, const char *id, const ink_args_t *args) {
    (void)ctx; (void)id; (void)args;
    if (current() && S.page == RECORDS) records_load();
}
static void records_create(void) {
    int w = INK_CANVAS_W - 2 * INK_MARGIN, y = 0, h = ink_ui_row_double();
    bool adult = ink_member_current()->role == INK_ROLE_ADULT;
    const ink_member_t *kid = NULL;
    for (int i = 0; ink_member_at(i); i++) if (ink_member_at(i)->role == INK_ROLE_CHILD && (!kid || !strcmp(R.member, ink_member_at(i)->id))) kid = ink_member_at(i);
    if (!R.member[0]) snprintf(R.member, sizeof R.member, "%s", ink_member_current()->id);
    char member[16]; memcpy(member, R.member, sizeof member);
    snprintf(R.args, sizeof R.args, "{\"member\":\"%s\"}", member);
    const char *const labels[] = {ink_member_current()->name, kid ? kid->name : TR("还没有孩子家人")};
    snprintf(R.child_args, sizeof R.child_args, "{\"member\":\"%s\"}", kid ? kid->id : "");
    lv_obj_t *tabs = ink_ui_segments(S.body, labels, adult ? 2 : 1, strcmp(R.member, ink_member_current()->id) != 0, INK_MARGIN, y, w, NULL, NULL);
    ink_shell_bind(lv_obj_get_child(tabs, 0), "shell.records", NULL);
    if (adult && kid) {
        ink_shell_bind(lv_obj_get_child(tabs, 1), "shell.records", R.child_args);
    }
    y += INK_SEGMENT + INK_GAP;
    lv_obj_t *group = ink_ui_box(S.body, INK_MARGIN, y, w, 4 * h, INK_RADIUS_CARD, INK_SURFACE_STATIC, false);
    static const char *const ids[] = {"activity", "conversations", "memory", "runtime"};
    static const char *const titles[] = {TR_NOOP("AI 替你做过的事"), TR_NOOP("和 AI 说过的话"), TR_NOOP("AI 记得的事"), TR_NOOP("运行记录")};
    static const char *const icons[] = {"ai", "message", "list", "info"};
    static const char *const caps[] = {"shell.ai_log", "shell.ai_home", "shell.ai_memory", "shell.runtime_records"};
    snprintf(R.count, sizeof R.count, TR("正在读取"));
    const char *values[] = {R.count, TR("暂时读不到次数"), TR("暂时看不到"), TR("不含内容，暂时读不到")};
    for (int i = 0; i < 4; i++) {
        lv_obj_t *r = ink_ui_box(group, 0, i * h, w, h, 0, INK_SURFACE_WHITE, false);
        lv_obj_set_style_bg_opa(r, LV_OPA_TRANSP, 0); ink_shell_bind(r, caps[i], i == 0 ? R.args : NULL);
        ink_ui_circle(r, icons[i], INK_GAP, (h - INK_CIRCLE_SMALL) / 2, INK_CIRCLE_SMALL, INK_ICON_MEDIUM, INK_SURFACE_WHITE, NULL, NULL);
        int left = INK_CIRCLE_SMALL + INK_GAP_XL, top = (h - ink_ui_line_height(INK_FONT_LIST) - ink_ui_line_height(INK_FONT_NOTE)) / 2;
        ink_ui_line(r, TR(titles[i]), INK_FONT_LIST, INK_WEIGHT_MEDIUM, left, top, w - left - INK_READING_MARGIN);
        lv_obj_t *v = ink_ui_line(r, values[i], INK_FONT_NOTE, INK_WEIGHT_REGULAR, left, top + ink_ui_line_height(INK_FONT_LIST), w - left - INK_READING_MARGIN);
        if (!i) R.count_label = v;
        ink_ui_icon(r, "next", INK_ICON_SMALL, INK_ICON_LINE, w - INK_READING_MARGIN, (h - INK_ICON_SMALL) / 2, false);
        if (i) ink_ui_group_divider(r, 0, w);
        R.fields[i] = (ink_ui_field_spec_t){.id = ids[i], .role = "text", .label = TR(titles[i]), .value = values[i]};
        R.actions[i] = (ink_ui_action_spec_t){.id = ids[i], .label = TR(titles[i]), .capability = caps[i], .args_json = i == 0 ? R.args : NULL, .enabled = true};
    }
    y += 4 * h + INK_GAP;
    ink_ui_label(S.body, TR(RETENTION), INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_MARGIN, y, w);
    ink_ui_label(S.body, TR(DELETE_NOTE), INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_MARGIN,
        y + ink_ui_text_height(TR(RETENTION), INK_FONT_NOTE, INK_WEIGHT_REGULAR, w) + INK_GAP_XS, w);
    lv_obj_t *del = ink_ui_pill(S.body, TR("删除这位家人的活动记录"), INK_MARGIN, INK_CANVAS_H - INK_STATUS_BAR - INK_PRIMARY - INK_GAP_XS - INK_MARGIN - INK_PRIMARY, w, INK_PRIMARY, false, NULL, NULL);
    ink_shell_bind(del, "activity.clear", R.args);
    R.fields[4] = (ink_ui_field_spec_t){.id = "retention", .role = "text", .label = TR(RETENTION), .value = TR(DELETE_NOTE)};
    R.fields[5] = (ink_ui_field_spec_t){.id = "member", .role = "status", .label = TR("家人"), .value = !strcmp(R.member, ink_member_current()->id) ? labels[0] : labels[1]};
    R.actions[4] = (ink_ui_action_spec_t){.id = "delete", .label = TR("删除这位家人的活动记录"), .capability = "activity.clear", .args_json = R.args, .enabled = true};
    R.actions[5] = (ink_ui_action_spec_t){.id = "mine", .label = labels[0], .capability = "shell.records", .enabled = true};
    R.actions[6] = (ink_ui_action_spec_t){.id = "children", .label = labels[1], .capability = "shell.records", .args_json = R.child_args, .enabled = adult && kid};
    R.model = (ink_ui_page_spec_t){.id = "Records", .title = TR("记录"), .fields = R.fields, .field_count = 6, .actions = R.actions, .action_count = 7};
    records_load();
}
static void entered(void *ctx, const char *value) {
    (void)ctx;
    if (!current() || !value[0]) return;
    ink_receipt_t rc; ink_cap_invoke_json(S.page == KID_PERIODS ? "family.kid_periods" : "family.save", S.page == KID_PERIODS ? "{\"edit\":\"save\"}" : NULL, INK_SRC_TOUCH, 0, &rc);
}
static void saved(void *ctx, const ink_receipt_t *rc) {
    ink_call_t *c = ctx; c->rc = *rc; c->rc.result = NULL;
    ink_cap_done(c);
}
static void changed(void *ctx, int h, const char *id) {
    (void)ctx; (void)h; (void)id;
    if (current() && !S.field) ink_ui_redraw();
}
static void members_changed(void *ctx, const char *id, const ink_args_t *args) {
    (void)ctx; (void)id; (void)args;
    if (current() && S.page == MEMBERS && (!S.field || (S.target[0] && !ink_member_exists(S.target)))) {
        ink_text_blur(0); ink_ui_redraw();
    }
}
static int snapshot(char *out, size_t cap) {
    if (!current()) return -1;
    if (S.page == RECORDS) return ink_ui_semantics_snapshot(&R.model, out, cap);
    if (S.page == RUNTIME) {
        ink_ui_field_spec_t f = {.id = "unavailable", .role = "status", .label = "运行记录", .value = "不含内容；暂时读不到运行记录，也不能删除。"};
        ink_ui_page_spec_t p = {.id = "RuntimeRecords", .title = "运行记录", .fields = &f, .field_count = 1};
        return ink_ui_semantics_snapshot(&p, out, cap);
    }
    if (S.page == KID_PERIODS && S.field) {
        int n = ink_ui_list_snapshot(S.list, out, cap);
        const char *key = ",\"time_editor\":";
        if (n < 0 || cap <= (size_t)n + strlen(key)) return -1;
        memcpy(out + n - 1, key, strlen(key)); n += (int)strlen(key) - 1;
        int more = ink_textfield_short_snapshot(S.field, out + n, cap - (size_t)n - 1);
        if (more < 0) return -1;
        memcpy(out + n + more, "}", 2); return n + more + 1;
    }
    return S.field ? ink_textfield_short_snapshot(S.field, out, cap) : ink_ui_list_snapshot(S.list, out, cap);
}
static void kid_rows(void) {
    char args[192], value[96];
    if (S.page == KID_APPS) {
        for (int i = 0; i < ink_app_count(); i++) {
            const ink_app_def_t *a = ink_app_at(i);
            if (!a->child_ok || !strcmp(a->id, "home")) continue;
            int mode = ink_member_app_mode(S.kid, a->id);
            snprintf(args, sizeof args, "{\"id\":\"%s\",\"app\":\"%s\",\"mode\":%d}", S.kid, a->id, mode ? 0 : 1);
            bool required = !strcmp(a->id, "phone") || !strcmp(a->id, "message") || !strcmp(a->id, "contacts") || !strcmp(a->id, "settings");
            row(a->id, TR(a->title), required ? TR("始终可用") : mode ? TR("开") : TR("关"), required ? NULL : "kid.app_set", args);
            if (mode && !required) {
                char id[48]; snprintf(id, sizeof id, "%s.unlimited", a->id);
                snprintf(args, sizeof args, "{\"id\":\"%s\",\"app\":\"%s\",\"mode\":%d}", S.kid, a->id, mode == 2 ? 1 : 2);
                row(id, TR("禁用时段也能用"), mode == 2 ? TR("开") : TR("关"), "kid.app_set", args);
            }
        }
        return;
    }
    ink_kid_period_t p; ink_member_period(S.kid, S.period, &p);
    if (!p.days && p.start == p.end) { p.start = 21 * 60; p.end = 7 * 60; }
    snprintf(value, sizeof value, TR("时段 %d · %02u:%02u—%s%02u:%02u"), S.period + 1, p.start / 60, p.start % 60, p.end <= p.start ? TR("次日") : "", p.end / 60, p.end % 60);
    snprintf(args, sizeof args, "{\"index\":%d}", (S.period + 1) % INK_KID_PERIODS);
    row("period", TR("选择时段"), value, "family.kid_periods", args);
    static const char *const labels[] = {TR_NOOP("启用时段"), TR_NOOP("星期日"), TR_NOOP("星期一"), TR_NOOP("星期二"), TR_NOOP("星期三"), TR_NOOP("星期四"), TR_NOOP("星期五"), TR_NOOP("星期六")};
    _Static_assert(sizeof labels / sizeof *labels + 1 <= ROWS_MAX, "period rows fit");
    for (unsigned i = 0; i < sizeof labels / sizeof *labels; i++) {
        ink_kid_period_t next = p;
        snprintf(value, sizeof value, "%s", (!i ? p.days != 0 : (p.days & (1u << (i - 1))) != 0) ? TR("开") : TR("关"));
        next.days = !i ? (p.days ? 0 : 127) : (uint8_t)(p.days ^ (1u << (i - 1)));
        snprintf(args, sizeof args, "{\"id\":\"%s\",\"index\":%d,\"start\":%u,\"end\":%u,\"days\":%u}", S.kid, S.period, next.start, next.end, next.days);
        row(labels[i], TR(labels[i]), value, "kid.period_set", args);
    }
}
static void create(lv_obj_t *screen, void *arg) {
    S.page = (int)(intptr_t)arg; S.generation++; S.count = 0; S.field = NULL;
    memset(&S.pos, 0, sizeof S.pos);
    ink_shell_statusbar(screen, true); ink_shell_topbar(screen, TR(TITLES[S.page]));
    int y = INK_STATUS_BAR + INK_PRIMARY + INK_GAP_XS, h = INK_CANVAS_H - y, w = INK_CANVAS_W - 2 * INK_MARGIN;
    S.body = ink_ui_box(screen, 0, y, INK_CANVAS_W, h, 0, INK_SURFACE_WHITE, false);
    if (S.page == RECORDS) { S.list = NULL; records_create(); return; }
    if (S.page == RUNTIME) { S.list = NULL; ink_ui_notice(S.body, TR("不含内容；暂时读不到运行记录，也不能删除。"), INK_GAP_L); return; }
    static const ink_ui_action_spec_t actions[] = {
        {.id = "prev", .label = TR_NOOP("上一页"), .capability = "family.prev"}, {.id = "next", .label = TR_NOOP("下一页"), .capability = "family.next"},
        {.id = "kid", .label = TR_NOOP("孩子的设置"), .capability = "shell.kid_settings", .enabled = true}};
    S.model = (ink_ui_page_spec_t){.id = NAMES[S.page], .title = TR(TITLES[S.page]), .object = {NAMES[S.page], 0}, .actions = actions, .action_count = S.page == MEMBERS ? 3 : 2};
    lv_obj_t *list_body = S.body;
    if (S.page == KID_APPS || S.page == KID_PERIODS) {
        const ink_member_t *kid = NULL;
        for (int i = 0; ink_member_at(i); i++) if (!strcmp(ink_member_at(i)->id, S.kid)) kid = ink_member_at(i);
        ink_ui_line(S.body, kid ? kid->name : "", INK_FONT_LIST, INK_WEIGHT_MEDIUM, INK_MARGIN, 0, w);
        int top = ink_ui_line_height(INK_FONT_LIST) + INK_GAP;
        ink_ui_line(S.body, TR("电话、短信、联系人不限时"), INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_MARGIN, top, w);
        top += ink_ui_line_height(INK_FONT_NOTE) + INK_GAP;
        if (S.page == KID_PERIODS) {
            ink_kid_period_t p; ink_member_period(S.kid, S.period, &p);
            if (!p.days && p.start == p.end) { p.start = 21 * 60; p.end = 7 * 60; }
            snprintf(S.values[0], sizeof S.values[0], "%02u:%02u-%02u:%02u", p.start / 60, p.start % 60, p.end / 60, p.end % 60);
            ink_text_cfg_t cfg = {.type = INK_TEXT_PLAIN, .initial = S.values[0], .max = 11};
            S.field = ink_textfield_create(S.body, &cfg, entered, NULL, false); lv_obj_set_pos(S.field, ink_ui_px(INK_MARGIN), ink_ui_px(top));
            S.input[0] = (ink_ui_field_spec_t){.id = "time", .role = "textbox", .label = TR("开始—结束（24 小时）")};
            S.form[0] = (ink_ui_action_spec_t){.id = "time", .label = S.input[0].label, .capability = "family.kid_periods", .args_json = "{\"edit\":\"focus\"}", .enabled = true};
            S.form[1] = (ink_ui_action_spec_t){.id = "save", .label = TR("保存时段"), .capability = "family.kid_periods", .args_json = "{\"edit\":\"save\"}", .enabled = true};
            S.model.fields = S.input; S.model.field_count = 1; S.model.actions = S.form; S.model.action_count = 2;
            ink_textfield_bind_short(S.field, &S.model, &S.input[0], &S.form[0]); ink_textfield_style(S.field, INK_FIELD_SINGLE, S.input[0].label);
            top += ink_textfield_height(INK_FIELD_SINGLE) + INK_GAP;
            ink_shell_bind(ink_ui_pill(S.body, S.form[1].label, INK_MARGIN, top, w, INK_CONTROL, true, NULL, NULL), S.form[1].capability, S.form[1].args_json);
            top += INK_CONTROL + INK_GAP_L;
        }
        list_body = ink_ui_box(S.body, 0, top, INK_CANVAS_W, h - top, 0, INK_SURFACE_WHITE, false);
    }
    ink_ui_page_spec_t list_model = S.model; list_model.actions = actions; list_model.action_count = S.page == MEMBERS ? 3 : 2;
    S.list = ink_ui_list_create(list_body, &list_model, &S.pos, S.page == KID_PERIODS ? 2 : 4, 1, ink_ui_row_double(), TR("这里还没有内容"), bind_row, NULL);
    char args[128];
    if (S.page == KID_APPS || S.page == KID_PERIODS) kid_rows();
    else if (S.page == MEMBERS) {
        for (int i = 0; ink_member_at(i); i++) {
            const ink_member_t *m = ink_member_at(i); snprintf(args, sizeof args, "{\"id\":\"%s\"}", m->id);
            row(m->id, m->name, !strcmp(m->id, ink_member_current()->id) ? TR("正在用") : m->role == INK_ROLE_CHILD ? TR("孩子") : TR("大人"), "family.edit", args);
        }
        row("add", TR("加一位家人"), "", "family.edit", NULL);
        row("pin", TR("我的密码"), TR("设置或更改"), "shell.pin_set", NULL);
        row("return", TR("离开多久要重新验证"), TR("选择免密码时长"), "shell.return", NULL);
    } else if (S.page == KID) {
        const ink_member_t *kid = NULL;
        for (int i = 0; ink_member_at(i); i++) if (ink_member_at(i)->role == INK_ROLE_CHILD && (!S.kid[0] || !strcmp(S.kid, ink_member_at(i)->id))) { kid = ink_member_at(i); break; }
        if (kid) snprintf(S.kid, sizeof S.kid, "%s", kid->id);
        row("child", TR("家人"), kid ? kid->name : TR("还没有孩子家人"), "family.kid_member", NULL);
        row("apps", TR("应用名单"), kid ? TR("选择能用的应用") : TR("还没有孩子家人"), kid ? "family.kid_apps" : NULL, NULL);
        row("time", TR("每天能用多久"), kid ? TR("正在读取") : TR("还没有孩子家人"), NULL, NULL);
        if (kid) {
            snprintf(args, sizeof args, "{\"id\":\"%s\"}", kid->id);
            ink_call_opts_t o = {.src = INK_SRC_SYSTEM, .args_json = args, .reply_exec = INK_EXEC_UI, .done = time_reply, .ctx = (void *)(uintptr_t)S.generation};
            ink_cap_call("member.time_get", NULL, &o);
        }
        setting("audio.child_max", TR("孩子的音量上限"), ink_setting_get("audio.child_max") >= 100 ? 10 : ink_setting_get("audio.child_max") + 10);
        row("periods", TR("禁用时段"), kid ? TR("选择时间和星期") : TR("还没有孩子家人"), kid ? "family.kid_periods" : NULL, NULL);
    } else if (S.page == WALLPAPER) {
        row("display.lock_style", TR("锁屏时显示"), TR(ink_lock_face_choices[ink_setting_get("display.lock_style")]), "shell.lock_faces", NULL);
        setting("power.lock_after_s", TR("自动锁屏"), ink_setting_get("power.lock_after_s") == 600 ? 300 : 600);
        row("choose", TR("从相册选"), ink_wallpaper_ready() ? TR("已选锁屏壁纸") : ink_setting_get("display.wallpaper_file") ? TR("壁纸暂时读不出来") : TR("还没有选壁纸"), "wallpaper.choose", NULL);
        setting("display.wallpaper_file", TR("移除壁纸"), 0);
        setting("power.lift_wake", TR("拿起亮屏"), !ink_setting_get("power.lift_wake"));
    } else row("unavailable", TR("暂时看不到 AI 记得的事"), TR("暂时无法查看和删除记忆"), NULL, NULL);
    ink_ui_list_refresh(S.list, S.count);
    if (S.page == MEMBERS) {
        ink_shell_bind(ink_ui_pill(screen, TR("孩子的设置"), INK_MARGIN, INK_CANVAS_H - INK_MARGIN - INK_CONTROL, w, INK_CONTROL, false, NULL, NULL), "shell.kid_settings", NULL);
    }
}
static void destroy(void) { S.generation++; S.body = S.field = NULL; S.list = NULL; }
static bool key(ink_key_t k, bool lng) { return current() && (!S.field || S.page == KID_PERIODS) && ink_ui_list_key(S.list, k, lng); }
#define PAGE(name_) {.name = name_, .create = create, .destroy = destroy, .snapshot = snapshot, .on_key = key}
static const ink_activity_t PAGES[] = {PAGE("SetMembers"), PAGE("SetKid"), PAGE("Wallpaper"), PAGE("Records"), PAGE("AiMemory"), PAGE("RuntimeRecords"), PAGE("SetKidApps"), PAGE("SetKidPeriods")};
void ink_settings_family_open(const char *name) {
    if (ink_member_current()->role == INK_ROLE_CHILD || !ink_member_space_open()) return;
    for (unsigned i = 0; i < sizeof NAMES / sizeof *NAMES; i++) if (!strcmp(name, NAMES[i])) { ink_shell_close(); ink_ui_push(&PAGES[i], (void *)(intptr_t)i); return; }
}
static void action(ink_call_t *c) {
    const char *id = c->cap->id;
    char edit[8] = ""; ink_args_get_str(&c->args, "edit", edit, sizeof edit);
    if (!strcmp(id, "shell.records")) {
        char member[16]; bool children = false; snprintf(member, sizeof member, "%s", ink_member_current()->id);
        ink_args_get_str(&c->args, "member", member, sizeof member); ink_args_get_bool(&c->args, "children", &children);
        if (children) for (int i = 0; ink_member_at(i); i++) if (ink_member_at(i)->role == INK_ROLE_CHILD) { snprintf(member, sizeof member, "%s", ink_member_at(i)->id); break; }
        bool allowed = !strcmp(member, ink_member_current()->id);
        for (int i = 0; ink_member_at(i); i++) if (!strcmp(member, ink_member_at(i)->id) && ink_member_at(i)->role == INK_ROLE_CHILD) allowed = true;
        if (!allowed) { c->rc.status = INK_RC_REJECTED; return; }
        snprintf(R.member, sizeof R.member, "%s", member);
        if (current() && S.page == RECORDS) ink_ui_redraw();
        else ink_settings_family_open("Records");
    } else if (!strncmp(id, "shell.", 6)) {
        const char *const caps[] = {"shell.family", "shell.kid_settings", "shell.wallpaper", "shell.records", "shell.ai_memory", "shell.runtime_records"};
        for (int i = 0; i < 6; i++) if (!strcmp(id, caps[i])) ink_settings_family_open(NAMES[i]);
    } else if (!current()) c->rc.status = INK_RC_STALE;
    else if (!strcmp(id, "family.kid_member")) {
        int at = -1;
        for (int i = 0; ink_member_at(i); i++) if (!strcmp(ink_member_at(i)->id, S.kid)) { at = i; break; }
        for (int n = 1; n <= INK_MEMBERS_MAX; n++) {
            const ink_member_t *m = ink_member_at((at + n) % INK_MEMBERS_MAX);
            if (m && m->role == INK_ROLE_CHILD) { snprintf(S.kid, sizeof S.kid, "%s", m->id); break; }
        }
        ink_ui_redraw();
    } else if (!strcmp(id, "family.kid_apps") || !strcmp(id, "family.kid_periods")) {
        if (!strcmp(id, "family.kid_periods") && edit[0]) {
            if (S.page != KID_PERIODS || !S.field) { c->rc.status = INK_RC_STALE; return; }
            if (!strcmp(edit, "focus")) { ink_textfield_short_focus(S.field); return; }
            unsigned sh, sm, eh, em; char tail, args[192]; ink_kid_period_t p; ink_member_period(S.kid, S.period, &p);
            const char *text = ink_textfield_text(S.field);
            if (strlen(text) != 11 || sscanf(text, "%2u:%2u-%2u:%2u%c", &sh, &sm, &eh, &em, &tail) != 4 || sh > 23 || eh > 23 || sm > 59 || em > 59) {
                ink_textfield_error(S.field, TR("请按 21:00-07:00 填写")); c->rc.status = INK_RC_INVALID; return;
            }
            snprintf(args, sizeof args, "{\"id\":\"%s\",\"index\":%d,\"start\":%u,\"end\":%u,\"days\":%u}", S.kid, S.period, sh * 60 + sm, eh * 60 + em, p.days);
            ink_call_opts_t o = {.src = c->src, .args_json = args, .reply_exec = INK_EXEC_UI, .done = saved, .ctx = c}; ink_cap_defer(c);
            if (ink_cap_call("kid.period_set", NULL, &o)) { c->rc.status = INK_RC_FAILED; ink_cap_done(c); }
            return;
        }
        int64_t index = 0; ink_args_get_int(&c->args, "index", &index); S.period = (int)index;
        if (S.page == KID_PERIODS && !strcmp(id, "family.kid_periods")) ink_ui_redraw();
        else ink_settings_family_open(!strcmp(id, "family.kid_apps") ? "SetKidApps" : "SetKidPeriods");
    }
    else if (!strcmp(id, "family.focus")) { if (S.field) ink_textfield_short_focus(S.field); }
    else if (!strcmp(id, "family.save")) {
        if (!S.field || (S.target[0] && !ink_member_exists(S.target))) { c->rc.status = INK_RC_STALE; return; }
        ink_args_t args; ink_args_init(&args); char member[16];
        snprintf(member, sizeof member, "p%llx", (unsigned long long)osal_wall_us() & 0xffffffffffffull);
        ink_args_str(&args, "id", S.target[0] ? S.target : member);
        ink_args_str(&args, "name", ink_textfield_text(S.field));
        ink_args_str(&args, "role", S.adult ? "adult" : "child");
        ink_call_opts_t o = {.src = c->src, .reply_exec = INK_EXEC_UI, .done = saved, .ctx = c}; ink_cap_defer(c);
        if (ink_cap_call(S.target[0] ? "member.rename" : "member.add", &args, &o)) { c->rc.status = INK_RC_FAILED; ink_cap_done(c); }
        memset(&args, 0, sizeof args);
    } else if (!strcmp(id, "family.role")) {
        if (!S.field || S.target[0]) { c->rc.status = INK_RC_STALE; return; }
        S.adult = !S.adult; S.input[1].value = S.adult ? TR("大人") : TR("孩子"); lv_label_set_text(lv_obj_get_child(S.role, 0), S.input[1].value); ink_ui_content_changed();
    }
    else if (!strcmp(id, "family.edit")) {
        if (S.field) { c->rc.status = INK_RC_STALE; return; }
        S.target[0] = 0; ink_args_get_str(&c->args, "id", S.target, sizeof S.target);
        lv_obj_clean(S.body); S.list = NULL;
        S.form[0] = (ink_ui_action_spec_t){.id = "name", .label = TR("称呼"), .capability = "family.focus", .enabled = true};
        S.form[1] = (ink_ui_action_spec_t){.id = "save", .label = TR("保存称呼"), .capability = "family.save", .enabled = true};
        S.form[2] = (ink_ui_action_spec_t){.id = "role", .label = TR("大人或孩子"), .capability = "family.role", .enabled = !S.target[0]};
        S.form[3] = (ink_ui_action_spec_t){.id = "remove", .label = TR("移除这位家人"), .capability = "member.remove", .enabled = S.target[0]};
        S.form[4] = (ink_ui_action_spec_t){.id = "pin", .label = TR("改我的密码"), .capability = "shell.pin_set", .enabled = true};
        S.input[0] = (ink_ui_field_spec_t){.id = "name", .role = "textbox", .label = TR("称呼")};
        S.input[1] = (ink_ui_field_spec_t){.id = "role", .role = "status", .label = TR("新家人的身份"), .value = TR("孩子")};
        S.model.fields = S.input; S.model.field_count = 1; S.model.actions = S.form; S.model.action_count = 5; S.adult = false;
        if (!S.target[0]) S.model.field_count = 2;
        ink_text_cfg_t cfg = {.type = INK_TEXT_PLAIN, .max = 31, .hint = TR("输入称呼，按完成保存")};
        S.field = ink_textfield_create(S.body, &cfg, entered, NULL, false);
        lv_obj_set_pos(S.field, ink_ui_px(INK_MARGIN), ink_ui_px(INK_GAP_L));
        ink_textfield_bind_short(S.field, &S.model, &S.input[0], &S.form[0]);
        ink_textfield_style(S.field, INK_FIELD_SINGLE, S.input[0].label);
        snprintf(S.args[0], sizeof S.args[0], "{\"id\":\"%s\"}", S.target); S.form[3].args_json = S.args[0];
        for (int i = 1; i < 5; i++) if (S.form[i].enabled) {
            lv_obj_t *b = ink_ui_pill(S.body, i == 2 ? TR("孩子") : S.form[i].label, INK_MARGIN, (i - 1) * (INK_CONTROL + INK_GAP) + ink_textfield_height(INK_FIELD_SINGLE) + INK_GAP + INK_GAP_L, INK_CANVAS_W - 2 * INK_MARGIN, INK_CONTROL, false, NULL, NULL);
            ink_shell_bind(b, S.form[i].capability, S.form[i].args_json); if (i == 2) S.role = b;
        }
    } else if (S.list && (!S.field || S.page == KID_PERIODS)) {
        bool next = !strcmp(id, "family.next");
        ink_ui_list_turn(S.list, next ? 1 : -1);
    }
    else c->rc.status = INK_RC_STALE;
}
int ink_settings_family_init(void) {
    static const ink_cap_t caps[] = {
        {.id = "family.kid_apps", .title = "打开孩子的应用名单", .effect = INK_FX_NAVIGATE, .run = action},
        {.id = "family.kid_periods", .title = "打开孩子的禁用时段", .input = "{\"type\":\"object\",\"properties\":{\"index\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":3},\"edit\":{\"type\":\"string\",\"enum\":[\"focus\",\"save\"]}}}", .effect = INK_FX_NAVIGATE, .run = action},
        {.id = "family.kid_member", .title = "选择下一个孩子的设置", .effect = INK_FX_NAVIGATE, .run = action},
        {.id = "shell.family", .title = "管理家人", .effect = INK_FX_NAVIGATE, .run = action},
        {.id = "shell.kid_settings", .title = "打开孩子的设置", .effect = INK_FX_NAVIGATE, .run = action},
        {.id = "shell.wallpaper", .title = "打开锁屏和壁纸", .effect = INK_FX_NAVIGATE, .run = action},
        {.id = "shell.records", .title = "查看使用记录", .effect = INK_FX_NAVIGATE, .run = action,
         .input = "{\"type\":\"object\",\"properties\":{\"member\":{\"type\":\"string\",\"maxLength\":15},\"children\":{\"type\":\"boolean\"}}}"},
        {.id = "shell.runtime_records", .title = "查看运行记录", .effect = INK_FX_NAVIGATE, .run = action},
        {.id = "shell.ai_memory", .app = "ai", .title = "查看 AI 记得的事", .effect = INK_FX_NAVIGATE, .run = action},
        {.id = "family.edit", .title = "编辑家人的称呼或添加家人", .activity = "SetMembers", .effect = INK_FX_NAVIGATE, .input = "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\",\"maxLength\":15}}}", .run = action},
        {.id = "family.focus", .title = "输入家人的称呼", .activity = "SetMembers", .effect = INK_FX_NAVIGATE, .run = action},
        {.id = "family.save", .title = "保存家人的称呼", .activity = "SetMembers", .effect = INK_FX_LOCAL_WRITE, .run = action},
        {.id = "family.role", .title = "选择添加大人或孩子", .activity = "SetMembers", .effect = INK_FX_NAVIGATE, .run = action},
        {.id = "family.prev", .title = "家人设置上一页", .effect = INK_FX_NAVIGATE, .run = action},
        {.id = "family.next", .title = "家人设置下一页", .effect = INK_FX_NAVIGATE, .run = action}};
    return ink_cap_register(caps, sizeof caps / sizeof *caps) || ink_settings_watch("", changed, NULL, INK_EXEC_UI) ||
        ink_cap_subscribe("member.changed", members_changed, NULL, INK_EXEC_UI) ||
        ink_cap_subscribe("confirm.result", time_changed, NULL, INK_EXEC_UI) ||
        ink_cap_subscribe("confirm.result", records_changed, NULL, INK_EXEC_UI) ? -1 : 0;
}
