/* Mobile data presenters; credentials go only through the protected text service. */
/* ui-scale-runtime: dimension y,w Row positions follow theme controls and measured line heights. */
#include <stdio.h>
#include <string.h>
#include "ink/osal.h"
#include "ink/sys/i18n.h"
#include "ink/sys/json.h"
#include "ink/sys/members.h"
#include "ink/sys/settings.h"
#include "ink/sys/state.h"
#include "ink/sys/textfield.h"
#include "ink/ui_shell.h"
enum { MOBILE_FIELDS_MAX = 5, MOBILE_ACTIONS_MAX = 4 };
static struct {
    lv_obj_t *screen; bool edit, has_password, unavailable; uint32_t generation;
    char name[101], user[65], password[65], current[64], previous[64];
    ink_ui_field_spec_t fields[MOBILE_FIELDS_MAX]; ink_ui_action_spec_t actions[MOBILE_ACTIONS_MAX]; ink_ui_page_spec_t model;
} P INK_PSRAM_BSS;
static bool current(void) { return P.screen && !strcmp(ink_ui_page_name(), "SetMobile"); }
static void reload(void *ctx, int h, const char *id);
static void loaded(void *ctx, const ink_receipt_t *rc) {
    if (!current() || P.edit || (uint32_t)(uintptr_t)ctx != P.generation || !rc->result) return;
    ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL); int64_t now = 0, last = 0;
    if (j && !ink_json_parse(j, rc->result, strlen(rc->result))) {
        ink_json_str(j, ink_json_find(j, 0, "name"), P.name, sizeof P.name); ink_json_str(j, ink_json_find(j, 0, "user"), P.user, sizeof P.user);
        P.has_password = ink_json_eq(j, ink_json_find(j, 0, "password"), "true");
        P.unavailable = !ink_json_eq(j, ink_json_find(j, 0, "interface"), "true");
        ink_json_int(j, ink_json_find(j, 0, "current"), &now); ink_json_int(j, ink_json_find(j, 0, "previous"), &last);
        snprintf(P.current, sizeof P.current, "%lld B", (long long)now); snprintf(P.previous, sizeof P.previous, "%lld B", (long long)last);
        if (!ink_json_eq(j, ink_json_find(j, 0, "available"), "true")) { snprintf(P.current, sizeof P.current, "%s", TR("暂时读不出来")); memcpy(P.previous, P.current, sizeof P.previous); }
    }
    osal_free(j); ink_ui_redraw();
}
static void reload(void *ctx, int h, const char *id) {
    (void)ctx; (void)h; (void)id;
    if (current() && !P.edit) { ink_call_opts_t o = {.src = INK_SRC_SYSTEM, .reply_exec = INK_EXEC_UI, .done = loaded, .ctx = (void *)(uintptr_t)P.generation}; ink_cap_call("net.usage", NULL, &o); }
}
static void readonly_changed(void *ctx, int h, const char *id) {
    (void)ctx; (void)id; int64_t readonly = 0;
    if (current() && ink_state_get_int(h, &readonly) && readonly) {
        P.edit = false; memset(P.password, 0, sizeof P.password); ink_ui_redraw();
    }
}
static void entered(void *ctx, const char *value) {
    int i = (int)(intptr_t)ctx; char *out = i == 0 ? P.name : i == 1 ? P.user : P.password;
    snprintf(out, i == 0 ? sizeof P.name : sizeof P.user, "%s", value);
    ink_text_blur(0);
}
static void saved(void *ctx, const ink_receipt_t *rc) {
    if (!current() || (uint32_t)(uintptr_t)ctx != P.generation) return;
    if (rc->status == INK_RC_DONE) { P.edit = false; memset(P.password, 0, sizeof P.password); reload(NULL, 0, NULL); }
}
static void create(lv_obj_t *screen, void *arg) {
    (void)arg; P.screen = screen;
    ink_shell_statusbar(screen, true); ink_shell_topbar(screen, TR("移动网络"));
    int y = INK_STATUS_BAR + INK_PRIMARY + INK_GAP, w = INK_CANVAS_W - 2 * INK_MARGIN;
    int64_t readonly = 0; ink_state_get_int(ink_state_find("cell.config_readonly"), &readonly);
    const char *labels[] = {TR("接入点名称"), TR("用户名"), TR("密码")}, *values[] = {P.name, P.user, P.password};
    _Static_assert(sizeof labels / sizeof *labels <= sizeof P.fields / sizeof *P.fields, "APN rows fit");
    if (P.edit) for (int i = 0; i < 3; i++) {
        ink_text_cfg_t cfg = {.type = i == 2 ? INK_TEXT_PASSWORD : INK_TEXT_PLAIN, .initial = values[i], .hint = labels[i], .max = i ? 64 : 100, .ai_hidden = i == 2};
        lv_obj_t *f = ink_textfield_create(screen, &cfg, entered, (void *)(intptr_t)i, false);
        lv_obj_set_y(f, ink_ui_px(y));
        ink_textfield_style(f, i == 2 ? INK_FIELD_PASSWORD : INK_FIELD_SINGLE, labels[i]);
        P.fields[i] = (ink_ui_field_spec_t){.id = i == 0 ? "name" : i == 1 ? "user" : "password", .role = "textbox", .label = labels[i], .value = i == 2 && P.has_password && !P.password[0] ? TR("有") : values[i], .secret = i == 2};
        y += ink_textfield_height(INK_FIELD_SINGLE) + INK_GAP;
    }
    else {
        if (P.unavailable) { snprintf(P.current, sizeof P.current, "%s", TR("未接入移动网络模组")); memcpy(P.previous, P.current, sizeof P.previous); }
        const char *titles[] = {TR("接入点"), TR("数据漫游"), TR("本月流量"), TR("上个月流量")};
        const char *text[] = {readonly ? TR("暂不能更改") : P.name[0] ? P.name : TR("自动"), ink_setting_get("net.roaming") ? TR("开") : TR("关"), P.current, P.previous};
        _Static_assert(sizeof titles / sizeof *titles <= sizeof P.fields / sizeof *P.fields, "mobile rows fit");
        for (int i = 0; i < 4; i++) {
            ink_ui_line(screen, titles[i], INK_FONT_SUBTITLE, INK_WEIGHT_MEDIUM, INK_MARGIN, y, w);
            ink_ui_line(screen, text[i], INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_MARGIN, y + ink_ui_line_height(INK_FONT_SUBTITLE), w);
            P.fields[i] = (ink_ui_field_spec_t){.id = titles[i], .role = "text", .label = titles[i], .value = text[i]}; y += INK_CONTROL + INK_GAP_L;
        }
    }
    const char *caps[] = {"net.apn_set", "shell.mobile_edit", "net.roaming_set", "net.usage_reset"};
    const char *buttons[] = {TR("自动"), TR("手动"), TR("数据漫游"), TR("清零流量统计")};
    const char *args[] = {"{\"automatic\":true}", "{}", ink_setting_get("net.roaming") ? "{\"enabled\":false}" : "{\"enabled\":true}", "{}"};
    _Static_assert(sizeof caps / sizeof *caps <= MOBILE_ACTIONS_MAX && sizeof buttons / sizeof *buttons == sizeof caps / sizeof *caps && sizeof args / sizeof *args == sizeof caps / sizeof *caps, "mobile actions fit");
    for (int i = 0; i < (P.edit ? 1 : 4); i++) {
        const char *cap = P.edit ? "shell.mobile_save" : caps[i], *label = P.edit ? TR("保存接入点") : buttons[i];
        P.actions[i] = (ink_ui_action_spec_t){.id = cap, .label = label, .capability = cap, .args_json = P.edit ? "{}" : args[i], .enabled = !P.unavailable && !(readonly && (P.edit || i < 2)) && ink_member_current()->role != INK_ROLE_CHILD};
        lv_obj_t *b = ink_ui_pill(screen, label, INK_MARGIN + (i % 2) * ((w + INK_GAP) / 2), y + (i / 2) * (INK_CONTROL + INK_GAP), (w - INK_GAP) / 2, INK_CONTROL, false, NULL, NULL);
        if (P.actions[i].enabled) ink_shell_bind(b, cap, P.actions[i].args_json);
        else lv_obj_add_state(b, LV_STATE_DISABLED);
    }
    P.model = (ink_ui_page_spec_t){.id = "SetMobile", .title = TR("移动网络"), .object = {"net.mobile", 0}, .fields = P.fields, .field_count = P.edit ? 3 : 4, .actions = P.actions, .action_count = P.edit ? 1 : 4};
}
/* A redraw also destroys and recreates the page; only a new visit (shell.mobile) starts a new generation, so a
 * reply still in flight across a redraw (readonly metadata, font size) is not dropped and the page never stays unavailable. */
static void destroy(void) { P.screen = NULL; memset(P.password, 0, sizeof P.password); }
static int snapshot(char *out, size_t cap) { return ink_ui_semantics_snapshot(&P.model, out, cap); }
static const ink_activity_t PAGE = {.name = "SetMobile", .create = create, .destroy = destroy, .snapshot = snapshot};
static void navigate(ink_call_t *c) {
    if (!strcmp(c->cap->id, "shell.mobile")) { P.edit = false; P.unavailable = true; P.generation++; ink_ui_push(&PAGE, NULL); reload(NULL, 0, NULL); return; }
    if (!current() || P.unavailable || ink_member_current()->role == INK_ROLE_CHILD) { c->rc.status = INK_RC_REJECTED; return; }
    int64_t readonly = 0; ink_state_get_int(ink_state_find("cell.config_readonly"), &readonly);
    if (readonly) { c->rc.status = INK_RC_REJECTED; snprintf(c->rc.say, sizeof c->rc.say, "%s", TR("暂不能更改")); return; }
    if (!strcmp(c->cap->id, "shell.mobile_edit")) { P.edit = true; ink_ui_redraw(); return; }
    ink_args_t *a = osal_malloc(sizeof *a, OSAL_MEM_EXTERNAL);
    if (!a) { c->rc.status = INK_RC_FAILED; return; }
    ink_args_init(a); ink_args_bool(a, "automatic", false); ink_args_str(a, "name", P.name); ink_args_str(a, "user", P.user);
    if (P.password[0] || !P.has_password) ink_args_str(a, "password", P.password);
    ink_call_opts_t o = {.src = c->src, .reply_exec = INK_EXEC_UI, .done = saved, .ctx = (void *)(uintptr_t)P.generation};
    if (ink_cap_call("net.apn_set", a, &o)) c->rc.status = INK_RC_FAILED;
    memset(a, 0, sizeof *a); osal_free(a);
}
int ink_mobile_network_init(void) {
    static const ink_cap_t caps[] = {
        {.id = "shell.mobile", .app = NULL, .title = "移动网络", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
        {.id = "shell.mobile_edit", .app = NULL, .title = "手动接入点", .activity = "SetMobile", .effect = INK_FX_NAVIGATE, .expose = INK_EXPOSE_OWNER, .run = navigate},
        {.id = "shell.mobile_save", .app = NULL, .title = "保存接入点", .activity = "SetMobile", .effect = INK_FX_NAVIGATE, .expose = INK_EXPOSE_OWNER, .run = navigate}};
    return ink_cap_register(caps, sizeof caps / sizeof *caps) || ink_state_watch("net.mobile_rev", reload, NULL, INK_EXEC_UI) ||
        ink_state_watch("cell.config_readonly", readonly_changed, NULL, INK_EXEC_UI);
}
