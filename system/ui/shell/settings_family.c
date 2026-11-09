/* Family settings use the existing member, settings and activity capabilities. */
/* ui-scale-runtime: dimension y,w,h Layout derives from theme rows and the available page body. */
#include <stdio.h>
#include <string.h>
#include "ink/osal.h"
#include "ink/sys/file_picker.h"
#include "ink/sys/json.h"
#include "ink/sys/members.h"
#include "ink/sys/settings.h"
#include "ink/sys/textfield.h"
#include "ink/sys/ui_list.h"
#include "ink/ui_shell.h"

enum { MEMBERS, KID, WALLPAPER, RECORDS, MEMORY };
static const char *const NAMES[] = {"SetMembers", "SetKid", "Wallpaper", "Records", "AiMemory"};
static const char *const TITLES[] = {"家人", "孩子的设置", "锁屏和壁纸", "记录", "AI 记得的事"};
static struct {
    int page, count;
    uint32_t epoch, generation;
    lv_obj_t *body, *field, *old_pin, *role;
    bool adult, pin;
    ink_ui_list_t *list;
    ink_ui_list_position_t pos;
    char target[16], titles[24][96], values[24][160], ids[24][48], caps[24][48], args[24][192];
    ink_ui_field_spec_t input[2];
    ink_ui_action_spec_t form[5];
    ink_ui_page_spec_t model;
} S INK_PSRAM_BSS;
static bool current(void) { return S.body && S.epoch == ink_member_epoch() && !strcmp(ink_ui_page_name(), NAMES[S.page]); }
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
    if (!strcmp(id, "display.lock_style")) snprintf(value, sizeof value, "%s", v ? "时钟和待办" : "这一页");
    if (!strcmp(id, "power.lock_after_s")) snprintf(value, sizeof value, "%lld 分钟", (long long)v / 60);
    if (!strcmp(id, "display.wallpaper_file")) snprintf(value, sizeof value, "%s", v ? "已选择" : "还没有选壁纸");
    row(id, title, value, "settings.set", args);
}
static void bind_row(lv_obj_t *slot, size_t i, ink_ui_field_spec_t *f, ink_ui_action_spec_t *a, void *ctx) {
    (void)ctx;
    *f = (ink_ui_field_spec_t){.id = S.ids[i], .role = "text", .label = S.titles[i], .value = S.values[i], .object = {S.ids[i], 0}};
    *a = (ink_ui_action_spec_t){.id = S.ids[i], .label = S.titles[i], .capability = S.caps[i], .args_json = S.args[i], .object = f->object, .enabled = S.caps[i][0]};
    lv_obj_clean(slot); lv_obj_set_style_bg_color(slot, ink_ui_gray(), 0); lv_obj_set_style_border_width(slot, 0, 0);
    int w = INK_CANVAS_W - 2 * INK_MARGIN;
    ink_ui_line(slot, S.titles[i], INK_FONT_LIST, INK_WEIGHT_MEDIUM, INK_GAP_L, INK_GAP_M, w - 2 * INK_GAP_L);
    ink_ui_line(slot, S.values[i], INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_GAP_L, INK_GAP_M + ink_ui_line_height(INK_FONT_LIST), w - 2 * INK_GAP_L);
}
static void records_reply(void *ctx, const ink_receipt_t *rc) {
    if ((uint32_t)(uintptr_t)ctx != S.generation || !current() || S.page != RECORDS) return;
    S.count = 0;
    ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL);
    int items = j && rc->result && !ink_json_parse(j, rc->result, strlen(rc->result)) ? ink_json_find(j, 0, "items") : -1;
    if (items >= 0) for (int t = items + 1; t < j->ntok && S.count < 20 && j->tok[t].start < j->tok[items].end; t++) {
        if (j->tok[t].type != 1) continue;
        char id[48], cap[48], status[32];
        ink_json_str(j, ink_json_find(j, t, "cap"), cap, sizeof cap); ink_json_str(j, ink_json_find(j, t, "status"), status, sizeof status);
        const ink_cap_t *k = ink_cap_find(cap); snprintf(id, sizeof id, "record:%d", S.count);
        row(id, k ? k->title : cap, !strcmp(status, "done") ? "已完成" : "未完成或等待确认", NULL, NULL);
        int end = j->tok[t].end;
        while (t + 1 < j->ntok && j->tok[t + 1].start < end) t++;
    }
    if (!S.count) row("activity", "AI 替你做过的事", items >= 0 ? "还没有记录" : "记录暂时读不出来", NULL, NULL);
    row("runtime", "运行记录", "设备还没有提供运行记录接口", NULL, NULL);
    row("memory", "AI 记得的事", "查看", "shell.ai_memory", NULL);
    osal_free(j); ink_ui_list_refresh(S.list, S.count);
}
static void entered(void *ctx, const char *value) {
    (void)ctx;
    if (!current() || !value[0]) return;
    ink_receipt_t rc; ink_cap_invoke(S.pin ? "family.pin_save" : "family.save", NULL, INK_SRC_TOUCH, 0, &rc);
}
static void saved(void *ctx, const ink_receipt_t *rc) {
    ink_call_t *c = ctx; c->rc = *rc; c->rc.result = NULL; ink_cap_done(c);
}
static void changed(void *ctx, int h, const char *id) {
    (void)ctx; (void)h; (void)id;
    if (current() && !S.field) ink_ui_redraw();
}
static int snapshot(char *out, size_t cap) {
    if (!current()) return -1;
    return S.field ? ink_textfield_short_snapshot(S.field, out, cap) : ink_ui_list_snapshot(S.list, out, cap);
}
static void create(lv_obj_t *screen, void *arg) {
    S.page = (int)(intptr_t)arg; S.epoch = ink_member_epoch(); S.generation++; S.count = 0; S.field = S.old_pin = NULL; S.pin = false;
    memset(&S.pos, 0, sizeof S.pos);
    ink_shell_statusbar(screen, true); ink_shell_topbar(screen, TITLES[S.page]);
    int y = INK_STATUS_BAR + INK_PRIMARY + INK_GAP_XS, h = INK_CANVAS_H - y, w = INK_CANVAS_W - 2 * INK_MARGIN;
    S.body = ink_ui_box(screen, 0, y, INK_CANVAS_W, h, 0, INK_SURFACE_WHITE, false);
    static const ink_ui_action_spec_t actions[] = {
        {.id = "prev", .label = "上一页", .capability = "family.prev"}, {.id = "next", .label = "下一页", .capability = "family.next"},
        {.id = "kid", .label = "孩子的设置", .capability = "shell.kid_settings", .enabled = true}};
    S.model = (ink_ui_page_spec_t){.id = NAMES[S.page], .title = TITLES[S.page], .object = {NAMES[S.page], 0}, .actions = actions, .action_count = S.page == MEMBERS ? 3 : 2};
    S.list = ink_ui_list_create(S.body, &S.model, &S.pos, 4, 1, ink_ui_row_double(), "这里还没有内容", bind_row, NULL);
    char args[128];
    if (S.page == MEMBERS) {
        for (int i = 0; ink_member_at(i); i++) {
            const ink_member_t *m = ink_member_at(i); snprintf(args, sizeof args, "{\"id\":\"%s\"}", m->id);
            row(m->id, m->name, !strcmp(m->id, ink_member_current()->id) ? "正在用" : m->role == INK_ROLE_CHILD ? "孩子" : "大人", "family.edit", args);
        }
        row("add", "加一位家人", "", "family.edit", NULL);
    } else if (S.page == KID) {
        const ink_member_t *kid = NULL;
        for (int i = 0; ink_member_at(i); i++) if (ink_member_at(i)->role == INK_ROLE_CHILD) { kid = ink_member_at(i); break; }
        row("child", "家人", kid ? kid->name : "还没有孩子家人", NULL, NULL);
        bool reader = false, ai = false;
        for (int i = 0; kid && i < 8 && kid->allow[i]; i++) { reader |= !strcmp(kid->allow[i], "reader."); ai |= !strcmp(kid->allow[i], "ai."); }
        row("apps", "能用什么", kid ? reader && ai ? "阅读、AI；电话和短信不限制" : "按家人的已有能力策略开放" : "还没有孩子家人", NULL, NULL);
        row("hours", "可用时段", "儿童时段策略还没有接入", NULL, NULL);
        setting("audio.child_max", "孩子的音量上限", ink_setting_get("audio.child_max") >= 100 ? 10 : ink_setting_get("audio.child_max") + 10);
    } else if (S.page == WALLPAPER) {
        setting("display.lock_style", "锁屏时显示", !ink_setting_get("display.lock_style"));
        setting("power.lock_after_s", "自动锁屏", ink_setting_get("power.lock_after_s") == 600 ? 300 : 600);
        row("choose", "从相册选", ink_wallpaper_ready() ? "已选壁纸；主页和锁屏共用" : ink_setting_get("display.wallpaper_file") ? "壁纸暂时读不出来" : "还没有选壁纸", "wallpaper.choose", NULL);
        setting("display.wallpaper_file", "移除壁纸", 0);
    } else if (S.page == RECORDS) {
        row("activity", "AI 替你做过的事", "正在读取", NULL, NULL);
        ink_call_opts_t o = {.src = INK_SRC_SYSTEM, .args_json = "{\"limit\":20}", .reply_exec = INK_EXEC_UI, .done = records_reply, .ctx = (void *)(uintptr_t)S.generation};
        ink_cap_call("activity.list", NULL, &o);
    } else row("unavailable", "暂时看不到 AI 记得的事", "家庭服务器还没有提供记忆接口", NULL, NULL);
    ink_ui_list_refresh(S.list, S.count);
    if (S.page == MEMBERS) {
        ink_shell_bind(ink_ui_pill(screen, "孩子的设置", INK_MARGIN, INK_CANVAS_H - INK_MARGIN - INK_CONTROL, w, INK_CONTROL, false, NULL, NULL), "shell.kid_settings", NULL);
    }
}
static void destroy(void) { S.generation++; S.body = S.field = S.old_pin = NULL; S.list = NULL; }
static bool key(ink_key_t k, bool lng) { return current() && !S.field && ink_ui_list_key(S.list, k, lng); }
#define PAGE(name_) {.name = name_, .create = create, .destroy = destroy, .snapshot = snapshot, .on_key = key}
static const ink_activity_t PAGES[] = {PAGE("SetMembers"), PAGE("SetKid"), PAGE("Wallpaper"), PAGE("Records"), PAGE("AiMemory")};
void ink_settings_family_open(const char *name) {
    if (ink_member_current()->role == INK_ROLE_CHILD || !ink_member_space_open()) return;
    for (int i = 0; i < 5; i++) if (!strcmp(name, NAMES[i])) { ink_shell_close(); ink_ui_push(&PAGES[i], (void *)(intptr_t)i); return; }
}
static void action(ink_call_t *c) {
    const char *id = c->cap->id;
    if (!strncmp(id, "shell.", 6)) {
        const char *const caps[] = {"shell.family", "shell.kid_settings", "shell.wallpaper", "shell.records", "shell.ai_memory"};
        for (int i = 0; i < 5; i++) if (!strcmp(id, caps[i])) ink_settings_family_open(NAMES[i]);
    } else if (!current()) c->rc.status = INK_RC_STALE;
    else if (!strcmp(id, "family.focus")) { if (S.field) ink_textfield_short_focus(S.field); }
    else if (!strcmp(id, "family.old_pin")) { if (S.old_pin) ink_textfield_short_focus(S.old_pin); }
    else if (!strcmp(id, "family.save") || !strcmp(id, "family.pin_save")) {
        if (!S.field || S.pin != !strcmp(id, "family.pin_save")) { c->rc.status = INK_RC_STALE; return; }
        ink_args_t args; ink_args_init(&args); char member[16];
        snprintf(member, sizeof member, "p%llx", (unsigned long long)osal_wall_us() & 0xffffffffffffull);
        ink_args_str(&args, "id", S.pin ? ink_member_current()->id : S.target[0] ? S.target : member);
        ink_args_str(&args, S.pin ? "new_pin" : "name", ink_textfield_text(S.field));
        if (S.pin) ink_args_str(&args, "old_pin", S.old_pin ? ink_textfield_text(S.old_pin) : "");
        else ink_args_str(&args, "role", S.adult ? "adult" : "child");
        ink_call_opts_t o = {.src = c->src, .reply_exec = INK_EXEC_UI, .done = saved, .ctx = c}; ink_cap_defer(c);
        if (ink_cap_call(S.pin ? "member.set_pin" : S.target[0] ? "member.rename" : "member.add", &args, &o)) { c->rc.status = INK_RC_FAILED; ink_cap_done(c); }
        memset(&args, 0, sizeof args);
    } else if (!strcmp(id, "family.role")) {
        if (!S.field || S.pin || S.target[0]) { c->rc.status = INK_RC_STALE; return; }
        S.adult = !S.adult; lv_label_set_text(lv_obj_get_child(S.role, 0), S.adult ? "大人" : "孩子"); ink_ui_content_changed();
    }
    else if (!strcmp(id, "family.edit")) {
        if (S.field) { c->rc.status = INK_RC_STALE; return; }
        S.target[0] = 0; ink_args_get_str(&c->args, "id", S.target, sizeof S.target);
        lv_obj_clean(S.body); S.list = NULL;
        S.form[0] = (ink_ui_action_spec_t){.id = "name", .label = "称呼", .capability = "family.focus", .enabled = true};
        S.form[1] = (ink_ui_action_spec_t){.id = "save", .label = "保存称呼", .capability = "family.save", .enabled = true};
        S.form[2] = (ink_ui_action_spec_t){.id = "role", .label = "大人或孩子", .capability = "family.role", .enabled = !S.target[0]};
        S.form[3] = (ink_ui_action_spec_t){.id = "remove", .label = "移除这位家人", .capability = "member.remove", .enabled = S.target[0]};
        S.form[4] = (ink_ui_action_spec_t){.id = "pin", .label = "改密码", .capability = "family.pin", .enabled = true};
        S.input[0] = (ink_ui_field_spec_t){.id = "name", .role = "textbox", .label = "称呼"};
        S.model.fields = S.input; S.model.field_count = 1; S.model.actions = S.form; S.model.action_count = 5; S.adult = false;
        ink_text_cfg_t cfg = {.type = INK_TEXT_PLAIN, .max = 31, .hint = "输入称呼，按完成保存"};
        S.field = ink_textfield_create(S.body, &cfg, entered, NULL, false);
        lv_obj_set_pos(S.field, ink_ui_px(INK_MARGIN), ink_ui_px(INK_GAP_L));
        ink_textfield_bind_short(S.field, &S.model, &S.input[0], &S.form[0]);
        snprintf(S.args[0], sizeof S.args[0], "{\"id\":\"%s\"}", S.target); S.form[3].args_json = S.args[0];
        for (int i = 1; i < 5; i++) if (S.form[i].enabled) {
            lv_obj_t *b = ink_ui_pill(S.body, i == 2 ? "孩子" : S.form[i].label, INK_MARGIN, i * (INK_CONTROL + INK_GAP) + INK_GAP_L, INK_CANVAS_W - 2 * INK_MARGIN, INK_CONTROL, false, NULL, NULL);
            ink_shell_bind(b, S.form[i].capability, S.form[i].args_json); if (i == 2) S.role = b;
        }
    } else if (!strcmp(id, "family.pin")) {
        if (S.pin) return;
        ink_text_blur(0); lv_obj_clean(S.body); S.list = NULL; S.pin = true;
        S.form[0] = (ink_ui_action_spec_t){.id = "new_pin", .label = "新密码", .capability = "family.focus", .enabled = true};
        S.form[1] = (ink_ui_action_spec_t){.id = "old_pin", .label = "原密码", .capability = "family.old_pin", .enabled = true};
        S.form[2] = (ink_ui_action_spec_t){.id = "save_pin", .label = "保存密码", .capability = "family.pin_save", .enabled = true};
        S.input[0] = (ink_ui_field_spec_t){.id = "new_pin", .role = "textbox", .label = "新密码", .secret = true};
        S.input[1] = (ink_ui_field_spec_t){.id = "old_pin", .role = "textbox", .label = "原密码", .secret = true};
        S.model.fields = S.input; S.model.field_count = 2; S.model.actions = S.form; S.model.action_count = 3;
        ink_text_cfg_t cfg = {.type = INK_TEXT_PASSWORD, .max = 8, .hint = "原密码", .ai_hidden = true};
        S.old_pin = ink_textfield_create(S.body, &cfg, NULL, NULL, false);
        lv_obj_set_pos(S.old_pin, ink_ui_px(INK_MARGIN), ink_ui_px(INK_GAP_L));
        ink_textfield_bind_short(S.old_pin, &S.model, &S.input[1], &S.form[1]);
        cfg.hint = "新密码"; S.field = ink_textfield_create(S.body, &cfg, entered, NULL, false);
        lv_obj_set_pos(S.field, ink_ui_px(INK_MARGIN), ink_ui_px(INK_CONTROL + 2 * INK_GAP_L));
        ink_textfield_bind_short(S.field, &S.model, &S.input[0], &S.form[0]);
        ink_shell_bind(ink_ui_pill(S.body, "保存密码", INK_MARGIN, 2 * (INK_CONTROL + INK_GAP_L), INK_CANVAS_W - 2 * INK_MARGIN, INK_CONTROL, false, NULL, NULL), "family.pin_save", NULL);
    } else if (S.list && !S.field) ink_ui_list_turn(S.list, !strcmp(id, "family.next") ? 1 : -1);
    else c->rc.status = INK_RC_STALE;
}
int ink_settings_family_init(void) {
    static const ink_cap_t caps[] = {
        {.id = "shell.family", .title = "管理家人", .effect = INK_FX_NAVIGATE, .run = action},
        {.id = "shell.kid_settings", .title = "打开孩子的设置", .effect = INK_FX_NAVIGATE, .run = action},
        {.id = "shell.wallpaper", .title = "打开锁屏和壁纸", .effect = INK_FX_NAVIGATE, .run = action},
        {.id = "shell.records", .title = "查看使用记录", .effect = INK_FX_NAVIGATE, .run = action},
        {.id = "shell.ai_memory", .title = "查看 AI 记得的事", .effect = INK_FX_NAVIGATE, .run = action},
        {.id = "family.edit", .title = "编辑家人的称呼或添加家人", .activity = "SetMembers", .effect = INK_FX_NAVIGATE, .input = "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\",\"maxLength\":15}}}", .run = action},
        {.id = "family.focus", .title = "输入家人的称呼", .activity = "SetMembers", .effect = INK_FX_NAVIGATE, .run = action},
        {.id = "family.save", .title = "保存家人的称呼", .activity = "SetMembers", .effect = INK_FX_LOCAL_WRITE, .run = action},
        {.id = "family.role", .title = "选择添加大人或孩子", .activity = "SetMembers", .effect = INK_FX_NAVIGATE, .run = action},
        {.id = "family.pin", .title = "进入密码设置", .activity = "SetMembers", .effect = INK_FX_NAVIGATE, .run = action},
        {.id = "family.old_pin", .title = "输入原密码（只能在设备上）", .activity = "SetMembers", .effect = INK_FX_NAVIGATE, .expose = INK_EXPOSE_OWNER, .run = action},
        {.id = "family.pin_save", .title = "保存密码（只能在设备上）", .activity = "SetMembers", .effect = INK_FX_LOCAL_WRITE, .expose = INK_EXPOSE_OWNER, .run = action},
        {.id = "family.prev", .title = "家人设置上一页", .effect = INK_FX_NAVIGATE, .run = action},
        {.id = "family.next", .title = "家人设置下一页", .effect = INK_FX_NAVIGATE, .run = action}};
    return ink_cap_register(caps, sizeof caps / sizeof *caps) || ink_settings_watch("", changed, NULL, INK_EXEC_UI) ? -1 : 0;
}
