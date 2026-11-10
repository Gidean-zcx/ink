/* Still-frame viewfinder; capture and encoding stay on the media executor. */
#include "ink/osal.h"
#include "ink/sys/appreg.h"
#include "ink/sys/camera.h"
#include "ink/sys/file.h"
#include "ink/sys/i18n.h"
#include "ink/sys/members.h"
#include "ink/sys/power.h"
#include "ink/sys/ui_semantics.h"
#include "ink/ui_shell.h"
#include "../gallery/photo.h"
#include <stdio.h>
#include <string.h>

uint8_t *ink_camera_png(const uint8_t *rgb, int w, int h, int *bytes);
int ink_camera_scan_init(void);
static lv_obj_t *view, *notice;
static lv_timer_t *timer;
static int taking;
static char camera_status[96];
static lv_obj_t *locked_root;
static int restricted;
static struct { uint32_t ids[INK_FS_INDEX_MAX]; unsigned count, at; bool reviewing; } shots INK_PSRAM_BSS;
bool ink_camera_restricted(void) { return __atomic_load_n(&restricted, __ATOMIC_ACQUIRE); }
static const char LOCK_PHOTOS[] = "/family/锁屏拍的照片";
static const ink_ui_action_spec_t CAMERA_ACTIONS[] = {
    {.id = "shoot", .label = "拍照", .capability = "camera.photo", .enabled = true},
    {.id = "preview", .label = "更新取景", .capability = "camera.preview", .enabled = true},
    {.id = "scan", .label = "扫码", .capability = "camera.scan", .enabled = true},
    {.id = "gallery", .label = "相册", .capability = "app.open", .args_json = "{\"id\":\"gallery\"}", .enabled = true},
    {.id = "close", .label = "关闭", .capability = "shell.back", .enabled = true}
};
static const ink_ui_field_spec_t CAMERA_FIELDS[] = {{.id = "status", .role = "status", .label = "取景和保存", .value = camera_status}};
static const ink_ui_page_spec_t CAMERA_PAGE = {.id = "Camera", .title = "相机", .object = {"camera:view", 1},
    .fields = CAMERA_FIELDS, .field_count = 1, .actions = CAMERA_ACTIONS, .action_count = 5};
typedef struct { ink_exec_job_t job; ink_image_t im; uint32_t epoch, generation, id; unsigned at; bool ok, saved, reviewed; } capture_t;
static void captured(void *arg) {
    capture_t *p = arg;
    if (p->epoch == ink_member_epoch() && p->generation == ink_ui_generation() && view && (ink_ui_page_current() || ink_camera_restricted())) {
        if (p->im.pixels) { lv_obj_clean(view); ink_photo_image(view, &p->im); }
        snprintf(camera_status, sizeof camera_status, "%s", p->ok ? (p->saved ? "已保存到存储卡" : "低频取景，点下方拍照") : "没能完成，请检查存储卡或重试");
        if (ink_camera_restricted() && p->ok) {
            if (p->saved && shots.count < INK_FS_INDEX_MAX) shots.ids[shots.count++] = p->id;
            shots.reviewing = p->saved || p->reviewed;
            shots.at = p->reviewed ? p->at : shots.count ? shots.count - 1 : 0;
            if (shots.reviewing) snprintf(camera_status, sizeof camera_status, TR("本次 %u 张，正在看第 %u 张"), shots.count, shots.at + 1);
            else snprintf(camera_status, sizeof camera_status, TR("本次已拍 %u 张"), shots.count);
        }
        lv_label_set_text(notice, camera_status); ink_ui_content_changed(); ink_ui_hint_obj(view, INK_REFRESH_UI);
    }
    ink_image_free(&p->im); osal_free(p); ink_power_release(INK_NEED_AWAKE); __atomic_store_n(&taking, 0, __ATOMIC_RELEASE);
}
static void take(ink_call_t *c) {
    bool limited = ink_camera_restricted();
    capture_t *p = osal_calloc(1, sizeof *p, OSAL_MEM_EXTERNAL);
    if (!p) { c->rc.status = INK_RC_FAILED; return; }
    ink_power_hold(INK_NEED_AWAKE);
    __atomic_store_n(&taking, 1, __ATOMIC_RELEASE);
    p->epoch = ink_member_epoch(); p->generation = ink_ui_generation(); p->saved = !strcmp(c->cap->id, "camera.photo");
    p->reviewed = !strcmp(c->cap->id, "camera.review");
    int w = 0, h = 0; uint8_t *rgb = NULL;
    if (p->reviewed) {
        ink_fs_entry_t e; ink_fs_file_t *f = NULL;
        p->at = shots.count ? (shots.at + 1) % shots.count : 0;
        void *data = shots.count && !ink_fs_lookup(shots.ids[p->at], &e) && e.size <= INK_IMAGE_MAX_INPUT ? osal_malloc((size_t)e.size, OSAL_MEM_EXTERNAL) : NULL;
        if (!data || ink_fs_open(e.path, INK_FS_READ, &f) || ink_fs_read(f, data, (size_t)e.size) != e.size || ink_image_decode(data, (size_t)e.size, &p->im)) c->rc.status = INK_RC_FAILED;
        if (f && ink_fs_close(f)) c->rc.status = INK_RC_FAILED;
        osal_free(data);
        if (p->im.pixels) ink_image_dither(&p->im);
    } else {
        rgb = ink_camera_read_rgb(&w, &h);
        if (!rgb) c->rc.status = INK_RC_FAILED;
    }
    if (rgb && p->saved) {
        int bytes = 0; uint8_t *png = ink_camera_png(rgb, w, h, &bytes);
        const char *member = ink_cap_call_member(c); char path[INK_FS_PATH_MAX]; ink_fs_file_t *f = NULL;
        const char *folder = limited ? LOCK_PHOTOS : "/me/照片";
        if (limited) for (int i = 0; ink_member_at(i); i++)
            if (ink_member_at(i)->role == INK_ROLE_ADULT) { member = ink_member_at(i)->id; break; }
        int r = png ? ink_fs_mkdir_for(member, folder) : INK_FS_ERR_IO;
        unsigned seq = 0, n; ink_fs_dir_t *dir = NULL; ink_fs_stat_t entry;
        if (!r && p->epoch != ink_member_epoch()) r = INK_FS_ERR_DENIED;
        if (!r) r = ink_fs_dir_open(folder, &dir);
        if (!r) while ((r = ink_fs_dir_next(dir, &entry)) == 1)
            if (!entry.is_dir && sscanf(entry.name, "照片-%u.png", &n) == 1 && n > seq) seq = n;
        ink_fs_dir_close(dir);
        if (!r) for (unsigned i = 0; i < INK_FS_INDEX_MAX; i++) {
            snprintf(path, sizeof path, "%s/照片-%u.png", folder, ++seq);
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
        if (!r && p->epoch == ink_member_epoch()) while (ink_fs_index(folder, INK_FS_IMAGE, &cursor, &e) == 1)
            if (!strcmp(e.path, path)) { indexed = true; p->id = e.id; if (!limited) snprintf(c->rc.object, sizeof c->rc.object, "file:%u", (unsigned)e.id); break; }
        if (r || !indexed) c->rc.status = INK_RC_FAILED;
    }
    p->ok = c->rc.status == INK_RC_DONE;
    if (rgb) {
        for (size_t i = 0; i < (size_t)w * h; i++) rgb[i] = (77u * rgb[3*i] + 150u * rgb[3*i+1] + 29u * rgb[3*i+2] + 128u) >> 8;
        p->im = (ink_image_t){(unsigned)w, (unsigned)h, rgb}; ink_image_dither(&p->im);
    }
    snprintf(c->rc.say, sizeof c->rc.say, "%s", p->ok ? (p->saved ? "已保存到存储卡" : "取景已更新") : "没能完成，请重试");
    if (ink_exec_post(INK_EXEC_UI, &p->job, captured, p)) { ink_image_free(&p->im); osal_free(p); ink_power_release(INK_NEED_AWAKE); __atomic_store_n(&taking, 0, __ATOMIC_RELEASE); }
}
static void preview(lv_timer_t *t) {
    (void)t;
    if (ink_camera_restricted() ? shots.reviewing || strcmp(ink_ui_action_page(), "Camera") : !ink_ui_page_current() || strcmp(ink_ui_page_name(), "Camera") || ink_ui_overlay_up()) return;
    if (__atomic_load_n(&taking, __ATOMIC_ACQUIRE)) return;
    ink_call_opts_t o = {.src = INK_SRC_SYSTEM}; ink_cap_call("camera.preview", NULL, &o);
}
static void camera_create(lv_obj_t *root, void *arg) {
    (void)arg; ink_shell_statusbar(root, true);
    int w = INK_CANVAS_W - 2 * INK_MARGIN, y = INK_STATUS_BAR;
    view = ink_ui_card(root, INK_MARGIN, y, w, INK_CANVAS_H - y - INK_CIRCLE_LARGE - 2 * INK_BLOCK, INK_SURFACE_WHITE);
    if (ink_camera_restricted()) ink_shell_bind(view, "camera.preview", NULL);
    snprintf(camera_status, sizeof camera_status, "正在取景");
    notice = ink_ui_notice(root, camera_status, INK_CANVAS_H - INK_CIRCLE_LARGE - INK_BLOCK);
    if (!ink_camera_restricted()) { /* the lock-screen camera only takes photos (D-121) */
        lv_obj_t *scan_button = ink_ui_pill(root, TR("扫码"), INK_MARGIN, INK_STATUS_BAR + INK_GAP, INK_PRIMARY * 2, INK_PRIMARY, false, NULL, NULL);
        ink_shell_bind(scan_button, "camera.scan", NULL);
    }
    lv_obj_t *b = ink_ui_circle(root, "gallery", INK_MARGIN, INK_CANVAS_H - INK_MARGIN - INK_CIRCLE_APP, INK_CIRCLE_APP, INK_ICON_LIST, INK_SURFACE_WHITE, NULL, NULL);
    if (ink_camera_restricted()) ink_shell_bind(b, "camera.review", NULL);
    else ink_shell_bind(b, "app.open", "{\"id\":\"gallery\"}");
    b = ink_ui_circle(root, "camera", (INK_CANVAS_W - INK_CIRCLE_LARGE) / 2, INK_CANVAS_H - INK_MARGIN - INK_CIRCLE_LARGE, INK_CIRCLE_LARGE, INK_ICON_APP, INK_SURFACE_BLACK, NULL, NULL);
    ink_shell_bind(b, "camera.photo", NULL);
    b = ink_ui_circle(root, "close", INK_CANVAS_W - INK_MARGIN - INK_CIRCLE_APP, INK_CANVAS_H - INK_MARGIN - INK_CIRCLE_APP, INK_CIRCLE_APP, INK_ICON_LIST, INK_SURFACE_STATIC, NULL, NULL);
    ink_shell_bind(b, "shell.back", NULL);
    timer = lv_timer_create(preview, 3000, NULL); preview(NULL);
}
static void camera_destroy(void) { if (timer) { lv_timer_delete(timer); } timer = NULL; view = notice = NULL; }
static int camera_snapshot(char *out, size_t cap) {
    if (ink_camera_restricted()) return snprintf(out, cap, TR("锁屏相机；取景；本次已拍 %u 张；动作：camera.preview、camera.photo、camera.review、shell.back。"), shots.count);
    return ink_ui_semantics_snapshot(&CAMERA_PAGE, out, cap);
}
static bool locked_key(ink_key_t k, bool lng) {
    if (!lng && (k == INK_KEY_HOME || k == INK_KEY_BACK)) ink_camera_close_locked();
    return true;
}
static const char *const LOCKED_CAPS[] = {"camera.preview", "camera.photo", "camera.review", "shell.back", "system.home", NULL};
static const ink_activity_t LOCKED_CAMERA = {.name = "Camera", .app = "camera", .locked_caps = LOCKED_CAPS, .on_key = locked_key};
static const ink_ui_overlay_t LOCK_CAMERA = {.name = "Camera", .activity = &LOCKED_CAMERA, .level = 3, .snapshot = camera_snapshot, .on_key = locked_key};
int ink_camera_open_locked(void) {
    if (locked_root) return 0;
    if (!ink_cap_find("camera.capture")) return -1;
    ink_ui_drop_app("camera");
    ink_ui_drop_app("gallery");
    ink_power_hold(INK_NEED_AWAKE);
    memset(&shots, 0, sizeof shots); __atomic_store_n(&restricted, 1, __ATOMIC_RELEASE);
    locked_root = ink_ui_box(lv_layer_sys(), 0, 0, INK_CANVAS_W, INK_CANVAS_H, 0, INK_SURFACE_WHITE, false);
    lv_obj_add_flag(locked_root, LV_OBJ_FLAG_CLICKABLE);
    ink_ui_overlay_add(&LOCK_CAMERA, locked_root); camera_create(locked_root, NULL);
    lv_obj_remove_flag(lv_obj_get_child(locked_root, 0), LV_OBJ_FLAG_CLICKABLE);
    ink_ui_hint_frame(INK_REFRESH_FLASH_UI);
    return 0;
}
bool ink_camera_close_locked(void) {
    if (!locked_root) return false;
    camera_destroy(); ink_ui_overlay_remove(&LOCK_CAMERA); lv_obj_delete(locked_root); locked_root = NULL;
    __atomic_store_n(&restricted, 0, __ATOMIC_RELEASE); memset(&shots, 0, sizeof shots);
    ink_power_release(INK_NEED_AWAKE);
    ink_power_lock(); ink_ui_hint_frame(INK_REFRESH_FLASH_UI);
    return true;
}
void ink_shell_camera_button(lv_obj_t *root, bool strip) {
    if (!ink_cap_find("camera.capture")) return;
    lv_obj_t *b = ink_ui_circle(root, "camera", INK_CANVAS_W - (strip ? 2 : 1) * INK_MARGIN - INK_CIRCLE, strip ? 0 : INK_STATUS_BAR + INK_GAP,
        INK_CIRCLE, INK_ICON_MEDIUM, INK_SURFACE_WHITE, NULL, NULL);
    ink_shell_bind(b, "app.open", "{\"id\":\"camera\"}");
}
static const ink_activity_t CAMERA = {.name = "Camera", .app = "camera", .create = camera_create, .destroy = camera_destroy, .snapshot = camera_snapshot};
static const ink_cap_t CAMERA_APP_CAPS[] = {
    {.id = "camera.preview", .locked_owner_only = true, .app = "camera", .title = "更新相机取景", .activity = "Camera", .effect = INK_FX_READ, .child_ok = true, .exec = INK_EXEC_MEDIA, .timeout_ms = 15000, .run = take},
    {.id = "camera.photo", .locked_owner_only = true, .app = "camera", .title = "拍照并保存到存储卡和相册", .activity = "Camera", .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .exec = INK_EXEC_MEDIA, .timeout_ms = 15000, .run = take},
    {.id = "camera.review", .locked_owner_only = true, .app = "camera", .title = "查看本次锁屏拍的下一张照片", .activity = "Camera", .effect = INK_FX_READ, .child_ok = true, .exec = INK_EXEC_MEDIA, .timeout_ms = 15000, .run = take}
};
int ink_camera_app_init(void) {
    static const char *const needs[] = {"camera.capture", NULL};
    static const ink_app_def_t app = {.id = "camera", .title = "相机", .entry = &CAMERA, .system_app = true, .child_ok = true, .needs = needs};
    return ink_camera_scan_init() || ink_cap_register(CAMERA_APP_CAPS, 3) || ink_app_register(&app) ? -1 : 0;
}
