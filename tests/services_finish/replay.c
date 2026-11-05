#include "product.h"

static ink_call_t *held[2];
static ink_receipt_t completed[2];
static int held_count, done_count;
static void hold(ink_call_t *c) { if(held_count<2) held[held_count++]=c; ink_cap_defer(c); }
static void completed_call(void *ctx,const ink_receipt_t *rc) { completed[(intptr_t)ctx]=*rc; done_count++; }
static const ink_cap_t HOLD={.id="review.hold",.effect=INK_FX_LOCAL_WRITE,.child_ok=true,.exec=INK_EXEC_DIRECT,.run=hold};
static const char *requested_member;
static ink_source_t requested_source = INK_SRC_AI;
static bool with_id(const char *id,const char *args,const char *rid) {
    ink_call_opts_t o={.src=requested_source,.request_id=rid,.args_json=args,
        .member=requested_member?requested_member:ink_member_current()->id,.reply_exec=INK_EXEC_DIRECT,.done=rc_done};
    g_done=0;g_result[0]=0;
    return ink_cap_call(id,NULL,&o)==0 && WAIT_FOR(g_done,5000);
}
static void run(void *arg) {
    (void)arg;delay_ms(1800);
    const char *owner="{\"path\":\"/me/SYNTHETIC_ADULT_PRIVATE\",\"text\":\"owner\"}";
    CHECK(with_id("file.write",owner,"synthetic-request-1")&&g_rc.status==INK_RC_DONE,"adult write");
    CHECK(with_id("file.write",owner,"synthetic-request-1")&&g_rc.status==INK_RC_DONE,"same-member retry");
    requested_source=INK_SRC_TOUCH;
    CHECK(with_id("file.write",owner,"synthetic-request-1")&&g_rc.status==INK_RC_INVALID,"same id cannot change source");
    requested_source=INK_SRC_AI;
    CHECK(with_id("file.write","{\"path\":\"/me/changed.txt\",\"text\":\"other\"}","synthetic-request-1")&&g_rc.status==INK_RC_INVALID,"same member cannot reuse id for different parameters");
    CHECK(with_id("file.mkdir","{\"path\":\"/me/other\"}","synthetic-request-1")&&g_rc.status==INK_RC_INVALID,"same id cannot change capability");
    CHECK(ink_cap_register(&HOLD,1)==0,"in-flight test capability");
    ink_call_opts_t pending={.src=INK_SRC_AI,.request_id="synthetic-inflight",.args_json="{\"value\":1}",.reply_exec=INK_EXEC_DIRECT,.done=completed_call,.ctx=(void *)0};
    CHECK(ink_cap_call("review.hold",NULL,&pending)==0&&held_count==1,"first in-flight request accepted");
    CHECK(with_id("review.hold","{\"value\":1}","synthetic-inflight")&&g_rc.status==INK_RC_QUEUED,"same in-flight request does not run twice");
    CHECK(with_id("review.hold","{\"value\":2}","synthetic-inflight")&&g_rc.status==INK_RC_INVALID,"in-flight parameter collision refused");
    CHECK(ink_member_switch("kid")==0,"switch to child");
    pending.ctx=(void *)1;
    CHECK(ink_cap_call("review.hold",NULL,&pending)==0&&held_count==2,"different member gets an independent in-flight slot");
    if(held_count==2) { ink_cap_done(held[0]); ink_cap_done(held[1]); }
    CHECK(done_count==2&&completed[0].status==INK_RC_STALE&&completed[1].status==INK_RC_DONE,"late provider receipt cannot cross members");
    CHECK(with_id("file.write","{\"path\":\"/me/child.txt\",\"text\":\"child\"}","synthetic-request-1"),"child receipt");
    CHECK(!strstr(g_rc.object,"SYNTHETIC_ADULT_PRIVATE"),"adult receipt leaked: %s status=%s",g_rc.object,ink_rc_name(g_rc.status));
    CHECK(AI("file.read","{\"path\":\"/me/child.txt\"}")&&g_rc.status==INK_RC_DONE&&strstr(g_result,"child"),"acknowledged child file exists: %s",ink_rc_name(g_rc.status));
    CHECK(with_id("file.write","{\"path\":\"/me/control.txt\",\"text\":\"child\"}","synthetic-child-2")&&g_rc.status==INK_RC_DONE&&strstr(g_rc.object,"control.txt"),"distinct id control");
    requested_member="owner";
    CHECK(with_id("file.write",owner,"synthetic-old-fresh")&&g_rc.status==INK_RC_STALE,"uncached old-member request is stale");
    CHECK(with_id("file.write",owner,"synthetic-request-1")&&g_rc.status==INK_RC_STALE&&!strstr(g_rc.object,"SYNTHETIC_ADULT_PRIVATE"),"cached old-member retry bypasses isolation: %s %s",ink_rc_name(g_rc.status),g_rc.object);
    requested_member=NULL;
    CHECK(ink_member_switch("owner")==0,"adult returns");
    CHECK(BY_HAND("member.set_pin","{\"id\":\"owner\",\"new_pin\":\"6767\"}")&&g_rc.status==INK_RC_DONE,"synthetic adult PIN");
    ink_power_lock();
    CHECK(WAIT_FOR(!ink_member_space_open(),2000),"adult space closes");
    CHECK(with_id("file.write",owner,"synthetic-request-1")&&g_rc.status==INK_RC_REJECTED&&!g_rc.object[0],"closed space cannot replay cached private receipt");
    ink_probe_count("test.finished");
}

void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg={.name="review2-probe",.fn=run,.stack_bytes=16384,
        .priority=OSAL_PRIO_NORMAL,.core=-1,.flags=OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg,NULL)==OSAL_OK,"test task");
}
