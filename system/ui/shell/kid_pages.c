/* Child policy surfaces; credentials use the existing local member form. */
/* ui-scale-runtime: dimension x,y,w Layout positions derive from theme tokens. */
#include <stdio.h>
#include <string.h>
#include "ink/sys/appreg.h"
#include "ink/sys/members.h"
#include "ink/sys/power.h"
#include "ink/sys/state.h"
#include "ink/ui_shell.h"
#include "member_pages.h"
#include "shell_card.h"

static struct { shell_card_t card; char app[32]; uint32_t epoch; bool up; } K;
static void action(ink_call_t *c);
static int snapshot(char *out, size_t cap) {
    return snprintf(out, cap, "%s；当前=%s；%s；动作：%s。", K.up ? "KidTimeUp" : "KidLimit", ink_member_current()->name,
        K.up ? "今天的时间到了；读到哪儿都记好了；电话和短信不限制" : "要爸爸妈妈来；请他们输密码；只放行这一次",
        K.up ? "shell.kid_phone、shell.kid_sms、shell.kid_add、member.time_get" : "shell.kid_cancel、shell.kid_parent");
}
static bool on_key(ink_key_t key, bool lng);
static const ink_ui_overlay_t VIEWS[] = {
    {.name = "KidTimeUp", .level = 1, .snapshot = snapshot, .on_key = on_key},
    {.name = "KidLimit", .level = 1, .snapshot = snapshot, .on_key = on_key}
};
static void close_page(void) {
    if (K.card.root) { ink_ui_overlay_remove(&VIEWS[K.up ? 0 : 1]); shell_card_close(&K.card); }
}
void ink_shell_kid_close(void) { close_page(); }
static bool on_key(ink_key_t key, bool lng) {
    if (K.up && !lng && (key == INK_KEY_HOME || key == INK_KEY_BACK) && ink_member_app_time_allows("home")) { close_page(); ink_ui_home(); return true; }
    if (!K.up && !lng && (key == INK_KEY_HOME || key == INK_KEY_BACK)) close_page();
    return true;
}
static void clicked(lv_event_t *e) { ink_receipt_t rc; ink_cap_invoke(lv_event_get_user_data(e), NULL, INK_SRC_TOUCH, 0, &rc); }
static void show(bool up) {
    close_page(); ink_shell_close(); K.up = up; K.epoch = ink_member_epoch();
    if (up && !strcmp(ink_ui_page_name(), "book")) ink_ui_redraw();
    shell_card_open(&K.card); shell_card_full(&K.card); lv_obj_clean(K.card.panel);
    ink_ui_overlay_add(&VIEWS[up ? 0 : 1], K.card.root);
    lv_obj_remove_flag(ink_shell_statusbar(K.card.panel, true), LV_OBJ_FLAG_CLICKABLE);
    int w = INK_CANVAS_W - 2 * INK_MARGIN, y = INK_CANVAS_H / 2 - INK_CIRCLE_LARGE;
    ink_ui_circle(K.card.panel, up ? "clock" : "family", (INK_CANVAS_W - INK_CIRCLE_LARGE) / 2, y,
        INK_CIRCLE_LARGE, INK_ICON_STATE, up ? INK_SURFACE_BLACK : INK_SURFACE_STATIC, NULL, NULL);
    y += INK_CIRCLE_LARGE + INK_GAP_L;
    lv_obj_t *title = ink_ui_label(K.card.panel, up ? "今天的时间到了" : "要爸爸妈妈来", INK_FONT_SUBTITLE, INK_WEIGHT_BOLD, INK_MARGIN, y, w);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0); y += ink_ui_line_height(INK_FONT_SUBTITLE) + INK_GAP_M;
    lv_obj_t *body = ink_ui_label(K.card.panel, up ? "读到哪儿都记好了，明天接着来。" : "请他们输密码，只放行这一次。", INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_MARGIN, y, w);
    lv_obj_set_style_text_align(body, LV_TEXT_ALIGN_CENTER, 0);
    y = INK_CANVAS_H - INK_MARGIN - INK_PRIMARY;
    if (up) ink_ui_pill(K.card.panel, "短信", (INK_CANVAS_W - INK_CIRCLE_APP) / 2, y - INK_CONTROL - INK_GAP_M, INK_CIRCLE_APP, INK_CONTROL, false, clicked, "shell.kid_sms");
    int left = up ? (w - INK_GAP_M) * 2 / 5 : (w - INK_GAP_M) / 2;
    ink_ui_pill(K.card.panel, up ? "打电话" : "算了", INK_MARGIN, y, left, INK_PRIMARY, false, clicked, up ? "shell.kid_phone" : "shell.kid_cancel");
    ink_ui_pill(K.card.panel, up ? "爸爸妈妈加时间" : "请他们输密码", INK_MARGIN + left + INK_GAP_M, y, w - left - INK_GAP_M, INK_PRIMARY, true, clicked, up ? "shell.kid_add" : "shell.kid_parent");
}
void ink_shell_kid_limit(const char *app) {
    if (ink_member_current()->role != INK_ROLE_CHILD) return;
    snprintf(K.app, sizeof K.app, "%s", app); show(ink_member_time_up());
}
static void changed(void *ctx, int h, const char *id) {
    (void)ctx; (void)h; (void)id;
    if (K.card.root && K.epoch != ink_member_epoch()) close_page();
    if (ink_power_state() == INK_PWR_LOCKED) { close_page(); return; }
    char app[32] = ""; ink_state_get_str(ink_state_find("app.foreground"), app, sizeof app);
    bool expired = ink_member_time_up();
    bool up = expired && !ink_member_app_time_allows(app);
    if (up && (!K.card.root || !K.up)) show(true);
    else if (!expired && K.card.root && K.up) close_page();
}
static void held(void *ctx, const ink_receipt_t *rc) {
    if ((uint32_t)(uintptr_t)ctx != ink_member_epoch()) return;
    unsigned token = 0;
    if (rc->status == INK_RC_PENDING_CONFIRMATION && sscanf(rc->object, "confirm:%u", &token) == 1) ink_shell_member_parent_open(token, K.up ? "今天加 15 分钟，请家长本人输入" : "这次打开这个应用，请家长本人输入");
}
static void action(ink_call_t *c) {
    if (!strcmp(c->cap->id, "shell.kid_allow")) {
        if (c->src != INK_SRC_CONFIRMED || !ink_member_any_pin() || ink_member_time_up()) { c->rc.status = INK_RC_REJECTED; return; }
        char id[32]; ink_args_get_str(&c->args, "id", id, sizeof id);
        if (!ink_member_app_mode(NULL, id)) { c->rc.status = INK_RC_REJECTED; return; }
        for (int i = 0; i < ink_app_count(); i++) if (!strcmp(ink_app_at(i)->id, id) && ink_app_at(i)->entry) {
            close_page();
            if (ink_ui_full()) ink_ui_home();
            ink_ui_push(ink_app_at(i)->entry, NULL); return;
        }
        c->rc.status = INK_RC_INVALID; return;
    }
    if (!K.card.root || K.epoch != ink_member_epoch()) { c->rc.status = INK_RC_STALE; return; }
    if (!strcmp(c->cap->id, "shell.kid_phone") || !strcmp(c->cap->id, "shell.kid_sms")) {
        close_page(); ink_app_open(!strcmp(c->cap->id, "shell.kid_sms") ? "message" : "phone", NULL, NULL);
    }
    else if (!strcmp(c->cap->id, "shell.kid_cancel")) { if (!K.up) close_page(); }
    else {
        char args[96]; snprintf(args, sizeof args, K.up ? "{\"minutes\":15}" : "{\"id\":\"%s\"}", K.app);
        ink_call_opts_t opts = {.src = c->src, .args_json = args, .reply_exec = INK_EXEC_UI, .done = held, .ctx = (void *)(uintptr_t)K.epoch};
        ink_cap_call(K.up ? "member.time_add" : "shell.kid_allow", NULL, &opts);
    }
}
int ink_kid_pages_init(void) {
    static const ink_cap_t caps[] = {
        {.id = "shell.kid_allow", .title = "家长只允许这一次打开应用", .ask = "只允许这一次打开 {id}", .input = "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\",\"maxLength\":31}},\"required\":[\"id\"]}", .effect = INK_FX_NAVIGATE, .child_ok = true, .ask_by_hand = true, .parent_pin = true, .run = action},
        {.id = "shell.kid_parent", .title = "请家长输入密码放行这一次", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = action},
        {.id = "shell.kid_add", .title = "请家长给今天加十五分钟", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = action},
        {.id = "shell.kid_sms", .title = "时间到后看短信", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = action},
        {.id = "shell.kid_phone", .title = "时间到后打电话", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = action},
        {.id = "shell.kid_cancel", .title = "算了，回到刚才的地方", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = action}
    };
    return ink_cap_register(caps, sizeof caps / sizeof *caps) || ink_state_watch("member.", changed, NULL, INK_EXEC_UI) ||
        ink_state_watch("app.cards_rev", changed, NULL, INK_EXEC_UI) ||
        ink_state_watch("app.foreground", changed, NULL, INK_EXEC_UI) || ink_state_watch("power.state", changed, NULL, INK_EXEC_UI) ? -1 : 0;
}
