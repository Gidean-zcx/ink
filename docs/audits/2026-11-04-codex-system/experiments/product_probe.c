/* Run the actual product boot, adding only a background-job observer. */
#include <stdio.h>
#define ink_main product_main
#include "../../../../system/boot/main.c"
#undef ink_main
static int steps;
static ink_job_step_t step(void *ctx) { (void)ctx; steps++; return INK_JOB_DONE; }
static void observe(void *arg) {
    (void)arg;
    osal_task_delay_us(24000000);
    static const ink_job_t job={.name="audit.after_voice", .step=step};
    int id=ink_job_submit(&job);
    osal_task_delay_us(2000000);
    printf("AUDIT after_dismiss rt_count=%d job=%d steps=%d pending=%d\n",ink_rt_open_count(),id,steps,ink_jobs_pending());
    ink_rt_close(1); /* experimental control: explicitly release the leaked first-turn session */
    osal_task_delay_us(500000);
    printf("AUDIT explicit_close rt_count=%d steps=%d pending=%d\n",ink_rt_open_count(),steps,ink_jobs_pending());
    fflush(stdout);
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg={.name="audit.observe", .fn=observe, .stack_bytes=8192,.priority=OSAL_PRIO_NORMAL,.core=-1,.flags=OSAL_TASK_STACK_EXTERNAL};
    if (osal_task_create(&cfg,NULL)!=OSAL_OK) osal_panic("audit observer");
}
