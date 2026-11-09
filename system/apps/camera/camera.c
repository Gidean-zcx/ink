/* Still-frame viewfinder; capture and encoding stay on the media executor. */
#include "ink/osal.h"
#include "ink/sys/appreg.h"
#include "ink/sys/file.h"
#include "ink/sys/members.h"
#include "ink/sys/ui_semantics.h"
#include "ink/ui_shell.h"
#include "../gallery/photo.h"
#include <stdio.h>
#include <string.h>

uint8_t *ink_camera_read_rgb(ink_call_t *c, int *w, int *h);
uint8_t *ink_camera_png(const uint8_t *rgb, int w, int h, int *bytes);
static lv_obj_t *view, *notice;
static lv_timer_t *timer;
static int taking;
static char camera_status[96];
static const ink_ui_action_spec_t CAMERA_ACTIONS[] = {
    {.id = "shoot", .label = "拍照", .capability = "camera.photo", .enabled = true},
    {.id = "preview", .label = "更新取景", .capability = "camera.preview", .enabled = true},
    {.id = "gallery", .label = "相册", .capability = "app.open", .args_json = "{\"id\":\"gallery\"}", .enabled = true},
    {.id = "close", .label = "关闭", .capability = "shell.back", .enabled = true}
};
static const ink_ui_field_spec_t CAMERA_FIELDS[] = {{.id = "status", .role = "status", .label = "取景和保存", .value = camera_status}};
static const ink_ui_page_spec_t CAMERA_PAGE = {.id = "Camera", .title = "相机", .object = {"camera:view", 1},
    .fields = CAMERA_FIELDS, .field_count = 1, .actions = CAMERA_ACTIONS, .action_count = 4};
typedef struct { ink_exec_job_t job; ink_image_t im; uint32_t epoch, generation; bool ok, saved; } capture_t;
static void captured(void *arg) {
    capture_t *p = arg;
    if (p->epoch == ink_member_epoch() && p->generation == ink_ui_generation() && view && ink_ui_page_current()) {
        if (p->im.pixels) { lv_obj_clean(view); ink_photo_image(view, &p->im); }
        snprintf(camera_status, sizeof camera_status, "%s", p->ok ? (p->saved ? "已保存到存储卡" : "低频取景，点下方拍照") : "没能完成，请检查存储卡或重试");
        lv_label_set_text(notice, camera_status); ink_ui_content_changed(); ink_ui_hint_obj(view, INK_REFRESH_UI);
    }
    ink_image_free(&p->im); osal_free(p); __atomic_store_n(&taking, 0, __ATOMIC_RELEASE);
}
static void take(ink_call_t *c) {
    capture_t *p = osal_calloc(1, sizeof *p, OSAL_MEM_EXTERNAL);
    if (!p) { c->rc.status = INK_RC_FAILED; return; }
    __atomic_store_n(&taking, 1, __ATOMIC_RELEASE);
    p->epoch = ink_member_epoch(); p->generation = ink_ui_generation(); p->saved = !strcmp(c->cap->id, "camera.photo");
    int w = 0, h = 0; uint8_t *rgb = ink_camera_read_rgb(c, &w, &h);
    if (rgb && p->saved) {
        int bytes = 0; uint8_t *png = ink_camera_png(rgb, w, h, &bytes);
        const char *member = ink_cap_call_member(c); char path[INK_FS_PATH_MAX]; ink_fs_file_t *f = NULL;
        int r = png ? ink_fs_mkdir_for(member, "/me/照片") : INK_FS_ERR_IO;
        static unsigned seq;
        if (!r) for (unsigned i = 0; i < INK_FS_INDEX_MAX; i++) {
            snprintf(path, sizeof path, "/me/照片/照片-%u.png", ++seq);
            r = ink_fs_open_for(member, path, INK_FS_WRITE | INK_FS_CREATE | INK_FS_EXCL, &f);
            if (r != INK_FS_ERR_EXISTS) break;
        }
        if (!r) {
            if (ink_fs_write(f, png, (size_t)bytes) != bytes || ink_fs_sync(f)) r = INK_FS_ERR_IO;
            if (ink_fs_close(f)) r = INK_FS_ERR_IO;
            if (r) ink_fs_remove_for(member, path);
        }
        osal_free(png);
        uint32_t cursor = 0; ink_fs_entry_t e; bool indexed = false;
        if (!r && p->epoch == ink_member_epoch()) while (ink_fs_index("/me/照片", INK_FS_IMAGE, &cursor, &e) == 1)
            if (!strcmp(e.path, path)) { indexed = true; snprintf(c->rc.object, sizeof c->rc.object, "file:%u", e.id); break; }
        if (r || !indexed) c->rc.status = INK_RC_FAILED;
    }
    p->ok = c->rc.status == INK_RC_DONE;
    if (rgb) {
        for (size_t i = 0; i < (size_t)w * h; i++) rgb[i] = (77u * rgb[3*i] + 150u * rgb[3*i+1] + 29u * rgb[3*i+2] + 128u) >> 8;
        p->im = (ink_image_t){(unsigned)w, (unsigned)h, rgb}; ink_image_dither(&p->im);
    }
    snprintf(c->rc.say, sizeof c->rc.say, "%s", p->ok ? (p->saved ? "已保存到存储卡" : "取景已更新") : "没能完成，请重试");
    if (ink_exec_post(INK_EXEC_UI, &p->job, captured, p)) { ink_image_free(&p->im); osal_free(p); __atomic_store_n(&taking, 0, __ATOMIC_RELEASE); }
}
static void preview(lv_timer_t *t) {
    (void)t;
    if (!ink_ui_page_current() || strcmp(ink_ui_page_name(), "Camera") || ink_ui_overlay_up() || __atomic_load_n(&taking, __ATOMIC_ACQUIRE)) return;
    ink_call_opts_t o = {.src = INK_SRC_SYSTEM}; ink_cap_call("camera.preview", NULL, &o);
}
static void camera_create(lv_obj_t *root, void *arg) {
    (void)arg; ink_shell_statusbar(root, true);
    int w = INK_CANVAS_W - 2 * INK_MARGIN, y = INK_STATUS_BAR;
    view = ink_ui_card(root, INK_MARGIN, y, w, INK_CANVAS_H - y - INK_CIRCLE_LARGE - 2 * INK_BLOCK, INK_SURFACE_WHITE);
    snprintf(camera_status, sizeof camera_status, "正在取景");
    notice = ink_ui_notice(root, camera_status, INK_CANVAS_H - INK_CIRCLE_LARGE - INK_BLOCK);
    lv_obj_t *b = ink_ui_circle(root, "gallery", INK_MARGIN, INK_CANVAS_H - INK_MARGIN - INK_CIRCLE_APP, INK_CIRCLE_APP, INK_ICON_LIST, INK_SURFACE_WHITE, NULL, NULL);
    ink_shell_bind(b, "app.open", "{\"id\":\"gallery\"}");
    b = ink_ui_circle(root, "camera", (INK_CANVAS_W - INK_CIRCLE_LARGE) / 2, INK_CANVAS_H - INK_MARGIN - INK_CIRCLE_LARGE, INK_CIRCLE_LARGE, INK_ICON_APP, INK_SURFACE_BLACK, NULL, NULL);
    ink_shell_bind(b, "camera.photo", NULL);
    b = ink_ui_circle(root, "close", INK_CANVAS_W - INK_MARGIN - INK_CIRCLE_APP, INK_CANVAS_H - INK_MARGIN - INK_CIRCLE_APP, INK_CIRCLE_APP, INK_ICON_LIST, INK_SURFACE_STATIC, NULL, NULL);
    ink_shell_bind(b, "shell.back", NULL);
    timer = lv_timer_create(preview, 3000, NULL); preview(NULL);
}
static void camera_destroy(void) { if (timer) lv_timer_delete(timer); timer = NULL; view = notice = NULL; }
static int camera_snapshot(char *out, size_t cap) { return ink_ui_semantics_snapshot(&CAMERA_PAGE, out, cap); }
static const ink_activity_t CAMERA = {.name = "Camera", .app = "camera", .create = camera_create, .destroy = camera_destroy, .snapshot = camera_snapshot};
static const ink_cap_t CAMERA_APP_CAPS[] = {
    {.id = "camera.preview", .title = "更新相机取景", .activity = "Camera", .effect = INK_FX_READ, .child_ok = true, .exec = INK_EXEC_MEDIA, .timeout_ms = 15000, .run = take},
    {.id = "camera.photo", .title = "拍照并保存到存储卡和相册", .activity = "Camera", .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .exec = INK_EXEC_MEDIA, .timeout_ms = 15000, .run = take}
};
int ink_camera_app_init(void) {
    static const char *const needs[] = {"camera.capture", NULL};
    static const ink_app_def_t app = {.id = "camera", .title = "相机", .entry = &CAMERA, .system_app = true, .child_ok = true, .needs = needs};
    return ink_cap_register(CAMERA_APP_CAPS, 2) || ink_app_register(&app) ? -1 : 0;
}
