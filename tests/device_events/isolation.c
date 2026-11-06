#include "../services_finish/product.h"

void test_member_pause(void), test_member_release(void), test_member_pause_done(void), test_member_change(const char *id);
bool test_member_paused(void), test_member_change_done(void);
void test_ai_identity_ask(void), test_ai_identity_posts(void), test_ai_identity_empty(void), test_ai_identity_new(void);
void test_ai_identity_current(void), test_ai_identity_voice_closed(void);
void test_ai_identity_switch(void), test_ai_identity_switch_and_ask(void);
void test_ai_identity_failed_restart(void), test_ai_identity_failed_clean(void);
void test_ai_review_voice(void), test_ai_review_release(void);

static const ink_cap_t CAPS[] = {
    {.id="fixture.publish_event", .title="合成异步事件", .form=INK_FORM_EVENT, .child_ok=true},
};
static volatile int pin_done, pin_result, stale_status;
static void pin_told(void *ctx, int result, uint32_t wait_s) {
    (void)ctx; (void)wait_s;
    pin_result = result; pin_done = 1;
}
static void stale_told(void *ctx, int status, const ink_json_t *j, int result) {
    (void)ctx; (void)j; (void)result;
    stale_status = status;
}
static void ready(void) {
    CHECK(WAIT_FOR(ink_hub_online() && ink_cap_available("voice.chat") &&
          ink_hub_member_matches(ink_member_current()->id, ink_hub_member_epoch(), ink_hub_connection()), 30000),
          "actual receiving connection synchronized to the open local identity");
}
static void emit(const char *member, const char *value) {
    ink_args_t *a = osal_malloc(sizeof *a, OSAL_MEM_EXTERNAL);
    CHECK(a != NULL, "event payload allocated");
    if (!a) return;
    ink_args_init(a); ink_args_str(a, "value", value);
    ink_cap_emit_for("fixture.publish_event", a, member);
    osal_free(a);
}
static bool event_received(int first, const char *value) {
    for (int i = first; i < sim_hub_events_seen(); i++) {
        const char *event = sim_hub_event_at(i);
        if (event && strstr(event, value)) return true;
    }
    return false;
}
static void check_audiences(int first) {
    bool valid = true;
    for (int i = first; i < sim_hub_events_seen(); i++) {
        const char *event = sim_hub_event_at(i), *audience = sim_hub_event_audience(i);
        if (!event || !strstr(event, "SYNTHETIC_PUBLISH_")) continue;
        const char *member = strstr(event, "\"member\":\"owner\"") ? "owner" : "kid";
        valid = valid && audience && !strcmp(audience, member);
    }
    CHECK(valid, "publication window never sends a private event into the previous member connection");
}
static void paused_change(const char *member, bool open, bool unlock) {
    char old[16];
    snprintf(old, sizeof old, "%s", ink_member_current()->id);
    uint32_t epoch = ink_hub_member_epoch();
    test_member_pause();
    pin_done = 0;
    if (unlock) CHECK(ink_member_unlock("owner", "2468", pin_told, NULL) == 0, "real PIN unlock queued");
    else test_member_change(open ? member : NULL);
    CHECK(WAIT_FOR(test_member_paused(), 5000), "identity changed before state publication");
    ink_member_identity_t identity;
    ink_member_identity(&identity);
    CHECK(!strcmp(identity.member, member) && identity.authed == open && identity.epoch > epoch,
          "coherent member authentication and epoch already change before state publication");
    CHECK(!ink_hub_member_matches(old, epoch, ink_hub_connection()), "old delivery identity is invalid before state publication");
    stale_status = 100;
    CHECK(ink_hub_call_for("ai.ask", "{\"question\":\"SYNTHETIC_OLD_DELIVERY\"}", 5000000,
          stale_told, NULL, old, epoch) > 0 && WAIT_FOR(stale_status != 100, 5000) && stale_status == INK_HUB_ERR_LOST,
          "old private request is rejected before state publication");
    int first = sim_hub_events_seen();
    double asks = sim_metric_value("test.wire.asks");
    double answers = sim_metric_value("ai.answers");
    char value[64];
    snprintf(value, sizeof value, "SYNTHETIC_PUBLISH_%s_%u", member, identity.epoch);
    emit(member, value);
    if (open) {
        ready();
        CHECK(WAIT_FOR(event_received(first, value), 5000), "asynchronous current-member event reaches synchronized receiving context");
        check_audiences(first);
        on_ui(test_ai_identity_ask);
        CHECK(WAIT_FOR(sim_metric_value("test.wire.asks") > asks, 5000), "current private request reaches synchronized receiving context");
        char key[64];
        snprintf(key, sizeof key, "test.wire.asks.%s", member);
        CHECK(sim_metric_value(key) > 0 && sim_metric_value("test.wire.ask.epoch") == identity.epoch &&
              sim_metric_value("test.wire.ask.authed") == 1, "private request uses the full current wire identity");
    } else {
        CHECK(WAIT_FOR(sim_metric_value("test.wire.context.epoch") == identity.epoch &&
                       sim_metric_value("test.wire.context.authed") == 0, 5000), "closed authentication also synchronizes before publication");
        CHECK(!event_received(first, value), "closed space does not deliver private events");
    }
    test_member_release();
    CHECK(WAIT_FOR(unlock ? pin_done && pin_result == INK_PIN_OK : test_member_change_done(), 5000), "member worker finishes normally");
    test_member_pause_done();
    if (open) {
        CHECK(WAIT_FOR(sim_metric_value("ai.answers") > answers, 10000), "new identity answer survives delayed state notification");
        on_ui(test_ai_identity_current);
    }
}
static void run(void *arg) {
    (void)arg;
    ink_hub_demand(); ready();
    double answers = sim_metric_value("ai.answers");
    on_ui(test_ai_identity_switch_and_ask);
    CHECK(WAIT_FOR(sim_metric_value("ai.answers") > answers, 10000), "queued old owner callback cannot dismiss a new member's question");
    on_ui(test_ai_identity_current);
    pin_done = 0;
    CHECK(ink_member_set_pin("owner", "", "2468", pin_told, NULL) == 0 &&
          WAIT_FOR(pin_done, 5000) && pin_result == INK_PIN_OK, "synthetic owner PIN installed");
    CHECK(ink_member_switch("kid") == 0, "start in child space"); ready();
    paused_change("owner", true, true);

    test_member_pause();
    test_member_change("kid");
    CHECK(WAIT_FOR(test_member_paused(), 5000), "adult to child publication deliberately delayed");
    on_ui(test_ai_identity_posts);
    on_ui(test_ai_identity_empty);
    ready();
    answers = sim_metric_value("ai.answers");
    on_ui(test_ai_identity_new);
    test_member_release();
    CHECK(WAIT_FOR(test_member_change_done(), 5000), "delayed member state released");
    test_member_pause_done();
    CHECK(WAIT_FOR(sim_metric_value("ai.answers") > answers, 10000), "child answer survives the old member's delayed callback");
    on_ui(test_ai_identity_current);

    paused_change("owner", true, true);
    paused_change("owner", false, false);
    paused_change("owner", true, true);
    paused_change("kid", true, false);
    paused_change("owner", true, true);

    on_ui(test_ai_identity_switch);
    ready();
    on_ui(test_ai_identity_empty);
    pin_done = 0;
    CHECK(ink_member_unlock("owner", "2468", pin_told, NULL) == 0 &&
          WAIT_FOR(pin_done, 5000) && pin_result == INK_PIN_OK, "owner returns for the real voice handoff");
    ready();
    on_ui(test_ai_identity_failed_restart);
    on_ui(test_ai_identity_failed_clean);
    double bytes = sim_metric_value("hub.binary_bytes");
    on_ui(test_ai_review_voice);
    CHECK(WAIT_FOR(sim_metric_value("hub.binary_bytes") > bytes, 3000), "normal voice uploads real microphone packets");
    on_ui(test_ai_review_release);
    CHECK(WAIT_FOR(!strcmp(ink_member_current()->id, "kid") && ink_cap_available("voice.chat"), 15000),
          "real voice transcript and tools/call switch to the child");
    CHECK(sim_metric_value("action.ai.system.switch_member") == 1 && sim_metric_value("hub.member_turns_cancelled") > 0,
          "voice handoff uses the ordinary capability and cancels the old turn");
    on_ui(test_ai_identity_voice_closed);
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    CHECK(ink_cap_register(CAPS, 1) == 0, "synthetic event registered");
    osal_task_cfg_t cfg = {.name="identity-review", .fn=run, .stack_bytes=16384, .priority=OSAL_PRIO_NORMAL,
                           .core=-1, .flags=OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "identity regression task started");
}
