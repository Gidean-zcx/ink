#include "product.h"
#include "ink/sys/notify.h"
volatile bool ink_test_armed, ink_test_waiting, ink_test_release;
volatile bool ink_test_event_armed, ink_test_event_waiting, ink_test_event_release;
void ink_test_hold(void); void ink_test_unlock(void);
void ink_test_due_after(int64_t utc_s);
static bool worker_done, accepted;
static volatile bool call_done;
static ink_rc_t receipt;
static char cap[32], args[256];
static const ink_db_def_t NOTES = {.name="review6.notes", .scope=INK_DB_MEMBER, .schema=1, .json=true, .ai_read=true, .ai_write=true};
static int event_results, event_private;
static char event_status[24];
static bool time_event, deleting;
static unsigned delete_token;
static int64_t time_event_target;
static volatile bool removal_ready;
static ink_receipt_t removal_receipt;
static ink_exec_job_t event_barrier_job;
static volatile bool event_barrier_done;
static void event_barrier(void *ctx) { (void)ctx; event_barrier_done = true; }
static void removal_done(void *ctx, const ink_receipt_t *rc) {
    (void)ctx;
    removal_receipt = *rc;
    removal_receipt.result = NULL;
    removal_ready = true;
}
static void observe(void *ctx,const char *id,const ink_args_t *a) {
    (void)ctx;
    if (!strcmp(id,"confirm.result")) { ink_args_get_str(a,"status",event_status,sizeof event_status); event_results++; }
    else { char title[48]=""; ink_args_get_str(a,"title",title,sizeof title); if(strstr(title,"SYNTH_PRIVATE"))event_private++; }
}
static bool confirmed(const char *id,const char *params) {
    if(!BY_HAND(id,params)||g_rc.status!=INK_RC_PENDING_CONFIRMATION)return false;
    unsigned token=0; int before=event_results;
    return sscanf(g_rc.object,"confirm:%u",&token)==1 && ink_confirm_decide(token,true,INK_SRC_TOUCH)==0 && WAIT_FOR(event_results>before,5000) && !strcmp(event_status,"done");
}
static bool seen(int first,const char *needle) {
    for(int i=first;i<sim_hub_events_seen();i++) { const char *s=sim_hub_event_at(i); if(s&&strstr(s,needle))return true; }
    return false;
}
static void event_receipt(void *ctx,const ink_receipt_t *rc) { (void)ctx; receipt=rc->status; call_done=true; }
static void post_event(void *arg) {
    (void)arg;
    if (time_event) {
        ink_test_due_after(time_event_target + 120);
        accepted = true;
    } else {
        ink_call_opts_t o={.src=INK_SRC_AI,.args_json="{\"title\":\"SYNTH_PRIVATE\"}",.reply_exec=INK_EXEC_DIRECT,.done=event_receipt};
        accepted=ink_cap_call("notify.post",NULL,&o)==0;
    }
    worker_done=true;
}
static void run_events(bool change) {
    const char *event = !time_event ? "notify.posted" : !strcmp(cap,"alarm.set") ? "alarm.missed" : "reminder.missed";
    CHECK(ink_cap_subscribe("confirm.result",observe,NULL,INK_EXEC_DIRECT)==0 && ink_cap_subscribe(event,observe,NULL,INK_EXEC_DIRECT)==0,"event observers");
    CHECK(confirmed("member.add","{\"id\":\"former\",\"role\":\"adult\"}"),"add original adult");
    CHECK(ink_member_switch("former")==0,"original adult enters");
    delay_ms(300);
    if (time_event) {
        time_event_target = ink_time_now_s() / 60 * 60 + 120;
        ink_tm_t local;
        ink_time_to_local(time_event_target, &local);
        if (!strcmp(cap,"alarm.set")) snprintf(args, sizeof args, "{\"hour\":%d,\"minute\":%d,\"title\":\"SYNTH_PRIVATE\"}", local.hour, local.min);
        else snprintf(args, sizeof args, "{\"at\":\"%04d-%02d-%02d %02d:%02d\",\"title\":\"SYNTH_PRIVATE\"}", local.year, local.month, local.day, local.hour, local.min);
        CHECK(AI(cap, args) && g_rc.status == INK_RC_DONE, "seed actual due schedule through bus");
    }
    ink_test_event_armed=true;
    osal_task_cfg_t cfg={.name="review6-notify",.fn=post_event,.stack_bytes=16384,.priority=OSAL_PRIO_NORMAL,.core=-1,.flags=OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg,NULL)==OSAL_OK && WAIT_FOR(ink_test_event_waiting,2000),"real producer has its private event ready");
    if(change) {
        CHECK(ink_member_switch("owner")==0,"parent enters"); delay_ms(300);
        CHECK(confirmed("member.remove","{\"id\":\"former\"}"),"real adult cleanup completes");
        CHECK(confirmed("member.add","{\"id\":\"former\",\"role\":\"child\"}"),"reuse as child");
        CHECK(ink_member_switch("former")==0 && ink_member_current()->role==INK_ROLE_CHILD,"new child enters");
    }
    ink_hub_demand(); CHECK(WAIT_FOR(ink_hub_online(),10000),"current hub online");
    int first=sim_hub_events_seen(); ink_test_event_release=true;
    CHECK(WAIT_FOR(worker_done,3000) && accepted,"original provider returns");
    CHECK(ink_exec_post(INK_EXEC_BG,&event_barrier_job,event_barrier,NULL)==0 && WAIT_FOR(event_barrier_done,3000),"queued event subscribers complete");
    CHECK(ink_fs_write_atomic("/me/SYNTH_CONTROL.txt","X",1)==0 && WAIT_FOR(seen(first,"SYNTH_CONTROL"),3000),"current event delivered");
    CHECK(change?event_private==0:event_private==1,"local event must not inherit reused member generation: %d",event_private);
    CHECK(change?!seen(first,"SYNTH_PRIVATE"):seen(first,"SYNTH_PRIVATE"),"old title must not reach reused child's real AI connection");
    CHECK(AI("notify.list",NULL) && g_rc.status==INK_RC_DONE && (change?!strstr(g_result,"SYNTH_PRIVATE"):strstr(g_result,"SYNTH_PRIVATE")!=NULL),"stored lifecycle control: %s %s",ink_rc_name(g_rc.status),g_result);
    ink_probe_count("test.finished");
}
static void worker(void *arg) {
    (void)arg;
    if (deleting) {
        int before = event_results;
        accepted = ink_confirm_decide(delete_token, true, INK_SRC_TOUCH) == 0;
        CHECK(accepted && WAIT_FOR(event_results > before, 5000), "human-confirmed deletion finishes");
        receipt = !strcmp(event_status, "stale") ? INK_RC_STALE : !strcmp(event_status, "done") ? INK_RC_DONE : INK_RC_FAILED;
    } else {
        ink_call_opts_t o = {.src=INK_SRC_AI, .args_json=args, .reply_exec=INK_EXEC_DIRECT, .done=event_receipt};
        accepted = ink_cap_call(cap, NULL, &o) == 0;
        CHECK(accepted && WAIT_FOR(call_done, 8000), "original capability receipt");
    }
    worker_done = true;
}
static void run(void *arg) {
    (void)arg; delay_ms(1800);
    const char *mode = getenv("INK_REVIEW_MODE");
    bool data = !strncmp(mode, "data", 4), cancel = !strcmp(mode, "cancel");
    deleting = !strcmp(mode, "data_delete");
    bool change = getenv("INK_REVIEW_CHANGE") != NULL;
    const char *lifecycle = getenv("INK_REVIEW_LIFECYCLE");
    time_event = !strcmp(mode,"alarm_events") || !strcmp(mode,"reminder_events");
    if (!strcmp(mode,"events") || time_event) {
        snprintf(cap, sizeof cap, "%s", !strcmp(mode,"reminder_events") ? "reminder.add" : "alarm.set");
        run_events(change);
        return;
    }
    CHECK(ink_db_register(&NOTES)==0, "register synthetic collection");
    if (lifecycle) {
        CHECK(ink_cap_subscribe("confirm.result",observe,NULL,INK_EXEC_DIRECT)==0, "lifecycle observer");
        CHECK(confirmed("member.add","{\"id\":\"former\",\"role\":\"adult\"}") && ink_member_switch("former")==0, "original incarnation enters");
        delay_ms(300);
    }
    if (deleting) {
        CHECK(ink_member_switch("kid")==0, "seed child namespace");
        CHECK(AI("data.put", "{\"collection\":\"review6.notes\",\"id\":17,\"value\":{\"text\":\"SYNTH_CHILD\"}}") && g_rc.status==INK_RC_DONE, "child control record");
        CHECK(ink_member_switch("owner")==0, "seed adult namespace");
        CHECK(AI("data.put", "{\"collection\":\"review6.notes\",\"id\":17,\"value\":{\"text\":\"SYNTH_PRIVATE\"}}") && g_rc.status==INK_RC_DONE, "adult record with same id");
    }
    if (cancel) {
        CHECK(AI("alarm.set", "{\"hour\":7,\"minute\":30,\"weekdays\":127,\"title\":\"SYNTH_PRIVATE\"}") && g_rc.status==INK_RC_DONE, "seed adult alarm");
        unsigned id=0; CHECK(sscanf(g_rc.object,"alarm:%u",&id)==1, "seed id");
        snprintf(cap,sizeof cap,"alarm.cancel"); snprintf(args,sizeof args,"{\"id\":%u}",id);
        CHECK(ink_member_switch("kid")==0, "child is original caller");
    } else if (data) {
        snprintf(cap,sizeof cap,"%s",deleting?"data.delete":"data.put");
        snprintf(args,sizeof args,"%s",deleting?"{\"collection\":\"review6.notes\",\"id\":17}":"{\"collection\":\"review6.notes\",\"id\":17,\"value\":{\"text\":\"SYNTH_PRIVATE\"}}");
    } else if (!strcmp(mode,"alarm")) {
        snprintf(cap,sizeof cap,"alarm.set");
        snprintf(args,sizeof args,"{\"hour\":7,\"minute\":30,\"weekdays\":127,\"title\":\"SYNTH_PRIVATE\"}");
    } else {
        snprintf(cap,sizeof cap,"reminder.add");
        snprintf(args,sizeof args,"{\"at\":\"2099-12-01 12:00\",\"title\":\"SYNTH_PRIVATE\"}");
    }
    if (deleting) {
        CHECK(ink_cap_subscribe("confirm.result",observe,NULL,INK_EXEC_DIRECT)==0, "deletion confirmation observer");
        CHECK(BY_HAND(cap,args) && g_rc.status==INK_RC_PENDING_CONFIRMATION && sscanf(g_rc.object,"confirm:%u",&delete_token)==1, "real human deletion confirmation");
    }
    if (!data) ink_test_hold();
    ink_test_armed=true;
    osal_task_cfg_t cfg={.name="review6-call",.fn=worker,.stack_bytes=16384,.priority=OSAL_PRIO_NORMAL,.core=-1,.flags=OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg,NULL)==OSAL_OK && WAIT_FOR(ink_test_waiting,1000), "real provider reached boundary after policy gate");
    if (lifecycle) {
        CHECK(ink_member_switch("owner")==0, "parent enters while provider waits");
        delay_ms(300);
        if (data) {
            unsigned token = 0;
            ink_call_opts_t o = {.src=INK_SRC_TOUCH, .args_json="{\"id\":\"former\"}", .reply_exec=INK_EXEC_DIRECT, .done=removal_done};
            CHECK(ink_cap_call("member.remove",NULL,&o)==0 && !removal_ready && !ink_member_removing("former"), "cleanup request waits behind accepted data provider");
            ink_test_release=true;
            CHECK(WAIT_FOR(worker_done,5000) && WAIT_FOR(removal_ready,5000) && removal_receipt.status==INK_RC_PENDING_CONFIRMATION && sscanf(removal_receipt.object,"confirm:%u",&token)==1, "cleanup reaches real human confirmation");
            int before = event_results;
            CHECK(ink_confirm_decide(token,true,INK_SRC_TOUCH)==0 && WAIT_FOR(event_results>before,5000) && !strcmp(event_status,"done"), "serialized provider and cleanup complete");
            CHECK(AI("data.list","{\"collection\":\"review6.notes\"}") && g_rc.status==INK_RC_DONE && !strstr(g_result,"SYNTH_PRIVATE"), "old data must not be copied into parent's namespace");
        } else CHECK(confirmed("member.remove","{\"id\":\"former\"}"), "real lifecycle cleanup before provider resumes");
        if (!strcmp(lifecycle,"reused")) {
            CHECK(confirmed("member.add","{\"id\":\"former\",\"role\":\"child\"}") && ink_member_switch("former")==0, "new incarnation enters");
        }
    } else if (change) CHECK(ink_member_switch(cancel?"owner":"kid")==0,"member changes while provider waits");
    ink_test_release=true;
    if (!data) ink_test_unlock();
    CHECK(WAIT_FOR(worker_done,5000) && accepted,"original call finishes");
    if (change || lifecycle) CHECK(receipt==INK_RC_STALE,"old receipt is stale");
    if (cancel) {
        CHECK(ink_member_switch("owner")==0,"inspect original adult alarm");
        CHECK(AI("alarm.list",NULL) && g_rc.status==INK_RC_DONE && strstr(g_result,"SYNTH_PRIVATE"),"child request must not delete adult alarm: %s",g_result);
    } else if (deleting) {
        CHECK(ink_member_switch("kid")==0, "inspect child namespace");
        CHECK(AI("data.list","{\"collection\":\"review6.notes\"}") && g_rc.status==INK_RC_DONE && strstr(g_result,"SYNTH_CHILD"), "adult deletion must preserve child's record");
        CHECK(ink_member_switch("owner")==0, "inspect original namespace");
        CHECK(AI("data.list","{\"collection\":\"review6.notes\"}") && g_rc.status==INK_RC_DONE && !strstr(g_result,"SYNTH_PRIVATE"), "adult deletion stays in original namespace");
    } else {
        CHECK(AI(data?"data.list":!strcmp(mode,"alarm")?"alarm.list":"reminder.list",data?"{\"collection\":\"review6.notes\"}":NULL) && g_rc.status==INK_RC_DONE,"read actual member data through bus");
        CHECK(change || lifecycle?!strstr(g_result,"SYNTH_PRIVATE"):strstr(g_result,"SYNTH_PRIVATE")!=NULL,"accepted adult content must stay out of child collection: %s",g_result);
        if (change && !lifecycle) {
            CHECK(ink_member_switch("owner")==0, "inspect original adult namespace");
            CHECK(AI(data?"data.list":!strcmp(mode,"alarm")?"alarm.list":"reminder.list",data?"{\"collection\":\"review6.notes\"}":NULL) && g_rc.status==INK_RC_DONE && strstr(g_result,"SYNTH_PRIVATE"), "accepted write is in original adult namespace");
        }
    }
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg={.name="review6-main",.fn=run,.stack_bytes=16384,.priority=OSAL_PRIO_NORMAL,.core=-1,.flags=OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg,NULL)==OSAL_OK,"review task");
}
