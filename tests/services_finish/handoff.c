#include "product.h"
static volatile int pin_done, pin_result;
static void pin_told(void *ctx, int result, uint32_t wait_s) {
    (void)ctx; (void)wait_s;
    pin_result = result;
    pin_done = 1;
}
static bool contains_since(int first, const char *text) {
    for (int i = first; i < sim_hub_events_seen(); i++) {
        const char *event = sim_hub_event_at(i);
        if (event && strstr(event, text)) return true;
    }
    return false;
}
static void run(void *arg) {
    (void)arg; delay_ms(1800); ink_hub_demand();
    CHECK(WAIT_FOR(ink_hub_online(), 30000), "online");
    int events = sim_hub_events_seen();
    CHECK(ink_hub_notify("device/event", "{\"member\":\"owner\",\"event\":\"media.finished\",\"args\":{\"path\":\"SYNTHETIC_OWNER_FILE\"}}") == 0, "old member event queued");
    CHECK(ink_member_switch("kid") == 0, "handed to child");
    delay_ms(1000); /* An already queued packet would have reached this local simulator by now. */
    CHECK(!contains_since(events, "SYNTHETIC_OWNER_FILE"), "queued transport event cannot cross handoff");
    ink_hub_demand();
    CHECK(WAIT_FOR(ink_hub_online(), 30000), "child connection");
    CHECK(ink_hub_notify("device/event", "{\"member\":\"owner\",\"event\":\"media.finished\",\"args\":{\"path\":\"SYNTHETIC_OWNER_FILE\"}}") != 0,
          "late producer cannot enqueue the previous member's event on the new connection");
    CHECK(ink_hub_notify("device/event", "{\"member\":\"kid\",\"event\":\"media.finished\",\"args\":{\"path\":\"SYNTHETIC_KID_FILE\"}}") == 0, "current event queued");
    CHECK(WAIT_FOR(contains_since(events, "SYNTHETIC_KID_FILE"), 2000), "current member event still reaches the hub");
    CHECK(ink_member_switch("owner") == 0, "back to owner");
    ink_hub_demand();
    CHECK(WAIT_FOR(ink_hub_online(), 30000), "owner connection");
    CHECK(ink_member_set_pin("owner", "", "2468", pin_told, NULL) == 0 && WAIT_FOR(pin_done, 5000) && pin_result == INK_PIN_OK,
          "synthetic PIN protects the owner space");
    ink_member_test_reset(); /* Close authentication explicitly; D-95 now grants a lock grace. */
    ink_power_lock();
    CHECK(WAIT_FOR(!ink_member_space_open(), 2000), "owner space closed");
    CHECK(ink_hub_notify("device/event", "{\"member\":\"owner\",\"event\":\"media.finished\",\"args\":{\"path\":\"SYNTHETIC_CLOSED_FILE\"}}") != 0,
          "a late producer cannot enqueue private events after the space closes");
    delay_ms(1000);
    CHECK(!contains_since(events, "SYNTHETIC_CLOSED_FILE"), "closed-space private event never leaves the device");
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name="finish-handoff", .fn=run, .stack_bytes=16384, .priority=OSAL_PRIO_NORMAL, .core=-1, .flags=OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "test task");
}
