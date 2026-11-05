#include "product.h"
#include "ink/sys/hub.h"
static int got_list;
static int test_hub_call(const char *method, const char *params, int64_t timeout, ink_hub_reply_fn cb, void *ctx) {
    if (!strcmp(method, "ai.listen") || !strcmp(method, "ai.ask")) {
        char *expected = osal_malloc(32768, OSAL_MEM_EXTERNAL);
        int n = expected ? ink_cap_ids_json(expected, 32768) : -1;
        const char *at = strstr(params, "\"actions\":");
        CHECK(n > 768, "actual capability table exceeds the old buffer (%d)", n);
        CHECK(at && n > 0 && !strncmp(at + strlen("\"actions\":"), expected, (size_t)n), "HUB2-006: complete actual table in %s", method);
        got_list++;
        osal_free(expected);
        return -1;
    }
    return ink_hub_call(method, params, timeout, cb, ctx);
}
#define ink_hub_call test_hub_call
#ifndef INK_TEST_AI_SOURCE
#define INK_TEST_AI_SOURCE "../../system/services/ai/ai_service.c"
#endif
#include INK_TEST_AI_SOURCE
#undef ink_hub_call
static void ask(void) { request("ai.listen", true); request("ai.ask", false); }
static void run(void *arg) {
    (void)arg;
    delay_ms(1800);
    on_ui(ask);
    CHECK(got_list == 2, "both request paths checked");
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name="finish-actions", .fn=run, .stack_bytes=16384, .priority=OSAL_PRIO_NORMAL, .core=-1, .flags=OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "test task");
}
