/* Product UI against the simulated modem; captures never open a desktop window. */
#include <stdlib.h>
#include "ink/hal_net.h"
#include "ink/sys/display.h"
#include "ink/ui_shell.h"
#define ink_main product_main
#include "../../system/boot/main.c"
#undef ink_main
#include "../integration/whole_product.h"
static int cell_dbm;
static bool card = true, coverage = true;
static int expected_bars;
static const char *expected_label;
static lv_obj_t *find_label(lv_obj_t *root, const char *text) {
    if (lv_obj_check_type(root, &lv_label_class) && !strcmp(lv_label_get_text(root), text)) return root;
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); i++) {
        lv_obj_t *found = find_label(lv_obj_get_child(root, (int)i), text);
        if (found) return found;
    }
    return NULL;
}
static void inspect(void) {
    lv_obj_t *label = find_label(lv_screen_active(), expected_label);
    CHECK(label, "status label %s exists", expected_label);
    if (label && !strcmp(expected_label, "4G")) {
        lv_obj_t *bar = lv_obj_get_parent(label);
        int next = (int)lv_obj_get_index(label) + 1;
        for (int i = 0; i < 4; i++) {
            lv_obj_t *o = lv_obj_get_child(bar, next + i);
            CHECK(o && (lv_obj_get_style_bg_opa(o, 0) == LV_OPA_COVER) == (i < expected_bars), "signal bar %d follows modem strength", i);
            CHECK(o && lv_obj_get_width(o) > 2 * lv_obj_get_style_border_width(o, 0), "unfilled signal bars have a visible hollow centre");
        }
    }
}
static bool state(const char *id) { int64_t v = 0; return ink_state_get_int(ink_state_find(id), &v) && v; }
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void capture(void *arg) { sim_display_snapshot(arg); }
static void frame(const char *name, const char *label, int bars) {
    expected_label = label; expected_bars = bars;
    CHECK(WAIT_FOR(settled(), 10000), "display settles"); on_ui(inspect);
    vtk_event_at(vtk_now() + SIM_MS(1), capture, (void *)name); delay_ms(5);
}
static void world(void *arg) {
    (void)arg;
    sim_net_set_card(0, card, true, true);
    sim_net_env(1, coverage, cell_dbm);
    sim_net_roaming(false); /* ask the existing comm event path to query the new modem status */
}
static void change_world(bool present, bool covered, int dbm) {
    card = present; coverage = covered; cell_dbm = dbm;
    vtk_event_at(vtk_now() + SIM_MS(1), world, NULL); delay_ms(800);
}
static bool set(const char *id, int value) {
    char args[96]; snprintf(args, sizeof args, "{\"id\":\"%s\",\"value\":%d}", id, value);
    return BY_HAND("settings.set", args) && g_rc.status == INK_RC_DONE && ink_setting_get(id) == value;
}
static void run(void *arg) {
    (void)arg;
    CHECK(WAIT_FOR(ink_member_space_open(), 10000), "member opens"); test_start_home(); ink_power_hold(INK_NEED_NO_LOCK);
    CHECK(!state("cell.sim_missing") && !state("cell.registered"), "initial registration is pending");
    frame("cell-no-service", "无服务", 0);
    CHECK(set("net.prefer", 1), "prefer Wi-Fi");
    CHECK(WAIT_FOR(state("cell.registered") && state("cell.signal"), 10000), "modem signal is published");
    change_world(true, true, -69); frame("cell-wifi-4g-strong", "4G", 4);
    hal_link_status_t link; hal_net_link_status(&link); CHECK(link.type == HAL_LINK_WIFI && link.up, "Wi-Fi and cellular registration coexist");
    change_world(true, true, -99); frame("cell-wifi-4g-weak", "4G", 1);
    CHECK(set("net.airplane", 1), "airplane mode"); delay_ms(1000);
    CHECK(!state("cell.registered"), "airplane clears registration");
    CHECK(WAIT_FOR(settled(), 10000), "airplane display settles");
    vtk_event_at(vtk_now() + SIM_MS(1), capture, "cell-airplane"); delay_ms(5);
    CHECK(set("net.airplane", 0), "leave airplane"); delay_ms(1500);
    change_world(false, true, -99); CHECK(WAIT_FOR(state("cell.sim_missing"), 5000), "missing SIM is explicit");
    frame("cell-no-sim", "无卡", 0);
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name = "cell-status-test", .fn = run, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    if (osal_task_create(&cfg, NULL) != OSAL_OK) osal_panic("cell UI test");
}
