/* Helpers limited to this task's presentation fixtures. */
#include <stdlib.h>
#include "ink/ui_shell.h"
#define ink_main product_main
#include "../configured_boot.c"
#undef ink_main
#include "../integration/whole_product.h"

static bool fx_before;
static int fx_scale;
static char fx_snapshot[8192];
static const char *fx_scene, *fx_wanted;
static lv_point_t fx_point;
static bool fx_found;
static char fx_variant[32];
static void fx_options(void *ctx) {
    (void)ctx; fx_before = getenv("INK_UI_FIX_BEFORE") != NULL;
    fx_scale = getenv("INK_UI_FIX_LARGE") ? 3 : 1;
    sim_copy_name(fx_variant, sizeof fx_variant, getenv("INK_UI_FIX_SCENE"));
}
static bool fx_settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void fx_quiet(void) { CHECK(WAIT_FOR(fx_settled(), 12000), "screen settles"); }
static void fx_start(void) {
    CHECK(WAIT_FOR(ink_member_space_open() && ink_ui_page_current(), 12000), "member and UI ready");
    test_start_home(); fx_quiet();
    CHECK(AI("settings.set", fx_scale == 3 ? "{\"id\":\"display.font_size\",\"value\":3}" : "{\"id\":\"display.font_size\",\"value\":1}") &&
          g_rc.status == INK_RC_DONE, "set requested font scale");
    fx_quiet(); ink_power_hold(INK_NEED_AWAKE);
}
static lv_obj_t *fx_label(lv_obj_t *obj, const char *text) {
    if (!lv_obj_is_visible(obj)) return NULL;
    if (lv_obj_check_type(obj, &lv_label_class) && strstr(lv_label_get_text(obj), text)) return obj;
    for (uint32_t i = 0; i < lv_obj_get_child_count(obj); i++) {
        lv_obj_t *found = fx_label(lv_obj_get_child(obj, (int32_t)i), text);
        if (found) return found;
    }
    return NULL;
}
static lv_obj_t *fx_bound(lv_obj_t *obj, const char *cap) {
    if (!lv_obj_is_visible(obj)) return NULL;
    const char *bound = ink_shell_bound_cap(obj);
    if (bound && !strcmp(bound, cap)) return obj;
    for (uint32_t i = 0; i < lv_obj_get_child_count(obj); i++) {
        lv_obj_t *found = fx_bound(lv_obj_get_child(obj, (int32_t)i), cap);
        if (found) return found;
    }
    return NULL;
}
static lv_obj_t *fx_exact(lv_obj_t *obj, const char *text) {
    if (!lv_obj_is_visible(obj)) return NULL;
    if (lv_obj_check_type(obj, &lv_label_class) && !strcmp(lv_label_get_text(obj), text)) return obj;
    for (uint32_t i = 0; i < lv_obj_get_child_count(obj); i++) {
        lv_obj_t *found = fx_exact(lv_obj_get_child(obj, (int32_t)i), text);
        if (found) return found;
    }
    return NULL;
}
static void fx_locate(void) {
    lv_obj_update_layout(lv_screen_active()); lv_obj_update_layout(lv_layer_sys());
    lv_obj_t *obj = fx_exact(lv_layer_sys(), fx_wanted);
    if (!obj) obj = fx_exact(lv_screen_active(), fx_wanted);
    fx_found = obj != NULL;
    if (obj) { lv_area_t a; lv_obj_get_coords(obj, &a); fx_point = (lv_point_t){(a.x1 + a.x2) / 2, (a.y1 + a.y2) / 2}; }
}
static void fx_press(const char *label) {
    fx_quiet(); fx_wanted = label; on_ui(fx_locate); CHECK(fx_found, "visible button: %s", label);
    if (fx_found) {
        double delivered = sim_metric_value("input.touch_delivery_ms");
        tap(fx_point.x, fx_point.y); fx_quiet();
        CHECK(sim_metric_value("input.touch_delivery_ms") > delivered, "touch driver received %s", label);
    }
}
static void fx_read(void) { CHECK(ink_ui_snapshot(fx_snapshot, sizeof fx_snapshot, NULL) > 0, "page snapshot"); }
static void fx_save(void *ctx) {
    (void)ctx; char path[768];
    int n = snprintf(path, sizeof path, "%s/%s.json", g_sim.out_dir, fx_scene);
    if (n < 0 || (size_t)n >= sizeof path) abort();
    FILE *f = fopen(path, "wb");
    if (!f || fputs(fx_snapshot, f) == EOF || fclose(f)) abort();
}
static void fx_picture(void *ctx) { sim_display_snapshot(ctx); }
static void fx_capture(const char *scene) {
    fx_quiet(); fx_scene = scene; on_ui(fx_read); vtk_host_call(fx_save, NULL);
    vtk_event_at(vtk_now() + SIM_MS(1), fx_picture, (void *)scene); delay_ms(5);
}
static void fx_fits(lv_obj_t *label, lv_obj_t *parent) {
    CHECK(label && parent, "text and container exist");
    if (!label || !parent) return;
    lv_obj_update_layout(parent); lv_area_t a, b; lv_obj_get_coords(label, &a); lv_obj_get_coords(parent, &b);
    CHECK(a.x1 >= b.x1 && a.x2 <= b.x2 && a.y1 >= b.y1 && a.y2 <= b.y2, "text fits container (bottom=%d limit=%d)", (int)a.y2, (int)b.y2);
    lv_point_t need;
    lv_text_get_size(&need, lv_label_get_text(label), lv_obj_get_style_text_font(label, 0),
        lv_obj_get_style_text_letter_space(label, 0), lv_obj_get_style_text_line_space(label, 0), lv_obj_get_width(label), LV_TEXT_FLAG_NONE);
    CHECK(lv_obj_get_height(label) >= need.y, "text is not clipped (height=%d need=%d)", (int)lv_obj_get_height(label), (int)need.y);
}
#define FX_MAIN(task_name) \
    void ink_main(void) { \
        vtk_host_call(fx_options, NULL); product_main(); \
        osal_task_cfg_t cfg = {.name = task_name, .fn = run, .stack_bytes = 24576, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL}; \
        CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start UI fixture"); \
    }
