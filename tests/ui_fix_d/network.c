/* Real shell and capabilities; scan receipts use ordered synthetic APs. */
#ifndef INK_UI_FIX_D_SOURCE
#define INK_UI_FIX_D_SOURCE "../../system/ui/shell/navigation.c"
#endif
#include INK_UI_FIX_D_SOURCE
#include "common.h"

static void u_aps(void *ctx) {
    (void)ctx;
    sim_wifi_ap_add("SYNTH_MAIN", -45, "");
    sim_wifi_ap_add("SYNTH_OPEN", -55, "");
    sim_wifi_ap_add("SYNTH_PRIVATE", -82, "SYNTH_PASSWORD");
}
static void u_results(void) {
    ink_receipt_t rc = {.status = INK_RC_DONE, .result =
        "[{\"ssid\":\"SYNTH_PRIVATE\",\"rssi\":-82,\"secured\":true},"
        "{\"ssid\":\"SYNTH_OPEN\",\"rssi\":-55,\"secured\":false},"
        "{\"ssid\":\"SYNTH_MAIN\",\"rssi\":-45,\"secured\":false}]"};
    wifi_receipt((void *)(((uintptr_t)N.epoch << 1) | 1), &rc);
}
static void u_network_check(void) {
    lv_obj_t *scan = u_cap(lv_screen_active(), "wifi.scan");
    U_CHECK(scan && lv_obj_get_width(scan) == ink_ui_px(INK_CANVAS_W - 2 * INK_MARGIN), "T07 scan keeps the full content width");
    U_CHECK(N.body_page == 0 && u_label(lv_screen_active(), "SYNTH_MAIN") &&
        u_label(lv_screen_active(), "SYNTH_PRIVATE"), "T08 scan results are selectable on the scan page");
    U_CHECK(N.wifi_count == 3 && !strcmp(N.wifi_ssid[0], "SYNTH_MAIN"), "T08 connected network is first");
    lv_obj_t *name = u_label(lv_screen_active(), "SYNTH_PRIVATE");
    int images = 0, black_bars = 0;
    if (name) {
        lv_obj_t *row = lv_obj_get_parent(name);
        for (uint32_t i = 0; i < lv_obj_get_child_count(row); i++) {
            lv_obj_t *o = lv_obj_get_child(row, (int32_t)i);
            if (lv_obj_check_type(o, &lv_image_class)) images++;
            for (uint32_t j = 0; j < lv_obj_get_child_count(o); j++)
                if (lv_obj_check_type(lv_obj_get_child(o, (int32_t)j), &lv_image_class)) images++;
            if (lv_obj_get_width(o) == ink_ui_px(INK_GAP_XS) &&
                lv_color_eq(lv_obj_get_style_bg_color(o, 0), ink_ui_ink())) black_bars++;
        }
    }
    U_CHECK(images >= 2 && black_bars == 1, "T08 weak secured network has a lock and one filled signal bar");
}
static int u_index(const char *name) {
    for (int i = 0; i < N.wifi_count; i++) if (!strcmp(name, N.wifi_ssid[i])) return i;
    return -1;
}
static bool u_open_connected(void) {
    char ssid[40], state[24]; state_text("wifi.ssid", ssid, sizeof ssid); state_text("wifi.state", state, sizeof state);
    return !N.picked_ssid[0] && !strcmp(ssid, "SYNTH_OPEN") && !strcmp(state, "connected");
}
static void u_pick(const char *name) {
    char args[48]; snprintf(args, sizeof args, "{\"index\":%d}", u_index(name));
    u_command("shell.wifi_pick", args);
}
static void u_strip_check(void) {
    lv_obj_t *area = u_textarea(N.page_body);
    CHECK(area, "T09 protected field exists");
    if (!area) return;
    lv_obj_t *field = lv_obj_get_parent(area); lv_area_t r = u_rect(field), keyboard;
    ink_shell_keyboard_area(&keyboard);
    bool strip = false;
    for (uint32_t i = 0; i < lv_obj_get_child_count(N.page_body); i++) {
        lv_obj_t *o = lv_obj_get_child(N.page_body, (int32_t)i); lv_area_t a = u_rect(o);
        if (a.x1 == 0 && a.x2 == ink_ui_width() - 1 && a.y1 < r.y1 && a.y2 >= keyboard.y1 &&
            lv_obj_get_style_border_side(o, 0) == LV_BORDER_SIDE_TOP && lv_obj_get_style_border_width(o, 0) > 0 &&
            lv_color_eq(lv_obj_get_style_bg_color(o, 0), ink_ui_paper())) strip = true;
    }
    U_CHECK(strip && r.y2 < keyboard.y1, "T09 password strip has a full white backing and top border above the keyboard");
    CHECK(ink_text_type() == INK_TEXT_PASSWORD && ink_text_ai_hidden(), "T09 password remains person-only");
    CHECK(ink_ui_context_snapshot(g_snap, sizeof g_snap, NULL) > 0 && !strstr(g_snap, "SYNTH_PASSWORD"), "T09 AI context excludes the password");
}
static bool u_has_field;
static void u_find_field(void) { u_has_field = u_textarea(N.page_body) != NULL; }
static void u_wrong_password(void) { ink_text_commit_utf8("SYNTH_WRONG"); }
static void u_password_retry_check(void) {
    U_CHECK(N.picked_ssid[0] && ink_text_focused() && ink_text_type() == INK_TEXT_PASSWORD && !ink_text_value()[0],
        "T09 failed join preserves the network and offers a fresh protected password field");
    U_CHECK(u_label(lv_screen_active(), N.result), "T09 join error remains visible with the password retry");
    CHECK(ink_ui_context_snapshot(g_snap, sizeof g_snap, NULL) > 0 && !strstr(g_snap, "SYNTH_WRONG"), "T09 failed password is absent from AI context");
}
static void u_scan_failed(void) {
    ink_receipt_t rc = {.status = INK_RC_FAILED};
    snprintf(rc.say, sizeof rc.say, "合成扫描失败，请重试");
    wifi_receipt((void *)(((uintptr_t)N.epoch << 1) | 1), &rc);
    U_CHECK(N.body_page == 0 && u_label(lv_screen_active(), rc.say) && u_cap(lv_screen_active(), "wifi.scan"), "T08 scan failure and retry share the current page");
}
static void u_known_empty(void) {
    N.known_count = 0; draw_page_body();
    U_CHECK(u_cap(lv_screen_active(), "shell.network"), "T10 empty saved networks has an exit to nearby networks");
}
static void u_known_list_check(void) {
    lv_obj_t *join = u_cap(lv_screen_active(), "wifi.connect"), *forget = u_cap(lv_screen_active(), "wifi.forget");
    lv_area_t a = u_rect(join), b = u_rect(forget);
    U_CHECK(join && forget && lv_obj_get_height(join) == ink_ui_px(ink_ui_row_double()) &&
        abs(a.y1 + a.y2 - b.y1 - b.y2) <= 2, "T11 saved network connects from its row and keeps forget on the same row");
    N.known_count = -2; draw_page_body();
    U_CHECK(u_label(lv_screen_active(), "网络列表暂时读不出来") && u_cap(lv_screen_active(), "shell.known_networks"), "T11 saved-network read failure has an explicit retry");
}
static void u_home_check(void) {
    N.home_page = 1; draw_home_content(); lv_obj_update_layout(N.content);
    lv_obj_t *resume = u_cap(N.content, "shell.resume"); lv_area_t r = u_rect(resume);
    lv_area_t prev = {0}; int count = 0;
    for (uint32_t i = 0; i < lv_obj_get_child_count(N.content); i++) {
        lv_obj_t *o = lv_obj_get_child(N.content, (int32_t)i);
        const char *cap = ink_shell_bound_cap(o);
        if (!cap || strcmp(cap, "shell.launch")) continue;
        lv_area_t a = u_rect(o);
        if (!count && resume) U_CHECK(a.y1 - r.y2 - 1 >= ink_ui_px(INK_GAP_M), "S02 home leaves a visible gap below continue reading");
        if (count % 4) U_CHECK(abs((a.x1 + a.x2 - prev.x1 - prev.x2) / 2 - ink_ui_px((INK_CANVAS_W - 2 * INK_MARGIN) / 4)) <= 2 && a.y1 == prev.y1,
            "H01 child-visible icons have no empty grid slots");
        prev = a; count++;
    }
    CHECK(count > 0, "home has visible application icons");
}
static void u_run(void *ctx) {
    (void)ctx; u_start(); vtk_event_at(vtk_now() + SIM_MS(1), u_aps, NULL); delay_ms(5);
    u_command("wifi.connect", "{\"ssid\":\"SYNTH_MAIN\"}");
    u_command("shell.network", NULL); on_ui(u_results); u_quiet(); on_ui(u_network_check); u_capture("Network");
    u_pick("SYNTH_OPEN");
    U_CHECK(WAIT_FOR(u_open_connected(), 15000), "T08 open-network connection receipt is complete");
    U_CHECK(WAIT_FOR(!strcmp(ink_text_value(), "") && !ink_text_focused(), 3000), "T08 open network connects without password entry");
    u_pick("SYNTH_PRIVATE");
    if (u_before) for (int i = 0; i < N.body_pages; i++) {
        on_ui(u_find_field);
        if (u_has_field) break;
        u_command("shell.page_next", NULL);
    }
    on_ui(u_strip_check); u_capture("WifiPassword");
    if (ink_text_focused()) {
        on_ui(u_wrong_password); u_command("keyboard.key", "{\"code\":10}");
        U_CHECK(WAIT_FOR(ink_text_focused(), 5000), "T09 failed join returns to password entry");
        on_ui(u_password_retry_check);
    }
    u_capture("WifiRetry");
    u_command("system.home", NULL); u_command("shell.network", NULL); on_ui(u_scan_failed); u_capture("NetworkFail");
    u_command("shell.known_networks", NULL);
    CHECK(WAIT_FOR(N.known_count >= 0, 15000), "saved networks load"); on_ui(u_known_list_check);
    u_command("shell.known_networks", NULL); CHECK(WAIT_FOR(N.known_count >= 0, 15000), "explicit list retry completes");
    on_ui(u_known_empty); u_capture("KnownEmpty");
    u_command("system.home", NULL); on_ui(u_home_check); u_quiet(); u_capture("Home");
    CHECK(ink_member_switch("kid") == 0, "switch to synthetic child"); u_quiet();
    u_command("system.home", NULL); on_ui(u_home_check); u_quiet(); u_capture("KidHome");
    ink_probe_count("test.finished");
}
void ink_main(void) { u_launch(u_run); }
