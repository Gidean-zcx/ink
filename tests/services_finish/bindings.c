#include "product.h"
#include "ink/sys/json.h"
static int session;
static int entered;
static char value[64];
static const char *initial;
static void submitted(void *ctx, const char *text) { (void)ctx; (void)text; entered++; }
static const ink_text_client_t client = {.entered=submitted};
static void focus(void) {
    ink_text_cfg_t cfg = {.type=INK_TEXT_PLAIN, .initial=initial};
    session = ink_text_focus(&cfg, &client, NULL);
    CHECK(session > 0, "focus");
}
static void read_value(void) { snprintf(value, sizeof value, "%s", ink_text_value()); }
static void edit(void) { ink_text_commit_utf8("L"); }
static void move(void) { ink_text_select(0, 0); }
static void blur(void) { ink_text_blur(0); }
static int64_t revision(void) {
    CHECK(AI("text.state", NULL) && g_rc.status == INK_RC_DONE, "read target");
    ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL);
    int64_t rev = 1;
    if (j && ink_json_parse(j, g_result, strlen(g_result)) == 0) ink_json_int(j, ink_json_find(j, 0, "revision"), &rev);
    osal_free(j);
    return rev;
}
static void run(void *arg) {
    (void)arg; delay_ms(1800);
    initial = "A"; on_ui(focus);
    int old_session = session; int64_t old_rev = revision();
    initial = "B"; on_ui(focus);
    char args[180];
    snprintf(args, sizeof args, "{\"session\":%d,\"revision\":%lld,\"text\":\"Z\"}", old_session, (long long)old_rev);
    CHECK(AI("text.insert", args) && g_rc.status == INK_RC_STALE, "AUD18-401: old field rejected");
    on_ui(read_value); CHECK(!strcmp(value, "B"), "new field unchanged (%s)", value);
    snprintf(args, sizeof args, "{\"session\":%d,\"revision\":%lld}", old_session, (long long)old_rev);
    CHECK(AI("text.submit", args) && g_rc.status == INK_RC_STALE && !entered, "AUD18-401: old submit cannot enter new field");
    old_rev = revision(); on_ui(edit);
    snprintf(args, sizeof args, "{\"session\":%d,\"revision\":%lld,\"text\":\"Z\"}", session, (long long)old_rev);
    CHECK(AI("text.insert", args) && g_rc.status == INK_RC_STALE, "AUD18-401: old content rejected");
    on_ui(read_value); CHECK(!strcmp(value, "BL"), "human edit preserved (%s)", value);
    old_rev = revision(); on_ui(move);
    snprintf(args, sizeof args, "{\"session\":%d,\"revision\":%lld,\"text\":\"Z\"}", session, (long long)old_rev);
    CHECK(AI("text.insert", args) && g_rc.status == INK_RC_STALE, "AUD18-401: old selection rejected");
    old_rev = revision();
    snprintf(args, sizeof args, "{\"session\":%d,\"revision\":%lld,\"start\":1,\"end\":2}", session, (long long)old_rev);
    CHECK(AI("text.select", args) && g_rc.status == INK_RC_DONE && g_rc.revision > old_rev, "selection returns its new edit revision");
    int64_t selected_rev = g_rc.revision;
    snprintf(args, sizeof args, "{\"session\":%d,\"revision\":%lld,\"text\":\"X\"}", session, (long long)selected_rev);
    CHECK(AI("text.insert", args) && g_rc.status == INK_RC_DONE, "fresh target accepted");
    on_ui(read_value); CHECK(!strcmp(value, "BX"), "correct selection replaced (%s)", value);
    on_ui(blur);
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name="finish-bindings", .fn=run, .stack_bytes=16384, .priority=OSAL_PRIO_NORMAL, .core=-1, .flags=OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "test task");
}
