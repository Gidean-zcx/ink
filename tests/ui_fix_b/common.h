/* Headless presentation fixtures; service integration remains in the existing suites. */
#include "ink/ui_shell.h"
#define ink_main product_main
#include "../../system/boot/main.c"
#undef ink_main
#include "../integration/whole_product.h"
#include <stdlib.h>

static char test_snapshot[8192], test_name[80];
static bool test_big;
static void test_read_font(void *arg) {
    (void)arg; const char *font = getenv("INK_UI_FIX_FONT"); test_big = font && !strcmp(font, "120");
}
static lv_obj_t *test_found;
static const char *test_wanted;
static bool test_settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void test_quiet(void) { CHECK(WAIT_FOR(test_settled(), 10000), "presentation settles"); }
static void test_find_at(lv_obj_t *o) {
    if (lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN)) return;
    if (lv_obj_check_type(o, &lv_label_class) && !strcmp(lv_label_get_text(o), test_wanted)) test_found = o;
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) test_find_at(lv_obj_get_child(o, (int32_t)i));
}
static lv_obj_t *test_label(lv_obj_t *root, const char *text) {
    test_found = NULL; test_wanted = text; lv_obj_update_layout(root); test_find_at(root); return test_found;
}
static bool test_fits(lv_obj_t *o) {
    if (!o) return false;
    lv_area_t a, clip; lv_obj_get_coords(o, &a);
    for (lv_obj_t *p = lv_obj_get_parent(o); p; p = lv_obj_get_parent(p)) {
        lv_obj_get_coords(p, &clip);
        if (a.x1 < clip.x1 || a.x2 > clip.x2 || a.y1 < clip.y1 || a.y2 > clip.y2) return false;
    }
    return a.x1 >= 0 && a.y1 >= 0 && a.x2 < ink_ui_width() && a.y2 < ink_ui_height();
}
static void test_snapshot_ui(void) {
    CHECK(ink_ui_snapshot(test_snapshot, sizeof test_snapshot, NULL) > 0, "page snapshot exports");
}
static void test_capture(void *arg) { sim_display_snapshot(arg); }
static void test_frame(const char *name) {
    test_quiet(); snprintf(test_name, sizeof test_name, "%s", name);
    on_ui(test_snapshot_ui);
    vtk_event_at(vtk_now() + SIM_MS(1), test_capture, test_name); delay_ms(20);
}
static void test_init(void) {
    CHECK(WAIT_FOR(ink_member_space_open(), 10000), "member space opens"); test_start_home(); test_quiet();
    vtk_host_call(test_read_font, NULL);
    char args[80]; snprintf(args, sizeof args, "{\"id\":\"display.font_size\",\"value\":%d}", test_big ? 3 : 1);
    CHECK(AI("settings.set", args) && g_rc.status == INK_RC_DONE, "choose screenshot font size"); test_quiet();
}
static void test_run(void *arg);
void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name = "uifixb", .fn = test_run, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL,
        .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    if (osal_task_create(&cfg, NULL) != OSAL_OK) osal_panic("UI fix B fixture");
}
