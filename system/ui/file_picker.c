#include "ink/sys/i18n.h"
#include "ink/sys/file_picker.h"
#include "ink/sys/ui_list.h"
#include "ink/sys/members.h"
#include "ink/sys/state.h"
#include "ink/ui_shell.h"
#include "ink/osal.h"
#include <stdio.h>
#include <string.h>

typedef struct {
    uint32_t id;
    char name[INK_FS_NAME_MAX];
} picker_item_t;
typedef struct {
    lv_obj_t *root, *notice, *caller;
    ink_ui_list_t *list;
    ink_ui_list_position_t position;
    ink_exec_job_t job;
    ink_file_picker_done_fn done;
    ink_call_t *call;
    void *ctx;
    ink_fs_type_t type;
    uint32_t epoch, picked;
    size_t count;
    int error;
    bool working;
    char ids[4][32], args[4][48];
    picker_item_t items[INK_FS_INDEX_MAX];
} picker_t;
static picker_t *P;
static const ink_ui_overlay_t OVERLAY;
static void caller_deleted(lv_event_t *e);

static const char *status(picker_t *p) {
    return p->working ? TR("正在读取文件") : p->error ? TR("暂时读不了文件，请返回后重试") : "";
}
static void close_picker(uint32_t id) {
    picker_t *p = P;
    P = NULL;
    lv_obj_remove_event_cb_with_user_data(p->caller, caller_deleted, p);
    ink_ui_overlay_remove(&OVERLAY); lv_obj_delete(p->root);
    ink_ui_content_changed(); ink_ui_hint_frame(INK_REFRESH_FLASH_UI);
    if (p->call) {
        char result[48]; snprintf(result, sizeof result, "{\"id\":%u}", (unsigned)id);
        p->call->rc.status = id ? INK_RC_DONE : INK_RC_CANCELLED;
        ink_cap_result(p->call, result); ink_cap_done(p->call); p->call = NULL;
    }
    if (p->done && p->epoch == ink_member_epoch()) p->done(p->ctx, id);
    if (!p->working) osal_free(p);
}
static void caller_deleted(lv_event_t *e) {
    picker_t *p = lv_event_get_user_data(e);
    p->done = NULL; close_picker(0);
}
static void bind_item(lv_obj_t *slot, size_t index, ink_ui_field_spec_t *field, ink_ui_action_spec_t *action, void *ctx) {
    picker_t *p = ctx;
    unsigned at = index % 4;
    picker_item_t *item = &p->items[index];
    const char *ready = p->type == INK_FS_TEXT ? TR("可以读") : TR("可以选择");
    if (!lv_obj_get_child_count(slot)) {
        ink_ui_icon(slot, p->type == INK_FS_IMAGE ? "camera" : "book", INK_ICON_LIST, INK_ICON_LINE, INK_GAP, INK_GAP_L, false);
        ink_ui_line(slot, "", INK_FONT_LIST, INK_WEIGHT_MEDIUM, INK_CIRCLE, INK_GAP_S, INK_CANVAS_W - 2 * INK_MARGIN - INK_CIRCLE - INK_GAP);
        ink_ui_line(slot, ready, INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_CIRCLE, INK_GAP_S + ink_ui_line_height(INK_FONT_LIST), INK_CANVAS_W - 2 * INK_MARGIN - INK_CIRCLE - INK_GAP);
    }
    lv_label_set_text(lv_obj_get_child(slot, 1), item->name);
    snprintf(p->ids[at], sizeof p->ids[at], "file:%u", (unsigned)item->id);
    snprintf(p->args[at], sizeof p->args[at], "{\"id\":%u}", (unsigned)item->id);
    *field = (ink_ui_field_spec_t){.id = p->ids[at], .role = "file", .label = item->name, .value = ready, .object = {p->ids[at], p->epoch}};
    *action = (ink_ui_action_spec_t){.id = p->ids[at], .label = TR("选择"), .capability = "file_picker.select", .args_json = p->args[at], .object = field->object, .enabled = !p->working && !p->error};
}
static int snapshot(char *out, size_t cap) {
    if (!P || P->epoch != ink_member_epoch()) { if (cap) out[0] = 0; return -1; }
    int n = ink_ui_list_snapshot(P->list, out, cap);
    if (n < 0) return -1;
    int more = snprintf(out + n - 1, cap - (size_t)n + 1, ",\"status\":\"%s\"}", status(P));
    if (more < 0 || (size_t)more >= cap - (size_t)n + 1) { out[0] = 0; return -1; }
    return n - 1 + more;
}
static bool on_key(ink_key_t key, bool lng) {
    if (!lng && (key == INK_KEY_HOME || key == INK_KEY_BACK)) {
        ink_receipt_t rc; ink_cap_invoke_json("file_picker.cancel", NULL, INK_SRC_KEY, ink_ui_content_version(), &rc);
        return true;
    }
    return ink_ui_list_key(P->list, key, lng);
}
static const ink_ui_overlay_t OVERLAY = {.name = "FilePicker", .level = 0, .on_key = on_key, .snapshot = snapshot};
static void member_changed(void *ctx, int h, const char *id) {
    (void)ctx; (void)h; (void)id;
    if (P && P->epoch != ink_member_epoch()) close_picker(0);
}
static void loaded(void *arg) {
    picker_t *p = arg;
    p->working = false;
    if (P != p) { osal_free(p); return; }
    if (p->epoch != ink_member_epoch()) { close_picker(0); return; }
    if (p->picked && !p->error) { close_picker(p->picked); return; }
    if (p->call) { p->call->rc.status = INK_RC_FAILED; ink_cap_done(p->call); p->call = NULL; }
    lv_label_set_text(p->notice, status(p));
    ink_ui_list_refresh(p->list, p->error ? 0 : p->count);
}
static void read_files(void *arg) {
    picker_t *p = arg;
    ink_fs_entry_t entry;
    if (p->picked) p->error = ink_fs_lookup(p->picked, &entry);
    else for (int root = -1; root < INK_MEMBERS_MAX && p->epoch == ink_member_epoch(); root++) {
        char path[INK_FS_PATH_MAX] = "/family";
        if (root >= 0) {
            const ink_member_t *m = ink_member_at(root);
            if (!m) break;
            snprintf(path, sizeof path, "/members/%s", m->id);
            if (!strcmp(m->id, ink_member_current()->id)) snprintf(path, sizeof path, "/me");
        }
        uint32_t cursor = 0;
        int r;
        while ((r = ink_fs_index(path, p->type, &cursor, &entry)) == 1) {
            if (p->epoch != ink_member_epoch()) break;
            picker_item_t *item = &p->items[p->count++];
            item->id = entry.id;
            snprintf(item->name, sizeof item->name, "%s", strrchr(entry.path, '/') + 1);
            char *dot = strrchr(item->name, '.');
            if (dot) *dot = 0;
            if (p->count == INK_FS_INDEX_MAX) break;
        }
        if (r < 0 && r != INK_FS_ERR_DENIED && r != INK_FS_ERR_NOT_FOUND) { p->error = r; break; }
        if (p->count == INK_FS_INDEX_MAX) break;
    }
    ink_exec_post(INK_EXEC_UI, &p->job, loaded, p);
}
static void action(ink_call_t *c) {
    if (!P || P->epoch != ink_member_epoch()) { c->rc.status = INK_RC_STALE; return; }
    if (!strcmp(c->cap->id, "file_picker.cancel")) { close_picker(0); return; }
    if (P->working) { c->rc.status = INK_RC_FAILED; return; }
    if (!strcmp(c->cap->id, "file_picker.select")) {
        int64_t id = 0;
        ink_args_get_int(&c->args, "id", &id);
        size_t start = P->position.page * 4, end = start + 4 < P->count ? start + 4 : P->count;
        for (size_t i = start; !P->error && i < end; i++) if (id == P->items[i].id) {
            P->picked = (uint32_t)id; P->working = true;
            if (ink_exec_post(INK_EXEC_BG, &P->job, read_files, P)) { P->working = false; c->rc.status = INK_RC_FAILED; return; }
            lv_label_set_text(P->notice, status(P)); ink_ui_content_changed();
            P->call = c; ink_cap_defer(c);
            snprintf(c->rc.object, sizeof c->rc.object, "file:%u", (unsigned)id);
            return;
        }
        c->rc.status = INK_RC_INVALID;
    } else if (!ink_ui_list_turn(P->list, !strcmp(c->cap->id, "file_picker.next") ? 1 : -1)) c->rc.status = INK_RC_INVALID;
}
int ink_file_picker_open(ink_fs_type_t type, ink_file_picker_done_fn done, void *ctx) {
    if (!ink_ui_is_current() || !ink_ui_page_current() || ink_ui_overlay_up() || P || !done || type < INK_FS_ANY || type > INK_FS_TEXT) return -1;
    picker_t *p = osal_calloc(1, sizeof *p, OSAL_MEM_EXTERNAL);
    if (!p) return -1;
    p->type = type; p->epoch = ink_member_epoch(); p->done = done; p->ctx = ctx; p->working = true;
    const char *title = type == INK_FS_IMAGE ? TR("选一张图片") : type == INK_FS_TEXT ? TR("选一本书") : type == INK_FS_AUDIO ? TR("选一段音频") : TR("选一个文件");
    const ink_ui_action_spec_t ACTIONS[] = {
        {.id = "prev", .label = TR("上一页"), .capability = "file_picker.prev"},
        {.id = "next", .label = TR("下一页"), .capability = "file_picker.next"},
        {.id = "cancel", .label = TR("取消"), .capability = "file_picker.cancel", .enabled = true}
    };
    ink_ui_page_spec_t page = {.id = "FilePicker", .title = title, .object = {"FilePicker", p->epoch}, .actions = ACTIONS, .action_count = 3};
    p->root = ink_ui_box(lv_layer_sys(), 0, 0, INK_CANVAS_W, INK_CANVAS_H, 0, INK_SURFACE_WHITE, true);
    ink_shell_statusbar(p->root, true);
    lv_obj_t *back = ink_ui_circle(p->root, "back", INK_MARGIN, INK_STATUS_BAR, INK_CONTROL, INK_ICON_MEDIUM, INK_SURFACE_STATIC, NULL, NULL);
    ink_shell_bind(back, "file_picker.cancel", NULL);
    ink_ui_label(p->root, title, INK_FONT_TITLE, INK_WEIGHT_BOLD, INK_MARGIN + INK_CONTROL + INK_GAP, INK_STATUS_BAR + INK_GAP_S, INK_CANVAS_W - 2 * INK_MARGIN - INK_CONTROL - INK_GAP);
    int y = INK_STATUS_BAR + INK_PRIMARY + INK_GAP_XS;
    lv_obj_t *body = ink_ui_box(p->root, 0, y, INK_CANVAS_W, INK_CANVAS_H - y, 0, INK_SURFACE_WHITE, false);
    p->notice = ink_ui_notice(p->root, status(p), INK_CANVAS_H - INK_PRIMARY);
    p->list = ink_ui_list_create(body, &page, &p->position, 4, 1, INK_ROW_DOUBLE, TR("这里还没有可选的文件"), bind_item, p);
    if (!p->list || ink_exec_post(INK_EXEC_BG, &p->job, read_files, p)) { lv_obj_delete(p->root); osal_free(p); return -1; }
    p->caller = lv_screen_active(); lv_obj_add_event_cb(p->caller, caller_deleted, LV_EVENT_DELETE, p);
    P = p; ink_ui_overlay_add(&OVERLAY, p->root); ink_ui_list_refresh(p->list, 0);
    ink_ui_hint_frame(INK_REFRESH_FLASH_UI);
    return 0;
}
int ink_file_picker_init(void) {
    static const ink_cap_t CAPS[] = {
        {.id = "file_picker.prev", .title = "上一页文件", .activity = "FilePicker", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = action},
        {.id = "file_picker.next", .title = "下一页文件", .activity = "FilePicker", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = action},
        {.id = "file_picker.cancel", .title = "取消选文件", .activity = "FilePicker", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = action},
        {.id = "file_picker.select", .title = "选择文件编号", .activity = "FilePicker", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = action,
         .input = "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"integer\",\"minimum\":1}},\"required\":[\"id\"]}"}
    };
    if (ink_cap_register(CAPS, sizeof CAPS / sizeof *CAPS)) return -1;
    return ink_state_watch("member.", member_changed, NULL, INK_EXEC_UI);
}
