/* Airplane mode and power saving through the real presenters, policy, settings file and the NT26 model. */
#include <stdlib.h>
#include "ink/sys/ai.h"
#include "ink/ui_shell.h"
#define ink_main product_main
#include "../system/boot/main.c"
#undef ink_main
#include "integration/whole_product.h"

static char snapshot[4096], phase[16];
static const char *wanted;
static int x, y;
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void quiet(void) { CHECK(WAIT_FOR(settled(), 10000), "display settles"); }
static int64_t state(const char *id) { int64_t v = 0; ink_state_get_int(ink_state_find(id), &v); return v; }
static void read_page(void) { if (ink_ui_snapshot(snapshot, sizeof snapshot, NULL) <= 0) snapshot[0] = 0; }
static bool page_has(const char *text) { on_ui(read_page); return strstr(snapshot, text) != NULL; }
static const char *top(void) { on_ui(read_screen); return g_top; }

/* the model's radios, read in event context */
static sim_cell_state_t cell;
static const char *link_name;
static bool link_up;
static void read_world(void *arg) { (void)arg; sim_net_cell_state(&cell); link_name = sim_net_link(&link_up); }
static void world(void) { vtk_event_at(vtk_now() + SIM_MS(1), read_world, NULL); delay_ms(5); }
static bool radio_off(void) { world(); return !cell.radio && cell.airplane && !cell.registered; }
static bool radio_on(void) { world(); return cell.radio && cell.registered && state("cell.registered"); }
static bool on_link(const char *name) { world(); return !strcmp(link_name, name) && (strcmp(name, "none") ? link_up : true); }

static void find_at(lv_obj_t *o) {
    if (lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN)) return;
    if (x < 0 && lv_obj_check_type(o, &lv_label_class) && !strcmp(lv_label_get_text(o), wanted)) { /* the first: a switch comes before a heading */
        lv_area_t a; lv_obj_update_layout(o); lv_obj_get_coords(o, &a);
        x = (a.x1 + a.x2) / 2; y = (a.y1 + a.y2) / 2;
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) find_at(lv_obj_get_child(o, (int32_t)i));
}
static void find(void) { x = y = -1; find_at(lv_screen_active()); find_at(lv_layer_sys()); }
static void press(const char *text) {
    quiet(); wanted = text; on_ui(find);
    CHECK(x >= 0 && y >= 0 && y < INK_CANVAS_H, "visible touch target: %s", text);
    if (x >= 0) tap(x, y);
    quiet();
}
static void capture(void *arg) { sim_display_snapshot(arg); }
static void save_snapshot(void *arg) {
    char path[600]; int n = snprintf(path, sizeof path, "%s/%s.json", g_sim.out_dir, (const char *)arg);
    if (n < 0 || (size_t)n >= sizeof path) abort();
    FILE *f = fopen(path, "wb");
    if (!f || fputs(snapshot, f) == EOF || fclose(f)) abort();
}
static void frame(const char *name) {
    quiet(); on_ui(read_page); vtk_host_call(save_snapshot, (void *)name);
    vtk_event_at(vtk_now() + SIM_MS(1), capture, (void *)name); delay_ms(50);
    char metric[64]; snprintf(metric, sizeof metric, "test.snapshot.%s", name); ink_probe_count(metric);
}
static void ask(void) { ink_ai_ask_text("合成测试问题"); }
static void flush(void) { CHECK(WAIT_FOR(ink_storage_pending() == 0, 10000), "writes settle"); delay_ms(1500); }

static void write_phase(void) {
    CHECK(AI("power.set_saving", "{\"enabled\":true}") && g_rc.status == INK_RC_DONE, "AI turns saving on");
    CHECK(ink_setting_get("power.saving") == 1 && state("power.saving") == 1, "saving is a stored setting and a state");
    CHECK(ink_setting_get("power.lock_after_s") == 600, "saving leaves the lock setting alone");
    CHECK(WAIT_FOR(radio_on(), 15000), "the card is registered before airplane mode");
    CHECK(AI("settings.set", "{\"id\":\"bt.enabled\",\"value\":0}") && g_rc.status == INK_RC_DONE, "Bluetooth off before airplane mode");

    /* the control centre's switch */
    CHECK(BY_HAND("shell.control", NULL) && g_rc.status == INK_RC_DONE, "open the control centre"); quiet();
    CHECK(BY_HAND("shell.more", NULL) && g_rc.status == INK_RC_DONE, "show more"); quiet();
    press("飞行模式");
    CHECK(WAIT_FOR(ink_setting_get("net.airplane") == 1, 3000), "touch turns airplane mode on");
    CHECK(!ink_setting_get("wifi.enabled") && !ink_setting_get("bt.enabled"), "Wi-Fi and Bluetooth go off with it");
    CHECK(WAIT_FOR(radio_off(), 5000), "the modem's radio is off (AT+CFUN=4)");
    CHECK(WAIT_FOR(on_link("none"), 5000), "no link is used: Wi-Fi is off too");
    CHECK(WAIT_FOR(page_has("飞行模式=1"), 3000), "the control centre snapshot says so: %.150s", snapshot);
    frame("control");
    CHECK(BY_HAND("shell.close", NULL), "close the control centre"); quiet();
    frame("statusbar");

    /* Wi-Fi on again by itself */
    CHECK(AI("shell.network", NULL) && g_rc.status == INK_RC_DONE, "open network settings"); quiet();
    CHECK(page_has("飞行模式=1") && page_has("Wi-Fi=0") && page_has("net.set_airplane"), "network snapshot: %.150s", snapshot);
    CHECK(AI("wifi.scan", NULL) && g_rc.status == INK_RC_REJECTED, "a switched-off Wi-Fi does not scan");
    press("Wi-Fi");
    CHECK(ink_setting_get("wifi.enabled") == 1 && ink_setting_get("net.airplane") == 1, "Wi-Fi on, airplane mode stays");
    CHECK(WAIT_FOR(on_link("wifi"), 10000), "Wi-Fi works again in airplane mode");
    CHECK(radio_off(), "the modem's radio stays off");
    CHECK(AI("wifi.scan", NULL) && g_rc.status == INK_RC_DONE, "Wi-Fi scans again");
    CHECK(WAIT_FOR(page_has("Wi-Fi=1"), 3000), "network snapshot follows");
    frame("network");
    CHECK(AI("settings.set", "{\"id\":\"bt.enabled\",\"value\":1}") && g_rc.status == INK_RC_DONE, "Bluetooth on by itself");
    CHECK(AI("bt.list", NULL) && g_rc.status == INK_RC_DONE, "Bluetooth is usable in airplane mode");

    /* a call and a message say why, with the way out */
    CHECK(BY_HAND("call.dial", "{\"number\":\"10086\"}") && g_rc.status == INK_RC_REJECTED && strstr(g_rc.say, "飞行模式已打开"), "dial is refused with the reason: %.120s", g_rc.say);
    CHECK(BY_HAND("message.send", "{\"to\":\"10086\",\"text\":\"合成短信\"}") && g_rc.status == INK_RC_REJECTED && strstr(g_rc.say, "飞行模式已打开"), "message is refused with the reason");
    CHECK(AI("system.home", NULL), "home before the phone"); quiet();
    CHECK(BY_HAND("phone.open", "{\"number\":\"10086\"}") && g_rc.status == INK_RC_DONE, "open the dial pad"); quiet();
    tap(INK_CANVAS_W / 2, INK_CANVAS_H - INK_MARGIN - INK_DIGIT / 2); quiet();
    CHECK(WAIT_FOR(!strcmp(top(), "AirplaneOn"), 3000), "the dial button shows the airplane panel: %.31s", g_top);
    CHECK(strstr(g_snap, "打电话") && strstr(g_snap, "net.set_airplane"), "its snapshot offers the way out: %.150s", g_snap);
    frame("call");
    press("取消");
    CHECK(WAIT_FOR(strcmp(top(), "AirplaneOn"), 2000) && ink_setting_get("net.airplane") == 1, "cancel keeps airplane mode");
    CHECK(AI("system.home", NULL), "home before messages"); quiet();
    CHECK(BY_HAND("sms.thread", "{\"peer\":\"10086\"}") && g_rc.status == INK_RC_DONE, "open a thread"); quiet();
    CHECK(BY_HAND("sms.compose", NULL) && g_rc.status == INK_RC_DONE, "write to it"); quiet();
    press("发送");
    CHECK(WAIT_FOR(!strcmp(top(), "AirplaneOn"), 3000) && strstr(g_snap, "发短信"), "sending shows the airplane panel: %.150s", g_snap);
    frame("sms");
    press("取消");

    /* an AI question that needs the network */
    CHECK(AI("settings.set", "{\"id\":\"wifi.enabled\",\"value\":0}") && g_rc.status == INK_RC_DONE && strstr(g_rc.say, "已关闭 Wi-Fi"),
          "Wi-Fi off again, through wifi.set_enabled: %.120s", g_rc.say);
    CHECK(WAIT_FOR(!ink_setting_get("wifi.enabled"), 4000), "off once the receipt is out");
    CHECK(WAIT_FOR(!state("net.online"), 5000), "offline");
    CHECK(AI("system.home", NULL), "home before asking"); quiet();
    on_ui(ask);
    CHECK(WAIT_FOR(!strcmp(top(), "AiOffline"), 3000), "the question ends at once: %.31s", g_top);
    CHECK(strstr(g_snap, "飞行模式已打开") && strstr(g_snap, "net.set_airplane"), "AI panel says airplane mode, offers the switch: %.150s", g_snap);
    frame("ai");
    press("关闭飞行模式");
    CHECK(WAIT_FOR(ink_setting_get("net.airplane") == 0, 3000), "the panel's button turns airplane mode off");
    CHECK(strcmp(top(), "AiOffline"), "the airplane error panel closes after the successful switch");
    CHECK(ink_setting_get("wifi.enabled") == 1 && ink_setting_get("bt.enabled") == 0, "Wi-Fi and Bluetooth as before airplane mode");
    CHECK(WAIT_FOR(radio_on(), 15000), "the radio is on and the card registered again");
    CHECK(WAIT_FOR(on_link("wifi"), 10000), "Wi-Fi in use again");
    CHECK(AI("ai.pages.close", NULL), "close the AI panel"); quiet();

    /* the AI turns it on: the receipt first, shown here, then the link goes */
    on_ui(ask); delay_ms(200);
    CHECK(AI("net.set_airplane", "{\"enabled\":true}") && g_rc.status == INK_RC_DONE && strstr(g_rc.say, "已打开飞行模式"), "AI's receipt: %.120s", g_rc.say);
    CHECK(ink_setting_get("net.airplane") == 0 && on_link("wifi"), "the link stays until the receipt is out");
    CHECK(WAIT_FOR(page_has("AI 要等关掉才能用"), 2000), "the device shows the receipt's words: %.150s", snapshot);
    CHECK(WAIT_FOR(ink_setting_get("net.airplane") == 1, 4000), "then airplane mode is on");
    CHECK(WAIT_FOR(radio_off() && on_link("none"), 5000), "and the radios are off");
    delay_ms(50000);
    CHECK(page_has("AI 要等关掉才能用") && !page_has("网络断了"), "the receipt's words stay: %.150s", snapshot);
    CHECK(AI("ai.panel.hide", NULL) || true, "leave the sheet");
    flush();
}

static void restart_phase(void) {
    CHECK(ink_setting_get("power.saving") == 1 && state("power.saving") == 1, "saving survives a restart");
    ink_power_activity(osal_now_us());
    delay_ms(59000);
    CHECK(strcmp(power_state(), "locked"), "saving does not lock before one minute");
    CHECK(WAIT_FOR(!strcmp(power_state(), "locked"), 3000), "saving still locks after one minute");
    ink_power_unlock(osal_now_us());
    CHECK(WAIT_FOR(strcmp(power_state(), "locked"), 2000), "wake up");
    CHECK(AI("power.set_saving", "{\"enabled\":false}") && g_rc.status == INK_RC_DONE && !ink_setting_get("power.saving"), "saving off");

    CHECK(ink_setting_get("net.airplane") == 1 && !ink_setting_get("wifi.enabled") && !ink_setting_get("bt.enabled"), "airplane mode survives a restart");
    CHECK(WAIT_FOR(radio_off(), 5000), "the modem's radio is turned off again after the restart");
    CHECK(WAIT_FOR(on_link("none"), 5000), "no link after the restart");
    CHECK(BY_HAND("call.dial", "{\"number\":\"10086\"}") && g_rc.status == INK_RC_REJECTED, "calls are refused after the restart");
    quiet(); frame("restart");
    CHECK(BY_HAND("net.set_airplane", "{\"enabled\":false}") && g_rc.status == INK_RC_DONE, "turn it off by hand");
    CHECK(WAIT_FOR(ink_setting_get("wifi.enabled") == 1, 3000) && ink_setting_get("bt.enabled") == 0, "the state before it comes back from the file");
    CHECK(WAIT_FOR(radio_on(), 15000), "the card registers again");
    flush();
}

static void again_phase(void) {
    CHECK(ink_setting_get("power.saving") == 0 && state("power.saving") == 0, "saving off survives a restart");
    CHECK(ink_setting_get("net.airplane") == 0 && ink_setting_get("wifi.enabled") == 1, "airplane mode off survives a restart");
    CHECK(WAIT_FOR(radio_on(), 15000), "the radio stays on");
    ink_power_activity(osal_now_us());
    delay_ms(61000);
    CHECK(strcmp(power_state(), "locked"), "without saving the device is not locked after one minute");
}

static void read_env(void *arg) {
    (void)arg;
    const char *value = getenv("INK_AIRPLANE_PHASE");
    snprintf(phase, sizeof phase, "%s", value ? value : "write");
}
static void run(void *arg) {
    (void)arg;
    vtk_host_call(read_env, NULL);
    CHECK(WAIT_FOR(ink_member_space_open(), 10000), "member space opens"); test_start_home(); quiet();
    if (!strcmp(phase, "restart")) restart_phase();
    else if (!strcmp(phase, "again")) again_phase();
    else write_phase();
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name = "airtest", .fn = run, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL,
        .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    if (osal_task_create(&cfg, NULL) != OSAL_OK) osal_panic("airplane test");
}
