#include "product.h"

static const ink_db_def_t NOTES = {.name = "review3.undo", .scope = INK_DB_MEMBER, .schema = 1,
    .json = true, .ai_read = true, .ai_write = true};
static ink_call_t *held;
static ink_receipt_t late_receipt;
static int late_done;

static void start_write(ink_call_t *c) {
    static const char draft[] = "{\"text\":\"SYNTH_OLD_MEMBER\"}";
    ink_db_t *db = ink_db_open(&NOTES, NULL);
    ink_db_write_t w = {.id = 42, .data = draft, .len = sizeof draft - 1, .src = c->src, .written_ver = &c->rc.revision};
    CHECK(db && ink_db_put(db, &w, NULL) == INK_DB_OK, "provider writes old member record");
    ink_db_close(db);
    snprintf(c->rc.object, sizeof c->rc.object, "review3.undo#42");
    held = c;
    ink_cap_defer(c);
}
static void done(void *ctx, const ink_receipt_t *rc) {
    (void)ctx;
    late_receipt = *rc;
    late_done++;
}
static const ink_cap_t WRITE = {.id = "review.late_undo", .effect = INK_FX_LOCAL_WRITE,
    .child_ok = true, .exec = INK_EXEC_DIRECT, .run = start_write};

static void run(void *arg) {
    (void)arg;
    delay_ms(1800);
    CHECK(ink_db_register(&NOTES) == 0 && ink_cap_register(&WRITE, 1) == 0, "register deferred provider");
    ink_call_opts_t opts = {.src = INK_SRC_AI, .reply_exec = INK_EXEC_DIRECT, .done = done};
    CHECK(ink_cap_call(WRITE.id, NULL, &opts) == 0 && held, "old member write is awaiting its final receipt");
    if (!held) { ink_probe_count("test.finished"); return; }
    uint32_t old_revision = held->rc.revision;
    CHECK(ink_member_switch("kid") == 0, "child takes device before provider finishes");
    delay_ms(300);
    ink_db_t *db = ink_db_open(&NOTES, NULL);
    static const char human[] = "{\"text\":\"SYNTH_CHILD_KEEP\"}";
    uint32_t child_revision = 0;
    ink_db_write_t w = {.id = 42, .data = human, .len = sizeof human - 1, .src = INK_SRC_TOUCH, .written_ver = &child_revision};
    CHECK(db && ink_db_put(db, &w, NULL) == INK_DB_OK && child_revision == old_revision, "independent member collections may have equal record versions");
    ink_db_close(db);
    ink_args_t inverse;
    ink_args_init(&inverse);
    ink_args_str(&inverse, "collection", NOTES.name);
    ink_args_int(&inverse, "id", 42);
    ink_args_int(&inverse, "if_revision", old_revision);
    ink_cap_undoable(held, "data.delete", &inverse, 10000);
    CHECK(held->rc.undo_ms == 0, "old provider cannot mint an inverse under the new member");
    ink_cap_done(held);
    CHECK(WAIT_FOR(late_done, 1000) && late_receipt.status == INK_RC_STALE, "late receipt remains bound to its original member");
    char undo[128];
    snprintf(undo, sizeof undo, "{\"object\":\"review3.undo#42\",\"revision\":%u}", old_revision);
    CHECK(AI("system.undo", undo) && g_rc.status == INK_RC_FAILED, "child cannot execute the old member's inverse");
    CHECK(AI("data.get", "{\"collection\":\"review3.undo\",\"id\":42}") && g_rc.status == INK_RC_DONE && strstr(g_result, "SYNTH_CHILD_KEEP"), "child record survives");
    CHECK(AI("data.put", "{\"collection\":\"review3.undo\",\"id\":43,\"value\":{\"text\":\"SYNTH_CONTROL\"}}") && g_rc.status == INK_RC_DONE && g_rc.undo_ms > 0, "current member still receives a valid inverse");
    snprintf(undo, sizeof undo, "{\"object\":\"%s\",\"revision\":%u}", g_rc.object, g_rc.revision);
    CHECK(AI("system.undo", undo) && g_rc.status == INK_RC_UNDONE, "current member's untouched record is undoable");
    ink_probe_count("test.finished");
}

void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name = "review3-undo-owner", .fn = run, .stack_bytes = 16384,
        .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "test task");
}
