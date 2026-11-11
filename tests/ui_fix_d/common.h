/* Headless presentation checks shared only by ui-fix-d's three drivers. */
#define ink_main product_main
#include "../../system/boot/main.c"
#undef ink_main
#include "../integration/whole_product.h"
#include <stdlib.h>

#ifdef INK_UI_FIX_D_BEFORE
static const bool u_before = true;
#else
static const bool u_before = false;
#endif
#define U_CHECK(cond, ...) CHECK((cond), __VA_ARGS__)
static int u_size;
static const char *u_picture;
static bool u_captured;
static bool u_settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void u_quiet(void) { CHECK(WAIT_FOR(u_settled(), 15000), "ui-fix-d display settles"); }
static void u_command(const char *cap, const char *args) {
    CHECK(BY_HAND(cap, args) && g_rc.status == INK_RC_DONE, "%.40s: %d %.100s", cap, g_rc.status, g_rc.say);
    u_quiet();
}
static void u_environment(void *ctx) { (void)ctx; u_size = getenv("INK_UI_FIX_D_LARGE") ? 3 : 1; }
static void u_start(void) {
    vtk_host_call(u_environment, NULL);
    CHECK(WAIT_FOR(ink_member_space_open(), 10000), "member space opens");
    test_start_home(); ink_power_hold(INK_NEED_AWAKE);
    u_command("settings.set", u_size == 3 ? "{\"id\":\"display.font_size\",\"value\":3}" : "{\"id\":\"display.font_size\",\"value\":1}");
    u_command("system.home", NULL);
}
static lv_obj_t *u_label(lv_obj_t *root, const char *text) {
    if (!lv_obj_is_visible(root)) return NULL;
    if (lv_obj_check_type(root, &lv_label_class) && !strcmp(lv_label_get_text(root), text)) return root;
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); i++) {
        lv_obj_t *found = u_label(lv_obj_get_child(root, (int32_t)i), text);
        if (found) return found;
    }
    return NULL;
}
static lv_obj_t *u_cap(lv_obj_t *root, const char *cap) {
    if (!lv_obj_is_visible(root)) return NULL;
    const char *bound = ink_shell_bound_cap(root);
    if (bound && !strcmp(bound, cap)) return root;
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); i++) {
        lv_obj_t *found = u_cap(lv_obj_get_child(root, (int32_t)i), cap);
        if (found) return found;
    }
    return NULL;
}
static lv_obj_t *u_textarea(lv_obj_t *root) {
    if (lv_obj_check_type(root, &lv_textarea_class)) return root;
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); i++) {
        lv_obj_t *found = u_textarea(lv_obj_get_child(root, (int32_t)i));
        if (found) return found;
    }
    return NULL;
}
static lv_area_t u_rect(lv_obj_t *obj) {
    lv_area_t r = {0};
    if (obj) { lv_obj_update_layout(obj); lv_obj_get_coords(obj, &r); }
    return r;
}
static bool u_primary(lv_obj_t *label) {
    return label && lv_color_eq(lv_obj_get_style_bg_color(lv_obj_get_parent(label), 0), ink_ui_ink());
}
static void u_frame(void *ctx) { sim_display_snapshot(ctx); u_captured = true; }
static void u_capture(const char *name) {
    u_quiet(); u_picture = name; u_captured = false;
    vtk_event_at(vtk_now() + SIM_MS(1), u_frame, (void *)u_picture);
    CHECK(WAIT_FOR(u_captured, 2000), "capture %.50s", name);
}
static void u_launch(void (*run)(void *)) {
    product_main();
    osal_task_cfg_t cfg = {.name = "ui-fix-d-test", .fn = run, .stack_bytes = 16384,
        .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start ui-fix-d driver");
}
