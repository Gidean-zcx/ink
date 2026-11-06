#include "../services_finish/product.h"

void test_member_pause(void), test_member_release(void), test_member_pause_done(void);
bool test_member_paused(void);
void test_ai_review_voice(void), test_ai_review_release(void), test_ai_private_queued(void);
int ink_text_target_session(void);
uint32_t ink_text_edit_revision(void);

static char private_page[96];
static void create_private(lv_obj_t *screen, void *arg) {
    (void)arg;
    snprintf(private_page, sizeof private_page, "SYNTHETIC_%s_PRIVATE_PAGE", ink_member_current()->id);
    lv_label_set_text(lv_label_create(screen), private_page);
    ink_text_cfg_t cfg = {.type=INK_TEXT_PLAIN, .initial="SYNTHETIC_OWNER_PRIVATE_INPUT", .max=128};
    CHECK(ink_text_focus(&cfg, NULL, NULL) > 0, "private input opened normally");
}
static void destroy_private(void) { ink_text_blur(0); private_page[0] = 0; }
static int snapshot_private(char *out, size_t cap) { return snprintf(out, cap, "%s", private_page); }
static const ink_activity_t PAGE = {.name="synthetic-private", .app="reader", .create=create_private,
    .destroy=destroy_private, .snapshot=snapshot_private};
static void read_private(ink_call_t *c) { snprintf(c->rc.say, sizeof c->rc.say, "%s", private_page); }
static const ink_cap_t PRIVATE_CAP = {.id="fixture.private_page", .title="合成私有页面", .activity="synthetic-private",
    .effect=INK_FX_READ, .child_ok=true, .run=read_private};
static void open_private(void) { ink_ui_push(&PAGE, NULL); }
static void check_owner_context(void) {
    char snapshot[2048]; const char *name = NULL;
    CHECK(ink_ui_snapshot(snapshot, sizeof snapshot, &name) > 0 && strstr(snapshot, "SYNTHETIC_owner_PRIVATE_PAGE") &&
          strstr(snapshot, "SYNTHETIC_OWNER_PRIVATE_INPUT"), "current owner page and input remain readable");
    ink_receipt_t rc;
    CHECK(ink_cap_invoke(PRIVATE_CAP.id, NULL, INK_SRC_AI, 0, &rc) == INK_RC_DONE &&
          strstr(rc.say, "SYNTHETIC_owner_PRIVATE_PAGE"), "current owner page capability remains available");
}
static void ask_child(void) {
    char snapshot[2048]; const char *name = NULL;
    ink_text_snapshot(snapshot, sizeof snapshot);
    CHECK(!strstr(snapshot, "SYNTHETIC_OWNER_PRIVATE_INPUT"), "stale input snapshot is unavailable before publication");
    ink_receipt_t rc;
    CHECK(ink_cap_invoke(PRIVATE_CAP.id, NULL, INK_SRC_AI, 0, &rc) == INK_RC_INVALID &&
          !strstr(rc.say, "SYNTHETIC_owner_PRIVATE_PAGE"), "new member cannot invoke old page capability");
    char edit[160];
    snprintf(edit, sizeof edit, "{\"session\":%d,\"revision\":%u,\"text\":\"SYNTHETIC_CHILD_EDIT\"}",
             ink_text_target_session(), ink_text_edit_revision());
    CHECK(ink_cap_invoke_json("text.insert", edit, INK_SRC_AI, 0, &rc) == INK_RC_STALE &&
          !strstr(ink_text_value(), "SYNTHETIC_CHILD_EDIT"), "new member cannot edit old input session");
    ink_text_blur(0);
    ink_ui_snapshot(snapshot, sizeof snapshot, &name);
    CHECK(!strstr(snapshot, "SYNTHETIC_owner_PRIVATE_PAGE"), "stale page snapshot is unavailable before publication");
    CHECK(!strcmp(ink_member_current()->id, "kid"), "new question belongs to child");
    ink_ai_ask_text("SYNTHETIC_CHILD_QUESTION");
}
static volatile bool pin_done;
static void pin_told(void *ctx, int result, uint32_t wait_s) {
    (void)ctx; (void)wait_s;
    CHECK(result == INK_PIN_OK, "real member worker finishes"); pin_done = true;
}
static void change_paused(const char *member) {
    pin_done = false; test_member_pause();
    CHECK(ink_member_unlock(member, "", pin_told, NULL) == 0, "real member unlock queued");
    CHECK(WAIT_FOR(test_member_paused(), 5000) && !strcmp(ink_member_current()->id, member),
          "identity changed while publication is paused");
}
static void finish_change(void) {
    test_member_release(); CHECK(WAIT_FOR(pin_done, 5000), "member publisher completes"); test_member_pause_done();
}
static void ready(void) {
    CHECK(WAIT_FOR(ink_hub_online() && ink_cap_available("voice.chat") &&
          ink_hub_member_matches(ink_member_current()->id, ink_hub_member_epoch(), ink_hub_connection()), 30000),
          "receiving connection has current identity");
}
static void warmup(void) { ink_ai_ask_text("SYNTHETIC_WARMUP"); }
static osal_sem_t *ui_release;
static bool ui_paused;
static ink_exec_job_t hold_job;
static void hold_ui(void *arg) {
    (void)arg; __atomic_store_n(&ui_paused, true, __ATOMIC_RELEASE);
    osal_sem_take(ui_release, OSAL_WAIT_FOREVER);
}
static void run(void *arg) {
    (void)arg; ink_hub_demand(); ready();
    on_ui(open_private); on_ui(check_owner_context);
    change_paused("kid");
    on_ui(ask_child);
    CHECK(WAIT_FOR(sim_metric_value("test.wire.asks.kid") > 0, 10000), "new question reaches child receiving context");
    const char *wire = sim_hub_last_ask();
    CHECK(wire && !strstr(wire, "SYNTHETIC_owner_PRIVATE_PAGE") && !strstr(wire, "SYNTHETIC_OWNER_PRIVATE_INPUT"),
          "new child request never uploads owner page or input");
    CHECK(wire && strstr(wire, "\"page_gen\":0") && strstr(wire, "\"actions\":[]") && strstr(wire, "\"state\":{}"),
          "handoff request carries no stale page actions or state");
    finish_change();
    CHECK(WAIT_FOR(strcmp(ink_ui_page_name(), "synthetic-private"), 5000), "old private page is destroyed");
    pin_done = false;
    CHECK(ink_member_unlock("owner", "", pin_told, NULL) == 0 && WAIT_FOR(pin_done, 5000), "owner returns normally");
    ready();
    double answers = sim_metric_value("ai.answers");
    on_ui(warmup); CHECK(WAIT_FOR(sim_metric_value("ai.answers") > answers, 10000), "text warmup completes");
    answers = sim_metric_value("ai.answers");
    double bytes = sim_metric_value("hub.binary_bytes");
    on_ui(test_ai_review_voice);
    CHECK(WAIT_FOR(sim_metric_value("hub.binary_bytes") > bytes, 3000), "real microphone frames reach hub");
    on_ui(test_ai_review_release);
    CHECK(WAIT_FOR(sim_metric_value("ai.answers") > answers, 15000), "voice answer completes");
    on_ui(test_ai_private_queued);
    ui_release = osal_sem_create(0, 1);
    CHECK(ui_release && ink_exec_post(INK_EXEC_UI, &hold_job, hold_ui, NULL) == 0 &&
          WAIT_FOR(__atomic_load_n(&ui_paused, __ATOMIC_ACQUIRE), 5000), "busy UI cannot revoke cached speech");
    change_paused("kid");
    delay_ms(200); /* Let the already submitted DMA ring leave the observation window. */
    double before = sim_audio_played_ms();
    delay_ms(800);
    double extra = sim_audio_played_ms() - before;
    ink_probe_value("test.private_speech_after_handoff_ms", extra);
    CHECK(extra == 0, "finished adult speech cannot play before member publication: %.0f ms", extra);
    osal_sem_give(ui_release);
    finish_change();
    on_ui(warmup); /* Also wait until the held UI job has left its semaphore. */
    osal_sem_delete(ui_release);
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    CHECK(ink_cap_register(&PRIVATE_CAP, 1) == 0, "synthetic page capability registered");
    osal_task_cfg_t cfg = {.name="private-boundary", .fn=run, .stack_bytes=16384, .priority=OSAL_PRIO_NORMAL,
        .core=-1, .flags=OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "private boundary task started");
}
