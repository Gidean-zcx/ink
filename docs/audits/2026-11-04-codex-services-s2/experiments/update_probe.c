#include "product_probe.h"
extern int audit_resource_case(int corrupt);
static volatile int audit_done;
static ink_exec_job_t resource_job;
static void run_resource(void *arg) {
    (void)arg;
    CHECK(audit_resource_case(getenv("AUDIT_CORRUPT_RESOURCE") != NULL) == 0, "reproduce resource recovery flaw");
    audit_done = 1;
}
static void audit_task(void *arg) {
    (void)arg;
    delay_ms(2000);
    ink_exec_post(INK_EXEC_BG, &resource_job, run_resource, NULL);
    CHECK(WAIT_FOR(audit_done, 6000), "resource case finishes");
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t c = {.name="audit-update", .fn=audit_task, .stack_bytes=16384, .priority=OSAL_PRIO_NORMAL, .core=-1, .flags=OSAL_TASK_STACK_EXTERNAL};
    osal_task_create(&c, NULL);
}
