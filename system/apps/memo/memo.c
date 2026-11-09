/* Member memos share the capability bus, F4 lists and the long-input draft. */
#include <stdio.h>
#include <string.h>
#include "ink/sys/app.h"
#include "ink/sys/appreg.h"
#include "ink/sys/json.h"
#include "ink/sys/members.h"
#include "ink/sys/textfield.h"
#include "ink/sys/ui_list.h"
#include "ink/ui_shell.h"

static const ink_db_def_t MEMOS = {.name = "memo.notes", .title = "备忘", .scope = INK_DB_MEMBER,
    .schema = 1, .json = true, .ai_read = true, .max_records = 200};
typedef struct { char body[4096], page[24]; uint32_t checked, draft; bool checklist, deleted; } memo_t;
static const ink_activity_t PAGES[6];
enum { LIST, DETAIL, EDIT, CHECKLIST, UNDO, FAIL };
static struct {
    ink_db_t *db;
    ink_ui_list_t *list;
    ink_ui_list_position_t position;
    lv_obj_t *field, *notice, *retry_button, *delete_button;
    memo_t note;
    char member[16], sent[4096], ids[4][32], args[4][96], labels[4][128], object[32], target[64], save_args[160];
    uint32_t id;
    int mode;
    bool saving, retry;
    ink_ui_action_spec_t actions[4];
    ink_ui_page_spec_t model;
    ink_exec_job_t refresh;
} M INK_PSRAM_BSS;

static bool read_note(ink_db_t *db, uint32_t id, memo_t *note) {
    char *raw = ink_app_alloc(INK_DB_RECORD_MAX + 1, true);
    ink_json_t *j = ink_app_alloc(sizeof *j, true);
    ink_rec_t rec = {0}; memset(note, 0, sizeof *note);
    bool ok = raw && j && ink_db_get(db, id, &rec, raw, INK_DB_RECORD_MAX) == 0;
    if (ok) {
        raw[rec.len] = 0; int64_t value = 0;
        ok = ink_json_parse(j, raw, rec.len) == 0 && ink_json_str(j, ink_json_find(j, 0, "body"), note->body, sizeof note->body) >= 0;
        note->checklist = ink_json_eq(j, ink_json_find(j, 0, "checklist"), "true");
        note->deleted = ink_json_eq(j, ink_json_find(j, 0, "deleted"), "true");
        ink_json_int(j, ink_json_find(j, 0, "checked"), &value); note->checked = (uint32_t)value;
        value = 0; ink_json_int(j, ink_json_find(j, 0, "draft"), &value); note->draft = (uint32_t)value;
        ink_json_str(j, ink_json_find(j, 0, "page"), note->page, sizeof note->page);
    }
    ink_app_free(j); ink_app_free(raw); return ok;
}
static unsigned lines(const char *s) { unsigned n = *s ? 1 : 0; for (; *s; s++) n += *s == '\n'; return n; }
static uint32_t remap_checks(const char *old, const char *text, uint32_t checked) {
    uint32_t used = 0, result = 0;
    for (unsigned i = 0; *text && i < 32; i++) {
        size_t len = strcspn(text, "\n"); const char *p = old;
        for (unsigned j = 0; *p && j < 32; j++) {
            size_t size = strcspn(p, "\n"); uint32_t bit = UINT32_C(1) << j;
            if (!(used & bit) && len == size && !memcmp(text, p, len)) { used |= bit; if (checked & bit) result |= UINT32_C(1) << i; break; }
            p += size; if (*p) p++;
        }
        text += len; if (*text) text++;
    }
    return result;
}
static bool clipped(const char *text) {
    while (*text) { size_t len = strcspn(text, "\n"); if (len > ink_text_byte_offset(text, 24)) return true; text += len; if (*text) text++; }
    return false;
}
static void refresh(void *arg) {
    (void)arg;
    if (!M.db || !ink_ui_page_current() || strcmp(M.member, ink_member_current()->id)) return;
    if (M.id && M.mode != LIST && M.mode != UNDO && read_note(M.db, M.id, &M.note) && M.note.deleted) ink_ui_push(&PAGES[UNDO], NULL);
    else if (M.field) return;
    else if (M.mode == DETAIL && M.note.checklist) ink_ui_push(&PAGES[CHECKLIST], (void *)(uintptr_t)M.id);
    else ink_ui_redraw();
}
static void mutate(ink_call_t *c) {
    int64_t id = 0, index = 0, draft = 0; ink_args_get_int(&c->args, "id", &id);
    ink_db_t *db = ink_db_open_member(&MEMOS, ink_cap_call_member(c));
    memo_t *n = ink_app_alloc(sizeof *n, true); char *raw = ink_app_alloc(INK_DB_RECORD_MAX + 1, true);
    bool save = !strcmp(c->cap->id, "memo.save");
    if (!db || !n || !raw) c->rc.status = INK_RC_FAILED;
    else if (!read_note(db, (uint32_t)id, n) && !save) c->rc.status = INK_RC_INVALID;
    if (c->rc.status == INK_RC_DONE) {
        if (save) {
            memcpy(raw, n->body, sizeof n->body);
            ink_args_get_int(&c->args, "draft", &draft); ink_args_get_str(&c->args, "page", n->page, sizeof n->page);
            if (n->deleted || ink_text_draft_read(ink_cap_call_member(c), n->page, (uint32_t)draft, n->body, sizeof n->body) < 0) c->rc.status = INK_RC_INVALID;
            if (n->checklist) n->checked = remap_checks(raw, n->body, n->checked);
            n->draft = (uint32_t)draft;
        } else if (!strcmp(c->cap->id, "memo.undo")) n->deleted = false;
        else if (n->deleted) c->rc.status = INK_RC_INVALID;
        else if (!strcmp(c->cap->id, "memo.delete")) n->deleted = true;
        else if (!strcmp(c->cap->id, "memo.convert")) n->checklist = true;
        else {
            ink_args_get_int(&c->args, "index", &index);
            if (!n->checklist || index < 0 || index >= 32 || (unsigned)index >= lines(n->body)) c->rc.status = INK_RC_INVALID;
            else n->checked ^= UINT32_C(1) << index;
        }
        if (n->checklist && lines(n->body) > 32) c->rc.status = INK_RC_INVALID;
        if (c->rc.status == INK_RC_DONE) {
            ink_jw_t w; ink_jw_init(&w, raw, INK_DB_RECORD_MAX + 1);
            ink_jw_raw(&w, "{\"body\":"); ink_jw_str(&w, n->body);
            ink_jw_raw(&w, ",\"checklist\":"); ink_jw_raw(&w, n->checklist ? "true" : "false"); ink_jw_raw(&w, ",\"deleted\":"); ink_jw_raw(&w, n->deleted ? "true" : "false");
            ink_jw_raw(&w, ",\"checked\":"); ink_jw_int(&w, n->checked); ink_jw_raw(&w, ",\"draft\":"); ink_jw_int(&w, n->draft);
            ink_jw_raw(&w, ",\"page\":"); ink_jw_str(&w, n->page); ink_jw_raw(&w, "}");
            ink_db_write_t wr = {.id = (uint32_t)id, .data = raw, .len = w.len, .src = c->src, .written_ver = &c->rc.revision};
            if (w.overflow || ink_db_put(db, &wr, NULL) != 0 || ink_db_wait(db, c->rc.revision, (int64_t)ink_cap_time_left_ms(c) * 1000) != 0) c->rc.status = INK_RC_FAILED;
        }
    }
    snprintf(c->rc.object, sizeof c->rc.object, "memo:%u", (unsigned)id);
    snprintf(c->rc.say, sizeof c->rc.say, "%s", c->rc.status == INK_RC_DONE ? "已记下" : "这条没存上，字还在这里。请腾出空间后重试");
    if (c->rc.status == INK_RC_DONE) ink_exec_post(INK_EXEC_UI, &M.refresh, refresh, NULL);
    ink_app_free(raw); ink_app_free(n); ink_db_close(db);
}
static void save_failed(bool failed) {
    int delta = (failed - (M.mode == FAIL)) * ink_ui_px(INK_PRIMARY);
    lv_obj_set_y(M.field, lv_obj_get_y(M.field) + delta); lv_obj_set_height(M.field, lv_obj_get_height(M.field) - delta);
    lv_obj_t *label = lv_obj_get_child(M.field, 0); lv_obj_set_height(label, lv_obj_get_height(label) - delta);
    M.mode = failed ? FAIL : EDIT;
    lv_obj_set_y(M.notice, ink_ui_px(failed ? INK_STATUS_BAR + INK_PRIMARY + INK_GAP_S : INK_CANVAS_H - INK_PRIMARY - INK_CONTROL - INK_GAP_L));
    if (failed) lv_obj_remove_flag(M.retry_button, LV_OBJ_FLAG_HIDDEN); else lv_obj_add_flag(M.retry_button, LV_OBJ_FLAG_HIDDEN);
    if (failed) lv_obj_add_flag(M.delete_button, LV_OBJ_FLAG_HIDDEN); else lv_obj_remove_flag(M.delete_button, LV_OBJ_FLAG_HIDDEN);
}
static void reply(void *ctx, const ink_receipt_t *rc) {
    if ((uint32_t)(uintptr_t)ctx != ink_ui_generation() || !ink_ui_page_current() || strcmp(M.member, ink_member_current()->id)) return;
    M.saving = false;
    if (rc->status != INK_RC_DONE && M.field && M.mode == EDIT) save_failed(true);
    else if (rc->status == INK_RC_DONE && M.field && M.mode == FAIL) save_failed(false);
    if (M.notice) lv_label_set_text(M.notice, rc->say);
    if (rc->status == INK_RC_DONE && !M.field) ink_ui_redraw();
    ink_ui_content_changed();
}
static void request(const char *cap, const ink_args_t *args) {
    ink_call_opts_t opts = {.src = INK_SRC_TOUCH, .member = M.member, .reply_exec = INK_EXEC_UI, .done = reply, .ctx = (void *)(uintptr_t)ink_ui_generation()};
    if (ink_cap_call(cap, args, &opts) != 0) { M.saving = false; if (M.notice) lv_label_set_text(M.notice, "这条没存上，字还在这里。请重试"); }
}
static void navigate(ink_call_t *c) {
    const char *cap = c->cap->id; int64_t id = 0; ink_args_get_int(&c->args, "id", &id);
    if (!strcmp(cap, "memo.next") || !strcmp(cap, "memo.prev")) { if (M.list) ink_ui_list_turn(M.list, !strcmp(cap, "memo.next") ? 1 : -1); }
    else if (!strcmp(cap, "memo.retry")) M.retry = true;
    else {
        int mode = !strcmp(cap, "memo.edit") ? EDIT : DETAIL;
        memo_t *n = ink_app_alloc(sizeof *n, true);
        if (id && (!M.db || !n || !read_note(M.db, (uint32_t)id, n) || n->deleted)) c->rc.status = INK_RC_INVALID;
        else { if (mode == DETAIL && n && n->checklist) mode = CHECKLIST; memset(&M.position, 0, sizeof M.position); ink_ui_push(&PAGES[mode], (void *)(uintptr_t)id); }
        ink_app_free(n);
    }
}
static void save_current(void) {
    ink_args_t a; ink_args_init(&a); ink_args_int(&a, "id", M.id); ink_args_int(&a, "draft", M.note.draft); ink_args_str(&a, "page", M.note.page);
    snprintf(M.sent, sizeof M.sent, "%s", ink_textfield_text(M.field)); M.retry = false; M.saving = true; request("memo.save", &a);
}
static void settle(void) {
    if (!M.field || M.saving || !ink_ui_page_current()) return;
    ink_textfield_long_refresh(M.field);
    const char *text = ink_textfield_text(M.field);
    if (!M.retry && (!strcmp(text, M.sent) || M.mode == FAIL)) return;
    save_current();
}
static void retry(lv_event_t *e) { (void)e; ink_args_t a; ink_args_init(&a); request("memo.retry", &a); }
static const ink_ui_action_spec_t PAGING[] = {
    {.id = "prev", .label = "上一页", .capability = "memo.prev"}, {.id = "next", .label = "下一页", .capability = "memo.next"},
    {.id = "new", .label = "写一条", .capability = "memo.edit", .enabled = true}
};
static bool nth(size_t index, uint32_t *id, memo_t *note) {
    ink_db_cursor_t cur = {0}; ink_rec_t rec;
    while (ink_db_next(M.db, &cur, &rec, NULL, 0) == 1) {
        if (read_note(M.db, rec.id, note) && !note->deleted && index-- == 0) { *id = rec.id; return true; }
    }
    return false;
}
static void bind_item(lv_obj_t *slot, size_t index, ink_ui_field_spec_t *field, ink_ui_action_spec_t *action, void *ctx) {
    (void)ctx; unsigned at = index % 4; uint32_t id = M.id; memo_t *n = ink_app_alloc(sizeof *n, true);
    if (!n) return;
    bool list = M.mode == LIST || M.mode == UNDO;
    if (list && !nth(index, &id, n)) { ink_app_free(n); return; }
    const char *text = list ? n->body : M.note.body;
    if (!list && M.mode == CHECKLIST) for (size_t i = 0; i < index && *text; i++) { const char *p = strchr(text, '\n'); text = p ? p + 1 : text + strlen(text); }
    else if (!list) text += ink_text_byte_offset(text, index * 24);
    size_t len = ink_text_byte_offset(text, 24);
    if (M.mode == CHECKLIST && strcspn(text, "\n") < len) len = strcspn(text, "\n");
    bool truncated = M.mode == CHECKLIST && strcspn(text, "\n") > len;
    snprintf(M.labels[at], sizeof M.labels[at], "%.*s%s", (int)len, text, truncated ? "…" : "");
    if (list) for (char *p = M.labels[at]; *p; p++) if (*p == '\n') *p = ' ';
    snprintf(M.ids[at], sizeof M.ids[at], "memo:%u:%zu", (unsigned)id, list ? 0 : index);
    snprintf(M.args[at], sizeof M.args[at], "{\"id\":%u,\"index\":%zu}", (unsigned)id, index);
    int x = M.mode == CHECKLIST ? INK_CONTROL : INK_GAP;
    if (!lv_obj_get_child_count(slot)) ink_ui_label(slot, "", INK_FONT_LIST, INK_WEIGHT_REGULAR, x, INK_GAP_S, INK_CANVAS_W - 2 * INK_MARGIN - x - INK_GAP);
    lv_label_set_text(lv_obj_get_child(slot, 0), M.labels[at]);
    if (M.mode == CHECKLIST) {
        if (lv_obj_get_child_count(slot) > 1) lv_obj_delete(lv_obj_get_child(slot, 1));
        bool checked = (M.note.checked & (UINT32_C(1) << index)) != 0;
        lv_obj_t *box = ink_ui_box(slot, INK_GAP, INK_GAP_L, INK_ICON_LIST, INK_ICON_LIST, -1, checked ? INK_SURFACE_BLACK : INK_SURFACE_WHITE, true);
        if (checked) ink_ui_icon(box, "check", INK_ICON_MEDIUM, INK_ICON_LINE, INK_GAP_XS, INK_GAP_XS, true);
    }
    *field = (ink_ui_field_spec_t){.id = M.ids[at], .role = M.mode == CHECKLIST ? "checkbox" : "listitem", .label = M.labels[at], .value = M.mode == CHECKLIST ? (M.note.checked & (UINT32_C(1) << index) ? "已完成" : "未完成") : "", .object = {M.ids[at], ink_db_last_ver(M.db)}};
    *action = (ink_ui_action_spec_t){.id = M.ids[at], .label = M.mode == CHECKLIST ? "勾选或取消" : "查看备忘", .capability = M.mode == CHECKLIST ? "memo.check" : "memo.open", .args_json = M.args[at], .object = field->object, .enabled = list || M.mode == CHECKLIST};
    ink_app_free(n);
}
static void create(lv_obj_t *screen, void *arg) {
    int mode = 0; while (mode < 6 && strcmp(PAGES[mode].name, ink_ui_page_name())) mode++;
    M.mode = mode; M.id = (uint32_t)(uintptr_t)arg; M.saving = M.retry = false;
    M.model = (ink_ui_page_spec_t){.id = ink_ui_page_name(), .title = "备忘"};
    if (strcmp(M.member, ink_member_current()->id)) { memset(&M.position, 0, sizeof M.position); memset(&M.note, 0, sizeof M.note); snprintf(M.member, sizeof M.member, "%s", ink_member_current()->id); }
    ink_shell_statusbar(screen, true); ink_shell_topbar(screen, "备忘");
    M.notice = ink_ui_notice(screen, mode == FAIL ? "这条没存上。字还在这里。请腾出空间后重试" : "", INK_CANVAS_H - INK_PRIMARY - INK_CONTROL - INK_GAP_L);
    M.db = ink_db_open_cached_member(&MEMOS, M.member);
    if (!M.db) {
        lv_label_set_text(M.notice, "正在读取备忘"); ink_args_t a; ink_args_init(&a); ink_args_str(&a, "collection", MEMOS.name); request("data.list", &a); return;
    }
    bool list = mode == LIST || mode == UNDO;
    if (!list && mode != FAIL) {
        if (!M.id) { M.id = ink_db_new_id(M.db); memset(&M.note, 0, sizeof M.note); }
        else if (!read_note(M.db, M.id, &M.note) || M.note.deleted) { lv_label_set_text(M.notice, "找不到这条备忘"); return; }
    }
    snprintf(M.object, sizeof M.object, "memo:%u", (unsigned)M.id);
    snprintf(M.target, sizeof M.target, "{\"id\":%u}", (unsigned)M.id);
    M.model = (ink_ui_page_spec_t){.id = ink_ui_page_name(), .title = "备忘", .object = {M.object, ink_db_last_ver(M.db)}, .actions = M.actions};
    M.actions[0] = (ink_ui_action_spec_t){.id = "edit", .label = "编辑备忘", .capability = "memo.edit", .args_json = M.target, .enabled = !list};
    M.actions[1] = (ink_ui_action_spec_t){.id = "delete", .label = "删除备忘", .capability = "memo.delete", .args_json = M.target, .enabled = !list};
    M.actions[2] = (ink_ui_action_spec_t){.id = "convert", .label = "转成清单", .capability = "memo.convert", .args_json = M.target, .enabled = !list && !M.note.checklist};
    M.model.action_count = 3;
    if (mode == EDIT || mode == FAIL) {
        ink_text_cfg_t cfg = {.type = INK_TEXT_PLAIN, .max = 4095, .initial = M.note.body, .draft_page = ink_ui_page_name(), .draft_id = !strcmp(M.note.page, ink_ui_page_name()) ? M.note.draft : 0};
        M.field = ink_textfield_long_create(screen, &cfg); snprintf(M.sent, sizeof M.sent, "%s", mode == FAIL ? M.note.body : "\1");
        M.note.draft = ink_text_draft_id(); snprintf(M.note.page, sizeof M.note.page, "%s", ink_ui_page_name());
        M.delete_button = ink_ui_circle(screen, "trash", INK_CANVAS_W - INK_MARGIN - INK_CONTROL, INK_STATUS_BAR + INK_GAP_S, INK_CONTROL, INK_ICON_LIST, INK_SURFACE_WHITE, NULL, NULL); ink_shell_bind(M.delete_button, "memo.delete", M.target);
        M.retry_button = ink_ui_pill(screen, "重试", INK_CANVAS_W - INK_MARGIN - INK_PRIMARY, INK_STATUS_BAR, INK_PRIMARY, INK_CONTROL, false, retry, NULL);
        lv_obj_add_flag(M.retry_button, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    size_t total = 0; uint32_t undo = 0, undo_ver = 0;
    if (list) {
        ink_db_cursor_t cur = {0}; ink_rec_t rec; memo_t *n = ink_app_alloc(sizeof *n, true);
        while (n && ink_db_next(M.db, &cur, &rec, NULL, 0) == 1) if (read_note(M.db, rec.id, n)) {
            if (!n->deleted) total++;
            else if (rec.ver > undo_ver) { undo = rec.id; undo_ver = rec.ver; }
        }
        ink_app_free(n);
    } else total = mode == CHECKLIST ? lines(M.note.body) : (ink_text_char_offset(M.note.body, strlen(M.note.body)) + 23) / 24;
    if (mode == CHECKLIST && clipped(M.note.body)) lv_label_set_text(M.notice, "长项已省略，编辑备忘可看全文");
    ink_ui_page_spec_t model = {.id = ink_ui_page_name(), .title = "备忘", .object = {"memo.notes", ink_db_last_ver(M.db)}, .actions = PAGING, .action_count = 3};
    lv_obj_t *body = ink_ui_box(screen, 0, INK_STATUS_BAR + INK_PRIMARY + INK_GAP_L, INK_CANVAS_W, 4 * (INK_ROW_DOUBLE + INK_GAP_M) + INK_PRIMARY, 0, INK_SURFACE_WHITE, false);
    M.list = ink_ui_list_create(body, &model, &M.position, 4, 1, INK_ROW_DOUBLE, "还没有备忘，写一条吧", bind_item, NULL);
    if (M.list) ink_ui_list_refresh(M.list, total);
    if (list) {
        ink_shell_bind(ink_ui_pill(screen, "写一条", INK_MARGIN, INK_CANVAS_H - INK_PRIMARY - INK_MARGIN, INK_CANVAS_W - 2 * INK_MARGIN, INK_PRIMARY, false, NULL, NULL), "memo.edit", "{}");
        M.model.action_count = 0;
        if (undo) {
            snprintf(M.target, sizeof M.target, "{\"id\":%u}", (unsigned)undo);
            M.actions[0] = (ink_ui_action_spec_t){.id = "undo", .label = "撤销删除", .capability = "memo.undo", .args_json = M.target, .enabled = true}; M.model.action_count = 1;
            ink_shell_bind(ink_ui_pill(screen, "已删除一条备忘 · 撤销", INK_MARGIN, INK_CANVAS_H - INK_PRIMARY - INK_MARGIN - INK_CONTROL - INK_GAP_M, INK_CANVAS_W - 2 * INK_MARGIN, INK_CONTROL, false, NULL, NULL), "memo.undo", M.target);
        }
    } else for (int i = 0; i < 3; i++) if (M.actions[i].enabled) ink_shell_bind(ink_ui_pill(screen, M.actions[i].label, INK_MARGIN + i * (INK_CANVAS_W - 2 * INK_MARGIN) / 3, INK_CANVAS_H - INK_PRIMARY - INK_MARGIN, (INK_CANVAS_W - 2 * INK_MARGIN) / 3, INK_CONTROL, false, NULL, NULL), M.actions[i].capability, M.target);
}
static void destroy(void) {
    if (M.field && M.mode != FAIL && ink_ui_page_current() && !strcmp(M.member, ink_member_current()->id) && strcmp(ink_textfield_text(M.field), M.sent)) save_current();
    ink_db_close(M.db); M.db = NULL; M.list = NULL; M.field = M.notice = M.retry_button = M.delete_button = NULL;
}
static int snapshot(char *out, size_t cap) {
    char *body = ink_app_alloc(cap, true), *actions = ink_app_alloc(cap, true); int result = -1;
    if (M.field) {
        snprintf(M.save_args, sizeof M.save_args, "{\"id\":%u,\"draft\":%u,\"page\":\"%s\"}", (unsigned)M.id, (unsigned)M.note.draft, M.note.page);
        M.actions[3] = M.mode == FAIL ? (ink_ui_action_spec_t){.id = "retry", .label = "重试", .capability = "memo.retry", .enabled = true} : (ink_ui_action_spec_t){.id = "save", .label = "记下当前草稿", .capability = "memo.save", .args_json = M.save_args, .enabled = true};
        M.model.actions = M.mode == FAIL ? M.actions + 3 : M.actions; M.model.action_count = M.mode == FAIL ? 1 : 4;
    }
    if (body && actions && ink_ui_semantics_snapshot(&M.model, actions, cap) > 0) {
        int n = M.field ? ink_textfield_long_snapshot(M.field, body, cap) : M.list ? ink_ui_list_snapshot(M.list, body, cap) : 0;
        if (!n) snprintf(body, cap, "{}");
        if (n >= 0) {
            ink_jw_t w; ink_jw_init(&w, out, cap);
            ink_jw_raw(&w, "{\"view\":"); ink_jw_str(&w, PAGES[M.mode].name);
            ink_jw_raw(&w, ",\"content_truncated\":"); ink_jw_raw(&w, M.mode == CHECKLIST && clipped(M.note.body) ? "true,\"read_full\":\"memo.edit\"" : "false");
            ink_jw_raw(&w, ",\"status\":"); ink_jw_str(&w, M.notice ? lv_label_get_text(M.notice) : "");
            ink_jw_raw(&w, ",\"content\":"); ink_jw_raw(&w, body); ink_jw_raw(&w, ",\"controls\":"); ink_jw_raw(&w, actions); ink_jw_raw(&w, "}");
            result = w.overflow ? -1 : (int)w.len;
        }
    }
    ink_app_free(actions); ink_app_free(body); if (result < 0 && cap) out[0] = 0; return result;
}
static bool page_key(ink_key_t key, bool lng) { return M.list && ink_ui_list_key(M.list, key, lng); }
#define PAGE(name_) {.name = name_, .app = "memo", .create = create, .destroy = destroy, .snapshot = snapshot, .settle = settle, .on_key = page_key}
static const ink_activity_t PAGES[] = {PAGE("Memo"), PAGE("MemoDetail"), PAGE("MemoEdit"), PAGE("MemoChecklist"), PAGE("MemoUndo"), PAGE("MemoSaveFail")};
static const ink_app_def_t APP = {.id = "memo", .title = "备忘", .icon = INK_UI_ICON(memo), .entry = &PAGES[LIST], .system_app = true, .child_ok = true};
#define ID "\"id\":{\"type\":\"integer\",\"minimum\":1,\"maximum\":4294967295}"
#define RECORD "{\"type\":\"object\",\"properties\":{" ID "},\"required\":[\"id\"]}"
static const ink_cap_t CAPS[] = {
    {.id = "memo.save", .title = "记下当前成员的长文草稿", .input = "{\"type\":\"object\",\"properties\":{" ID ",\"draft\":{\"type\":\"integer\",\"minimum\":1},\"page\":{\"type\":\"string\",\"enum\":[\"MemoEdit\",\"MemoSaveFail\"]}},\"required\":[\"id\",\"draft\",\"page\"]}", .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .exec = INK_EXEC_BG, .run = mutate},
    {.id = "memo.delete", .title = "删除备忘（可撤销）", .input = RECORD, .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .exec = INK_EXEC_BG, .run = mutate},
    {.id = "memo.undo", .title = "撤销删除备忘", .input = RECORD, .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .exec = INK_EXEC_BG, .run = mutate},
    {.id = "memo.convert", .title = "逐行转成清单（最多 32 项）", .input = RECORD, .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .exec = INK_EXEC_BG, .run = mutate},
    {.id = "memo.check", .title = "勾选或取消清单项", .input = "{\"type\":\"object\",\"properties\":{" ID ",\"index\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":31}},\"required\":[\"id\",\"index\"]}", .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .exec = INK_EXEC_BG, .run = mutate},
    {.id = "memo.open", .title = "查看备忘", .input = RECORD, .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "memo.edit", .title = "写一条或编辑备忘", .input = "{\"type\":\"object\",\"properties\":{" ID "}}", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "memo.next", .title = "备忘下一页", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "memo.prev", .title = "备忘上一页", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "memo.retry", .title = "重试记下备忘", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate}
};
int ink_memo_init(void) {
    return ink_db_register(&MEMOS) || ink_cap_register(CAPS, sizeof CAPS / sizeof *CAPS) || ink_app_register(&APP) ? -1 : 0;
}
