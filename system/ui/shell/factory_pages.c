/* ui-scale-runtime: dimension x,y,w,h Theme-derived page and row positions. */
/* ui-scale-runtime: dimension heights[0],heights[1],at,left,button_h Measured complete text lines plus theme padding and control minimums. */
#include "ink/sys/i18n.h"
#include <stdio.h>
#include <string.h>
#include "ink/osal.h"
#include "ink/sys/json.h"
#include "ink/sys/members.h"
#include "ink/sys/time.h"
#include "ink/sys/ui_semantics.h"
#include "ink/ui_shell.h"

static struct {
    lv_obj_t *screen, *status, *backup_button;
    lv_timer_t *timer;
    uint32_t epoch, generation;
    bool media, done, pending;
    char backup[256], member[64];
    ink_ui_page_spec_t model;
    ink_ui_field_spec_t fields[5];
    ink_ui_action_spec_t actions[4];
} F INK_PSRAM_BSS;
static void create(lv_obj_t *screen, void *arg);
static void destroy(void) { if (F.timer) lv_timer_delete(F.timer); F.timer = NULL; F.screen = NULL; F.generation++; }
static int snapshot(char *out, size_t cap) { return ink_ui_semantics_snapshot(&F.model, out, cap); }
static const ink_activity_t RESET = {.name = "FactoryReset", .app = "factory", .create = create, .destroy = destroy, .snapshot = snapshot};
static const ink_activity_t DONE = {.name = "FactoryDone", .app = "factory", .create = create, .destroy = destroy, .snapshot = snapshot};
static bool current(void) { return F.screen && F.epoch == ink_member_epoch() && !strcmp(ink_ui_page_name(), F.done ? DONE.name : RESET.name); }
static void reply(void *ctx, const ink_receipt_t *rc) {
    if (!current() || (uint32_t)(uintptr_t)ctx != F.generation) return;
    F.pending = false;
    ink_ui_busy_control(F.backup_button, false);
    lv_obj_set_style_text_color(lv_obj_get_child(F.backup_button, 0), ink_ui_ink(), 0);
    ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL); char text[256];
    snprintf(text, sizeof text, "%s", TR("备份状态暂时读不出来，请稍后再试"));
    if (j && rc->status == INK_RC_DONE && rc->result && !ink_json_parse(j, rc->result, strlen(rc->result))) {
        int64_t saved = 0; ink_json_int(j, ink_json_find(j, 0, "saved_s"), &saved);
        if (saved > 0) {
            ink_tm_t tm; ink_time_to_local(saved, &tm);
            int p = ink_json_find(j, 0, "unbacked");
            snprintf(text, sizeof text, TR("上次备份：%d月%d日 %02d:%02d。%s"), tm.month, tm.day, tm.hour, tm.min,
                ink_json_eq(j, p, "true") ? TR("之后有东西还没备份。") : ink_json_eq(j, p, "false") ? TR("之后没有未备份的东西。") : TR("之后的改动暂不能核实。先备份再继续。 "));
        } else snprintf(text, sizeof text, TR("这位家人还没有备份。清空前请先备份。"));
    }
    osal_free(j);
    if (strcmp(text, F.backup)) { snprintf(F.backup, sizeof F.backup, "%s", text); lv_label_set_text(F.status, F.backup); ink_ui_content_changed(); }
}
static void poll(lv_timer_t *timer) {
    (void)timer;
    if (!current() || F.done || F.pending) return;
    ink_call_opts_t o = {.src = INK_SRC_SYSTEM, .args_json = "{\"changes\":true}", .reply_exec = INK_EXEC_UI, .done = reply,
        .ctx = (void *)(uintptr_t)F.generation, .member = ink_member_current()->id};
    F.pending = true;
    if (ink_cap_call("backup.status", NULL, &o)) F.pending = false;
}
static void click(lv_event_t *e) {
    if (!current()) return;
    const ink_ui_action_spec_t *a = lv_event_get_user_data(e);
    if (!strcmp(a->capability, "backup.now")) ink_ui_busy_control(lv_event_get_target_obj(e), true);
    if (!strcmp(a->capability, "backup.now")) lv_obj_set_style_text_color(lv_obj_get_child(F.backup_button, 0), ink_ui_paper(), 0);
    ink_call_opts_t o = {.src = INK_SRC_TOUCH, .args_json = a->args_json, .member = ink_member_current()->id};
    ink_cap_call(a->capability, NULL, &o);
}
static ink_ui_action_spec_t *action(const char *label, const char *cap, const char *args) {
    ink_ui_action_spec_t *a = &F.actions[F.model.action_count++];
    *a = (ink_ui_action_spec_t){cap, label, cap, args, {NULL, 0}, true}; return a;
}
static void draw(void) {
    lv_obj_clean(F.screen);
    int w = INK_CANVAS_W - 2 * INK_MARGIN, y = INK_STATUS_BAR + INK_CONTROL + INK_GAP;
    const char *name = F.done ? DONE.name : RESET.name;
    F.model = (ink_ui_page_spec_t){name, F.done ? TR("已清空") : TR("清空这台设备"), {name, 0}, F.fields, F.done ? 2 : 5, F.actions, 0};
    F.fields[0] = (ink_ui_field_spec_t){"member", "text", TR("家人"), F.member, {NULL, 0}, false};
    ink_shell_statusbar(F.screen, !F.done);
    if (F.done) {
        F.fields[1] = (ink_ui_field_spec_t){"result", "text", TR("说明"), TR("家庭服务器上的备份仍保留。本机已清空；服务器是否已移除这台设备，请在手机上核对。"), {NULL, 0}, false};
        ink_ui_circle(F.screen, "check", (INK_CANVAS_W - INK_CIRCLE_LARGE) / 2, INK_CANVAS_H / 2 - INK_CIRCLE_LARGE, INK_CIRCLE_LARGE, INK_ICON_STATE, INK_SURFACE_BLACK, NULL, NULL);
        lv_obj_t *t = ink_ui_label(F.screen, TR("这台设备已经清空了"), INK_FONT_SUBTITLE, INK_WEIGHT_BOLD, INK_MARGIN, INK_CANVAS_H / 2 + INK_GAP_XL, w);
        lv_obj_set_style_text_align(t, LV_TEXT_ALIGN_CENTER, 0);
        t = ink_ui_label(F.screen, F.fields[1].value, INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_MARGIN, INK_CANVAS_H / 2 + INK_GAP_XL + INK_LINE_SUBTITLE + INK_GAP, w);
        lv_obj_set_style_text_align(t, LV_TEXT_ALIGN_CENTER, 0);
        ink_ui_pill(F.screen, TR("重新设置设备"), INK_MARGIN, INK_CANVAS_H - INK_MARGIN - INK_PRIMARY, w, INK_PRIMARY, true, click, action(TR("重新设置设备"), "factory.finish", NULL)); return;
    }
    ink_shell_topbar(F.screen, TR("清空这台设备")); action(TR("返回"), "shell.back", NULL);
    const char *titles[] = {TR("会清除（所有家人）"), F.media ? TR("备份保留，卡上另选") : TR("不会动")};
    const char *rows[] = {TR("家人、设置和 Wi-Fi"), TR("备忘、录音、照片（含卡上个人资料）"), TR("家庭服务器上的备份"), TR("存储卡的书和音频")};
    const char *icons[] = {"family", "memo", "cloud", "sd"};
    for (int i = 0; i < 2; i++) {
        ink_ui_label(F.screen, titles[i], INK_FONT_LIST, INK_WEIGHT_BOLD, INK_MARGIN, y, w);
        y += ink_ui_text_height(titles[i], INK_FONT_LIST, INK_WEIGHT_BOLD, w) + INK_GAP_XS;
        int heights[2], left = INK_CIRCLE_SMALL + INK_GAP_XL, at = 0;
        const char *note = F.media ? TR("删除，找不回来") : TR("默认保留");
        for (int k = 0; k < 2; k++) {
            int text_h = ink_ui_text_height(rows[2 * i + k], INK_FONT_NOTE, INK_WEIGHT_MEDIUM,
                w - left - (i && k ? INK_PRIMARY + INK_GAP : INK_GAP));
            if (i && k) text_h += INK_GAP_XS + ink_ui_text_height(note, INK_FONT_NOTE, INK_WEIGHT_REGULAR, w - left - INK_PRIMARY - INK_GAP);
            heights[k] = LV_MAX(INK_CONTROL, text_h + 2 * INK_GAP_XS);
        }
        lv_obj_t *card = ink_ui_card(F.screen, INK_MARGIN, y, w, heights[0] + heights[1], INK_SURFACE_STATIC);
        for (int k = 0; k < 2; k++) {
            ink_ui_circle(card, icons[2 * i + k], INK_GAP, at + (heights[k] - INK_CIRCLE_SMALL) / 2, INK_CIRCLE_SMALL, INK_ICON_SMALL, INK_SURFACE_WHITE, NULL, NULL);
            ink_ui_label(card, rows[2 * i + k], INK_FONT_NOTE, INK_WEIGHT_MEDIUM, left, at + INK_GAP_XS, w - left - (i && k ? INK_PRIMARY + INK_GAP : INK_GAP));
            at += heights[k];
        }
        ink_ui_group_divider(card, heights[0], w);
        if (i) {
            ink_ui_label(card, note, INK_FONT_NOTE, INK_WEIGHT_REGULAR, left, heights[0] + 2 * INK_GAP_XS +
                ink_ui_text_height(rows[3], INK_FONT_NOTE, INK_WEIGHT_MEDIUM, w - left - INK_PRIMARY - INK_GAP), w - left - INK_PRIMARY - INK_GAP);
            ink_ui_toggle(card, "", F.media, w - INK_PRIMARY - INK_GAP, heights[0], INK_PRIMARY, click, action(TR("一起清空存储卡上的书和音频"), "factory.media", F.media ? "{\"clear_media\":false}" : "{\"clear_media\":true}"));
        }
        y += heights[0] + heights[1] + INK_GAP_M;
    }
    F.status = ink_ui_label(F.screen, F.backup, INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_MARGIN, y, w);
    F.fields[1] = (ink_ui_field_spec_t){"cleared", "text", TR("会清除"), TR("所有家人的资料、设置和 Wi-Fi；含卡上个人资料及缓存"), {NULL, 0}, false};
    F.fields[2] = (ink_ui_field_spec_t){"kept", "text", TR("不会动"), F.media ? TR("家庭服务器上的备份；卡上其他共享文件") : TR("家庭服务器上的备份；卡上的书和音频及其他共享文件"), {NULL, 0}, false};
    F.fields[3] = (ink_ui_field_spec_t){"media", "toggle", TR("存储卡上的书和音频也一起清空"), F.media ? TR("开") : TR("关"), {NULL, 0}, false};
    F.fields[4] = (ink_ui_field_spec_t){"backup", "text", TR("这位家人的备份"), F.backup, {NULL, 0}, false};
    int button_h = LV_MAX(INK_CONTROL, ink_ui_text_height(TR("验证并清空"), INK_FONT_LIST, INK_WEIGHT_MEDIUM, (w - INK_GAP) / 2 - 2 * INK_GAP_XS) + 2 * INK_GAP_XS);
    F.backup_button = ink_ui_pill(F.screen, TR("先备份"), INK_MARGIN, INK_CANVAS_H - INK_MARGIN - button_h, (w - INK_GAP) / 2, button_h, false, click, action(TR("先备份"), "backup.now", NULL));
    ink_ui_pill(F.screen, TR("验证并清空"), INK_MARGIN + (w + INK_GAP) / 2, INK_CANVAS_H - INK_MARGIN - button_h, (w - INK_GAP) / 2, button_h, true, click, action(TR("验证并清空"), "system.factory_reset", F.media ? "{\"clear_media\":true}" : "{\"clear_media\":false}"));
}
static void create(lv_obj_t *screen, void *arg) {
    F.screen = screen; F.done = arg != NULL; F.epoch = ink_member_epoch(); F.generation++; F.pending = false;
    snprintf(F.member, sizeof F.member, "%s", ink_member_current()->name); draw();
    if (!F.done) { F.timer = lv_timer_create(poll, 1000, NULL); poll(NULL); }
}
void ink_factory_done_open(void) { ink_ui_push(&DONE, (void *)1); }
static void navigate(ink_call_t *c) {
    if (!strcmp(c->cap->id, "factory.open")) { F.media = false; snprintf(F.backup, sizeof F.backup, TR("正在读取备份记录")); ink_shell_close(); ink_ui_drop_app("factory"); ink_ui_push(&RESET, NULL); }
    else if (!current() || F.done != !strcmp(c->cap->id, "factory.finish")) c->rc.status = INK_RC_STALE;
    else if (F.done) { ink_ui_drop_app("factory"); ink_cap_invoke_json("shell.launch", "{\"id\":\"boot\"}", c->src, 0, &c->rc); }
    else { ink_args_get_bool(&c->args, "clear_media", &F.media); draw(); ink_ui_content_changed(); }
}
int ink_factory_pages_init(void) {
    static const ink_cap_t caps[] = {
        {.id = "factory.open", .title = "查看清空这台设备的范围", .effect = INK_FX_NAVIGATE, .run = navigate},
        {.id = "factory.media", .title = "选择是否一起清空存储卡上的书和音频", .input = "{\"type\":\"object\",\"properties\":{\"clear_media\":{\"type\":\"boolean\"}},\"required\":[\"clear_media\"]}", .effect = INK_FX_NAVIGATE, .run = navigate},
        {.id = "factory.finish", .title = "清空后开始开机设置", .effect = INK_FX_NAVIGATE, .run = navigate}};
    return ink_cap_register(caps, sizeof caps / sizeof *caps);
}
