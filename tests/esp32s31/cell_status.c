/* Product UI against the simulated modem; captures never open a desktop window. */
#include <stdlib.h>
#include "ink/hal_net.h"
#include "ink/sys/display.h"
#include "ink/ui_shell.h"
static bool readonly_fixture;
#define hal_cell_status ink_test_base_cell_status
#include "../../ports/sim/hal/hal_cellular_sim.c"
#undef hal_cell_status
bool hal_cell_status(hal_cell_status_t *out) {
    bool ok = ink_test_base_cell_status(out);
    out->config_readonly = __atomic_load_n(&readonly_fixture, __ATOMIC_ACQUIRE);
    return ok;
}
#define ink_main product_main
#include "../../system/boot/main.c"
#undef ink_main
#include "../integration/whole_product.h"
static int cell_dbm;
static bool card = true, coverage = true;
static int expected_bars;
static bool readonly_visible;
static bool captured;
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
static void capture(void *arg) { sim_display_snapshot(arg); captured = true; }
static void frame(const char *name, const char *label, int bars) {
    expected_label = label; expected_bars = bars;
    CHECK(WAIT_FOR(settled(), 10000), "display settles"); on_ui(inspect);
    captured = false;
    vtk_event_at(vtk_now() + SIM_MS(1), capture, (void *)name);
    CHECK(WAIT_FOR(captured, 1000), "status frame was captured");
}
static void world(void *arg) {
    (void)arg;
    sim_net_set_card(0, card, true, true);
    sim_net_env(1, coverage, cell_dbm);
    sim_net_roaming(false); /* ask the existing comm event path to query the new modem status */
}
static bool world_ready(void) {
    int64_t dbm = 0;
    return ink_state_get_int(ink_state_find("cell.signal"), &dbm) && dbm == cell_dbm &&
        state("cell.sim_missing") == !card && state("cell.registered") == card;
}
static void change_world(bool present, bool covered, int dbm) {
    card = present; coverage = covered; cell_dbm = dbm;
    vtk_event_at(vtk_now() + SIM_MS(1), world, NULL);
    CHECK(WAIT_FOR(world_ready(), 5000), "modem reports the requested card and signal state");
}
static void inspect_readonly(void) {
    readonly_visible = ink_ui_snapshot(g_result, sizeof g_result, NULL) > 0 && strstr(g_result, "暂不能更改") &&
        !strstr(g_result, "net.apn_set") && !strstr(g_result, "shell.mobile_edit");
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
    CHECK(set("net.airplane", 1), "airplane mode");
    CHECK(WAIT_FOR(!state("cell.registered"), 5000), "airplane clears registration");
    CHECK(WAIT_FOR(settled(), 10000), "airplane display settles");
    captured = false; vtk_event_at(vtk_now() + SIM_MS(1), capture, "cell-airplane");
    CHECK(WAIT_FOR(captured, 1000), "airplane frame was captured");
    CHECK(set("net.airplane", 0), "leave airplane");
    CHECK(WAIT_FOR(state("cell.registered"), 10000), "radio registers after leaving airplane mode");
    change_world(false, true, -99); CHECK(WAIT_FOR(state("cell.sim_missing"), 5000), "missing SIM is explicit");
    frame("cell-no-sim", "无卡", 0);
    CHECK(BY_HAND("shell.mobile", NULL) && g_rc.status == INK_RC_DONE, "open readonly mobile page");
    __atomic_store_n(&readonly_fixture, true, __ATOMIC_RELEASE);
    ink_state_set_bool(ink_state_find("cell.config_readonly"), true); /* S31 metadata arrives while the page is open. */
    CHECK(WAIT_FOR(({ on_ui(inspect_readonly); readonly_visible; }), 5000), "readonly=%d page=%.24s snapshot=%.150s", state("cell.config_readonly"), ink_ui_page_name(), g_result);
    CHECK(BY_HAND("shell.mobile_edit", NULL) && g_rc.status == INK_RC_REJECTED && strstr(g_rc.say, "暂不能更改"), "local readonly edit is explicitly refused");
    CHECK(AI("shell.mobile_edit", NULL) && g_rc.status == INK_RC_INVALID && strstr(g_rc.detail, "not offered"), "the page-private edit capability is not offered to AI");
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name = "cell-status-test", .fn = run, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    if (osal_task_create(&cfg, NULL) != OSAL_OK) osal_panic("cell UI test");
}
