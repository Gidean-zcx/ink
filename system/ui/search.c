/* F7: read-only member search; three visible results, no resident worker. */
/* ui-scale-runtime: dimension x,y,w,h Theme-derived fixed search and result rectangles. */
#include <stdio.h>
#include <string.h>
#include "ink/osal.h"
#include "ink/sys/appreg.h"
#include "ink/sys/file.h"
#include "ink/sys/json.h"
#include "ink/sys/members.h"
#include "ink/sys/search.h"
#include "ink/sys/settings.h"
#include "ink/sys/textfield.h"
#include "ink/sys/ui_list.h"
#include "ink/ui_shell.h"

typedef struct { char id[64], title[128], value[128]; unsigned kind; uint32_t revision; } result_t;
static const char *const TYPES[] = {"应用", "设置", "联系人", "备忘", "文件"};
static const char *const ICONS[] = {"grid", "gear", "phone", "memo", "folder"};
static const ink_activity_t SEARCH, EMPTY, FILE_INFO;
static struct {
    char member[16], query[193], args[3][160], error[96];
    uint32_t epoch, version;
    size_t total;
    result_t items[3], file;
    ink_ui_list_position_t position;
    ink_ui_list_t *list;
    lv_obj_t *field;
    ink_ui_field_spec_t spec;
    ink_ui_page_spec_t model;
    bool pending;
} S INK_PSRAM_BSS;
typedef struct { char query[193], member[16], raw[INK_DB_RECORD_MAX + 1], text[4096];
    ink_json_t json; ink_jw_t writer; size_t total, cursor; unsigned shown; } query_t;
static bool visible(void) {
    return ink_ui_page_current() && S.epoch == ink_member_epoch() &&
        (!strcmp(ink_ui_page_name(), "Search") || !strcmp(ink_ui_page_name(), "SearchEmpty"));
}
static void match(query_t *q, unsigned kind, const char *id, const char *title, const char *value, uint32_t revision) {
    if (!strstr(title, q->query) && !strstr(value, q->query) && !strstr(id, q->query)) return;
    if (q->total++ < q->cursor || q->shown == 3) return;
    ink_jw_t *w = &q->writer;
    ink_jw_raw(w, q->shown++ ? ",{" : "{");
    ink_jw_raw(w, "\"id\":"); ink_jw_str(w, id);
    ink_jw_raw(w, ",\"title\":"); ink_jw_strn(w, title, ink_text_byte_offset(title, 40));
    ink_jw_raw(w, ",\"value\":"); ink_jw_str(w, value);
    ink_jw_raw(w, ",\"kind\":"); ink_jw_int(w, kind);
    ink_jw_raw(w, ",\"type\":"); ink_jw_str(w, TYPES[kind]);
    ink_jw_raw(w, ",\"revision\":"); ink_jw_int(w, revision); ink_jw_raw(w, "}");
}
static void query(ink_call_t *c) {
    query_t *q = osal_calloc(1, sizeof *q, OSAL_MEM_EXTERNAL);
    char *out = osal_malloc(4096, OSAL_MEM_EXTERNAL);
    if (!q || !out) { c->rc.status = INK_RC_FAILED; goto done; }
    ink_args_get_str(&c->args, "query", q->query, sizeof q->query);
    snprintf(q->member, sizeof q->member, "%s", ink_cap_call_member(c));
    int64_t cursor = 0, version = 0;
    ink_args_get_int(&c->args, "cursor", &cursor); ink_args_get_int(&c->args, "version", &version);
    q->cursor = (size_t)cursor;
    ink_jw_init(&q->writer, out, 4096); ink_jw_raw(&q->writer, "{\"member\":"); ink_jw_str(&q->writer, q->member);
    ink_jw_raw(&q->writer, ",\"version\":"); ink_jw_int(&q->writer, version); ink_jw_raw(&q->writer, ",\"results\":[");
    for (int i = 0; i < ink_app_count(); i++) {
        const ink_app_def_t *a = ink_app_at(i);
        if (ink_app_visible(a) && strcmp(a->id, "home")) match(q, 0, a->id, a->title, "应用", 1);
    }
    if (ink_member_current()->role != INK_ROLE_CHILD) {
        match(q, 0, "settings", "设置", "应用", 1);
        for (int i = 0; ink_setting_at(i); i++) {
            const ink_setting_def_t *d = ink_setting_at(i);
            char id[64]; snprintf(id, sizeof id, "setting:%s", d->id);
            match(q, 1, id, d->title, "设置", 1);
        }
    }
    const char *const collections[] = {"phone.contacts", "memo.notes"};
    for (unsigned i = 0; i < 2; i++) {
        ink_db_t *db = ink_db_open_member(ink_db_find_def(collections[i]), q->member);
        if (!db) { c->rc.status = INK_RC_FAILED; break; }
        ink_db_cursor_t cur = {0}; ink_rec_t rec;
        while (ink_db_next(db, &cur, &rec, q->raw, sizeof q->raw - 1) == 1) {
            q->raw[rec.len] = 0;
            if (ink_json_parse(&q->json, q->raw, rec.len) != 0 ||
                (i && ink_json_eq(&q->json, ink_json_find(&q->json, 0, "deleted"), "true"))) continue;
            if (ink_json_str(&q->json, ink_json_find(&q->json, 0, i ? "body" : "name"), q->text, sizeof q->text) < 0) continue;
            char id[64], value[128] = "";
            snprintf(id, sizeof id, "%s:%u", i ? "memo" : "contact", (unsigned)rec.id);
            if (!i) ink_json_str(&q->json, ink_json_find(&q->json, 0, "number"), value, sizeof value);
            match(q, i + 2, id, q->text, value, rec.ver);
        }
        ink_db_close(db);
    }
    for (unsigned root = 0; root < 2; root++) {
        uint32_t at = 0; ink_fs_entry_t e; int r;
        while ((r = ink_fs_index(root ? "/family" : "/me", INK_FS_ANY, &at, &e)) == 1) {
            char id[64]; snprintf(id, sizeof id, "file:%u", (unsigned)e.id);
            match(q, 4, id, strrchr(e.path, '/') + 1, e.path, 1);
        }
        if (r < 0 && r != INK_FS_ERR_NOT_FOUND) c->rc.status = INK_RC_FAILED;
    }
    if (strcmp(q->member, ink_member_current()->id) || !ink_member_space_open()) c->rc.status = INK_RC_STALE;
    ink_jw_raw(&q->writer, "],\"total\":"); ink_jw_int(&q->writer, q->total);
    ink_jw_raw(&q->writer, ",\"cursor\":"); ink_jw_int(&q->writer, cursor);
    ink_jw_raw(&q->writer, ",\"next_cursor\":");
    ink_jw_int(&q->writer, q->cursor + q->shown < q->total ? (int64_t)(q->cursor + q->shown) : -1);
    ink_jw_raw(&q->writer, "}");
    if (q->writer.overflow) c->rc.status = INK_RC_FAILED;
    if (c->rc.status == INK_RC_DONE) ink_cap_result(c, out);
done:
    osal_free(out); osal_free(q);
}
static const ink_ui_action_spec_t ACTIONS[] = {
    {.id = "prev", .label = "上一页", .capability = "search.prev"},
    {.id = "next", .label = "下一页", .capability = "search.next"},
    {.id = "query", .label = "输入关键词", .capability = "search.focus", .enabled = true}
};
static void bind_item(lv_obj_t *slot, size_t index, ink_ui_field_spec_t *field, ink_ui_action_spec_t *action, void *ctx) {
    unsigned at = index % 3; result_t *r = &S.items[at];
    int x = INK_GAP + INK_CIRCLE, w = INK_CANVAS_W - 2 * INK_MARGIN - x - INK_GAP;
    if (!lv_obj_get_child_count(slot)) {
        ink_ui_icon(slot, ICONS[r->kind], INK_ICON_LIST, INK_ICON_LINE, INK_GAP, INK_GAP_L, false);
        ink_ui_line(slot, "", INK_FONT_LIST, INK_WEIGHT_MEDIUM, x, INK_GAP_S, w);
        ink_ui_line(slot, "", INK_FONT_NOTE, INK_WEIGHT_REGULAR, x, INK_LINE_LIST + INK_GAP_S, w);
    }
    lv_label_set_text(lv_obj_get_child(slot, 1), r->title);
    lv_label_set_text(lv_obj_get_child(slot, 2), TYPES[r->kind]);
    snprintf(S.args[at], sizeof S.args[at], "{\"id\":\"%s\",\"version\":%u}", r->id, (unsigned)S.version);
    *field = (ink_ui_field_spec_t){.id = r->id, .label = r->title, .value = TYPES[r->kind], .role = "listitem", .object = {r->id, r->revision}};
    *action = (ink_ui_action_spec_t){.id = r->id, .label = "打开", .capability = "search.open", .args_json = S.args[at], .object = field->object, .enabled = !S.pending};
}
static void loaded(void *ctx, const ink_receipt_t *rc) {
    if ((uint32_t)(uintptr_t)ctx != S.version || !visible()) return;
    S.pending = false; S.total = 0;
    ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL);
    if (rc->status == INK_RC_DONE && j && rc->result && ink_json_parse(j, rc->result, strlen(rc->result)) == 0) {
        int64_t total = 0; ink_json_int(j, ink_json_find(j, 0, "total"), &total); S.total = (size_t)total;
        int array = ink_json_find(j, 0, "results"), at = 0;
        memset(S.items, 0, sizeof S.items);
        for (int t = array + 1; array >= 0 && t < j->ntok && j->tok[t].end < j->tok[array].end && at < 3; t++) {
            if (j->tok[t].type != 1) continue;
            result_t *r = &S.items[at++]; int64_t n = 0;
            ink_json_str(j, ink_json_find(j, t, "id"), r->id, sizeof r->id);
            ink_json_str(j, ink_json_find(j, t, "title"), r->title, sizeof r->title);
            ink_json_str(j, ink_json_find(j, t, "value"), r->value, sizeof r->value);
            ink_json_int(j, ink_json_find(j, t, "kind"), &n); r->kind = (unsigned)n;
            ink_json_int(j, ink_json_find(j, t, "revision"), &n); r->revision = (uint32_t)n;
        }
    } else snprintf(S.error, sizeof S.error, "暂时搜不了，请稍后再试");
    osal_free(j);
    const ink_activity_t *page = !S.total && !S.error[0] ? &EMPTY : &SEARCH;
    if (strcmp(ink_ui_page_name(), page->name)) { ink_ui_pop(); ink_ui_push(page, NULL); }
    else ink_ui_redraw();
}
static void request(void) {
    S.pending = true; S.error[0] = 0; S.version++;
    ink_args_t *a = osal_malloc(sizeof *a, OSAL_MEM_EXTERNAL);
    if (a) {
        ink_args_init(a); ink_args_str(a, "query", S.query);
        ink_args_int(a, "cursor", S.position.page * 3); ink_args_int(a, "version", S.version);
        ink_call_opts_t o = {.src = INK_SRC_SYSTEM, .member = S.member, .reply_exec = INK_EXEC_UI, .done = loaded, .ctx = (void *)(uintptr_t)S.version};
        if (ink_cap_call("search.query", a, &o) != 0) { S.pending = false; snprintf(S.error, sizeof S.error, "暂时搜不了，请稍后再试"); }
        osal_free(a);
    } else S.pending = false;
}
void ink_search_open(const char *value) {
    char copy[193]; snprintf(copy, sizeof copy, "%s", value);
    bool same = visible();
    ink_text_blur(0);
    snprintf(S.query, sizeof S.query, "%s", copy); snprintf(S.member, sizeof S.member, "%s", ink_member_current()->id);
    S.epoch = ink_member_epoch(); S.total = 0; memset(&S.position, 0, sizeof S.position);
    request();
    if (same) ink_ui_redraw(); else ink_ui_push(&SEARCH, NULL);
}
static void entered(void *ctx, const char *value) {
    ink_args_t *a = osal_malloc(sizeof *a, OSAL_MEM_EXTERNAL); ink_receipt_t rc;
    if (!a) return;
    ink_args_init(a); ink_args_str(a, "query", value); ink_cap_invoke("search.show", a, INK_SRC_TOUCH, 0, &rc); osal_free(a);
}
static void create(lv_obj_t *screen, void *arg) {
    ink_shell_statusbar(screen, true);
    ink_shell_bind(ink_ui_circle(screen, "back", INK_MARGIN, INK_STATUS_BAR, INK_CONTROL, INK_ICON_LIST, INK_SURFACE_STATIC, NULL, NULL), "shell.back", NULL);
    int x = INK_MARGIN + INK_CONTROL + INK_GAP_M, y = INK_STATUS_BAR + INK_CONTROL + INK_GAP_M;
    S.model = (ink_ui_page_spec_t){.id = ink_ui_page_name(), .title = "搜索", .object = {"search", S.version}, .fields = &S.spec, .field_count = 1, .actions = ACTIONS + 2, .action_count = 1};
    ink_text_cfg_t cfg = {.type = INK_TEXT_PLAIN, .enter = INK_ENTER_SEARCH, .max = 192, .initial = S.query, .hint = "搜索"};
    S.field = ink_textfield_create(screen, &cfg, entered, NULL, false);
    lv_obj_set_pos(S.field, ink_ui_px(x), ink_ui_px(INK_STATUS_BAR)); lv_obj_set_width(S.field, ink_ui_px(INK_CANVAS_W - INK_MARGIN - x));
    S.spec = (ink_ui_field_spec_t){.id = "query", .label = "关键词", .role = "searchbox"};
    ink_textfield_bind_short(S.field, &S.model, &S.spec, ACTIONS + 2);
    if (!S.total) {
        int middle = (INK_CANVAS_H - INK_CIRCLE_LARGE) / 2;
        ink_ui_circle(screen, "search", (INK_CANVAS_W - INK_CIRCLE_LARGE) / 2, middle, INK_CIRCLE_LARGE, INK_ICON_STATE, INK_SURFACE_STATIC, NULL, NULL);
        lv_obj_t *label = ink_ui_label(screen, S.pending ? "正在搜索" : S.error[0] ? S.error : "没有找到相关内容", INK_FONT_SUBTITLE, INK_WEIGHT_BOLD, INK_MARGIN, middle + INK_CIRCLE_LARGE + INK_GAP_XL, INK_CANVAS_W - 2 * INK_MARGIN);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
        return;
    }
    S.model.fields = NULL; S.model.field_count = 0; S.model.actions = ACTIONS; S.model.action_count = 3;
    lv_obj_t *body = ink_ui_box(screen, 0, y, INK_CANVAS_W, 3 * (INK_ROW_DOUBLE + INK_GAP_M) + INK_CONTROL + INK_GAP_XL, 0, INK_SURFACE_WHITE, false);
    S.list = ink_ui_list_create(body, &S.model, &S.position, 3, 1, INK_ROW_DOUBLE, "", bind_item, NULL);
    if (S.list) ink_ui_list_refresh(S.list, S.total);
}
static void destroy(void) { S.field = NULL; S.list = NULL; }
static int snapshot(char *out, size_t cap) {
    if (!visible()) { if (cap) out[0] = 0; return -1; }
    int n = S.list ? ink_ui_list_snapshot(S.list, out, cap) : ink_textfield_short_snapshot(S.field, out, cap);
    if (n < 1) return -1;
    ink_jw_t w = {.buf = out, .cap = cap, .len = (size_t)n - 1};
    ink_jw_raw(&w, ",\"query\":"); ink_jw_str(&w, S.query);
    ink_jw_raw(&w, ",\"query_version\":"); ink_jw_int(&w, S.version);
    ink_jw_raw(&w, ",\"pending\":"); ink_jw_raw(&w, S.pending ? "true}" : "false}");
    if (w.overflow) { out[0] = 0; return -1; }
    return (int)w.len;
}
static bool page_key(ink_key_t k, bool lng) { return S.list && ink_ui_list_key(S.list, k, lng); }
static const ink_activity_t SEARCH = {.name = "Search", .create = create, .destroy = destroy, .snapshot = snapshot, .on_key = page_key};
static const ink_activity_t EMPTY = {.name = "SearchEmpty", .create = create, .destroy = destroy, .snapshot = snapshot, .on_key = page_key};
static void file_create(lv_obj_t *screen, void *arg) {
    ink_shell_statusbar(screen, true); ink_shell_topbar(screen, "文件信息");
    ink_ui_label(screen, S.file.title, INK_FONT_SUBTITLE, INK_WEIGHT_BOLD, INK_MARGIN, INK_STATUS_BAR + INK_PRIMARY + INK_GAP_L, INK_CANVAS_W - 2 * INK_MARGIN);
    ink_ui_label(screen, S.file.value, INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_MARGIN, INK_STATUS_BAR + INK_PRIMARY + INK_ROW_DOUBLE, INK_CANVAS_W - 2 * INK_MARGIN);
}
static int file_snapshot(char *out, size_t cap) {
    ink_ui_field_spec_t field = {.id = "file", .role = "file", .label = S.file.title, .value = S.file.value, .object = {S.file.id, S.file.revision}};
    ink_ui_page_spec_t model = {.id = "SearchFile", .title = "文件信息", .fields = &field, .field_count = 1};
    return ink_ui_semantics_snapshot(&model, out, cap);
}
static const ink_activity_t FILE_INFO = {.name = "SearchFile", .create = file_create, .snapshot = file_snapshot};
static void navigate(ink_call_t *c) {
    if (!strcmp(c->cap->id, "search.show")) { char query[193]; ink_args_get_str(&c->args, "query", query, sizeof query); ink_search_open(query); return; }
    if (!visible()) { c->rc.status = INK_RC_STALE; return; }
    if (!strcmp(c->cap->id, "search.focus")) { if (ink_textfield_short_focus(S.field)) c->rc.status = INK_RC_FAILED; return; }
    if (S.pending) { c->rc.status = INK_RC_STALE; return; }
    if (strcmp(c->cap->id, "search.open")) {
        int delta = !strcmp(c->cap->id, "search.next") ? 1 : -1;
        if ((delta < 0 && !S.position.page) || (delta > 0 && (S.position.page + 1) * 3 >= S.total)) return;
        S.position.page = (size_t)((int)S.position.page + delta); request(); return;
    }
    char id[64]; int64_t version = 0; ink_args_get_str(&c->args, "id", id, sizeof id); ink_args_get_int(&c->args, "version", &version);
    if (version != S.version) { c->rc.status = INK_RC_STALE; return; }
    for (unsigned i = 0; i < 3; i++) if (S.items[i].id[0] && !strcmp(S.items[i].id, id)) {
        result_t r = S.items[i]; snprintf(S.position.selected, sizeof S.position.selected, "%s", id);
        if (r.kind == 4) { S.file = r; ink_ui_push(&FILE_INFO, NULL); return; }
        char args[128]; const char *capability = r.kind == 2 ? "contacts.edit" : r.kind == 3 ? "memo.open" : "shell.launch";
        if (r.kind == 1) { ink_shell_open_page(strstr(id, "audio.") ? "SetSound" : strstr(id, "display.") || strstr(id, "light.") ? "SetDisplay" : "Settings"); return; }
        if (r.kind >= 2) snprintf(args, sizeof args, "{\"id\":%s}", strchr(id, ':') + 1);
        else snprintf(args, sizeof args, "{\"id\":\"%s\"}", id);
        ink_cap_invoke_json(capability, args, c->src, 0, &c->rc); return;
    }
    c->rc.status = INK_RC_STALE;
}
#define QUERY "\"query\":{\"type\":\"string\",\"maxLength\":64}"
#define NAV(id_, title_) {.id = id_, .title = title_, .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate}
int ink_search_init(void) {
    static const ink_cap_t caps[] = {
        {.id = "search.query", .title = "分页搜索当前家人的应用、设置、联系人、备忘和文件", .input = "{\"type\":\"object\",\"properties\":{" QUERY ",\"cursor\":{\"type\":\"integer\",\"minimum\":0},\"version\":{\"type\":\"integer\",\"minimum\":0}},\"required\":[\"query\"]}", .output = "{\"type\":\"object\"}", .effect = INK_FX_READ, .child_ok = true, .exec = INK_EXEC_BG, .run = query},
        {.id = "search.show", .title = "打开全局搜索", .input = "{\"type\":\"object\",\"properties\":{" QUERY "},\"required\":[\"query\"]}", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
        {.id = "search.open", .title = "打开当前页搜索结果", .input = "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\"},\"version\":{\"type\":\"integer\"}},\"required\":[\"id\",\"version\"]}", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
        NAV("search.focus", "输入搜索关键词"), NAV("search.next", "搜索下一页"), NAV("search.prev", "搜索上一页")};
    return ink_cap_register(caps, sizeof caps / sizeof *caps);
}
