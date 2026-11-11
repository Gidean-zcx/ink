/* Synthetic names, text and policies through the real product. */
#include "ink/hal_afe.h"
#include "ink/sys/ai.h"
#include "ink/sys/hub.h"
#include "ink/sys/textinput.h"
#include "ink/ui_shell.h"
#define ink_main product_main
#include "configured_boot.c"
#undef ink_main
#include "integration/whole_product.h"
#include <stdlib.h>

static int count, x, y, clicks;
static bool marked, captured;
static ink_text_type_t type;
static bool hidden;
static char snapshot[4096];
static const char *said;
static bool restarting;
static void inspect(void) {
    ink_text_cand_t words[INK_TEXT_PAGE];
    count = ink_text_candidates(0, words, INK_TEXT_PAGE);
    marked = count > 0 && words[0].ai;
    ink_ui_context_snapshot(snapshot, sizeof snapshot, NULL);
}
static void focus(void) {
    ink_text_cfg_t cfg = {.type = type, .initial = "示例", .ai_hidden = hidden};
    CHECK(ink_text_focus(&cfg, NULL, NULL) > 0, "focus test input");
    ink_text_commit_utf8("。 ");
}
static void type_more(void) { ink_text_key('!'); }
static void blur(void) { ink_text_blur(0); }
static void close_ai(void) { ink_ai_dismiss(); }
static void quiet(void) { CHECK(WAIT_FOR(!ink_ui_busy() && !ink_display_busy(), 10000), "display settles"); }
static double requests(void) { return sim_metric_value("hub.requests.text/suggest"); }
static bool suggestions(void) { on_ui(inspect); return count > 0 && marked; }
static void capture(void *arg) { sim_display_snapshot(arg); captured = true; }
static void frame(const char *name) {
    quiet(); captured = false; vtk_event_at(vtk_now() + SIM_MS(1), capture, (void *)name);
    CHECK(WAIT_FOR(captured, 1000), "capture frame");
}
static void pressed(lv_event_t *e) { (void)e; clicks++; }
static void find_candidate(lv_obj_t *o) {
    if (!lv_obj_is_visible(o)) return;
    if (lv_obj_check_type(o, &lv_label_class) && !strcmp(lv_label_get_text(o), "一起看书")) {
        lv_area_t a; lv_obj_get_coords(o, &a); x = (a.x1 + a.x2) / 2; y = (a.y1 + a.y2) / 2;
        lv_area_t icon; lv_obj_get_coords(lv_obj_get_child(lv_obj_get_parent(o), 0), &icon);
        CHECK(icon.x2 < a.x1, "AI icon does not overlap candidate text");
        lv_obj_add_event_cb(lv_obj_get_parent(o), pressed, LV_EVENT_PRESSED, NULL);
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) find_candidate(lv_obj_get_child(o, (int32_t)i));
}
static void locate(void) { x = y = -1; lv_obj_update_layout(lv_layer_sys()); find_candidate(lv_layer_sys()); }
static void inserted(void) { CHECK(strstr(ink_text_value(), "一起看书") != NULL, "INSERT: touch inserts continuation"); }
static void long_prefix(void) {
    static char text[2400];
    text[0] = 0;
    for (int i = 0; i < 600; i++) strcat(text, "字");
    ink_text_cfg_t cfg = {.type = INK_TEXT_PLAIN, .max = 4095, .initial = text};
    CHECK(ink_text_focus(&cfg, NULL, NULL) > 0, "long synthetic input");
    ink_text_commit_utf8("。TAIL_SECRET");
    ink_text_set_cursor(601);
}
static void mic_event(void *arg) { (void)arg; sim_audio_mic_say(said); }
static void silence_event(void *arg) { (void)arg; sim_audio_mic_source("silence"); }
static void say(const char *text) {
    said = text; vtk_event_at(vtk_now() + SIM_MS(1), mic_event, NULL); delay_ms(900);
    vtk_event_at(vtk_now() + SIM_MS(1), silence_event, NULL); delay_ms(500);
}
static void network(void *arg) { sim_net_set_link(arg); }
static void link_to(const char *name) { vtk_event_at(vtk_now() + SIM_MS(1), network, (void *)name); delay_ms(100); }
static volatile bool pin_done;
static int pin_result;
static void pin_reply(void *ctx, int result, uint32_t wait) { (void)ctx; (void)wait; pin_result = result; pin_done = true; }
static void set(const char *args) { CHECK(AI("settings.set", args) && g_rc.status == INK_RC_DONE, "setting %.100s: %.100s", args, g_rc.detail); }
static void run(void *arg) {
    (void)arg;
    if (restarting) {
        pin_done = false;
        CHECK(ink_member_unlock("owner", "24681357", pin_reply, NULL) == 0 && WAIT_FOR(pin_done, 6000) && pin_result == INK_PIN_OK, "restart owner unlock");
        CHECK(ink_setting_get("text.assist") == 1 && ink_setting_get("voice.name") == 1, "MEMBER: adult preference and name survive reboot");
        CHECK(ink_member_switch("kid") == 0 && ink_setting_get("text.assist") == 0, "MEMBER: separate child preference survives reboot");
        ink_probe_count("test.finished"); return;
    }
    CHECK(WAIT_FOR(ink_member_space_open(), 10000), "member is ready");
    test_start_home(); ink_hub_demand(); CHECK(WAIT_FOR(ink_hub_online(), 10000), "hub online");
    if (!hal_afe_available()) {
        CHECK(!hal_afe_names()[0] && hal_afe_select_name(0) < 0, "unsupported HAL reports empty names");
        CHECK(AI("shell.voice", NULL), "open unsupported voice page"); on_ui(inspect);
        CHECK(strstr(snapshot, "这台设备暂不支持叫名字"), "UNSUPPORTED: page tells the truth");
        CHECK(AI("settings.set", "{\"id\":\"voice.name\",\"value\":0}") && g_rc.status == INK_RC_INVALID, "unsupported selection refused");
        frame("voice-unsupported"); ink_probe_count("test.finished"); return;
    }
    CHECK(hal_afe_names()[0] && hal_afe_names()[1] && !hal_afe_names()[3], "three supported names");
    set("{\"id\":\"voice.handsfree\",\"value\":1}");
    set("{\"id\":\"voice.name\",\"value\":\"小墨\"}"); delay_ms(500);
    double wakes = sim_metric_value("afe.wakes"); say("小墨");
    CHECK(sim_metric_value("afe.wakes") == wakes + 1, "NAME: original name wakes"); delay_ms(14000);
    set("{\"id\":\"voice.name\",\"value\":\"小书\"}"); delay_ms(100);
    wakes = sim_metric_value("afe.wakes"); say("小墨");
    CHECK(sim_metric_value("afe.wakes") == wakes, "NAME: old name no longer wakes");
    say("小书"); CHECK(sim_metric_value("afe.wakes") == wakes + 1, "NAME: selected name wakes immediately"); delay_ms(14000);
    set("{\"id\":\"voice.handsfree\",\"value\":2}");
    on_ui(close_ai);
    CHECK(AI("system.home", NULL) && AI("shell.voice", NULL), "voice settings"); frame("voice-name");
    CHECK(AI("system.home", NULL), "home");
    CHECK(ink_setting_get("text.assist") == 0, "TOGGLE: continuation defaults off");
    on_ui(focus); double before = requests(); delay_ms(1400);
    CHECK(requests() == before && !suggestions(), "TOGGLE: off sends nothing");
    set("{\"id\":\"text.assist\",\"value\":true}"); on_ui(focus); before = requests(); delay_ms(400);
    CHECK(requests() == before, "PAUSE: no request before idle delay");
    CHECK(WAIT_FOR(suggestions(), 5000) && requests() > before, "PAUSE: idle input requests suggestions");
    on_ui(inspect); CHECK(strstr(snapshot, "AI 接话"), "AI marker in semantic snapshot"); frame("assist-candidates");
    CHECK(BY_HAND("keyboard.candidates", "{}") && g_rc.status == INK_RC_DONE, "expand continuation candidates"); on_ui(inspect);
    CHECK(strstr(snapshot, "一起看书") && strstr(snapshot, "明天再聊") && strstr(snapshot, "谢谢你"), "all three continuations are accessible");
    frame("assist-expanded");
    on_ui(locate); CHECK(x >= 0, "continuation is a visible touch target");
    if (x >= 0) { int seen = clicks; tap(x, y); CHECK(WAIT_FOR(clicks > seen, 2000), "touch reached candidate driver"); }
    on_ui(inserted);
    CHECK(WAIT_FOR(suggestions(), 5000), "suggestions after insertion"); on_ui(type_more); on_ui(inspect);
    CHECK(!marked, "INVALIDATE: next key invalidates suggestions");
    on_ui(focus); CHECK(WAIT_FOR(suggestions(), 5000), "fresh suggestions");
    set("{\"id\":\"text.assist\",\"value\":false}"); on_ui(inspect);
    CHECK(!marked, "TOGGLE: off removes shown candidates"); before = requests(); delay_ms(1400); CHECK(requests() == before, "TOGGLE: disabled stays silent");
    set("{\"id\":\"text.assist\",\"value\":true}");
    type = INK_TEXT_PASSWORD; on_ui(focus); before = requests(); delay_ms(1400);
    CHECK(requests() == before && !suggestions(), "PASSWORD: secret input sends nothing");
    type = INK_TEXT_PLAIN; hidden = true; on_ui(focus); before = requests(); delay_ms(1400);
    CHECK(requests() == before && !suggestions(), "HIDDEN: hidden input sends nothing"); hidden = false;
    link_to("none"); CHECK(WAIT_FOR(!ink_hub_online(), 5000), "hub offline");
    on_ui(focus); before = requests(); double attempts = sim_metric_value("text.assist_requests"); delay_ms(1400);
    CHECK(requests() == before && sim_metric_value("text.assist_requests") == attempts && !suggestions(), "OFFLINE: no outgoing request");
    link_to("wifi"); ink_hub_demand(); CHECK(WAIT_FOR(ink_hub_online(), 10000), "hub reconnects");
    on_ui(long_prefix); CHECK(WAIT_FOR(suggestions(), 5000), "long prefix gets suggestions");
    CHECK(sim_metric_value("test.prefix.chars") == 500 && !sim_metric_value("test.prefix.suffix"), "PREFIX: only 500 characters before cursor");
    on_ui(blur);
    CHECK(ink_member_switch("kid") == 0 && WAIT_FOR(!strcmp(ink_member_current()->id, "kid"), 2000), "switch child"); delay_ms(100);
    CHECK(ink_setting_get("text.assist") == 0, "MEMBER: child does not inherit adult preference");
    CHECK(AI("settings.set", "{\"id\":\"voice.name\",\"value\":2}") && g_rc.status == INK_RC_REJECTED, "CHILD_NAME: children cannot choose names");
    set("{\"id\":\"text.assist\",\"value\":true}"); on_ui(focus); CHECK(WAIT_FOR(suggestions(), 5000), "allowed child can request");
    CHECK(ink_member_switch("owner") == 0, "return owner"); delay_ms(100); on_ui(inspect);
    CHECK(!count && ink_setting_get("text.assist") == 1, "MEMBER: switch clears candidates and restores owner preference");
    pin_done = false;
    CHECK(ink_member_set_pin("owner", NULL, "24681357", pin_reply, NULL) == 0 && WAIT_FOR(pin_done, 6000) && pin_result == INK_PIN_OK, "synthetic parent PIN");
    CHECK(AI("kid.app_set", "{\"id\":\"kid\",\"app\":\"ai\",\"mode\":0}") && g_rc.status == INK_RC_PENDING_CONFIRMATION, "parent disables child AI");
    unsigned token = 0; CHECK(sscanf(g_rc.object, "confirm:%u", &token) == 1, "policy confirmation"); pin_done = false;
    CHECK(ink_confirm_accept_with_pin(token, "24681357", pin_reply, NULL) == 0 && WAIT_FOR(pin_done, 6000) && pin_result == INK_PIN_OK, "parent confirms policy");
    CHECK(WAIT_FOR(ink_member_app_mode("kid", "ai") == 0, 6000), "AI policy stored");
    CHECK(ink_member_switch("kid") == 0, "enter child"); delay_ms(100);
    ink_hub_demand(); CHECK(WAIT_FOR(ink_hub_online(), 10000), "disabled child's connection is ready for the policy check");
    on_ui(focus); before = requests(); delay_ms(1400);
    CHECK(requests() == before && !suggestions(), "CHILD_AI: disabled child AI sends nothing");
    double saved = sim_metric_value("settings.saved");
    set("{\"id\":\"text.assist\",\"value\":false}");
    CHECK(WAIT_FOR(sim_metric_value("settings.saved") > saved, 6000), "member preferences saved");
    on_ui(blur); CHECK(WAIT_FOR(ink_storage_pending() == 0, 10000), "writes finish");
    ink_probe_count("test.finished");
}
void ink_main(void) {
    restarting = getenv("INK_ASSIST_RESTART") != NULL;
    product_main();
    osal_task_cfg_t cfg = {.name = "voice-assist-test", .fn = run, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL,
        .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "test task starts");
}
