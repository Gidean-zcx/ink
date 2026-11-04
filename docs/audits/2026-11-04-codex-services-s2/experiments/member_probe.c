/* Whole product and its existing test helpers. Only the task below is audit code. */
#include "product_probe.h"
static volatile int pin_done, pin_result;
static void pin_told(void *ctx, int result, uint32_t wait_s) {
    (void)ctx; (void)wait_s;
    pin_result = result;
    pin_done = 1;
}
static int set_pin(const char *id, const char *old_pin, const char *new_pin) {
    pin_done = 0;
    if (ink_member_set_pin(id, old_pin, new_pin, pin_told, NULL)) return -99;
    return WAIT_FOR(pin_done, 5000) ? pin_result : -98;
}

static void audit_task(void *arg) {
    (void)arg;
    delay_ms(1800);
    const char *mode = getenv("AUDIT_MODE");
    if (mode && !strcmp(mode, "write")) {
        CHECK(set_pin("owner", NULL, "123456") == INK_PIN_OK, "set fixture PIN");
        ink_db_flush_all();
        CHECK(WAIT_FOR(ink_db_idle() && !ink_storage_pending(), 6000), "fixture durable");
        printf("AUDIT member setup has_pin=%d\n", ink_member_has_pin("owner"));
    } else {
        bool failed = getenv("AUDIT_MEMBER_LOAD_FAIL") != NULL;
        printf("AUDIT member boot injected=%d has_pin=%d space_open=%d\n", failed, ink_member_has_pin("owner"), ink_member_space_open());
        CHECK(AI("file.list", "{\"path\":\"/me\"}"), "AI call returns");
        printf("AUDIT member AI file.list status=%d detail=%s\n", g_rc.status, g_rc.detail);
        if (failed) CHECK(!ink_member_has_pin("owner") && ink_member_space_open() && g_rc.status != INK_RC_REJECTED, "reproduce fail-open on member table load error");
        else CHECK(ink_member_has_pin("owner") && !ink_member_space_open() && g_rc.status == INK_RC_REJECTED, "control remains protected");
    }
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t c = {.name="audit-member", .fn=audit_task, .stack_bytes=16384, .priority=OSAL_PRIO_NORMAL, .core=-1, .flags=OSAL_TASK_STACK_EXTERNAL};
    osal_task_create(&c, NULL);
}
