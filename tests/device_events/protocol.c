#include "../services_finish/product.h"

void test_cap_directory(void), test_hub_budget(void), test_ai_downlink(void), test_ai_begin(void), test_ai_limit(void);
void test_hub_reconnect(void);
void test_cap_accept_handoff(void);
void test_ai_queue_speech(void), test_ai_speech_revoked(void);
void test_ai_stale_question(void), test_ai_stale_result(void);
bool test_ai_stale_done(void);
void test_on_hub(void (*fn)(void));
uint32_t test_event_boot(void), test_event_acked(void);
void test_event_ack(uint32_t boot, uint32_t upto);
static int runs, closed;
static volatile bool injected, ui_waiting;
static char injection[512];
static osal_sem_t *ui_release;
static ink_exec_job_t ui_hold;
static void bump(ink_call_t *c) { (void)c; runs++; }
static const ink_cap_t FIXTURE[] = {
    {.id="fixture.bump", .title="合成调用", .effect=INK_FX_LOCAL_WRITE, .child_ok=true, .exec=INK_EXEC_DIRECT, .run=bump},
    {.id="fixture.event", .title="合成事件", .form=INK_FORM_EVENT, .child_ok=true},
};
static void inject(void *arg) {
    (void)arg;
    sim_hub_request_device_raw("tools/call", injection);
    injected = true;
}
static void send_named(const char *name, const char *meta) {
    snprintf(injection, sizeof injection, "{\"name\":\"%s\",\"arguments\":{}%s%s}", name, meta ? ",\"_meta\":" : "", meta ? meta : "");
    injected = false;
    vtk_event_after(SIM_MS(1), inject, NULL);
    CHECK(WAIT_FOR(injected, 1000), "request injected");
}
static void send_call(const char *meta) { send_named("fixture.bump", meta); }
static bool reply_is(const char *text) {
    const char *reply = sim_hub_device_reply();
    return reply && strstr(reply, text);
}
static void hold_ui(void *arg) {
    (void)arg;
    ui_waiting = true;
    osal_sem_take(ui_release, OSAL_WAIT_FOREVER);
}
static void session_closed(void *ctx, int sid, int reason) { (void)ctx; (void)sid; (void)reason; closed++; }
static void ack_off(void *arg) { (void)arg; sim_hub_event_acks(false); injected = true; }
static void ack_on(void *arg) { (void)arg; sim_hub_event_acks(true); injected = true; }
static void run(void *arg) {
    (void)arg;
    ink_hub_demand();
    CHECK(WAIT_FOR(ink_hub_online() && ink_cap_available("asr.stream"), 30000), "hub online with complete directory");
    test_on_hub(test_cap_directory);
    CHECK(WAIT_FOR(ink_cap_available("asr.stream"), 2000), "normal directory restored");
    test_on_hub(test_hub_budget);
    CHECK(WAIT_FOR(sim_hub_last_ask() && strstr(sim_hub_last_ask(), "SYNTHETIC_BUDGET"), 2000), "actual wire request recorded");
    ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL);
    int64_t budget = 0;
    const char *ask = sim_hub_last_ask();
    CHECK(j && ask && ink_json_parse(j, ask, strlen(ask)) == 0 &&
        ink_json_int(j, ink_json_find(j, 0, "params.budget_ms"), &budget) && budget > 7000 && budget <= 7123,
        "wire budget is remaining time, not the original 45 seconds");
    ink_args_t *a = osal_malloc(sizeof *a, OSAL_MEM_EXTERNAL);
    ink_args_init(a); ink_args_str(a, "value", "SYNTHETIC_EVENT");
    int before = sim_hub_events_seen();
    ink_cap_emit("fixture.event", a);
    CHECK(WAIT_FOR(sim_hub_events_seen() > before, 2000), "event delivered");
    const char *event = sim_hub_event_at(sim_hub_events_seen() - 1);
    int64_t boot = 0, seq = 0;
    char member[16] = "";
    CHECK(event && !ink_json_parse(j, event, strlen(event)) && ink_json_int(j, ink_json_find(j, 0, "params.seq"), &seq) && seq > 0 &&
        ink_json_int(j, ink_json_find(j, 0, "params.boot"), &boot) && boot == test_event_boot() &&
        ink_json_str(j, ink_json_find(j, 0, "params.member"), member, sizeof member) > 0 && !strcmp(member, "owner"),
        "event carries boot, sequence and emitting member");
    CHECK(WAIT_FOR(test_event_acked() == (uint32_t)seq, 2000), "event ACK consumed");
    test_event_ack((uint32_t)boot + 1, (uint32_t)seq + 100);
    CHECK(test_event_acked() == (uint32_t)seq, "wrong boot and future ACK cannot clear events");
    test_event_ack((uint32_t)boot, (uint32_t)seq + 100);
    CHECK(test_event_acked() == (uint32_t)seq, "future ACK on this boot is also rejected");
    injected = false; vtk_event_after(SIM_MS(1), ack_off, NULL);
    CHECK(WAIT_FOR(injected, 1000), "ACK loss enabled");
    before = sim_hub_events_seen();
    ink_cap_emit("fixture.event", a);
    CHECK(WAIT_FOR(sim_hub_events_seen() > before, 2000) && test_event_acked() == (uint32_t)seq, "unacknowledged event retained");
    before = sim_hub_events_seen();
    test_on_hub(test_hub_reconnect);
    CHECK(WAIT_FOR(sim_hub_events_seen() > before, 30000), "unacknowledged event replayed after reconnect");
    event = sim_hub_event_at(sim_hub_events_seen() - 1);
    int64_t replay_seq = 0, replay_boot = 0;
    CHECK(event && !ink_json_parse(j, event, strlen(event)) && ink_json_int(j, ink_json_find(j, 0, "params.seq"), &replay_seq) &&
        replay_seq == seq + 1 && ink_json_int(j, ink_json_find(j, 0, "params.boot"), &replay_boot) && replay_boot == boot,
        "replay retains the exact event identity");
    injected = false; vtk_event_after(SIM_MS(1), ack_on, NULL);
    CHECK(WAIT_FOR(injected, 1000), "ACK delivery restored");
    test_on_hub(test_hub_reconnect);
    CHECK(WAIT_FOR(test_event_acked() == (uint32_t)replay_seq, 30000), "replayed event can be acknowledged");
    send_call(NULL);
    CHECK(WAIT_FOR(reply_is("-32023"), 2000) && runs == 0, "missing member metadata rejected");
    send_call("{\"member\":\"kid\",\"epoch\":1}");
    CHECK(WAIT_FOR(reply_is("-32023"), 2000) && runs == 0, "wrong member rejected");
    char meta[128];
    snprintf(meta, sizeof meta, "{\"member\":\"owner\",\"epoch\":%u}", ink_hub_member_epoch());
    send_call(meta);
    CHECK(WAIT_FOR(reply_is("\"status\":\"done\""), 2000) && runs == 1, "matching metadata invokes the atomic capability");
    test_on_hub(test_cap_accept_handoff);
    CHECK(WAIT_FOR(reply_is("\"id\":2000000000"), 2000) && reply_is("-32023") && runs == 1,
          "validation-to-allocation handoff cannot rebind a delivered call");
    snprintf(meta, sizeof meta, "{\"member\":\"owner\",\"epoch\":%u,\"asked\":true,\"undo_ms\":3000}", ink_hub_member_epoch());
    send_named("message.send", meta);
    CHECK(WAIT_FOR(reply_is("-32021"), 2000) && sim_net_sms_sent() == 0, "unsupported automatic window never sends a message immediately");
    snprintf(meta, sizeof meta, "{\"member\":\"owner\",\"epoch\":%u,\"undo_ms\":0}", ink_hub_member_epoch());
    send_call(meta);
    CHECK(WAIT_FOR(reply_is("-32602"), 2000) && runs == 1, "shortened undo window rejected");
    snprintf(meta, sizeof meta, "{\"member\":\"owner\",\"epoch\":%u}", ink_hub_member_epoch());
    on_ui(test_ai_queue_speech);
    ui_release = osal_sem_create(0, 1);
    CHECK(ink_exec_post(INK_EXEC_UI, &ui_hold, hold_ui, NULL) == 0 && WAIT_FOR(ui_waiting, 1000), "UI executor held");
    double accepted = sim_metric_value("tools.accepted");
    send_call(meta);
    CHECK(WAIT_FOR(sim_metric_value("tools.accepted") > accepted, 2000), "call waiting in UI queue");
    uint32_t epoch = ink_hub_member_epoch();
    CHECK(ink_member_switch("kid") == 0 && ink_member_switch("owner") == 0 && ink_hub_member_epoch() > epoch, "A to B to A advances delivery epoch");
    test_ai_speech_revoked();
    osal_sem_give(ui_release);
    CHECK(WAIT_FOR(reply_is("-32023"), 2000) && runs == 1, "queued old incarnation cannot execute after returning to same member");
    ink_rt_handler_t h = {.on_closed=session_closed};
    int sid = ink_rt_open("echo", "{}", &h);
    CHECK(sid > 0 && WAIT_FOR(ink_rt_ready(sid), 2000), "realtime fixture open");
    CHECK(ink_member_switch("kid") == 0 && !ink_rt_is_open(sid) && closed == 1, "member boundary closes realtime session");
    CHECK(ink_member_switch("owner") == 0, "back to owner");
    CHECK(WAIT_FOR(ink_hub_online() && ink_cap_available("asr.stream"), 30000), "new member directory");
    double questions = sim_metric_value("hub.requests.ai.ask");
    on_ui(test_ai_stale_question);
    CHECK(WAIT_FOR(test_ai_stale_done(), 3000), "prepared question receives a terminal receipt");
    test_ai_stale_result();
    CHECK(sim_metric_value("hub.requests.ai.ask") == questions, "old private payload never reaches the hub under the new identity");
    on_ui(test_ai_downlink);
    on_ui(test_ai_begin);
    delay_ms(31000); /* Exercise the real recording limit with the key held. */
    on_ui(test_ai_limit);
    osal_free(a); osal_free(j);
    ink_probe_value("test.event_boot", test_event_boot());
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    CHECK(ink_cap_register(FIXTURE, 2) == 0, "fixture capabilities registered");
    osal_task_cfg_t cfg = {.name="device-events", .fn=run, .stack_bytes=16384, .priority=OSAL_PRIO_NORMAL, .core=-1, .flags=OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "fixture task");
}
