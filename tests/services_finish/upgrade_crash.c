#include "product.h"

#include "ink/hal_ota.h"
static int review_mark_valid(void) {
    const char *cut = getenv("INK_REVIEW_CUT");
    if (cut && !strcmp(cut, "before")) {
        ink_probe_count("review.cut_before_valid");
        hal_power_reboot();
        for (;;) osal_task_delay_us(1000000);
    }
    if (cut && !strcmp(cut, "error")) { ink_probe_count("review.valid_error"); return -1; }
    int r = hal_ota_mark_valid();
    if (cut && !strcmp(cut, "after")) {
        ink_probe_count("review.cut_after_valid");
        hal_power_reboot();
        for (;;) osal_task_delay_us(1000000);
    }
    return r;
}
#define hal_ota_mark_valid review_mark_valid
#ifndef INK_TEST_UPDATE_SOURCE
#define INK_TEST_UPDATE_SOURCE "../../system/services/update/update_service.c"
#endif
#include INK_TEST_UPDATE_SOURCE
#undef hal_ota_mark_valid
static void review_run(void *arg) {
    (void)arg; delay_ms(1800);
    if (getenv("INK_REVIEW_INSTALL")) {
        CHECK(BY_HAND("system.upgrade_from_card", "{\"file\":\"next.inkfw\"}") &&
            g_rc.status == INK_RC_PENDING_CONFIRMATION, "installation asks");
        unsigned token = 0; sscanf(g_rc.object, "confirm:%u", &token);
        CHECK(ink_confirm_decide(token, true, INK_SRC_TOUCH) == 0, "person accepts");
    } else if (!getenv("INK_REVIEW_CUT")) {
        delay_ms(6000);
        ink_probe_value("review.version", U.me.version_code);
        ink_probe_value("review.rolled_back", U.me.rolled_back);
        CHECK(!U.st.state[0], "rollback must leave a recoverable transaction: version=%u state=%s code=%u path=%s",
            U.me.version_code, U.st.state, U.st.code, U.st.path);
        const char *why = not_now();
        CHECK(!why, "future installation must not be permanently blocked: %s", why ? why : "allowed");
        bool rollback = getenv("INK_REVIEW_EXPECT_ROLLBACK") != NULL;
        CHECK(U.me.version_code == (rollback ? 100u : 200u), "expected running version after commit boundary");
        if (rollback) CHECK(sim_metric_value("ota.resource_bytes") == 0, "rolled-back slot never writes foreign resources");
        else if (getenv("INK_REVIEW_EXPECT_RESOURCES")) CHECK(sim_metric_value("ota.resource_bytes") == 12288, "valid slot resumes all resources");
    }
    ink_probe_count("test.finished");
}

void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name="review-regression", .fn=review_run,
        .stack_bytes=16384, .priority=OSAL_PRIO_NORMAL, .core=-1,
        .flags=OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "review task");
}
