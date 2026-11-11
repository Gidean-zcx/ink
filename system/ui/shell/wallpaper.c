/* Decode on the media executor; keep only dithered pixels in PSRAM. */
/* ui-scale-runtime: dimension x,y,w,h Theme-sized preview rows and widget controls. */
#include "ink/sys/i18n.h"
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include "ink/osal.h"
#include "ink/sys/file_picker.h"
#include "ink/sys/image.h"
#include "ink/sys/members.h"
#include "ink/sys/settings.h"
#include "ink/sys/state.h"
#include "ink/ui_shell.h"
#include "../lock_faces/faces.h"

enum { FACE_SETTINGS_MAX = INK_LOCK_FACE_COUNT + 3 };
static struct {
    ink_ui_field_spec_t fields[FACE_SETTINGS_MAX];
    ink_ui_action_spec_t actions[FACE_SETTINGS_MAX];
    char args[FACE_SETTINGS_MAX][96];
    ink_ui_page_spec_t model;
} P INK_PSRAM_BSS;

void ink_lock_face_settings(lv_obj_t *parent) {
    int w = INK_CANVAS_W - 2 * INK_MARGIN, h = 2 * ink_ui_line_height(INK_FONT_STATUS) + 2 * INK_GAP_S;
    if (h < INK_CONTROL) h = INK_CONTROL;
    static const char *const previews[] = {"", "08:30", "", "08\n30", "08:30\n-- --", "1 2 3\n4 5 6"};
    _Static_assert(sizeof previews / sizeof *previews == INK_LOCK_FACE_COUNT, "previews match");
    for (int i = 0; i < INK_LOCK_FACE_COUNT; i++) {
        const char *title = TR(ink_lock_face_choices[i]);
        bool selected = ink_setting_get("display.lock_style") == i;
        lv_obj_t *p = ink_ui_box(parent, INK_MARGIN, i * h, w, h, 0, INK_SURFACE_WHITE, false);
        lv_obj_t *preview = ink_ui_box(p, 0, INK_GAP_XS, INK_PRIMARY, h - 2 * INK_GAP_XS, INK_RADIUS_KEY, INK_SURFACE_STATIC, true);
        lv_obj_t *label = ink_ui_label(preview, previews[i], INK_FONT_STATUS, INK_WEIGHT_REGULAR, 0, 0, INK_PRIMARY);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0); lv_obj_center(label);
        if (i == 0 || i == 2) lv_obj_center(ink_ui_icon(preview, i == 0 ? "page" : "gallery", INK_ICON_MEDIUM, INK_ICON_LINE, 0, 0, false));
        ink_ui_line(p, title, INK_FONT_LIST, INK_WEIGHT_REGULAR, INK_PRIMARY + INK_GAP,
                    (h - ink_ui_line_height(INK_FONT_LIST)) / 2, w - INK_PRIMARY - INK_CONTROL - INK_GAP);
        ink_ui_box(p, w - INK_ICON_SMALL, (h - INK_ICON_SMALL) / 2, INK_ICON_SMALL, INK_ICON_SMALL, -1, INK_SURFACE_WHITE, true);
        if (selected) ink_ui_icon(p, "check", INK_ICON_SMALL, INK_ICON_LINE, w - INK_ICON_SMALL, (h - INK_ICON_SMALL) / 2, false);
        snprintf(P.args[i], sizeof P.args[i], "{\"id\":\"display.lock_style\",\"value\":%d}", i);
        P.fields[i] = (ink_ui_field_spec_t){.id = ink_lock_face_choices[i], .role = "option", .label = title, .value = selected ? TR("已选择") : ""};
        P.actions[i] = (ink_ui_action_spec_t){.id = ink_lock_face_choices[i], .label = title, .capability = "settings.set", .args_json = P.args[i], .enabled = true};
        ink_shell_bind(p, "settings.set", P.args[i]);
    }
    static const char *const ids[] = {"display.lock_todo", "display.lock_reading", "display.lock_background"};
    static const char *const titles[] = {TR_NOOP("锁屏显示待办"), TR_NOOP("锁屏继续读"), TR_NOOP("表盘叠在壁纸上")};
    _Static_assert(sizeof ids / sizeof *ids == sizeof titles / sizeof *titles, "widget settings match");
    _Static_assert(INK_LOCK_FACE_COUNT + sizeof ids / sizeof *ids <= FACE_SETTINGS_MAX, "face setting rows fit");
    for (size_t i = 0; i < sizeof ids / sizeof *ids; i++) {
        int k = INK_LOCK_FACE_COUNT + (int)i; bool on = ink_setting_get(ids[i]) != 0;
        snprintf(P.args[k], sizeof P.args[k], "{\"id\":\"%s\",\"value\":%d}", ids[i], !on);
        P.fields[k] = (ink_ui_field_spec_t){.id = ids[i], .role = "setting", .label = TR(titles[i]), .value = on ? TR("开") : TR("关")};
        P.actions[k] = (ink_ui_action_spec_t){.id = ids[i], .label = TR(titles[i]), .capability = "settings.set", .args_json = P.args[k], .enabled = true};
        ink_shell_bind(ink_ui_toggle(parent, TR(titles[i]), on, INK_MARGIN,
            INK_LOCK_FACE_COUNT * h + (int)i * INK_CONTROL, w, NULL, NULL), "settings.set", P.args[k]);
    }
    P.model = (ink_ui_page_spec_t){.id = "LockFaces", .title = TR("锁屏画面"), .object = {"display.lock_style", 0}, .fields = P.fields, .field_count = FACE_SETTINGS_MAX,
        .actions = P.actions, .action_count = FACE_SETTINGS_MAX};
}
static void face_create(lv_obj_t *screen, void *arg) {
    (void)arg;
    ink_shell_statusbar(screen, true); ink_shell_topbar(screen, TR("锁屏画面"));
    int y = INK_STATUS_BAR + INK_PRIMARY + INK_GAP_XS;
    ink_lock_face_settings(ink_ui_box(screen, 0, y, INK_CANVAS_W, INK_CANVAS_H - y, 0, INK_SURFACE_WHITE, false));
}
static int face_snapshot(char *out, size_t cap) { return ink_ui_semantics_snapshot(&P.model, out, cap); }
static const ink_activity_t FACE_PAGE = {.name = "LockFaces", .create = face_create, .snapshot = face_snapshot};
static void face_open(ink_call_t *c) { (void)c; ink_shell_close(); ink_ui_push(&FACE_PAGE, NULL); }
static void face_changed(void *ctx, int h, const char *id) {
    (void)ctx; (void)h; (void)id;
    if (ink_ui_page_current() && !strcmp(ink_ui_page_name(), FACE_PAGE.name)) ink_ui_redraw();
}

typedef struct { ink_exec_job_t job; ink_image_t image; uint32_t epoch; int64_t id; } loaded_t;
static struct { ink_image_t image; lv_image_dsc_t dsc; ink_exec_job_t load; uint32_t epoch; } W INK_PSRAM_BSS;
bool ink_wallpaper_ready(void) { return W.image.pixels && W.epoch == ink_member_epoch(); }
void ink_wallpaper_draw(lv_obj_t *parent) {
    if (!ink_wallpaper_ready()) return;
    lv_obj_t *im = lv_image_create(parent); lv_image_set_src(im, &W.dsc);
    lv_obj_set_size(im, ink_ui_px(INK_CANVAS_W), ink_ui_px(INK_CANVAS_H));
    lv_image_set_inner_align(im, LV_IMAGE_ALIGN_COVER); lv_obj_move_background(im);
}
static void install(void *arg) {
    loaded_t *result = arg;
    if (result->epoch != ink_member_epoch() || result->id != ink_setting_get("display.wallpaper_file")) {
        ink_image_free(&result->image); osal_free(result); return;
    }
    ink_image_free(&W.image); W.image = result->image; W.epoch = result->epoch; osal_free(result);
    W.dsc = (lv_image_dsc_t){.header = {.magic = LV_IMAGE_HEADER_MAGIC, .cf = LV_COLOR_FORMAT_L8,
        .w = W.image.width, .h = W.image.height, .stride = W.image.width},
        .data_size = W.image.width * W.image.height, .data = W.image.pixels};
    ink_ui_redraw();
}
static void load(void *arg) {
    (void)arg;
    uint32_t epoch = ink_member_epoch(); int64_t id = ink_setting_get("display.wallpaper_file");
    ink_fs_entry_t e; ink_fs_file_t *f = NULL; void *bytes = NULL; ink_image_t image = {0};
    if (id && !ink_fs_lookup((uint32_t)id, &e) && e.type == INK_FS_IMAGE && e.size > 0 && e.size <= INK_IMAGE_MAX_INPUT &&
        (bytes = osal_malloc((size_t)e.size, OSAL_MEM_EXTERNAL)) && !ink_fs_open(e.path, INK_FS_READ, &f)) {
        if (ink_fs_read(f, bytes, (size_t)e.size) == e.size && !ink_image_decode(bytes, (size_t)e.size, &image)) ink_image_dither(&image);
        ink_fs_close(f);
    }
    osal_free(bytes);
    if (epoch != ink_member_epoch()) { ink_image_free(&image); return; }
    loaded_t *result = osal_calloc(1, sizeof *result, OSAL_MEM_EXTERNAL);
    if (!result) { ink_image_free(&image); return; }
    result->image = image; result->epoch = epoch; result->id = id;
    if (ink_exec_post(INK_EXEC_UI, &result->job, install, result)) { ink_image_free(&result->image); osal_free(result); }
}
static void changed(void *ctx, int h, const char *id) {
    (void)ctx; (void)h; (void)id;
    if (!ink_setting_get("display.wallpaper_file") && !W.image.pixels) return;
    ink_exec_post(INK_EXEC_MEDIA, &W.load, load, NULL);
}
static void selected(void *ctx, uint32_t id) {
    if (!id) return;
    ink_args_t args; ink_args_init(&args); ink_args_str(&args, "id", "display.wallpaper_file"); ink_args_int(&args, "value", id);
    ink_call_opts_t o = {.src = (ink_source_t)(uintptr_t)ctx}; ink_cap_call("settings.set", &args, &o);
}
static void choose(ink_call_t *c) {
    if (ink_file_picker_open(INK_FS_IMAGE, selected, (void *)(uintptr_t)c->src)) c->rc.status = INK_RC_FAILED;
}
int ink_wallpaper_init(void) {
    static const ink_setting_def_t widgets[] = {
        {.id = "display.lock_todo", .title = TR_NOOP("锁屏显示待办"), .type = INK_SET_BOOL, .per_member = true, .child_may_change = true},
        {.id = "display.lock_reading", .title = TR_NOOP("锁屏继续读"), .type = INK_SET_BOOL, .per_member = true, .child_may_change = true},
        {.id = "display.lock_background", .title = TR_NOOP("表盘叠在壁纸上"), .type = INK_SET_BOOL, .per_member = true, .child_may_change = true}};
    if (ink_settings_define(widgets, sizeof widgets / sizeof *widgets)) return -1;
    static const ink_setting_def_t setting = {.id = "display.wallpaper_file", .title = TR_NOOP("壁纸图片编号"), .type = INK_SET_INT, .max = UINT32_MAX};
    static const ink_cap_t cap = {.id = "wallpaper.choose", .title = "选择锁屏壁纸", .effect = INK_FX_NAVIGATE, .run = choose};
    static const ink_cap_t faces = {.id = "shell.lock_faces", .title = "选择锁屏表盘和小组件", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = face_open};
    if (ink_settings_define(&setting, 1) || ink_cap_register(&cap, 1) ||
        ink_cap_register(&faces, 1) || ink_settings_watch("display.lock_", face_changed, NULL, INK_EXEC_UI) ||
        ink_settings_watch("display.wallpaper_file", changed, NULL, INK_EXEC_UI) || ink_state_watch("member.", changed, NULL, INK_EXEC_UI)) return -1;
    changed(NULL, 0, NULL); return 0;
}
