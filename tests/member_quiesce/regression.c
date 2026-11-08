/* Real providers at the three review boundaries; all contents are synthetic. */
#include "../services_finish/product.h"

volatile bool ink_test_armed, ink_test_waiting, ink_test_release;
volatile bool ink_test_event_armed, ink_test_event_waiting, ink_test_event_release;
void ink_test_hold(void), ink_test_unlock(void);
void test_member_pause(void), test_member_release(void), test_member_pause_done(void);
bool test_member_paused(void);
void test_upload_pause(void), test_upload_release(void);
bool test_upload_waiting(void);

static volatile bool switched, switching, replied, blocked, unblock;
static int switch_result, starts;
static ink_rc_t receipt;
static uint64_t switch_us;
static const char *target = "kid";
static ink_exec_job_t hold_job;
static int event_count;
static const char *provider_id, *provider_args;
static const ink_db_def_t NOTES = {.name="review6.notes", .scope=INK_DB_MEMBER, .schema=1,
    .json=true, .ai_read=true, .ai_write=true};

static void switch_task(void *arg) {
    (void)arg;
    switching = true;
    uint64_t at = osal_now_us();
    switch_result = ink_member_switch(target);
    switch_us = osal_now_us() - at;
    switched = true;
}
static void spawn(void (*fn)(void *), const char *name) {
    osal_task_cfg_t cfg = {.name=name, .fn=fn, .stack_bytes=16384, .priority=OSAL_PRIO_NORMAL,
        .core=-1, .flags=OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "synthetic worker starts");
}
static void done(void *ctx, const ink_receipt_t *rc) {
    (void)ctx; receipt = rc->status; replied = true;
}
static bool submit(const char *id, const char *args) {
    ink_call_opts_t opts = {.src=INK_SRC_AI, .args_json=args, .reply_exec=INK_EXEC_DIRECT, .done=done};
    return ink_cap_call(id, NULL, &opts) == 0;
}
static void count_start(ink_call_t *c) { (void)c; starts++; }
static const ink_cap_t FIXTURES[] = {
    {.id="fixture.new", .run=count_start, .child_ok=true, .exec=INK_EXEC_DIRECT},
    {.id="fixture.queued", .run=count_start, .child_ok=true, .exec=INK_EXEC_MEDIA, .timeout_ms=80},
};
static void hold(void *arg) {
    (void)arg; blocked = true;
    while (!unblock) delay_ms(1);
}
static void begin_switch(void) {
    switched = switching = false;
    spawn(switch_task, "handoff");
    CHECK(WAIT_FOR(switching, 1000), "handoff starts");
    delay_ms(40); /* Observe the held provider, before releasing its real boundary. */
}
static void check_stopped(const char *member) {
    CHECK(!switched && !strcmp(ink_member_current()->id, member), "identity stays with the unfinished provider");
    int before = starts;
    ink_call_opts_t opts = {.src=INK_SRC_AI};
    CHECK(ink_cap_call("fixture.new", NULL, &opts) < 0 && starts == before, "handoff refuses new capability calls");
}
static void observe(void *ctx, const char *id, const ink_args_t *args) {
    (void)ctx; (void)id; (void)args;
    CHECK(!strcmp(ink_member_current()->id, "owner"), "queued private event finishes before identity changes");
    event_count++;
}
static void notify_task(void *arg) { (void)arg; CHECK(submit("notify.post", "{\"title\":\"SYNTH_PRIVATE\"}"), "real notification accepted"); }
static void provider_task(void *arg) { (void)arg; CHECK(submit(provider_id, provider_args), "real call accepted"); }

static void calls(const char *mode) {
    bool data = !strcmp(mode, "data"), cancel = !strcmp(mode, "cancel"), notify = !strcmp(mode, "events");
    const char *cap = data ? "data.put" : cancel ? "alarm.cancel" : !strcmp(mode, "alarm") ? "alarm.set" : "reminder.add";
    const char *args = data ? "{\"collection\":\"review6.notes\",\"id\":17,\"value\":{\"text\":\"SYNTH_PRIVATE\"}}" :
        !strcmp(mode, "alarm") ? "{\"hour\":7,\"minute\":30,\"weekdays\":127,\"title\":\"SYNTH_PRIVATE\"}" :
        "{\"at\":\"2099-12-01 12:00\",\"title\":\"SYNTH_PRIVATE\"}";
    char cancel_args[64];
    CHECK(ink_db_register(&NOTES) == 0, "synthetic collection registered");
    if (cancel) {
        CHECK(AI("alarm.set", "{\"hour\":7,\"minute\":30,\"weekdays\":127,\"title\":\"SYNTH_PRIVATE\"}") && g_rc.status == INK_RC_DONE, "adult alarm seeded");
        unsigned id = 0;
        CHECK(sscanf(g_rc.object, "alarm:%u", &id) == 1, "real alarm id");
        snprintf(cancel_args, sizeof cancel_args, "{\"id\":%u}", id); args = cancel_args;
        CHECK(ink_member_switch("kid") == 0, "child makes original cancellation"); target = "owner";
    }
    if (notify) {
        CHECK(ink_cap_subscribe("notify.posted", observe, NULL, INK_EXEC_MEDIA) == 0, "local event subscriber");
        CHECK(ink_exec_post(INK_EXEC_MEDIA, &hold_job, hold, NULL) == 0 && WAIT_FOR(blocked, 1000), "event executor held");
        ink_test_event_armed = true; spawn(notify_task, "notify-provider");
        CHECK(WAIT_FOR(ink_test_event_waiting, 1000), "notification persisted before event submission");
    } else {
        if (!data) ink_test_hold();
        ink_test_armed = true;
        provider_id = cap; provider_args = args; spawn(provider_task, "real-provider");
        CHECK(WAIT_FOR(ink_test_waiting, 1000), "real provider reaches review boundary");
    }
    begin_switch(); check_stopped(cancel ? "kid" : "owner");
    ink_test_release = ink_test_event_release = true;
    if (notify) {
        CHECK(WAIT_FOR(replied, 2000), "notification provider finishes");
        CHECK(!switched, "handoff also waits for queued local event");
        unblock = true;
    } else if (!data) ink_test_unlock();
    CHECK(WAIT_FOR(switched, 10000) && switch_result == 0, "handoff completes after provider and event");
    CHECK(replied, "original receipt delivered before handoff");
    ink_probe_value("test.switch.inflight_ms", switch_us / 1000.0);
    if (notify) CHECK(event_count == 1, "private local event delivered exactly once before handoff");
    if (cancel) {
        CHECK(receipt == INK_RC_REJECTED, "child cancellation retains child permissions");
        CHECK(AI("alarm.list", NULL) && strstr(g_result, "SYNTH_PRIVATE"), "adult alarm remains");
    } else {
        CHECK(receipt == INK_RC_DONE, "completed original operation has an honest success receipt");
        CHECK(AI(notify ? "notify.list" : data ? "data.list" : !strcmp(mode, "alarm") ? "alarm.list" : "reminder.list",
            data ? "{\"collection\":\"review6.notes\"}" : NULL) && !strstr(g_result, "SYNTH_PRIVATE"), "new member cannot read old private content");
    }
}

static char private_page[96];
static void create_private(lv_obj_t *screen, void *arg) {
    (void)arg; snprintf(private_page, sizeof private_page, "SYNTHETIC_OWNER_PRIVATE_PAGE");
    lv_label_set_text(lv_label_create(screen), private_page);
}
static void destroy_private(void) { private_page[0] = 0; }
static int snapshot(char *out, size_t cap) { return snprintf(out, cap, "%s", private_page); }
static const ink_activity_t PAGE = {.name="synthetic-private", .app="reader", .create=create_private,
    .destroy=destroy_private, .snapshot=snapshot};
static void open_private(void) { ink_ui_push(&PAGE, NULL); }
static void ask(void) { ink_ai_ask_text("SYNTHETIC_CHILD_QUESTION"); }
static volatile bool unlocked;
static void pin_done(void *ctx, int result, uint32_t wait_s) {
    (void)ctx; (void)wait_s; CHECK(result == INK_PIN_OK, "normal PIN worker completes"); unlocked = true;
}
static void publication(void) {
    on_ui(open_private); test_member_pause();
    CHECK(ink_member_unlock("kid", "", pin_done, NULL) == 0 && WAIT_FOR(test_member_paused(), 3000), "real unlock paused before state publication");
    CHECK(ink_hub_call_for("ai.ask", "{\"question\":\"SYNTHETIC_BOUNDARY\"}", 1000000, NULL, NULL,
        "kid", ink_member_epoch()) < 0, "transport refuses new requests while publication is pending");
    int before = (int)sim_metric_value("test.wire.asks");
    on_ui(ask); delay_ms(100);
    CHECK(sim_metric_value("test.wire.asks") == before, "handoff refuses new page uploads until publication finishes");
    test_member_release(); CHECK(WAIT_FOR(unlocked, 3000), "publication completes"); test_member_pause_done();
    CHECK(WAIT_FOR(strcmp(ink_ui_page_name(), "synthetic-private"), 3000), "old page is destroyed");
    on_ui(ask);
    CHECK(WAIT_FOR(sim_metric_value("test.wire.asks.kid") > 0, 10000), "new member can ask after handoff");
    CHECK(!strstr(sim_hub_last_ask(), "SYNTHETIC_OWNER_PRIVATE_PAGE"), "actual child wire request excludes adult page");
}
static void upload(void) {
    on_ui(open_private); test_upload_pause(); on_ui(ask);
    CHECK(WAIT_FOR(test_upload_waiting(), 3000), "real upload paused at socket send");
    begin_switch(); check_stopped("owner"); test_upload_release();
    CHECK(WAIT_FOR(switched, 10000) && switch_result == 0, "handoff waits for accepted upload");
    CHECK(sim_metric_value("test.wire.asks.owner") == 1, "accepted private upload stays on original connection");
    ink_probe_value("test.switch.upload_ms", switch_us / 1000.0);
}
static void queued(void) {
    CHECK(ink_exec_post(INK_EXEC_MEDIA, &hold_job, hold, NULL) == 0 && WAIT_FOR(blocked, 1000), "provider executor held");
    CHECK(submit("fixture.queued", NULL), "queued capability accepted");
    begin_switch();
    CHECK(WAIT_FOR(switched, 3000) && switch_result == 0, "queued timeout does not prevent handoff");
    CHECK(replied && receipt == INK_RC_CANCELLED && !starts, "QoS expiry cancels queued operation truthfully");
    unblock = true; delay_ms(100);
    CHECK(starts == 0, "cancelled queued provider never runs later");
    ink_probe_value("test.switch.timeout_ms", switch_us / 1000.0);
}
static void run(void *arg) {
    (void)arg;
    CHECK(WAIT_FOR(ink_hub_online() && ink_cap_available("voice.chat"), 30000), "synthetic hub ready");
    CHECK(ink_cap_register(FIXTURES, 2) == 0, "synthetic capabilities registered");
    const char *mode = getenv("INK_QUIESCE_MODE");
    if (!strcmp(mode, "publication")) publication();
    else if (!strcmp(mode, "upload")) upload();
    else if (!strcmp(mode, "queued")) queued();
    else if (!strcmp(mode, "idle")) {
        uint64_t at = osal_now_us();
        CHECK(ink_member_switch("kid") == 0, "idle handoff succeeds");
        ink_probe_value("test.switch.idle_ms", (osal_now_us() - at) / 1000.0);
    } else calls(mode);
    ink_probe_count("test.finished");
}
void ink_main(void) { product_main(); ink_hub_demand(); spawn(run, "quiesce-test"); }
