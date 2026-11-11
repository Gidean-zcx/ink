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

enum { MEMBERS, KID, WALLPAPER, RECORDS, MEMORY, RUNTIME, KID_APPS, KID_PERIODS };
#define ROWS_MAX 48
_Static_assert(40 <= ROWS_MAX && INK_MEMBERS_MAX + 3 <= ROWS_MAX, "family rows fit (members, add, pin, return)");
static const char *const NAMES[] = {"SetMembers", "SetKid", "Wallpaper", "Records", "AiMemory", "RuntimeRecords", "SetKidApps", "SetKidPeriods"};
static const char *const TITLES[] = {"家人", "孩子的设置", "锁屏和壁纸", "记录", "AI 记得的事", "运行记录", TR_NOOP("应用名单"), TR_NOOP("禁用时段")};
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
    if (!strcmp(id, "display.lock_style")) snprintf(value, sizeof value, "%s", v == 2 ? "壁纸" : v == 1 ? "时钟和待办" : "这一页");
    if (!strcmp(id, "power.lock_after_s")) snprintf(value, sizeof value, "%lld 分钟", (long long)v / 60);
    if (!strcmp(id, "display.wallpaper_file")) snprintf(value, sizeof value, "%s", v ? "已选择" : "还没有选壁纸");
    if (!strcmp(id, "power.lift_wake")) snprintf(value, sizeof value, "%s", v ? "开，平放不动 3 分钟先锁屏" : "关");
    row(id, title, value, "settings.set", args);
}
static void time_reply(void *ctx, const ink_receipt_t *rc) {
    if ((uint32_t)(uintptr_t)ctx != S.generation || !current() || S.page != KID) return;
    ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL); int64_t minutes = 0; char id[16];
    if (j && rc->result && !ink_json_parse(j, rc->result, strlen(rc->result)) && ink_json_int(j, ink_json_find(j, 0, "minutes"), &minutes)) {
        ink_json_str(j, ink_json_find(j, 0, "id"), id, sizeof id);
        if (!minutes) snprintf(S.values[2], sizeof S.values[2], "不限");
        else snprintf(S.values[2], sizeof S.values[2], minutes % 60 ? "%lld 分钟" : "%lld 小时", (long long)(minutes % 60 ? minutes : minutes / 60));
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
    ink_ui_line(slot, S.titles[i], INK_FONT_LIST, INK_WEIGHT_MEDIUM, INK_GAP_L, INK_GAP_M, w - 2 * INK_GAP_L);
    ink_ui_line(slot, S.values[i], INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_GAP_L, INK_GAP_M + ink_ui_line_height(INK_FONT_LIST), w - 2 * INK_GAP_L);
}
static struct {
    char member[16], args[64], count[48];
    lv_obj_t *count_label;
    ink_ui_field_spec_t fields[6];
    ink_ui_action_spec_t actions[7];
    ink_ui_page_spec_t model;
} R INK_PSRAM_BSS;
static const char RETENTION[] = "活动记录：大人 1 年，孩子 180 天。";
static const char DELETE_NOTE[] = "其余记录暂不能删除。";
static void records_reply(void *ctx, const ink_receipt_t *rc) {
    if ((uint32_t)(uintptr_t)ctx != S.generation || !current() || S.page != RECORDS) return;
    ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL); int64_t total = 0;
    bool ok = j && rc->status == INK_RC_DONE && rc->result && !ink_json_parse(j, rc->result, strlen(rc->result)) &&
        ink_json_int(j, ink_json_find(j, 0, "total"), &total);
    if (ok) snprintf(R.count, sizeof R.count, "%lld 件", (long long)total);
    else snprintf(R.count, sizeof R.count, "记录暂时读不出来");
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
    for (int i = 0; ink_member_at(i); i++) if (ink_member_at(i)->role == INK_ROLE_CHILD) { kid = ink_member_at(i); break; }
    if (!R.member[0]) snprintf(R.member, sizeof R.member, "%s", ink_member_current()->id);
    char member[16]; memcpy(member, R.member, sizeof member);
    snprintf(R.args, sizeof R.args, "{\"member\":\"%s\"}", member);
    const char *const labels[] = {"我", "孩子"};
    lv_obj_t *tabs = ink_ui_segments(S.body, labels, adult ? 2 : 1, strcmp(R.member, ink_member_current()->id) != 0, INK_MARGIN, y, w, NULL, NULL);
    ink_shell_bind(lv_obj_get_child(tabs, 0), "shell.records", NULL);
    if (adult && kid) {
        char args[64]; snprintf(args, sizeof args, "{\"member\":\"%s\"}", kid->id);
        ink_shell_bind(lv_obj_get_child(tabs, 1), "shell.records", args);
    }
    y += INK_SEGMENT + INK_GAP;
    lv_obj_t *group = ink_ui_box(S.body, INK_MARGIN, y, w, 4 * h, INK_RADIUS_CARD, INK_SURFACE_STATIC, false);
    static const char *const ids[] = {"activity", "conversations", "memory", "runtime"};
    static const char *const titles[] = {"AI 替你做过的事", "和 AI 说过的话", "AI 记得的事", "运行记录"};
    static const char *const icons[] = {"ai", "message", "list", "info"};
    static const char *const caps[] = {"shell.ai_log", "shell.ai_home", "shell.ai_memory", "shell.runtime_records"};
    snprintf(R.count, sizeof R.count, "正在读取");
    const char *values[] = {R.count, "暂时读不到次数", "暂时看不到", "不含内容，暂时读不到"};
    for (int i = 0; i < 4; i++) {
        lv_obj_t *r = ink_ui_box(group, 0, i * h, w, h, 0, INK_SURFACE_WHITE, false);
        lv_obj_set_style_bg_opa(r, LV_OPA_TRANSP, 0); ink_shell_bind(r, caps[i], i == 0 ? R.args : NULL);
        ink_ui_circle(r, icons[i], INK_GAP, (h - INK_CIRCLE_SMALL) / 2, INK_CIRCLE_SMALL, INK_ICON_MEDIUM, INK_SURFACE_WHITE, NULL, NULL);
        int left = INK_CIRCLE_SMALL + INK_GAP_XL, top = (h - ink_ui_line_height(INK_FONT_LIST) - ink_ui_line_height(INK_FONT_NOTE)) / 2;
        ink_ui_line(r, titles[i], INK_FONT_LIST, INK_WEIGHT_MEDIUM, left, top, w - left - INK_READING_MARGIN);
        lv_obj_t *v = ink_ui_line(r, values[i], INK_FONT_NOTE, INK_WEIGHT_REGULAR, left, top + ink_ui_line_height(INK_FONT_LIST), w - left - INK_READING_MARGIN);
        if (!i) R.count_label = v;
        ink_ui_icon(r, "next", INK_ICON_SMALL, INK_ICON_LINE, w - INK_READING_MARGIN, (h - INK_ICON_SMALL) / 2, false);
        if (i) ink_ui_group_divider(r, 0, w);
        R.fields[i] = (ink_ui_field_spec_t){.id = ids[i], .role = "text", .label = titles[i], .value = values[i]};
        R.actions[i] = (ink_ui_action_spec_t){.id = ids[i], .label = titles[i], .capability = caps[i], .args_json = i == 0 ? R.args : NULL, .enabled = true};
    }
    y += 4 * h + INK_GAP;
    ink_ui_label(S.body, RETENTION, INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_MARGIN, y, w);
    ink_ui_label(S.body, DELETE_NOTE, INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_MARGIN,
        y + ink_ui_text_height(RETENTION, INK_FONT_NOTE, INK_WEIGHT_REGULAR, w) + INK_GAP_XS, w);
    lv_obj_t *del = ink_ui_pill(S.body, "全部删除", INK_MARGIN, INK_CANVAS_H - INK_STATUS_BAR - INK_PRIMARY - INK_GAP_XS - INK_MARGIN - INK_PRIMARY, w, INK_PRIMARY, false, NULL, NULL);
    ink_shell_bind(del, "activity.clear", R.args);
    R.fields[4] = (ink_ui_field_spec_t){.id = "retention", .role = "text", .label = RETENTION, .value = DELETE_NOTE};
    R.fields[5] = (ink_ui_field_spec_t){.id = "member", .role = "status", .label = "家人", .value = R.member};
    R.actions[4] = (ink_ui_action_spec_t){.id = "delete", .label = "全部删除", .capability = "activity.clear", .args_json = R.args, .enabled = true};
    R.actions[5] = (ink_ui_action_spec_t){.id = "mine", .label = "我", .capability = "shell.records", .enabled = true};
    R.actions[6] = (ink_ui_action_spec_t){.id = "children", .label = "孩子", .capability = "shell.records", .args_json = "{\"children\":true}", .enabled = adult && kid};
    R.model = (ink_ui_page_spec_t){.id = "Records", .title = "记录", .fields = R.fields, .field_count = 6, .actions = R.actions, .action_count = 7};
    records_load();
}
static void entered(void *ctx, const char *value) {
    (void)ctx;
    if (!current() || !value[0]) return;
    ink_receipt_t rc; ink_cap_invoke("family.save", NULL, INK_SRC_TOUCH, 0, &rc);
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
    return S.field ? ink_textfield_short_snapshot(S.field, out, cap) : ink_ui_list_snapshot(S.list, out, cap);
}
static void kid_rows(void) {
    char args[192], value[96];
    if (S.page == KID_APPS) {
        for (int i = 0; i < ink_app_count(); i++) {
            const ink_app_def_t *a = ink_app_at(i);
            if (!a->child_ok || !strcmp(a->id, "home")) continue;
            int mode = ink_member_app_mode(S.kid, a->id);
            snprintf(args, sizeof args, "{\"id\":\"%s\",\"app\":\"%s\",\"mode\":%d}", S.kid, a->id, (mode + 1) % 3);
            bool required = !strcmp(a->id, "phone") || !strcmp(a->id, "message") || !strcmp(a->id, "settings");
            row(a->id, TR(a->title), mode == 2 ? TR("禁用时段也能用") : mode ? TR("可用") : TR("不开放"), required ? NULL : "kid.app_set", args);
        }
        return;
    }
    ink_kid_period_t p; ink_member_period(S.kid, S.period, &p);
    if (!p.days && p.start == p.end) { p.start = 21 * 60; p.end = 7 * 60; }
    snprintf(value, sizeof value, TR("时段 %d，共 %d 段"), S.period + 1, INK_KID_PERIODS);
    snprintf(args, sizeof args, "{\"index\":%d}", (S.period + 1) % INK_KID_PERIODS);
    row("period", TR("选择时段"), value, "family.kid_periods", args);
    static const char *const labels[] = {TR_NOOP("开始时间"), TR_NOOP("结束时间"), TR_NOOP("启用时段"), TR_NOOP("星期日"), TR_NOOP("星期一"), TR_NOOP("星期二"), TR_NOOP("星期三"), TR_NOOP("星期四"), TR_NOOP("星期五"), TR_NOOP("星期六")};
    _Static_assert(sizeof labels / sizeof *labels + 1 <= ROWS_MAX, "period rows fit");
    for (unsigned i = 0; i < sizeof labels / sizeof *labels; i++) {
        ink_kid_period_t next = p;
        if (i < 2) {
            unsigned minute = i ? p.end : p.start;
            snprintf(value, sizeof value, "%02u:%02u", minute / 60, minute % 60);
            if (i) next.end = (uint16_t)((p.end + 30) % 1440);
            else next.start = (uint16_t)((p.start + 30) % 1440);
        } else {
            snprintf(value, sizeof value, "%s", (i == 2 ? p.days != 0 : (p.days & (1u << (i - 3))) != 0) ? TR("开") : TR("关"));
            next.days = i == 2 ? (p.days ? 0 : 127) : (uint8_t)(p.days ^ (1u << (i - 3)));
        }
        snprintf(args, sizeof args, "{\"id\":\"%s\",\"index\":%d,\"start\":%u,\"end\":%u,\"days\":%u}", S.kid, S.period, next.start, next.end, next.days);
        row(labels[i], TR(labels[i]), value, "kid.period_set", args);
    }
}
static void create(lv_obj_t *screen, void *arg) {
    S.page = (int)(intptr_t)arg; S.generation++; S.count = 0; S.field = NULL;
    memset(&S.pos, 0, sizeof S.pos);
    ink_shell_statusbar(screen, true); ink_shell_topbar(screen, S.page >= KID_APPS ? TR(TITLES[S.page]) : TITLES[S.page]);
    int y = INK_STATUS_BAR + INK_PRIMARY + INK_GAP_XS, h = INK_CANVAS_H - y, w = INK_CANVAS_W - 2 * INK_MARGIN;
    S.body = ink_ui_box(screen, 0, y, INK_CANVAS_W, h, 0, INK_SURFACE_WHITE, false);
    if (S.page == RECORDS) { S.list = NULL; records_create(); return; }
    if (S.page == RUNTIME) { S.list = NULL; ink_ui_notice(S.body, "不含内容；暂时读不到运行记录，也不能删除。", INK_GAP_L); return; }
    static const ink_ui_action_spec_t actions[] = {
        {.id = "prev", .label = "上一页", .capability = "family.prev"}, {.id = "next", .label = "下一页", .capability = "family.next"},
        {.id = "kid", .label = "孩子的设置", .capability = "shell.kid_settings", .enabled = true}};
    S.model = (ink_ui_page_spec_t){.id = NAMES[S.page], .title = S.page >= KID_APPS ? TR(TITLES[S.page]) : TITLES[S.page], .object = {NAMES[S.page], 0}, .actions = actions, .action_count = S.page == MEMBERS ? 3 : 2};
    S.list = ink_ui_list_create(S.body, &S.model, &S.pos, 4, 1, ink_ui_row_double(), "这里还没有内容", bind_row, NULL);
    char args[128];
    if (S.page == KID_APPS || S.page == KID_PERIODS) kid_rows();
    else if (S.page == MEMBERS) {
        for (int i = 0; ink_member_at(i); i++) {
            const ink_member_t *m = ink_member_at(i); snprintf(args, sizeof args, "{\"id\":\"%s\"}", m->id);
            row(m->id, m->name, !strcmp(m->id, ink_member_current()->id) ? "正在用" : m->role == INK_ROLE_CHILD ? "孩子" : "大人", "family.edit", args);
        }
        row("add", "加一位家人", "", "family.edit", NULL);
        row("pin", "我的密码", "设置或更改", "shell.pin_set", NULL);
        row("return", TR("离开多久要重新验证"), TR("选择免密码时长"), "shell.return", NULL);
    } else if (S.page == KID) {
        const ink_member_t *kid = NULL;
        for (int i = 0; ink_member_at(i); i++) if (ink_member_at(i)->role == INK_ROLE_CHILD && (!S.kid[0] || !strcmp(S.kid, ink_member_at(i)->id))) { kid = ink_member_at(i); break; }
        if (kid) snprintf(S.kid, sizeof S.kid, "%s", kid->id);
        row("child", "家人", kid ? kid->name : "还没有孩子家人", "family.kid_member", NULL);
        row("apps", TR("应用名单"), kid ? TR("选择能用的应用") : "还没有孩子家人", kid ? "family.kid_apps" : NULL, NULL);
        row("time", "每天能用多久", kid ? "正在读取" : "还没有孩子家人", NULL, NULL);
        if (kid) {
            snprintf(args, sizeof args, "{\"id\":\"%s\"}", kid->id);
            ink_call_opts_t o = {.src = INK_SRC_SYSTEM, .args_json = args, .reply_exec = INK_EXEC_UI, .done = time_reply, .ctx = (void *)(uintptr_t)S.generation};
            ink_cap_call("member.time_get", NULL, &o);
        }
        setting("audio.child_max", "孩子的音量上限", ink_setting_get("audio.child_max") >= 100 ? 10 : ink_setting_get("audio.child_max") + 10);
        row("periods", TR("禁用时段"), kid ? TR("选择时间和星期") : "还没有孩子家人", kid ? "family.kid_periods" : NULL, NULL);
    } else if (S.page == WALLPAPER) {
        setting("display.lock_style", "锁屏时显示", (ink_setting_get("display.lock_style") + 1) % 3);
        setting("power.lock_after_s", "自动锁屏", ink_setting_get("power.lock_after_s") == 600 ? 300 : 600);
        row("choose", "从相册选", ink_wallpaper_ready() ? "已选锁屏壁纸" : ink_setting_get("display.wallpaper_file") ? "壁纸暂时读不出来" : "还没有选壁纸", "wallpaper.choose", NULL);
        setting("display.wallpaper_file", "移除壁纸", 0);
        setting("power.lift_wake", "拿起亮屏", !ink_setting_get("power.lift_wake"));
    } else row("unavailable", "暂时看不到 AI 记得的事", "暂时无法查看和删除记忆", NULL, NULL);
    ink_ui_list_refresh(S.list, S.count);
    if (S.page == MEMBERS) {
        ink_shell_bind(ink_ui_pill(screen, "孩子的设置", INK_MARGIN, INK_CANVAS_H - INK_MARGIN - INK_CONTROL, w, INK_CONTROL, false, NULL, NULL), "shell.kid_settings", NULL);
    }
}
static void destroy(void) { S.generation++; S.body = S.field = NULL; S.list = NULL; }
static bool key(ink_key_t k, bool lng) { return current() && !S.field && ink_ui_list_key(S.list, k, lng); }
#define PAGE(name_) {.name = name_, .create = create, .destroy = destroy, .snapshot = snapshot, .on_key = key}
static const ink_activity_t PAGES[] = {PAGE("SetMembers"), PAGE("SetKid"), PAGE("Wallpaper"), PAGE("Records"), PAGE("AiMemory"), PAGE("RuntimeRecords"), PAGE("SetKidApps"), PAGE("SetKidPeriods")};
void ink_settings_family_open(const char *name) {
    if (ink_member_current()->role == INK_ROLE_CHILD || !ink_member_space_open()) return;
    for (unsigned i = 0; i < sizeof NAMES / sizeof *NAMES; i++) if (!strcmp(name, NAMES[i])) { ink_shell_close(); ink_ui_push(&PAGES[i], (void *)(intptr_t)i); return; }
}
static void action(ink_call_t *c) {
    const char *id = c->cap->id;
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
        S.adult = !S.adult; S.input[1].value = S.adult ? "大人" : "孩子"; lv_label_set_text(lv_obj_get_child(S.role, 0), S.input[1].value); ink_ui_content_changed();
    }
    else if (!strcmp(id, "family.edit")) {
        if (S.field) { c->rc.status = INK_RC_STALE; return; }
        S.target[0] = 0; ink_args_get_str(&c->args, "id", S.target, sizeof S.target);
        lv_obj_clean(S.body); S.list = NULL;
        S.form[0] = (ink_ui_action_spec_t){.id = "name", .label = "称呼", .capability = "family.focus", .enabled = true};
        S.form[1] = (ink_ui_action_spec_t){.id = "save", .label = "保存称呼", .capability = "family.save", .enabled = true};
        S.form[2] = (ink_ui_action_spec_t){.id = "role", .label = "大人或孩子", .capability = "family.role", .enabled = !S.target[0]};
        S.form[3] = (ink_ui_action_spec_t){.id = "remove", .label = "移除这位家人", .capability = "member.remove", .enabled = S.target[0]};
        S.form[4] = (ink_ui_action_spec_t){.id = "pin", .label = "改我的密码", .capability = "shell.pin_set", .enabled = true};
        S.input[0] = (ink_ui_field_spec_t){.id = "name", .role = "textbox", .label = "称呼"};
        S.input[1] = (ink_ui_field_spec_t){.id = "role", .role = "status", .label = "新家人的身份", .value = "孩子"};
        S.model.fields = S.input; S.model.field_count = 1; S.model.actions = S.form; S.model.action_count = 5; S.adult = false;
        if (!S.target[0]) S.model.field_count = 2;
        ink_text_cfg_t cfg = {.type = INK_TEXT_PLAIN, .max = 31, .hint = "输入称呼，按完成保存"};
        S.field = ink_textfield_create(S.body, &cfg, entered, NULL, false);
        lv_obj_set_pos(S.field, ink_ui_px(INK_MARGIN), ink_ui_px(INK_GAP_L));
        ink_textfield_bind_short(S.field, &S.model, &S.input[0], &S.form[0]);
        snprintf(S.args[0], sizeof S.args[0], "{\"id\":\"%s\"}", S.target); S.form[3].args_json = S.args[0];
        for (int i = 1; i < 5; i++) if (S.form[i].enabled) {
            lv_obj_t *b = ink_ui_pill(S.body, i == 2 ? "孩子" : S.form[i].label, INK_MARGIN, i * (INK_CONTROL + INK_GAP) + INK_GAP_L, INK_CANVAS_W - 2 * INK_MARGIN, INK_CONTROL, false, NULL, NULL);
            ink_shell_bind(b, S.form[i].capability, S.form[i].args_json); if (i == 2) S.role = b;
        }
    } else if (S.list && !S.field) {
        bool next = !strcmp(id, "family.next");
        ink_ui_list_turn(S.list, next ? 1 : -1);
    }
    else c->rc.status = INK_RC_STALE;
}
int ink_settings_family_init(void) {
    static const ink_cap_t caps[] = {
        {.id = "family.kid_apps", .title = "打开孩子的应用名单", .effect = INK_FX_NAVIGATE, .run = action},
        {.id = "family.kid_periods", .title = "打开孩子的禁用时段", .input = "{\"type\":\"object\",\"properties\":{\"index\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":3}}}", .effect = INK_FX_NAVIGATE, .run = action},
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
