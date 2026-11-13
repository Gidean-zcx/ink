#ifndef UI_FIX_PRODUCT
#define UI_FIX_PRODUCT "../../system/apps/gallery/gallery.c"
#endif
#include UI_FIX_PRODUCT
#include "common.h"

static char fx_path[INK_FS_PATH_MAX];
static void fx_grid_check(void) {
    if (fx_before) return;
    CHECK(G.list && G.count == 2 && G.slots[0] && G.slots[1], "two indexed photos are visible");
    for (unsigned i = 0; i < 2; i++) {
        const lv_image_dsc_t *d = G.slots[i] && lv_obj_get_child_count(G.slots[i]) ? lv_image_get_src(lv_obj_get_child(G.slots[i], 0)) : NULL;
        bool black = false, white = false;
        if (d) for (unsigned at = 0; at < d->data_size; at++) { black |= d->data[at] == 0; white |= d->data[at] == 255; }
        CHECK(d && d->header.w == INK_THUMB_EDGE && black && white, "thumbnail %u contains recognisable synthetic structure", i);
        lv_obj_add_state(G.slots[i], LV_STATE_PRESSED);
        CHECK(lv_obj_get_style_outline_width(G.slots[i], LV_PART_MAIN) >= ink_ui_px(INK_STROKE), "thumbnail press has a visible outline");
        lv_obj_remove_state(G.slots[i], LV_STATE_PRESSED);
    }
    /* gallery-3: reloading by hand is offered only when the album cannot be read (D-138) */
    fx_read(); CHECK(!strstr(fx_snapshot, "gallery.reload") && strstr(fx_snapshot, "app.open"), "snapshot offers the camera and no reload while the album reads");
}
static bool fx_pictures(void) {
    return G.count == 2 && G.slots[0] && G.slots[1] && lv_obj_get_child_count(G.slots[0]) && lv_obj_get_child_count(G.slots[1]);
}
static void fx_empty_check(void) {
    if (fx_before) return;
    CHECK(G.count == 0 && fx_label(lv_screen_active(), "还没有照片"), "empty album explains how to take a photo");
    lv_obj_t *bar = lv_obj_get_child(lv_obj_get_child(G.content, -1), -1); /* gallery-3: the grid box holds the list and its page bar */
    CHECK(lv_obj_has_flag(bar, LV_OBJ_FLAG_HIDDEN) || (lv_obj_has_flag(lv_obj_get_child(bar, 0), LV_OBJ_FLAG_HIDDEN) && lv_obj_has_flag(lv_obj_get_child(bar, 2), LV_OBJ_FLAG_HIDDEN)), "empty album has no ineffective arrows");
}
static void fx_photo_check(void) {
    if (fx_before) return;
    fx_read(); CHECK(strstr(fx_snapshot, "SYNTH-checker.png") && strstr(fx_snapshot, "第") && strstr(fx_snapshot, "/2张"), "current photo is identified by filename and album position");
    lv_obj_t *label = fx_label(lv_screen_active(), "SYNTH-checker.png"); fx_fits(label, lv_screen_active());
    lv_obj_t *ask = fx_bound(lv_screen_active(), "photo.ask"), *trash = fx_bound(lv_screen_active(), "photo.trash");
    lv_area_t at, aa; if (ask && trash) { lv_obj_get_coords(trash, &at); lv_obj_get_coords(ask, &aa); }
    /* gallery-3: delete is a labelled tool at the bottom, AI a secondary action at the top, far from it */
    CHECK(ask && trash && aa.y2 < at.y1 && fx_label(trash, "删除"), "delete is a labelled bottom tool and AI a secondary top action");
    if (ask) CHECK(!lv_color_eq(lv_obj_get_style_bg_color(ask, 0), ink_ui_ink()), "AI is not the dominant black primary button");
    CHECK(strstr(fx_snapshot, fx_path) && strstr(fx_snapshot, "photo.ask") && strstr(fx_snapshot, "photo.trash"), "ask and delete refer to the actual loaded file");
}
static void fx_error_check(void) {
    if (fx_before) return;
    CHECK(fx_label(lv_screen_active(), "没插存储卡") && !fx_label(lv_screen_active(), "还没有照片"), "read error never pretends the album is empty");
    CHECK(fx_bound(lv_screen_active(), "gallery.reload"), "read failure has an in-page manual reload");
}
static void fx_no_card(void *ctx) { (void)ctx; sim_sd_set_present(false); }
static void fx_card(void *ctx) { (void)ctx; sim_sd_set_present(true); }
static void run(void *ctx) {
    (void)ctx; fx_start(); fx_ok("app.open", "{\"id\":\"gallery\"}");
    CHECK(WAIT_FOR(G.message[0] == 0, 10000), "album loads"); fx_quiet();
    if (!G.count) { fx_capture("gallery-empty"); on_ui(fx_empty_check); ink_probe_count("test.finished"); return; }
    CHECK(WAIT_FOR(fx_pictures(), 10000), "thumbnail callbacks finish"); fx_capture("gallery"); on_ui(fx_grid_check);
    uint32_t cursor = 0, selected = 0; ink_fs_entry_t e;
    while (ink_fs_index("/me", INK_FS_IMAGE, &cursor, &e) == 1)
        if (strstr(e.path, "SYNTH-checker.png")) { selected = e.id; snprintf(fx_path, sizeof fx_path, "%s", e.path); }
    CHECK(selected != 0, "locate target by name instead of host directory order");
    char args[48]; snprintf(args, sizeof args, "{\"id\":%u}", selected); fx_ok("photo.open", args);
    CHECK(WAIT_FOR(G.path[0], 10000), "photo decode finishes"); fx_capture("photo"); on_ui(fx_photo_check);
    /* gallery-3: deleting moves it to 最近删除 without a card; 撤销 on the page brings it back */
    ink_confirm_view_t card; ink_fs_stat_t st;
    CHECK(AI("photo.trash", NULL) && g_rc.status == INK_RC_DONE && ink_confirm_list(&card, 1) == 0, "deletion moves the photo without a card");
    CHECK(ink_fs_stat(fx_path, &st) == INK_FS_ERR_NOT_FOUND, "the photo left its place");
    fx_press("photo.restore"); CHECK(WAIT_FOR(ink_fs_stat(fx_path, &st) == 0, 10000), "touch 撤销 puts it back"); fx_quiet(); fx_press("shell.back");
    CHECK(WAIT_FOR(fx_pictures(), 10000), "album restored after photo");
    if (!fx_before) { fx_quiet(); fx_read(); CHECK(!strstr(fx_snapshot, "gallery.reload"), "no reload button while the album reads"); fx_ok("gallery.reload", NULL); CHECK(WAIT_FOR(fx_pictures(), 10000), "AI reload reruns thumbnail reads"); }
    vtk_event_at(vtk_now() + SIM_MS(1), fx_no_card, NULL); delay_ms(5);
    if (fx_before) { fx_ok("system.home", NULL); fx_ok("app.open", "{\"id\":\"gallery\"}"); }
    else fx_ok("gallery.reload", NULL);
    CHECK(WAIT_FOR(G.data && G.data->err == INK_FS_ERR_NO_CARD && !G.list, 10000), "missing synthetic card reports read failure"); fx_capture("gallery-error"); on_ui(fx_error_check);
    vtk_event_at(vtk_now() + SIM_MS(1), fx_card, NULL); delay_ms(5);
    if (!fx_before) CHECK(WAIT_FOR(fx_pictures(), 10000), "the album comes back by itself when the card returns");
    ink_probe_count("test.finished");
}
void ink_main(void) {
    vtk_host_call(fx_options, NULL); product_main();
    osal_task_cfg_t cfg = {.name = "ui-fix-gallery", .fn = run, .stack_bytes = 24576, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start gallery driver");
}
