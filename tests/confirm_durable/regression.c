#include "../services_finish/product.h"
#include "ink/hal_power.h"

extern void review_request(const ink_json_t *j);
extern void review_tools(const ink_json_t *j);
extern bool review_running_durable(uint32_t token);
extern unsigned test_result_count(void), test_last_token(void);
extern bool review_sent(uint32_t token);
extern void test_event_ack(bool valid);
extern void review_forget_delivery(uint32_t token);
static const ink_db_def_t EFFECT = {.name="test.confirm_effect", .scope=INK_DB_DEVICE, .schema=1};
static ink_db_t *effects;
static uint32_t active_token;
static unsigned runs;
static char returned_digest[65];
static char returned_status[24];
static void event(void *ctx, const char *id, const ink_args_t *a) {
    (void)ctx; (void)id;
    ink_args_get_str(a, "digest", returned_digest, sizeof returned_digest);
    ink_args_get_str(a, "status", returned_status, sizeof returned_status);
}
static void action(ink_call_t *c) {
    CHECK(review_running_durable(active_token), "running marker precedes the effect");
    runs++;
    uint32_t ver = 0;
    ink_db_write_t put = {.id=1, .data=&runs, .len=sizeof runs, .written_ver=&ver};
    CHECK(ink_db_put(effects, &put, NULL) == 0 && ink_db_wait(effects, ver, 5000000) == 0, "synthetic effect durable");
    snprintf(c->rc.say, sizeof c->rc.say, "合成动作完成");
}
static const ink_cap_t CAPS[] = {
    {.id="test.confirm", .title="合成确认", .ask="合成内容", .ask_by_hand=true, .effect=INK_FX_LOCAL_WRITE, .child_ok=true, .exec=INK_EXEC_BG, .timeout_ms=12000, .run=action},
    {.id="test.page", .title="合成页面确认", .ask="合成内容", .activity="home", .ask_by_hand=true, .effect=INK_FX_LOCAL_WRITE, .child_ok=true, .exec=INK_EXEC_BG, .timeout_ms=12000, .run=action},
    {.id="test.auto", .title="合成自动动作", .effect=INK_FX_LOCAL_WRITE, .child_ok=true, .exec=INK_EXEC_BG, .timeout_ms=12000, .run=action},
};
static void durable(void) {
    ink_db_flush_all();
    CHECK(WAIT_FOR(ink_db_idle() && !ink_storage_pending(), 10000), "all result writes durable");
}
static uint32_t hold(const char *cap) {
    CHECK(BY_HAND(cap, NULL) && g_rc.status == INK_RC_PENDING_CONFIRMATION, "confirmation held (%d)", g_rc.status);
    unsigned token = 0;
    CHECK(sscanf(g_rc.object, "confirm:%u", &token) == 1, "token returned");
    active_token = token;
    durable();
    return token;
}
static bool state(uint32_t token, const char *expected) {
    char args[64], match[64];
    snprintf(args, sizeof args, "{\"token\":%u}", token);
    snprintf(match, sizeof match, "\"state\":\"%s\"", expected);
    bool ok = AI("confirm.status", args) && g_rc.status == INK_RC_DONE && strstr(g_result, match);
    CHECK(ok, "token %u expected %s, got %s", token, expected, g_result);
    return ok;
}
static void changed_page(void) { ink_ui_content_changed(); }
static void gateway(const char *json, bool tool) {
    ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL);
    CHECK(j && ink_json_parse(j, json, strlen(json)) == 0, "gateway fixture parses");
    if (j) { if (tool) review_tools(j); else review_request(j); osal_free(j); }
}
static void automatic(void) {
    active_token = 0;
    ink_hub_demand();
    CHECK(WAIT_FOR(ink_hub_online() && ink_hub_member_matches("owner", ink_hub_member_epoch(), ink_hub_connection()), 30000), "automatic gateway connected");
    char json[400];
    snprintf(json, sizeof json, "{\"name\":\"test.auto\",\"arguments\":{},\"_meta\":{\"member\":\"owner\",\"epoch\":%u,\"request_id\":\"SYNTHETIC-AUTO\",\"asked\":true}}", ink_hub_member_epoch());
    gateway(json, true);
}
static void test_send_tool(const char *name, const char *args, const char *rid) {
    char json[1024];
    snprintf(json, sizeof json, "{\"name\":\"%s\",\"arguments\":%s,\"_meta\":{\"member\":\"owner\",\"epoch\":%u,\"request_id\":\"%s\",\"asked\":true}}", name, args ? args : "{}", ink_hub_member_epoch(), rid);
    gateway(json, true);
}
static bool query_done(uint32_t token) {
    char args[64]; snprintf(args, sizeof args, "{\"token\":%u}", token);
    return AI("confirm.status", args) && g_rc.status == INK_RC_DONE && strstr(g_result, "\"state\":\"done\"");
}
static void accept_and_switch(void *arg) { /* Same executor turn: acceptance is queued before the switch. */
    uint32_t token = (uint32_t)(uintptr_t)arg;
    CHECK(ink_confirm_decide(token, true, INK_SRC_TOUCH) == 0, "person accepts before member switch");
    CHECK(ink_member_switch("kid") == 0, "member switches before accepted dispatch");
}
static void result_cases(const char *phase) {
    ink_hub_demand();
    CHECK(WAIT_FOR(ink_hub_online() && ink_hub_member_matches("owner", ink_hub_member_epoch(), ink_hub_connection()), 30000), "result gateway connected");
    if (!strcmp(phase, "receipts")) {
        CHECK(AI("notify.post", "{\"title\":\"SYNTHETIC-UNDO\"}") && g_rc.status == INK_RC_DONE, "real undoable notification");
        char args[120]; snprintf(args, sizeof args, "{\"object\":\"%s\"}", g_rc.object);
        unsigned token = test_last_token() + 1;
        test_send_tool("system.undo", args, "SYNTHETIC-UNDO");
        CHECK(WAIT_FOR(query_done(token), 10000), "successful undo is durable done (%s)", g_result);
        CHECK(strstr(g_result, "\"status\":\"undone\""), "original undo receipt retained");
        CHECK(AI("notify.list", NULL) && !strstr(g_result, "SYNTHETIC-UNDO"), "notification actually removed");
        char json[600];
        snprintf(json, sizeof json, "{\"title\":\"合成中枢确认\",\"body\":\"example.com 合成表单\",\"member\":\"owner\",\"parent\":false,\"expires_ms\":10000,\"digest\":\"%064d\",\"_meta\":{\"member\":\"owner\",\"epoch\":%u}}", 0, ink_hub_member_epoch());
        gateway(json, false); ink_confirm_view_t card;
        CHECK(ink_confirm_list(&card, 1) == 1, "short hub card exists");
        CHECK(ink_confirm_decide(card.token, true, INK_SRC_TOUCH) == 0, "person accepts short hub card");
        durable(); state(card.token, "accepted");
        delay_ms(11000); state(card.token, "expired");
        return;
    }
    if (!strcmp(phase, "capacity")) {
        uint32_t unacked = hold("test.confirm"), first = test_last_token() + 1, last = 0;
        CHECK(ink_confirm_decide(unacked, false, INK_SRC_TOUCH) == 0, "unacknowledged confirmation result");
        durable();
        int completed = 0;
        for (int i = 0; i < 1200; i++) {
            char rid[40]; snprintf(rid, sizeof rid, "SYNTHETIC-CAPACITY-%04d", i);
            last = test_last_token() + 1;
            test_send_tool("clipboard.write", "{\"text\":\"SYNTHETIC\"}", rid);
            bool done = WAIT_FOR(query_done(last), 10000);
            CHECK(done, "automatic write %d durable (%s)", i + 1, g_result);
            if (!done) break;
            CHECK(WAIT_FOR(review_sent(last), 10000), "automatic result %d sent", i + 1);
            if (!i) { test_event_ack(false); state(unacked, "rejected"); review_forget_delivery(last); }
            test_event_ack(true); completed++;
        }
        CHECK(completed == 1200, "acknowledged results never exhaust the store (%d)", completed);
        CHECK(test_result_count() <= 19, "only metadata, unacked and recent results retained (%u)", test_result_count());
        state(first, "done"); /* A skipped event is not acknowledged by a later cumulative ACK. */
        state(first + 1, "unknown");
        /* The first ACK also covered the confirmation: test a genuinely missing ACK separately below. */
        for (uint32_t t = last - 15; t <= last; t++) state(t, "done");
        uint32_t pending = hold("test.confirm");
        CHECK(ink_confirm_decide(pending, false, INK_SRC_TOUCH) == 0, "confirmation still works after capacity run");
        durable();
        for (int i = 0; i < 20; i++) {
            uint32_t t = hold("test.confirm");
            CHECK(ink_confirm_decide(t, false, INK_SRC_TOUCH) == 0, "unacknowledged result %d", i);
        }
        durable();
        for (int i = 0; i < 20; i++) {
            char rid[40]; snprintf(rid, sizeof rid, "SYNTHETIC-AFTER-UNACKED-%02d", i);
            uint32_t t = test_last_token() + 1;
            test_send_tool("clipboard.write", "{\"text\":\"SYNTHETIC\"}", rid);
            CHECK(WAIT_FOR(query_done(t), 10000) && WAIT_FOR(review_sent(t), 10000), "later automatic result %d", i);
            test_event_ack(true);
        }
        durable(); state(pending, "rejected");
        CHECK(test_result_count() >= 30, "unacknowledged old results survive later ACKs (%u)", test_result_count());
    } else {
        CHECK(test_result_count() >= 30 && test_result_count() < 50, "retained results survive restart (%u)", test_result_count());
        uint32_t token = test_last_token() + 1;
        test_send_tool("clipboard.write", "{\"text\":\"SYNTHETIC-RESTART\"}", "SYNTHETIC-RESTART");
        CHECK(WAIT_FOR(query_done(token), 10000), "new automatic write works after capacity restart (%s)", g_result);
        durable();
    }
}
static void run(void *arg) {
    (void)arg; delay_ms(1800);
    CHECK(ink_db_register(&EFFECT) == 0 && (effects=ink_db_open(&EFFECT, NULL)), "effect collection");
    ink_rec_t rec;
    if (ink_db_get(effects, 1, &rec, &runs, sizeof runs) != 0) runs = 0;
    CHECK(ink_cap_register(CAPS, 3) == 0 && ink_cap_find("confirm.status"), "capabilities registered");
    CHECK(ink_cap_subscribe("confirm.result", event, NULL, INK_EXEC_DIRECT) == 0, "confirmation event observer");
    const char *phase = getenv("INK_CONFIRM_PHASE");
    bool auto_phase = strstr(phase, "auto") != NULL;
    if (!strcmp(phase, "failure")) {
        uint32_t token = hold("test.confirm");
        CHECK(ink_confirm_decide(token, true, INK_SRC_TOUCH) == 0, "accept with failed commit");
        CHECK(WAIT_FOR(!strcmp(returned_status, "failed"), 10000), "failed commit emits failure receipt");
        durable(); state(token, "failed");
        CHECK(runs == 0 && strstr(g_result, "could not be committed"), "failed commit never executes and preserves reason");
    } else if (!strcmp(phase, "receipts") || !strcmp(phase, "capacity") || !strcmp(phase, "restart-capacity")) {
        result_cases(phase);
    } else if (!strcmp(phase, "cut-auto")) {
        automatic(); delay_ms(15000); CHECK(false, "automatic cut hook did not fire");
    } else if (!strcmp(phase, "cut")) {
        uint32_t token = hold("test.confirm");
        CHECK(token == 2 && state(token, "pending"), "first durable token");
        CHECK(ink_confirm_decide(token, true, INK_SRC_TOUCH) == 0, "person accepts before cut");
        delay_ms(15000);
        CHECK(false, "cut hook did not fire");
    } else if (!strncmp(phase, "restart", 7)) {
        const char *cut = getenv("INK_CONFIRM_EXPECT");
        unsigned expected = strstr(cut, "done") ? 1 : 0;
        CHECK(runs == expected, "effect count after cut: %u expected %u", runs, expected);
        state(2, !strcmp(cut, "before-running") && !auto_phase ? "expired" : !strcmp(cut, "after-done") ? "done" : "unknown");
        CHECK(ink_confirm_decide(2, true, INK_SRC_TOUCH) == -1, "old token cannot execute again");
        if (auto_phase && strcmp(cut, "before-running")) {
            automatic(); delay_ms(500);
            CHECK(runs == expected, "persistent request id refuses automatic restart replay");
            CHECK(AI("confirm.status", "{\"request_id\":\"SYNTHETIC-AUTO\"}") && strstr(g_result, !strcmp(cut, "after-done") ? "\"state\":\"done\"" : "\"state\":\"unknown\""), "automatic restart result found (%s)", g_result);
        }
        uint32_t next = hold("test.confirm");
        CHECK(next > 2, "restart never reuses token");
        CHECK(ink_confirm_withdraw(next, INK_SRC_TOUCH) == 0, "new token control");
        durable(); CHECK(runs == expected, "no restart replay");
    } else {
        uint32_t token = hold("test.confirm"); state(token, "pending");
        CHECK(ink_confirm_decide(token, true, INK_SRC_AI) == -1, "AI cannot accept");
        CHECK(ink_confirm_decide(token, false, INK_SRC_TOUCH) == 0, "person rejects");
        durable(); state(token, "rejected");
        token = hold("test.confirm");
        CHECK(ink_confirm_decide(token, true, INK_SRC_TOUCH) == 0, "person accepts");
        CHECK(WAIT_FOR(runs == 1, 10000), "action ran once");
        durable(); state(token, "done"); CHECK(strstr(g_result, "合成动作完成"), "receipt retained");
        CHECK(ink_confirm_decide(token, true, INK_SRC_TOUCH) == -1 && runs == 1, "duplicate decision refused");
        CHECK(ink_member_switch("kid") == 0, "member changes"); state(token, "unknown");
        CHECK(ink_member_switch("owner") == 0, "owner returns");
        token = hold("test.confirm");
        CHECK(ink_member_switch("kid") == 0, "change while pending");
        CHECK(ink_confirm_decide(token, true, INK_SRC_TOUCH) == -1, "old member confirmation refused");
        CHECK(ink_member_switch("owner") == 0, "return after invalidation");
        durable(); state(token, "member_changed");
        token = hold("test.confirm");
        ink_exec_job_t switch_job = {0};
        CHECK(ink_exec_post(INK_EXEC_BG, &switch_job, accept_and_switch, (void *)(uintptr_t)token) == 0, "queue accepted-member execution check");
        CHECK(WAIT_FOR(!strcmp(ink_member_current()->id, "kid"), 10000), "accepted member changed");
        delay_ms(500); durable();
        CHECK(runs == 1, "old accepted member action has no side effect (%u)", runs);
        ink_probe_value("test.stale_member_effects", runs - 1);
        CHECK(ink_member_switch("owner") == 0, "owner returns after accepted invalidation");
        state(token, "member_changed");
        on_ui(ink_ui_home);
        token = hold("test.page"); on_ui(changed_page);
        CHECK(ink_confirm_decide(token, true, INK_SRC_TOUCH) == 0, "stale confirmation checked by gate");
        delay_ms(500); durable(); state(token, "failed"); CHECK(runs == 1 && strstr(g_result, "stale"), "changed content not executed");
        token = hold("test.confirm"); delay_ms(61000); durable(); state(token, "expired");
        ink_hub_demand();
        CHECK(WAIT_FOR(ink_hub_online() && ink_hub_member_matches("owner", ink_hub_member_epoch(), ink_hub_connection()), 30000), "hub connected");
        char json[600];
        snprintf(json, sizeof json, "{\"title\":\"合成中枢确认\",\"body\":\"example.com 合成表单\",\"member\":\"owner\",\"parent\":false,\"expires_ms\":60000,\"digest\":\"%064d\",\"_meta\":{\"member\":\"owner\",\"epoch\":%u}}", 0, ink_hub_member_epoch());
        gateway(json, false); ink_confirm_view_t card;
        CHECK(ink_confirm_list(&card, 1) == 1 && !strcmp(card.title, "合成中枢确认"), "dedicated request creates card");
        CHECK(ink_confirm_decide(card.token, true, INK_SRC_TOUCH) == 0, "hub card accepted by person");
        delay_ms(500); durable(); state(card.token, "accepted"); CHECK(runs == 1, "hub capability never executes on device");
        CHECK(strlen(returned_digest) == 64 && strspn(returned_digest, "0") == 64, "hub result echoes the bound digest");
        snprintf(json, sizeof json, "{\"title\":\"合成中枢确认\",\"body\":\"example.com 合成表单\",\"member\":\"kid\",\"parent\":false,\"expires_ms\":60000,\"digest\":\"%064d\",\"_meta\":{\"member\":\"kid\",\"epoch\":%u}}", 0, ink_hub_member_epoch());
        gateway(json, false); CHECK(ink_confirm_list(&card, 1) == 0, "stale target rejected by dedicated request");
        snprintf(json, sizeof json, "{\"title\":\"合成中枢确认\",\"body\":\"example.com 合成表单\",\"member\":\"owner\",\"parent\":false,\"expires_ms\":60000,\"digest\":\"%064d\",\"_meta\":{\"member\":\"owner\",\"epoch\":%u}}", 1, ink_hub_member_epoch());
        uint32_t accepted = card.token;
        gateway(json, false);
        CHECK(ink_confirm_list(&card, 1) == 1, "second hub card queued");
        CHECK(ink_confirm_decide(card.token, false, INK_SRC_TOUCH) == 0, "hub rejection");
        durable(); state(card.token, "rejected");
        CHECK(strlen(returned_digest) == 64 && returned_digest[63] == '1', "rejection also echoes its digest");
        CHECK(!ink_cap_find("confirm.request") && !ink_cap_available("confirm/request"), "request is outside model tools");
        for (int i = 0; i < 18; i++) { uint32_t kept = hold("test.confirm"); CHECK(ink_confirm_decide(kept, false, INK_SRC_TOUCH) == 0, "retain result %d", i); }
        durable(); state(accepted, "unknown"); /* ACKed and older than the most recent sixteen. */
        automatic();
        CHECK(WAIT_FOR(runs == 2, 10000), "automatic action executes once"); durable();
        CHECK(WAIT_FOR(AI("confirm.status", "{\"request_id\":\"SYNTHETIC-AUTO\"}") && strstr(g_result, "\"state\":\"done\""), 10000), "automatic result query after receipt commit (%s)", g_result);
        automatic(); delay_ms(500); CHECK(runs == 2, "automatic duplicate not executed");
    }
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name="confirm-regression", .fn=run, .stack_bytes=16384, .priority=OSAL_PRIO_NORMAL, .core=-1, .flags=OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "test task");
}
