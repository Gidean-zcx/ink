#include "../services_finish/product.h"

void test_on_hub(void (*fn)(void)), test_hub_reconnect(void), test_hub_fail_member_send(void);
void test_hub_send_boundary(void);
bool test_hub_hello(void);
void test_ai_review_voice(void), test_ai_review_push_refusal(void), test_ai_review_open_refusal(void);
void test_ai_review_release(void), test_ai_review_failure(void), test_ai_review_speaker_failure(void);
void test_ai_review_new_turn(void), test_ai_review_clean(void);
void test_ai_review_aborted(void), test_ai_review_early_failure(void), test_ai_review_first_text(void);

static volatile int pin_done, pin_result;
static void pin_told(void *ctx, int result, uint32_t wait_s) {
    (void)ctx; (void)wait_s;
    pin_result = result;
    pin_done = 1;
}
static void ask(void) { ink_ai_ask_text("SYNTHETIC_CURRENT_MEMBER_QUESTION"); }
static void dismiss(void) { ink_ai_dismiss(); }
static void wait_hello(void) {
    for (int i = 0; i < 30000 && !test_hub_hello(); i++) osal_task_delay_us(1000);
    CHECK(test_hub_hello(), "real handshake waiting for hello response");
}
static void ready(void) {
    CHECK(WAIT_FOR(ink_hub_online() && ink_cap_available("voice.chat"), 30000), "current member directory ready");
}
static void private_question(const char *member) {
    char key[64];
    snprintf(key, sizeof key, "test.wire.asks.%s", member);
    double received = sim_metric_value(key), asks = sim_metric_value("test.wire.asks"), answers = sim_metric_value("ai.answers");
    uint32_t epoch = ink_hub_member_epoch();
    on_ui(ask);
    CHECK(WAIT_FOR(sim_metric_value("test.wire.asks") > asks, 30000), "real private question delivered");
    CHECK(sim_metric_value(key) == received + 1 && sim_metric_value("test.wire.ask.epoch") == epoch &&
          sim_metric_value("test.wire.ask.authed") == 1, "handshake handoff reaches the current member and epoch");
    CHECK(WAIT_FOR(sim_metric_value("ai.answers") > answers, 10000), "real private answer completes");
    on_ui(dismiss);
    ready();
}
static void unlock_owner(void) {
    pin_done = 0;
    CHECK(ink_member_unlock("owner", "2468", pin_told, NULL) == 0 && WAIT_FOR(pin_done, 5000) && pin_result == INK_PIN_OK,
          "synthetic PIN unlocks owner locally");
}
static int closed_status = 100;
static void closed_told(void *ctx, int status, const ink_json_t *j, int result) {
    (void)ctx; (void)j; (void)result;
    closed_status = status;
}
static void voice_turn(void (*release)(void), const char *fault) {
    double answers = sim_metric_value("ai.answers");
    double bytes = sim_metric_value("hub.binary_bytes");
    on_ui(test_ai_review_voice);
    CHECK(WAIT_FOR(sim_metric_value("hub.binary_bytes") > bytes, 2000), "real microphone data reaches the hub");
    on_ui(release);
    if (fault) {
        CHECK(WAIT_FOR(sim_metric_value(fault) > 0, 10000), "real downlink fault injected once");
        on_ui(test_ai_review_aborted);
    }
    CHECK(WAIT_FOR(sim_metric_value("ai.answers") > answers, 25000), "real voice answer completes");
}
static void run(void *arg) {
    (void)arg;
    ink_hub_demand();
    wait_hello();
    CHECK(ink_member_switch("kid") == 0, "owner to child during initial hello");
    private_question("kid");
    test_on_hub(test_hub_reconnect);
    wait_hello();
    CHECK(ink_member_switch("owner") == 0, "child to owner during reconnect hello");
    private_question("owner");
    test_on_hub(test_hub_reconnect);
    wait_hello();
    CHECK(ink_member_switch("kid") == 0 && ink_member_switch("owner") == 0, "A to B to A during hello");
    private_question("owner");
    CHECK(ink_member_switch("kid") == 0, "ordinary online handoff");
    private_question("kid");
    test_on_hub(test_hub_reconnect);
    wait_hello();
    test_hub_fail_member_send();
    CHECK(ink_member_switch("owner") == 0, "handoff with refused member synchronization");
    private_question("owner");
    CHECK(sim_metric_value("test.member_sync_refused") == 1 && sim_metric_value("hub.backoffs") > 0,
          "failed identity synchronization reconnects before private delivery");
    test_on_hub(test_hub_send_boundary);
    ready();

    pin_done = 0;
    CHECK(ink_member_set_pin("owner", "", "2468", pin_told, NULL) == 0 && WAIT_FOR(pin_done, 5000) && pin_result == INK_PIN_OK,
          "synthetic owner PIN installed");
    ink_member_test_reset();
    CHECK(!ink_member_space_open(), "owner authentication closed");
    test_on_hub(test_hub_reconnect);
    wait_hello();
    unlock_owner();
    CHECK(test_hub_hello(), "authentication changed inside hello window");
    private_question("owner");
    test_on_hub(test_hub_reconnect);
    wait_hello();
    ink_member_test_reset();
    double asks = sim_metric_value("test.wire.asks");
    CHECK(ink_hub_call_for("ai.ask", "{\"question\":\"SYNTHETIC_CLOSED_SPACE\"}", 5000000, closed_told, NULL,
                          "owner", ink_hub_member_epoch()) > 0, "closed-space request queued during hello");
    CHECK(WAIT_FOR(closed_status != 100 && ink_hub_online(), 30000) && closed_status == INK_HUB_ERR_LOST &&
          sim_metric_value("test.wire.asks") == asks, "closed authentication prevents private delivery");
    CHECK(WAIT_FOR(sim_metric_value("test.wire.context.epoch") == ink_hub_member_epoch() &&
                   sim_metric_value("test.wire.context.authed") == 0, 2000), "hello synchronization preserves closed authentication");
    unlock_owner();
    private_question("owner");

    voice_turn(test_ai_review_push_refusal, "test.audio.refused");
    on_ui(test_ai_review_failure);
    voice_turn(test_ai_review_open_refusal, "test.audio.open_refused");
    on_ui(test_ai_review_failure);
    voice_turn(test_ai_review_release, NULL);
    on_ui(test_ai_review_clean);
    on_ui(test_ai_review_speaker_failure);
    on_ui(test_ai_review_failure);
    double answers = sim_metric_value("ai.answers");
    on_ui(test_ai_review_new_turn);
    CHECK(WAIT_FOR(sim_metric_value("ai.answers") > answers, 10000), "new typed turn completes after old speech failure");
    on_ui(test_ai_review_clean);
    answers = sim_metric_value("ai.answers");
    double texts = sim_metric_value("ai.release_to_first_text_visible_ms");
    on_ui(test_ai_review_voice);
    on_ui(test_ai_review_early_failure); /* Exercise a failure notification arriving before any answer text. */
    on_ui(test_ai_review_release);
    CHECK(WAIT_FOR(sim_metric_value("ai.release_to_first_text_visible_ms") > texts, 10000), "real first text follows early speech failure");
    on_ui(test_ai_review_first_text);
    CHECK(WAIT_FOR(sim_metric_value("ai.answers") > answers, 25000), "early-failure voice answer completes");
    on_ui(test_ai_review_failure);
    on_ui(dismiss);
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name="device-review", .fn=run, .stack_bytes=16384, .priority=OSAL_PRIO_NORMAL,
                           .core=-1, .flags=OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "review regression task started");
}
