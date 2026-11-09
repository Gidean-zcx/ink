/* File pages use the existing bus, member permissions and F4 list. */
#include <stdio.h>
#include <string.h>
#include "ink/sys/app.h"
#include "ink/sys/appreg.h"
#include "ink/sys/file.h"
#include "ink/sys/image.h"
#include "ink/sys/json.h"
#include "ink/sys/members.h"
#include "ink/sys/textfield.h"
#include "ink/sys/ui_list.h"
#include "ink/ui_shell.h"

enum { HOME, LIST, ACTIONS, MOVE };
typedef struct { char path[INK_FS_PATH_MAX], name[INK_FS_NAME_MAX]; uint32_t id; int64_t size; bool dir; ink_fs_type_t type; } item_t;
static const ink_activity_t PAGES[4];
static struct {
    item_t items[INK_FS_INDEX_MAX], selected;
    ink_ui_list_t *list;
    ink_ui_list_position_t position;
    ink_ui_page_spec_t model;
    ink_ui_action_spec_t actions[6];
    ink_ui_field_spec_t fields[3];
    lv_obj_t *screen, *notice, *field, *preview, *images[4];
    lv_image_dsc_t thumbs[4];
    uint8_t pixels[4][INK_ICON_LIST * INK_ICON_LIST];
    char member[16], path[INK_FS_PATH_MAX], dest[INK_FS_PATH_MAX], cursor[16], preview_text[129];
    char ids[4][32], args[4][384], names[4][INK_FS_NAME_MAX], values[4][40];
    char rename[576], target[576], source[288], object[32];
    uint32_t epoch, serial, selected_id, index_cursor;
    size_t count;
    int mode;
    bool busy, indexing;
} F INK_PSRAM_BSS;

static const char *directory(void) { return F.mode == MOVE ? F.dest : F.path; }
static bool writable(const char *path) { return ink_member_current()->role == INK_ROLE_ADULT || (!strncmp(path, "/me", 3) && (!path[3] || path[3] == '/')); }
static bool join(char *out, size_t cap, const char *dir, const char *name) { return snprintf(out, cap, "%s/%s", dir, name) < (int)cap; }
static void status(const char *text) { if (F.notice) lv_label_set_text(F.notice, text); ink_ui_content_changed(); }
static bool current(void *ctx) { return (uint32_t)((uintptr_t)ctx >> 3) == F.serial && ink_ui_page_current() && F.epoch == ink_member_epoch(); }
static void request(const char *cap, const ink_args_t *args, ink_cap_done_fn done, unsigned slot) {
    ink_call_opts_t opts = {.src = INK_SRC_APP, .member = F.member, .reply_exec = INK_EXEC_UI, .done = done, .ctx = (void *)(((uintptr_t)F.serial << 3) | slot)};
    if (ink_cap_call(cap, args, &opts)) { F.busy = false; status("暂时读不了文件，请返回后重试"); }
}
static void operation_done(void *ctx, const ink_receipt_t *rc) {
    if (!current(ctx)) return;
    status(rc->say[0] ? rc->say : rc->status == INK_RC_DONE ? "已完成" : rc->status == INK_RC_PENDING_CONFIRMATION ? "请在设备上确认" : "没能完成，请检查文件后重试");
}
static void clicked(lv_event_t *e) {
    unsigned index = (unsigned)(uintptr_t)lv_event_get_user_data(e);
    const ink_ui_action_spec_t *a = &F.actions[index];
    if (!a->enabled || !ink_ui_page_current() || F.epoch != ink_member_epoch()) return;
    ink_call_opts_t opts = {.src = INK_SRC_TOUCH, .member = F.member, .args_json = a->args_json, .reply_exec = INK_EXEC_UI, .done = operation_done, .ctx = (void *)((uintptr_t)F.serial << 3)};
    if (ink_cap_call(a->capability, NULL, &opts)) status("没能完成，请重试");
}
static lv_obj_t *button(unsigned i, lv_obj_t *screen, const char *label, const char *cap, const char *args, int x, int y, int width, bool primary) {
    F.actions[i] = (ink_ui_action_spec_t){.id = cap, .label = label, .capability = cap, .args_json = args, .enabled = true};
    return ink_ui_pill(screen, label, x, y, width, INK_CONTROL, primary, clicked, (void *)(uintptr_t)i);
}
static void pair_json(char *out, size_t cap, const char *key, const char *value, const char *key2, const char *value2) {
    ink_jw_t w; ink_jw_init(&w, out, cap); ink_jw_raw(&w, "{"); ink_jw_str(&w, key); ink_jw_raw(&w, ":"); ink_jw_str(&w, value);
    if (key2) { ink_jw_raw(&w, ","); ink_jw_str(&w, key2); ink_jw_raw(&w, ":"); ink_jw_str(&w, value2); } ink_jw_raw(&w, "}");
    if (w.overflow) out[0] = 0;
}
static void renamed(lv_event_t *e) {
    (void)e; char path[INK_FS_PATH_MAX], parent[INK_FS_PATH_MAX];
    snprintf(parent, sizeof parent, "%s", F.selected.path); *strrchr(parent, '/') = 0;
    const char *name = ink_textfield_text(F.field);
    bool valid = name[0] && name[0] != '.' && !strchr(name, '/') && join(path, sizeof path, parent, name);
    pair_json(F.rename, sizeof F.rename, "from", F.selected.path, "to", valid ? path : "");
    F.actions[1].enabled = valid && writable(F.selected.path); ink_ui_content_changed();
}
static void opened(void *ctx, const ink_receipt_t *rc) {
    if (!current(ctx)) return;
    ink_json_t *j = ink_app_alloc(sizeof *j, true);
    if (j && rc->status == INK_RC_DONE && rc->result && !ink_json_parse(j, rc->result, strlen(rc->result))) {
        ink_json_str(j, ink_json_find(j, 0, "text"), F.preview_text, sizeof F.preview_text);
        if (F.preview) lv_label_set_text(F.preview, F.preview_text);
        status("仅预览文件开头");
    } else operation_done(ctx, rc);
    ink_app_free(j);
}
static void thumb_done(void *ctx, const ink_receipt_t *rc) {
    if (!current(ctx) || rc->status != INK_RC_DONE || !rc->result) return;
    unsigned at = (uintptr_t)ctx & 7; ink_json_t *j = ink_app_alloc(sizeof *j, true); int64_t w = 0, h = 0;
    if (j && !ink_json_parse(j, rc->result, strlen(rc->result))) {
        ink_json_int(j, ink_json_find(j, 0, "width"), &w); ink_json_int(j, ink_json_find(j, 0, "height"), &h);
        int t = ink_json_find(j, 0, "gray4");
        if (t >= 0 && w > 0 && h > 0 && w <= INK_THUMB_EDGE && h <= INK_THUMB_EDGE && j->tok[t].end - j->tok[t].start == w * h && F.images[at]) {
            const char *gray = j->text + j->tok[t].start;
            for (unsigned y = 0; y < INK_ICON_LIST; y++) for (unsigned x = 0; x < INK_ICON_LIST; x++) {
                char c = gray[(y * (unsigned)h / INK_ICON_LIST) * (unsigned)w + x * (unsigned)w / INK_ICON_LIST];
                F.pixels[at][y * INK_ICON_LIST + x] = (uint8_t)((c <= '9' ? c - '0' : c - 'a' + 10) * 17);
            }
            F.thumbs[at] = (lv_image_dsc_t){.header = {.magic = LV_IMAGE_HEADER_MAGIC, .cf = LV_COLOR_FORMAT_L8, .w = INK_ICON_LIST, .h = INK_ICON_LIST, .stride = INK_ICON_LIST}, .data_size = sizeof F.pixels[at], .data = F.pixels[at]};
            lv_image_set_src(F.images[at], &F.thumbs[at]); lv_obj_remove_flag(F.images[at], LV_OBJ_FLAG_HIDDEN); ink_ui_content_changed();
        }
    }
    ink_app_free(j);
}
static void thumbnail(unsigned at, uint32_t id) { ink_args_t a; ink_args_init(&a); ink_args_int(&a, "id", id); request("file.thumb", &a, thumb_done, at); }
static void bind_item(lv_obj_t *slot, size_t index, ink_ui_field_spec_t *field, ink_ui_action_spec_t *action, void *ctx) {
    (void)ctx; unsigned at = index % 4; item_t *item = &F.items[index];
    if (!lv_obj_get_child_count(slot)) {
        ink_ui_line(slot, "", INK_FONT_LIST, INK_WEIGHT_MEDIUM, INK_CIRCLE, INK_GAP_S, INK_CANVAS_W - 2 * INK_MARGIN - INK_CIRCLE - INK_GAP);
        ink_ui_line(slot, "", INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_CIRCLE, INK_GAP_S + ink_ui_line_height(INK_FONT_LIST), INK_CANVAS_W - 2 * INK_MARGIN - INK_CIRCLE - INK_GAP);
        static const char *const icons[] = {"folder", "book", "headphones", "camera"};
        for (unsigned i = 0; i < 4; i++) ink_ui_icon(slot, icons[i], INK_ICON_LIST, INK_ICON_LINE, INK_GAP, INK_GAP_L, false);
        lv_obj_t *im = lv_image_create(slot); lv_obj_set_pos(im, ink_ui_px(INK_GAP), ink_ui_px(INK_GAP_L));
    }
    unsigned icon = item->dir ? 0 : item->type == INK_FS_IMAGE ? 3 : item->type == INK_FS_AUDIO ? 2 : 1;
    for (unsigned i = 0; i < 4; i++) { lv_obj_t *obj = lv_obj_get_child(slot, (int)i + 2); if (i == icon) lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN); else lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN); }
    F.images[at] = lv_obj_get_child(slot, 6); lv_obj_add_flag(F.images[at], LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_bg_color(slot, ink_ui_gray(), 0); lv_obj_set_style_border_width(slot, 0, 0);
    char name[sizeof item->name]; /* a copy: gcc refuses snprintf from F into F */
    memcpy(name, item->name, sizeof name); name[sizeof name - 1] = 0;
    snprintf(F.names[at], sizeof F.names[at], "%s", name);
    const char *ext = strrchr(name, '.');
    if (ink_ui_text_width(F.names[at], INK_FONT_LIST, INK_WEIGHT_MEDIUM) > INK_CANVAS_W - 2 * INK_MARGIN - INK_CIRCLE - INK_GAP)
        snprintf(F.names[at], sizeof F.names[at], "%.*s…%s", (int)ink_text_byte_offset(name, 7), name, ext && strlen(ext) < 12 ? ext : "");
    lv_label_set_text(lv_obj_get_child(slot, 0), F.names[at]);
    if (item->dir) snprintf(F.values[at], sizeof F.values[at], "%s", strcmp(F.names[at], item->name) ? "文件夹（名称已截断）" : "文件夹"); else snprintf(F.values[at], sizeof F.values[at], "%lld 字节", (long long)item->size);
    lv_label_set_text(lv_obj_get_child(slot, 1), F.values[at]);
    if (item->id) snprintf(F.ids[at], sizeof F.ids[at], "file:%u", (unsigned)item->id); else snprintf(F.ids[at], sizeof F.ids[at], "dir:%zu", index);
    if (item->dir) pair_json(F.args[at], sizeof F.args[at], "path", item->path, NULL, NULL); else snprintf(F.args[at], sizeof F.args[at], "{\"id\":%u}", (unsigned)item->id);
    *field = (ink_ui_field_spec_t){.id = F.ids[at], .role = item->dir ? "directory" : "file", .label = item->dir ? F.names[at] : item->name, .value = F.values[at], .object = {F.ids[at], F.epoch}};
    *action = (ink_ui_action_spec_t){.id = F.ids[at], .label = item->dir ? "打开文件夹" : "文件操作", .capability = item->dir ? "files.browse" : "files.select", .args_json = F.args[at], .object = field->object, .enabled = !F.busy};
    if (item->type == INK_FS_IMAGE && item->id) thumbnail(at, item->id);
}
static void load(void);
static void listed(void *ctx, const ink_receipt_t *rc) {
    if (!current(ctx)) return;
    ink_json_t *j = ink_app_alloc(sizeof *j, true);
    bool ok = j && rc->status == INK_RC_DONE && rc->result && !ink_json_parse(j, rc->result, strlen(rc->result));
    if (ok) for (int t = 2; t < j->ntok; t++) {
        if (j->tok[t].type != 1) continue;
        item_t item = {0}; int64_t id = 0; char type[12] = "";
        item.dir = ink_json_eq(j, ink_json_find(j, t, "dir"), "true");
        if (!F.indexing && item.dir) {
            ink_json_str(j, ink_json_find(j, t, "name"), item.name, sizeof item.name);
            if (!join(item.path, sizeof item.path, directory(), item.name)) continue;
        } else if (F.indexing) {
            ink_json_str(j, ink_json_find(j, t, "path"), item.path, sizeof item.path);
            const char *name = strrchr(item.path, '/'); size_t len = strlen(directory());
            if (!name || (size_t)(name - item.path) != len || strncmp(item.path, directory(), len)) continue;
            snprintf(item.name, sizeof item.name, "%s", name + 1); ink_json_int(j, ink_json_find(j, t, "id"), &id); item.id = (uint32_t)id;
            ink_json_str(j, ink_json_find(j, t, "type"), type, sizeof type); item.type = !strcmp(type, "image") ? INK_FS_IMAGE : !strcmp(type, "audio") ? INK_FS_AUDIO : !strcmp(type, "text") ? INK_FS_TEXT : INK_FS_OTHER;
            ink_json_int(j, ink_json_find(j, t, "size"), &item.size);
        } else continue;
        if (F.count == INK_FS_INDEX_MAX) { ok = false; break; } F.items[F.count++] = item;
    }
    int next = ok ? ink_json_find(j, 0, "next") : -1;
    F.cursor[0] = 0; F.index_cursor = 0;
    if (next >= 0) { int64_t value = 0; if (F.indexing) { ink_json_int(j, next, &value); F.index_cursor = (uint32_t)value; } else ink_json_str(j, next, F.cursor, sizeof F.cursor); }
    ink_app_free(j);
    if (ok && next >= 0) { load(); return; }
    if (ok && !F.indexing && F.mode != MOVE) { F.indexing = true; load(); return; }
    F.busy = false; if (!ok) F.count = 0;
    status(ok ? "" : "暂时读不了这个文件夹，请返回后重试"); ink_ui_list_refresh(F.list, F.count);
}
static void load(void) {
    ink_args_t a; ink_args_init(&a); ink_args_str(&a, "path", directory()); ink_args_int(&a, "limit", 10);
    if (F.indexing) ink_args_int(&a, "cursor", F.index_cursor); else ink_args_str(&a, "cursor", F.cursor);
    request(F.indexing ? "file.index" : "file.list", &a, listed, 0);
}
static void selected(void *ctx, const ink_receipt_t *rc) {
    if (!current(ctx)) return;
    ink_json_t *j = ink_app_alloc(sizeof *j, true);
    if (!j || rc->status != INK_RC_DONE || !rc->result || ink_json_parse(j, rc->result, strlen(rc->result))) { ink_app_free(j); F.busy = false; status("找不到这个文件，请返回列表"); return; }
    int t = ink_json_find(j, 0, "entries") + 1; memset(&F.selected, 0, sizeof F.selected); F.selected.id = F.selected_id;
    ink_json_str(j, ink_json_find(j, t, "path"), F.selected.path, sizeof F.selected.path);
    const char *name = strrchr(F.selected.path, '/'); snprintf(F.selected.name, sizeof F.selected.name, "%s", name ? name + 1 : "");
    char type[12]; ink_json_str(j, ink_json_find(j, t, "type"), type, sizeof type); ink_app_free(j); F.busy = false;
    pair_json(F.source, sizeof F.source, "path", F.selected.path, NULL, NULL);
    snprintf(F.object, sizeof F.object, "file:%u", (unsigned)F.selected_id);
    F.fields[0] = (ink_ui_field_spec_t){.id = "name", .role = "textbox", .label = "文件名", .value = F.selected.name, .object = {F.object, F.epoch}};
    F.fields[1] = (ink_ui_field_spec_t){.id = "path", .role = "text", .label = "位置", .value = F.selected.path};
    F.fields[2] = (ink_ui_field_spec_t){.id = "preview", .role = "text", .label = "文件开头", .value = F.preview_text}; F.model.field_count = 3;
    int y = INK_STATUS_BAR + INK_PRIMARY + INK_GAP_L, width = INK_CANVAS_W - 2 * INK_MARGIN;
    ink_text_cfg_t cfg = {.type = INK_TEXT_PLAIN, .initial = F.selected.name, .max = INK_FS_NAME_MAX - 1};
    if (writable(F.selected.path)) {
        F.field = ink_textfield_create(F.screen, &cfg, NULL, NULL, false); lv_obj_set_pos(F.field, ink_ui_px(INK_MARGIN), ink_ui_px(y)); lv_obj_set_size(F.field, ink_ui_px(width), ink_ui_px(INK_CONTROL));
        F.actions[0] = (ink_ui_action_spec_t){.id = "name", .label = "编辑文件名", .capability = "files.name", .enabled = true};
        ink_textfield_bind_short(F.field, &F.model, &F.fields[0], &F.actions[0]); lv_obj_add_event_cb(F.field, renamed, LV_EVENT_VALUE_CHANGED, NULL);
        button(1, F.screen, "改名", "file.move", F.rename, INK_MARGIN, y + INK_CONTROL + INK_GAP_L, width, false); F.actions[1].id = "rename"; renamed(NULL);
    } else { F.fields[0].role = "text"; ink_ui_line(F.screen, F.selected.name, INK_FONT_LIST, INK_WEIGHT_MEDIUM, INK_MARGIN, y, width); }
    F.preview = ink_ui_label(F.screen, "", INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_MARGIN, y + 2 * (INK_CONTROL + INK_GAP_L), width);
    const char *open_cap = !strcmp(type, "text") ? "files.preview" : !strcmp(type, "audio") ? "media.play" : "photo.open";
    snprintf(F.args[0], sizeof F.args[0], "{\"id\":%u}", (unsigned)F.selected_id);
    lv_obj_t *open = button(2, F.screen, "打开", open_cap, !strcmp(type, "audio") ? F.source : F.args[0], INK_MARGIN, INK_CANVAS_H - INK_MARGIN - 2 * INK_PRIMARY - INK_GAP, width, false);
    F.actions[2].enabled = !strcmp(type, "text") || !strcmp(type, "audio") || !strcmp(type, "image");
    if (!strcmp(type, "image")) { F.images[0] = lv_image_create(F.screen); lv_obj_set_pos(F.images[0], ink_ui_px(INK_MARGIN), ink_ui_px(y + 2 * (INK_CONTROL + INK_GAP_L))); thumbnail(0, F.selected_id); }
    if (!F.actions[2].enabled) { lv_obj_add_flag(open, LV_OBJ_FLAG_HIDDEN); status("这种文件暂时打不开"); } else status("");
    if (writable(F.selected.path)) {
        button(3, F.screen, "移动", "files.move", NULL, INK_MARGIN, INK_CANVAS_H - INK_PRIMARY - INK_MARGIN, (width - INK_GAP) / 2, false);
        button(4, F.screen, "删除", "file.delete", F.source, INK_MARGIN + (width + INK_GAP) / 2, INK_CANVAS_H - INK_PRIMARY - INK_MARGIN, (width - INK_GAP) / 2, false);
    }
    F.model.object = (ink_ui_object_ref_t){F.object, F.epoch}; F.model.action_count = 5;
}
static void navigate(ink_call_t *c) {
    if (!ink_ui_page_current() || F.epoch != ink_member_epoch() || strcmp(ink_ui_action_page(), PAGES[F.mode].name)) { c->rc.status = INK_RC_STALE; return; }
    const char *id = c->cap->id;
    if (!strcmp(id, "files.name")) { if (!F.field || !writable(F.selected.path)) c->rc.status = INK_RC_REJECTED; else ink_textfield_short_focus(F.field); return; }
    if (F.busy) { c->rc.status = INK_RC_FAILED; return; }
    if (!strcmp(id, "files.next") || !strcmp(id, "files.prev")) { F.serial++; if (!F.list || !ink_ui_list_turn(F.list, !strcmp(id, "files.next") ? 1 : -1)) c->rc.status = INK_RC_INVALID; }
    else if (!strcmp(id, "files.browse")) {
        char path[INK_FS_PATH_MAX]; ink_args_get_str(&c->args, "path", path, sizeof path);
        bool allowed = (!strncmp(path, "/me", 3) && (!path[3] || path[3] == '/')) || (!strncmp(path, "/family", 7) && (!path[7] || path[7] == '/')) || (!strncmp(path, "/members/", 9) && ink_member_current()->role == INK_ROLE_ADULT);
        if (!allowed || (F.mode == MOVE && !writable(path))) { c->rc.status = INK_RC_REJECTED; return; }
        snprintf(F.mode == MOVE ? F.dest : F.path, sizeof F.path, "%s", path); memset(&F.position, 0, sizeof F.position);
        if (F.mode == HOME) ink_ui_push(&PAGES[LIST], NULL); else ink_ui_redraw();
    } else if (!strcmp(id, "files.select")) {
        int64_t value = 0; ink_args_get_int(&c->args, "id", &value); F.selected_id = (uint32_t)value; ink_ui_push(&PAGES[ACTIONS], NULL);
    } else if (!strcmp(id, "files.move")) { if (F.mode != ACTIONS || !F.selected.id || !writable(F.selected.path)) c->rc.status = INK_RC_REJECTED; else { F.dest[0] = 0; ink_ui_push(&PAGES[MOVE], NULL); } }
    else if (!strcmp(id, "files.preview")) {
        int64_t value = 0; ink_args_get_int(&c->args, "id", &value);
        if (F.mode != ACTIONS || value != F.selected_id || !F.selected.id) { c->rc.status = INK_RC_INVALID; return; }
        ink_args_t a; ink_args_init(&a); ink_args_str(&a, "path", F.selected.path); ink_args_int(&a, "max_bytes", sizeof F.preview_text - 1); request("file.read", &a, opened, 0);
    } else {
        char *path = F.mode == MOVE ? F.dest : F.path, *end = strrchr(path, '/');
        bool member_root = !strncmp(path, "/members/", strlen("/members/")) && !strchr(path + strlen("/members/"), '/');
        if (F.mode == ACTIONS || F.mode == HOME) ink_ui_pop();
        else if (end && end != path && !member_root) { *end = 0; memset(&F.position, 0, sizeof F.position); ink_ui_redraw(); }
        else if (F.mode == MOVE && path[0]) { path[0] = 0; ink_ui_redraw(); } else ink_ui_pop();
    }
}
static void create(lv_obj_t *screen, void *arg) {
    (void)arg; F.serial++; F.screen = screen; F.count = 0; F.indexing = false; F.cursor[0] = 0; F.index_cursor = 0; F.preview_text[0] = 0;
    F.mode = 0; while (F.mode < 3 && strcmp(PAGES[F.mode].name, ink_ui_page_name())) F.mode++;
    if (F.epoch != ink_member_epoch()) { memset(&F.position, 0, sizeof F.position); memset(&F.selected, 0, sizeof F.selected); F.path[0] = F.dest[0] = 0; F.selected_id = 0; F.epoch = ink_member_epoch(); }
    snprintf(F.member, sizeof F.member, "%s", ink_member_current()->id); if (F.mode == HOME) F.path[0] = 0;
    memset(F.actions, 0, sizeof F.actions); memset(F.fields, 0, sizeof F.fields);
    F.model = (ink_ui_page_spec_t){.id = PAGES[F.mode].name, .title = F.mode == MOVE ? "放到哪里？" : "文件", .object = {"files", F.epoch}, .fields = F.fields, .actions = F.actions};
    ink_shell_statusbar(screen, true);
    ink_shell_bind(ink_ui_circle(screen, "back", INK_MARGIN, INK_STATUS_BAR + INK_GAP_S, INK_CONTROL, INK_ICON_MEDIUM, INK_SURFACE_STATIC, NULL, NULL), "files.up", NULL);
    const char *title = directory()[0] && F.mode == LIST ? !strcmp(directory(), "/me") ? "我的" : !strcmp(directory(), "/family") ? "家庭" : strrchr(directory(), '/') + 1 : F.model.title;
    ink_ui_line(screen, title, INK_FONT_TITLE, INK_WEIGHT_BOLD, INK_MARGIN + INK_CONTROL + INK_GAP, INK_STATUS_BAR + INK_GAP_S, INK_CANVAS_W - 2 * INK_MARGIN - INK_CONTROL - INK_GAP);
    int notice_y = INK_CANVAS_H - INK_PRIMARY - INK_CONTROL - INK_GAP_L;
    if (F.mode == ACTIONS) notice_y = INK_CANVAS_H - INK_MARGIN - 2 * INK_PRIMARY - INK_GAP - 2 * ink_ui_line_height(INK_FONT_NOTE) - INK_GAP_L;
    F.notice = ink_ui_notice(screen, "", notice_y);
    F.busy = F.mode != HOME && directory()[0];
    if (F.mode == ACTIONS) { memset(&F.selected, 0, sizeof F.selected); F.busy = true; ink_args_t a; ink_args_init(&a); ink_args_int(&a, "id", F.selected_id); status("正在读取文件"); request("file.lookup", &a, selected, 0); return; }
    F.actions[0] = (ink_ui_action_spec_t){.id = "prev", .label = "上一页", .capability = "files.prev"};
    F.actions[1] = (ink_ui_action_spec_t){.id = "next", .label = "下一页", .capability = "files.next"}; F.model.action_count = 2;
    if (F.mode == MOVE) {
        if (directory()[0]) ink_ui_line(screen, directory(), INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_MARGIN, INK_STATUS_BAR + INK_PRIMARY + INK_GAP_S, INK_CANVAS_W - 2 * INK_MARGIN);
        char path[INK_FS_PATH_MAX]; bool place = F.selected.id && directory()[0] && join(path, sizeof path, directory(), F.selected.name) && writable(directory());
        pair_json(F.target, sizeof F.target, "from", F.selected.path, "to", place ? path : "");
        int width = (INK_CANVAS_W - 2 * INK_MARGIN - INK_GAP) / 2;
        button(3, screen, "取消", "shell.back", NULL, INK_MARGIN, INK_CANVAS_H - INK_PRIMARY - INK_MARGIN, width, false);
        button(2, screen, "放到这里", "file.move", F.target, INK_MARGIN + width + INK_GAP, INK_CANVAS_H - INK_PRIMARY - INK_MARGIN, width, true); F.actions[2].enabled = place; F.model.action_count = 3;
    }
    lv_obj_t *body = ink_ui_box(screen, 0, INK_STATUS_BAR + INK_PRIMARY + INK_GAP_L + (F.mode == MOVE ? INK_LINE_NOTE + INK_GAP_L : 0), INK_CANVAS_W, 4 * (INK_ROW_DOUBLE + INK_GAP_M) + INK_PRIMARY, 0, INK_SURFACE_WHITE, false);
    F.list = ink_ui_list_create(body, &F.model, &F.position, 4, 1, INK_ROW_DOUBLE, "这里还没有文件", bind_item, NULL);
    if (!F.list) { F.busy = false; status("暂时打不开文件列表"); return; }
    if (F.busy) { status("正在读取文件夹"); ink_ui_list_refresh(F.list, 0); load(); return; }
    if (F.mode != MOVE || writable("/family")) F.items[F.count++] = (item_t){.path = "/family", .name = "家庭", .dir = true};
    F.items[F.count++] = (item_t){.path = "/me", .name = "我的", .dir = true};
    if (ink_member_current()->role == INK_ROLE_ADULT) for (int i = 0; i < INK_MEMBERS_MAX; i++) {
        const ink_member_t *m = ink_member_at(i); if (!m) break; if (!strcmp(m->id, F.member)) continue;
        item_t *item = &F.items[F.count++]; memset(item, 0, sizeof *item); item->dir = true;
        snprintf(item->path, sizeof item->path, "/members/%s", m->id); snprintf(item->name, sizeof item->name, "%s的文件", m->name);
    }
    ink_ui_list_refresh(F.list, F.count);
}
static void destroy(void) { F.serial++; F.list = NULL; F.field = F.notice = F.preview = NULL; memset(F.images, 0, sizeof F.images); }
static int snapshot(char *out, size_t cap) {
    int n = F.list ? ink_ui_list_snapshot(F.list, out, cap) : ink_ui_semantics_snapshot(&F.model, out, cap);
    if (n < 0) return -1;
    ink_jw_t w = {.buf = out, .cap = cap, .len = (size_t)n - 1}; ink_jw_raw(&w, ",\"status\":"); ink_jw_str(&w, F.notice ? lv_label_get_text(F.notice) : "");
    ink_jw_raw(&w, ",\"directory\":"); ink_jw_str(&w, directory()); ink_jw_raw(&w, ",\"back\":\"files.up\"");
    if (F.mode == MOVE) ink_jw_raw(&w, ",\"cancel\":\"shell.back\"");
    ink_jw_raw(&w, "}");
    if (w.overflow) { out[0] = 0; return -1; }
    return (int)w.len;
}
static bool page_key(ink_key_t key, bool lng) { return F.list && ink_ui_list_key(F.list, key, lng); }
static void changed(void *ctx, const char *id, const ink_args_t *args) {
    (void)ctx; (void)id;
    if (!ink_ui_page_current() || F.epoch != ink_member_epoch() || strcmp(ink_ui_page_name(), PAGES[F.mode].name)) return;
    char path[INK_FS_PATH_MAX], kind[16]; ink_args_get_str(args, "path", path, sizeof path); ink_args_get_str(args, "kind", kind, sizeof kind);
    if ((F.mode == MOVE && !strcmp(kind, "moved")) || (F.mode == ACTIONS && !strcmp(kind, "removed") && !strcmp(path, F.selected.path))) ink_ui_pop(); else ink_ui_redraw();
}
#define PAGE(name_) {.name = name_, .app = "files", .create = create, .destroy = destroy, .snapshot = snapshot, .on_key = page_key}
static const ink_activity_t PAGES[] = {PAGE("Files"), PAGE("FileList"), PAGE("FileActions"), PAGE("FileMove")};
static const char *const PERMISSIONS[] = {"file", "media", NULL};
static const ink_app_def_t APP = {.id = "files", .title = "文件", .icon = INK_UI_ICON(folder), .entry = &PAGES[HOME], .system_app = true, .child_ok = true, .permissions = PERMISSIONS};
#define ID_INPUT "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"integer\",\"minimum\":1,\"maximum\":4294967295}},\"required\":[\"id\"]}"
static const ink_cap_t CAPS[] = {
    {.id = "files.browse", .title = "浏览文件夹", .input = "{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\",\"maxLength\":127}},\"required\":[\"path\"]}", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "files.select", .title = "按编号查看文件操作", .input = ID_INPUT, .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "files.move", .title = "选移动目标文件夹", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "files.name", .title = "编辑文件名", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "files.preview", .title = "预览当前文本文件开头", .input = ID_INPUT, .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "files.next", .title = "文件下一页", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "files.prev", .title = "文件上一页", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "files.up", .title = "返回文件上一级", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate}
};
int ink_files_init(void) { return ink_cap_register(CAPS, sizeof CAPS / sizeof *CAPS) || ink_app_register(&APP) || ink_cap_subscribe("file.changed", changed, NULL, INK_EXEC_UI) < 0 ? -1 : 0; }
