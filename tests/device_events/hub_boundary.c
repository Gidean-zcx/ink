#ifndef INK_TEST_HUB_SOURCE
#define INK_TEST_HUB_SOURCE "../../system/services/hub/hub_link.c"
#endif
#include INK_TEST_HUB_SOURCE
#include "../integration/whole_product.h"

void test_hub_budget(void) {
    pending_t p = {.id = new_id(), .used = true, .deadline = osal_now_us() + 7123000,
        .params = "{\"budget_ms\":45000,\"question\":\"SYNTHETIC_BUDGET\",\"page\":{\"actions\":[]}}"};
    snprintf(p.method, sizeof p.method, "ai.ask");
    CHECK(send_request(&p) == 0, "send delayed request with remaining budget");
    call_req_t *r = osal_calloc(1, sizeof *r, OSAL_MEM_EXTERNAL);
    r->id = new_id();
    r->deadline = osal_now_us() + 1000000;
    snprintf(r->method, sizeof r->method, "fixture.expired");
    uint64_t deadline = r->deadline;
    uint32_t id = r->id;
    accept_call(r, deadline + 1);
    bool expired = true;
    for (int i = 0; i < MAX_PENDING; i++) if (H.pend[i].used && H.pend[i].id == id) expired = false;
    CHECK(expired, "queue delay cannot restart an expired request deadline");
}

void test_hub_reconnect(void) {
    fail_all(INK_HUB_ERR_LOST, true);
    close_ws(); drain_tx(false); set_state(HS_DISCONNECTED);
    ink_hub_demand();
}

static void (*fixture_hub_fn)(void), (*fixture_tick)(void);
static osal_sem_t *fixture_hub_done;
static void fixture_run(void) {
    H.tick = fixture_tick;
    fixture_hub_fn();
    osal_sem_give(fixture_hub_done);
}
void test_on_hub(void (*fn)(void)) {
    fixture_hub_done = osal_sem_create(0, 1);
    fixture_hub_fn = fn; fixture_tick = H.tick; H.tick = fixture_run;
    ink_hub_wake();
    CHECK(osal_sem_take(fixture_hub_done, 5000000) == OSAL_OK, "hub fixture finished");
    osal_sem_delete(fixture_hub_done);
}
