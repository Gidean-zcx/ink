/* Refuse both queued and completed writes without changing production storage. */
#include "../services_finish/product.h"
#include "ink/hal_power.h"
#include "ink/sys/pairing.h"
static int refuse;
static int borrow_fault(const char *, const void *, size_t, ink_storage_done_fn, void *);
#define ink_storage_write_borrow borrow_fault
#include "../../system/services/hub/pairing.c"
#undef ink_storage_write_borrow
static int borrow_fault(const char *path, const void *data, size_t len, ink_storage_done_fn done, void *ctx) {
    if (refuse && !strcmp(path, IDENTITY_FILE) && len == sizeof(pair_identity_t)) {
        const pair_identity_t *s = data;
        bool target = s->state == INK_PAIR_COMMITTING && ((refuse <= 2) == !s->token[0]);
        if (target) {
            if (refuse == 1 || refuse == 3) return -1;
            done(ctx, -1); return 0;
        }
    }
    return ink_storage_write_borrow(path, data, len, done, ctx);
}
static void offer_event(void *arg) {
    (void)arg;
    sim_hub_request_device("pair/offer", "{\"txid\":\"test-tx-1\",\"family\":\"测试家庭\",\"by\":\"测试大人\",\"name\":\"测试设备\"}");
}
static void show_offer(void) {
    CHECK(WAIT_FOR(ink_pairing_deadline() > osal_now_us(), 10000), "current claim is ready after restart");
    vtk_event_at(vtk_now() + SIM_MS(1), offer_event, NULL);
    CHECK(WAIT_FOR(sim_hub_reply_matches_request(), 3000), "offer answered");
}
static void run_fault(void *arg) {
    (void)arg;
    int mode = atoi(getenv("INK_PAIR_STORAGE_MODE"));
    bool recover = getenv("INK_PAIR_STORAGE_RECOVER") != NULL;
    if (!recover) {
        const char *config = "{\"url\":\"wss://example.com/v1/device\",\"pin\":\"1111111111111111111111111111111111111111111111111111111111111111\"}";
        CHECK(BY_HAND("pairing.configure", config) && g_rc.status == INK_RC_DONE, "configuration saved");
        CHECK(WAIT_FOR(ink_pairing_state() == INK_PAIR_PAIRING, 10000), "pairing begins");
        show_offer();
        refuse = mode;
        CHECK(BY_HAND("pairing.accept", "{\"txid\":\"test-tx-1\"}"), "person answers");
        if (mode <= 2) {
            CHECK(g_rc.status == INK_RC_FAILED && ink_pairing_state() == INK_PAIR_PAIRING && sim_metric_value("hub.requests.pair/accept") == 0,
                  "failed confirmation journal cannot send accept");
        } else {
            CHECK(g_rc.status == INK_RC_DONE && WAIT_FOR(sim_metric_value("pairing.save_failed") > 0, 5000), "token storage failure observed");
            CHECK(ink_pairing_state() == INK_PAIR_COMMITTING && !ink_hub_online() && sim_metric_value("hub.pair_ack_received") == 0 && sim_metric_value("hub.requests.hello") == 0,
                  "failed token write cannot acknowledge or authenticate");
        }
        ink_probe_count("test.checkpoint"); hal_power_reboot(); return;
    }
    if (mode <= 2) {
        CHECK(WAIT_FOR(ink_pairing_state() == INK_PAIR_PAIRING, 10000) && sim_metric_value("hub.requests.pair/accept") == 0,
              "restart does not invent a failed confirmation");
        show_offer();
        CHECK(!sim_hub_pair_txid()[0], "failed confirmation is not sent as a committed transaction");
        CHECK(BY_HAND("pairing.accept", "{\"txid\":\"test-tx-1\"}") && g_rc.status == INK_RC_DONE, "person can retry confirmation");
    }
    CHECK(WAIT_FOR(ink_pairing_state() == INK_PAIR_BOUND && ink_hub_online(), 15000), "restart recovers without a false saved receipt");
    if (mode > 2) CHECK(!strcmp(sim_hub_pair_txid(), "test-tx-1"), "failed token write resumes committed transaction");
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name="pair-store-test", .fn=run_fault, .stack_bytes=16384, .priority=OSAL_PRIO_NORMAL, .core=-1, .flags=OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "test task");
}
