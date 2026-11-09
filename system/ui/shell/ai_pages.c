/* AI failures and the current member's capability receipts. */
#include <stdio.h>
#include <string.h>
#include "ink/osal.h"
#include "ink/sys/ai.h"
#include "ink/sys/appreg.h"
#include "ink/sys/data.h"
#include "ink/sys/json.h"
#include "ink/sys/members.h"
#include "ink/sys/time.h"
#include "ink/sys/ui_list.h"
#include "ink/ui_shell.h"

static const char *const BOARDS[] = {"AiOffline", "AiTimeout", "AiBudget"};
static const char *const TITLES[] = {"暂时问不了 AI，书还能继续读", "等得太久，先停了", "今天的额度用完了"};
static struct { lv_obj_t *root; int kind; ink_ui_page_spec_t page; } E;
static ink_ui_field_spec_t error_fields[2];
static ink_ui_action_spec_t error_actions[] = {
    {.id = "close", .label = "继续读", .capability = "ai.pages.close", .enabled = true},
    {.id = "retry", .label = "再说一次", .capability = "ai.pages.retry", .enabled = true}
};
static int error_snapshot(char *out, size_t cap) { return ink_ui_semantics_snapshot(&E.page, out, cap); }
static void error_deleted(lv_event_t *e) { (void)e; E.root = NULL; }
void ink_ai_pages_status(const char *status, lv_obj_t *parent, ink_ui_overlay_t *overlay) {
    if (E.root) lv_obj_delete(E.root);
    bool lost = !strcmp(status, "网络断了，回答没收全，请再试一次");
    int kind = !strcmp(status, "现在没有网络，稍后再问我") || lost ? 0 :
        !strcmp(status, "家庭服务器没有回应，请再试一次") ? 1 : !strcmp(status, "今天的额度用完了") ? 2 : -1;
    if (kind < 0) return;
    E.kind = kind;
    const char *detail = kind == 0 ? (lost ? "网络断了，回答没收全，不会自动重试。" : "连不上家庭服务器。") : kind == 1 ? "没有收到结果，不会自动重试。" :
        ink_member_current()->role == INK_ROLE_CHILD ? "可以问爸爸妈妈。书还能继续读。" : "明天恢复，也可以在手机上加。书还能继续读。";
    int w = INK_CANVAS_W - 2 * INK_MARGIN, y = INK_GAP_L + INK_CONTROL + INK_GAP_XXL;
    int title_h = ink_ui_text_height(TITLES[kind], INK_FONT_SUBTITLE, INK_WEIGHT_BOLD, w);
    int detail_h = ink_ui_text_height(detail, INK_FONT_NOTE, INK_WEIGHT_REGULAR, w);
    int h = y + title_h + INK_GAP_S + detail_h + INK_GAP_XXL + INK_PRIMARY + INK_MARGIN;
    lv_obj_set_y(parent, ink_ui_px(INK_CANVAS_H - h));
    lv_obj_set_height(parent, ink_ui_px(h + INK_RADIUS_PANEL));
    E.root = ink_ui_box(parent, 0, 0, INK_CANVAS_W, h + INK_RADIUS_PANEL, INK_RADIUS_PANEL, INK_SURFACE_WHITE, false);
    lv_obj_add_flag(E.root, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(E.root, error_deleted, LV_EVENT_DELETE, NULL);
    ink_ui_box(E.root, (INK_CANVAS_W - INK_ICON_STATE) / 2, INK_GAP_M, INK_ICON_STATE, INK_GAP_S, -1, INK_SURFACE_BLACK, false);
    ink_ui_circle(E.root, "ai", INK_MARGIN, INK_GAP_L, INK_CIRCLE_SMALL, INK_ICON_MEDIUM, INK_SURFACE_BLACK, NULL, NULL);
    lv_obj_t *b = ink_ui_circle(E.root, "close", INK_CANVAS_W - INK_MARGIN - INK_CONTROL, INK_GAP_L, INK_CONTROL, INK_ICON_MEDIUM, INK_SURFACE_WHITE, NULL, NULL);
    ink_shell_bind(b, "ai.pages.close", NULL);
    ink_ui_label(E.root, TITLES[kind], INK_FONT_SUBTITLE, INK_WEIGHT_BOLD, INK_MARGIN, y, w);
    ink_ui_label(E.root, detail, INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_MARGIN, y + title_h + INK_GAP_S, w);
    int primary_x = INK_MARGIN, primary_w = w;
    if (kind < 2) {
        primary_w = (w - INK_GAP) / 2; primary_x += primary_w + INK_GAP;
        error_actions[1].label = kind ? "再说一次" : "再试一次";
        b = ink_ui_pill(E.root, error_actions[1].label, INK_MARGIN, h - INK_MARGIN - INK_PRIMARY, primary_w, INK_PRIMARY, false, NULL, NULL);
        ink_shell_bind(b, "ai.pages.retry", NULL);
    }
    b = ink_ui_pill(E.root, "继续读", primary_x, h - INK_MARGIN - INK_PRIMARY, primary_w, INK_PRIMARY, true, NULL, NULL);
    ink_shell_bind(b, "ai.pages.close", NULL);
    error_fields[0] = (ink_ui_field_spec_t){.id = "status", .role = "status", .label = "状态", .value = TITLES[kind]};
    error_fields[1] = (ink_ui_field_spec_t){.id = "detail", .role = "text", .label = "说明", .value = detail};
    E.page = (ink_ui_page_spec_t){.id = BOARDS[kind], .title = TITLES[kind], .object = {"ai:error", 1},
        .fields = error_fields, .field_count = 2, .actions = error_actions, .action_count = kind == 2 ? 1 : 2};
    overlay->name = BOARDS[kind]; overlay->level = 0; overlay->snapshot = error_snapshot;
    ink_ui_content_changed(); ink_ui_hint_obj(parent, INK_REFRESH_FLASH_UI);
}

static struct {
    ink_db_t *db; ink_ui_list_t *list; ink_ui_list_position_t position;
    bool receipts; unsigned capacity; uint32_t version, generation;
    lv_obj_t *notice; const char *status;
    ink_json_t json;
    char row[512], ids[8][64], args[8][256], titles[8][128], values[8][192];
} L INK_PSRAM_BSS;
static const char *receipt_text(const char *status) {
    static const char *const names[] = {"done", "partial", "queued", "pending_confirmation", "rejected", "stale", "invalid", "failed", "cancelled", "undone"};
    static const char *const words[] = {"做好了", "做好了一部分", "还没做，等联网", "等你确认", "不能做", "页面或家人已变，操作已停止", "操作无效", "结果不确定，不会自动重试", "已取消", "已撤销"};
    for (unsigned i = 0; i < sizeof names / sizeof *names; i++) if (!strcmp(status, names[i])) return words[i];
    return "结果不确定，不会自动重试";
}
static void bind_row(lv_obj_t *slot, size_t index, ink_ui_field_spec_t *field, ink_ui_action_spec_t *action, void *ctx) {
    (void)ctx;
    unsigned i = index % L.capacity; ink_db_cursor_t cursor = {.desc = true}; ink_rec_t rec = {0};
    for (size_t at = 0; at <= index; at++) if (ink_db_next(L.db, &cursor, &rec, L.row, sizeof L.row - 1) != 1) return;
    L.row[rec.len < sizeof L.row ? rec.len : sizeof L.row - 1] = 0;
    if (ink_json_parse(&L.json, L.row, strlen(L.row))) return;
    char cap[48] = "", status[32] = "", object[48] = "";
    ink_json_str(&L.json, ink_json_find(&L.json, 0, "cap"), cap, sizeof cap);
    ink_json_str(&L.json, ink_json_find(&L.json, 0, "status"), status, sizeof status);
    ink_json_str(&L.json, ink_json_find(&L.json, 0, "object"), object, sizeof object);
    int64_t revision = 0, undo_ms = 0, when_s = 0, age;
    ink_json_int(&L.json, ink_json_find(&L.json, 0, "revision"), &revision);
    ink_json_int(&L.json, ink_json_find(&L.json, 0, "undo_ms"), &undo_ms);
    ink_json_int(&L.json, ink_json_find(&L.json, 0, "when"), &when_s);
    age = osal_wall_us() / 1000000 - when_s;
    bool undo = L.receipts && object[0] && undo_ms > 0 && age >= 0 && age * 1000 < undo_ms;
    const ink_cap_t *definition = ink_cap_find(cap);
    snprintf(L.ids[i], sizeof L.ids[i], "activity:%u", rec.id);
    snprintf(L.args[i], sizeof L.args[i], "{\"id\":%u}", rec.id);
    if (undo) {
        ink_jw_t w; ink_jw_init(&w, L.args[i], sizeof L.args[i]);
        ink_jw_raw(&w, "{\"object\":"); ink_jw_str(&w, object);
        ink_jw_raw(&w, ",\"revision\":"); ink_jw_int(&w, revision); ink_jw_raw(&w, "}");
    }
    snprintf(L.titles[i], sizeof L.titles[i], "%s", L.receipts ? receipt_text(status) : definition ? definition->title : cap);
    char when[32] = "时间未记录";
    if (rec.sort > 0) { ink_tm_t tm; ink_time_to_local(rec.sort, &tm); snprintf(when, sizeof when, "%02d-%02d %02d:%02d", tm.month, tm.day, tm.hour, tm.min); }
    snprintf(L.values[i], sizeof L.values[i], "%s · %s%s%s", when,
        L.receipts ? (definition ? definition->title : cap) : receipt_text(status), L.receipts && object[0] ? "\n" : "", L.receipts ? object : "");
    *field = (ink_ui_field_spec_t){.id = L.ids[i], .role = "listitem", .label = L.titles[i], .value = L.values[i], .object = {L.ids[i], rec.ver}};
    *action = (ink_ui_action_spec_t){.id = L.ids[i], .label = L.receipts ? "撤销" : "查看回执", .capability = L.receipts ? "system.undo" : "shell.ai_receipts", .args_json = L.args[i], .enabled = !L.receipts || undo, .object = {L.ids[i], rec.ver}};
    int w = INK_CANVAS_W - 4 * INK_MARGIN;
    if (!lv_obj_get_child_count(slot)) {
        (L.receipts ? ink_ui_label : ink_ui_line)(slot, "", INK_FONT_LIST, INK_WEIGHT_MEDIUM, INK_MARGIN, INK_GAP_M, w);
        (L.receipts ? ink_ui_label : ink_ui_line)(slot, "", INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_MARGIN, INK_GAP_M + ink_ui_line_height(INK_FONT_LIST), w);
    }
    lv_label_set_text(lv_obj_get_child(slot, 0), L.titles[i]); lv_label_set_text(lv_obj_get_child(slot, 1), L.values[i]);
    if (L.receipts) {
        lv_obj_set_y(lv_obj_get_child(slot, 1), ink_ui_px(INK_GAP_M + ink_ui_text_height(L.titles[i], INK_FONT_LIST, INK_WEIGHT_MEDIUM, w)));
        if (lv_obj_get_child_count(slot) > 2) lv_obj_delete(lv_obj_get_child(slot, 2));
        if (undo) {
            lv_obj_t *b = ink_ui_pill(slot, "撤销", INK_MARGIN, 2 * ink_ui_line_height(INK_FONT_LIST) + 4 * ink_ui_line_height(INK_FONT_NOTE) + 2 * INK_GAP_M, INK_PRIMARY, INK_CONTROL, false, NULL, NULL);
            ink_shell_bind(b, "system.undo", L.args[i]);
        }
    }
}
static ink_ui_action_spec_t list_actions[] = {
    {.id = "prev", .label = "上一页", .capability = "ai.pages.prev", .enabled = true},
    {.id = "next", .label = "下一页", .capability = "ai.pages.next", .enabled = true},
    {.id = "back", .label = "返回", .capability = "shell.back", .enabled = true}
};
static void list_loaded(void *ctx, const ink_receipt_t *rc) {
    if (!L.list || (uintptr_t)ctx != L.generation || !ink_ui_page_current()) return;
    if (rc->status == INK_RC_DONE) L.db = ink_db_open_cached_member(ink_db_find_def("activity.log"), ink_member_current()->id);
    if (rc->status == INK_RC_DONE && L.db) { ink_ui_redraw(); return; }
    L.status = "记录暂时读不了，请返回后重试";
    lv_label_set_text(L.notice, L.status); ink_ui_content_changed(); ink_ui_hint_obj(L.notice, INK_REFRESH_UI);
}
static void list_create(lv_obj_t *screen, void *arg) {
    L.receipts = !strcmp(ink_ui_page_name(), "AiReceipts");
    memset(&L.position, 0, sizeof L.position);
    L.db = ink_db_open_cached_member(ink_db_find_def("activity.log"), ink_member_current()->id);
    L.version = L.db ? ink_db_last_ver(L.db) : 0;
    L.generation = ink_ui_generation(); L.status = L.db ? "" : "正在读取记录"; L.notice = NULL;
    ink_shell_statusbar(screen, true);
    const char *title = L.receipts ? "AI 做事的结果" : "AI 替你做过的事";
    ink_shell_topbar(screen, title);
    int y = INK_STATUS_BAR + INK_PRIMARY + INK_GAP;
    int row_h = L.receipts ? 2 * ink_ui_line_height(INK_FONT_LIST) + 4 * ink_ui_line_height(INK_FONT_NOTE) + INK_CONTROL + 3 * INK_GAP_M : ink_ui_row_double();
    L.capacity = (INK_CANVAS_H - y - INK_CONTROL - 2 * INK_GAP - INK_MARGIN) / (row_h + INK_GAP_M);
    if (L.capacity > 8) L.capacity = 8;
    lv_obj_t *body = ink_ui_box(screen, 0, y, INK_CANVAS_W, INK_CANVAS_H - y, 0, INK_SURFACE_WHITE, false);
    ink_ui_page_spec_t page = {.id = ink_ui_page_name(), .title = title, .object = {"activity:log", L.version},
        .actions = list_actions, .action_count = 3};
    uint32_t selected = (uint32_t)(uintptr_t)arg;
    if (selected && L.db) {
        ink_db_cursor_t cursor = {.desc = true}; ink_rec_t rec; size_t index = 0;
        while (ink_db_next(L.db, &cursor, &rec, NULL, 0) == 1 && rec.id != selected) index++;
        L.position.page = index / L.capacity;
    }
    L.list = ink_ui_list_create(body, &page, &L.position, L.capacity, 1, row_h,
        L.db ? "还没有 AI 做过的事" : "", bind_row, NULL);
    if (L.list) ink_ui_list_refresh(L.list, L.db ? (size_t)ink_db_count(L.db) : 0);
    if (!L.db && L.list) {
        L.notice = ink_ui_notice(body, L.status, INK_GAP_L);
        ink_call_opts_t opts = {.src = INK_SRC_APP, .member = ink_member_current()->id, .reply_exec = INK_EXEC_UI,
            .done = list_loaded, .ctx = (void *)(uintptr_t)L.generation};
        if (ink_cap_call("activity.list", NULL, &opts)) list_loaded(opts.ctx, &(ink_receipt_t){.status = INK_RC_FAILED});
    }
}
static void list_destroy(void) { if (L.db) ink_db_close(L.db); L.db = NULL; L.list = NULL; }
static int list_snapshot(char *out, size_t cap) {
    if (!L.list) return -1;
    if (L.db) return ink_ui_list_snapshot(L.list, out, cap);
    ink_ui_field_spec_t field = {.id = "status", .role = "status", .label = "状态", .value = L.status};
    ink_ui_page_spec_t page = {.id = ink_ui_page_name(), .title = "AI 做过的事", .fields = &field, .field_count = 1, .actions = list_actions + 2, .action_count = 1};
    return ink_ui_semantics_snapshot(&page, out, cap);
}
static bool list_key(ink_key_t k, bool lng) { return L.list && ink_ui_list_key(L.list, k, lng); }
static void list_settle(void) {
    if (L.list && !L.db) {
        L.db = ink_db_open_cached_member(ink_db_find_def("activity.log"), ink_member_current()->id);
        if (L.db) ink_ui_redraw();
    }
    if (L.db && L.list && L.version != ink_db_last_ver(L.db)) {
        L.version = ink_db_last_ver(L.db); ink_ui_list_refresh(L.list, (size_t)ink_db_count(L.db));
    }
}
static const ink_activity_t LOG = {.name = "AiLog", .app = "ai-log", .create = list_create, .destroy = list_destroy,
    .on_key = list_key, .snapshot = list_snapshot, .settle = list_settle};
static const ink_activity_t RECEIPTS = {.name = "AiReceipts", .app = "ai-log", .create = list_create, .destroy = list_destroy,
    .on_key = list_key, .snapshot = list_snapshot, .settle = list_settle};
static const ink_app_def_t APP = {.id = "ai-log", .title = "AI 做过的事", .icon = "ai", .entry = &LOG, .system_app = true, .child_ok = true};
static void action(ink_call_t *c) {
    const char *id = c->cap->id;
    if (!strcmp(id, "shell.ai_log")) { ink_ai_dismiss(); if (ink_app_open("ai-log", NULL, NULL)) c->rc.status = INK_RC_FAILED; }
    else if (!strcmp(id, "shell.ai_receipts")) {
        int64_t selected = 0; ink_args_get_int(&c->args, "id", &selected);
        ink_ai_dismiss(); ink_ui_push(&RECEIPTS, (void *)(uintptr_t)selected);
    } else if (!strcmp(id, "ai.pages.close") || !strcmp(id, "ai.pages.retry")) {
        if (!E.root) { c->rc.status = INK_RC_STALE; return; }
        bool retry = !strcmp(id, "ai.pages.retry") && E.kind < 2;
        ink_ai_dismiss();
        if (retry) ink_cap_invoke("ai.panel.listen", NULL, c->src, 0, &c->rc);
    } else if (!L.list || (strcmp(ink_ui_page_name(), "AiLog") && strcmp(ink_ui_page_name(), "AiReceipts"))) c->rc.status = INK_RC_STALE;
    else if (!ink_ui_list_turn(L.list, !strcmp(id, "ai.pages.next") ? 1 : -1)) c->rc.status = INK_RC_INVALID;
}
#define PAGE_CAP(id_, title_) {.id = id_, .title = title_, .effect = INK_FX_NAVIGATE, .child_ok = true, .run = action}
static const ink_cap_t CAPS[] = {
    PAGE_CAP("shell.ai_log", "查看 AI 做过的事"),
    {.id = "shell.ai_receipts", .title = "查看 AI 做事的结果", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = action,
     .input = "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"integer\",\"minimum\":1,\"maximum\":4294967295}}}"},
    PAGE_CAP("ai.pages.close", "继续读"), PAGE_CAP("ai.pages.retry", "重新用话筒问"),
    PAGE_CAP("ai.pages.next", "下一页 AI 记录"), PAGE_CAP("ai.pages.prev", "上一页 AI 记录")
};
int ink_ai_pages_init(void) { return ink_cap_register(CAPS, sizeof CAPS / sizeof *CAPS) || ink_app_register(&APP); }
