/* Executor startup under fixed-priority scheduling on the two-core profile. Two higher-priority
 * callers arrive after state=starting is published. All callers have the normal unpinned affinity. */
#include <stdio.h>
#include "ink/osal.h"
#include "ink/sys/exec.h"
static osal_sem_t *preempt_create(uint32_t initial, uint32_t max);
#define osal_sem_create preempt_create
#include "../../../../system/services/res/exec.c"
#undef osal_sem_create
static osal_sem_t *go;
static int enabled, low_done, high_done;
static osal_sem_t *preempt_create(uint32_t initial, uint32_t max) {
    if(enabled) {
        enabled=0;
        osal_sem_give(go);
        osal_sem_give(go);
        osal_task_delay_us(1000); /* model preemption while the first task starts the executor */
    }
    return osal_sem_create(initial,max);
}
static void low(void *ctx) { (void)ctx; ink_exec_ready(INK_EXEC_BG); low_done=1; }
static void high(void *ctx) { (void)ctx; osal_sem_take(go,OSAL_WAIT_FOREVER); ink_exec_ready(INK_EXEC_BG); high_done++; }
static void observe(void *ctx) {
    (void)ctx; osal_task_delay_us(200000);
    printf("AUDIT exec startup state=%d low_done=%d high_done=%d\n",EX[INK_EXEC_BG].state,low_done,high_done);
    fflush(stdout);
}
void ink_main(void) {
    ink_exec_init(); go=osal_sem_create(0,2); enabled=1;
    osal_task_cfg_t c={.name="audit-high",.fn=high,.stack_bytes=4096,.priority=5,.core=-1,.flags=OSAL_TASK_STACK_EXTERNAL};
    if(osal_task_create(&c,NULL)!=OSAL_OK) osal_panic("high");
    c.name="audit-higher"; c.priority=6;
    if(osal_task_create(&c,NULL)!=OSAL_OK) osal_panic("higher");
    c.name="audit-observe"; c.fn=observe; c.priority=10;
    if(osal_task_create(&c,NULL)!=OSAL_OK) osal_panic("observe");
    c.name="audit-low"; c.fn=low; c.priority=3;
    if(osal_task_create(&c,NULL)!=OSAL_OK) osal_panic("low");
}
