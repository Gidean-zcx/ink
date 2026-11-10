/* Real lock surfaces, input driver, policy, communication service and AT model. */
#include <stdlib.h>
#include "../../system/apps/phone/phone.c"
#define ink_main product_main
#include "../../system/boot/main.c"
#undef ink_main
#include "../integration/whole_product.h"

static char phase[16];
static const char *wanted, *shot;
static int x, y;
static bool english;
static volatile int pin_done;
static int pin_result;
static sim_cell_state_t cell;
static int modem_call;
static char modem_number[24];
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void quiet(void) { CHECK(WAIT_FOR(settled(), 10000), "emergency display settles"); }
static void inspect_world(void *arg) {
    (void)arg; sim_net_cell_state(&cell); modem_call = sim_net_call(modem_number, sizeof modem_number);
}
static bool world(bool on, int call) {
    vtk_event_at(vtk_now() + SIM_MS(1), inspect_world, NULL); delay_ms(5);
    return cell.radio == on && modem_call == call;
}
static void locate(lv_obj_t *o) {
    if (!lv_obj_is_visible(o)) return;
    if (lv_obj_check_type(o, &lv_label_class) && !strcmp(lv_label_get_text(o), wanted)) {
        lv_area_t a; lv_obj_get_coords(o, &a); x = (a.x1 + a.x2) / 2; y = (a.y1 + a.y2) / 2;
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) locate(lv_obj_get_child(o, (int32_t)i));
}
static void find(void) { x = y = -1; lv_obj_update_layout(lv_layer_sys()); locate(lv_layer_sys()); }
static void press(const char *text) {
    quiet(); wanted = text; on_ui(find);
    CHECK(x >= 0 && y >= 0 && y < INK_CANVAS_H, "visible control: %s", text);
    if (x >= 0) {
        sim_input_report(); double before = sim_metric_value("input.touches"); tap(x, y); sim_input_report();
        CHECK(WAIT_FOR(sim_metric_value("input.touches") > before, 2000), "touch reached the driver");
    }
    quiet();
}
static void privacy(void) {
    CHECK(E.root && !strcmp(ink_ui_top_name(), "PhoneEmergency"), "restricted page covers every previous surface");
    if (!E.root) return;
    CHECK(ink_ui_context_snapshot(g_snap, sizeof g_snap, NULL) > 0, "bounded AI snapshot fits");
    CHECK(strstr(g_snap, english ? "Emergency call" : "紧急呼叫") &&
          !strstr(g_snap, "00000000001") && !strstr(g_snap, "owner") && !strstr(g_snap, "kid") &&
          !strstr(g_snap, "phone.calls") && !strstr(g_snap, "contacts") && !strstr(g_snap, "1111"),
          "snapshot excludes prior number, identity, history, contacts and PIN: %.150s", g_snap);
    CHECK(lv_obj_get_width(E.root) == ink_ui_px(INK_CANVAS_W) && lv_obj_get_height(E.root) == ink_ui_px(INK_CANVAS_H), "opaque page covers the full display");
}
static void capture_event(void *arg) { (void)arg; sim_display_snapshot(shot); }
static void frame(const char *name) { quiet(); shot = name; vtk_event_at(vtk_now() + SIM_MS(1), capture_event, NULL); delay_ms(5); }
static void told(void *ctx, int result, uint32_t wait_s) { (void)ctx; (void)wait_s; pin_result = result; pin_done = 1; }
static void no_cards(void *arg) { (void)arg; sim_net_set_card(0, false, false, false); sim_net_set_card(1, false, false, false); }
static void read_env(void *arg) {
    (void)arg; const char *s = getenv("INK_EMERGENCY_PHASE"); snprintf(phase, sizeof phase, "%s", s ? s : "lock");
}
static void open_entry(void) {
    press(english ? "Emergency call" : "紧急呼叫");
    CHECK(E.root && !E.active, "physical emergency entry opens restricted keypad"); on_ui(privacy);
}
static void finish_call(bool airplane, bool remote, const char *number) {
    CHECK(WAIT_FOR(world(true, 1), 10000), "emergency AT call connects with RF on");
    CHECK(!strcmp(modem_number, number), "modem dialled the requested emergency number: %.20s", modem_number);
    CHECK(E.active, "emergency page tracks the active call");
    CHECK(ink_power_state() == INK_PWR_LOCKED || !ink_member_space_open(), "call did not authenticate or unlock");
    CHECK(ink_setting_get("net.airplane") == airplane, "airplane setting is unchanged during call");
    CHECK(BY_HAND("call.dial", "{\"number\":\"00000000001\"}") && g_rc.status == INK_RC_REJECTED, "ordinary call stays locked");
    CHECK(BY_HAND("phone.contacts", NULL) && g_rc.status == INK_RC_REJECTED, "contacts stay locked");
    on_ui(privacy); frame("active");
    if (remote) hw(HW_HANGUP, 0, 0);
    else press(english ? "Hang up" : "挂断");
    CHECK(WAIT_FOR(!E.root && world(!airplane, 0), 10000), "hangup closes restricted page and restores RF");
    quiet();
    CHECK(ink_power_state() == INK_PWR_LOCKED && !strcmp(ink_ui_top_name(), "LockPage"), "hangup returns to still-locked screen");
    CHECK(ink_setting_get("net.airplane") == airplane && (!airplane || cell.airplane), "CFUN=4 and the original setting are restored");
}
static void run(void *arg) {
    (void)arg; vtk_host_call(read_env, NULL); english = !strcmp(phase, "en");
    bool gate = !strcmp(phase, "gate"), kid = !strcmp(phase, "kid");
    bool no_sim = !strcmp(phase, "nosim"), airplane = no_sim || !strcmp(phase, "airplane");
    CHECK(WAIT_FOR(ink_member_space_open(), 10000), "initial member opens"); test_start_home(); quiet();
    if (kid) CHECK(ink_member_switch("kid") == 0, "child selected before lock");
    CHECK(BY_HAND("phone.open", "{\"number\":\"00000000001\"}") && g_rc.status == INK_RC_DONE, "private number exists underneath"); quiet();
    if (english) CHECK(BY_HAND("settings.set", "{\"id\":\"system.language\",\"value\":1}") && g_rc.status == INK_RC_DONE, "switch to English");
    if (gate) {
        CHECK(!ink_member_set_pin("owner", "", "1111", told, NULL) && WAIT_FOR(pin_done, 5000) && pin_result == INK_PIN_OK, "synthetic adult PIN set");
        CHECK(setting_by_hand("member.idle_return_s", 30), "existing lock grace set");
    }
    if (airplane) {
        CHECK(BY_HAND("net.set_airplane", "{\"enabled\":true}") && g_rc.status == INK_RC_DONE, "enable airplane mode");
        CHECK(WAIT_FOR(world(false, 0), 5000), "RF is off before emergency");
    } else CHECK(WAIT_FOR(world(true, 0) && cell.registered, 15000), "ordinary card registered");
    if (no_sim) { vtk_event_at(vtk_now() + SIM_MS(1), no_cards, NULL); delay_ms(5); }
    ink_power_lock(); CHECK(WAIT_FOR(ink_power_state() == INK_PWR_LOCKED, 3000), "device locked"); quiet();
    if (gate) {
        CHECK(WAIT_FOR(!ink_member_space_open(), 35000), "adult authentication expires");
        key(HAL_KEY_POWER); quiet(); CHECK(!strcmp(ink_ui_top_name(), "member_gate"), "PIN gate has the entry");
    }
    frame("entry"); open_entry(); frame("keypad");
    CHECK(AI("phone.emergency", "{\"number\":\"120\"}") && g_rc.status != INK_RC_DONE, "AI cannot dial directly");
    CHECK(AI("call.dial", "{\"number\":\"120\"}") && g_rc.status != INK_RC_DONE, "AI cannot use the ordinary alias");
    CHECK(AI("phone.emergency_key", "{\"key\":\"1\"}") && g_rc.status != INK_RC_DONE, "AI cannot type emergency keys");
    CHECK(AI("phone.emergency_open", NULL) && g_rc.status == INK_RC_DONE && !E.active, "AI can only open the page"); quiet();
    CHECK(!ink_cap_ai_callable(ink_cap_find("phone.emergency")) && ink_cap_ai_callable(ink_cap_find("phone.emergency_open")), "AI directory exposes only opening");
    CHECK(BY_HAND("phone.emergency", "{\"number\":\"10086\"}") && g_rc.status == INK_RC_REJECTED && strstr(g_rc.say, english ? "Only emergency" : "锁屏时只能拨紧急电话"), "service enforces the four numbers");
    press("1"); press("3"); press("8"); press(english ? "Dial" : "拨号");
    CHECK(E.root && !E.active && world(!airplane, 0), "ordinary keypad number is not dialled");
    CHECK(strstr(lv_label_get_text(E.notice), english ? "Only emergency" : "锁屏时只能拨紧急电话"), "keypad gives the required refusal");
    for (int i = 0; i < 3; i++) press(english ? "Delete" : "删除");
    if (airplane) press(no_sim ? "112" : "110");
    else { press("1"); press("2"); press("0"); press(english ? "Dial" : "拨号"); }
    finish_call(airplane, gate || no_sim, airplane ? no_sim ? "112" : "110" : "120");
    CHECK(!gate || !ink_member_space_open(), "expired adult remains unauthenticated");
    if (!strcmp(phase, "lock")) {
        const char *numbers[] = {"110", "119", "112"};
        for (unsigned i = 0; i < sizeof numbers / sizeof *numbers; i++) { open_entry(); press(numbers[i]); finish_call(false, false, numbers[i]); }
    }
    frame("returned"); ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name = "emergency-test", .fn = run, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL,
        .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    if (osal_task_create(&cfg, NULL) != OSAL_OK) osal_panic("emergency test");
}
