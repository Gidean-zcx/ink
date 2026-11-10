/* Compare the current network page's actual button with its AI action. */
#include "ink/ui_shell.h"
#define ink_main product_main
#include "../../system/boot/main.c"
#undef ink_main
#include "../integration/whole_product.h"

static char page[4096];
static bool button;
static int x, y;
static void labels(lv_obj_t *obj) {
    if (!lv_obj_is_visible(obj)) return;
    if (lv_obj_check_type(obj, &lv_label_class) && !strcmp(lv_label_get_text(obj), "忘掉这个网络")) {
        for (lv_obj_t *p = obj; p; p = lv_obj_get_parent(p)) {
            const char *cap = ink_shell_bound_cap(p);
            if (!cap || strcmp(cap, "wifi.forget") || lv_obj_has_state(p, LV_STATE_DISABLED)) continue;
            lv_area_t a; lv_obj_get_coords(p, &a);
            button = true;
            x = (a.x1 + a.x2) / 2;
            y = (a.y1 + a.y2) / 2;
        }
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(obj); i++) labels(lv_obj_get_child(obj, (int32_t)i));
}
static void inspect(void) {
    CHECK(ink_ui_snapshot(page, sizeof page, NULL) > 0, "network snapshot fits");
    CHECK(ink_ui_context_snapshot(g_snap, sizeof g_snap, NULL) > 0, "network production AI context fits");
    lv_obj_update_layout(lv_screen_active());
    button = false;
    x = y = -1;
    labels(lv_screen_active());
}
static void quiet(void) { CHECK(WAIT_FOR(!ink_ui_busy() && !ink_display_busy(), 10000), "network page settles"); }
static void add_ap(void *arg) { (void)arg; sim_wifi_ap_add("SYNTH_WIFI", -40, ""); }
static void capture(void *arg) { sim_display_snapshot(arg); }
static void run(void *arg) {
    (void)arg;
    CHECK(WAIT_FOR(ink_member_space_open(), 10000), "member space opens");
    test_start_home();
    vtk_event_at(vtk_now() + SIM_MS(1), add_ap, NULL);
    delay_ms(5);
    CHECK(AI("wifi.connect", "{\"ssid\":\"SYNTH_WIFI\"}") && g_rc.status == INK_RC_DONE, "connect synthetic network");
    CHECK(AI("settings.set", "{\"id\":\"display.font_size\",\"value\":3}") && g_rc.status == INK_RC_DONE, "use largest font");
    CHECK(AI("shell.network", NULL) && g_rc.status == INK_RC_DONE, "open connected network settings");
    quiet(); on_ui(inspect);
    CHECK(strstr(page, "第 1 页，共 2 页") && button && strstr(page, "wifi.forget(") && strstr(g_snap, "wifi.forget("),
        "first page has both the forget button and its snapshot action");
    CHECK(AI("shell.page_next", NULL) && g_rc.status == INK_RC_DONE, "turn to second page");
    quiet(); on_ui(inspect);
    CHECK(strstr(page, "第 2 页，共 2 页") && !button, "second page has no forget button");
    CHECK(!strstr(page, "wifi.forget(") && !strstr(g_snap, "wifi.forget("), "second page omits the hidden forget action");
    vtk_event_at(vtk_now() + SIM_MS(1), capture, "network-second");
    CHECK(AI("shell.page_prev", NULL) && g_rc.status == INK_RC_DONE, "return to first page");
    quiet(); on_ui(inspect);
    CHECK(strstr(page, "第 1 页，共 2 页") && button && strstr(page, "wifi.forget(") && strstr(g_snap, "wifi.forget("),
        "returning to first page restores the exact visible action");
    if (button) tap(x, y);
    ink_confirm_view_t card = {0};
    CHECK(WAIT_FOR(ink_confirm_list(&card, 1) == 1, 5000) && !strcmp(card.cap, "wifi.forget") && card.src == INK_SRC_TOUCH,
        "visible button invokes wifi.forget through the existing human confirmation");
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name = "wifi-pages-test", .fn = run, .stack_bytes = 16384,
        .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start network pagination test");
}
