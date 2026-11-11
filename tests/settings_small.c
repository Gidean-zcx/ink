/* Saved networks and redo, using synthetic credentials and actual touch targets. */
#include "ink/ui_shell.h"
#include "ink/hal_net.h"
#include "ink/sys/textinput.h"
#include "ink/sys/crc.h"
#define ink_main product_main
#include "../system/boot/main.c"
#undef ink_main
#include "integration/whole_product.h"

static char page[3072], connected[33];
static const char *wanted_cap, *wanted_name, *wanted_label;
static int x, y, clicks, row_y, best_y, confirmations;
static bool disabled, captured;

static lv_obj_t *label(lv_obj_t *o, const char *text) {
    if (!lv_obj_is_visible(o)) return NULL;
    if (lv_obj_check_type(o, &lv_label_class) && !strcmp(lv_label_get_text(o), text)) return o;
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) {
        lv_obj_t *found = label(lv_obj_get_child(o, (int32_t)i), text);
        if (found) return found;
    }
    return NULL;
}
static void clicked(lv_event_t *e) { (void)e; clicks++; }
static void buttons(lv_obj_t *o) {
    if (!lv_obj_is_visible(o)) return;
    const char *cap = ink_shell_bound_cap(o);
    if (cap && wanted_cap && !strcmp(cap, wanted_cap)) {
        lv_area_t a; lv_obj_get_coords(o, &a);
        if (a.y1 >= row_y && a.y1 < best_y) {
            best_y = a.y1; x = (a.x1 + a.x2) / 2; y = (a.y1 + a.y2) / 2;
            disabled = lv_obj_has_state(o, LV_STATE_DISABLED);
            CHECK(a.x1 >= 0 && a.x2 < ink_ui_width() && a.y2 < ink_ui_height(), "action fits on screen");
            lv_obj_add_event_cb(o, clicked, LV_EVENT_PRESSED, NULL);
        }
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) buttons(lv_obj_get_child(o, (int32_t)i));
}
static void inspect(void) {
    ink_ui_snapshot(page, sizeof page, NULL);
    ink_ui_context_snapshot(g_snap, sizeof g_snap, NULL);
    ink_state_get_str(ink_state_find("wifi.ssid"), connected, sizeof connected);
}
static void locate(void) {
    lv_obj_update_layout(lv_screen_active()); lv_obj_update_layout(lv_layer_sys());
    x = y = -1; row_y = 0; best_y = ink_ui_height(); disabled = false;
    lv_obj_t *name = wanted_name ? label(lv_screen_active(), wanted_name) : NULL;
    if (wanted_name && !name) return;
    if (name) { lv_area_t a; lv_obj_get_coords(lv_obj_get_parent(name), &a); row_y = a.y1; }
    buttons(lv_screen_active()); buttons(lv_layer_sys());
    if (wanted_label) CHECK(label(lv_layer_sys(), wanted_label) || label(lv_screen_active(), wanted_label), "translated action is visible: %s", wanted_label);
}
static void quiet(void) { CHECK(WAIT_FOR(!ink_ui_busy() && !ink_display_busy(), 10000), "display settles"); }
static bool press(const char *cap, const char *name) {
    wanted_cap = cap; wanted_name = name; wanted_label = NULL;
    quiet(); on_ui(locate);
    CHECK(x >= 0 && !disabled, "visible enabled touch target: %s", cap);
    if (x < 0 || disabled) return false;
    int before = clicks; tap(x, y);
    CHECK(WAIT_FOR(clicks > before, 2000), "driver delivered the touch: %s", cap);
    return clicks > before;
}
static bool saved_count(int count) {
    char expected[40]; snprintf(expected, sizeof expected, "已存=%d；", count);
    return WAIT_FOR((on_ui(inspect), strstr(page, expected) != NULL), 8000);
}
static void capture(void *arg) { sim_display_snapshot(arg); captured = true; }
static void frame(const char *name) {
    quiet(); captured = false;
    vtk_event_at(vtk_now() + SIM_MS(1), capture, (void *)name);
    CHECK(WAIT_FOR(captured, 1000), "capture settled frame");
}
static void add_aps(void *arg) {
    (void)arg;
    sim_wifi_ap_add("SYNTH_ALPHA", -40, "SYNTH_ONLY_PASS");
    sim_wifi_ap_add("SYNTH_BETA", -40, "SYNTH_ONLY_PASS");
    sim_wifi_ap_add("SYNTH_GAMMA", -40, "SYNTH_ONLY_PASS");
}
static void confirmation(void *ctx, const char *id, const ink_args_t *args) {
    (void)ctx; (void)id;
    char status[24];
    if (ink_args_get_str(args, "status", status, sizeof status) > 0 && !strcmp(status, "done")) confirmations++;
}
static void more_networks(void) {
    ink_db_t *db = ink_db_open(ink_db_find_def("net.wifi"), NULL);
    for (int i = 0; i < 5; i++) {
        hal_wifi_known_t k = {0}; snprintf(k.ssid, sizeof k.ssid, "SYNTH_EXTRA_%d", i);
        uint32_t id = ink_crc32(0, k.ssid, strlen(k.ssid)) & 0x7fffffff;
        ink_db_write_t w = {.id = id ? id : 1, .data = &k, .len = sizeof k, .src = INK_SRC_TOUCH};
        CHECK(db && ink_db_put(db, &w, NULL) == INK_DB_OK, "seed extra saved network");
    }
    ink_db_close(db);
}
static void focus(void) {
    ink_text_cfg_t cfg = {.type = INK_TEXT_PLAIN, .initial = "A"};
    CHECK(ink_text_focus(&cfg, NULL, NULL) > 0, "focus synthetic text");
    ink_text_commit_utf8("B");
    CHECK(!label(lv_layer_sys(), "Redo") && !label(lv_layer_sys(), "重做"), "ordinary input keeps the candidate and microphone row");
    ink_text_select(0, 1);
}
static void text_is_a(void) { CHECK(!strcmp(ink_text_value(), "A"), "touch undo restores A"); }
static void text_is_ab(void) { CHECK(!strcmp(ink_text_value(), "AB"), "redo restores AB"); }
static void redo_state(void) {
    CHECK(x >= 0 && disabled == !ink_text_can_redo(), "redo enablement matches the session");
    lv_obj_t *redo = label(lv_layer_sys(), wanted_label);
    CHECK(redo != NULL, "redo label exists");
    if (redo) {
        CHECK(lv_obj_get_style_text_decor(redo, 0) == (disabled ? LV_TEXT_DECOR_STRIKETHROUGH : LV_TEXT_DECOR_NONE), "disabled redo has a visible monochrome mark");
        lv_area_t a; lv_obj_get_coords(redo, &a);
        CHECK(a.x1 >= 0 && a.x2 < ink_ui_width() && a.y2 < ink_ui_height(), "redo label fits the screen");
    }
}
static void keyboard_case(bool english) {
    on_ui(focus);
    wanted_cap = "text.redo"; wanted_name = NULL; wanted_label = english ? "Redo" : "重做";
    quiet(); on_ui(locate); on_ui(redo_state);
    if (x < 0) return;
    CHECK(disabled, "redo starts disabled");
    int before = clicks; tap(x, y); CHECK(clicks == before, "disabled redo does not accept touch"); on_ui(text_is_ab);
    CHECK(press("text.undo", NULL), "touch undo uses text.undo"); on_ui(text_is_a);
    wanted_cap = "text.redo"; wanted_label = english ? "Redo" : "重做";
    quiet(); on_ui(locate); on_ui(redo_state);
    CHECK(!disabled, "redo is enabled after undo");
    frame(english ? "redo-en" : "redo-zh");
    CHECK(press("text.redo", NULL), "touch redo uses text.redo"); on_ui(text_is_ab);
    CHECK(AI("text.undo", NULL) && g_rc.status == INK_RC_DONE, "AI undo");
    CHECK(AI("text.redo", NULL) && g_rc.status == INK_RC_DONE, "AI redo uses the same session"); on_ui(text_is_ab);
    CHECK(AI("text.undo", NULL) && g_rc.status == INK_RC_DONE, "undo before new input");
    CHECK(BY_HAND("keyboard.key", "{\"code\":33}") && g_rc.status == INK_RC_DONE, "type a new punctuation key");
    wanted_cap = "text.redo"; wanted_name = NULL; wanted_label = english ? "Redo" : "重做";
    quiet(); on_ui(locate); on_ui(redo_state); CHECK(disabled, "new input disables redo");
    frame(english ? "redo-disabled-en" : "redo-disabled-zh");
    CHECK(AI("text.redo", NULL) && g_rc.status == INK_RC_FAILED, "AI cannot redo after new input");
    CHECK(BY_HAND("system.home", NULL), "close keyboard");
}
static void child_page(void) {
    ink_shell_open_page("KnownNetworks");
    CHECK(strcmp(ink_ui_page_name(), "KnownNetworks"), "child cannot open the saved page directly");
}
static void run(void *arg) {
    (void)arg;
    CHECK(WAIT_FOR(ink_member_space_open(), 10000), "member space opens"); test_start_home();
    ink_cap_subscribe("confirm.result", confirmation, NULL, INK_EXEC_DIRECT);
    vtk_event_at(vtk_now() + SIM_MS(1), add_aps, NULL); delay_ms(5);
    const char *const args[] = {
        "{\"ssid\":\"SYNTH_BETA\",\"password\":\"SYNTH_ONLY_PASS\"}",
        "{\"ssid\":\"SYNTH_GAMMA\",\"password\":\"SYNTH_ONLY_PASS\"}",
        "{\"ssid\":\"SYNTH_ALPHA\",\"password\":\"SYNTH_ONLY_PASS\"}"};
    for (unsigned i = 0; i < sizeof args / sizeof *args; i++) CHECK(BY_HAND("wifi.connect", args[i]) && g_rc.status == INK_RC_DONE, "save synthetic secured network");
    CHECK(AI("shell.network", NULL) && g_rc.status == INK_RC_DONE, "open network settings");
    bool opened = false;
    for (int i = 0; i < 3 && !opened; i++) {
        wanted_cap = "shell.known_networks"; wanted_name = wanted_label = NULL; quiet(); on_ui(locate);
        if (x >= 0) opened = press("shell.known_networks", NULL);
        else CHECK(AI("shell.page_next", NULL), "find saved networks entry");
    }
    CHECK(opened, "network settings exposes saved networks");
    if (!opened) { ink_probe_count("test.finished"); return; }
    CHECK(saved_count(3), "all three saved networks load"); on_ui(inspect);
    CHECK(strstr(page, "SYNTH_ALPHA=connected") && strstr(page, "SYNTH_BETA=saved") && strstr(page, "SYNTH_GAMMA=saved"), "snapshot names and current network");
    CHECK(!strstr(page, "SYNTH_ONLY_PASS") && !strstr(g_snap, "SYNTH_ONLY_PASS"), "password absent from both snapshots");
    frame("networks-zh");
    CHECK(press("wifi.connect", "SYNTH_BETA"), "connect saved network by touch");
    CHECK(WAIT_FOR((on_ui(inspect), strstr(page, "SYNTH_BETA=connected")), 10000), "saved password reused for connection");
    CHECK(press("wifi.forget", "SYNTH_GAMMA"), "forget offline saved network by touch");
    ink_confirm_view_t card = {0};
    CHECK(WAIT_FOR(ink_confirm_list(&card, 1) == 1, 3000) && !strcmp(card.cap, "wifi.forget") && card.src == INK_SRC_TOUCH, "existing human confirmation is retained");
    on_ui(inspect); CHECK(strstr(page, "SYNTH_GAMMA") != NULL, "network remains until confirmed");
    CHECK(ink_confirm_decide(card.token, true, INK_SRC_TOUCH) == 0 && WAIT_FOR(confirmations > 0, 5000), "confirm forget");
    CHECK(saved_count(2), "forget removes exactly one saved network"); on_ui(inspect);
    CHECK(!strstr(page, "SYNTH_GAMMA") && !strcmp(connected, "SYNTH_BETA"), "forgetting offline network preserves current connection");
    CHECK(AI("system.undo", "{\"object\":\"wifi:SYNTH_GAMMA\"}") && g_rc.status == INK_RC_UNDONE, "undo forgotten credentials without exposing the password");
    CHECK(saved_count(3), "undo restores page entry");
    CHECK(press("wifi.connect", "SYNTH_GAMMA"), "connect restored network");
    CHECK(WAIT_FOR((on_ui(inspect), strstr(page, "SYNTH_GAMMA=connected")), 10000), "restored password works");
    CHECK(AI("wifi.restore", "{\"revision\":1}") && g_rc.status != INK_RC_DONE, "AI cannot call the credential inverse directly");
    more_networks();
    CHECK(AI("shell.back", NULL) && AI("shell.known_networks", NULL) && g_rc.status == INK_RC_DONE && saved_count(8), "open all eight saved networks");
    unsigned seen = 0;
    for (int i = 0; i < 4; i++) {
        quiet();
        for (int n = 0; n < 5; n++) {
            char name[33]; snprintf(name, sizeof name, "SYNTH_EXTRA_%d", n);
            wanted_cap = NULL; wanted_name = name; wanted_label = NULL; on_ui(locate);
            /* locate records actual label visibility, independent of collection order. */
            if (row_y > 0) seen |= 1u << n;
        }
        CHECK(AI("shell.page_next", NULL), "turn saved network page");
    }
    CHECK(seen == 31, "every additional network is visible across pages");
    CHECK(AI("settings.set", "{\"id\":\"system.language\",\"value\":1}") && g_rc.status == INK_RC_DONE, "switch to English");
    CHECK(AI("shell.known_networks", NULL) && saved_count(8), "reopen English saved networks"); frame("networks-en");
    CHECK(AI("system.home", NULL), "return home"); keyboard_case(true);
    CHECK(AI("settings.set", "{\"id\":\"system.language\",\"value\":0}") && g_rc.status == INK_RC_DONE, "switch to Chinese"); keyboard_case(false);
    CHECK(!ink_cap_find("shell.known_networks")->child_ok && !ink_cap_find("shell.network")->child_ok, "saved networks inherits adult network settings access");
    CHECK(ink_member_switch("kid") == 0, "switch to child");
    CHECK(AI("shell.known_networks", NULL) && g_rc.status == INK_RC_REJECTED, "child capability request is rejected"); on_ui(child_page);
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name = "settings-small", .fn = run, .stack_bytes = 16384,
        .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start settings-small test");
}
