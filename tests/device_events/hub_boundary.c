#include "ink/hal_net.h"
#include "ink/osal.h"
#include "ink/probe.h"
#include "ink/sys/members.h"
#include <stdio.h>
#include <string.h>
static bool fail_member_send;
static int identity_fault;
static void fixture_identity(ink_member_identity_t *out) {
    ink_member_identity(out);
    if (identity_fault == 1) snprintf(out->member, sizeof out->member, "%s", !strcmp(out->member, "owner") ? "kid" : "owner");
}
static bool handoff_on_alloc, handoff_succeeded;
static void *fixture_hub_malloc(size_t size, uint32_t caps) {
    void *p = osal_malloc(size, caps);
    if (handoff_on_alloc) {
        handoff_on_alloc = false;
        handoff_succeeded = ink_member_switch("kid") == 0 && ink_member_switch("owner") == 0;
    }
    return p;
}
static int fixture_ws_send(hal_ws_t *ws, bool binary, const void *data, size_t len, int64_t timeout_us) {
    static const char prefix[] = "{\"jsonrpc\":\"2.0\",\"method\":\"ink/member\"";
    if (fail_member_send && !binary && len >= sizeof prefix - 1 && !memcmp(data, prefix, sizeof prefix - 1)) {
        fail_member_send = false;
        ink_probe_count("test.member_sync_refused");
        return -1;
    }
    return hal_ws_send(ws, binary, data, len, timeout_us);
}
#define hal_ws_send fixture_ws_send
#define osal_malloc fixture_hub_malloc
#define ink_member_identity fixture_identity
#ifndef INK_TEST_HUB_SOURCE
#define INK_TEST_HUB_SOURCE "../../system/services/hub/hub_link.c"
#endif
#include INK_TEST_HUB_SOURCE
#undef hal_ws_send
#undef osal_malloc
#undef ink_member_identity
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

bool test_hub_hello(void) { return get_state() == HS_HELLO; }
void test_hub_fail_member_send(void) { fail_member_send = true; }

static int send_status;
static void send_told(void *ctx, int status, const ink_json_t *j, int result) {
    (void)ctx; (void)j; (void)result;
    send_status = status;
}
void test_hub_send_boundary(void) {
    pending_t p = {.id=new_id(), .used=true, .deadline=osal_now_us() + 5000000,
                   .member_epoch=ink_hub_member_epoch(), .cb=send_told};
    snprintf(p.member, sizeof p.member, "%s", ink_member_current()->id);
    snprintf(p.method, sizeof p.method, "ai.ask");
    p.params = dup_mem("{\"question\":\"SYNTHETIC_ALLOC_HANDOFF\"}", strlen("{\"question\":\"SYNTHETIC_ALLOC_HANDOFF\"}"));
    CHECK(p.params != NULL, "private send fixture allocated");
    if (!p.params) return;
    send_status = 100;
    handoff_on_alloc = true;
    int result = send_request(&p);
    CHECK(handoff_succeeded && result < 0 && send_status == INK_HUB_ERR_LOST,
          "handoff during wire allocation cannot send a stale private request");
    finish(&p, INK_HUB_ERR_LOST, NULL, -1);
}

void test_hub_identity_tuple(void) {
    ink_member_identity_t identity;
    identity_fault = 1;
    fixture_identity(&identity);
    CHECK(!ink_hub_member_matches(identity.member, identity.epoch, H.ws_gen), "matching epoch alone cannot authorize a different wire member");
    identity_fault = 0;
    fixture_identity(&identity);
    CHECK(ink_hub_member_matches(identity.member, identity.epoch, H.ws_gen), "complete matching wire identity permits normal delivery");
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
