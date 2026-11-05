#include "product_probe.h"
extern int audit_waiting_chunk(void), audit_download_task(void);
static void audit_task(void *arg) {
    (void)arg;
    delay_ms(1800);
    ink_hub_demand();
    CHECK(WAIT_FOR(ink_hub_online(), 20000), "hub online");
    CHECK(BY_HAND("update.check", NULL) && g_rc.status == INK_RC_DONE, "check: %s", g_rc.detail);
    CHECK(BY_HAND("update.download", NULL) && g_rc.status == INK_RC_DONE, "download: %s", g_rc.detail);
    for (int i = 0; i < 10000 && !audit_waiting_chunk(); i++) delay_ms(1);
    CHECK(audit_waiting_chunk(), "a real chunk request is pending");
    char a[64];
    snprintf(a, sizeof a, "{\"id\":%d}", audit_download_task());
    printf("AUDIT cancelling pending download task=%d\n", audit_download_task()); fflush(stdout);
    CHECK(BY_HAND("job.cancel", a) && g_rc.status == INK_RC_DONE, "cancel: %s", g_rc.detail);
    delay_ms(4000);
    printf("AUDIT survived late chunk\n");
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t c = {.name="audit-cancel", .fn=audit_task, .stack_bytes=16384, .priority=OSAL_PRIO_NORMAL, .core=-1, .flags=OSAL_TASK_STACK_EXTERNAL};
    osal_task_create(&c, NULL);
}
