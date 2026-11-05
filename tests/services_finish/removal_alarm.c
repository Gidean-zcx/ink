#include "product.h"

#include "ink/hal_power.h"
#include "ink/sys/notify.h"
static const ink_db_def_t PRIVATE = {.name="review2.private", .scope=INK_DB_MEMBER, .schema=1, .json=true, .ai_read=true, .ai_write=true};
void ink_test_hold_notify(void);
void ink_test_release_notify(void);
void ink_test_hold_activity(void);
void ink_test_release_activity(void);
static int cancelled, delayed_events;
static ink_fs_dir_t *old_dir;
static bool ui_held, release_ui;
static ink_exec_job_t held_ui_job;
static void cancel_task(void *ctx) { (void)ctx; cancelled++; }
static void hold_ui(void *ctx) { (void)ctx; ui_held = true; while (!release_ui) osal_task_delay_us(10000); }
static void delayed_event(void *ctx, const char *id, const ink_args_t *a) { (void)ctx; (void)id; (void)a; delayed_events++; }

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
static void inspect(void) {
    CHECK(ink_member_switch("oldadult")==0&&ink_member_current()->role==INK_ROLE_CHILD,"reused id is child");
    CHECK(AI("alarm.list",NULL)&&g_rc.status==INK_RC_DONE,"child alarm list");
    CHECK(!strstr(g_result,"SYNTH_ALARM"),"adult alarm leaked: %s",g_result);
    CHECK(AI("reminder.list",NULL)&&g_rc.status==INK_RC_DONE,"child reminder list");
    CHECK(!strstr(g_result,"SYNTH_REMINDER"),"adult reminder leaked: %s",g_result);
    char raw[32];
    CHECK(ink_storage_read("/user/m/oldadult/synthetic.raw",raw,sizeof raw)<0,"raw Flash read cache erased");
    CHECK(AI("notify.list",NULL) && !strstr(g_result,"SYNTH_PRIVATE"),"old notifications do not return");
    CHECK(AI("data.list","{\"collection\":\"review2.private\"}") && !strstr(g_result,"SYNTH_PRIVATE"),"old member collections do not return");
    CHECK(AI("activity.list",NULL) && !strstr(g_result,"SYNTH_PRIVATE"),"deferred activity does not return");
    CHECK(AI("job.list",NULL) && !strstr(g_result,"SYNTH_PRIVATE"),"running, ended and shared task history erased");
    CHECK(AI("file.read","{\"path\":\"/me/control.txt\"}") && g_rc.status != INK_RC_DONE,"personal file erased");
    ink_fs_stat_t st;
    CHECK(ink_fs_stat("/app/synthetic/private.txt", &st) != 0 && ink_fs_stat("/cache/SYNTH_PRIVATE.txt", &st) != 0,"app files and unowned transient cache erased");
}
static void run(void *arg) {
    (void)arg;delay_ms(1800);
    CHECK(ink_cap_subscribe("confirm.result",result,NULL,INK_EXEC_DIRECT)==0,"listener");
    CHECK(ink_db_register(&PRIVATE)==0,"member collection");
    if(getenv("INK_REVIEW_INSPECT")){inspect();ink_probe_count("test.finished");return;}
    CHECK(confirmed("member.add","{\"id\":\"oldadult\",\"role\":\"adult\"}"),"add adult");
    CHECK(ink_member_switch("oldadult")==0,"enter adult");
    CHECK(ink_fs_write_atomic("/me/control.txt","SYNTHETIC",9)==0,"personal card file");
    CHECK(ink_storage_write_atomic("/user/m/oldadult/synthetic.raw","SYNTH_PRIVATE",13)==0 && WAIT_FOR(!ink_storage_pending(),4000),"raw cached member state");
    CHECK(ink_fs_dir_open("/me",&old_dir)==0,"old personal directory handle");
    CHECK(ink_fs_write_atomic("/app/synthetic/private.txt","SYNTH_PRIVATE",13)==0,"app-private file");
    CHECK(ink_fs_write_atomic("/cache/SYNTH_PRIVATE.txt","SYNTH_PRIVATE",13)==0,"global transient cache");
    CHECK(AI("data.put","{\"collection\":\"review2.private\",\"value\":{\"text\":\"SYNTH_PRIVATE\"}}") && g_rc.status==INK_RC_DONE,"private member record");
    CHECK(AI("notify.post","{\"title\":\"SYNTH_PRIVATE\"}") && g_rc.status==INK_RC_DONE,"private notification");
    int ended = ink_task_begin("synthetic", "SYNTH_PRIVATE_ENDED", false, NULL, NULL);
    ink_task_end(ended, INK_TASK_DONE, "SYNTH_PRIVATE_RESULT", NULL);
    CHECK(ink_task_begin("synthetic", "SYNTH_PRIVATE_RUNNING", false, cancel_task, NULL)>0,"running member task");
    CHECK(ink_task_begin("synthetic", "SYNTH_PRIVATE_SHARED", true, cancel_task, NULL)>0,"formerly owned shared task");
    ink_test_hold_activity();
    CHECK(AI("file.write","{\"path\":\"/me/SYNTH_PRIVATE.txt\",\"text\":\"draft\"}") && g_rc.status==INK_RC_DONE,"hold accepted activity before its write");
    char old_undo[160];
    snprintf(old_undo,sizeof old_undo,"{\"object\":\"%s\",\"revision\":%u}",g_rc.object,g_rc.revision);
    CHECK(AI("alarm.set","{\"hour\":7,\"minute\":30,\"weekdays\":127,\"title\":\"SYNTH_ALARM\"}")&&g_rc.status==INK_RC_DONE,"adult alarm");
    CHECK(AI("reminder.add","{\"at\":\"2099-12-01 12:00\",\"title\":\"SYNTH_REMINDER\"}")&&g_rc.status==INK_RC_DONE,"adult reminder");
    CHECK(ink_member_switch("kid")==0,"distinct child control");
    CHECK(AI("alarm.list",NULL)&&!strstr(g_result,"SYNTH_ALARM"),"distinct child cannot see adult alarm");
    CHECK(ink_member_switch("owner")==0,"owner manages family");delay_ms(300);
    ink_test_hold_notify();
    ink_notify_t n = {.title="SYNTH_PRIVATE_LATE", .member="oldadult", .prio=INK_NOTIFY_NORMAL};
    CHECK(ink_notify_post(&n,NULL)==0,"hold queued notification for old member");
    CHECK(ink_exec_post(INK_EXEC_UI,&held_ui_job,hold_ui,NULL)==0 && WAIT_FOR(ui_held,1000),"hold local event delivery");
    CHECK(ink_cap_subscribe("review.private",delayed_event,NULL,INK_EXEC_UI)==0,"delayed event listener");
    ink_args_t ev; ink_args_init(&ev); ink_args_str(&ev,"member","oldadult");
    ink_cap_emit("review.private",&ev);
    CHECK(confirmed("member.remove","{\"id\":\"oldadult\"}"),"normal removal done");
    CHECK(confirmed("member.add","{\"id\":\"oldadult\",\"role\":\"child\"}"),"reuse id as child");
    CHECK(cancelled==2,"both running tasks cancelled before id reuse");
    ink_test_release_notify(); ink_test_release_activity();
    release_ui=true;
    delay_ms(300);
    CHECK(delayed_events==0,"queued local event cannot cross incarnation");
    ink_fs_stat_t old_entry;
    CHECK(ink_fs_dir_next(old_dir,&old_entry)==INK_FS_ERR_DENIED,"old directory handle revoked across id reuse");
    ink_fs_dir_close(old_dir);
    inspect();
    CHECK(BY_HAND("file.write","{\"path\":\"/me/SYNTH_PRIVATE.txt\",\"text\":\"SYNTH_CHILD_NEW\"}") && g_rc.status==INK_RC_DONE,"child creates a new object at old path");
    CHECK(AI("system.undo",old_undo) && g_rc.status != INK_RC_UNDONE,"old incarnation undo ticket refused");
    CHECK(AI("file.read","{\"path\":\"/me/SYNTH_PRIVATE.txt\"}") && g_rc.status==INK_RC_DONE && strstr(g_result,"SYNTH_CHILD_NEW"),"old ticket preserves the child new file");
    ink_db_flush_all();
    CHECK(WAIT_FOR(ink_db_idle()&&!ink_storage_pending(),4000),"durable checkpoint");
    ink_probe_count("test.finished");hal_power_reboot();
}

void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg={.name="review2-probe",.fn=run,.stack_bytes=16384,
        .priority=OSAL_PRIO_NORMAL,.core=-1,.flags=OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg,NULL)==OSAL_OK,"test task");
}
