/* Decode on the media executor; keep only dithered pixels in PSRAM. */
#include "ink/sys/i18n.h"
#include <limits.h>
#include "ink/osal.h"
#include "ink/sys/file_picker.h"
#include "ink/sys/image.h"
#include "ink/sys/members.h"
#include "ink/sys/settings.h"
#include "ink/sys/state.h"
#include "ink/ui_shell.h"

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
    static const ink_setting_def_t setting = {.id = "display.wallpaper_file", .title = TR_NOOP("壁纸图片编号"), .type = INK_SET_INT, .max = UINT32_MAX};
    static const ink_cap_t cap = {.id = "wallpaper.choose", .title = "选择锁屏壁纸", .effect = INK_FX_NAVIGATE, .run = choose};
    if (ink_settings_define(&setting, 1) || ink_cap_register(&cap, 1) ||
        ink_settings_watch("display.wallpaper_file", changed, NULL, INK_EXEC_UI) || ink_state_watch("member.", changed, NULL, INK_EXEC_UI)) return -1;
    changed(NULL, 0, NULL); return 0;
}
