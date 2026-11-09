/* Settings presenters share the existing F1/F4 bindings and policy gate. */
/* ui-scale-runtime: dimension h,w,y Theme sizes plus measured text, bounded by the page body. */
#include <stdio.h>
#include <string.h>
#include "ink/osal.h"
#include "ink/sys/json.h"
#include "ink/sys/members.h"
#include "ink/sys/settings.h"
#include "ink/sys/state.h"
#include "ink/sys/ui_list.h"
#include "ink/ui_shell.h"

enum { VOICE, STORAGE, ABOUT, PRIVACY };
typedef struct { const char *id, *title, *value, *setting, *cap; } row_t;
static const row_t VOICE_ROWS[] = {
    {"voice.handsfree", "什么时候能叫名字", NULL, "voice.handsfree", NULL},
    {"voice.name", "用什么名字叫它", "暂不能更改", NULL, NULL},
    {"voice.interrupt", "说话能打断回答", "暂不能设置", NULL, NULL},
    {"ai.records", "AI 替你做过的事", "记录页面还没接入", NULL, NULL},
    {"ai.memory", "AI 记得的事", "记忆页面还没接入", NULL, NULL},
    {"text.assist", "打字时帮你接话", "暂不能设置", NULL, NULL}};
static const row_t STORAGE_ROWS[] = {
    {"backup", "备份", NULL, NULL, "backup.now"},
    {"storage.free", "存储卡空间", NULL, NULL, NULL},
    {"storage.card", "存储卡", NULL, NULL, NULL},
    {"storage.mode", "U 盘", NULL, NULL, NULL},
    {"storage.user_free", "设备内部空间", NULL, NULL, NULL},
    {"storage.categories", "书、音频、照片和其他", "暂不能查看分类占用", NULL, NULL},
    {"storage.clean", "清理临时文件", "暂不能清理", NULL, NULL}};
static const row_t ABOUT_ROWS[] = {
    {"device", "这台设备", NULL, NULL, NULL},
    {"update.version", "系统版本", NULL, NULL, NULL},
    {"update.check", "系统更新", NULL, NULL, "update.check"},
    {"update.auto_download", "自动下载新版本", NULL, "update.auto_download", NULL},
    {"privacy", "隐私", "查看", NULL, "shell.privacy"},
    {"device.details", "设备详情", "暂不能查看", NULL, NULL},
    {"licenses", "开源许可", "许可页面还没接入", NULL, NULL},
    {"reset", "清空这台设备", "清掉所有家人的数据，找不回来", NULL, "system.factory_reset"}};
static const row_t PRIVACY_ROWS[] = {
    {"privacy.storage", "你的东西放在哪", "设置和备忘在设备里，书和录音在存储卡上；备份在家庭服务器上。", NULL, NULL},
    {"privacy.ai", "AI 能看到什么", "提问时能看到当前页。密码框里的字看不到；语音和页面会交给家庭选择的模型服务商。", NULL, NULL},
    {"ai.read_clipboard", "AI 可以读剪贴板", NULL, "ai.read_clipboard", NULL},
    {"privacy.retention", "留多久", "语音文字和运行记录的保留期限，这里暂不能查看。", NULL, NULL},
    {"privacy.child", "孩子的记录", "爸爸妈妈可以查看和删除。孩子能看到这一点；记录页面还没接入。", NULL, NULL}};
static const struct { const char *name, *title, *cap; const row_t *rows; size_t count; } PAGES[] = {
    {"SetVoice", "语音和 AI", "shell.voice", VOICE_ROWS, sizeof VOICE_ROWS / sizeof *VOICE_ROWS},
    {"SetStorage", "存储和备份", "shell.storage", STORAGE_ROWS, sizeof STORAGE_ROWS / sizeof *STORAGE_ROWS},
    {"SetAbout", "关于和更新", "shell.about", ABOUT_ROWS, sizeof ABOUT_ROWS / sizeof *ABOUT_ROWS},
    {"SetPrivacy", "隐私", "shell.privacy", PRIVACY_ROWS, sizeof PRIVACY_ROWS / sizeof *PRIVACY_ROWS}};
static struct {
    int page;
    uint32_t generation;
    char member[16], values[8][384], args[8][128], backup[96];
    lv_obj_t *body;
    ink_ui_list_t *list;
    ink_ui_list_position_t position;
} S INK_PSRAM_BSS;
static bool current(void) {
    return S.list && ink_ui_page_current() && !strcmp(ink_ui_page_name(), PAGES[S.page].name) &&
        ink_member_space_open() && !strcmp(S.member, ink_member_current()->id);
}
static bool state_number(const char *id, int64_t *v) { return ink_state_get_int(ink_state_find(id), v); }
static void state_text(const char *id, char *out, size_t cap) {
    if (ink_state_get_str(ink_state_find(id), out, cap) < 0) snprintf(out, cap, "暂时读不出来");
}
static void value(size_t i) {
    const row_t *r = &PAGES[S.page].rows[i];
    char *out = S.values[i]; size_t cap = sizeof S.values[i];
    int64_t v = 0;
    if (r->setting) {
        v = ink_setting_get(r->setting);
        const char *text = v ? "开" : "关";
        if (!strcmp(r->setting, "voice.handsfree")) text = v == 2 ? "关" : v == 1 ? "一直" : "使用时";
        snprintf(out, cap, "%s", text);
        snprintf(S.args[i], sizeof S.args[i], "{\"id\":\"%s\",\"value\":%lld}", r->setting,
            (long long)(!strcmp(r->setting, "voice.handsfree") ? (v + 1) % 3 : !v));
    } else if (r->value) snprintf(out, cap, "%s", r->value);
    else if (!strcmp(r->id, "backup")) snprintf(out, cap, "%s", S.backup);
    else if (!strcmp(r->id, "device")) snprintf(out, cap, "%s的设备", ink_member_current()->name);
    else if (!strcmp(r->id, "storage.card")) {
        int64_t present = 0;
        bool known = state_number(r->id, &present);
        snprintf(out, cap, "%s", !known ? "暂时读不出来" : present ? "可用" : "没有可用的存储卡");
    } else if (!strcmp(r->id, "storage.free") || !strcmp(r->id, "storage.user_free")) {
        int64_t present = 0; char mode[32] = "";
        state_number("storage.card", &present);
        ink_state_get_str(ink_state_find("storage.mode"), mode, sizeof mode);
        if (!strcmp(r->id, "storage.free") && (!present || !strcmp(mode, "usb")))
            snprintf(out, cap, "%s", !strcmp(mode, "usb") ? "存储卡正借给电脑" : "没有可用的存储卡");
        else if (state_number(r->id, &v) && v >= 0) snprintf(out, cap, "还能放 %lld KiB", (long long)v / 1024);
        else snprintf(out, cap, "暂时读不出来");
    } else if (!strcmp(r->id, "storage.mode")) {
        char mode[32]; state_text(r->id, mode, sizeof mode);
        snprintf(out, cap, "%s", !strcmp(mode, "usb") ? "存储卡正借给电脑" : !strcmp(mode, "normal") ? "设备在用" : mode);
    } else if (!strcmp(r->id, "update.check")) {
        char state[32]; state_text("update.state", state, sizeof state);
        const char *text = !strcmp(state, "idle") ? "检查新版本" : !strcmp(state, "available") ? "有新版本" :
            !strcmp(state, "failed") ? "检查或更新失败，可以再试" : !strcmp(state, "ready") ? "已下载" : "正在更新";
        snprintf(out, cap, "%s", text);
    } else state_text(r->id, out, cap);
}
static void bind_row(lv_obj_t *slot, size_t i, ink_ui_field_spec_t *field, ink_ui_action_spec_t *action, void *ctx) {
    (void)ctx;
    const row_t *r = &PAGES[S.page].rows[i];
    value(i);
    *field = (ink_ui_field_spec_t){.id = r->id, .role = r->setting ? "setting" : "text", .label = r->title,
        .value = S.values[i], .object = {r->id, 0}};
    *action = (ink_ui_action_spec_t){.id = r->id, .label = r->title, .capability = r->setting ? "settings.set" : r->cap,
        .args_json = r->setting ? S.args[i] : NULL, .object = field->object, .enabled = r->setting || r->cap};
    if (!lv_obj_get_child_count(slot)) {
        ink_ui_line(slot, "", INK_FONT_LIST, INK_WEIGHT_MEDIUM, INK_GAP, INK_GAP, INK_CANVAS_W - 2 * INK_MARGIN - 2 * INK_GAP);
        ink_ui_label(slot, "", INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_GAP,
            INK_GAP + ink_ui_line_height(INK_FONT_LIST) + INK_GAP_XS, INK_CANVAS_W - 2 * INK_MARGIN - 2 * INK_GAP);
    }
    lv_obj_set_style_bg_color(slot, S.page == PRIVACY ? ink_ui_paper() : ink_ui_gray(), 0);
    lv_obj_set_style_border_width(slot, 0, 0);
    lv_label_set_text(lv_obj_get_child(slot, 0), r->title);
    lv_label_set_text(lv_obj_get_child(slot, 1), S.values[i]);
    lv_obj_remove_flag(lv_obj_get_child(slot, 0), LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(lv_obj_get_child(slot, 1), LV_OBJ_FLAG_HIDDEN);
    while (lv_obj_get_child_count(slot) > 2) lv_obj_delete(lv_obj_get_child(slot, 2));
    if (r->setting && strcmp(r->setting, "voice.handsfree")) {
        lv_obj_add_flag(lv_obj_get_child(slot, 0), LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(lv_obj_get_child(slot, 1), LV_OBJ_FLAG_HIDDEN);
        ink_ui_toggle(slot, r->title, ink_setting_get(r->setting) != 0, INK_GAP, INK_GAP,
            INK_CANVAS_W - 2 * INK_MARGIN - 2 * INK_GAP, NULL, NULL);
    } else if (r->setting) {
        const char *const choices[] = {"使用时", "一直", "关"};
        lv_obj_add_flag(lv_obj_get_child(slot, 1), LV_OBJ_FLAG_HIDDEN);
        lv_obj_t *segments = ink_ui_segments(slot, choices, 3, (int)ink_setting_get(r->setting), INK_GAP,
            INK_GAP + ink_ui_line_height(INK_FONT_LIST) + INK_GAP_XS, INK_CANVAS_W - 2 * INK_MARGIN - 2 * INK_GAP, NULL, NULL);
        for (int k = 0; k < 3; k++) {
            char args[96]; snprintf(args, sizeof args, "{\"id\":\"voice.handsfree\",\"value\":%d}", k);
            ink_shell_bind(lv_obj_get_child(segments, k), "settings.set", args);
        }
    }
    if (!action->enabled) lv_obj_remove_flag(slot, LV_OBJ_FLAG_CLICKABLE);
    else lv_obj_add_flag(slot, LV_OBJ_FLAG_CLICKABLE);
}
static void refresh(void) { if (current()) ink_ui_list_refresh(S.list, PAGES[S.page].count); }
static void changed(void *ctx, int h, const char *id) {
    (void)ctx; (void)h; (void)id; refresh();
}
static void backup_reply(void *ctx, const ink_receipt_t *rc) {
    if ((uint32_t)(uintptr_t)ctx != S.generation || !current() || S.page != STORAGE) return;
    const char *text = "备份记录暂时读不出来";
    ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL);
    char phase[32] = ""; bool durable = false;
    if (j && rc->status == INK_RC_DONE && rc->result && !ink_json_parse(j, rc->result, strlen(rc->result))) {
        ink_json_str(j, ink_json_find(j, 0, "phase"), phase, sizeof phase);
        durable = ink_json_eq(j, ink_json_find(j, 0, "durable"), "true");
        text = !strcmp(phase, "none") ? "还没有备份" : !strcmp(phase, "saved") ?
            (durable ? "已备份" : "等待服务器保存回执") : !strcmp(phase, "done") ? "已恢复" :
            !strcmp(phase, "failed") ? "备份或恢复失败" : "备份或恢复正在进行";
    }
    osal_free(j); snprintf(S.backup, sizeof S.backup, "%s", text); refresh();
}
static void backup_changed(void *ctx, const char *id, const ink_args_t *args) {
    (void)ctx; (void)id; (void)args;
    if (current() && S.page == STORAGE) {
        ink_call_opts_t o = {.src = INK_SRC_SYSTEM, .member = S.member, .reply_exec = INK_EXEC_UI,
            .done = backup_reply, .ctx = (void *)(uintptr_t)S.generation};
        if (ink_cap_call("backup.status", NULL, &o)) {
            snprintf(S.backup, sizeof S.backup, "备份记录暂时读不出来"); refresh();
        }
    }
}
static void create(lv_obj_t *screen, void *arg) {
    S.page = (int)(intptr_t)arg; S.generation++;
    snprintf(S.member, sizeof S.member, "%s", ink_member_current()->id);
    memset(&S.position, 0, sizeof S.position);
    snprintf(S.backup, sizeof S.backup, "正在读取备份记录");
    ink_shell_statusbar(screen, true); ink_shell_topbar(screen, PAGES[S.page].title);
    S.body = ink_ui_box(screen, 0, INK_STATUS_BAR + INK_PRIMARY + INK_GAP_XS, INK_CANVAS_W,
        INK_CANVAS_H - INK_STATUS_BAR - INK_PRIMARY - INK_GAP_XS, 0, INK_SURFACE_WHITE, false);
    static const ink_ui_action_spec_t actions[] = {
        {.id = "prev", .label = "上一页", .capability = "shell.settings_prev"},
        {.id = "next", .label = "下一页", .capability = "shell.settings_next"}};
    ink_ui_page_spec_t page = {.id = PAGES[S.page].name, .title = PAGES[S.page].title,
        .object = {PAGES[S.page].name, 0}, .actions = actions, .action_count = 2};
    int h = S.page == VOICE ? 2 * INK_CONTROL : ink_ui_row_double();
    for (size_t i = 0; i < PAGES[S.page].count; i++) {
        value(i);
        int y = 2 * INK_GAP + ink_ui_line_height(INK_FONT_LIST) + INK_GAP_XS +
            ink_ui_text_height(S.values[i], INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_CANVAS_W - 2 * INK_MARGIN - 2 * INK_GAP);
        if (y > h) h = y;
    }
    int room = INK_CANVAS_H - INK_STATUS_BAR - INK_PRIMARY - INK_GAP_XS - INK_CONTROL - 2 * INK_GAP;
    unsigned capacity = (unsigned)(room / (h + INK_GAP_M));
    S.list = ink_ui_list_create(S.body, &page, &S.position, capacity, 1, h, "这里还没有内容", bind_row, NULL);
    refresh(); backup_changed(NULL, NULL, NULL);
}
static void destroy(void) { S.generation++; S.list = NULL; S.body = NULL; }
static int snapshot(char *out, size_t cap) { return current() ? ink_ui_list_snapshot(S.list, out, cap) : -1; }
static bool key(ink_key_t k, bool lng) { return current() && ink_ui_list_key(S.list, k, lng); }
static const ink_activity_t ACTIVITIES[] = {
    {.name = "SetVoice", .create = create, .destroy = destroy, .snapshot = snapshot, .on_key = key},
    {.name = "SetStorage", .create = create, .destroy = destroy, .snapshot = snapshot, .on_key = key},
    {.name = "SetAbout", .create = create, .destroy = destroy, .snapshot = snapshot, .on_key = key},
    {.name = "SetPrivacy", .create = create, .destroy = destroy, .snapshot = snapshot, .on_key = key}};
void ink_settings_page_open(const char *name) {
    if (ink_member_current()->role == INK_ROLE_CHILD || !ink_member_space_open()) return;
    for (size_t i = 0; i < sizeof PAGES / sizeof *PAGES; i++) if (!strcmp(name, PAGES[i].name)) {
        ink_shell_close(); ink_ui_push(&ACTIVITIES[i], (void *)(intptr_t)i); return;
    }
}
static void navigate(ink_call_t *c) {
    if (!strcmp(c->cap->id, "shell.settings_prev") || !strcmp(c->cap->id, "shell.settings_next")) {
        if (!current()) c->rc.status = INK_RC_STALE;
        else ink_ui_list_turn(S.list, !strcmp(c->cap->id, "shell.settings_next") ? 1 : -1);
    } else for (size_t i = 0; i < sizeof PAGES / sizeof *PAGES; i++)
        if (!strcmp(c->cap->id, PAGES[i].cap)) ink_settings_page_open(PAGES[i].name);
}
static const ink_cap_t CAPS[] = {
    {.id = "shell.voice", .title = "打开语音和 AI 设置", .effect = INK_FX_NAVIGATE, .run = navigate},
    {.id = "shell.storage", .title = "打开存储和备份设置", .effect = INK_FX_NAVIGATE, .run = navigate},
    {.id = "shell.about", .title = "打开关于和更新", .effect = INK_FX_NAVIGATE, .run = navigate},
    {.id = "shell.privacy", .title = "打开隐私", .effect = INK_FX_NAVIGATE, .run = navigate},
    {.id = "shell.settings_prev", .title = "设置上一页", .effect = INK_FX_NAVIGATE, .run = navigate},
    {.id = "shell.settings_next", .title = "设置下一页", .effect = INK_FX_NAVIGATE, .run = navigate}};
int ink_settings_pages_init(void) {
    if (ink_cap_register(CAPS, sizeof CAPS / sizeof *CAPS) || ink_settings_watch("", changed, NULL, INK_EXEC_UI) ||
        ink_state_watch("storage.", changed, NULL, INK_EXEC_UI) || ink_state_watch("update.", changed, NULL, INK_EXEC_UI)) return -1;
    return ink_cap_subscribe("backup.changed", backup_changed, NULL, INK_EXEC_UI);
}
