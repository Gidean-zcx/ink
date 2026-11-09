/* Conversation entry and typed questions; turns remain owned by the AI service. */
#include <stdio.h>
#include <string.h>
#include "ink/osal.h"
#include "ink/sys/ai.h"
#include "ink/sys/appreg.h"
#include "ink/sys/textfield.h"
#include "ai_chat.h"

static lv_obj_t *field;
static bool tapped;
static ink_exec_job_t focus_job;
static ink_ui_field_spec_t input = {.id = "question", .role = "textbox", .label = "问题"};
static ink_ui_action_spec_t actions[] = {
    {.id = "ask", .label = "问点什么…", .capability = "shell.ai_ask", .enabled = true},
    {.id = "voice", .label = "用话筒问", .capability = "ai.panel.listen", .enabled = true},
    {.id = "back", .label = "返回", .capability = "shell.back", .enabled = true},
    {.id = "send", .label = "发送", .capability = "shell.ai_send", .enabled = true}
};
static const ink_ui_page_spec_t ASK_MODEL = {.id = "AiAsk", .title = "问 AI", .object = {"ai:question", 1},
    .fields = &input, .field_count = 1, .actions = actions, .action_count = 4};
static const ink_ui_field_spec_t empty = {.id = "history", .role = "status", .label = "对话记录", .value = "还没有对话记录"};
static const ink_ui_page_spec_t HOME_MODEL = {.id = "AiHome", .title = "对话记录", .object = {"ai:home", 1},
    .fields = &empty, .field_count = 1, .actions = actions, .action_count = 3};
static int home_snapshot(char *out, size_t cap) { return ink_ui_semantics_snapshot(&HOME_MODEL, out, cap); }
static int ask_snapshot(char *out, size_t cap) {
    actions[3].enabled = input.value && input.value[0];
    return field ? ink_textfield_short_snapshot(field, out, cap) : -1;
}
static void entered(void *ctx, const char *value) {
    (void)ctx; (void)value;
    ink_receipt_t rc;
    ink_cap_invoke("shell.ai_send", NULL, INK_SRC_TOUCH, 0, &rc);
}
static void focus_input(void *ctx) { (void)ctx; if (field) ink_textfield_short_focus(field); }
static void ask_create(lv_obj_t *screen, void *arg) {
    (void)arg;
    ink_shell_statusbar(screen, true);
    ink_shell_topbar(screen, ASK_MODEL.title);
    ink_text_cfg_t cfg = {.type = INK_TEXT_PLAIN, .enter = INK_ENTER_SEND, .max = 383, .hint = "问点什么…"};
    field = ink_textfield_create(screen, &cfg, entered, NULL, false);
    if (!field) return;
    lv_obj_set_pos(field, ink_ui_px(INK_MARGIN), ink_ui_px(INK_STATUS_BAR + INK_PRIMARY + INK_GAP));
    lv_obj_set_size(field, ink_ui_px(INK_CANVAS_W - 2 * INK_MARGIN), ink_ui_px(4 * ink_ui_line_height(INK_FONT_BODY) + 2 * INK_GAP_L));
    ink_textfield_bind_short(field, &ASK_MODEL, &input, &actions[0]);
    ink_exec_post(INK_EXEC_UI, &focus_job, focus_input, NULL);
}
static void ask_destroy(void) { field = NULL; }
static void home_create(lv_obj_t *screen, void *arg) {
    (void)arg;
    ink_shell_statusbar(screen, true);
    ink_shell_topbar(screen, HOME_MODEL.title);
    ink_ui_notice(screen, empty.value, INK_STATUS_BAR + INK_PRIMARY + INK_BLOCK);
    lv_obj_t *ask = ink_ui_pill(screen, actions[0].label, INK_MARGIN, INK_CANVAS_H - INK_MARGIN - INK_PRIMARY,
        INK_CANVAS_W - 2 * INK_MARGIN, INK_PRIMARY, false, NULL, NULL);
    ink_shell_bind(ask, actions[0].capability, NULL);
    lv_obj_t *mic = ink_ui_circle(screen, "mic", INK_CANVAS_W - INK_MARGIN - INK_CONTROL,
        INK_CANVAS_H - INK_MARGIN - INK_PRIMARY, INK_CONTROL, INK_ICON_MEDIUM, INK_SURFACE_BLACK, NULL, NULL);
    ink_shell_bind(mic, actions[1].capability, NULL);
}
static const ink_activity_t ASK = {.name = "AiAsk", .app = "ai", .create = ask_create, .destroy = ask_destroy, .snapshot = ask_snapshot};
static const ink_activity_t HOME = {.name = "AiHome", .app = "ai", .create = home_create, .snapshot = home_snapshot};
static const ink_app_def_t APP = {.id = "ai", .title = "对话记录", .icon = "ai", .entry = &HOME, .system_app = true, .child_ok = true};
static void action(ink_call_t *c) {
    const char *id = c->cap->id;
    if (!strcmp(id, "shell.ai_home")) { ink_shell_ai_hide(); ink_ui_drop_app("ai"); if (ink_app_open("ai", NULL, NULL)) c->rc.status = INK_RC_FAILED; }
    else if (!strcmp(id, "shell.ai_ask")) {
        if (!strcmp(ink_ui_page_name(), ASK.name)) { if (!field || ink_textfield_short_focus(field)) c->rc.status = INK_RC_FAILED; }
        else ink_ui_push(&ASK, NULL);
    } else if (!strcmp(id, "shell.ai_send")) {
        if (!field || strcmp(ink_ui_page_name(), ASK.name) || !ink_textfield_text(field)[0]) { c->rc.status = INK_RC_INVALID; return; }
        if (ink_text_focused()) ink_text_blur(0);
        ink_ai_ask_text(ink_textfield_text(field));
    } else if (!strcmp(id, "shell.ai_stop")) ink_ai_stop_speech();
    else {
        bool pressed; int64_t time = (int64_t)osal_now_us();
        ink_args_get_int(&c->args, "t_us", &time);
        if (ink_args_get_bool(&c->args, "pressed", &pressed)) { tapped = false; ink_ai_key(pressed, (uint64_t)time); }
        else { tapped = true; ink_ai_tap(); }
    }
}
#define CHAT_CAP(id_, title_) {.id = id_, .title = title_, .effect = INK_FX_NAVIGATE, .child_ok = true, .run = action}
static const ink_cap_t CAPS[] = {
    CHAT_CAP("shell.ai_home", "打开对话记录"), CHAT_CAP("shell.ai_ask", "输入问题"),
    CHAT_CAP("shell.ai_send", "发送输入框里的问题"), CHAT_CAP("shell.ai_stop", "停下 AI 的朗读，保留文字"),
    {.id = "shell.ai_key", .title = "按住说话或点按听", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = action,
     .input = "{\"type\":\"object\",\"properties\":{\"pressed\":{\"type\":\"boolean\"},\"t_us\":{\"type\":\"integer\"}}}"}
};
void ink_ai_chat_key(bool pressed, uint64_t t_us) {
    ink_args_t args; ink_args_init(&args);
    ink_args_bool(&args, "pressed", pressed); ink_args_int(&args, "t_us", (int64_t)t_us);
    ink_receipt_t rc; ink_cap_invoke("shell.ai_key", &args, INK_SRC_KEY, 0, &rc);
}
bool ink_ai_chat_tapped(void) { return tapped; }
int ink_ai_chat_init(void) { return ink_cap_register(CAPS, sizeof CAPS / sizeof *CAPS) || ink_app_register(&APP); }

int ink_ai_chat_panel_snapshot(const char *status, const char *question, const char *answer,
                               int page, int pages, bool speaking, char *out, size_t cap) {
    char count[64]; snprintf(count, sizeof count, "页码=%d/%d", page + 1, pages);
    const char *board = speaking ? "AiSpeak" : strstr(status, "正在听") ? "AiListenTap" : pages > 1 ? "AiLong" : "AiAsk";
    ink_ui_field_spec_t fields[] = {
        {.id = "board", .role = "status", .label = "页面", .value = board},
        {.id = "status", .role = "status", .label = "状态", .value = status},
        {.id = "question", .role = "text", .label = "问题", .value = question},
        {.id = "answer", .role = "text", .label = "回答", .value = answer},
        {.id = "page", .role = "status", .label = "页码", .value = count}
    };
    ink_ui_action_spec_t available[] = {
        {.id = "hide", .label = "收起", .capability = "ai.panel.hide", .enabled = true},
        {.id = "listen", .label = "继续问", .capability = "ai.panel.listen", .enabled = true},
        {.id = "next", .label = "下一页", .capability = "ai.panel.next", .enabled = !speaking && page + 1 < pages},
        {.id = "prev", .label = "上一页", .capability = "ai.panel.prev", .enabled = !speaking && page > 0},
        {.id = "stop", .label = "停下", .capability = "shell.ai_stop", .enabled = speaking},
        {.id = "home", .label = "对话记录", .capability = "shell.ai_home", .enabled = !speaking && answer[0]}
    };
    ink_ui_page_spec_t model = {.id = ink_ui_page_name(), .title = "AI 面板", .object = {"ai:panel", 1},
        .fields = fields, .field_count = sizeof fields / sizeof *fields, .actions = available, .action_count = sizeof available / sizeof *available};
    return ink_ui_semantics_snapshot(&model, out, cap);
}
