/* Two legal screen updates overlap while the UI task is busy. Product decoder/show code unchanged. */
#include <stdio.h>
#include "ink/osal.h"
#include "ink/sys/exec.h"
#include "ink/sys/ui.h"
#include "../../../../system/apps/remote/remote.c"
#define ink_main product_main
#include "../../../../system/boot/main.c"
#undef ink_main

static osal_sem_t *decoded;
static ink_exec_job_t setup_job, media_job, inspect_job;
static void timer_noop(lv_timer_t *t) { (void)t; }
static void region(int x, int black, int last) {
    uint8_t p[11]={(uint8_t)x,0,0,0,1,0,1,0,0,1,(uint8_t)(black?0:0xFF)};
    region_job_t *j=osal_calloc(1,sizeof *j+sizeof p,OSAL_MEM_EXTERNAL);
    j->gen=RS.gen; j->len=sizeof p; j->flags=last?INK_RT_F_LAST:0;
    memcpy(j->data,p,sizeof p); decode_region(j);
}
static void decode_batch(void *ctx) {
    (void)ctx;
    region(0,1,1); /* complete update A: black at x=0 */
    region(1,1,0); /* only the first region of update B: black at x=1, no LAST yet */
    osal_sem_give(decoded);
}
static void inspect(void *ctx) {
    (void)ctx;
    printf("AUDIT remote A_finished_B_unfinished shown_A=%d shown_partial_B=%d updates=%u owed=%d\n",RS.px[0],RS.px[1],RS.updates,RS.owed);
    fflush(stdout);
}
static void setup(void *ctx) {
    (void)ctx;
    if(!RS.lock) RS.lock=osal_mutex_create();
    RS.gen=1; RS.w=RS.h=4; RS.active=true; RS.first_shown=true;
    RS.px=osal_malloc(16,OSAL_MEM_EXTERNAL); RS.back=osal_malloc(16,OSAL_MEM_EXTERNAL);
    memset(RS.px,0xFF,16); memset(RS.back,0xFF,16);
    RS.img=lv_obj_create(lv_screen_active());
    RS.credit_timer=lv_timer_create(timer_noop,1000,NULL); lv_timer_pause(RS.credit_timer);
    ink_exec_post(INK_EXEC_MEDIA,&media_job,decode_batch,NULL);
    osal_sem_take(decoded,OSAL_WAIT_FOREVER); /* UI occupied while both frames arrive */
    ink_exec_post(INK_EXEC_UI,&inspect_job,inspect,NULL); /* after the queued show_update */
}
static void audit_task(void *ctx) {
    (void)ctx; osal_task_delay_us(2000000);
    ink_exec_post(INK_EXEC_UI,&setup_job,setup,NULL);
}
void ink_main(void) {
    decoded=osal_sem_create(0,1); product_main();
    osal_task_cfg_t cfg={.name="audit",.fn=audit_task,.stack_bytes=4096,.priority=OSAL_PRIO_NORMAL,.core=-1,.flags=OSAL_TASK_STACK_EXTERNAL};
    if(osal_task_create(&cfg,NULL)!=OSAL_OK) osal_panic("audit task");
}
