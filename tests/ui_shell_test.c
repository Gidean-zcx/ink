/* Shell priority, restoration and refresh regressions on the real product. */
#include <stdio.h>
#include <string.h>
#include "ink/sys/confirm.h"
#include "ink/sys/interrupt.h"
#include "ink/sys/notify.h"
#include "ink/sys/settings.h"
#include "ink/sys/textfield.h"
#include "ink/ui_shell.h"

#define ink_main product_main
#include "../system/boot/main.c"
#undef ink_main
#include "integration/whole_product.h"

static uint8_t *before;
static size_t frame_bytes;
static bool same;
static int hit, interrupt_id;
static lv_obj_t *field;
static void hit_run(ink_call_t *c) { (void)c; hit++; }
static const ink_cap_t CAPS[] = {
    {.id = "test.ui_hit", .title = "测试点击", .effect = INK_FX_NAVIGATE, .child_ok = true, .while_locked = true, .run = hit_run},
    {.id = "test.ui_confirm", .title = "测试高风险动作", .effect = INK_FX_LOCAL_DESTRUCTIVE, .child_ok = true, .run = hit_run}
};
static void save_frame(void) {
    int w, h; ink_display_native_size(&w, &h); frame_bytes = (size_t)w * h;
    if (!before) before = osal_malloc(frame_bytes, OSAL_MEM_EXTERNAL);
    ink_display_lock(); memcpy(before, ink_display_fb(), frame_bytes); ink_display_unlock();
}
static void staged_redraw(void) { ink_ui_stage_next(); lv_obj_invalidate(lv_screen_active()); }
static void compare_frame(void) {
    ink_display_lock(); same = !memcmp(before, ink_display_fb(), frame_bytes); ink_display_unlock();
}
static void raise_call(void) {
    ink_interrupt_t it = {.kind = INK_INT_CALL, .nactions = 1};
    snprintf(it.title, sizeof it.title, "测试来电");
    it.actions[0].label = "测试接听"; it.actions[0].cap = "test.ui_hit";
    interrupt_id = ink_interrupt_raise(&it);
}
static void focus_text(lv_obj_t *screen, void *arg) {
    (void)arg;
    ink_text_cfg_t cfg = {.type = INK_TEXT_PLAIN, .max = 128, .hint = "测试"};
    field = ink_textfield_create(screen, &cfg, NULL, NULL, true);
    lv_obj_set_pos(field, INK_MARGIN, INK_PRIMARY);
}
static int input_snapshot(char *buf, size_t cap) { return snprintf(buf, cap, "测试输入页面"); }
static const ink_activity_t INPUT = {.name = "test.input", .create = focus_text, .snapshot = input_snapshot};
static void open_input(void) { ink_ui_push(&INPUT, NULL); }
static void end_input(void) { ink_text_blur(0); ink_ui_home(); }
static void swipe(int x0, int y0, int x1, int y1) {
    hw(HW_PRESS, x0, y0); delay_ms(100); hw(HW_PRESS, x1, y1); delay_ms(100); hw(HW_RELEASE, x1, y1); delay_ms(400);
}
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void quiet(void) { CHECK(WAIT_FOR(settled(), 10000), "the panel settles"); }
static void snapshot_event(void *name) { sim_display_snapshot(name); }
static bool same_page_content(const char *a, const char *b) {
    const char *av = strstr(a, "\"page_version\":"), *bv = strstr(b, "\"page_version\":");
    if (!av || !bv || av - a != bv - b) return false;
    const char *at = strchr(av, ','), *bt = strchr(bv, ',');
    return at && bt && !strncmp(a, b, (size_t)(av - a)) && !strcmp(at, bt);
}
static void panel(const char *name) { vtk_event_at(vtk_now() + SIM_MS(1), snapshot_event, (void *)name); delay_ms(5); }
static bool clock_tile;
static void find_clock_in(lv_obj_t *root) {
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); i++) {
        lv_obj_t *child = lv_obj_get_child(root, (int32_t)i);
        if (lv_obj_has_flag(child, LV_OBJ_FLAG_HIDDEN)) continue;
        if (lv_obj_check_type(child, &lv_label_class) && !strcmp(lv_label_get_text(child), "时钟")) clock_tile = true;
        find_clock_in(child);
    }
}
static void find_clock(void) { clock_tile = false; find_clock_in(lv_screen_active()); }
static lv_point_t at;
static bool label_at(lv_obj_t *root, const char *text) {
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); i++) {
        lv_obj_t *child = lv_obj_get_child(root, (int32_t)i);
        if (lv_obj_check_type(child, &lv_label_class) && !strcmp(lv_label_get_text(child), text)) {
            lv_area_t a; lv_obj_get_coords(child, &a); at = (lv_point_t){(a.x1 + a.x2) / 2, (a.y1 + a.y2) / 2};
            return true;
        }
        if (label_at(child, text)) return true;
    }
    return false;
}
static const char *wanted_label;
static bool wanted_seen;
static void find_wanted(void) { lv_obj_update_layout(lv_screen_active()); wanted_seen = label_at(lv_screen_active(), wanted_label); }
static void find_in_panel(void) { lv_obj_update_layout(lv_layer_sys()); wanted_seen = label_at(lv_layer_sys(), wanted_label); }
static bool tap_label(const char *text) { wanted_label = text; on_ui(find_wanted); if (wanted_seen) tap(at.x, at.y); return wanted_seen; }
static bool field_at(lv_obj_t *root) {
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); i++) {
        lv_obj_t *child = lv_obj_get_child(root, (int32_t)i);
        if (lv_obj_check_type(child, &lv_textarea_class)) {
            lv_area_t a; lv_obj_get_coords(child, &a); at = (lv_point_t){(a.x1 + a.x2) / 2, (a.y1 + a.y2) / 2};
            return true;
        }
        if (field_at(child)) return true;
    }
    return false;
}
static void find_field(void) { lv_obj_update_layout(lv_screen_active()); wanted_seen = field_at(lv_screen_active()); }
static void clock_page(void) { /* page on through all apps until the Clock row is shown */
    on_ui(find_clock);
    for (int i = 0; i < 6 && !clock_tile; i++) { CHECK(BY_HAND("shell.apps_next", NULL) && g_rc.status == INK_RC_DONE, "page on to the Clock row"); quiet(); on_ui(find_clock); }
}

static void test_task(void *arg) {
    (void)arg; delay_ms(1800); quiet();
    CHECK(BY_HAND("shell.control", NULL) && g_rc.status == INK_RC_DONE, "control through the bus"); quiet();
    CHECK(!strcmp(ink_ui_top_name(), "Control"), "control is an overlay");
    int64_t before_light = ink_setting_get("display.light");
    /* control-1: the brightness slider is in the second stage; find it instead of assuming its row */
    CHECK(BY_HAND("shell.more", NULL) && g_rc.status == INK_RC_DONE, "the second stage shows the sliders"); quiet();
    wanted_label = "亮度"; on_ui(find_in_panel); CHECK(wanted_seen, "the brightness slider is on the panel");
    swipe(40, at.y, 500, at.y);
    CHECK(!strcmp(ink_ui_top_name(), "Control") && ink_setting_get("display.light") != before_light, "slider owns a swipe starting in the left edge");
    ink_probe_count("test.slider_priority");
    key(HAL_KEY_HOME); quiet(); CHECK(!strcmp(ink_ui_top_name(), "home"), "HOME closes the overlay first");
    swipe(300, 20, 300, 300); quiet(); CHECK(!strcmp(ink_ui_top_name(), "Control"), "top edge before foreground gestures");
    key(HAL_KEY_HOME); quiet();
    CHECK(BY_HAND("shell.settings", NULL) && g_rc.status == INK_RC_DONE, "settings opens"); quiet();
    swipe(20, 400, 250, 400); quiet(); CHECK(!strcmp(ink_ui_page_name(), "home"), "left edge returns one level");
    swipe(500, 500, 200, 500); quiet(); CHECK(!strcmp(ink_shell_board(), "Main"), "the one page does not turn (D-135)");
    CHECK(tap_label("全部应用"), "全部应用 is on the home"); quiet();
    CHECK(!strcmp(ink_shell_board(), "Apps"), "全部应用 opens the second page");
    swipe(500, 500, 200, 500); quiet(); on_ui(read_screen);
    CHECK(!strcmp(ink_shell_board(), "Apps") && strstr(g_snap, "工具页=2/"), "foreground swipe turns only on release");
    clock_page();
    CHECK(clock_tile, "Apps shows its Clock tile on the appropriate page");
    on_ui(find_field); CHECK(wanted_seen, "the search field is on the page"); if (wanted_seen) tap(at.x, at.y);
    CHECK(ink_text_focused(), "application search takes the input focus");
    CHECK(AI("text.insert", "{\"text\":\"阅读\"}") && g_rc.status == INK_RC_DONE, "the AI can fill the same application query");
    CHECK(BY_HAND("text.submit", NULL) && g_rc.status == INK_RC_DONE, "the search key submits through the text capability");
    quiet(); on_ui(read_screen);
    CHECK(!strcmp(ink_ui_page_name(), "Apps") && strstr(g_snap, "查找=阅读") && strstr(g_snap, "阅读（reader）") && !ink_text_focused(),
          "the real query lists the matching apps in place and closes its keyboard");
    on_ui(find_clock); CHECK(!clock_tile, "the nonmatching Clock tile is absent from the rendered tree");
    CHECK(tap_label("搜索全部内容") && wanted_seen, "the search of everything follows the matches"); quiet();
    on_ui(read_screen); CHECK(!strcmp(ink_ui_page_name(), "Search") && strstr(g_snap, "阅读"), "it opens global Search with the same words");
    CHECK(BY_HAND("shell.back", NULL) && g_rc.status == INK_RC_DONE, "global search returns to the original Apps page"); quiet();
    CHECK(BY_HAND("shell.search", "{\"query\":\"\"}") && g_rc.status == INK_RC_DONE, "clearing the same query restores Apps"); quiet();
    clock_page();
    CHECK(clock_tile, "clearing the query restores the rendered tile");
    key(HAL_KEY_PREV); quiet();
    on_ui(open_input); quiet();
    swipe(300, 1200, 300, 900); quiet(); CHECK(ink_text_focused() && !strcmp(ink_ui_page_name(), "test.input"), "keyboard owns the bottom edge");
    ink_probe_count("test.keyboard_priority");
    on_ui(end_input); quiet();

    /* Opening a call invalidates a finger already down over its future action. */
    on_ui(save_frame); panel("before-call");
    hw(HW_PRESS, INK_CANVAS_W / 2, INK_CANVAS_H - INK_MARGIN - INK_PRIMARY / 2);
    on_ui(raise_call); delay_ms(500);
    hw(HW_RELEASE, INK_CANVAS_W / 2, INK_CANVAS_H - INK_MARGIN - INK_PRIMARY / 2); quiet();
    CHECK(hit == 0, "a finger down before an interruption cannot accept it");
    key(HAL_KEY_HOME); CHECK(!strcmp(ink_ui_top_name(), "interrupt"), "a call owns HOME");
    hw(HW_KEY_DOWN, HAL_KEY_AI, 0); hw(HW_KEY_UP, HAL_KEY_AI, 0);
    CHECK(!strcmp(ink_ui_top_name(), "interrupt"), "a call owns AI");
    ink_interrupt_end(interrupt_id); quiet(); on_ui(compare_frame); panel("after-call");
    CHECK(same, "interrupt close restores every framebuffer byte");

    CHECK(AI("test.ui_confirm", NULL) && g_rc.status == INK_RC_PENDING_CONFIRMATION, "high risk is a real confirmation");
    quiet(); on_ui(save_frame); panel("before-confirm-call"); on_ui(raise_call); quiet();
    ink_interrupt_end(interrupt_id); quiet(); on_ui(compare_frame); panel("after-confirm-call");
    CHECK(same && !strcmp(ink_ui_top_name(), "confirm"), "the confirmation returns pixel for pixel after a call");
    key(HAL_KEY_HOME); quiet(); CHECK(!strcmp(ink_ui_top_name(), "home") && hit == 0, "HOME cancels confirmation without executing it");

    CHECK(BY_HAND("shell.apps", NULL) && g_rc.status == INK_RC_DONE, "open all apps, the pages that turn (D-135)"); quiet();
    double clean = sim_metric_value("display.dispatch.FLASH_UI+clean");
    key(HAL_KEY_NEXT); quiet();
    CHECK(sim_metric_value("display.dispatch.FLASH_UI+clean") > clean, "an all-apps page turn is all new content: a clean refresh, no ghost (D-108)");
    double preview = sim_metric_value("display.two_phase.preview"), gray = sim_metric_value("display.two_phase.gray");
    on_ui(staged_redraw); quiet();
    CHECK(sim_metric_value("display.two_phase.preview") > preview && sim_metric_value("display.two_phase.gray") > gray, "a staged frame has a binary preview and gray refinement");
    key(HAL_KEY_PREV); quiet();
    for (int i = 0; i < 8; i++) {
        char title[48]; snprintf(title, sizeof title, "测试通知 %d", i);
        ink_notify_t note = {.title = title, .body = "分页测试", .prio = INK_NOTIFY_LOW, .src = INK_SRC_SYSTEM};
        CHECK(ink_notify_post(&note, NULL) == 0, "post synthetic notification %d", i);
    }
    CHECK(BY_HAND("shell.notifications", NULL) && g_rc.status == INK_RC_DONE, "open the real notification list"); quiet();
    on_ui(read_screen); char first[sizeof g_snap]; snprintf(first, sizeof first, "%s", g_snap);
    CHECK(BY_HAND("shell.notify_next", NULL) && g_rc.status == INK_RC_DONE, "the notification page advances through its capability"); quiet();
    on_ui(read_screen); CHECK(strcmp(first, g_snap), "the next page shows different real records");
    CHECK(BY_HAND("shell.notify_prev", NULL) && g_rc.status == INK_RC_DONE, "the notification page returns through the same capability"); quiet();
    on_ui(read_screen); CHECK(same_page_content(first, g_snap), "the previous records, actions and pagination are restored");
    CHECK(strcmp(first, g_snap), "returning to the same records gives a fresh page version");
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main(); test_start_home(); ink_cap_register(CAPS, sizeof CAPS / sizeof *CAPS);
    osal_task_cfg_t cfg = {.name = "ui-test", .fn = test_task, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    if (osal_task_create(&cfg, NULL) != OSAL_OK) osal_panic("UI test task");
}
