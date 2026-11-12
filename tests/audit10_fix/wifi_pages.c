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
    int current = 1, pages = 0, forget_page = 0, count = 0;
    const char *at = strstr(page, "第 ");
    CHECK(at && sscanf(at, "第 %d 页，共 %d 页", &current, &pages) == 2 && pages > 1 && pages <= 8, "network pages are bounded");
    for (int i = 1; i <= pages; i++) {
        CHECK(button == (strstr(page, "wifi.forget(") != NULL) && button == (strstr(g_snap, "wifi.forget(") != NULL),
            "page %d snapshot exposes exactly its visible forget action", i);
        if (button) { forget_page = i; count++; }
        if (i < pages) { CHECK(AI("shell.page_next", NULL) && g_rc.status == INK_RC_DONE, "visit next page"); quiet(); on_ui(inspect); }
    }
    CHECK(count == 1, "exactly one page offers the current network's forget action");
    vtk_event_at(vtk_now() + SIM_MS(1), capture, "network-second");
    for (int i = pages; i > forget_page; i--) { CHECK(AI("shell.page_prev", NULL) && g_rc.status == INK_RC_DONE, "return to forget page"); quiet(); }
    on_ui(inspect);
    CHECK(button && strstr(page, "wifi.forget(") && strstr(g_snap, "wifi.forget("), "return restores the exact visible action");
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
