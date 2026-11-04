/* Inspect the real product's AI state on its UI task. No injected failure or altered product code. */
#include <stdio.h>
#include "ink/osal.h"
#include "ink/sys/exec.h"
#include "sim.h"
#include "../../../../system/services/ai/ai_service.c"
#define ink_main product_main
#include "../../../../system/boot/main.c"
#undef ink_main

static ink_exec_job_t observe_job;
static osal_sem_t *observed;

static void snapshot(void *where) {
    printf("AUDIT_AI %s at=%.3f session=%d seq=%d capturing=%d asking=%d up_open=%d drop=%d rt=%d failures=%.0f answers=%.0f status=%s\n",
           (char *)where, osal_now_us()/1e6, AI.session, AI.seq, AI.capturing, AI.asking,
           AI.up_open, AI.drop_frames, ink_rt_open_count(), sim_metric_value("ai.failures"),
           sim_metric_value("ai.answers"), AI.status ? lv_label_get_text(AI.status) : "none");
    osal_sem_give(observed);
}

static void observe_at(int s, const char *where) {
    uint64_t t=(uint64_t)s*1000000, now=osal_now_us();
    if(t>now) osal_task_delay_us(t-now);
    ink_exec_post(INK_EXEC_UI, &observe_job, snapshot, (void *)where);
    osal_sem_take(observed, OSAL_WAIT_FOREVER);
}

static void audit_task(void *arg) {
    (void)arg;
    observe_at(5, "speaking");
    observe_at(13, "lost_while_held");
    observe_at(19, "after_release");
    observe_at(28, "network_back");
    observe_at(43, "next_turn");
    ink_probe_count("audit.finished");
}

void ink_main(void) {
    observed=osal_sem_create(0,1);
    product_main();
    osal_task_cfg_t c={.name="audit-ai",.fn=audit_task,.stack_bytes=4096,.priority=3,.core=-1,.flags=OSAL_TASK_STACK_EXTERNAL};
    if(osal_task_create(&c,NULL)!=OSAL_OK) osal_panic("audit task");
}
