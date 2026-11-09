/* Member-scoped backup presenters; the existing service owns every operation. */
/* ui-scale-runtime: dimension x,y,w Theme-derived layout coordinates. */
/* ui-scale-runtime: font font Only theme text sizes are passed by the presenter. */
#include <stdio.h>
#include <string.h>
#include "ink/osal.h"
#include "ink/sys/cap.h"
#include "ink/sys/json.h"
#include "ink/sys/members.h"
#include "ink/sys/state.h"
#include "ink/sys/time.h"
#include "ink/sys/ui_semantics.h"
#include "ink/ui_shell.h"

enum { BACKUP, BACKUP_FAIL, RESTORE, RESTORE_RUN, RESTORE_DONE, RESTORE_FAIL };
static const char *const NAMES[] = {"Backup", "BackupFail", "Restore", "RestoreRun", "RestoreDone", "RestoreFail"};
static const char OMITTED[] = "设置、家人身份和密码需重新设置；网络和家庭需重新加入。共享内容按需下载。已发短信、已执行的操作和过期提醒不重放。本机 AI 操作记录和清单外文件保留。";
static struct {
    lv_obj_t *screen;
    lv_timer_t *timer;
    uint32_t epoch, generation;
    int page;
    bool pending, restore;
    char member[32], phase[24], preview[33], args[80], title[96], info[160], detail[256];
    ink_ui_page_spec_t model;
    ink_ui_field_spec_t fields[5];
    ink_ui_action_spec_t actions[4];
} B INK_PSRAM_BSS;
static void create(lv_obj_t *screen, void *arg);
static void destroy(void);
static int snapshot(char *out, size_t cap) { return ink_ui_semantics_snapshot(&B.model, out, cap); }
#define PAGE(n_) {.name = n_, .app = "backup", .create = create, .destroy = destroy, .snapshot = snapshot}
static const ink_activity_t PAGES[] = {PAGE("Backup"), PAGE("BackupFail"), PAGE("Restore"), PAGE("RestoreRun"), PAGE("RestoreDone"), PAGE("RestoreFail")};
static bool current(void) {
    return B.screen && B.epoch == ink_member_epoch() && ink_ui_page_current() &&
        !strcmp(ink_ui_page_name(), NAMES[B.page]);
}
static void draw(void);
static void show(int page) {
    if (page == B.page) { ink_ui_content_changed(); draw(); return; }
    ink_ui_drop_app("backup");
    ink_ui_push(&PAGES[page], (void *)(intptr_t)page);
}
static void reply(void *ctx, const ink_receipt_t *rc);
static void request(const char *cap, const char *args) {
    ink_call_opts_t opts = {.src = INK_SRC_SYSTEM, .args_json = args, .reply_exec = INK_EXEC_UI,
        .done = reply, .ctx = (void *)(uintptr_t)B.generation, .member = ink_member_current()->id};
    B.pending = ink_cap_call(cap, NULL, &opts) == 0;
}
static void poll(lv_timer_t *timer) {
    (void)timer;
    if (current() && !B.pending) request("backup.status", NULL);
}
static void clicked(lv_event_t *e) {
    if (!current()) return;
    const ink_ui_action_spec_t *a = lv_event_get_user_data(e);
    if (!strcmp(a->capability, "restore.preview")) B.restore = true;
    if (!strcmp(a->capability, "backup.now")) B.restore = false;
    ink_call_opts_t opts = {.src = INK_SRC_TOUCH, .args_json = a->args_json, .reply_exec = INK_EXEC_UI,
        .done = !strcmp(a->capability, "backup.now") ? NULL : reply, .ctx = (void *)(uintptr_t)B.generation, .member = ink_member_current()->id};
    if (strcmp(a->capability, "shell.back") && strcmp(a->capability, "system.home"))
        ink_ui_busy_control(lv_event_get_target_obj(e), true);
    ink_cap_call(a->capability, NULL, &opts);
}
static void button(const char *label, const char *cap, const char *args, int x, int y, int w, bool primary) {
    ink_ui_action_spec_t *a = &B.actions[B.model.action_count++];
    *a = (ink_ui_action_spec_t){cap, label, cap, args, {NULL, 0}, true};
    ink_ui_pill(B.screen, label, x, y, w, INK_PRIMARY, primary, clicked, a);
}
static void text(const char *s, int font, int y, bool center) {
    lv_obj_t *label = ink_ui_label(B.screen, s, font, font == INK_FONT_NOTE ? INK_WEIGHT_REGULAR : INK_WEIGHT_BOLD,
        INK_MARGIN, y, INK_CANVAS_W - 2 * INK_MARGIN);
    if (center) lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
}
static void draw(void) {
    lv_obj_clean(B.screen);
    B.model = (ink_ui_page_spec_t){NAMES[B.page], B.restore ? "恢复备份" : "备份", {NAMES[B.page], 0}, B.fields, 5, B.actions, 1};
    B.actions[0] = (ink_ui_action_spec_t){"back", "返回", "shell.back", NULL, {NULL, 0}, true};
    B.fields[0] = (ink_ui_field_spec_t){"member", "text", "家人", B.member, {NULL, 0}, false};
    B.fields[1] = (ink_ui_field_spec_t){"status", "text", "状态", B.title, {NULL, 0}, false};
    B.fields[2] = (ink_ui_field_spec_t){"summary", "text", "备份摘要", B.info, {NULL, 0}, false};
    B.fields[3] = (ink_ui_field_spec_t){"details", "text", "说明", B.detail, {NULL, 0}, false};
    B.fields[4] = (ink_ui_field_spec_t){"omitted", "text", "不恢复的内容", B.restore ? OMITTED : "自动备份暂不能设置", {NULL, 0}, false};
    ink_shell_statusbar(B.screen, true);
    int w = INK_CANVAS_W - 2 * INK_MARGIN, bottom = INK_CANVAS_H - INK_MARGIN - INK_PRIMARY;
    int middle = INK_CANVAS_H / 2, icon_y = middle - INK_CIRCLE_LARGE;
    if (B.page != RESTORE_RUN && B.page != RESTORE_DONE) ink_shell_topbar(B.screen, B.model.title);
    if (B.page == BACKUP || B.page == BACKUP_FAIL || B.page == RESTORE_FAIL) {
        ink_ui_circle(B.screen, B.page == BACKUP ? "check" : B.page == BACKUP_FAIL ? "cloudoff" : "warn",
            (INK_CANVAS_W - INK_CIRCLE_LARGE) / 2, icon_y, INK_CIRCLE_LARGE, INK_ICON_STATE,
            B.page == BACKUP && !strcmp(B.phase, "saved") ? INK_SURFACE_BLACK : INK_SURFACE_STATIC, NULL, NULL);
        text(B.title, INK_FONT_SUBTITLE, middle + INK_GAP_XL, true);
        text(B.page == BACKUP ? B.info : B.detail, INK_FONT_NOTE, middle + INK_GAP_XL + INK_LINE_SUBTITLE + INK_GAP, true);
    }
    if (B.page == BACKUP) {
        text("只备份这位家人的资料；自动备份暂不能设置。", INK_FONT_NOTE, bottom - 2 * INK_PRIMARY - INK_GAP_L, true);
        button("恢复备份", "restore.preview", "{\"summary\":true}", INK_MARGIN, bottom - INK_PRIMARY - INK_GAP, w, false);
        button("现在备份一次", "backup.now", NULL, INK_MARGIN, bottom, w, false);
    } else if (B.page == BACKUP_FAIL) button("再试一次", "backup.now", NULL, INK_MARGIN, bottom, w, true);
    else if (B.page == RESTORE) {
        text(B.info, INK_FONT_NOTE, INK_STATUS_BAR + INK_PRIMARY + INK_GAP, false);
        lv_obj_t *card = ink_ui_card(B.screen, INK_MARGIN, 2 * INK_PRIMARY + INK_GAP, w, INK_ROW_DOUBLE, INK_SURFACE_STATIC);
        ink_ui_label(card, B.member, INK_FONT_LIST, INK_WEIGHT_BOLD, INK_GAP_XL, INK_GAP, w - 2 * INK_GAP_XL);
        ink_ui_label(card, B.title, INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_GAP_XL, INK_GAP + INK_LINE_LIST, w - 2 * INK_GAP_XL);
        text(OMITTED, INK_FONT_NOTE, 2 * INK_PRIMARY + INK_GAP + INK_ROW_DOUBLE + INK_GAP_L, false);
        text("这位家人现在的资料会被替换。", INK_FONT_NOTE, bottom - INK_LINE_NOTE - INK_GAP, true);
        button("取消", "shell.back", NULL, INK_MARGIN, bottom, (w - INK_GAP) / 2, false);
        char preview[sizeof B.preview]; memcpy(preview, B.preview, sizeof preview);
        snprintf(B.args, sizeof B.args, "{\"preview\":\"%s\"}", preview);
        button(B.preview[0] ? "恢复到这台设备" : "再查一次", B.preview[0] ? "restore.apply" : "restore.preview",
            B.preview[0] ? B.args : "{\"summary\":true}", INK_MARGIN + (w + INK_GAP) / 2, bottom, (w - INK_GAP) / 2, true);
    } else if (B.page == RESTORE_RUN) {
        text("正在放回来", INK_FONT_TITLE, INK_STATUS_BAR + INK_PRIMARY, false);
        const char *steps[] = {"下载备份", "检查是否完整", "放到这台设备上", "完成后请重启"};
        int active = !strcmp(B.phase, "checking") ? 1 : 2;
        for (int i = 0; i < 4; i++) {
            int y = 2 * INK_PRIMARY + INK_GAP_XL + i * INK_SEGMENT;
            if (i < active) ink_ui_circle(B.screen, "check", INK_MARGIN, y, INK_CIRCLE_SMALL,
                INK_ICON_SMALL, INK_SURFACE_BLACK, NULL, NULL);
            else ink_ui_box(B.screen, INK_MARGIN, y, INK_CIRCLE_SMALL, INK_CIRCLE_SMALL, -1, INK_SURFACE_WHITE, true);
            if (i == active) ink_ui_box(B.screen, INK_MARGIN + (INK_CIRCLE_SMALL - INK_ICON_SMALL) / 2,
                y + (INK_CIRCLE_SMALL - INK_ICON_SMALL) / 2, INK_ICON_SMALL, INK_ICON_SMALL, -1, INK_SURFACE_BLACK, false);
            ink_ui_label(B.screen, steps[i], INK_FONT_LIST, i == active ? INK_WEIGHT_BOLD : INK_WEIGHT_REGULAR,
                INK_MARGIN + INK_CIRCLE_SMALL + INK_GAP_L, y + INK_GAP, w - INK_CIRCLE_SMALL - INK_GAP_L);
        }
        text("不要关机。中途失败时，有些资料可能已经放回。", INK_FONT_NOTE, bottom, true);
    } else if (B.page == RESTORE_DONE) {
        text("放回来了", INK_FONT_TITLE, INK_STATUS_BAR + INK_GAP_XXL, false);
        lv_obj_t *card = ink_ui_card(B.screen, INK_MARGIN, 2 * INK_PRIMARY, w, INK_ROW_DOUBLE, INK_SURFACE_STATIC);
        ink_ui_label(card, B.member, INK_FONT_SUBTITLE, INK_WEIGHT_BOLD, INK_GAP_XL, INK_GAP, w - 2 * INK_GAP_XL);
        ink_ui_label(card, B.info, INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_GAP_XL, INK_GAP + INK_LINE_SUBTITLE, w - 2 * INK_GAP_XL);
        text("以下内容没有放回", INK_FONT_SUBTITLE, 2 * INK_PRIMARY + INK_ROW_DOUBLE + INK_GAP_XL, false);
        text(OMITTED, INK_FONT_NOTE, 2 * INK_PRIMARY + INK_ROW_DOUBLE + INK_GAP_XL + INK_LINE_SUBTITLE + INK_GAP, false);
        text("请重启设备，让放回的资料生效。", INK_FONT_NOTE, bottom - INK_LINE_NOTE - INK_GAP, false);
        button("开始用", "system.home", NULL, INK_MARGIN, bottom, w, true);
    } else {
        button("先不恢复", "shell.back", NULL, INK_MARGIN, bottom, (w - INK_GAP) / 2, false);
        button("再试一次", "restore.preview", "{\"summary\":true}", INK_MARGIN + (w + INK_GAP) / 2, bottom, (w - INK_GAP) / 2, true);
    }
}
static void reply(void *ctx, const ink_receipt_t *rc) {
    if (!current() || (uint32_t)(uintptr_t)ctx != B.generation) return;
    B.pending = false;
    if (rc->status == INK_RC_PENDING_CONFIRMATION || rc->status == INK_RC_CANCELLED) { draw(); return; }
    if (rc->status != INK_RC_DONE && rc->status != INK_RC_QUEUED) {
        snprintf(B.title, sizeof B.title, "%s", B.restore ? "没有恢复成功" : "还没备份好");
        snprintf(B.detail, sizeof B.detail, "%s", rc->say[0] ? rc->say : "暂时读不出来，请检查网络和存储卡后再试。");
        show(B.restore ? RESTORE_FAIL : BACKUP_FAIL); return;
    }
    ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL);
    if (!j || !rc->result || ink_json_parse(j, rc->result, strlen(rc->result))) { osal_free(j); return; }
    char phase[24] = "", reason[160] = "";
    ink_json_str(j, ink_json_find(j, 0, "phase"), phase, sizeof phase);
    bool changed = strcmp(B.phase, phase) != 0;
    if (!changed && (B.page == BACKUP_FAIL || B.page == RESTORE_FAIL) &&
        (!strcmp(phase, "none") || !strcmp(phase, "saved") || !strcmp(phase, "preview") || !strcmp(phase, "done"))) { osal_free(j); return; }
    snprintf(B.phase, sizeof B.phase, "%s", phase);
    if (!strcmp(phase, "collecting") || !strcmp(phase, "uploading") || !strcmp(phase, "saved")) B.restore = false;
    if (!strcmp(phase, "fetching") || !strcmp(phase, "preparing") || !strcmp(phase, "preview") ||
        !strcmp(phase, "checking") || !strcmp(phase, "restoring")) B.restore = true;
    ink_json_str(j, ink_json_find(j, 0, "preview"), B.preview, sizeof B.preview);
    ink_json_str(j, ink_json_find(j, 0, "reason"), reason, sizeof reason);
    int64_t created = 0, count = 0, online = 0;
    bool summary = ink_json_int(j, ink_json_find(j, 0, "item_count"), &count);
    ink_json_int(j, ink_json_find(j, 0, "created_s"), &created);
    if (!created) ink_json_int(j, ink_json_find(j, 0, "saved_s"), &created);
    if (summary || changed) {
        ink_tm_t tm = {0}; ink_time_to_local(created, &tm);
        if (created > 0) snprintf(B.info, sizeof B.info, "%d月%d日 %02d:%02d%s", tm.month, tm.day, tm.hour, tm.min, summary ? "的备份" : "已收到");
        else snprintf(B.info, sizeof B.info, "备份时间暂不能查看");
        if (summary) snprintf(B.detail, sizeof B.detail, "共 %d 项", (int)count);
    }
    ink_state_get_int(ink_state_find("net.online"), &online);
    bool failed = !strcmp(phase, "failed"), offline = !online && (!strcmp(phase, "collecting") || !strcmp(phase, "uploading") || !strcmp(phase, "fetching"));
    int page = B.restore ? RESTORE : BACKUP;
    if (failed || offline) {
        page = B.restore ? RESTORE_FAIL : BACKUP_FAIL;
        snprintf(B.title, sizeof B.title, "%s", B.restore ? "没有恢复成功" : "还没备份好");
        snprintf(B.detail, sizeof B.detail, "%s", offline ? "连不上家庭服务器。资料先留在这台设备上，联网后可以继续。" : reason);
    } else if (B.restore) {
        page = !strcmp(phase, "done") ? RESTORE_DONE : !strcmp(phase, "checking") || !strcmp(phase, "restoring") ? RESTORE_RUN : RESTORE;
        char title[96]; snprintf(title, sizeof title, "%.90s", page == RESTORE_DONE ? "已恢复" : page == RESTORE_RUN ? "正在检查并放回资料" : B.preview[0] ? B.detail : "正在查找这位家人的备份");
        memcpy(B.title, title, strlen(title) + 1);
    } else snprintf(B.title, sizeof B.title, "%s", !strcmp(phase, "saved") ? "已备份到家庭服务器" : !strcmp(phase, "none") ? "还没有备份" : !strcmp(phase, "done") ? "上次恢复已完成" : "正在备份");
    osal_free(j);
    if (changed || summary || page != B.page) show(page);
    if (changed && (!strcmp(phase, "saved") || !strcmp(phase, "preview") || !strcmp(phase, "done")))
        request("backup.manifest", "{\"summary\":true}");
}
static void create(lv_obj_t *screen, void *arg) {
    B.screen = screen; B.page = (int)(intptr_t)arg; B.epoch = ink_member_epoch(); B.generation++;
    if (B.page == BACKUP) {
        B.restore = false; B.phase[0] = B.preview[0] = B.info[0] = B.detail[0] = 0;
        snprintf(B.title, sizeof B.title, "正在读取备份记录");
    }
    snprintf(B.member, sizeof B.member, "%s", ink_member_current()->name);
    draw(); B.timer = lv_timer_create(poll, 1000, NULL); request("backup.status", NULL);
}
static void destroy(void) {
    if (B.timer) lv_timer_delete(B.timer);
    B.timer = NULL; B.screen = NULL; B.pending = false; B.generation++;
}
static void open_page(ink_call_t *c) {
    (void)c;
    ink_shell_close();
    ink_ui_drop_app("backup");
    ink_ui_push(&PAGES[BACKUP], NULL);
}
int ink_backup_pages_init(void) {
    static const ink_cap_t cap = {.id = "backup.open", .title = "打开备份", .effect = INK_FX_NAVIGATE, .run = open_page};
    return ink_cap_register(&cap, 1);
}
