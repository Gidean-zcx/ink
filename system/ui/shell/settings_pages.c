/* Settings presenters share the existing F1/F4 bindings and policy gate. */
/* ui-scale-runtime: dimension h,w,y,head,left,title_w,room,body,cell_h Theme sizes plus measured text, bounded by the page body. */
/* ui-scale-runtime: dimension S.y,heading,row_height,row_body Measured section heights and cumulative positions using theme tokens. */
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
typedef struct { const char *id, *title, *value, *setting, *cap, *icon, *section, *note; } row_t;
static const row_t VOICE_ROWS[] = {
    {"voice.handsfree", "什么时候能叫名字", NULL, "voice.handsfree", NULL, NULL, NULL, "没锁屏时才听，不影响待机。"},
    {"voice.name", "用什么名字叫它", "暂不能更改", NULL, NULL, "wave", NULL, NULL},
    {"voice.interrupt", "说话能打断回答", "暂不能设置", NULL, NULL, "mic", NULL, NULL},
    {"ai.records", "AI 替你做过的事", NULL, NULL, "shell.ai_log", "list", "AI", NULL},
    {"ai.memory", "AI 记得的事", "查看", NULL, "shell.ai_memory", "ai", NULL, "你说的事 AI 会直接做。花钱、填密码、删了找不回来的，才会问你。"},
    {"text.assist", "打字时帮你接话", "暂不能设置", NULL, NULL, "edit", "打字", NULL}};
static const row_t STORAGE_ROWS[] = {
    {"backup", "备份", NULL, NULL, "backup.open", "cloud", NULL, NULL},
    {"storage.free", "空间", NULL, NULL, NULL, NULL, "空间", NULL},
    {"storage.books", "书", NULL, NULL, NULL, "book", NULL, NULL},
    {"storage.audio", "音频", NULL, NULL, NULL, "headphones", NULL, NULL},
    {"storage.photos", "照片和其他", NULL, NULL, NULL, "gallery", NULL, NULL},
    {"storage.clean", "清理临时文件", NULL, NULL, "file.cleanup", "trash", "", "仅清理临时文件、缓存和未传完的文件。"}};
static const row_t ABOUT_ROWS[] = {
    {"device", "这台设备", NULL, NULL, NULL, NULL, NULL, NULL},
    {"update.version", "系统版本", NULL, NULL, NULL, NULL, NULL, NULL},
    {"update.check", "系统更新", NULL, NULL, "shell.update", "update", NULL, NULL},
    {"privacy", "隐私", "查看", NULL, "shell.privacy", "shield", NULL, NULL},
    {"device.details", "设备详情", "暂不能查看", NULL, NULL, "display", "", NULL},
    {"licenses", "开源许可", "许可页面还没接入", NULL, NULL, "list", NULL, NULL},
    {"update.auto_download", "自动下载新版本", NULL, "update.auto_download", NULL, NULL, "", NULL},
    {"reset", "清空这台设备", "清掉所有家人的数据，找不回来", NULL, "factory.open", NULL, NULL, NULL}};
static const row_t PRIVACY_ROWS[] = {
    {"privacy.storage", "你的东西放在哪", "设置和备忘在设备里，书和录音在存储卡上；备份在家庭服务器上。", NULL, NULL, NULL, NULL, NULL},
    {"privacy.ai", "AI 能看到什么", "提问时能看到当前页。密码框里的字看不到；语音和页面会交给家庭选择的模型服务商。", NULL, NULL, NULL, NULL, NULL},
    {"privacy.retention", "留多久", "活动记录：大人 1 年，孩子 180 天。对话、记忆和运行记录的期限暂时查不到。", NULL, NULL, NULL, NULL, NULL},
    {"privacy.child", "孩子的记录", "爸爸妈妈可以查看孩子的活动记录，确认后删除；其他记录暂不能删除。", NULL, "shell.records", NULL, NULL, NULL},
    {"ai.read_clipboard", "AI 可以读剪贴板", NULL, "ai.read_clipboard", NULL, NULL, NULL, NULL},
    {"privacy.records", "查看和删除记录", "", NULL, "shell.records", NULL, NULL, NULL}};
static const struct { const char *name, *title, *cap; const row_t *rows; size_t count; } PAGES[] = {
    {"SetVoice", "语音和 AI", "shell.voice", VOICE_ROWS, sizeof VOICE_ROWS / sizeof *VOICE_ROWS},
    {"SetStorage", "存储和备份", "shell.storage", STORAGE_ROWS, sizeof STORAGE_ROWS / sizeof *STORAGE_ROWS},
    {"SetAbout", "关于和更新", "shell.about", ABOUT_ROWS, sizeof ABOUT_ROWS / sizeof *ABOUT_ROWS},
    {"SetPrivacy", "隐私", "shell.privacy", PRIVACY_ROWS, sizeof PRIVACY_ROWS / sizeof *PRIVACY_ROWS}};
static struct {
    int page;
    unsigned capacity;
    int y;
    uint32_t generation;
    char member[16], values[8][384], args[8][128], backup[96];
    lv_obj_t *body;
    ink_ui_list_t *list;
    ink_ui_list_position_t position;
} S INK_PSRAM_BSS;
static void storage_value(size_t i, char *out, size_t cap);
static void storage_action(size_t i, ink_ui_action_spec_t *action);
static void storage_request(void *ctx, const char *id, const ink_args_t *args);
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
    else if (S.page == STORAGE && i > 1) storage_value(i, out, cap);
    else if (!strcmp(r->id, "device")) snprintf(out, cap, "%s的设备", ink_member_current()->name);
    else if (!strcmp(r->id, "storage.free")) {
        int64_t present = 0; char mode[32] = "";
        state_number("storage.card", &present);
        ink_state_get_str(ink_state_find("storage.mode"), mode, sizeof mode);
        if (!present || !strcmp(mode, "usb"))
            snprintf(out, cap, "%s", !strcmp(mode, "usb") ? "存储卡正借给电脑" : "没有可用的存储卡");
        else if (state_number(r->id, &v) && v >= 0) {
            int64_t unit = v >= 1024LL * 1024 * 1024 ? 1024LL * 1024 * 1024 : v >= 1024 * 1024 ? 1024 * 1024 : 1024;
            snprintf(out, cap, "还能放 %lld.%lld %s", (long long)(v / unit), (long long)(v % unit * 10 / unit),
                unit > 1024 * 1024 ? "GB" : unit > 1024 ? "MB" : "KB");
        }
        else snprintf(out, cap, "暂时读不出来");
    } else if (!strcmp(r->id, "update.check")) {
        char state[32]; state_text("update.state", state, sizeof state);
        const char *text = !strcmp(state, "idle") ? "检查新版本" : !strcmp(state, "available") ? "有新版本" :
            !strcmp(state, "failed") ? "检查或更新失败，可以再试" : !strcmp(state, "ready") ? "已下载" : "正在更新";
        snprintf(out, cap, "%s", text);
    } else state_text(r->id, out, cap);
}
static bool paragraph(size_t i) { return S.page == PRIVACY && i < 4; }
static bool footer(size_t i) { return !strcmp(PAGES[S.page].rows[i].id, "reset") || !strcmp(PAGES[S.page].rows[i].id, "privacy.records"); }
static int heading(size_t i) {
    const char *text = PAGES[S.page].rows[i].section;
    return !text ? 0 : !text[0] ? INK_GAP_L : INK_GAP_L + ink_ui_line_height(INK_FONT_SUBTITLE) + INK_GAP_M;
}
static int row_body(size_t i) {
    const row_t *r = &PAGES[S.page].rows[i];
    int w = INK_CANVAS_W - 2 * INK_MARGIN;
    if (paragraph(i)) return ink_ui_line_height(INK_FONT_SUBTITLE) + INK_GAP_XS +
        ink_ui_text_height(S.values[i], INK_FONT_BODY, INK_WEIGHT_REGULAR, w) + INK_GAP_L;
    if (S.page == VOICE && !i) return ink_ui_line_height(INK_FONT_LIST) + INK_GAP_XS + INK_SEGMENT;
    if (S.page == ABOUT && !i) return INK_CIRCLE_APP + INK_GAP + ink_ui_line_height(INK_FONT_SUBTITLE);
    if (S.page == ABOUT && i == 1) return ink_ui_line_height(INK_FONT_NOTE) + INK_GAP_L;
    if (footer(i)) return INK_CONTROL + INK_GAP_XS + ink_ui_line_height(INK_FONT_NOTE);
    if (r->setting) return INK_CONTROL;
    return !r->icon || ink_ui_text_width(r->title, INK_FONT_LIST, INK_WEIGHT_MEDIUM) +
        ink_ui_text_width(S.values[i], INK_FONT_NOTE, INK_WEIGHT_REGULAR) + INK_GAP > w - INK_CIRCLE_SMALL - INK_GAP_XL - INK_GAP
        ? ink_ui_row_double() : INK_CONTROL;
}
static int row_height(size_t i) {
    const char *note = PAGES[S.page].rows[i].note;
    return heading(i) + row_body(i) + (note ? INK_GAP_XS +
        ink_ui_text_height(note, INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_CANVAS_W - 2 * INK_MARGIN) : 0);
}
static void bind_row(lv_obj_t *slot, size_t i, ink_ui_field_spec_t *field, ink_ui_action_spec_t *action, void *ctx) {
    (void)ctx;
    const row_t *r = &PAGES[S.page].rows[i];
    value(i);
    *field = (ink_ui_field_spec_t){.id = r->id, .role = r->setting ? "setting" : "text", .label = r->title,
        .value = S.values[i], .object = {r->id, 0}};
    *action = (ink_ui_action_spec_t){.id = r->id, .label = r->title, .capability = r->setting ? "settings.set" : r->cap,
        .args_json = r->setting ? S.args[i] : NULL, .object = field->object, .enabled = r->setting || r->cap};
    if (!strcmp(r->id, "privacy.child")) action->args_json = "{\"children\":true}";
    if (S.page == STORAGE) storage_action(i, action);
    int w = INK_CANVAS_W - 2 * INK_MARGIN, head = heading(i), body = row_body(i), h = row_height(i);
    if (!(i % S.capacity)) S.y = 0;
    if (footer(i)) S.y = INK_CANVAS_H - INK_STATUS_BAR - INK_PRIMARY - INK_GAP_XS - INK_CONTROL - 2 * INK_GAP - h;
    lv_obj_set_pos(slot, ink_ui_px(INK_MARGIN), ink_ui_px(S.y)); S.y += h + INK_STROKE;
    lv_obj_set_height(slot, ink_ui_px(h));
    lv_obj_set_style_bg_opa(slot, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(slot, 0, 0);
    lv_obj_set_style_radius(slot, 0, 0);
    lv_obj_set_style_clip_corner(slot, false, 0);
    if (!lv_obj_get_child_count(slot)) {
        ink_ui_box(slot, 0, 0, w, body, INK_RADIUS_CARD, INK_SURFACE_WHITE, false);
        ink_ui_label(slot, "", INK_FONT_LIST, INK_WEIGHT_MEDIUM, 0, 0, w);
        ink_ui_label(slot, "", INK_FONT_NOTE, INK_WEIGHT_REGULAR, 0, 0, w);
    }
    lv_obj_t *bg = lv_obj_get_child(slot, 0), *title = lv_obj_get_child(slot, 1), *text = lv_obj_get_child(slot, 2);
    lv_obj_remove_flag(title, LV_OBJ_FLAG_HIDDEN); lv_obj_remove_flag(text, LV_OBJ_FLAG_HIDDEN);
    while (lv_obj_get_child_count(slot) > 3) lv_obj_delete(lv_obj_get_child(slot, 3));
    lv_obj_clean(bg); lv_obj_set_pos(bg, 0, ink_ui_px(head)); lv_obj_set_height(bg, ink_ui_px(body));
    lv_obj_set_style_clip_corner(bg, false, 0);
    lv_obj_set_style_bg_color(bg, r->icon ? ink_ui_gray() : ink_ui_paper(), 0);
    lv_obj_set_style_radius(bg, footer(i) ? LV_RADIUS_CIRCLE : ink_ui_px(INK_RADIUS_CARD), 0);
    lv_obj_set_style_border_width(bg, !r->icon && (S.page == STORAGE || (footer(i) && S.page == PRIVACY)) ? ink_ui_px(INK_STROKE) : 0, 0);
    if (r->icon) {
        if (i % S.capacity && PAGES[S.page].rows[i - 1].icon && !r->section && !PAGES[S.page].rows[i - 1].note)
            ink_ui_box(bg, 0, 0, w, body / 2, 0, INK_SURFACE_STATIC, false);
        if ((i + 1) % S.capacity && i + 1 < PAGES[S.page].count && PAGES[S.page].rows[i + 1].icon && !PAGES[S.page].rows[i + 1].section && !r->note)
            ink_ui_box(bg, 0, body / 2, w, body - body / 2, 0, INK_SURFACE_STATIC, false);
        lv_obj_t *circle = ink_ui_circle(slot, r->icon, INK_GAP, head + (body - INK_CIRCLE_SMALL) / 2,
            INK_CIRCLE_SMALL, INK_ICON_MEDIUM, INK_SURFACE_WHITE, NULL, NULL);
        lv_obj_set_style_border_width(circle, 0, 0);
    }
    int left = r->icon ? INK_CIRCLE_SMALL + INK_GAP_XL : paragraph(i) || S.page == VOICE || S.page == ABOUT ? 0 : INK_GAP_XL;
    int y = head + (body - ink_ui_line_height(INK_FONT_LIST) - ink_ui_line_height(INK_FONT_NOTE) - INK_GAP_XS) / 2;
    lv_label_set_text(title, r->title); lv_label_set_text(text, S.values[i]);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_LEFT, 0); lv_obj_set_style_text_align(text, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_text_font(title, ink_ui_font_px(paragraph(i) ? INK_FONT_SUBTITLE : INK_FONT_LIST, INK_WEIGHT_MEDIUM), 0);
    lv_obj_set_style_text_font(text, ink_ui_font_px(paragraph(i) ? INK_FONT_BODY : INK_FONT_NOTE, INK_WEIGHT_REGULAR), 0);
    lv_obj_set_pos(title, ink_ui_px(left), ink_ui_px(paragraph(i) ? head : y)); lv_obj_set_width(title, ink_ui_px(w - left - (r->icon ? INK_GAP : 0)));
    lv_obj_set_pos(text, ink_ui_px(left), ink_ui_px(paragraph(i) ? head + ink_ui_line_height(INK_FONT_SUBTITLE) + INK_GAP_XS : y + ink_ui_line_height(INK_FONT_LIST) + INK_GAP_XS));
    lv_obj_set_width(text, ink_ui_px(w - left - (r->icon ? INK_GAP : 0)));
    if (r->icon && body == INK_CONTROL) {
        const char *caption = r->title;
        int title_w = ink_ui_text_width(caption, INK_FONT_LIST, INK_WEIGHT_MEDIUM);
        lv_obj_set_width(title, ink_ui_px(title_w));
        lv_obj_set_y(title, ink_ui_px(head + (body - ink_ui_line_height(INK_FONT_LIST)) / 2));
        lv_obj_set_pos(text, ink_ui_px(left + title_w + INK_GAP), ink_ui_px(head + (body - ink_ui_line_height(INK_FONT_NOTE)) / 2));
        lv_obj_set_width(text, ink_ui_px(w - left - title_w - 2 * INK_GAP));
        lv_obj_set_style_text_align(text, LV_TEXT_ALIGN_RIGHT, 0);
    }
    if (r->section && r->section[0]) ink_ui_label(slot, r->section, INK_FONT_SUBTITLE, INK_WEIGHT_BOLD, 0, INK_GAP_L, w);
    if (r->note) ink_ui_label(slot, r->note, INK_FONT_NOTE, INK_WEIGHT_REGULAR, 0, head + body + INK_GAP_XS, w);
    if (r->setting && strcmp(r->setting, "voice.handsfree")) {
        lv_obj_add_flag(title, LV_OBJ_FLAG_HIDDEN); lv_obj_add_flag(text, LV_OBJ_FLAG_HIDDEN);
        ink_ui_toggle(slot, r->title, ink_setting_get(r->setting) != 0, 0, head, w, NULL, NULL);
    } else if (r->setting) {
        const char *const choices[] = {"使用时", "一直", "关"};
        lv_obj_set_y(title, 0); lv_obj_add_flag(text, LV_OBJ_FLAG_HIDDEN);
        lv_obj_t *segments = ink_ui_segments(slot, choices, 3, (int)ink_setting_get(r->setting), 0,
            ink_ui_line_height(INK_FONT_LIST) + INK_GAP_XS, w, NULL, NULL);
        for (int k = 0; k < 3; k++) {
            char args[96]; snprintf(args, sizeof args, "{\"id\":\"voice.handsfree\",\"value\":%d}", k);
            ink_shell_bind(lv_obj_get_child(segments, k), "settings.set", args);
        }
    } else if (S.page == ABOUT && i < 2) {
        lv_obj_add_flag(title, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_pos(text, 0, ink_ui_px(i ? 0 : INK_CIRCLE_APP + INK_GAP)); lv_obj_set_width(text, ink_ui_px(w));
        lv_obj_set_style_text_align(text, LV_TEXT_ALIGN_CENTER, 0);
        if (!i) {
            lv_obj_set_style_text_font(text, ink_ui_font_px(INK_FONT_SUBTITLE, INK_WEIGHT_BOLD), 0);
            ink_ui_circle(slot, "ai", (w - INK_CIRCLE_APP) / 2, 0, INK_CIRCLE_APP, INK_ICON_APP, INK_SURFACE_BLACK, NULL, NULL);
        }
    } else if (S.page == STORAGE && i == 1) {
        lv_obj_add_flag(title, LV_OBJ_FLAG_HIDDEN); lv_obj_set_y(text, ink_ui_px(head + (body - ink_ui_line_height(INK_FONT_LIST)) / 2));
        lv_obj_set_style_text_font(text, ink_ui_font_px(INK_FONT_LIST, INK_WEIGHT_REGULAR), 0);
    } else if (footer(i)) {
        lv_obj_set_pos(title, 0, ink_ui_px((INK_CONTROL - ink_ui_line_height(INK_FONT_LIST)) / 2)); lv_obj_set_width(title, ink_ui_px(w));
        lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(text, 0, ink_ui_px(INK_CONTROL + INK_GAP_XS)); lv_obj_set_width(text, ink_ui_px(w));
        lv_obj_set_style_text_align(text, LV_TEXT_ALIGN_CENTER, 0); lv_obj_set_height(bg, ink_ui_px(INK_CONTROL));
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
    int room = INK_CANVAS_H - INK_STATUS_BAR - INK_PRIMARY - INK_GAP_XS - INK_CONTROL - 2 * INK_GAP;
    for (size_t i = 0; i < PAGES[S.page].count; i++) value(i);
    /* Keep the voice controls together; every F4 page must fit the measured text. */
    for (S.capacity = S.page == VOICE ? 3 : (unsigned)PAGES[S.page].count; S.capacity > 1; S.capacity--) {
        bool fits = true; int y = 0;
        for (size_t i = 0; i < PAGES[S.page].count; i++) {
            if (!(i % S.capacity)) y = 0;
            y += row_height(i) + INK_STROKE;
            if (y > room) fits = false;
        }
        if (fits) break;
    }
    int cell_h = room / (int)S.capacity - INK_GAP_M;
    S.list = ink_ui_list_create(S.body, &page, &S.position, S.capacity, 1, cell_h, "这里还没有内容", bind_row, NULL);
    refresh(); backup_changed(NULL, NULL, NULL); storage_request(NULL, NULL, NULL);
}
static void destroy(void) { S.generation++; S.list = NULL; S.body = NULL; }
static int snapshot(char *out, size_t cap) {
    if (!current()) return -1;
    int n = ink_ui_list_snapshot(S.list, out, cap);
    if (n < 0) return -1;
    ink_jw_t w = {.buf = out, .cap = cap, .len = (size_t)n - 1};
    ink_jw_raw(&w, ",\"notes\":["); bool comma = false;
    size_t end = (S.position.page + 1) * S.capacity;
    for (size_t i = S.position.page * S.capacity; i < end && i < PAGES[S.page].count; i++) {
        const row_t *r = &PAGES[S.page].rows[i]; const char *texts[] = {r->section, r->note};
        for (size_t k = 0; k < 2; k++) if (texts[k] && texts[k][0]) {
            if (comma) ink_jw_raw(&w, ",");
            ink_jw_str(&w, texts[k]); comma = true;
        }
    }
    ink_jw_raw(&w, "]}");
    if (w.overflow) { out[0] = 0; return -1; }
    return (int)w.len;
}
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
    if (ink_cap_subscribe("file.changed", storage_request, NULL, INK_EXEC_UI)) return -1;
    return ink_cap_subscribe("backup.changed", backup_changed, NULL, INK_EXEC_UI);
}

static struct { uint32_t generation; int state[2]; int64_t bytes[2][4], clean[3], revision; } U INK_PSRAM_BSS;
static void storage_size(int64_t n, char *out, size_t cap) {
    int64_t unit = n >= 1073741824 ? 1073741824 : n >= 1048576 ? 1048576 : n >= 1024 ? 1024 : 1;
    if (unit == 1) snprintf(out, cap, "%lld B", (long long)n);
    else snprintf(out, cap, "%lld.%lld %s", (long long)(n / unit), (long long)(n % unit * 10 / unit),
        unit == 1024 ? "KB" : unit == 1048576 ? "MB" : "GB");
}
static void storage_value(size_t i, char *out, size_t cap) {
    int state = U.generation == S.generation ? U.state[i == 5] : 0;
    if (state != 1) { snprintf(out, cap, "%s", state ? "暂时读不出来" : "正在整理"); return; }
    char a[32], b[32];
    if (i == 5) {
        storage_size(U.clean[0] + U.clean[1] + U.clean[2], a, sizeof a);
        snprintf(out, cap, "能腾出 %s", a);
    } else {
        int k = (int)i - 2;
        storage_size(U.bytes[0][k] + (k == 2 ? U.bytes[0][3] : 0), a, sizeof a);
        storage_size(U.bytes[1][k] + (k == 2 ? U.bytes[1][3] : 0), b, sizeof b);
        snprintf(out, cap, "我 %s / 全机 %s", a, b);
    }
}
static void storage_action(size_t i, ink_ui_action_spec_t *action) {
    if (i != 5) return;
    action->capability = U.clean[0] || U.clean[2] ? "file.cleanup" : "file.cache_clear";
    snprintf(S.args[i], sizeof S.args[i], "{\"revision\":%lld}", (long long)U.revision);
    action->args_json = S.args[i];
    action->enabled = U.generation == S.generation && U.state[1] == 1 && U.clean[0] + U.clean[1] + U.clean[2] > 0;
}
static void storage_reply(void *ctx, const ink_receipt_t *rc, bool clean) {
    if ((uint32_t)(uintptr_t)ctx != S.generation || !current() || S.page != STORAGE) return;
    ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL);
    U.state[clean] = 2;
    if (j && rc->status == INK_RC_DONE && rc->result && !ink_json_parse(j, rc->result, strlen(rc->result))) {
        if (!clean && strstr(rc->result, "\"organizing\"")) { U.state[0] = 0; osal_free(j); refresh(); return; }
        U.state[clean] = 1;
        static const char *const keys[] = {"books", "audio", "photos", "other", "temporary", "cache", "partial"};
        for (int s = 0; s < (clean ? 1 : 2); s++) {
            int root = clean ? 0 : ink_json_find(j, 0, s ? "device" : "member");
            for (int k = 0; k < (clean ? 3 : 4); k++)
                ink_json_int(j, ink_json_find(j, root, keys[clean ? k + 4 : k]), clean ? &U.clean[k] : &U.bytes[s][k]);
        }
        if (clean) ink_json_int(j, ink_json_find(j, 0, "revision"), &U.revision);
    }
    osal_free(j); refresh();
}
static void storage_usage_reply(void *ctx, const ink_receipt_t *rc) { storage_reply(ctx, rc, false); }
static void storage_clean_reply(void *ctx, const ink_receipt_t *rc) { storage_reply(ctx, rc, true); }
static void storage_request(void *ctx, const char *id, const ink_args_t *args) {
    (void)ctx; (void)id; (void)args;
    if (!current() || S.page != STORAGE) return;
    memset(&U, 0, sizeof U); U.generation = S.generation;
    ink_call_opts_t o = {.src = INK_SRC_SYSTEM, .member = S.member, .reply_exec = INK_EXEC_UI,
        .done = storage_usage_reply, .ctx = (void *)(uintptr_t)S.generation};
    if (ink_cap_call("file.usage", NULL, &o)) U.state[0] = 2;
    o.done = storage_clean_reply;
    if (ink_cap_call("file.cleanup_preview", NULL, &o)) U.state[1] = 2;
    refresh();
}
