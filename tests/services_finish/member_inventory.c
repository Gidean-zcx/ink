
#include "product.h"
#include "ink/sys/crc.h"
#include "ink/hal_power.h"
static int results;
static char result_status[24];
static void result(void *ctx,const char *id,const ink_args_t *args) {
    (void)ctx;(void)id;ink_args_get_str(args,"status",result_status,sizeof result_status);results++;
}
static bool confirmed(const char *cap,const char *args) {
    if(!BY_HAND(cap,args)||g_rc.status!=INK_RC_PENDING_CONFIRMATION)return false;
    unsigned token=0;if(sscanf(g_rc.object,"confirm:%u",&token)!=1)return false;
    int before=results;
    return ink_confirm_decide(token,true,INK_SRC_TOUCH)==0&&WAIT_FOR(results>before,5000)&&!strcmp(result_status,"done");
}

static const ink_db_def_t PREFS={.name="notify.prefs",.scope=INK_DB_MEMBER,.schema=1,.json=true};
static const char *const collections[]={"activity.log","notify.inbox","notify.prefs","text.user_freq","media.positions"};
static const char *const rawpaths[]={"/user/m/oldadult/reader.12345678.pos","/user/m/oldadult/app.synthetic.state"};
static const char marker[]="{\"text\":\"SYNTH_PRIVATE\"}";
static const ink_db_def_t *def(int i) { return i==2?&PREFS:ink_db_find_def(collections[i]); }
static void inspect(void) {
    for(int i=0;i<5;i++) {
        ink_db_t *db=ink_db_open(def(i),"oldadult");
        CHECK(db&&ink_db_get(db,88001,NULL,NULL,0)==INK_DB_ERR_NOT_FOUND,"old record absent: %s",collections[i]);
        ink_db_close(db);
        db=ink_db_open(def(i),"owner");
        CHECK(db&&ink_db_get(db,88001,NULL,NULL,0)==INK_DB_OK,"other adult record preserved: %s",collections[i]);
        ink_db_close(db);
    }
    char raw[64];
    for(int i=0;i<2;i++) CHECK(ink_storage_read(rawpaths[i],raw,sizeof raw)<0,"raw member file absent: %s",rawpaths[i]);
    CHECK(ink_storage_read("/user/m/owner/app.synthetic.state",raw,sizeof raw)==(int)sizeof marker-1 && !memcmp(raw,marker,sizeof marker-1),"other adult raw state preserved");
    CHECK(ink_member_switch("oldadult")==0,"enter child");
    ink_fs_stat_t st;
    CHECK(ink_fs_stat("/me/nested/private.txt",&st)!=0,"nested personal file absent");
    CHECK(ink_fs_stat("/app/synthetic/private.txt",&st)!=0,"app private file absent");
    CHECK(ink_fs_stat("/cache/sys/synthetic",&st)!=0,"global cache removed");
    CHECK(ink_fs_stat("/cache/sys/media-pos",&st)!=0,"media position cache removed");
    CHECK(ink_fs_stat("/family/SYNTH_CONTROL.txt",&st)==0,"shared content preserved");
    CHECK(ink_member_current()->role==INK_ROLE_CHILD,"reused member is child");
}
static void run(void *arg) {
    (void)arg; delay_ms(1800);
    CHECK(ink_cap_subscribe("confirm.result",result,NULL,INK_EXEC_DIRECT)==0,"listener");
    if(getenv("INK_REVIEW_INSPECT")) {inspect();ink_probe_count("test.finished");return;}
    CHECK(confirmed("member.add","{\"id\":\"oldadult\",\"role\":\"adult\"}"),"add adult");
    CHECK(ink_member_switch("oldadult")==0,"enter old adult");delay_ms(300);
    int registered=0;
    for(int i=0;ink_db_def_at(i);i++) if(ink_db_def_at(i)->scope==INK_DB_MEMBER) registered++;
    CHECK(registered==4,"all registered member collections enumerated: %d",registered);
    for(int i=0;i<5;i++) for(int k=0;k<2;k++) {
        ink_db_t *db=ink_db_open(def(i),k?"owner":"oldadult");
        ink_db_write_t w={.id=88001,.data=marker,.len=sizeof marker-1,.src=INK_SRC_TOUCH};
        CHECK(db&&ink_db_put(db,&w,NULL)==INK_DB_OK,"seed actual collection: %s",collections[i]);
        if(db) CHECK(ink_db_wait(db,ink_db_last_ver(db),4000000)==0,"collection durable");
        ink_db_close(db);
    }
    for(int i=0;i<2;i++) CHECK(ink_storage_write_atomic(rawpaths[i],marker,sizeof marker-1)==0,"seed raw file");
    CHECK(ink_storage_write_atomic("/user/m/owner/app.synthetic.state",marker,sizeof marker-1)==0,"seed other adult raw state");
    CHECK(ink_fs_write_atomic("/me/nested/private.txt",marker,sizeof marker-1)==0,"seed personal file");
    CHECK(ink_fs_write_atomic("/app/synthetic/private.txt",marker,sizeof marker-1)==0,"seed app file");
    CHECK(ink_fs_write_atomic("/cache/sys/synthetic",marker,sizeof marker-1)==0,"seed cache");
    CHECK(ink_fs_write_atomic("/cache/sys/media-pos",marker,sizeof marker-1)==0,"seed actual media position cache");
    CHECK(ink_fs_write_atomic("/family/SYNTH_CONTROL.txt",marker,sizeof marker-1)==0,"seed shared control");
    CHECK(WAIT_FOR(!ink_storage_pending(),4000),"raw files durable");
    CHECK(ink_member_switch("owner")==0,"owner manages family");delay_ms(300);
    CHECK(confirmed("member.remove","{\"id\":\"oldadult\"}"),"remove old adult");
    CHECK(confirmed("member.add","{\"id\":\"oldadult\",\"role\":\"child\"}"),"reuse as child");
    inspect(); ink_db_flush_all();
    CHECK(WAIT_FOR(ink_db_idle()&&!ink_storage_pending(),4000),"durable checkpoint");
    ink_probe_count("test.finished");hal_power_reboot();
}

void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg={.name="review3-probe",.fn=run,.stack_bytes=16384,
        .priority=OSAL_PRIO_NORMAL,.core=-1,.flags=OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg,NULL)==OSAL_OK,"test task");
}
