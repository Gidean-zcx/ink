#include "product.h"

static void review_run(void *arg) {
    (void)arg; delay_ms(1800);
    CHECK(AI("settings.set", "{\"id\":\"display.light\",\"value\":10}") && g_rc.status==INK_RC_DONE, "first change");
    CHECK(AI("settings.set", "{\"id\":\"display.light\",\"value\":20}") && g_rc.status==INK_RC_DONE, "second change");
    CHECK(AI("system.undo", "{\"object\":\"setting:display.light\"}") && g_rc.status==INK_RC_UNDONE, "undo second change");
    CHECK(ink_setting_get("display.light")==10, "undo must restore immediate previous value 10, actual=%lld",
        (long long)ink_setting_get("display.light"));
    CHECK(AI("system.undo", "{\"object\":\"setting:display.light\"}") && g_rc.status == INK_RC_STALE &&
          ink_setting_get("display.light") == 10, "older inverse cannot overwrite the newer revision");
    CHECK(AI("settings.set", "{\"id\":\"display.light\",\"value\":20}") && g_rc.status == INK_RC_DONE, "new AI change");
    char undo[160];
    snprintf(undo, sizeof undo, "{\"object\":\"%s\",\"revision\":%u}", g_rc.object, g_rc.revision);
    CHECK(BY_HAND("settings.set", "{\"id\":\"display.light\",\"value\":30}") && g_rc.status == INK_RC_DONE, "later human change");
    CHECK(AI("system.undo", undo) && g_rc.status == INK_RC_STALE && ink_setting_get("display.light") == 30, "bound AI undo preserves later human change");
    ink_receipt_t rc;
    CHECK(ink_setting_set("display.light", 40, INK_SRC_TOUCH, &rc) == INK_RC_DONE, "direct human change");
    CHECK(AI("system.undo", "{\"object\":\"setting:display.light\"}") && g_rc.status == INK_RC_STALE &&
          ink_setting_get("display.light") == 40, "unbound undo also validates the setting version");
    CHECK(AI("settings.set", "{\"id\":\"display.light\",\"value\":50}") && g_rc.status == INK_RC_DONE, "expiring AI change");
    delay_ms(10100); /* Cross the documented 10-second undo deadline. */
    CHECK(AI("system.undo", "{\"object\":\"setting:display.light\"}") && g_rc.status == INK_RC_FAILED &&
          ink_setting_get("display.light") == 50, "expired undo has no effect");
    ink_probe_count("test.finished");
}

void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name="review-regression", .fn=review_run,
        .stack_bytes=16384, .priority=OSAL_PRIO_NORMAL, .core=-1,
        .flags=OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "review task");
}
