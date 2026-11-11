/* AI failures and the current member's capability receipts. */
#include "ink/sys/i18n.h"
#include <stdio.h>
#include <string.h>
#include "ink/osal.h"
#include "ink/sys/ai.h"
#include "ink/sys/appreg.h"
#include "ink/sys/data.h"
#include "ink/sys/json.h"
#include "ink/sys/members.h"
#include "ink/sys/settings.h"
#include "ink/sys/time.h"
#include "ink/sys/ui_list.h"
#include "ink/ui_shell.h"
#include "ai_pages.h"

static const char *const TITLES[] = {TR_NOOP("暂时问不了 AI，书还能继续读"), TR_NOOP("等得太久，先停了"), TR_NOOP("额度用完了")};
static struct { lv_obj_t *root; int kind; ink_ui_page_spec_t page; } E;
static ink_ui_field_spec_t error_fields[2];
static ink_ui_action_spec_t error_actions[] = {
    {.id = "close", .label = TR_NOOP("继续读"), .capability = "ai.pages.close", .enabled = true},
    {.id = "retry", .label = TR_NOOP("再说一次"), .capability = "ai.pages.retry", .enabled = true}
};
static int error_snapshot(char *out, size_t cap) { return ink_ui_semantics_snapshot(&E.page, out, cap); }
static bool error_key(ink_key_t key, bool lng) {
    if (lng || (key != INK_KEY_HOME && key != INK_KEY_BACK)) return false;
    ink_ai_dismiss(); return true;
}
static const ink_ui_overlay_t ERROR_OVERLAYS[] = {
    {.name = "AiOffline", .on_key = error_key, .snapshot = error_snapshot},
    {.name = "AiTimeout", .on_key = error_key, .snapshot = error_snapshot},
    {.name = "AiBudget", .on_key = error_key, .snapshot = error_snapshot}
};
static void error_deleted(lv_event_t *e) { (void)e; E.root = NULL; }
const ink_ui_overlay_t *ink_ai_pages_status(ink_ai_error_t error, lv_obj_t *parent) {
    if (E.root) lv_obj_delete(E.root);
    bool lost = error == INK_AI_ERROR_LOST;
    int kind = error == INK_AI_ERROR_OFFLINE || lost ? 0 :
        error == INK_AI_ERROR_TIMEOUT ? 1 : error == INK_AI_ERROR_BUDGET ? 2 : -1;
    if (kind < 0) return NULL;
    E.kind = kind; error_actions[0].label = TR("继续读");
    bool plane = kind == 0 && ink_setting_get("net.airplane"); /* the way out is the switch, not trying again */
    bool kid = ink_member_current()->role == INK_ROLE_CHILD, one = kind == 2 || (plane && kid); /* the switch is not a child's */
    const char *title = plane ? TR("飞行模式已打开") : TR(TITLES[kind]);
    const char *detail = plane ? (kid ? TR("请家长关闭飞行模式。书还能继续读。") : TR("关闭飞行模式后再问。书还能继续读。")) : kind == 0 ? (lost ? TR("网络断了，回答没收全，不会自动重试。") : TR("连不上家庭服务器。")) : kind == 1 ? TR("没有收到结果，不会自动重试。") :
        kid ? TR("可以问爸爸妈妈。书还能继续读。") : TR("恢复时间还不确定。书还能继续读。");
    int w = INK_CANVAS_W - 2 * INK_MARGIN, y = INK_GAP_L + INK_CONTROL + INK_GAP_XXL;
    int title_h = ink_ui_text_height(title, INK_FONT_SUBTITLE, INK_WEIGHT_BOLD, w);
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
    ink_ui_label(E.root, title, INK_FONT_SUBTITLE, INK_WEIGHT_BOLD, INK_MARGIN, y, w);
    ink_ui_label(E.root, detail, INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_MARGIN, y + title_h + INK_GAP_S, w);
    int primary_x = INK_MARGIN, primary_w = w;
    if (!one) {
        primary_w = (w - INK_GAP) / 2; primary_x += primary_w + INK_GAP;
        error_actions[1].label = plane ? TR("关闭飞行模式") : kind ? TR("再说一次") : TR("再试一次");
        error_actions[1].capability = plane ? "net.set_airplane" : "ai.pages.retry";
        error_actions[1].args_json = plane ? "{\"enabled\":false}" : NULL;
        b = ink_ui_pill(E.root, error_actions[1].label, INK_MARGIN, h - INK_MARGIN - INK_PRIMARY, primary_w, INK_PRIMARY, false, NULL, NULL);
        ink_shell_bind(b, error_actions[1].capability, error_actions[1].args_json);
    }
    b = ink_ui_pill(E.root, TR("继续读"), primary_x, h - INK_MARGIN - INK_PRIMARY, primary_w, INK_PRIMARY, true, NULL, NULL);
    ink_shell_bind(b, "ai.pages.close", NULL);
    error_fields[0] = (ink_ui_field_spec_t){.id = "status", .role = "status", .label = TR("状态"), .value = title};
    error_fields[1] = (ink_ui_field_spec_t){.id = "detail", .role = "text", .label = TR("说明"), .value = detail};
    E.page = (ink_ui_page_spec_t){.id = ERROR_OVERLAYS[kind].name, .title = title, .object = {"ai:error", 1},
        .fields = error_fields, .field_count = 2, .actions = error_actions, .action_count = one ? 1 : 2};
    ink_ui_content_changed(); ink_ui_hint_obj(parent, INK_REFRESH_FLASH_UI);
    return &ERROR_OVERLAYS[kind];
}

enum { AI_ROWS_MAX = 8 };
static struct {
    ink_db_t *db; ink_ui_list_t *list; ink_ui_list_position_t position[2];
    char member[16], heading[128]; int64_t keep_s;
    bool receipts; unsigned capacity; uint32_t version, generation, epoch, selected;
    lv_obj_t *notice; const char *status;
    ink_json_t json;
    char row[512], ids[AI_ROWS_MAX][64], args[AI_ROWS_MAX][256], titles[AI_ROWS_MAX][128], values[AI_ROWS_MAX][512];
} L INK_PSRAM_BSS;
static const char *receipt_text(const char *status) {
    static const char *const names[] = {"done", "partial", "queued", "pending_confirmation", "rejected", "stale", "invalid", "failed", "cancelled", "undone"};
    static const char *const words[] = {TR_NOOP("做好了"), TR_NOOP("做好了一部分"), TR_NOOP("还没做，等联网"), TR_NOOP("等你确认"), TR_NOOP("不能做"), TR_NOOP("页面或家人已变，操作已停止"), TR_NOOP("操作无效"), TR_NOOP("结果不确定，不会自动重试"), TR_NOOP("已取消"), TR_NOOP("已撤销")};
    for (unsigned i = 0; i < sizeof names / sizeof *names; i++) if (!strcmp(status, names[i])) return TR(words[i]);
    return TR("结果不确定，不会自动重试");
}
static int next_record(ink_db_cursor_t *cursor, ink_rec_t *rec, char *row, size_t cap) {
    int got; int64_t now = osal_wall_us() / 1000000;
    do { got = ink_db_next(L.db, cursor, rec, row, cap); }
    while (got == 1 && rec->sort > 0 && now > 0 && rec->sort < now - L.keep_s);
    return got;
}
static size_t list_count(void) {
    if (!L.db) return 0;
    ink_db_cursor_t cursor = {0}; ink_rec_t rec; size_t n = (size_t)ink_db_count(L.db);
    int64_t now = osal_wall_us() / 1000000;
    /* The index is sorted: only the expired prefix needs visiting. */
    if (now > 0) {
        while (ink_db_next(L.db, &cursor, &rec, NULL, 0) == 1 && rec.sort < now - L.keep_s)
            if (rec.sort > 0) n--;
    }
    return n;
}
static void bind_row(lv_obj_t *slot, size_t index, ink_ui_field_spec_t *field, ink_ui_action_spec_t *action, void *ctx) {
    (void)ctx;
    unsigned i = index % L.capacity; ink_db_cursor_t cursor = {.desc = true}; ink_rec_t rec = {0};
    for (size_t at = 0; at <= index; at++) if (next_record(&cursor, &rec, L.row, sizeof L.row - 1) != 1) return;
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
    bool undo = !strcmp(L.member, ink_member_current()->id) && L.receipts && object[0] && undo_ms > 0 && age >= 0 && age * 1000 < undo_ms;
    const ink_cap_t *definition = ink_cap_find(cap);
    const char *name = TR("对象名未记录"), *operation = definition ? TR(definition->title) : TR("未记录的操作");
    for (int at = 0; !strncmp(object, "setting:", 8) && ink_setting_at(at); at++)
        if (!strcmp(ink_setting_at(at)->id, object + 8)) { name = TR(ink_setting_at(at)->title); break; }
    const char *reason = !L.receipts || undo ? "" : !undo_ms || !object[0] ? TR("\n此操作不支持撤销") :
        strcmp(L.member, ink_member_current()->id) ? TR("\n只能由操作的家人撤销") :
        when_s <= 0 || age < 0 ? TR("\n时间不明，无法判断撤销期限") : TR("\n已过撤销期限");
    snprintf(L.ids[i], sizeof L.ids[i], "activity:%u", (unsigned)rec.id);
    char member[16]; snprintf(member, sizeof member, "%s", L.member);
    snprintf(L.args[i], sizeof L.args[i], "{\"id\":%u,\"member\":\"%s\"}", (unsigned)rec.id, member);
    if (undo) {
        ink_jw_t w; ink_jw_init(&w, L.args[i], sizeof L.args[i]);
        ink_jw_raw(&w, "{\"object\":"); ink_jw_str(&w, object);
        ink_jw_raw(&w, ",\"revision\":"); ink_jw_int(&w, revision); ink_jw_raw(&w, "}");
    }
    snprintf(L.titles[i], sizeof L.titles[i], "%s", L.receipts ? receipt_text(status) : operation);
    char when[32]; snprintf(when, sizeof when, "%s", TR("时间未记录"));
    if (rec.sort > 0) { ink_tm_t tm; ink_time_to_local(rec.sort, &tm); snprintf(when, sizeof when, "%02d-%02d %02d:%02d", tm.month, tm.day, tm.hour, tm.min); }
    int bytes = snprintf(L.values[i], sizeof L.values[i], "%s · %s\n%s%s%s%s%s", when,
        L.receipts ? operation : receipt_text(status), name, object[0] ? "（" : "", object, object[0] ? "）" : "", reason);
    if (bytes < 0 || (size_t)bytes >= sizeof L.values[i]) snprintf(L.values[i], sizeof L.values[i], "%s\n%s", when, TR("记录太长，请查看原始回执"));
    *field = (ink_ui_field_spec_t){.id = L.ids[i], .role = "listitem", .label = L.titles[i], .value = L.values[i], .object = {L.ids[i], rec.ver}};
    *action = (ink_ui_action_spec_t){.id = L.ids[i], .label = L.receipts ? TR("撤销") : TR("查看回执"), .capability = L.receipts ? "system.undo" : "shell.ai_receipts", .args_json = L.args[i], .enabled = !L.receipts || undo, .object = {L.ids[i], rec.ver}};
    int w = INK_CANVAS_W - 4 * INK_MARGIN;
    if (!lv_obj_get_child_count(slot)) {
        ink_ui_label(slot, "", INK_FONT_LIST, INK_WEIGHT_MEDIUM, INK_MARGIN, INK_GAP_M, w);
        ink_ui_label(slot, "", INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_MARGIN, INK_GAP_M + ink_ui_line_height(INK_FONT_LIST), w);
    }
    lv_label_set_text(lv_obj_get_child(slot, 0), L.titles[i]); lv_label_set_text(lv_obj_get_child(slot, 1), L.values[i]);
    lv_obj_set_y(lv_obj_get_child(slot, 1), ink_ui_px(INK_GAP_M + ink_ui_text_height(L.titles[i], INK_FONT_LIST, INK_WEIGHT_MEDIUM, w)));
    if (L.receipts) {
        if (lv_obj_get_child_count(slot) > 2) lv_obj_delete(lv_obj_get_child(slot, 2));
        if (undo) {
            lv_obj_t *b = ink_ui_pill(slot, TR("撤销"), INK_MARGIN, 2 * ink_ui_line_height(INK_FONT_LIST) + 4 * ink_ui_line_height(INK_FONT_NOTE) + 2 * INK_GAP_M, INK_PRIMARY, INK_CONTROL, false, NULL, NULL);
            ink_shell_bind(b, "system.undo", L.args[i]);
        }
    }
}
static ink_ui_action_spec_t list_actions[] = {
    {.id = "prev", .label = TR_NOOP("上一页"), .capability = "ai.pages.prev", .enabled = true},
    {.id = "next", .label = TR_NOOP("下一页"), .capability = "ai.pages.next", .enabled = true},
    {.id = "back", .label = TR_NOOP("返回"), .capability = "shell.back", .enabled = true}
};
static void list_loaded(void *ctx, const ink_receipt_t *rc) {
    if (!L.list || (uintptr_t)ctx != L.generation || !ink_ui_page_current()) return;
    if (rc->status == INK_RC_DONE) L.db = ink_db_open_cached_member(ink_db_find_def("activity.log"), L.member);
    if (rc->status == INK_RC_DONE && L.db) { ink_ui_redraw(); return; }
    L.status = TR("记录暂时读不了，请返回后重试");
    lv_label_set_text(L.notice, L.status); ink_ui_content_changed(); ink_ui_hint_obj(L.notice, INK_REFRESH_UI);
}
static void list_create(lv_obj_t *screen, void *arg) {
    L.receipts = !strcmp(ink_ui_page_name(), "AiReceipts");
    list_actions[0].label = TR("上一页"); list_actions[1].label = TR("下一页"); list_actions[2].label = TR("返回");
    if (L.epoch != ink_member_epoch()) { memset(L.position, 0, sizeof L.position); L.epoch = ink_member_epoch(); snprintf(L.member, sizeof L.member, "%s", ink_member_current()->id); }
    L.keep_s = 365ll * 86400;
    for (int i = 0; ink_member_at(i); i++) if (!strcmp(ink_member_at(i)->id, L.member) && ink_member_at(i)->role == INK_ROLE_CHILD) L.keep_s = 180ll * 86400;
    L.db = ink_db_open_cached_member(ink_db_find_def("activity.log"), L.member);
    L.version = L.db ? ink_db_last_ver(L.db) : 0;
    L.generation = ink_ui_generation(); L.status = L.db ? "" : TR("正在读取记录"); L.notice = NULL;
    ink_shell_statusbar(screen, true);
    bool self = !strcmp(L.member, ink_member_current()->id);
    const char *title = L.receipts ? (self ? TR("AI 做事的结果") : TR("孩子的 AI 做事结果")) : (self ? TR("AI 替你做过的事") : TR("孩子的 AI 活动记录"));
    ink_shell_topbar(screen, title);
    const char *name = L.member;
    for (int at = 0; ink_member_at(at); at++) if (!strcmp(ink_member_at(at)->id, L.member)) { name = ink_member_at(at)->name; break; }
    snprintf(L.heading, sizeof L.heading, TR("%s · %s"), title, name);
    ink_ui_line(screen, name, INK_FONT_NOTE, INK_WEIGHT_MEDIUM, INK_MARGIN, INK_STATUS_BAR + INK_PRIMARY, INK_CANVAS_W - 2 * INK_MARGIN);
    int y = INK_STATUS_BAR + INK_PRIMARY + ink_ui_line_height(INK_FONT_NOTE) + INK_GAP;
    int row_h = 2 * ink_ui_line_height(INK_FONT_LIST) + 4 * ink_ui_line_height(INK_FONT_NOTE) + (L.receipts ? INK_CONTROL + 3 * INK_GAP_M : 2 * INK_GAP_M);
    L.capacity = (INK_CANVAS_H - y - INK_CONTROL - 2 * INK_GAP - INK_MARGIN) / (row_h + INK_GAP_M);
    if (L.capacity > AI_ROWS_MAX) L.capacity = AI_ROWS_MAX;
    lv_obj_t *body = ink_ui_box(screen, 0, y, INK_CANVAS_W, INK_CANVAS_H - y, 0, INK_SURFACE_WHITE, false);
    ink_ui_page_spec_t page = {.id = ink_ui_page_name(), .title = L.heading, .object = {"activity:log", L.version},
        .actions = list_actions, .action_count = 3};
    (void)arg; uint32_t selected = L.selected;
    if (selected && L.db) {
        ink_db_cursor_t cursor = {.desc = true}; ink_rec_t rec; size_t index = 0;
        while (next_record(&cursor, &rec, NULL, 0) == 1 && rec.id != selected) index++;
        L.position[L.receipts].page = index / L.capacity; L.selected = 0;
    }
    L.list = ink_ui_list_create(body, &page, &L.position[L.receipts], L.capacity, 1, row_h,
        L.db ? TR("还没有 AI 做过的事") : "", bind_row, NULL);
    if (L.list) ink_ui_list_refresh(L.list, list_count());
    if (!L.db && L.list) {
        L.notice = ink_ui_notice(body, L.status, INK_GAP_L);
        ink_call_opts_t opts = {.src = INK_SRC_APP, .member = ink_member_current()->id, .reply_exec = INK_EXEC_UI,
            .done = list_loaded, .ctx = (void *)(uintptr_t)L.generation};
        char args[64]; snprintf(args, sizeof args, "{\"member\":\"%s\"}", L.member); opts.args_json = args;
        if (ink_cap_call("activity.list", NULL, &opts)) list_loaded(opts.ctx, &(ink_receipt_t){.status = INK_RC_FAILED});
    }
}
static void list_destroy(void) { if (L.db) ink_db_close(L.db); L.db = NULL; L.list = NULL; }
static int list_snapshot(char *out, size_t cap) {
    if (!L.list) return -1;
    if (L.db) return ink_ui_list_snapshot(L.list, out, cap);
    ink_ui_field_spec_t field = {.id = "status", .role = "status", .label = TR("状态"), .value = L.status};
    ink_ui_page_spec_t page = {.id = ink_ui_page_name(), .title = TR("AI 做过的事"), .fields = &field, .field_count = 1, .actions = list_actions + 2, .action_count = 1};
    return ink_ui_semantics_snapshot(&page, out, cap);
}
static bool list_key(ink_key_t k, bool lng) { return L.list && ink_ui_list_key(L.list, k, lng); }
static void list_settle(void) {
    if (L.list && !L.db) {
        L.db = ink_db_open_cached_member(ink_db_find_def("activity.log"), L.member);
        if (L.db) ink_ui_redraw();
    }
    if (L.db && L.list && L.version != ink_db_last_ver(L.db)) {
        L.version = ink_db_last_ver(L.db); ink_ui_list_refresh(L.list, list_count());
    }
}
static const ink_activity_t LOG = {.name = "AiLog", .app = "ai-log", .create = list_create, .destroy = list_destroy,
    .on_key = list_key, .snapshot = list_snapshot, .settle = list_settle};
static const ink_activity_t RECEIPTS = {.name = "AiReceipts", .app = "ai-log", .create = list_create, .destroy = list_destroy,
    .on_key = list_key, .snapshot = list_snapshot, .settle = list_settle};
static void action(ink_call_t *c) {
    const char *id = c->cap->id;
    if (!strcmp(id, "shell.ai_log") || !strcmp(id, "shell.ai_receipts")) {
        bool receipts = !strcmp(id, "shell.ai_receipts");
        char member[16]; snprintf(member, sizeof member, "%s", ink_member_current()->id);
        ink_args_get_str(&c->args, "member", member, sizeof member);
        bool allowed = !strcmp(member, ink_member_current()->id);
        for (int i = 0; ink_member_at(i); i++) if (!strcmp(member, ink_member_at(i)->id) && ink_member_at(i)->role == INK_ROLE_CHILD && ink_member_current()->role == INK_ROLE_ADULT) allowed = true;
        if (!allowed) { c->rc.status = INK_RC_REJECTED; return; }
        ink_ai_dismiss();
        if (!receipts || strcmp(member, L.member)) ink_ui_drop_app("ai-log");
        snprintf(L.member, sizeof L.member, "%s", member); L.epoch = ink_member_epoch();
        int64_t selected = 0;
        if (receipts) ink_args_get_int(&c->args, "id", &selected);
        memset(&L.position[receipts], 0, sizeof L.position[receipts]); L.selected = (uint32_t)selected;
        ink_ui_push(receipts ? &RECEIPTS : &LOG, NULL);
    } /* entered from SetVoice, Records and AiHome, not an app */
    else if (!strcmp(id, "ai.pages.close") || !strcmp(id, "ai.pages.retry")) {
        if (!E.root) { c->rc.status = INK_RC_STALE; return; }
        bool retry = !strcmp(id, "ai.pages.retry") && E.kind < 2;
        ink_ai_dismiss();
        if (retry) ink_cap_invoke("ai.panel.listen", NULL, c->src, 0, &c->rc);
    } else if (!L.list || (strcmp(ink_ui_page_name(), "AiLog") && strcmp(ink_ui_page_name(), "AiReceipts"))) c->rc.status = INK_RC_STALE;
    else if (!ink_ui_list_turn(L.list, !strcmp(id, "ai.pages.next") ? 1 : -1)) c->rc.status = INK_RC_INVALID;
}
#define PAGE_CAP(id_, title_) {.id = id_, .app = "ai", .title = title_, .effect = INK_FX_NAVIGATE, .child_ok = true, .run = action}
static const ink_cap_t CAPS[] = {
    {.id = "shell.ai_log", .app = "ai", .title = "查看 AI 做过的事", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = action,
     .input = "{\"type\":\"object\",\"properties\":{\"member\":{\"type\":\"string\",\"maxLength\":15}}}"},
    {.id = "shell.ai_receipts", .app = "ai", .title = "查看 AI 做事的结果", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = action,
     .input = "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"integer\",\"minimum\":1,\"maximum\":4294967295},\"member\":{\"type\":\"string\",\"maxLength\":15}}}"},
    PAGE_CAP("ai.pages.close", "继续读"), PAGE_CAP("ai.pages.retry", "重新用话筒问"),
    PAGE_CAP("ai.pages.next", "下一页 AI 记录"), PAGE_CAP("ai.pages.prev", "上一页 AI 记录")
};
int ink_ai_pages_init(void) { return ink_cap_register(CAPS, sizeof CAPS / sizeof *CAPS); }
