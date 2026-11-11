/* Member-scoped backup presenters; the existing service owns every operation. */
/* ui-scale-runtime: dimension x,y,w Theme-derived layout coordinates. */
/* ui-scale-runtime: font font Only theme text sizes are passed by the presenter. */
#include "ink/sys/i18n.h"
#include <stdio.h>
#include <string.h>
#include "ink/osal.h"
#include "ink/sys/cap.h"
#include "ink/sys/json.h"
#include "ink/sys/members.h"
#include "ink/sys/state.h"
#include "ink/sys/time.h"
#include "ink/sys/settings.h"
#include "ink/sys/ui_semantics.h"
#include "ink/ui_shell.h"

enum { BACKUP, BACKUP_FAIL, RESTORE, RESTORE_RUN, RESTORE_DONE, RESTORE_FAIL };
enum { BACKUP_FIELDS = 5, BACKUP_ACTIONS = 4 };
_Static_assert(5 <= BACKUP_FIELDS && 4 <= BACKUP_ACTIONS, "backup page capacity");
static const char *const NAMES[] = {"Backup", "BackupFail", "Restore", "RestoreRun", "RestoreDone", "RestoreFail"};
static const char OMITTED[] = TR_NOOP("设置、家人身份和密码需重新设置；网络和家庭需重新加入。共享内容按需下载。已发短信、已执行的操作和过期提醒不重放。本机 AI 操作记录和清单外文件保留。");
static const char UNCERTAIN[] = TR_NOOP("部分资料可能已替换，具体范围暂不能核实。请重新预览后再试。");
static struct {
    lv_obj_t *screen;
    lv_timer_t *timer;
    uint32_t epoch, generation;
    int page;
    bool pending, restore, restart;
    char member[32], phase[24], preview[33], args[80], title[96], info[160], detail[256];
    char automatic[96], auto_args[64];
    ink_ui_page_spec_t model;
    ink_ui_field_spec_t fields[BACKUP_FIELDS];
    ink_ui_action_spec_t actions[BACKUP_ACTIONS];
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
    B.pending = true;
    if (ink_cap_call(cap, NULL, &opts) != 0) B.pending = false;
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
        .done = !strcmp(a->capability, "backup.now") || !strcmp(a->capability, "settings.set") ? NULL : reply, .ctx = (void *)(uintptr_t)B.generation, .member = ink_member_current()->id};
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
    int mode = (int)ink_setting_get("backup.auto");
    snprintf(B.automatic, sizeof B.automatic, TR("自动备份：%s"), mode == 1 ? TR("每天") : mode == 2 ? TR("每周") : TR("关"));
    snprintf(B.auto_args, sizeof B.auto_args, "{\"id\":\"backup.auto\",\"value\":%d}", (mode + 1) % 3);
    B.model = (ink_ui_page_spec_t){NAMES[B.page], B.restore ? TR("恢复备份") : TR("备份"), {NAMES[B.page], 0}, B.fields, 5, B.actions, 1};
    B.actions[0] = (ink_ui_action_spec_t){"back", TR("返回"), "shell.back", NULL, {NULL, 0}, true};
    B.fields[0] = (ink_ui_field_spec_t){"member", "text", TR("家人"), B.member, {NULL, 0}, false};
    B.fields[1] = (ink_ui_field_spec_t){"status", "text", TR("状态"), B.title, {NULL, 0}, false};
    B.fields[2] = (ink_ui_field_spec_t){"summary", "text", TR("备份摘要"), B.info, {NULL, 0}, false};
    B.fields[3] = (ink_ui_field_spec_t){"details", "text", TR("说明"), B.detail, {NULL, 0}, false};
    B.fields[4] = (ink_ui_field_spec_t){"omitted", "text", B.restore ? TR("不恢复的内容") : TR("自动备份"), B.restore ? TR(OMITTED) : B.automatic, {NULL, 0}, false};
    if (B.page == RESTORE_FAIL) B.fields[4].value = TR(UNCERTAIN);
    if (B.page == BACKUP) { B.fields[3] = B.fields[4]; B.model.field_count = 4; }
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
        if (B.page == RESTORE_FAIL) text(TR(UNCERTAIN), INK_FONT_NOTE, middle + INK_GAP_XL + INK_LINE_SUBTITLE + INK_GAP_L + ink_ui_text_height(B.detail, INK_FONT_NOTE, INK_WEIGHT_REGULAR, w), false);
    }
    if (B.page == BACKUP) {
        text(TR("只备份这位家人；自动备份须连 Wi-Fi、充电且无人使用。"), INK_FONT_NOTE, INK_STATUS_BAR + INK_PRIMARY + INK_MARGIN, true);
        if (ink_member_current()->role == INK_ROLE_ADULT)
            button(B.automatic, "settings.set", B.auto_args, INK_MARGIN, bottom - 2 * (INK_PRIMARY + INK_GAP), w, false);
        button(TR("恢复备份"), "restore.preview", "{\"summary\":true}", INK_MARGIN, bottom - INK_PRIMARY - INK_GAP, w, false);
        button(TR("现在备份一次"), "backup.now", NULL, INK_MARGIN, bottom, w, false);
    } else if (B.page == BACKUP_FAIL) button(TR("再试一次"), "backup.now", NULL, INK_MARGIN, bottom, w, true);
    else if (B.page == RESTORE) {
        text(B.info, INK_FONT_NOTE, INK_STATUS_BAR + INK_PRIMARY + INK_GAP, false);
        lv_obj_t *card = ink_ui_card(B.screen, INK_MARGIN, 2 * INK_PRIMARY + INK_GAP, w, INK_ROW_DOUBLE, INK_SURFACE_STATIC);
        ink_ui_label(card, B.member, INK_FONT_LIST, INK_WEIGHT_BOLD, INK_GAP_XL, INK_GAP, w - 2 * INK_GAP_XL);
        ink_ui_label(card, B.title, INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_GAP_XL, INK_GAP + INK_LINE_LIST, w - 2 * INK_GAP_XL);
        text(TR(OMITTED), INK_FONT_NOTE, 2 * INK_PRIMARY + INK_GAP + INK_ROW_DOUBLE + INK_GAP_L, false);
        const char *scope = TR("只替换所选家人备份清单内的资料；清单外文件保留。其他家人不变。");
        B.fields[3].value = scope;
        text(scope, INK_FONT_NOTE, bottom - ink_ui_text_height(scope, INK_FONT_NOTE, INK_WEIGHT_REGULAR, w) - INK_GAP_L, false);
        button(TR("取消"), "shell.back", NULL, INK_MARGIN, bottom, (w - INK_GAP) / 2, false);
        char preview[sizeof B.preview]; memcpy(preview, B.preview, sizeof preview);
        snprintf(B.args, sizeof B.args, "{\"preview\":\"%s\"}", preview);
        button(B.preview[0] ? TR("恢复备份") : TR("再查一次"), B.preview[0] ? "restore.apply" : "restore.preview",
            B.preview[0] ? B.args : "{\"summary\":true}", INK_MARGIN + (w + INK_GAP) / 2, bottom, (w - INK_GAP) / 2, true);
    } else if (B.page == RESTORE_RUN) {
        text(TR("正在放回来"), INK_FONT_TITLE, INK_STATUS_BAR + INK_PRIMARY, false);
        text(B.title, INK_FONT_LIST, 2 * INK_PRIMARY + INK_GAP_XL, false);
        text(TR("不要关机。中途失败时，有些资料可能已经放回。"), INK_FONT_NOTE, bottom, true);
    } else if (B.page == RESTORE_DONE) {
        text(TR("放回来了"), INK_FONT_TITLE, INK_STATUS_BAR + INK_GAP_XXL, false);
        lv_obj_t *card = ink_ui_card(B.screen, INK_MARGIN, 2 * INK_PRIMARY, w, INK_ROW_DOUBLE, INK_SURFACE_STATIC);
        ink_ui_label(card, B.member, INK_FONT_SUBTITLE, INK_WEIGHT_BOLD, INK_GAP_XL, INK_GAP, w - 2 * INK_GAP_XL);
        ink_ui_label(card, B.info, INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_GAP_XL, INK_GAP + INK_LINE_SUBTITLE, w - 2 * INK_GAP_XL);
        text(TR("以下内容没有放回"), INK_FONT_SUBTITLE, 2 * INK_PRIMARY + INK_ROW_DOUBLE + INK_GAP_XL, false);
        text(TR(OMITTED), INK_FONT_NOTE, 2 * INK_PRIMARY + INK_ROW_DOUBLE + INK_GAP_XL + INK_LINE_SUBTITLE + INK_GAP, false);
        text(B.restart ? TR("请重启设备，让放回的资料生效。") : TR("资料已放回。"), INK_FONT_NOTE, bottom - INK_LINE_NOTE - INK_GAP_L, false);
        button(B.restart ? TR("重启") : TR("开始用"), B.restart ? "system.reboot" : "system.home", NULL, INK_MARGIN, bottom, w, true);
    } else {
        button(TR("先不恢复"), "shell.back", NULL, INK_MARGIN, bottom, (w - INK_GAP) / 2, false);
        button(TR("再试一次"), "restore.preview", "{\"summary\":true}", INK_MARGIN + (w + INK_GAP) / 2, bottom, (w - INK_GAP) / 2, true);
    }
}
static void reply(void *ctx, const ink_receipt_t *rc) {
    if (!current() || (uint32_t)(uintptr_t)ctx != B.generation) return;
    B.pending = false;
    if (rc->status == INK_RC_PENDING_CONFIRMATION || rc->status == INK_RC_CANCELLED) { draw(); return; }
    if (rc->status != INK_RC_DONE && rc->status != INK_RC_QUEUED) {
        snprintf(B.title, sizeof B.title, "%s", B.restore ? TR("没有恢复成功") : TR("还没备份好"));
        snprintf(B.detail, sizeof B.detail, "%s", rc->say[0] ? rc->say : TR("暂时读不出来，请检查网络和存储卡后再试。"));
        show(B.restore ? RESTORE_FAIL : BACKUP_FAIL); return;
    }
    ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL);
    if (!j || !rc->result || ink_json_parse(j, rc->result, strlen(rc->result))) { osal_free(j); return; }
    char phase[24] = "", reason[160] = "";
    ink_json_str(j, ink_json_find(j, 0, "phase"), phase, sizeof phase);
    bool changed = strcmp(B.phase, phase) != 0;
    /* What the page shows before this reply: the poll comes every second, and a redraw is a refresh on the panel and
     * drops a tap in progress, so only a visible change redraws (the batch gate after small-gaps-3 merged). */
    char was_title[sizeof B.title], was_info[sizeof B.info], was_detail[sizeof B.detail], was_preview[sizeof B.preview];
    memcpy(was_title, B.title, sizeof was_title); memcpy(was_info, B.info, sizeof was_info);
    memcpy(was_detail, B.detail, sizeof was_detail); memcpy(was_preview, B.preview, sizeof was_preview);
    if (!changed && (B.page == BACKUP_FAIL || B.page == RESTORE_FAIL) &&
        (!strcmp(phase, "none") || !strcmp(phase, "saved") || !strcmp(phase, "preview") || !strcmp(phase, "done"))) { osal_free(j); return; }
    snprintf(B.phase, sizeof B.phase, "%s", phase);
    B.restart = ink_json_eq(j, ink_json_find(j, 0, "restart_required"), "true");
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
        if (created > 0) snprintf(B.info, sizeof B.info, summary ? TR("%d月%d日 %02d:%02d的备份") : TR("%d月%d日 %02d:%02d已收到"), tm.month, tm.day, tm.hour, tm.min);
        else snprintf(B.info, sizeof B.info, TR("备份时间暂不能查看"));
        if (summary) snprintf(B.detail, sizeof B.detail, TR("共 %d 项"), (int)count);
    }
    ink_state_get_int(ink_state_find("net.online"), &online);
    bool failed = !strcmp(phase, "failed"), offline = !online && (!strcmp(phase, "collecting") || !strcmp(phase, "uploading") || !strcmp(phase, "fetching"));
    int page = B.restore ? RESTORE : BACKUP;
    if (failed || offline) {
        page = B.restore ? RESTORE_FAIL : BACKUP_FAIL;
        snprintf(B.title, sizeof B.title, "%s", B.restore ? TR("没有恢复成功") : TR("还没备份好"));
        snprintf(B.detail, sizeof B.detail, "%s", offline ? TR("连不上家庭服务器。资料先留在这台设备上，联网后可以继续。") : reason);
    } else if (B.restore) {
        page = !strcmp(phase, "done") ? RESTORE_DONE : !strcmp(phase, "preview") ? RESTORE : RESTORE_RUN;
        const char *stage = !strcmp(phase, "fetching") ? TR("正在下载备份") : !strcmp(phase, "preparing") ? TR("正在整理恢复预览") : !strcmp(phase, "checking") ? TR("正在检查资料") : !strcmp(phase, "restoring") ? TR("正在放回资料") : TR("正在处理");
        char title[96]; snprintf(title, sizeof title, "%.90s", page == RESTORE_DONE ? TR("已恢复") : page == RESTORE_RUN ? stage : B.preview[0] ? B.detail : TR("正在查找这位家人的备份"));
        memcpy(B.title, title, strlen(title) + 1);
    } else snprintf(B.title, sizeof B.title, "%s", !strcmp(phase, "saved") ? TR("已备份到家庭服务器") : !strcmp(phase, "none") ? TR("还没有备份") : !strcmp(phase, "done") ? TR("上次恢复已完成") : TR("正在备份"));
    osal_free(j);
    bool shown_changed = strcmp(was_title, B.title) || strcmp(was_info, B.info) || strcmp(was_detail, B.detail) ||
        strcmp(was_preview, B.preview);
    if (changed || shown_changed || page != B.page) show(page);
    if (changed && (!strcmp(phase, "saved") || !strcmp(phase, "preview") || !strcmp(phase, "done")))
        request("backup.manifest", "{\"summary\":true}");
}
static void create(lv_obj_t *screen, void *arg) {
    B.screen = screen; B.page = (int)(intptr_t)arg; B.epoch = ink_member_epoch(); B.generation++;
    if (B.page == BACKUP) {
        B.restore = false; B.phase[0] = B.preview[0] = B.info[0] = B.detail[0] = 0;
        snprintf(B.title, sizeof B.title, TR("正在读取备份记录"));
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
static void auto_changed(void *ctx, int h, const char *id) {
    (void)ctx; (void)h; (void)id;
    if (current() && B.page == BACKUP) { ink_ui_content_changed(); draw(); }
}
int ink_backup_pages_init(void) {
    static const ink_cap_t cap = {.id = "backup.open", .title = "打开备份", .effect = INK_FX_NAVIGATE, .run = open_page};
    return ink_cap_register(&cap, 1) || ink_settings_watch("backup.auto", auto_changed, NULL, INK_EXEC_UI) ? -1 : 0;
}
