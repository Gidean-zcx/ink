/* Drive the real phone, contacts, policy, store and simulated AT modem. */
#include <stdlib.h>
#ifndef INK_PHONE_SOURCE
#define INK_PHONE_SOURCE "../../system/apps/phone/phone.c"
#endif
#include INK_PHONE_SOURCE
#define ink_main product_main
#include "../../system/boot/main.c"
#undef ink_main
#include "../integration/whole_product.h"

static char phase[24], scene[24], snap[4096];
static const char *label, *bound;
static lv_obj_t *found;
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void quiet(void) { CHECK(WAIT_FOR(settled(), 10000), "the phone display settles"); }
static void read_env(void *arg) {
    (void)arg;
    snprintf(phase, sizeof phase, "%s", getenv("INK_PHONE_PHASE"));
    const char *s = getenv("INK_PHONE_SCENE"); snprintf(scene, sizeof scene, "%s", s ? s : "");
}
static void read_page(void) {
    CHECK(ink_ui_snapshot(snap, sizeof snap, NULL) > 0, "phone page has a snapshot");
    CHECK(ink_ui_context_snapshot(g_snap, sizeof g_snap, NULL) > 0, "phone snapshot fits the real AI context");
}
static void inspect(void) { on_ui(read_page); }
static void find_control(lv_obj_t *o) {
    if (lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN)) return;
    if ((label && lv_obj_check_type(o, &lv_label_class) && !strcmp(lv_label_get_text(o), label)) ||
        (bound && ink_shell_bound_cap(o) && !strcmp(ink_shell_bound_cap(o), bound))) found = o;
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) find_control(lv_obj_get_child(o, (int32_t)i));
}
static void locate(void) {
    found = NULL; find_control(lv_screen_active()); find_control(lv_layer_sys());
    CHECK(found, "locate visible phone control");
    if (!found) return;
    lv_area_t r; lv_obj_update_layout(found); lv_obj_get_coords(found, &r);
    g_btn_x[0] = (r.x1 + r.x2) / 2; g_btn_y[0] = (r.y1 + r.y2) / 2;
}
static void click(const char *text, const char *cap) {
    label = text; bound = cap; on_ui(locate);
    if (found) tap(g_btn_x[0], g_btn_y[0]);
    quiet();
}
static void open_phone(const char *number) {
    char args[80]; snprintf(args, sizeof args, "{\"number\":\"%s\"}", number);
    CHECK(AI("phone.open", args) && g_rc.status == INK_RC_DONE, "open the real dialler"); quiet();
}
static void recent(void) {
    CHECK(AI("phone.recents", NULL) && g_rc.status == INK_RC_DONE, "open member call history"); quiet();
    CHECK(WAIT_FOR(P.list != NULL, 10000), "background history load finishes"); quiet(); inspect();
}
static void history(int count) {
    recent();
    char total[32]; snprintf(total, sizeof total, "\"total\":%d", count); /* groups: every call here has its own number */
    CHECK(strstr(snap, total), "member has exactly %d history records", count);
    CHECK(P.db && ink_db_count(P.db) == count, "member history holds exactly %d records", count);
    CHECK(AI("data.list", "{\"collection\":\"phone.calls\",\"limit\":10}") && g_rc.status == INK_RC_DONE, "AI reads the same member history through data.list");
}
static void outgoing(void) {
    tap(INK_CANVAS_W / 2, INK_CANVAS_H - INK_MARGIN - INK_DIGIT / 2);
    CHECK(WAIT_FOR(H.active && H.answered, 10000), "the outgoing AT call really connects"); quiet();
    CHECK(!strcmp(ink_ui_page_name(), "PhoneCall"), "dialling opens PhoneCall");
    inspect(); CHECK(strstr(snap, "call.hangup") && strstr(snap, "通话中"), "call snapshot exposes status and the same hangup capability");
}
static void hangup(void) {
    click("挂断", NULL); /* LilyPhone's hang-up button (its own callback calls call.hangup) */

    CHECK(WAIT_FOR(!H.active, 10000), "hangup reaches the call history subscriber");
    CHECK(WAIT_FOR(ink_db_idle(), 10000), "ended call history is durable"); quiet();
}
static void incoming(bool answer) {
    snprintf(g_hw.num, sizeof g_hw.num, "%s", answer ? "00000000002" : "00000000003");
    hw(HW_CALL, 0, 0);
    CHECK(WAIT_FOR(H.active && !strcmp(active_state(), "call"), 10000), "real modem incoming event raises the existing interruption"); quiet();
    if (answer) {
        click("接听", NULL);
        CHECK(WAIT_FOR(H.answered, 10000), "the interruption's answer button calls the real service"); quiet();
    }
    click(answer ? "挂断" : "拒接", NULL);
    CHECK(WAIT_FOR(!H.active && strcmp(active_state(), "call"), 10000), "answer or rejection ends the simulated call");
    CHECK(WAIT_FOR(ink_db_idle(), 10000), "incoming outcome is persisted"); quiet();
}
static void reopen_and_hangup(void) {
    CHECK(BY_HAND("system.home", NULL) && g_rc.status == INK_RC_DONE, "leave the active call for home"); quiet();
    CHECK(BY_HAND("shell.launch", "{\"id\":\"phone\"}") && g_rc.status == INK_RC_DONE, "reopen phone during the call"); quiet(); inspect();
    CHECK(sim_net_call(NULL, 0) == 1, "the AT call remains active after navigation");
    bool resumed = !strcmp(ink_ui_page_name(), "PhoneCall") && strstr(snap, "call.hangup") && strstr(snap, "00000000001");
    CHECK(resumed, "PC-R1-01: active call must remain reachable");
    if (resumed) hangup();
    else { /* Clean up the negative control without cascading failures. */
        CHECK(BY_HAND("call.hangup", NULL) && g_rc.status == INK_RC_DONE, "clean up the unreachable call");
        CHECK(WAIT_FOR(!H.active && ink_db_idle(), 10000), "unreachable call is stored"); quiet();
    }
}
static void check_records(void) {
    ink_db_cursor_t cursor = {0}; ink_rec_t rec; char raw[256] = {0};
    const char *kinds[] = {"打出", "未接", "打来", "打出", "打出"};
    for (unsigned i = 0; i < sizeof kinds / sizeof *kinds; i++) {
        CHECK(ink_db_next(P.db, &cursor, &rec, raw, sizeof raw - 1) == 1 && strstr(raw, kinds[i]), "history record %u has its real call outcome", i);
    }
    CHECK(ink_db_durable_ver(P.db) == ink_db_last_ver(P.db), "all five real call outcomes reached storage");
}
static void isolation(void) {
    CHECK(ink_member_switch("kid") == 0, "switch to synthetic child"); quiet(); history(0);
    CHECK(!strstr(snap, "00000000001"), "child snapshot cannot disclose adult call numbers");
    CHECK(!strstr(g_result, "00000000001"), "child data.list cannot disclose adult calls");
    open_phone("00000000009"); outgoing(); hangup(); history(1);
    CHECK(strstr(snap, "00000000009") && !strstr(snap, "00000000001"), "child keeps an independent history");
    CHECK(ink_member_switch("owner") == 0, "return to synthetic adult"); quiet(); history(5);
    CHECK(!strstr(snap, "00000000009"), "adult snapshot cannot disclose child call numbers");
}
static void write_phase(void) {
    CHECK(BY_HAND("shell.launch", "{\"id\":\"phone\"}") && g_rc.status == INK_RC_DONE, "home phone entry opens Phone"); quiet();
    CHECK(!strcmp(ink_ui_page_name(), "Phone"), "the app directory now enters the dialler");
    click("1", NULL); inspect(); CHECK(strstr(snap, "call.dial") && strstr(snap, "phone.contacts"), "dial snapshot has shared actions");
    CHECK(AI("phone.key", "{\"key\":\"delete\"}") && g_rc.status == INK_RC_DONE, "AI deletes the same digit as the keypad"); quiet();
    CHECK(AI("phone.number", NULL) && g_rc.status == INK_RC_DONE, "phone focuses the F2 number input"); quiet();
    CHECK(AI("text.insert", "{\"text\":\"00000000001\"}") && g_rc.status == INK_RC_DONE, "F2 edits the live dial number");
    CHECK(AI("text.submit", NULL) && g_rc.status == INK_RC_DONE, "F2 submits the live number"); quiet();
    outgoing(); reopen_and_hangup(); incoming(false); history(2); incoming(true); inspect();
    CHECK(ink_db_count(P.db) == 3, "the incoming call is stored while history stays open");
    CHECK(!strcmp(ink_ui_page_name(), "PhoneRecents") && strstr(snap, "\"total\":3") && strstr(snap, "00000000002"), "PC-R1-02: open history must refresh");
    history(3);
    CHECK(strstr(g_result, "未接") && strstr(g_result, "打来") && strstr(g_result, "打出"), "all three call outcomes are readable through the data service");
    open_phone("00000000004");
    CHECK(AI("call.dial", "{\"number\":\"00000000004\"}") && g_rc.status == INK_RC_PENDING_CONFIRMATION, "AI dial still asks for confirmation"); quiet();
    CHECK(!H.active, "AI request has not dialled before human confirmation");
    ink_confirm_view_t pending[1];
    CHECK(ink_confirm_list(pending, 1) == 1 && strstr(pending[0].detail, "00000000004"), "confirmation identifies the exact dial number");
    CHECK(ink_confirm_decide(pending[0].token, true, INK_SRC_AI) != 0, "AI cannot confirm its own dial request");
    CHECK(ink_confirm_decide(pending[0].token, true, INK_SRC_TOUCH) == 0, "human confirms the dial request");
    CHECK(WAIT_FOR(H.active && H.answered, 10000), "confirmed AI call uses the AT model"); quiet(); hangup();
    CHECK(AI("contacts.put", "{\"name\":\"合成家人\",\"number\":\"00000000005\"}") && g_rc.status == INK_RC_DONE, "create a synthetic contact");
    CHECK(AI("phone.contacts", NULL) && g_rc.status == INK_RC_DONE, "open the real contact picker"); quiet();
    CHECK(AI("contacts.edit", "{\"id\":1}") && g_rc.status == INK_RC_DONE, "open the stored contact"); quiet();
    click("选择这个号码", NULL);
    CHECK(WAIT_FOR(!strcmp(ink_ui_page_name(), "Phone"), 10000), "contacts.select returns to the dialler"); quiet(); inspect();
    CHECK(strstr(snap, "00000000005"), "selected number reaches the phone snapshot"); outgoing(); hangup(); history(5);
    on_ui(check_records);
    key(INK_KEY_NEXT); quiet(); inspect(); CHECK(strstr(snap, "\"page\":2") && strstr(snap, "\"visible\":1"), "physical paging shows the final real record");
    CHECK(AI("phone.prev", NULL) && g_rc.status == INK_RC_DONE, "AI uses the same history pager"); quiet(); isolation();
}
static void verify_phase(void) {
    history(5); on_ui(check_records);
    CHECK(ink_member_switch("kid") == 0, "switch to child after a new process boot"); quiet(); history(1);
    CHECK(strstr(snap, "00000000009") && !strstr(snap, "00000000001"), "member isolation and exact history survive reboot");
}
static void add_call_headset(void *arg) { (void)arg; sim_headset_add("00:11:22:33:44:55", "合成耳机"); }
static void lose_call_headset(void *arg) { (void)arg; sim_headset_range("00:11:22:33:44:55", false); }
static void recording_reason(void) {
    CHECK(P.notice && strstr(lv_label_get_text(P.notice), "暂不能录音"), "record button explains why recording is unavailable");
}
static void controls_phase(void) {
    open_phone("00000000001"); outgoing();
    CHECK(sim_net_call_audio(160) == 3, "unmuted call has uplink and speaker audio");
    click("静音", NULL);
    CHECK(WAIT_FOR(P.muted, 3000), "mute button waits for modem acknowledgement and readback"); quiet(); inspect();
    CHECK(strstr(snap, "已静音") && sim_net_call_audio(160) == 2, "muted call has no uplink, but downlink stays audible");
    CHECK(AI("call.mute", "{\"on\":false}") && g_rc.status == INK_RC_DONE, "AI unmutes through the same capability without confirmation");
    CHECK(WAIT_FOR(!P.muted, 3000) && sim_net_call_audio(160) == 3, "modem readback and uplink recover"); quiet();
    click("耳机未连", NULL);
    CHECK(ink_audio_route() == INK_ROUTE_SPEAKER, "disconnected headset button leaves the actual route alone");
    CHECK(!strcmp(ink_ui_page_name(), "SetBluetooth"), "disconnected headset button opens Bluetooth settings");
    CHECK(BY_HAND("shell.back", NULL) && g_rc.status == INK_RC_DONE, "return from Bluetooth settings"); quiet();
    CHECK(!strcmp(ink_ui_page_name(), "PhoneCall") && H.active, "Bluetooth settings preserves the active call");
    click("键盘", NULL); inspect();
    CHECK(!strcmp(ink_ui_page_name(), "PhoneKeypad") && strstr(snap, "已发送的按键") && strstr(snap, "call.dtmf"), "keypad has the real AI snapshot and digit actions");
    click("1", NULL); click("0", NULL); click("*", NULL); click("#", NULL);
    CHECK(!strcmp(sim_net_call_tones(), "10*#") && !strcmp(P.tones, "10*#"), "touch digits equal the sequence accepted by AT+VTS");
    CHECK(AI("call.dtmf", "{\"key\":\"2\"}") && g_rc.status == INK_RC_PENDING_CONFIRMATION, "unrequested AI DTMF uses the D-105 external-action gate");
    CHECK(!strcmp(sim_net_call_tones(), "10*#"), "unconfirmed AI has sent no tone");
    ink_confirm_view_t pending[1];
    CHECK(ink_confirm_list(pending, 1) == 1, "DTMF confirmation is visible");
    CHECK(ink_confirm_decide(pending[0].token, true, INK_SRC_AI) != 0, "AI cannot confirm its own DTMF");
    CHECK(ink_confirm_decide(pending[0].token, true, INK_SRC_TOUCH) == 0, "human confirms the exact digit");
    CHECK(WAIT_FOR(!strcmp(P.tones, "10*#2"), 3000) && !strcmp(sim_net_call_tones(), "10*#2"), "confirmed AI and touch share the modem path"); quiet(); inspect();
    CHECK(!strstr(snap, "10*#2") && strstr(snap, "\"secret\":true") && strstr(snap, "有值"), "acknowledged digits remain secret in the keypad snapshot");
    CHECK(BY_HAND("call.dtmf", "{\"key\":\"Z\"}") && g_rc.status == INK_RC_INVALID && !strcmp(sim_net_call_tones(), "10*#2"), "invalid DTMF never reaches the modem");
    click(NULL, "phone.keypad");
    vtk_event_at(vtk_now() + SIM_MS(1), add_call_headset, NULL); delay_ms(5);
    CHECK(BY_HAND("bt.pair", "{\"addr\":\"00:11:22:33:44:55\"}") && g_rc.status == INK_RC_DONE, "pair synthetic call headset");
    CHECK(BY_HAND("bt.connect", "{\"addr\":\"00:11:22:33:44:55\"}") && g_rc.status == INK_RC_DONE, "connect during the active call");
    CHECK(WAIT_FOR(ink_audio_route() == INK_ROUTE_HEADSET, 3000), "active call really switches the audio chip mode"); quiet(); inspect();
    CHECK(sim_net_call_audio(160) == 5 && strstr(snap, "声音走耳机"), "downlink goes only to headset and UI tells the actual route");
    click("耳机", NULL);
    CHECK(WAIT_FOR(ink_audio_route() == INK_ROUTE_SPEAKER, 3000) && sim_net_call_audio(160) == 3, "call button switches back to speaker"); quiet();
    CHECK(AI("call.route", "{\"headset\":true}") && g_rc.status == INK_RC_DONE, "AI switches to the connected headset without confirmation");
    CHECK(WAIT_FOR(ink_audio_route() == INK_ROUTE_HEADSET, 3000), "AI route request reaches hardware");
    vtk_event_at(vtk_now() + SIM_MS(1), lose_call_headset, NULL);
    CHECK(WAIT_FOR(ink_audio_route() == INK_ROUTE_SPEAKER, 3000), "headset loss returns to the actual speaker route"); quiet();
    click("录音", NULL); on_ui(recording_reason); inspect();
    CHECK(strstr(snap, "通话音频接线未核实，暂不能录音"), "AI sees the honest recording limitation");
    CHECK(AI("call.record", NULL) && g_rc.status == INK_RC_FAILED && strstr(g_rc.say, "暂不能录音"), "unavailable AI recording reports the same reason without opening a microphone");
    CHECK(ink_member_switch("kid") == 0, "switch away from the call owner"); quiet();
    CHECK(AI("call.mute", "{\"on\":true}") && g_rc.status == INK_RC_STALE, "child cannot control the adult call");
    CHECK(AI("phone.keypad", NULL) && g_rc.status == INK_RC_STALE, "another member cannot open the owned call keypad"); quiet(); inspect();
    CHECK(!strstr(snap, "00000000001") && !strstr(snap, "10*#2"), "child keypad discloses neither number nor entered tones");
    CHECK(ink_member_switch("owner") == 0, "return to call owner"); quiet();
    CHECK(BY_HAND("phone.return", NULL) && g_rc.status == INK_RC_DONE, "return to the owned call"); quiet(); hangup();
    CHECK(!P.tones[0] && !sim_net_call_tones()[0] && sim_net_call_audio(160) == 0, "hangup clears both digit displays and all call audio");
    CHECK(BY_HAND("call.mute", "{\"on\":true}") && g_rc.status == INK_RC_STALE, "ended calls reject mute");
}
static void audit7_rebind(void) {
    open_phone("00000000001"); outgoing();
    CHECK(AI("call.dtmf", "{\"key\":\"7\"}") && g_rc.status == INK_RC_PENDING_CONFIRMATION,
        "DTMF waits for a human");
    ink_confirm_view_t pending = {0};
    CHECK(ink_confirm_list(&pending, 1) == 1, "keep the original confirmation token");
    CHECK(strstr(pending.detail, "00000000001"), "B7-01: confirmation names the original peer");
    CHECK(ink_confirm_decide(pending.token, true, INK_SRC_AI) < 0, "AI cannot approve DTMF");
    hw(HW_HANGUP, 0, 0);
    CHECK(WAIT_FOR(!H.active, 10000), "first peer ends the call"); quiet();
    ink_confirm_view_t remaining;
    CHECK(ink_confirm_list(&remaining, 1) == 0, "B7-01: ending the call removes its pending card");
    snprintf(g_hw.num, sizeof g_hw.num, "00000000002"); hw(HW_CALL, 0, 0);
    CHECK(WAIT_FOR(H.active, 10000), "second peer rings");
    CHECK(BY_HAND("call.answer", NULL) && g_rc.status == INK_RC_DONE && WAIT_FOR(H.answered, 10000), "answer the second call"); quiet();
    CHECK(ink_confirm_decide(pending.token, true, INK_SRC_TOUCH) < 0, "B7-01: old card cannot execute on the new call");
    CHECK(AI("call.record", NULL) && g_rc.status == INK_RC_FAILED, "drain prior controls through the same background executor");
    ink_db_t *results = ink_db_open(ink_db_find_def("confirm.results"), NULL);
    CHECK(results && ink_db_wait(results, ink_db_last_ver(results), 10000000) == 0, "confirmation result reaches durable storage");
    ink_db_close(results);
    char args[64]; snprintf(args, sizeof args, "{\"token\":%u}", (unsigned)pending.token);
    CHECK(AI("confirm.status", args) && g_rc.status == INK_RC_DONE && strstr(g_result, "\"state\":\"expired\""),
        "B7-01: old confirmation receipt is expired: %.150s", g_result);
    CHECK(!sim_net_call_tones()[0], "B7-01: second modem call receives no old tone");
    CHECK(call_as("call.dtmf", "{\"key\":\"7\",\"_call\":1,\"_peer\":\"00000000001\"}", INK_SRC_CONFIRMED) &&
        g_rc.status == INK_RC_STALE && !sim_net_call_tones()[0], "B7-01: execution rejects an already accepted old generation");
    CHECK(BY_HAND("call.hangup", NULL) && g_rc.status == INK_RC_DONE, "end second call");
}
static unsigned audit7_events;
static void audit7_event(void *ctx, const char *id, const ink_args_t *a) {
    (void)ctx; (void)id;
    char payload[1024];
    CHECK(ink_args_to_json(a, payload, sizeof payload) > 0, "serialize real controls event");
    CHECK(!ink_args_find(a, "key") && !strstr(payload, "246802"), "B7-02: controls event contains no key value");
    audit7_events++;
}
static void audit7_privacy(void) {
    open_phone("00000000001"); outgoing(); click("键盘", NULL);
    CHECK(ink_cap_subscribe("call.controls", audit7_event, NULL, INK_EXEC_DIRECT) == 0, "observe real controls payloads");
    const char *keys[] = {"2", "4", "6", "8", "0", "2"};
    for (unsigned i = 0; i < sizeof keys / sizeof *keys; i++) click(keys[i], NULL);
    CHECK(audit7_events == 6 && !strcmp(sim_net_call_tones(), "246802") && !strcmp(P.tones, "246802"),
        "real touches reach the modem and stay visible locally");
    inspect();
    CHECK(!strstr(snap, "246802") && !strstr(g_snap, "246802") && strstr(snap, "\"secret\":true") &&
        strstr(snap, "有值") && strstr(snap, "\"id\":\"tone_count\"") && strstr(snap, "\"value\":\"6\""),
        "B7-02: AI snapshots reveal only presence and digit count");
    CHECK(AI("call.dtmf", "{\"key\":\"3\"}") && g_rc.status == INK_RC_PENDING_CONFIRMATION &&
        !strcmp(sim_net_call_tones(), "246802"), "AI DTMF still requires confirmation");
    ink_confirm_view_t pending;
    CHECK(ink_confirm_list(&pending, 1) == 1 && ink_confirm_decide(pending.token, false, INK_SRC_TOUCH) == 0, "reject AI tone");
    CHECK(WAIT_FOR(ink_db_idle(), 10000), "activity records settle");
    ink_db_t *db = ink_db_open_member(ink_db_find_def("activity.log"), "owner");
    ink_db_cursor_t cursor = {0}; ink_rec_t rec; char raw[2048]; unsigned count = 0;
    CHECK(db != NULL, "open the real activity log");
    while (db && ink_db_next(db, &cursor, &rec, raw, sizeof raw - 1) == 1) {
        raw[rec.len] = 0; count++;
        CHECK(!strstr(raw, "246802") && !strstr(raw, "\"key\""), "B7-02: activity records contain no entered keys");
    }
    CHECK(count > 0, "inspect actual activity records"); ink_db_close(db); hangup();
}
static void capture(void *arg) {
    (void)arg; CHECK(settled(), "comparison frame is fully presented");
    char key_name[64]; snprintf(key_name, sizeof key_name, "test.snapshot.%s", scene);
    ink_probe_count("test.snapshot.settled"); ink_probe_count(key_name); sim_display_snapshot("scene");
}
static void scene_phase(void) {
    if (!strcmp(scene, "Phone")) open_phone("00000000001");
    else if (!strcmp(scene, "PhoneRecents")) history(5);
    else { open_phone("00000000001"); outgoing(); }
    if (!strcmp(scene, "PhoneKeypad")) { click("键盘", NULL); click("1", NULL); }
    inspect(); quiet(); vtk_event_at(vtk_now() + SIM_MS(1), capture, NULL); delay_ms(5);
}
static void run(void *arg) {
    (void)arg; vtk_host_call(read_env, NULL);
    CHECK(WAIT_FOR(ink_member_space_open(), 10000), "member space opens"); quiet(); test_start_home(); quiet();
    ink_time_set(1793803620, false);
    if (!strcmp(phase, "write")) write_phase();
    else if (!strcmp(phase, "verify")) verify_phase();
    else if (!strcmp(phase, "controls")) controls_phase();
    else if (!strcmp(phase, "audit7-rebind")) audit7_rebind();
    else if (!strcmp(phase, "audit7-privacy")) audit7_privacy();
    else scene_phase();
    CHECK(WAIT_FOR(ink_db_idle(), 10000), "no phone writes remain outstanding");
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name = "phone-test", .fn = run, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start phone acceptance driver");
}
