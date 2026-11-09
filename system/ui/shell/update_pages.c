/* Update surfaces read the service; controls use the capability bus. */
/* ui-scale-runtime: dimension x,y,w,h,P.progress Theme-derived layout and percentage progress width. */
/* ui-scale-runtime: font font Only subtitle and note theme tokens reach text(). */
#include <stdio.h>
#include <string.h>
#include "ink/osal.h"
#include "ink/sys/exec.h"
#include "ink/sys/members.h"
#include "ink/sys/state.h"
#include "ink/sys/ui_semantics.h"
#include "ink/ui_shell.h"
#include "shell_card.h"

void ink_update_page_info(char *version, size_t cap, uint32_t *bytes, int *task);
enum { UPDATE, READY, BLOCKED, FAIL, INSTALL, DONE, RECOVERY };
static const char *const NAMES[] = {"Update", "UpdateReady", "UpdateBlocked", "UpdateFail", "UpdateInstall", "UpdateDone", "BootRecovery"};
static struct {
    shell_card_t card;
    int page, result, task, progress;
    uint32_t epoch;
    char state[16], version[24], current[24], title[128], body[256], percent[24], cancel[48], error[128];
    ink_ui_field_spec_t fields[4];
    ink_ui_action_spec_t actions[3];
    ink_ui_page_spec_t model;
} P INK_PSRAM_BSS;
static void render(void);
static int snapshot(char *out, size_t cap) { return ink_ui_semantics_snapshot(&P.model, out, cap); }
static void close_page(void);
static bool key(ink_key_t k, bool lng) {
    if (P.page == INSTALL) return true;
    if (!lng && (k == INK_KEY_BACK || k == INK_KEY_HOME)) { close_page(); return true; }
    return false;
}
#define OVERLAY(name_, level_) {.name = name_, .level = level_, .snapshot = snapshot, .on_key = key}
static const ink_ui_overlay_t PAGES[] = {OVERLAY("Update", 0), OVERLAY("UpdateReady", 0), OVERLAY("UpdateBlocked", 0),
    OVERLAY("UpdateFail", 0), OVERLAY("UpdateInstall", 4), OVERLAY("UpdateDone", 0), OVERLAY("BootRecovery", 0)};
static void close_page(void) {
    if (!P.card.root) return;
    ink_ui_overlay_remove(&PAGES[P.page]); shell_card_close(&P.card);
}
static int64_t integer(const char *id) { int64_t n = 0; ink_state_get_int(ink_state_find(id), &n); return n; }
static bool is_state(const char *s) { return !strcmp(P.state, s); }
static void read_state(void) {
    ink_state_get_str(ink_state_find("update.state"), P.state, sizeof P.state);
    ink_state_get_str(ink_state_find("update.version"), P.current, sizeof P.current);
    P.progress = (int)integer("update.progress");
    ink_update_page_info(P.version, sizeof P.version, &(uint32_t){0}, &P.task);
}
static const char *blocked(void) {
    uint32_t bytes = 0; int task;
    ink_update_page_info(P.version, sizeof P.version, &bytes, &task);
    if (integer("battery.level") < 30 && !integer("battery.charging")) return "电量不足，要充到 30% 后再安装。";
    char mode[16] = ""; ink_state_get_str(ink_state_find("storage.mode"), mode, sizeof mode);
    if (!strcmp(mode, "usb")) return "存储卡正借给电脑，退出 U 盘模式后再试。";
    if (!integer("storage.card")) return "请放入存储卡后再试。";
    if (!is_state("ready") && bytes && integer("storage.free") < bytes) return "存储卡空间不够，腾出空间后再试。";
    return NULL;
}
static void text(const char *value, int font, int y, bool bold) {
    lv_obj_t *l = ink_ui_label(P.card.panel, value, font, bold ? INK_WEIGHT_BOLD : INK_WEIGHT_REGULAR,
        INK_MARGIN, y, INK_CANVAS_W - 2 * INK_MARGIN);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
}
static void reply(void *ctx, const ink_receipt_t *rc) {
    if ((uint32_t)(uintptr_t)ctx != ink_member_epoch() || !P.card.root) return;
    if (rc->status == INK_RC_FAILED || rc->status == INK_RC_REJECTED || rc->status == INK_RC_INVALID) {
        snprintf(P.error, sizeof P.error, "没能完成这次更新，可以再试一次。"); render();
    }
}
static void clicked(lv_event_t *e) {
    const ink_ui_action_spec_t *a = lv_event_get_user_data(e);
    if (P.epoch != ink_member_epoch()) return;
    P.error[0] = 0;
    ink_call_opts_t opts = {.src = INK_SRC_TOUCH, .args_json = a->args_json, .reply_exec = INK_EXEC_UI,
        .done = reply, .ctx = (void *)(uintptr_t)P.epoch};
    ink_cap_call(a->capability, NULL, &opts);
}
static void button(const char *label, const char *cap, const char *args, int x, int w, bool primary) {
    ink_ui_action_spec_t *a = &P.actions[P.model.action_count++];
    *a = (ink_ui_action_spec_t){cap, label, cap, args, {NULL, 0}, true};
    ink_ui_pill(P.card.panel, label, x, INK_CANVAS_H - INK_MARGIN - INK_PRIMARY, w, INK_PRIMARY, primary, clicked, a);
}
static void render(void) {
    read_state();
    bool installing = is_state("installing") || is_state("rebooting") || is_state("verifying");
    const char *why = blocked();
    int page = installing ? INSTALL : P.result ? (P.result > 0 ? DONE : RECOVERY) : P.error[0] || is_state("failed") ? FAIL :
        (is_state("ready") || is_state("available")) && why ? BLOCKED : is_state("ready") ? READY : UPDATE;
    close_page(); P.page = page; P.epoch = ink_member_epoch();
    shell_card_open(&P.card); shell_card_full(&P.card); lv_obj_clean(P.card.panel);
    ink_ui_overlay_add(&PAGES[page], P.card.root); ink_ui_content_changed();
    lv_obj_t *bar = ink_shell_statusbar(P.card.panel, true);
    if (page == INSTALL) lv_obj_remove_flag(bar, LV_OBJ_FLAG_CLICKABLE);
    if (page < INSTALL) {
        lv_obj_t *back = ink_ui_circle(P.card.panel, "back", INK_MARGIN, INK_STATUS_BAR + INK_GAP_S, INK_CIRCLE, INK_ICON_LIST, INK_SURFACE_STATIC, NULL, NULL);
        ink_shell_bind(back, "shell.update_close", NULL);
        ink_ui_label(P.card.panel, "系统更新", INK_FONT_TITLE, INK_WEIGHT_BOLD, INK_MARGIN + INK_CIRCLE + INK_GAP,
            INK_STATUS_BAR + INK_GAP_S, INK_CANVAS_W - 2 * INK_MARGIN - INK_CIRCLE - INK_GAP);
    }
    P.model = (ink_ui_page_spec_t){NAMES[page], "系统更新", {"system:update", ink_ui_content_version()}, P.fields, 4, P.actions, 0};
    snprintf(P.percent, sizeof P.percent, "%d%%", P.progress);
    snprintf(P.title, sizeof P.title, "%s", page == BLOCKED ? "现在装不了" : page == FAIL ? "这次更新没完成" : page == INSTALL ? "正在安装，不要关机" : "系统更新");
    snprintf(P.body, sizeof P.body, "%s", page == BLOCKED ? why : page == FAIL ? "可以重新下载。原来的系统仍然保留。" :
        page == READY ? "安装要几分钟，这期间不能用。" : page == INSTALL ? "如果新系统不能正常启动，会自动退回原来的系统。" :
        page == DONE ? "更新已完成。更新说明暂不能查看。" : page == RECOVERY ? "你的东西都在。之后可以再试一次。" :
        is_state("downloading") ? "下载完再问你要不要安装。" : is_state("available") ? "发现新版本，可以下载。" : "可以向家庭服务器检查新版本。");
    if (page == READY) snprintf(P.title, sizeof P.title, "%s 下载好了", P.version);
    if (page == DONE) snprintf(P.title, sizeof P.title, "已经是 %s 了", P.current);
    if (page == RECOVERY) snprintf(P.title, sizeof P.title, "新版本没装好，退回了 %s", P.current);
    if (page == UPDATE) snprintf(P.title, sizeof P.title, is_state("downloading") ? "正在下载 %s，可以继续读" : "系统版本 %s", is_state("downloading") ? P.version : P.current);
    int w = INK_CANVAS_W - 2 * INK_MARGIN, half = (w - INK_GAP) / 2;
    if (page == UPDATE) {
        int y = INK_STATUS_BAR + INK_PRIMARY + INK_MARGIN;
        ink_ui_label(P.card.panel, P.title, INK_FONT_SUBTITLE, INK_WEIGHT_BOLD, INK_MARGIN, y, w);
        y += 2 * INK_LINE_SUBTITLE + INK_GAP;
        if (is_state("downloading")) {
            ink_ui_box(P.card.panel, INK_MARGIN, y, w - INK_CIRCLE, INK_GAP_M, -1, INK_SURFACE_WHITE, true);
            ink_ui_box(P.card.panel, INK_MARGIN, y, (w - INK_CIRCLE) * P.progress / 100, INK_GAP_M, -1, INK_SURFACE_BLACK, false);
            ink_ui_label(P.card.panel, P.percent, INK_FONT_NOTE, INK_WEIGHT_MEDIUM, INK_CANVAS_W - INK_MARGIN - INK_CIRCLE, y - INK_GAP_M, INK_CIRCLE);
        }
        ink_ui_label(P.card.panel, P.body, INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_MARGIN, y + INK_GAP_XL, w);
        ink_ui_card(P.card.panel, INK_MARGIN, y + INK_BLOCK_L, w, INK_ROW_DOUBLE, INK_SURFACE_STATIC);
        ink_ui_label(P.card.panel, "更新说明暂不能查看", INK_FONT_LIST, INK_WEIGHT_REGULAR, 2 * INK_MARGIN, y + INK_BLOCK_L + INK_MARGIN, w - 2 * INK_MARGIN);
    } else if (page == INSTALL) {
        text(P.title, INK_FONT_SUBTITLE, INK_STATUS_BAR + INK_BLOCK_L, true);
        const char *steps[] = {"检查更新包", "安装", "重启", "检查是否正常"};
        int stage = is_state("verifying") ? 0 : is_state("installing") ? 1 : 2;
        for (int i = 0; i < 4; i++) {
            int y = INK_STATUS_BAR + 2 * INK_BLOCK_L + i * INK_BLOCK_L;
            lv_obj_t *step = ink_ui_box(P.card.panel, INK_MARGIN, y, INK_CIRCLE_SMALL, INK_CIRCLE_SMALL, -1,
                i < stage ? INK_SURFACE_BLACK : INK_SURFACE_WHITE, true);
            if (i < stage) ink_ui_icon(step, "check", INK_ICON_SMALL, INK_ICON_LINE, (INK_CIRCLE_SMALL - INK_ICON_SMALL) / 2, (INK_CIRCLE_SMALL - INK_ICON_SMALL) / 2, true);
            ink_ui_label(P.card.panel, steps[i], INK_FONT_LIST, i == stage ? INK_WEIGHT_BOLD : INK_WEIGHT_REGULAR,
                INK_MARGIN + INK_CIRCLE_SMALL + INK_GAP, y + INK_GAP_M, w - INK_CIRCLE_SMALL - INK_GAP);
        }
        text(P.percent, INK_FONT_NOTE, INK_CANVAS_H - INK_PRIMARY - INK_BLOCK_L, false);
        text(P.body, INK_FONT_NOTE, INK_CANVAS_H - INK_PRIMARY, false);
    } else {
        int y = INK_CANVAS_H / 2 - INK_CIRCLE_LARGE;
        if (page == READY) y -= INK_BLOCK_L;
        ink_ui_circle(P.card.panel, page == READY ? "download" : page == DONE ? "check" : page == RECOVERY ? "undo" : page == BLOCKED ? "battery" : "warn",
            (INK_CANVAS_W - INK_CIRCLE_LARGE) / 2, y, INK_CIRCLE_LARGE, INK_ICON_STATE,
            page == READY || page == DONE ? INK_SURFACE_BLACK : INK_SURFACE_STATIC, NULL, NULL);
        y += INK_CIRCLE_LARGE + INK_MARGIN;
        text(P.title, INK_FONT_SUBTITLE, y, true);
        text(P.body, INK_FONT_NOTE, y + ink_ui_text_height(P.title, INK_FONT_SUBTITLE, INK_WEIGHT_BOLD, w) + INK_GAP_M, false);
    }
    P.fields[0] = (ink_ui_field_spec_t){.id = "status", .role = "text", .label = "状态", .value = P.title};
    P.fields[1] = (ink_ui_field_spec_t){.id = "detail", .role = "text", .label = "说明", .value = P.body};
    P.fields[2] = (ink_ui_field_spec_t){.id = "version", .role = "text", .label = "当前版本", .value = P.current};
    P.fields[3] = (ink_ui_field_spec_t){.id = "progress", .role = "text", .label = "进度", .value = P.percent};
    if (page == INSTALL) return;
    bool pair = page == READY || page == FAIL || page == UPDATE;
    bool downloading = page == UPDATE && is_state("downloading");
    button(page == READY ? "以后再装" : page == FAIL ? "先不管" : pair ? "返回" : "知道了", "shell.update_close", "{}",
        downloading ? INK_MARGIN + half + INK_GAP : INK_MARGIN, pair ? half : w, !pair || downloading);
    if (page == READY) button("现在安装", "system.upgrade", "{}", INK_MARGIN + half + INK_GAP, half, true);
    else if (page == FAIL || (page == UPDATE && !downloading)) button(page == FAIL ? "重新下载" : is_state("available") ? "下载" : "检查更新",
        page == FAIL || is_state("available") ? "update.download" : "update.check", "{}", INK_MARGIN + half + INK_GAP, half, true);
    else if (downloading) {
        snprintf(P.cancel, sizeof P.cancel, "{\"id\":%d}", P.task);
        button("暂停", "job.cancel", P.cancel, INK_MARGIN, half, false);
    }
}
static void navigate(ink_call_t *c) {
    if (P.card.root && P.page == INSTALL) { c->rc.status = INK_RC_REJECTED; return; }
    if (!strcmp(c->cap->id, "shell.update_close")) close_page();
    else { P.error[0] = 0; P.result = 0; ink_shell_close(); render(); }
}
static void changed(void *ctx, int h, const char *id) {
    (void)ctx; (void)h;
    if (!strncmp(id, "member.", 7)) { close_page(); P.error[0] = 0; P.result = 0; return; }
    char state[16] = ""; ink_state_get_str(ink_state_find("update.state"), state, sizeof state);
    bool install = !strcmp(state, "verifying") || !strcmp(state, "installing") || !strcmp(state, "rebooting");
    if (!P.card.root && !install) return;
    if (!strcmp(id, "update.progress") && (int)integer(id) / 10 == P.progress / 10) return;
    if (!strcmp(id, "update.state") && strcmp(state, "failed")) P.error[0] = 0;
    render();
}
static void event(void *ctx, const char *id, const ink_args_t *a) {
    (void)ctx;
    if (!strcmp(id, "update.result")) { bool ok = false; ink_args_get_bool(a, "ok", &ok); P.result = ok ? 1 : -1; render(); }
    else if (!strcmp(id, "update.available") && !strcmp(ink_ui_page_name(), "SetAbout")) { P.result = 0; render(); }
    else if (!strcmp(id, "confirm.result")) {
        char cap[48] = "", status[24] = "";
        ink_args_get_str(a, "cap", cap, sizeof cap); ink_args_get_str(a, "status", status, sizeof status);
        if (!strncmp(cap, "system.upgrade", 14) && !strcmp(status, "failed") && P.card.root) {
            snprintf(P.error, sizeof P.error, "安装没有完成，可以再试一次。"); render();
        }
    }
}
int ink_update_pages_init(void) {
    static const ink_cap_t caps[] = {
        {.id = "shell.update", .title = "打开系统更新", .effect = INK_FX_NAVIGATE, .run = navigate},
        {.id = "shell.update_close", .title = "返回更新前的页面", .effect = INK_FX_NAVIGATE, .run = navigate}};
    return ink_cap_register(caps, 2) || ink_state_watch("update.", changed, NULL, INK_EXEC_UI) ||
        ink_state_watch("battery.", changed, NULL, INK_EXEC_UI) || ink_state_watch("storage.", changed, NULL, INK_EXEC_UI) ||
        ink_state_watch("member.", changed, NULL, INK_EXEC_UI) || ink_cap_subscribe("update.", event, NULL, INK_EXEC_UI) ||
        ink_cap_subscribe("confirm.result", event, NULL, INK_EXEC_UI) ? -1 : 0;
}
