/* Saturate the UI's wake queue while owner-held jobs wait, then verify FIFO and busy coverage. */
#include <stdio.h>
#include "ink/osal.h"
#include "ink/sys/exec.h"
#include "ink/sys/ui.h"
#include "../../../../system/ui/ui_task.c"
#define ink_main product_main
#include "../../../../system/boot/main.c"
#undef ink_main
static osal_sem_t *entered, *release;
static ink_exec_job_t block_job, jobs[80];
static int count, order_ok=1, busy_ok=1;
static void block(void *ctx) { (void)ctx; osal_sem_give(entered); osal_sem_take(release,OSAL_WAIT_FOREVER); }
static void item(void *ctx) {
    if((intptr_t)ctx!=count) order_ok=0;
    if(!ink_ui_busy()) busy_ok=0;
    count++;
}
static void audit_task(void *ctx) {
    (void)ctx; osal_task_delay_us(2000000);
    ink_exec_post(INK_EXEC_UI,&block_job,block,NULL);
    osal_sem_take(entered,OSAL_WAIT_FOREVER);
    for(intptr_t i=0;i<80;i++) ink_exec_post(INK_EXEC_UI,&jobs[i],item,(void*)i);
    printf("AUDIT ui blocked wake_queue=%zu busy=%d waiting=%d\n",osal_queue_count(UI.q),ink_ui_busy(),ink_exec_waiting(INK_EXEC_UI));
    osal_sem_give(release);
    for(int i=0;count<80 && i<500;i++) osal_task_delay_us(10000);
    osal_task_delay_us(1500000);
    printf("AUDIT ui drained count=%d fifo=%d busy_during_jobs=%d waiting=%d\n",count,order_ok,busy_ok,ink_exec_waiting(INK_EXEC_UI));
    fflush(stdout);
}
void ink_main(void) {
    entered=osal_sem_create(0,1); release=osal_sem_create(0,1); product_main();
    osal_task_cfg_t cfg={.name="audit",.fn=audit_task,.stack_bytes=4096,.priority=OSAL_PRIO_NORMAL,.core=-1,.flags=OSAL_TASK_STACK_EXTERNAL};
    if(osal_task_create(&cfg,NULL)!=OSAL_OK) osal_panic("audit task");
}
