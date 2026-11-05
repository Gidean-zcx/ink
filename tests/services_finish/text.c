#include "product.h"
extern void ink_test_fail_snapshot(void);
static void edits(void) {
    ink_text_cfg_t cfg = {.type=INK_TEXT_PLAIN, .initial="A", .max=4095};
    CHECK(ink_text_focus(&cfg, NULL, NULL) > 0, "focus");
    ink_text_commit_utf8("B");
    ink_text_commit_utf8("C");
    CHECK(ink_text_undo() && !strcmp(ink_text_value(), "AB"), "undo C");
    ink_test_fail_snapshot();
    ink_text_commit_utf8("D");
    CHECK(!ink_text_can_redo() && !ink_text_redo() && !strcmp(ink_text_value(), "ABD"), "AUD18-202: failed snapshot cannot resurrect stale redo");
    ink_text_blur(0);
    cfg.initial = "中文"; cfg.max = 4;
    CHECK(ink_text_focus(&cfg, NULL, NULL) > 0 && !strcmp(ink_text_value(), "中"), "AUD18-209: initial text ends at a whole character");
    ink_text_blur(0);
}
static int entered;
static void enter(void *ctx, const char *value) { (void)ctx; (void)value; entered++; }
static const ink_text_client_t client = {.entered=enter};
static void hidden(void) {
    ink_text_cfg_t cfg = {.type=INK_TEXT_PLAIN, .initial="SYNTHETIC", .ai_hidden=true};
    CHECK(ink_text_focus(&cfg, &client, NULL) > 0, "hidden focus");
}
static void run(void *arg) {
    (void)arg;
    delay_ms(1800);
    on_ui(edits);
    on_ui(hidden);
    CHECK(AI("text.submit", NULL) && g_rc.status == INK_RC_REJECTED && !entered, "AUD18-208: hidden field cannot be submitted");
    CHECK(AI("text.insert", "{\"text\":\"x\"}") && g_rc.status == INK_RC_REJECTED, "hidden field cannot be edited");
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name="finish-text", .fn=run, .stack_bytes=16384, .priority=OSAL_PRIO_NORMAL, .core=-1, .flags=OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "test task");
}
