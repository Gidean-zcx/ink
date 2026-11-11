/* Conversation entry and typed questions; turns remain owned by the AI service. */
#include "ink/sys/i18n.h"
#include <stdio.h>
#include <string.h>
#include "ink/osal.h"
#include "ink/sys/ai.h"
#include "ink/sys/appreg.h"
#include "ink/sys/textfield.h"
#include "ink/sys/ui_list.h"
#include "ink/sys/hub.h"
#include "ink/sys/members.h"
#include "ink/sys/json.h"
#include "ink/sys/time.h"
#include "ai_chat.h"

static lv_obj_t *field;
static bool tapped;
static ink_exec_job_t focus_job;
static ink_ui_field_spec_t input = {.id = "question", .role = "textbox", .label = TR_NOOP("问题")};
static ink_ui_action_spec_t actions[] = {
    {.id = "ask", .label = TR_NOOP("问点什么…"), .capability = "shell.ai_ask", .enabled = true},
    {.id = "voice", .label = TR_NOOP("用话筒问"), .capability = "ai.panel.listen", .enabled = true},
    {.id = "back", .label = TR_NOOP("返回"), .capability = "shell.back", .enabled = true},
    {.id = "send", .label = TR_NOOP("发送"), .capability = "shell.ai_send", .enabled = true}
};
static ink_ui_page_spec_t ASK_MODEL = {.id = "AiAsk", .title = TR_NOOP("问 AI"), .object = {"ai:question", 1},
    .fields = &input, .field_count = 1, .actions = actions, .action_count = 4};
static ink_ui_field_spec_t empty = {.id = "history", .role = "status", .label = TR_NOOP("对话记录"), .value = TR_NOOP("还没有对话记录")};
static ink_ui_page_spec_t HOME_MODEL = {.id = "AiHome", .title = TR_NOOP("对话记录"), .object = {"ai:home", 1},
    .fields = &empty, .field_count = 1, .actions = actions, .action_count = 3};
#define HISTORY_ROWS 3
static struct {
    ink_db_t *db; ink_ui_list_t *list; ink_ui_list_position_t position; uint32_t epoch;
    char ids[HISTORY_ROWS][32], args[HISTORY_ROWS][48], labels[HISTORY_ROWS][128], info[HISTORY_ROWS][192];
} H INK_PSRAM_BSS;
static int home_snapshot(char *out, size_t cap) { return H.list && ink_db_count(H.db) ? ink_ui_list_snapshot(H.list, out, cap) : ink_ui_semantics_snapshot(&HOME_MODEL, out, cap); }
static void history_ready(void *ctx, const ink_receipt_t *rc) {
    if ((uint32_t)(uintptr_t)ctx == ink_member_epoch() && !strcmp(ink_ui_page_name(), "AiHome") && rc->status == INK_RC_DONE) ink_ui_redraw();
}
static void history_changed(void *ctx, const char *id, const ink_args_t *args) {
    (void)ctx; (void)id; (void)args;
    if (!strcmp(ink_ui_page_name(), "AiHome") && ink_ui_page_current()) ink_ui_redraw();
}
static void history_bind(lv_obj_t *slot, size_t index, ink_ui_field_spec_t *item_field, ink_ui_action_spec_t *action, void *ctx) {
    (void)ctx; unsigned at = index % HISTORY_ROWS; ink_db_cursor_t cur = {.desc = true}; ink_rec_t rec = {0};
    char *raw = osal_malloc(INK_DB_RECORD_MAX + 1, OSAL_MEM_EXTERNAL); ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL);
    if (!raw || !j) { osal_free(raw); osal_free(j); return; }
    int got; do { got = ink_db_next(H.db, &cur, &rec, raw, INK_DB_RECORD_MAX); } while (got == 1 && index-- > 0);
    if (got != 1) { osal_free(raw); osal_free(j); return; }
    raw[rec.len] = 0;
    char question[512] = "", page[128] = ""; int64_t time = 0;
    if (!ink_json_parse(j, raw, rec.len)) {
        ink_json_str(j, ink_json_find(j, 0, "question"), question, sizeof question);
        ink_json_str(j, ink_json_find(j, 0, "page"), page, sizeof page); ink_json_int(j, ink_json_find(j, 0, "at"), &time);
    }
    size_t bytes = ink_text_byte_offset(question, 30);
    snprintf(H.labels[at], sizeof H.labels[at], "%.*s%s", (int)bytes, question, question[bytes] ? "…" : "");
    const char *source = !strcmp(page, "home") ? TR("主页") : !page[0] ? TR("无页面上下文") : TR("其他页面");
    if (!strncmp(page, "Read", 4)) source = TR("阅读器");
    if (!strncmp(page, "Set", 3)) source = TR("设置");
    if (!strncmp(page, "File", 4)) source = TR("文件");
    for (int i = 0; i < ink_app_count(); i++) {
        const ink_app_def_t *app = ink_app_at(i);
        if (app->entry && !strcmp(app->entry->name, page)) { source = TR(app->title); break; }
    }
    ink_tm_t tm; ink_time_to_local(time, &tm);
    if (time) snprintf(H.info[at], sizeof H.info[at], "%02d/%02d %02d:%02d · %s", tm.month, tm.day, tm.hour, tm.min, source);
    else snprintf(H.info[at], sizeof H.info[at], "%s · %s", TR("时间未知"), source);
    snprintf(H.ids[at], sizeof H.ids[at], "history:%u", (unsigned)rec.id); snprintf(H.args[at], sizeof H.args[at], "{\"id\":%u}", (unsigned)rec.id);
    lv_obj_clean(slot); lv_obj_t *question_label = ink_ui_label(slot, H.labels[at], INK_FONT_LIST, INK_WEIGHT_MEDIUM, INK_GAP, INK_GAP_S, INK_CANVAS_W - 2 * INK_MARGIN - INK_CONTROL - 2 * INK_GAP);
    lv_obj_set_height(question_label, ink_ui_px(2 * ink_ui_line_height(INK_FONT_LIST))); lv_label_set_long_mode(question_label, LV_LABEL_LONG_DOT);
    ink_ui_line(slot, H.info[at], INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_GAP, INK_GAP_S + 2 * ink_ui_line_height(INK_FONT_LIST), INK_CANVAS_W - 2 * INK_MARGIN - INK_CONTROL - 2 * INK_GAP);
    lv_obj_t *trash = ink_ui_circle(slot, "trash", INK_CANVAS_W - 2 * INK_MARGIN - INK_CONTROL - INK_GAP, INK_GAP_S, INK_CONTROL, INK_ICON_SMALL, INK_SURFACE_WHITE, NULL, NULL);
    ink_shell_bind(trash, "ai.history.delete", H.args[at]);
    *item_field = (ink_ui_field_spec_t){.id = H.ids[at], .label = H.labels[at], .value = H.info[at], .role = "listitem", .object = {H.ids[at], rec.ver}};
    *action = (ink_ui_action_spec_t){.id = H.ids[at], .label = TR("打开对话"), .capability = "ai.history.open", .args_json = H.args[at], .object = item_field->object, .enabled = true};
    osal_free(j); osal_free(raw);
}
static void home_destroy(void) { H.list = NULL; if (H.db) ink_db_close(H.db); H.db = NULL; }
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
static void translate(void) {
    input.label = TR("问题");
    actions[0].label = TR("问点什么…"); actions[1].label = TR("用话筒问");
    actions[2].label = TR("返回"); actions[3].label = TR("发送");
    ASK_MODEL.title = TR("问 AI"); HOME_MODEL.title = TR("对话记录");
    empty.label = TR("对话记录"); empty.value = TR("还没有对话记录");
}
static void ask_create(lv_obj_t *screen, void *arg) {
    (void)arg; translate();
    ink_shell_statusbar(screen, true);
    ink_shell_topbar(screen, ASK_MODEL.title);
    ink_text_cfg_t cfg = {.type = INK_TEXT_PLAIN, .enter = INK_ENTER_SEND, .max = 383, .hint = TR("问点什么…")};
    field = ink_textfield_create(screen, &cfg, entered, NULL, false);
    if (!field) return;
    lv_obj_set_pos(field, ink_ui_px(INK_MARGIN), ink_ui_px(INK_STATUS_BAR + INK_PRIMARY + INK_GAP));
    ink_textfield_style(field, INK_FIELD_BODY, input.label);
    ink_textfield_bind_short(field, &ASK_MODEL, &input, &actions[0]);
    ink_exec_post(INK_EXEC_UI, &focus_job, focus_input, NULL);
}
static void ask_destroy(void) { field = NULL; }
static void home_create(lv_obj_t *screen, void *arg) {
    (void)arg; translate();
    ink_shell_statusbar(screen, true);
    ink_shell_topbar(screen, HOME_MODEL.title);
    if (H.epoch != ink_member_epoch()) { memset(&H.position, 0, sizeof H.position); H.epoch = ink_member_epoch(); }
    H.db = ink_db_open_cached_member(ink_db_find_def("ai.history"), ink_member_current()->id);
    if (!H.db) {
        ink_ui_notice(screen, TR("正在读取对话记录"), INK_STATUS_BAR + INK_PRIMARY + INK_BLOCK);
        ink_call_opts_t o = {.src = INK_SRC_TOUCH, .reply_exec = INK_EXEC_UI, .done = history_ready, .ctx = (void *)(uintptr_t)H.epoch};
        ink_cap_call("ai.history.list", NULL, &o);
    } else {
        const ink_ui_action_spec_t paging[] = {
            {.id = "prev", .label = TR("上一页"), .capability = "ai.history.prev"},
            {.id = "next", .label = TR("下一页"), .capability = "ai.history.next"},
            {.id = "clear", .label = TR("清空记录"), .capability = "ai.history.clear", .enabled = ink_db_count(H.db) > 0}};
        ink_ui_page_spec_t model = {.id = "AiHome", .title = TR("对话记录"), .object = {"ai.history", ink_db_last_ver(H.db)}, .actions = paging, .action_count = sizeof paging / sizeof *paging};
        int row_height = ink_ui_row_double() + ink_ui_line_height(INK_FONT_LIST);
        lv_obj_t *body = ink_ui_box(screen, 0, INK_STATUS_BAR + INK_PRIMARY + INK_GAP, INK_CANVAS_W, HISTORY_ROWS * (row_height + INK_GAP_M) + INK_PRIMARY, 0, INK_SURFACE_WHITE, false);
        H.list = ink_ui_list_create(body, &model, &H.position, HISTORY_ROWS, 1, row_height, empty.value, history_bind, NULL);
        if (H.list) ink_ui_list_refresh(H.list, (size_t)ink_db_count(H.db));
    }
    lv_obj_t *clear = ink_ui_circle(screen, "trash", INK_CANVAS_W - INK_MARGIN - INK_CONTROL, INK_STATUS_BAR, INK_CONTROL, INK_ICON_MEDIUM, INK_SURFACE_WHITE, NULL, NULL);
    if (H.db && ink_db_count(H.db)) ink_shell_bind(clear, "ai.history.clear", NULL);
    else lv_obj_add_state(clear, LV_STATE_DISABLED);
    lv_obj_t *ask = ink_ui_pill(screen, actions[0].label, INK_MARGIN, INK_CANVAS_H - INK_MARGIN - INK_PRIMARY,
        INK_CANVAS_W - 2 * INK_MARGIN - INK_CONTROL - INK_GAP, INK_CONTROL, false, NULL, NULL);
    ink_shell_bind(ask, actions[0].capability, NULL);
    lv_obj_t *mic = ink_ui_circle(screen, "mic", INK_CANVAS_W - INK_MARGIN - INK_CONTROL,
        INK_CANVAS_H - INK_MARGIN - INK_PRIMARY, INK_CONTROL, INK_ICON_MEDIUM, INK_SURFACE_WHITE, NULL, NULL);
    ink_shell_bind(mic, actions[1].capability, NULL);
}
static const ink_activity_t ASK = {.name = "AiAsk", .app = "ai", .create = ask_create, .destroy = ask_destroy, .snapshot = ask_snapshot};
static const ink_activity_t HOME = {.name = "AiHome", .app = "ai", .create = home_create, .destroy = home_destroy, .snapshot = home_snapshot};
static const ink_app_def_t APP = {.id = "ai", .title = TR_NOOP("对话记录"), .icon = "ai", .entry = &HOME, .system_app = true, .child_ok = true};
static void action(ink_call_t *c) {
    const char *id = c->cap->id;
    if (!strcmp(id, "ai.history.next") || !strcmp(id, "ai.history.prev")) { if (H.list) ink_ui_list_turn(H.list, strstr(id, "next") ? 1 : -1); return; }
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
#define CHAT_CAP(id_, title_) {.id = id_, .app = "ai", .title = title_, .effect = INK_FX_NAVIGATE, .child_ok = true, .run = action}
static const ink_cap_t CAPS[] = {
    {.id = "ai.history.prev", .app = "ai", .title = "对话记录上一页", .activity = "AiHome", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = action},
    {.id = "ai.history.next", .app = "ai", .title = "对话记录下一页", .activity = "AiHome", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = action},
    CHAT_CAP("shell.ai_home", "打开对话记录"), CHAT_CAP("shell.ai_ask", "输入问题"),
    {.id = "shell.ai_send", .app = "ai", .title = "发送输入框里的问题", .activity = "AiAsk", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = action},
    {.id = "shell.ai_stop", .title = "停下 AI 的朗读，保留文字", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = action},
    {.id = "shell.ai_key", .app = "ai", .title = "按住说话或点按听", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = action,
     .input = "{\"type\":\"object\",\"properties\":{\"pressed\":{\"type\":\"boolean\"},\"t_us\":{\"type\":\"integer\"}}}"}
};
void ink_ai_chat_key(bool pressed, uint64_t t_us) {
    ink_args_t args; ink_args_init(&args);
    ink_args_bool(&args, "pressed", pressed); ink_args_int(&args, "t_us", (int64_t)t_us);
    ink_receipt_t rc; ink_cap_invoke("shell.ai_key", &args, INK_SRC_KEY, 0, &rc);
}
bool ink_ai_chat_tapped(void) { return tapped; }
int ink_ai_chat_init(void) { return ink_cap_register(CAPS, sizeof CAPS / sizeof *CAPS) || ink_app_register(&APP) || ink_cap_subscribe("ai.history.changed", history_changed, NULL, INK_EXEC_UI); }

int ink_ai_chat_panel_snapshot(const char *status, const char *key, const char *question, const char *answer,
                               int page, int pages, bool speaking, uint32_t revision, char *out, size_t cap) {
    char count[64]; snprintf(count, sizeof count, TR("页码=%d/%d"), page + 1, pages);
    const char *board = speaking ? "AiSpeak" : strstr(key, "正在听") ? "AiListenTap" : pages > 1 ? "AiLong" : "AiAsk";
    ink_ui_field_spec_t fields[] = {
        {.id = "board", .role = "status", .label = TR("页面"), .value = board},
        {.id = "status", .role = "status", .label = TR("状态"), .value = !ink_hub_online() && answer[0] && !status[0] ? TR("离线，暂时不能朗读") : status},
        {.id = "question", .role = "text", .label = TR("问题"), .value = question},
        {.id = "answer", .role = "text", .label = TR("回答"), .value = answer},
        {.id = "page", .role = "status", .label = TR("页码"), .value = count}
    };
    ink_ui_action_spec_t available[] = {
        {.id = "replay", .label = ink_hub_online() ? TR("再读一遍") : TR("再读一遍（离线不可用）"), .capability = "ai.panel.replay", .enabled = ink_ai_can_replay()},
        {.id = "hide", .label = TR("收起"), .capability = "ai.panel.hide", .enabled = true},
        {.id = "listen", .label = TR("继续问"), .capability = "ai.panel.listen", .enabled = true},
        {.id = "next", .label = TR("下一页"), .capability = "ai.panel.next", .enabled = !speaking && page + 1 < pages},
        {.id = "prev", .label = TR("上一页"), .capability = "ai.panel.prev", .enabled = !speaking && page > 0},
        {.id = "stop", .label = TR("停下"), .capability = "shell.ai_stop", .enabled = speaking},
        {.id = "record_stop", .label = TR("停止录音"), .capability = "audio.record_stop", .enabled = strstr(key, "正在录音") != NULL},
        {.id = "home", .label = TR("对话记录"), .capability = "shell.ai_home", .enabled = !speaking && answer[0]}
    };
    ink_ui_page_spec_t model = {.id = ink_ui_page_name(), .title = TR("AI 面板"), .object = {"ai:panel", revision},
        .fields = fields, .field_count = sizeof fields / sizeof *fields, .actions = available, .action_count = sizeof available / sizeof *available};
    return ink_ui_semantics_snapshot(&model, out, cap);
}
