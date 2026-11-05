#include "product.h"

#include "ink/hal_power.h"
static int results;
static char result_status[24];
static void result(void *ctx,const char *id,const ink_args_t *args) {
    (void)ctx;(void)id;ink_args_get_str(args,"status",result_status,sizeof result_status);results++;
}
static bool confirmed(const char *cap,const char *args) {
    if(!BY_HAND(cap,args)||g_rc.status!=INK_RC_PENDING_CONFIRMATION)return false;
    unsigned token=0;sscanf(g_rc.object,"confirm:%u",&token);
    int before=results;
    return ink_confirm_decide(token,true,INK_SRC_TOUCH)==0&&WAIT_FOR(results>before,5000)&&!strcmp(result_status,"done");
}

static void run(void *arg) {
    (void)arg;delay_ms(1800);
    CHECK(ink_cap_subscribe("confirm.result",result,NULL,INK_EXEC_DIRECT)==0,"listener");
    CHECK(confirmed("member.add","{\"id\":\"unused\",\"role\":\"child\"}"),"add unused member");
    bool removed=confirmed("member.remove","{\"id\":\"unused\"}");
    CHECK(removed,"empty member removal should succeed: %s",result_status);
    if(!removed) {
        removed=confirmed("member.remove","{\"id\":\"unused\"}");
        CHECK(removed,"retry should not remain blocked by absent parent: %s",result_status);
        if(!removed) {
            CHECK(AI("member.list",NULL)&&strstr(g_result,"\"removing\":true"),"failed removal retains quarantine");
            CHECK(ink_fs_write_atomic("/me/control.txt","SYNTHETIC",9)==0,"unrelated owner file establishes personal root");
            CHECK(confirmed("member.remove","{\"id\":\"unused\"}"),"same retry succeeds once parent exists");
        }
    }
    ink_probe_count("test.finished");
}

void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg={.name="review2-probe",.fn=run,.stack_bytes=16384,
        .priority=OSAL_PRIO_NORMAL,.core=-1,.flags=OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg,NULL)==OSAL_OK,"test task");
}
