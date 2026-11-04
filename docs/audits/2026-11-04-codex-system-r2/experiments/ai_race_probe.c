/* A real reply on the hub task, with a UI key press scheduled at its lock boundary.
 * Both AI and boot sources are included unchanged, as in tests/integration/lifecycle.c. */
#include <stdio.h>
#include "ink/osal.h"
#include "ink/sys/ai.h"
#include "ink/sys/exec.h"
#include "ink/sys/rt.h"
#include "ink/sys/ui.h"

static void audit_unlock(osal_mutex_t *m);
#define osal_mutex_unlock audit_unlock
#include "../../../../system/services/ai/ai_service.c"
#undef osal_mutex_unlock
#define ink_main product_main
#include "../../../../system/boot/main.c"
#undef ink_main

static int armed, ran, old_sid, new_sid, new_seq;
static osal_sem_t *new_turn_done;
static ink_exec_job_t next_job, key_job;
static void new_turn(void *ctx) {
    (void)ctx;
    ink_ai_key(true,osal_now_us());
    new_sid=AI.session; new_seq=AI.seq;
    printf("AUDIT ai inside_new_press old_sid=%d new_sid=%d seq=%d mic_failed=%d rt_count=%d\n",old_sid,new_sid,new_seq,AI.mic_failed,ink_rt_open_count());
    osal_sem_give(new_turn_done);
}
static void audit_unlock(osal_mutex_t *m) {
    osal_mutex_unlock(m);
    if(armed && m==AI.lock && AI.session && AI.session_seq==AI.seq && AI.drop_frames && !AI.up_open) {
        armed=0; old_sid=AI.session;
        ink_exec_post(INK_EXEC_UI,&next_job,new_turn,NULL);
        osal_sem_take(new_turn_done,OSAL_WAIT_FOREVER);
        ran=1;
    }
}
static void key(void *ctx) { ink_ai_key(ctx!=NULL,osal_now_us()); }
static void audit_task(void *ctx) {
    (void)ctx;
    osal_task_delay_us(3000000);
    ink_exec_post(INK_EXEC_UI,&key_job,key,(void*)1);
    osal_task_delay_us(1000000);
    ink_exec_post(INK_EXEC_UI,&key_job,key,NULL);
    armed=1;
    for(int n=0;!ran && n<3000;n++) osal_task_delay_us(10000);
    osal_task_delay_us(100000);
    printf("AUDIT ai after_old_reply hook=%d new_sid=%d new_seq=%d current_session=%d capturing=%d up_open=%d rt_count=%d\n",ran,new_sid,new_seq,AI.session,AI.capturing,AI.up_open,ink_rt_open_count());
    fflush(stdout);
}
void ink_main(void) {
    new_turn_done=osal_sem_create(0,1);
    product_main();
    osal_task_cfg_t cfg={.name="audit",.fn=audit_task,.stack_bytes=4096,.priority=OSAL_PRIO_NORMAL,.core=-1,.flags=OSAL_TASK_STACK_EXTERNAL};
    if(osal_task_create(&cfg,NULL)!=OSAL_OK) osal_panic("audit task");
}
