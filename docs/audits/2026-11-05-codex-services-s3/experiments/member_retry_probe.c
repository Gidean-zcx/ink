#include "product_probe.h"
extern void audit_arm_retry(void), audit_release_retry(void);
extern int audit_retry_entered(void);
static volatile int pin_done, pin_result;
static void told(void *ctx, int r, uint32_t wait_s) { (void)ctx; (void)wait_s; pin_result=r; pin_done=1; }
static void audit_task(void *arg) {
    (void)arg; delay_ms(1800);
    if (getenv("AUDIT_WRITE_FIXTURE")) {
        CHECK(ink_member_set_pin("owner", NULL, "123456", told, NULL) == 0, "set fixture PIN");
        CHECK(WAIT_FOR(pin_done, 5000) && pin_result == INK_PIN_OK, "fixture accepted");
        ink_db_flush_all();
        CHECK(WAIT_FOR(ink_db_idle() && !ink_storage_pending(), 6000), "fixture durable");
    } else {
        CHECK(ink_member_has_pin("owner") && !ink_member_space_open(), "unread table initially fails closed");
        audit_arm_retry();
        CHECK(ink_member_unlock("owner", "000000", told, NULL) == 0, "start wrong PIN retry");
        CHECK(WAIT_FOR(audit_retry_entered(), 5000), "retry reaches datastore iteration");
        int open = ink_member_space_open(), pin = ink_member_has_pin("owner");
        CHECK(AI("state.list", NULL), "AI direct call while retry is paused");
        printf("AUDIT retry has_pin=%d space_open=%d ai_status=%d\n", pin, open, g_rc.status);
        CHECK(open && !pin && g_rc.status == INK_RC_DONE, "reproduce transient fail-open");
        audit_release_retry();
        CHECK(WAIT_FOR(pin_done, 5000) && pin_result == INK_PIN_WRONG, "PIN still rejected eventually");
        printf("AUDIT after_retry space_open=%d pin_result=%d\n", ink_member_space_open(), pin_result);
        CHECK(!ink_member_space_open(), "closes after reload");
    }
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t c = {.name="audit-retry", .fn=audit_task, .stack_bytes=16384, .priority=OSAL_PRIO_NORMAL, .core=-1, .flags=OSAL_TASK_STACK_EXTERNAL};
    osal_task_create(&c, NULL);
}
