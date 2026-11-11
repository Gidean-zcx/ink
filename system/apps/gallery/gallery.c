/* F4 grid, media thumbnails and a member-scoped photo viewer. */
#include "ink/osal.h"
#include "ink/sys/appreg.h"
#include "ink/sys/i18n.h"
#include "ink/sys/file.h"
#include "ink/sys/json.h"
#include "ink/sys/members.h"
#include "ink/sys/ai.h"
#include "ink/sys/ui_list.h"
#include "ink/ui_shell.h"
#include "photo.h"
#include <stdio.h>
#include <string.h>

typedef struct { lv_image_dsc_t dsc; ink_image_t im; } picture_t;
static void picture_deleted(lv_event_t *e) {
    picture_t *p = lv_event_get_user_data(e); ink_image_free(&p->im); osal_free(p);
}
void ink_photo_image(lv_obj_t *parent, ink_image_t *im) {
    picture_t *p = osal_calloc(1, sizeof *p, OSAL_MEM_EXTERNAL);
    if (!p) return;
    p->im = *im; *im = (ink_image_t){0};
    p->dsc = (lv_image_dsc_t){.header = {.magic = LV_IMAGE_HEADER_MAGIC, .cf = LV_COLOR_FORMAT_L8,
        .w = p->im.width, .h = p->im.height, .stride = p->im.width}, .data_size = p->im.width * p->im.height, .data = p->im.pixels};
    lv_obj_t *obj = lv_image_create(parent); lv_image_set_src(obj, &p->dsc);
    lv_obj_update_layout(parent);
    unsigned sx = (unsigned)lv_obj_get_content_width(parent) * LV_SCALE_NONE / p->im.width;
    unsigned sy = (unsigned)lv_obj_get_content_height(parent) * LV_SCALE_NONE / p->im.height;
    lv_image_set_scale(obj, sx < sy ? sx : sy); lv_image_set_antialias(obj, false); lv_obj_center(obj);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE); lv_obj_add_event_cb(obj, picture_deleted, LV_EVENT_DELETE, p);
}
static const ink_activity_t GALLERY, PHOTO;
enum { PHOTO_SLOTS = 9, PHOTO_COLUMNS = 3 };
typedef struct { uint32_t epoch, generation, photo, revision; } photo_question_t;
static struct {
    ink_ui_list_t *list; ink_ui_list_position_t position;
    uint32_t ids[INK_FS_INDEX_MAX], epoch, photo, revision;
    size_t count; lv_obj_t *slots[PHOTO_SLOTS], *box, *status, *trash, *name;
    char objects[PHOTO_SLOTS][32], args[PHOTO_SLOTS][48], path[INK_FS_PATH_MAX], delete_args[2 * INK_FS_PATH_MAX + 32], message[96], caption[INK_FS_PATH_MAX + 64];
    ink_ui_action_spec_t photo_actions[3];
} G INK_PSRAM_BSS;
typedef struct { ink_exec_job_t job; uint32_t epoch, generation, id, revision, ids[INK_FS_INDEX_MAX]; size_t count; int err; ink_image_t im; char path[INK_FS_PATH_MAX]; } load_t;
typedef struct { uint32_t epoch, generation, id; unsigned slot; } thumb_t;
static void thumb_ready(void *ctx, const ink_receipt_t *rc) {
    thumb_t *t = ctx;
    if (t->epoch == ink_member_epoch() && t->generation == ink_ui_generation() && G.list && G.slots[t->slot] &&
        G.ids[G.position.page * PHOTO_SLOTS + t->slot] == t->id && rc->status == INK_RC_DONE && rc->result) {
        ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL); int64_t w = 0, h = 0;
        if (j && !ink_json_parse(j, rc->result, strlen(rc->result))) {
            ink_json_int(j, ink_json_find(j, 0, "width"), &w); ink_json_int(j, ink_json_find(j, 0, "height"), &h);
            int tok = ink_json_find(j, 0, "gray4");
            if (tok >= 0 && w > 0 && h > 0 && w <= INK_THUMB_EDGE && h <= INK_THUMB_EDGE && j->tok[tok].end - j->tok[tok].start == w * h) {
                ink_image_t im = {(unsigned)w, (unsigned)h, osal_malloc((size_t)w * h, OSAL_MEM_EXTERNAL)};
                if (im.pixels) {
                    const char *s = rc->result + j->tok[tok].start;
                    for (int64_t i = 0; i < w * h; i++) im.pixels[i] = (uint8_t)((s[i] <= '9' ? s[i] - '0' : s[i] - 'a' + 10) * 17);
                    lv_obj_clean(G.slots[t->slot]); ink_photo_image(G.slots[t->slot], &im); ink_image_free(&im);
                    ink_ui_hint_obj(G.slots[t->slot], INK_REFRESH_UI);
                }
            }
        }
        osal_free(j);
    }
    osal_free(t);
}
static void bind_photo(lv_obj_t *slot, size_t index, ink_ui_field_spec_t *field, ink_ui_action_spec_t *action, void *ctx) {
    (void)ctx; unsigned at = index % PHOTO_SLOTS; G.slots[at] = slot; lv_obj_clean(slot);
    lv_obj_set_style_outline_width(slot, ink_ui_px(INK_STROKE), LV_STATE_PRESSED);
    lv_obj_set_style_outline_color(slot, ink_ui_ink(), LV_STATE_PRESSED);
    snprintf(G.objects[at], sizeof G.objects[at], "file:%u", (unsigned)G.ids[index]);
    snprintf(G.args[at], sizeof G.args[at], "{\"id\":%u}", (unsigned)G.ids[index]);
    *field = (ink_ui_field_spec_t){.id = G.objects[at], .role = "image", .label = TR("照片"), .value = G.objects[at], .object = {G.objects[at], 1}};
    *action = (ink_ui_action_spec_t){.id = G.objects[at], .label = TR("查看照片"), .capability = "photo.open", .args_json = G.args[at], .object = field->object, .enabled = true};
    thumb_t *t = osal_malloc(sizeof *t, OSAL_MEM_EXTERNAL);
    if (t) {
        *t = (thumb_t){G.epoch, ink_ui_generation(), G.ids[index], at};
        ink_call_opts_t o = {.src = INK_SRC_SYSTEM, .args_json = G.args[at], .reply_exec = INK_EXEC_UI, .done = thumb_ready, .ctx = t};
        if (ink_cap_call("file.thumb", NULL, &o)) osal_free(t);
    }
}
static void loaded(void *arg) {
    load_t *p = arg;
    if (p->epoch == ink_member_epoch() && p->generation == ink_ui_generation() && ink_ui_page_current()) {
        snprintf(G.message, sizeof G.message, "%s", p->err ? TR("暂时读不了照片，请点重读照片") : "");
        if (!p->id && G.list) {
            G.count = p->count; memcpy(G.ids, p->ids, p->count * sizeof *p->ids); ink_ui_list_refresh(G.list, p->err ? 0 : p->count);
            if (p->err) lv_obj_add_flag(lv_obj_get_child(G.box, 0), LV_OBJ_FLAG_HIDDEN);
            if (p->count <= PHOTO_SLOTS || p->err) {
                lv_obj_t *bar = lv_obj_get_child(G.box, -1);
                lv_obj_add_flag(lv_obj_get_child(bar, 0), LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(lv_obj_get_child(bar, 2), LV_OBJ_FLAG_HIDDEN);
            }
        } else if (p->id == G.photo && G.box && !p->err) {
            G.revision = p->revision;
            snprintf(G.path, sizeof G.path, "%s", p->path); ink_photo_image(G.box, &p->im);
            size_t at = 0; while (at < G.count && G.ids[at] != p->id) at++;
            const char *name = strrchr(p->path, '/'); name = name ? name + 1 : p->path;
            if (at < G.count) snprintf(G.caption, sizeof G.caption, TR("%s · 第%zu/%zu张"), name, at + 1, G.count);
            else snprintf(G.caption, sizeof G.caption, "%s", name);
            lv_label_set_text(G.name, G.caption);
            G.photo_actions[1].enabled = true;
            ink_jw_t w; ink_jw_init(&w, G.delete_args, sizeof G.delete_args); ink_jw_raw(&w, "{\"path\":"); ink_jw_str(&w, p->path); ink_jw_raw(&w, "}");
            G.photo_actions[0].enabled = true; G.photo_actions[0].args_json = G.delete_args;
            ink_shell_bind(G.trash, "file.delete", G.delete_args); lv_obj_remove_flag(G.trash, LV_OBJ_FLAG_HIDDEN);
        }
        if (G.status) lv_label_set_text(G.status, G.message);
        ink_ui_content_changed(); ink_ui_hint_frame(INK_REFRESH_UI);
    }
    ink_image_free(&p->im); osal_free(p);
}
static void read_photos(void *arg) {
    load_t *p = arg; ink_fs_entry_t e;
    if (p->epoch != ink_member_epoch()) p->err = INK_FS_ERR_DENIED;
    else if (!p->id) {
        uint32_t cursor = 0; int r;
        while ((r = ink_fs_index("/me", INK_FS_IMAGE, &cursor, &e)) == 1 && p->count < INK_FS_INDEX_MAX && p->epoch == ink_member_epoch()) p->ids[p->count++] = e.id;
        if (r < 0 && r != INK_FS_ERR_NOT_FOUND) p->err = r;
        cursor = 0;
        while (p->count < INK_FS_INDEX_MAX && p->epoch == ink_member_epoch() && ink_fs_index("/family/锁屏拍的照片", INK_FS_IMAGE, &cursor, &e) == 1) p->ids[p->count++] = e.id;
    } else {
        p->revision = ink_fs_revision();
        p->err = ink_fs_lookup(p->id, &e);
        ink_fs_file_t *f = NULL;
        void *data = !p->err && e.size > 0 && e.size <= INK_IMAGE_MAX_INPUT ? osal_malloc((size_t)e.size, OSAL_MEM_EXTERNAL) : NULL;
        if (!data || ink_fs_open(e.path, INK_FS_READ, &f)) p->err = INK_FS_ERR_IO;
        if (f) {
            if (ink_fs_read(f, data, (size_t)e.size) != e.size || ink_image_decode(data, (size_t)e.size, &p->im)) p->err = INK_FS_ERR_IO;
            if (ink_fs_close(f)) p->err = INK_FS_ERR_IO;
        }
        osal_free(data);
        if (!p->err) { snprintf(p->path, sizeof p->path, "%s", e.path); ink_image_dither(&p->im); }
    }
    ink_exec_post(INK_EXEC_UI, &p->job, loaded, p);
}
static void load(uint32_t id) {
    snprintf(G.message, sizeof G.message, TR("正在读取照片"));
    if (!id && G.box) lv_obj_add_flag(lv_obj_get_child(G.box, 0), LV_OBJ_FLAG_HIDDEN);
    load_t *p = osal_calloc(1, sizeof *p, OSAL_MEM_EXTERNAL);
    if (!p) { snprintf(G.message, sizeof G.message, TR("内存不足，请返回后重试")); return; }
    p->id = id; p->epoch = ink_member_epoch(); p->generation = ink_ui_generation();
    if (ink_exec_post(INK_EXEC_MEDIA, &p->job, read_photos, p)) { osal_free(p); snprintf(G.message, sizeof G.message, TR("暂时读不了照片，请重试")); }
}
static void gallery_create(lv_obj_t *root, void *arg) {
    const ink_ui_action_spec_t GALLERY_ACTIONS[] = {
        {.id = "prev", .label = TR("上一页"), .capability = "gallery.prev"},
        {.id = "next", .label = TR("下一页"), .capability = "gallery.next"},
        {.id = "camera", .label = TR("拍照"), .capability = "app.open", .args_json = "{\"id\":\"camera\"}", .enabled = true}
    };
    const ink_ui_page_spec_t GALLERY_PAGE = {.id = "Gallery", .title = TR("相册"), .object = {"gallery:photos", 1}, .actions = GALLERY_ACTIONS, .action_count = 3};
    (void)arg;
    if (G.epoch != ink_member_epoch()) { memset(&G.position, 0, sizeof G.position); G.epoch = ink_member_epoch(); G.count = 0; }
    ink_shell_statusbar(root, true); ink_shell_topbar(root, TR("相册"));
    lv_obj_t *b = ink_ui_circle(root, "camera", INK_CANVAS_W - INK_MARGIN - INK_CIRCLE, INK_STATUS_BAR, INK_CIRCLE, INK_ICON_MEDIUM, INK_SURFACE_STATIC, NULL, NULL);
    ink_shell_bind(b, "app.open", "{\"id\":\"camera\"}");
    int y = INK_STATUS_BAR + INK_PRIMARY + INK_GAP;
    G.box = ink_ui_box(root, 0, y, INK_CANVAS_W, INK_CANVAS_H - y, 0, INK_SURFACE_WHITE, false);
    int cell = (INK_CANVAS_W - 2 * INK_MARGIN - (PHOTO_COLUMNS - 1) * INK_GAP_M) / PHOTO_COLUMNS;
    G.list = ink_ui_list_create(G.box, &GALLERY_PAGE, &G.position, PHOTO_SLOTS, PHOTO_COLUMNS, cell, TR("还没有照片，点右上角拍一张"), bind_photo, NULL);
    b = ink_ui_pill(root, TR("重读照片"), INK_MARGIN, INK_CANVAS_H - INK_MARGIN - INK_CONTROL, INK_CANVAS_W - 2 * INK_MARGIN, INK_CONTROL, false, NULL, NULL);
    ink_shell_bind(b, "gallery.reload", NULL);
    load(0); G.status = ink_ui_notice(root, G.message, INK_CANVAS_H - INK_MARGIN - INK_CONTROL - INK_GAP_L - 2 * ink_ui_line_height(INK_FONT_NOTE));
}
static void photo_create(lv_obj_t *root, void *arg) {
    G.photo = (uint32_t)(uintptr_t)arg; G.path[0] = G.caption[0] = 0;
    ink_shell_statusbar(root, true); ink_shell_topbar(root, TR("照片"));
    G.photo_actions[0] = (ink_ui_action_spec_t){.id = "delete", .label = TR("删除当前照片"), .capability = "file.delete"};
    G.photo_actions[1] = (ink_ui_action_spec_t){.id = "ask", .label = TR("问 AI 这张图"), .capability = "photo.ask"};
    G.photo_actions[2] = (ink_ui_action_spec_t){.id = "back", .label = TR("返回"), .capability = "shell.back", .enabled = true};
    lv_obj_t *b = ink_ui_circle(root, "trash", INK_CANVAS_W - INK_MARGIN - INK_CIRCLE, INK_STATUS_BAR, INK_CIRCLE, INK_ICON_MEDIUM, INK_SURFACE_STATIC, NULL, NULL);
    G.trash = b; lv_obj_add_flag(b, LV_OBJ_FLAG_HIDDEN);
    G.name = ink_ui_notice(root, TR("正在读取照片"), INK_STATUS_BAR + INK_PRIMARY);
    int y = INK_STATUS_BAR + INK_PRIMARY + 2 * ink_ui_line_height(INK_FONT_NOTE) + INK_GAP_M, w = INK_CANVAS_W - 2 * INK_MARGIN;
    int footer = INK_CANVAS_H - INK_MARGIN - INK_CONTROL;
    G.box = ink_ui_card(root, INK_MARGIN, y, w, footer - y - 2 * ink_ui_line_height(INK_FONT_NOTE) - 2 * INK_GAP_L, INK_SURFACE_WHITE);
    b = ink_ui_pill(root, TR("问 AI 这张图"), INK_MARGIN, footer, w, INK_CONTROL, false, NULL, NULL); ink_shell_bind(b, "photo.ask", NULL);
    load(G.photo); G.status = ink_ui_notice(root, G.message, footer - 2 * ink_ui_line_height(INK_FONT_NOTE) - INK_GAP_L);
}
static void gallery_destroy(void) { G.list = NULL; G.box = G.status = NULL; memset(G.slots, 0, sizeof G.slots); }
static void photo_destroy(void) { G.box = G.status = G.trash = G.name = NULL; }
static int gallery_snapshot(char *out, size_t cap) {
    int n = G.list ? ink_ui_list_snapshot(G.list, out, cap) : -1;
    if (n < 0) return -1;
    char *end = strstr(out, "],\"pagination\":");
    char extra[256]; ink_jw_t a; ink_jw_init(&a, extra, sizeof extra);
    ink_jw_raw(&a, ",{\"id\":\"reload\",\"label\":"); ink_jw_str(&a, TR("重读照片"));
    ink_jw_raw(&a, ",\"capability\":\"gallery.reload\",\"args_json\":\"{}\",\"object\":{\"id\":\"gallery:photos\",\"revision\":1}}");
    if (!end || a.overflow || (size_t)n + a.len >= cap) { out[0] = 0; return -1; }
    memmove(end + a.len, end, strlen(end) + 1); memcpy(end, extra, a.len); n += (int)a.len;
    ink_jw_t w = {.buf = out, .cap = cap, .len = (size_t)n - 1};
    ink_jw_raw(&w, ",\"status\":"); ink_jw_str(&w, G.message); ink_jw_raw(&w, "}");
    if (w.overflow) { out[0] = 0; return -1; }
    return (int)w.len;
}
static int photo_snapshot(char *out, size_t cap) {
    const ink_ui_field_spec_t PHOTO_FIELDS[] = {
        {.id = "file", .role = "image", .label = TR("照片文件"), .value = G.path},
        {.id = "status", .role = "status", .label = TR("提示"), .value = G.message},
        {.id = "name", .role = "text", .label = TR("当前照片"), .value = G.caption}
    };
    const ink_ui_page_spec_t PHOTO_PAGE = {.id = "Photo", .title = TR("照片"), .object = {G.path, G.revision}, .fields = PHOTO_FIELDS, .field_count = 3, .actions = G.photo_actions, .action_count = 3};
    return ink_ui_semantics_snapshot(&PHOTO_PAGE, out, cap);
}
static bool gallery_key(ink_key_t k, bool lng) { return G.list && ink_ui_list_key(G.list, k, lng); }
static const ink_activity_t GALLERY = {.name = "Gallery", .app = "gallery", .create = gallery_create, .destroy = gallery_destroy, .snapshot = gallery_snapshot, .on_key = gallery_key};
static const ink_activity_t PHOTO = {.name = "Photo", .app = "gallery", .create = photo_create, .destroy = photo_destroy, .snapshot = photo_snapshot};
static void photo_found(void *ctx, const ink_receipt_t *rc) {
    ink_call_t *c = ctx;
    ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL);
    int64_t id = 0; ink_args_get_int(&c->args, "id", &id);
    bool found = j && rc->status == INK_RC_DONE && rc->result && !ink_json_parse(j, rc->result, strlen(rc->result)) &&
        ink_json_eq(j, ink_json_find(j, ink_json_find(j, 0, "entries") + 1, "type"), "image");
    osal_free(j);
    if (!found) c->rc.status = INK_RC_REJECTED;
    else if (strcmp(ink_cap_call_member(c), ink_member_current()->id) || !ink_member_space_open()) c->rc.status = INK_RC_STALE;
    else ink_ui_push(&PHOTO, (void *)(uintptr_t)id);
    ink_cap_done(c);
}
static void photo_question_ready(void *ctx, const ink_receipt_t *rc) {
    photo_question_t *p = ctx;
    if (p->epoch == ink_member_epoch() && p->generation == ink_ui_generation() && p->photo == G.photo &&
        p->revision && p->revision == ink_fs_revision() && ink_ui_page_current() && !strcmp(ink_ui_page_name(), "Photo") && rc->status == INK_RC_DONE && rc->result)
        ink_ai_ask_photo(rc->result);
    osal_free(p);
}
static void gallery_action(ink_call_t *c) {
    if (!strcmp(c->cap->id, "gallery.reload")) { ink_ui_redraw(); return; }
    if (!strcmp(c->cap->id, "photo.ask")) {
        photo_question_t *p = osal_malloc(sizeof *p, OSAL_MEM_EXTERNAL);
        if (!p || !G.path[0] || !G.revision || G.revision != ink_fs_revision()) { osal_free(p); c->rc.status = INK_RC_STALE; return; }
        *p = (photo_question_t){ink_member_epoch(), ink_ui_generation(), G.photo, ink_fs_revision()};
        ink_args_t a; ink_args_init(&a); ink_args_int(&a, "id", G.photo);
        ink_call_opts_t o = {.src = INK_SRC_SYSTEM, .member = ink_cap_call_member(c), .reply_exec = INK_EXEC_UI, .done = photo_question_ready, .ctx = p};
        if (ink_cap_call("file.thumb", &a, &o)) { osal_free(p); c->rc.status = INK_RC_FAILED; }
        return;
    }
    if (!strcmp(c->cap->id, "photo.open")) {
        /* photo.open already passed the app policy; the service still checks file ownership. */
        ink_call_opts_t o = {.src = INK_SRC_SYSTEM, .member = ink_cap_call_member(c), .reply_exec = INK_EXEC_UI, .done = photo_found, .ctx = c};
        ink_cap_defer(c);
        if (ink_cap_call("file.lookup", &c->args, &o)) { c->rc.status = INK_RC_FAILED; ink_cap_done(c); }
        return;
    }
    if (G.list) ink_ui_list_turn(G.list, !strcmp(c->cap->id, "gallery.next") ? 1 : -1);
}
static const ink_cap_t GALLERY_CAPS[] = {
    {.id = "gallery.reload", .app = "gallery", .title = "手动重读当前家人的照片", .activity = "Gallery", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = gallery_action},
    {.id = "photo.ask", .app = "gallery", .title = "把当前有权查看的照片缩略图交给 AI", .activity = "Photo", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = gallery_action},
    {.id = "gallery.prev", .app = "gallery", .title = "相册上一页", .activity = "Gallery", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = gallery_action},
    {.id = "gallery.next", .app = "gallery", .title = "相册下一页", .activity = "Gallery", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = gallery_action},
    {.id = "photo.open", .locked_owner_only = true, .app = "gallery", .title = "查看有权访问的图片文件", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = gallery_action,
     .input = "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"integer\",\"minimum\":1,\"maximum\":4294967295}},\"required\":[\"id\"]}"}
};
static void photos_changed(void *ctx, const char *id, const ink_args_t *args) {
    (void)ctx; (void)id;
    if (!ink_ui_page_current()) return;
    if (!strcmp(ink_ui_page_name(), "Gallery")) ink_ui_redraw();
    else if (!strcmp(ink_ui_page_name(), "Photo")) {
        char path[INK_FS_PATH_MAX], kind[24]; ink_args_get_str(args, "path", path, sizeof path); ink_args_get_str(args, "kind", kind, sizeof kind);
        if (!strcmp(path, G.path) && !strcmp(kind, "removed")) ink_ui_pop();
    }
}
int ink_gallery_init(void) {
    static const ink_app_def_t app = {.id = "gallery", .title = TR_NOOP("相册"), .icon = INK_UI_ICON(gallery), .entry = &GALLERY, .system_app = true, .child_ok = true};
    return ink_cap_register(GALLERY_CAPS, sizeof GALLERY_CAPS / sizeof *GALLERY_CAPS) || ink_app_register(&app) || ink_cap_subscribe("file.changed", photos_changed, NULL, INK_EXEC_UI) ? -1 : 0;
}
