/* Real boot, registry and camera page with only boot detection made absent. */
#include <string.h>
#include "ink/hal_camera.h"
static bool absent_info(hal_camera_info_t *out) { *out = (hal_camera_info_t){0}; return false; }
#define hal_camera_info absent_info
#include "../../system/services/camera/camera_service.c"
#undef hal_camera_info
#include "../../system/apps/camera/camera.c"
#define ink_main product_main
#include "../../system/boot/main.c"
#undef ink_main
#include "../integration/whole_product.h"
static bool shot;
static lv_point_t back_point;
static bool back_found, label_found;
static const char *message;
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void quiet(void) { CHECK(WAIT_FOR(settled(), 15000), "page settles"); }
static void inspect_obj(lv_obj_t *o) {
    if (!lv_obj_is_visible(o)) return;
    const char *cap = ink_shell_bound_cap(o);
    if (cap && !strcmp(cap, "shell.back")) {
        lv_area_t a; lv_obj_get_coords(o, &a);
        back_point = (lv_point_t){(a.x1 + a.x2) / 2, (a.y1 + a.y2) / 2}; back_found = true;
    }
    CHECK(!cap || (strcmp(cap, "camera.photo") && strcmp(cap, "camera.scan") && strcmp(cap, "camera.preview")), "absent camera has no capture controls");
    if (lv_obj_check_type(o, &lv_label_class) && !strcmp(lv_label_get_text(o), message)) {
        label_found = true;
        lv_area_t a; lv_obj_get_content_coords(o, &a);
        lv_point_t size;
        lv_text_get_size(&size, message, lv_obj_get_style_text_font(o, LV_PART_MAIN), 0, 0, lv_area_get_width(&a), LV_TEXT_FLAG_NONE);
        CHECK(size.x <= lv_area_get_width(&a) && size.y <= lv_area_get_height(&a), "camera notice is not clipped");
        CHECK(a.x1 >= 0 && a.y1 >= INK_STATUS_BAR && a.x2 < INK_CANVAS_W && a.y2 < INK_CANVAS_H - INK_CIRCLE_APP, "camera notice is on screen above the close button");
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) inspect_obj(lv_obj_get_child(o, (int)i));
}
static void inspect(void) {
    back_found = label_found = false;
    lv_obj_update_layout(lv_screen_active()); inspect_obj(lv_screen_active());
    CHECK(label_found && back_found && !timer && !view, "notice and back button replace preview and timer");
    CHECK(ink_ui_snapshot(g_snap, sizeof g_snap, NULL) > 0 && strstr(g_snap, message) && strstr(g_snap, "shell.back") && !strstr(g_snap, "camera.photo"), "AI sees the same notice and back action");
}
static void frame(void *arg) { (void)arg; CHECK(settled(), "frame is settled"); sim_display_snapshot(message[0] == 'N' ? "camera-absent-en" : "camera-absent-zh"); shot = true; }
static void run(void *arg) {
    (void)arg;
    CHECK(WAIT_FOR(ink_cap_find("app.open") && ink_ui_page_current(), 15000), "boot ready"); test_start_home(); quiet();
    CHECK(!ink_cap_find("camera.capture") && !ink_cap_find("camera.scan"), "failed detection exposes no hardware capture capability");
    for (int en = 0; en < 2; en++) {
        CHECK(BY_HAND("settings.set", en ? "{\"id\":\"system.language\",\"value\":1}" : "{\"id\":\"system.language\",\"value\":0}") && g_rc.status == INK_RC_DONE, "set interface language"); quiet();
        message = en ? "No camera found" : "没有找到摄像头";
        CHECK(BY_HAND("app.open", "{\"id\":\"camera\"}") && g_rc.status == INK_RC_DONE, "camera opens even when hardware is absent"); quiet();
        CHECK(!strcmp(ink_ui_page_name(), "Camera"), "camera stays on its own explanation page"); on_ui(inspect);
        shot = false; vtk_event_at(vtk_now() + SIM_MS(1), frame, NULL); CHECK(WAIT_FOR(shot, 2000), "save explanation screenshot");
        CHECK(AI("camera.photo", NULL) && g_rc.status == INK_RC_FAILED, "AI shutter reports missing camera without entering the service");
        tap(back_point.x, back_point.y); quiet();
        CHECK(strcmp(ink_ui_page_name(), "Camera"), "physical close returns to the previous page");
    }
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name = "absent-camera", .fn = run, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start test driver");
}
