#include "product.h"
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
    CHECK(ink_hub_notify("device/event", "{\"member\":\"kid\",\"event\":\"media.finished\",\"args\":{\"path\":\"SYNTHETIC_KID_FILE\"}}") == 0, "current event queued");
    CHECK(WAIT_FOR(contains_since(events, "SYNTHETIC_KID_FILE"), 2000), "current member event still reaches the hub");
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name="finish-handoff", .fn=run, .stack_bytes=16384, .priority=OSAL_PRIO_NORMAL, .core=-1, .flags=OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "test task");
}
