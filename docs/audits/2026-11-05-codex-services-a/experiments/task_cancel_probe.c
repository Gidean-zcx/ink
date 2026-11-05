#include "product_probe.h"
#include "ink/sys/svc.h"
extern int audit_dl_job(void),audit_dl_task(void),audit_dl_waiting(void);
static osal_sem_t *at_boundary,*other_done;
static osal_task_t *caller;
static bool armed;
static int old_job,new_task;
static volatile bool restarted;
static void restarted_cb(void *ctx,const ink_receipt_t *rc) {
    (void)ctx;
    CHECK(rc->status==INK_RC_DONE,"second download starts (%s)",rc->detail);
    restarted=true;
}
void audit_task_unlock(void) {
    ink_svc_unlock();
    if(armed && osal_task_self()==caller) {
        armed=false;
        osal_sem_give(at_boundary);
        CHECK(osal_sem_take(other_done,5000000)==OSAL_OK,"other task completed transition");
    }
}
static void other(void *arg) {
    (void)arg;
    osal_sem_take(at_boundary,OSAL_WAIT_FOREVER);
    ink_job_cancel(old_job);
    CHECK(WAIT_FOR(!audit_dl_job(),2000),"old download finishes");
    ink_call_opts_t opts={.src=INK_SRC_TOUCH,.reply_exec=INK_EXEC_DIRECT,.done=restarted_cb};
    CHECK(ink_cap_call("update.download",NULL,&opts)==0,"start next download through capability");
    CHECK(WAIT_FOR(restarted && audit_dl_job()>0,2000),"new download active");
    new_task=audit_dl_task();
    printf("AUDIT before_old_cancel_resumes new_job=%d new_task=%d\n",audit_dl_job(),new_task);
    osal_sem_give(other_done);
}
static void audit_task(void *arg) {
    (void)arg;delay_ms(1800);caller=osal_task_self();
    at_boundary=osal_sem_create(0,1);other_done=osal_sem_create(0,1);
    osal_task_cfg_t cfg={.name="audit-other",.fn=other,.stack_bytes=16384,.priority=OSAL_PRIO_NORMAL,.core=-1,.flags=OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg,NULL)==OSAL_OK,"second task");
    ink_hub_demand();CHECK(WAIT_FOR(ink_hub_online(),30000),"online");
    CHECK(AI("update.check",NULL) && g_rc.status==INK_RC_DONE,"offer");
    CHECK(BY_HAND("update.download",NULL) && g_rc.status==INK_RC_DONE,"first download");
    CHECK(WAIT_FOR(audit_dl_waiting()==1,3000),"in flight");
    old_job=audit_dl_job();int old_task=audit_dl_task();
    char args[64];snprintf(args,sizeof args,"{\"id\":%d}",old_task);
    armed=true;
    CHECK(BY_HAND("job.cancel",args) && g_rc.status==INK_RC_DONE,"request cancellation of old task");
    CHECK(WAIT_FOR(!audit_dl_job(),2000),"old callback stopped new job");
    CHECK(AI("job.list",NULL),"task list");
    printf("AUDIT old_task=%d new_task=%d tasks=%s\n",old_task,new_task,g_result);
    CHECK(new_task!=old_task && strstr(g_result,"\"state\":\"cancelled\""),"reproduced old cancel crossing into new download");
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg={.name="audit-cancel",.fn=audit_task,.stack_bytes=16384,.priority=OSAL_PRIO_NORMAL,.core=-1,.flags=OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg,NULL)==OSAL_OK,"audit task");
}
