/* These fixtures exercise only page presentation with synthetic service data. */
#include "../services_finish/product.h"
static int fx_scale;
static const char *fx_wanted;
static lv_area_t fx_area;
static lv_obj_t *fx_label;
static bool fx_captured;
static void fx_options(void *arg) {
    (void)arg;
    const char *s = getenv("INK_UI_FIX_SCALE");
    fx_scale = s && !strcmp(s, "120") ? 3 : 1;
}
static bool fx_settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void fx_quiet(void) { CHECK(WAIT_FOR(fx_settled(), 15000), "fixture display settles"); }
static void fx_locate(lv_obj_t *o) {
    if (!lv_obj_is_visible(o)) return;
    if (lv_obj_check_type(o, &lv_label_class) && !strcmp(lv_label_get_text(o), fx_wanted)) {
        fx_label = o;
        lv_obj_get_coords(o, &fx_area);
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) fx_locate(lv_obj_get_child(o, (int)i));
}
static void fx_find(void) {
    fx_label = NULL;
    lv_obj_update_layout(lv_screen_active());
    lv_obj_update_layout(lv_layer_sys());
    fx_locate(lv_screen_active());
    fx_locate(lv_layer_sys());
}
static void fx_press(const char *label) {
    fx_quiet(); fx_wanted = label; on_ui(fx_find);
    CHECK(fx_label, "visible touch action: %s", label);
    if (fx_label) tap((fx_area.x1 + fx_area.x2) / 2, (fx_area.y1 + fx_area.y2) / 2);
}
static void fx_read(void) { read_screen(); }
static void fx_frame(void *arg) { sim_display_snapshot(arg); fx_captured = true; }
static void fx_capture(const char *name) {
    fx_quiet(); on_ui(fx_read);
    fx_captured = false;
    vtk_event_at(vtk_now() + SIM_MS(1), fx_frame, (void *)name);
    CHECK(WAIT_FOR(fx_captured, 2000), "capture %s", name);
}
static void fx_start(void) {
    vtk_host_call(fx_options, NULL);
    test_start_home();
    ink_receipt_t rc;
    CHECK(ink_setting_set("display.font_size", fx_scale, INK_SRC_TOUCH, &rc) == INK_RC_DONE, "select real font size");
    fx_quiet();
}
static void fx_task(osal_task_fn fn) {
    osal_task_cfg_t cfg = {.name = "ui-fix-fixture", .fn = fn, .stack_bytes = 16384,
        .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start bounded page fixture");
}
