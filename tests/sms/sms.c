/* Exercise the real app and modem model; the mutant removes the member store boundary. */
#include <stdlib.h>
#include "ink/sys/data.h"
#ifdef INK_SMS_NO_MEMBER_CHECK
#define ink_db_open_cached_member(def, member) ink_db_open_cached_member(def, "owner")
#endif
#include "../../system/apps/sms/sms.c"
#undef ink_db_open_cached_member
#define ink_main product_main
#include "../../system/boot/main.c"
#undef ink_main
#include "../integration/whole_product.h"

static char phase[24], body[355], arguments[768], snap[4096], sent_text[400], sent_number[24];
static const char PEER[] = "00000000001";
static const char *stage;
static void write_stage(void *arg) { (void)arg; if (fprintf(stderr, "sms-stage: %s\n", stage) < 0 || fflush(stderr)) abort(); }
static void progress(const char *value) { stage = value; vtk_host_call(write_stage, NULL); }
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void quiet(void) { CHECK(WAIT_FOR(settled(), 10000), "SMS display settles"); }
static void read_phase(void *arg) { (void)arg; snprintf(phase, sizeof phase, "%s", getenv("INK_SMS_PHASE")); }
static void snapshot_page(void) {
    CHECK(ink_ui_snapshot(snap, sizeof snap, NULL) > 0, "real SMS page has an AI snapshot");
    CHECK(ink_ui_context_snapshot(g_snap, sizeof g_snap, NULL) > 0, "SMS snapshot fits the production AI buffer");
}
static void open_list(void) {
    CHECK(BY_HAND("shell.launch", "{\"id\":\"sms\"}") && g_rc.status == INK_RC_DONE, "system launcher opens SMS");
    CHECK(WAIT_FOR(S.list != NULL, 10000), "member data loads on the background executor"); quiet(); on_ui(snapshot_page);
}
static int stored(const char *member) {
    ink_db_t *db = ink_db_open_member(&MESSAGES, member);
    int count = db ? ink_db_count(db) : -1; ink_db_close(db); return count;
}
static bool has_status(const char *status) {
    ink_db_t *db = ink_db_open_member(&MESSAGES, "owner"); ink_db_cursor_t cur = {0}; ink_rec_t rec;
    char peer[24], text[355], value[96]; bool found = false;
    while (db && ink_db_next(db, &cur, &rec, NULL, 0) == 1) {
        if (read_message(db, rec.id, peer, text, value) && !strcmp(value, status)) found = true;
    }
    CHECK(db && ink_db_durable_ver(db) == ink_db_last_ver(db), "receipt follows durable SMS storage");
    ink_db_close(db); return found;
}
static void incoming(void *arg) { (void)arg; CHECK(sim_net_deliver_sms(PEER, "合成短信：回家后联系"), "modem accepts synthetic incoming SMS"); }
static void deliver(void) {
    int before = stored("owner"); vtk_event_at(vtk_now() + SIM_MS(1), incoming, NULL);
    CHECK(WAIT_FOR(stored("owner") == before + 1, 10000), "message.received reaches the durable conversation");
}
static void capture(void *arg) { (void)arg; CHECK(settled(), "capture is settled"); sim_display_snapshot("scene"); }
static void scene(const char *name) {
    quiet(); on_ui(snapshot_page); ink_probe_count("test.snapshot.settled");
    char counter[64]; snprintf(counter, sizeof counter, "test.snapshot.%s", name); ink_probe_count(counter);
    vtk_event_at(vtk_now() + SIM_MS(1), capture, NULL); delay_ms(5);
}
static void type_short(void) {
    CHECK(S.field && ink_text_focused(), "F2 input is focused");
    ink_text_commit_utf8("合成短短信"); ink_text_enter();
}
static void type_long(void) {
    CHECK(S.field && ink_text_focused(), "F3 long input is focused");
    ink_text_select(0, ink_text_char_offset(ink_text_value(), strlen(ink_text_value()))); ink_text_commit_utf8(body);
}
static void inspect_long(void) {
    CHECK(S.field && !strcmp(ink_textfield_text(S.field), body), "long input retains every Chinese character");
    CHECK(S.field && ink_textfield_long_snapshot(S.field, snap, sizeof snap) > 0 && strstr(snap, "ImeLong"), "long editor exposes its owned viewport");
}
static void inspect_thread(void) {
    snapshot_page(); CHECK(S.list && S.count && strstr(snap, body) && strstr(snap, "已发出"), "thread snapshot shows the complete long outgoing SMS and receipt");
    lv_obj_t *root = lv_screen_active(); (void)root;
}
static void inspect_compose(void) { snapshot_page(); CHECK(S.field && strstr(snap, "sms.send") && strstr(snap, "00000000001"), "compose snapshot exposes its exact recipient and send capability"); }
static lv_obj_t *located;
static const char *wanted;
static void find_label(lv_obj_t *o) {
    if (lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN)) return;
    if (lv_obj_check_type(o, &lv_label_class) && !strcmp(lv_label_get_text(o), wanted)) located = o;
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) find_label(lv_obj_get_child(o, i));
}
static void locate(void) {
    located = NULL; find_label(lv_screen_active()); CHECK(located, "visible SMS button: %s", wanted);
    if (located) { lv_area_t a; lv_obj_update_layout(lv_obj_get_parent(located)); lv_obj_get_coords(lv_obj_get_parent(located), &a); g_btn_x[0] = (a.x1 + a.x2) / 2; g_btn_y[0] = (a.y1 + a.y2) / 2; }
}
static void tap_text(const char *label) { wanted = label; on_ui(locate); if (located) tap(g_btn_x[0], g_btn_y[0]); quiet(); }
static void no_registration(void) { ink_state_set_bool(ink_state_find("cell.registered"), false); }
static void restore_registration(void) { ink_state_set_bool(ink_state_find("cell.registered"), true); }
static void fail_modem(void *arg) { (void)arg; sim_net_set_card(0, true, false, true); }
static void restore_modem(void *arg) { (void)arg; sim_net_set_card(0, true, true, true); }
static volatile bool confirmation_done;
static char confirmation_status[24];
static void confirmed(void *ctx, const char *event, const ink_args_t *a) {
    (void)ctx; (void)event; ink_args_get_str(a, "status", confirmation_status, sizeof confirmation_status); confirmation_done = true;
}
static void compose(void) {
    progress("contacts.put");
    CHECK(AI("contacts.put", "{\"name\":\"合成收件人\",\"number\":\"00000000001\"}") && g_rc.status == INK_RC_DONE, "create only a synthetic contact");
    progress("compose"); CHECK(BY_HAND("sms.compose", NULL) && g_rc.status == INK_RC_DONE, "compose starts with contact selection");
    CHECK(WAIT_FOR(S.list != NULL, 10000), "F4 contact selection loads"); quiet();
    progress("select"); tap_text("合成收件人"); CHECK(WAIT_FOR(S.field != NULL, 10000), "touch selection goes through contacts.select and opens the composer");
    CHECK(!strcmp(S.to, PEER), "selected contact provides the exact number"); on_ui(inspect_compose);
}
static void isolation(void) {
    CHECK(ink_member_switch("kid") == 0, "switch to synthetic child"); quiet(); open_list();
    CHECK(strstr(snap, "\"total\":0") && !strstr(snap, "合成短信"), "child cannot read adult SMS");
    CHECK(AI("data.list", "{\"collection\":\"sms.messages\"}") && g_rc.status == INK_RC_DONE && !strstr(g_result, "合成短信"), "AI data reads obey the same member boundary");
    CHECK(AI("sms.thread", "{\"peer\":\"00000000001\"}") && g_rc.status == INK_RC_DONE, "child may open only their own empty conversation"); quiet(); on_ui(snapshot_page);
    CHECK(strstr(snap, "\"total\":0"), "guessing an adult peer does not expose a record");
    CHECK(ink_member_switch("owner") == 0, "return to synthetic adult"); quiet(); open_list();
    CHECK(strstr(snap, "合成短信") || strstr(snap, "已发出") || S.count > 0, "adult retains their conversations");
}
static void run(void *arg) {
    (void)arg; vtk_host_call(read_phase, NULL);
    CHECK(WAIT_FOR(ink_member_space_open(), 10000), "member space opens");
    CHECK(WAIT_FOR(sim_net_modem_registered(), 10000), "simulated AT modem registers"); delay_ms(5000); quiet();
    for (int i = 0; i < 118; i++) memcpy(body + i * 3, "中", 3); body[354] = 0;
    if (!strcmp(phase, "verify")) {
        open_list(); CHECK(stored("owner") == 21 && has_status("收到") && has_status("已发出") && has_status("发送失败，未发出") && has_status("结果未知，请先核实"), "inbound history beyond 16 and all outgoing results survive a fresh process"); isolation();
    } else if (!strcmp(phase, "SmsCompose")) { open_list(); compose(); scene("SmsCompose"); }
    else if (!strcmp(phase, "SmsThread")) {
        open_list(); snprintf(arguments, sizeof arguments, "{\"to\":\"%s\",\"text\":\"%s\"}", PEER, body);
        CHECK(BY_HAND("sms.send", arguments) && g_rc.status == INK_RC_DONE, "send long fixture through the AT model");
        CHECK(AI("sms.thread", "{\"peer\":\"00000000001\"}") && g_rc.status == INK_RC_DONE, "open the long conversation"); quiet(); on_ui(inspect_thread); scene("SmsThread");
    } else {
        open_list(); CHECK(stored("owner") == 0, "fresh SMS page has no invented data");
        for (int i = 0; i < 17; i++) deliver(); open_list(); CHECK(S.count == 1, "17 incoming messages group into one peer conversation"); scene("Sms");
        compose(); progress("short"); CHECK(AI("sms.text", NULL) && g_rc.status == INK_RC_DONE, "focus the F2 short host"); on_ui(type_short);
        progress("long"); CHECK(AI("sms.long", NULL) && g_rc.status == INK_RC_DONE, "open F3 long editor");
        on_ui(type_long);
        on_ui(inspect_long);
        CHECK(BY_HAND("shell.back", NULL) && g_rc.status == INK_RC_DONE, "return long text to composer"); quiet(); on_ui(inspect_compose);
        int count = stored("owner"); tap_text("发送"); CHECK(WAIT_FOR(stored("owner") == count + 1 && has_status("已发出"), 10000), "physical send button persists and sends once");
        CHECK(sim_net_sent_sms(0, sent_number, sizeof sent_number, sent_text, sizeof sent_text) == 1 && !strcmp(sent_text, body) && !strcmp(sent_number, PEER), "118 Chinese characters arrive unchanged at the simulated AT boundary");
        CHECK(AI("sms.thread", "{\"peer\":\"00000000001\"}") && g_rc.status == INK_RC_DONE, "open long message"); quiet(); on_ui(inspect_thread);
        snprintf(arguments, sizeof arguments, "{\"to\":\"%s\",\"text\":\"合成回执\"}", PEER);
        on_ui(no_registration); CHECK(BY_HAND("sms.send", arguments) && g_rc.status != INK_RC_DONE && has_status("发送失败，未发出"), "known preflight failure is persisted as not sent"); on_ui(restore_registration);
        vtk_event_at(vtk_now() + SIM_MS(1), fail_modem, NULL); delay_ms(5);
        CHECK(BY_HAND("sms.send", arguments) && g_rc.status == INK_RC_FAILED && has_status("结果未知，请先核实"), "undifferentiated modem error is honestly unknown");
        vtk_event_at(vtk_now() + SIM_MS(1), restore_modem, NULL); delay_ms(5);
        CHECK(ink_cap_subscribe("confirm.result", confirmed, NULL, INK_EXEC_DIRECT) == 0, "observe actual confirmation receipts");
        int before = sim_net_sms_sent(); CHECK(AI("sms.send", arguments) && g_rc.status == INK_RC_PENDING_CONFIRMATION, "unsolicited AI send requires confirmation");
        ink_confirm_view_t pending[1]; CHECK(ink_confirm_list(pending, 1) == 1 && sim_net_sms_sent() == before, "confirmation holds exact external effect");
        confirmation_done = false; CHECK(ink_confirm_decide(pending[0].token, true, INK_SRC_TOUCH) == 0, "human accepts the send card");
        CHECK(WAIT_FOR(confirmation_done, 10000) && !strcmp(confirmation_status, "done") && sim_net_sms_sent() == before + 1, "confirmed AI send uses the same durable provider once");
        isolation(); CHECK(stored("owner") == 21, "adult retains every received and sent record");
    }
    CHECK(WAIT_FOR(ink_db_idle() && !ink_storage_pending(), 10000), "all SMS records are durable before ending"); ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main(); osal_task_cfg_t cfg = {.name = "sms-test", .fn = run, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start bounded SMS acceptance test");
}
