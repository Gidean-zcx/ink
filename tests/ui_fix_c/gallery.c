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
    fx_read(); CHECK(strstr(fx_snapshot, "gallery.reload") && strstr(fx_snapshot, "app.open"), "snapshot offers manual reload and camera actions");
}
static bool fx_pictures(void) {
    return G.count == 2 && G.slots[0] && G.slots[1] && lv_obj_get_child_count(G.slots[0]) && lv_obj_get_child_count(G.slots[1]);
}
static void fx_empty_check(void) {
    if (fx_before) return;
    CHECK(G.count == 0 && fx_label(lv_screen_active(), "还没有照片"), "empty album explains how to take a photo");
    lv_obj_t *bar = lv_obj_get_child(G.box, -1);
    CHECK(lv_obj_has_flag(lv_obj_get_child(bar, 0), LV_OBJ_FLAG_HIDDEN) && lv_obj_has_flag(lv_obj_get_child(bar, 2), LV_OBJ_FLAG_HIDDEN), "empty album has no ineffective arrows");
}
static void fx_photo_check(void) {
    if (fx_before) return;
    fx_read(); CHECK(strstr(fx_snapshot, "SYNTH-checker.png") && strstr(fx_snapshot, "第") && strstr(fx_snapshot, "/2张"), "current photo is identified by filename and album position");
    lv_obj_t *label = fx_label(lv_screen_active(), "SYNTH-checker.png"); fx_fits(label, lv_screen_active());
    lv_obj_t *ask = fx_bound(lv_screen_active(), "photo.ask"), *trash = fx_bound(lv_screen_active(), "file.delete");
    CHECK(ask && trash && lv_obj_get_y(trash) < lv_obj_get_y(ask), "delete is a header tool and AI a secondary bottom action");
    if (ask) CHECK(!lv_color_eq(lv_obj_get_style_bg_color(ask, 0), ink_ui_ink()), "AI is not the dominant black primary button");
    CHECK(strstr(fx_snapshot, fx_path) && strstr(fx_snapshot, "photo.ask") && strstr(fx_snapshot, "file.delete"), "ask and delete refer to the actual loaded file");
}
static void fx_error_check(void) {
    if (fx_before) return;
    CHECK(fx_label(lv_screen_active(), "暂时读不了照片") && !fx_label(lv_screen_active(), "还没有照片"), "read error never pretends the album is empty");
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
    CHECK(AI("file.delete", G.delete_args) && g_rc.status == INK_RC_PENDING_CONFIRMATION, "deletion still requires confirmation");
    unsigned token = 0; CHECK(sscanf(g_rc.object, "confirm:%u", &token) == 1, "delete confirmation token");
    CHECK(ink_confirm_decide(token, false, INK_SRC_TOUCH) == 0, "cancel deletion"); fx_quiet(); fx_press("shell.back");
    CHECK(WAIT_FOR(fx_pictures(), 10000), "album restored after photo");
    if (!fx_before) { fx_press("gallery.reload"); CHECK(WAIT_FOR(fx_pictures(), 10000), "touch reload reruns thumbnail reads"); fx_ok("gallery.reload", NULL); }
    vtk_event_at(vtk_now() + SIM_MS(1), fx_no_card, NULL); delay_ms(5);
    if (fx_before) { fx_ok("system.home", NULL); fx_ok("app.open", "{\"id\":\"gallery\"}"); }
    else fx_ok("gallery.reload", NULL);
    CHECK(WAIT_FOR(strstr(G.message, "暂时读不了") != NULL, 10000), "missing synthetic card reports read failure"); fx_capture("gallery-error"); on_ui(fx_error_check);
    vtk_event_at(vtk_now() + SIM_MS(1), fx_card, NULL); delay_ms(5);
    if (!fx_before) { fx_press("gallery.reload"); CHECK(WAIT_FOR(fx_pictures(), 10000), "same-page manual reload recovers after card returns"); }
    ink_probe_count("test.finished");
}
void ink_main(void) {
    vtk_host_call(fx_options, NULL); product_main();
    osal_task_cfg_t cfg = {.name = "ui-fix-gallery", .fn = run, .stack_bytes = 24576, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start gallery driver");
}
