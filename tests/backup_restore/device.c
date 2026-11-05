/* Real services, synthetic member records/files, and actual process restarts. */
#include "../services_finish/product.h"
#include "ink/hal_power.h"
#include "ink/sys/backup.h"
#include "ink/sys/crc.h"
#include "ink/sys/layout.h"
#include "ink/sys/sha256.h"

void sim_backup_faults(unsigned faults);
static const char *phase;
static const ink_db_def_t MISSING = {.name="test.missing",.title="测试未安装的集合",.scope=INK_DB_MEMBER,.schema=7};
static const ink_db_def_t NOTES = {.name="test.backup", .title="测试备份资料", .scope=INK_DB_MEMBER, .schema=3,
    .json=true, .ai_read=true, .ai_write=true, .max_records=32};
static unsigned fault_flags;
static void fault_event(void *arg) { (void)arg; sim_backup_faults(fault_flags); }
static void outage(void *arg) { (void)arg; sim_net_outage(SIM_MS(1500)); }
static void cut(void *arg) { (void)arg; sim_storage_power_cut(); }
static void arm_cut(void *arg) { sim_storage_arm_power_cut((int)(intptr_t)arg); }
static void set_fault(unsigned value) { fault_flags=value; vtk_event_at(vtk_now()+SIM_MS(1), fault_event, NULL); delay_ms(5); }
static bool status(const char *wanted) {
    return AI("backup.status", NULL) && g_rc.status == INK_RC_DONE && strstr(g_result, wanted);
}
static bool wait_phase(const char *wanted, int seconds) {
    for (int i=0; i<seconds*5; i++) {
        ink_power_activity(osal_now_us());
        if (status(wanted)) return true;
        if (strstr(g_result, "\"phase\":\"failed\"")) return false;
        delay_ms(200);
    }
    return status(wanted);
}
static void checkpoint(void) {
    CHECK(WAIT_FOR(ink_storage_pending()==0, 5000), "journal is durable before checkpoint");
    ink_probe_count("test.checkpoint"); vtk_event_at(vtk_now()+SIM_MS(1), cut, NULL); delay_ms(100);
}
static void save_record(const char *value) {
    ink_db_t *db=ink_db_open(&NOTES, NULL); CHECK(db, "open member fixture");
    ink_db_write_t w={.id=41, .sort=1234, .data=value, .len=strlen(value), .schema=3, .src=INK_SRC_TOUCH};
    CHECK(ink_db_put(db, &w, NULL)==0 && ink_db_wait(db, ink_db_last_ver(db), 5000000)==0, "fixture durable");
    ink_db_close(db);
}
static void fixtures(const char *who) {
    CHECK(ink_member_switch(who)==0, "select synthetic member %s", who);
    save_record(!strcmp(who,"owner") ? "{\"text\":\"测试大人资料\"}" : "{\"text\":\"测试孩子资料\"}");
    ink_db_t *db=ink_db_open(&NOTES, NULL);
    ink_db_write_t w={.id=70, .data="{}", .len=2, .src=INK_SRC_TOUCH};
    CHECK(ink_db_put(db,&w,NULL)==0 && ink_db_del(db,70,INK_SRC_TOUCH)==0 && ink_db_wait(db,3,5000000)==0, "durable deletion and next id");
    ink_db_close(db);
    char path[INK_LAYOUT_PATH_MAX]; snprintf(path,sizeof path,"/user/m/%s/test.position",who);
    CHECK(ink_storage_write_member_atomic(path,"offset=1234",11,ink_member_generation(who))==0 && WAIT_FOR(ink_storage_pending()==0,5000),"raw member state durable");
    uint8_t *bytes=osal_malloc(98304,OSAL_MEM_EXTERNAL); CHECK(bytes,"file fixture buffer");
    if (bytes) {
        memset(bytes,!strcmp(who,"owner") ? 'A' : 'K',98304);
        CHECK(ink_fs_mkdir("/me/photos")==0 && ink_fs_write_atomic("/me/photos/test.bin",bytes,98304)==0,"personal file fixture");
        osal_free(bytes);
    }
    CHECK(ink_fs_write_atomic("/me/empty.txt","",0)==0,"empty personal file fixture");
    if (!strcmp(who,"owner")) {
        CHECK(BY_HAND("reader.open_book","{\"title\":\"测试长书\"}") && g_rc.status==INK_RC_DONE,"open real reader");
        CHECK(BY_HAND("reader.goto_page","{\"page\":4}") && g_rc.status==INK_RC_DONE,"real reader fourth page");
        CHECK(BY_HAND("system.home",NULL) && g_rc.status==INK_RC_DONE,"leave reader and save position");
        CHECK(WAIT_FOR(ink_storage_pending()==0,5000),"reader position durable");
    }
}
static void backup(void) {
    CHECK(AI("backup.now",NULL) && g_rc.status==INK_RC_QUEUED,"start member backup (%s %s)",g_rc.say,g_rc.detail);
    CHECK(wait_phase("\"phase\":\"saved\"",180),"backup received (%s)",g_result);
    CHECK(strstr(g_result,"\"received\":true") && strstr(g_result,"\"durable\":false"),"hub receipt is distinct from cloud durability");
    CHECK(AI("backup.manifest",NULL) && g_rc.status==INK_RC_DONE && strstr(g_result,"test.backup") && strstr(g_result,"test.position") && strstr(g_result,"photos/test.bin"),"manifest includes collection, state and files (%s)",g_result);
    CHECK(strstr(g_result,"not_restored") && !strstr(g_result,"member.table") && !strstr(g_result,"test-only"),"credential exclusions are explicit");
    CHECK(AI("data.status","{\"collection\":\"test.backup\",\"id\":41}") && strstr(g_result,"at_hub"),"uploaded collection has real at_hub receipt (%s)",g_result);
}
static void preview(char token[33]) {
    CHECK(AI("restore.preview",NULL) && (g_rc.status==INK_RC_QUEUED || g_rc.status==INK_RC_DONE),"request restore preview");
    CHECK(wait_phase("\"phase\":\"preview\"",60),"preview ready (%s)",g_result);
    ink_json_t *j=osal_malloc(sizeof *j,OSAL_MEM_EXTERNAL);
    CHECK(j && ink_json_parse(j,g_result,strlen(g_result))==0 && ink_json_str(j,ink_json_find(j,0,"preview"),token,33)==32,"preview has an opaque bound token");
    osal_free(j);
    CHECK(AI("backup.manifest",NULL) && strstr(g_result,"not_restored") && strstr(g_result,"test.missing") && strstr(g_result,"collection_unavailable"),"preview enumerates omissions, including a collection not installed here");
}
static unsigned request_apply(const char *token) {
    char args[96]; snprintf(args,sizeof args,"{\"preview\":\"%s\"}",token);
    CHECK(AI("restore.apply",args) && g_rc.status==INK_RC_PENDING_CONFIRMATION,"overwrite always asks a person");
    unsigned confirm=0; CHECK(sscanf(g_rc.object,"confirm:%u",&confirm)==1,"confirmation has a token");
    CHECK(ink_confirm_decide(confirm,true,INK_SRC_AI)!=0,"AI cannot authorize overwrite");
    return confirm;
}
static void start_restore(void) {
    char token[33]=""; preview(token);
    unsigned confirm=request_apply(token);
    CHECK(ink_confirm_decide(confirm,true,INK_SRC_TOUCH)==0,"person approves the bound preview");
    CHECK(wait_phase("\"phase\":\"restoring\"",10),"restore intent durable (%s)",g_result);
}
static void verify(const char *who) {
    CHECK(ink_member_switch(who)==0,"verify member %s",who);
    ink_db_t *db=ink_db_open(&NOTES,NULL); ink_rec_t rec; char value[128]="";
    CHECK(db && ink_db_get(db,41,&rec,value,sizeof value)==0 && rec.id==41 && rec.ver==1 && rec.schema==3 && rec.sort==1234 &&
          strstr(value,!strcmp(who,"owner") ? "测试大人资料" : "测试孩子资料"),"original ids, versions, schema, sort and own content (%s)",value);
    CHECK(db && ink_db_count(db)==1 && ink_db_get(db,70,NULL,NULL,0)==INK_DB_ERR_NOT_FOUND && ink_db_last_ver(db)==3 && ink_db_new_id(db)>70,"deletion and next id survive restore");
    ink_db_close(db);
    char path[INK_LAYOUT_PATH_MAX]; snprintf(path,sizeof path,"/user/m/%s/test.position",who); memset(value,0,sizeof value);
    CHECK(ink_storage_read(path,value,sizeof value)==11 && !strcmp(value,"offset=1234"),"member page state restored");
    ink_fs_file_t *f=NULL;
    CHECK(ink_fs_open("/me/photos/test.bin",INK_FS_READ,&f)==0,"restored personal file opens");
    uint8_t bytes[128];
    if (f) {
        bool correct=true; int64_t n;
        size_t total=0;
        while ((n=ink_fs_read(f,bytes,sizeof bytes))>0) { total+=(size_t)n; for(int i=0;i<n;i++) if(bytes[i]!=(!strcmp(who,"owner")?'A':'K'))correct=false; }
        CHECK(n==0 && total==98304 && correct,"whole file restored, no mixed members or blocks");
        CHECK(ink_fs_close(f)==0,"restored file closed");
    }
    ink_fs_stat_t st; CHECK(ink_fs_stat("/me/empty.txt",&st)==0 && st.size==0,"empty file restored");
}

static volatile int api_done, storage_done, pin_done;
static int pin_result;
static void checked_pin(void *ctx, int rc, uint32_t wait_s) { (void)ctx; (void)wait_s; pin_result=rc; pin_done=1; }
static int storage_result;
static ink_exec_job_t api_job;
static void restore_done(void *ctx, int rc) { (void)ctx; storage_result=rc; storage_done=1; }
static void api_checks(void *arg) {
    (void)arg;
    ink_db_t *db=ink_db_open(&NOTES,NULL);
    ink_db_write_t w={.id=41,.data="{\"v\":1}",.len=7,.sort=42,.src=INK_SRC_TOUCH};
    CHECK(db && ink_db_put(db,&w,NULL)==0 && ink_db_wait(db,1,5000000)==0,"snapshot API initial durable record");
    void *snapshot=NULL; size_t len=0;
    CHECK(ink_db_export(db,&snapshot,&len)==0 && snapshot && len>32,"consistent durable snapshot");
    w.data="{\"v\":2}"; w.flags=INK_DB_LAZY;
    CHECK(ink_db_put(db,&w,NULL)==0,"accept lazy newer edit");
    void *unsaved=NULL; size_t unsaved_len=0;
    CHECK(ink_db_export(db,&unsaved,&unsaved_len)!=0 && !unsaved,"unsaved changes cannot enter a backup");
    CHECK(ink_db_wait(db,2,5000000)==0,"new edit becomes durable");
    if (snapshot) {
        ((uint8_t *)snapshot)[len-1]^=1;
        CHECK(ink_db_import(db,snapshot,len,2)!=0,"bad CRC refused before any installation");
        ((uint8_t *)snapshot)[len-1]^=1;
        CHECK(ink_db_import(db,snapshot,len,1)!=0,"stale expected revision refuses replacement");
        char value[32]="";
        CHECK(ink_db_get(db,41,NULL,value,sizeof value)==0 && !strcmp(value,"{\"v\":2}"),"rejected imports retain original");
        CHECK(ink_db_import(db,snapshot,len,2)==0,"valid atomic snapshot installation");
        ink_rec_t rec;
        CHECK(ink_db_get(db,41,&rec,value,sizeof value)==0 && rec.ver==1 && rec.id==41 && rec.sort==42 && !strcmp(value,"{\"v\":1}"),"installed record preserves identity and metadata");
    }
    osal_free(snapshot); ink_db_close(db);
    const char *path="/user/m/owner/test.cas";
    CHECK(ink_storage_write_atomic(path,"A",1)==0 && WAIT_FOR(ink_storage_pending()==0,5000),"conditional storage fixture");
    uint8_t hash[32]; ink_sha256("A",1,hash);
    ink_storage_hold_writes(true);
    CHECK(ink_storage_write_atomic(path,"B",1)==0,"newer user write queued before restore");
    storage_done=0;
    CHECK(ink_storage_restore(path,"old",3,hash,restore_done,NULL)==0,"conditional restore queued");
    char buf[16]={0};
    CHECK(ink_storage_read(path,buf,sizeof buf)==1 && buf[0]=='B',"unverified conditional content is never exposed to readers");
    ink_storage_hold_writes(false);
    CHECK(WAIT_FOR(storage_done,5000) && storage_result==-3,"storage queue checks hash after earlier writes");
    CHECK(ink_storage_read(path,buf,sizeof buf)==1 && buf[0]=='B',"failed conditional write preserves newer content");
    ink_sha256("B",1,hash); storage_done=0;
    CHECK(ink_storage_restore(path,"A",1,hash,restore_done,NULL)==0 && WAIT_FOR(storage_done,5000) && storage_result==0,"unchanged storage restores atomically");
    CHECK(ink_storage_read(path,buf,sizeof buf)==1 && buf[0]=='A',"storage read cache reflects successful restore");
    CHECK(ink_fs_mkdir("/me/cas")==0 && ink_fs_write_atomic("/me/cas/guard.txt","B",1)==0,"conditional card fixture");
    CHECK(hal_fs_mkdir("/content/.m/owner/.backup")==0,"private staging fixture");
    hal_file_t *f=hal_fs_open("/content/.m/owner/.backup/down127",HAL_O_WRITE|HAL_O_CREATE|HAL_O_TRUNC);
    CHECK(f && hal_fs_write(f,"A",1)==1,"stage replacement"); if(f)CHECK(hal_fs_close(f)==0,"stage closed");
    ink_sha256("A",1,hash);
    CHECK(ink_fs_restore("owner",ink_member_generation("owner"),127,"/me/cas/guard.txt",hash)==INK_FS_ERR_EXISTS,"card edit lock refuses changed target");
    ink_sha256("B",1,hash);
    CHECK(ink_fs_restore("owner",ink_member_generation("owner")+1,127,"/me/cas/guard.txt",hash)==INK_FS_ERR_DENIED,"card restore rejects a different member incarnation");
    CHECK(ink_fs_restore("owner",ink_member_generation("owner"),127,"/me/cas/guard.txt",hash)==0,"unchanged card target restores");
    ink_fs_file_t *read=NULL;
    CHECK(ink_fs_open("/me/cas/guard.txt",INK_FS_READ,&read)==0 && ink_fs_read(read,buf,sizeof buf)==1 && buf[0]=='A',"card replacement content");
    if(read)CHECK(ink_fs_close(read)==0,"conditional card read closed");
    uint32_t generation=ink_member_generation("owner");
    CHECK(ink_member_switch("kid")==0,"another member arrives before a delayed directory creation");
    CHECK(ink_fs_mkdir_member_generation("owner",generation,"/me/restore-parent")==0,"delayed directory keeps original member");
    ink_fs_stat_t st;
    CHECK(ink_fs_stat("/me/restore-parent",&st)==INK_FS_ERR_NOT_FOUND,"new member receives no directory from the old restore");
    CHECK(ink_member_switch("owner")==0 && ink_fs_stat("/me/restore-parent",&st)==0 && st.is_dir,"directory exists only in original member namespace");
    api_done=1;
}

static void run(void *arg) {
    (void)arg;
    if (!strcmp(phase,"api")) {
        CHECK(ink_exec_post(INK_EXEC_BG,&api_job,api_checks,NULL)==0 && WAIT_FOR(api_done,30000),"snapshot and conditional restore API checks finish");
    } else if (!strcmp(phase,"api-reboot")) {
        ink_db_t *db=ink_db_open(&NOTES,NULL); char value[32]=""; ink_rec_t rec;
        CHECK(db && ink_db_get(db,41,&rec,value,sizeof value)==0 && rec.ver==1 && !strcmp(value,"{\"v\":1}"),"old higher-version logs cannot resurrect after restart"); ink_db_close(db);
    } else if (!strcmp(phase,"receipt-reboot")) {
        CHECK(status("\"phase\":\"saved\""),"upload receipt survives reboot");
        CHECK(AI("data.status","{\"collection\":\"test.backup\",\"id\":41}") && strstr(g_result,"at_hub"),"record receipt survives reboot");
    } else if (!strcmp(phase,"malicious")) {
        CHECK(AI("restore.preview",NULL) && g_rc.status==INK_RC_QUEUED,"fetch malicious manifest fixture");
        CHECK(wait_phase("\"phase\":\"failed\"",60),"foreign member or unsafe path rejected before preview");
        CHECK(ink_storage_size("/user/m/owner/test.position")<0,"malicious manifest cannot install any local state");
    } else if (!strcmp(phase,"seed")) {
        set_fault(4); fixtures("owner"); backup(); fixtures("kid"); backup();
        CHECK(ink_member_switch("owner")==0,"return adult");
    } else if (!strcmp(phase,"wipe")) {
        CHECK(BY_HAND("system.factory_reset",NULL) && g_rc.status==INK_RC_PENDING_CONFIRMATION,"clear device requires confirmation");
        unsigned token=0; sscanf(g_rc.object,"confirm:%u",&token);
        CHECK(ink_confirm_decide(token,true,INK_SRC_TOUCH)==0,"person clears synthetic device");
        return;
    } else if (!strcmp(phase,"restore-cut")) {
        CHECK(ink_storage_size("/user/m/owner/test.position")<0 && hal_fs_size("/content/.m/owner/photos/test.bin")<0,"device actually cleared");
        start_restore();
        CHECK(WAIT_FOR(sim_metric_value("backup.mock_downloaded_bytes")>=16384,90000),"download reaches durable block before power cut"); checkpoint(); return;
    } else if (!strcmp(phase,"restore-resume")) {
        CHECK(wait_phase("\"phase\":\"done\"",180),"restore resumes after power cut (%s)",g_result);
        verify("owner");
        CHECK(ink_member_switch("kid")==0 && status("\"phase\":\"none\""),"child cannot see adult restore journal");
        ink_db_t *db=ink_db_open(&NOTES,NULL); CHECK(db && ink_db_count(db)==0,"adult restore does not install child data"); ink_db_close(db);
        start_restore(); CHECK(wait_phase("\"phase\":\"done\"",180),"child's own restore completes"); verify("kid");
    } else if (!strcmp(phase,"verify-reboot")) {
        verify("owner");
        CHECK(BY_HAND("reader.open_book","{\"title\":\"测试长书\"}") && g_rc.status==INK_RC_DONE,"restored book can open after restart");
        CHECK(AI("reader.read_page",NULL) && g_rc.revision==4 && strstr(g_rc.object,"#page=4"),"actual reader returns to fourth page (%s)",g_rc.object);
        CHECK(BY_HAND("system.home",NULL),"leave restored page"); verify("kid");
        CHECK(sim_metric_value("hub.requests.message/send")==0 && sim_metric_value("hub.requests.confirm/request")==0,"restore never replays external actions");
    } else if (!strcmp(phase,"upload-cut") || !strcmp(phase,"network")) {
        set_fault(5); fixtures("owner");
        CHECK(AI("backup.now",NULL) && g_rc.status==INK_RC_QUEUED,"start interrupted backup");
        CHECK(WAIT_FOR(sim_metric_value("backup.mock_lost_ack")>0,20000),"hub persisted a block whose ACK was lost");
        if (!strcmp(phase,"upload-cut")) { checkpoint(); return; }
        vtk_event_at(vtk_now()+SIM_MS(1),outage,NULL);
        CHECK(wait_phase("\"phase\":\"saved\"",180),"backup continues after lost ACK and disconnect (%s)",g_result);
        CHECK(sim_metric_value("backup.mock_resumed_upload")>0,"reconnect uses hub's persisted offset");
    } else if (!strcmp(phase,"upload-resume")) {
        CHECK(wait_phase("\"phase\":\"saved\"",180),"backup resumes after real process restart (%s)",g_result);
        CHECK(sim_metric_value("backup.mock_resumed_upload")>0,"no lost-ACK duplicate bytes on restart");
    } else if (!strcmp(phase,"restore-network")) {
        start_restore();
        CHECK(WAIT_FOR(sim_metric_value("backup.mock_downloaded_bytes")>=16384,90000),"restore reached a block before network loss");
        vtk_event_at(vtk_now()+SIM_MS(1),outage,NULL);
        CHECK(wait_phase("\"phase\":\"done\"",180),"restore continues after disconnect");
        CHECK(sim_metric_value("backup.mock_resumed_download")>0,"restore reconnect keeps downloaded blocks"); verify("owner");
    } else if (!strcmp(phase,"member-resume")) {
        set_fault(5); fixtures("owner");
        CHECK(AI("backup.now",NULL) && g_rc.status==INK_RC_QUEUED,"begin backup before member switch");
        CHECK(WAIT_FOR(sim_metric_value("backup.mock_lost_ack")>0,20000),"old member upload is in progress");
        CHECK(ink_member_switch("kid")==0 && status("\"phase\":\"none\""),"new member has independent progress");
        CHECK(ink_member_switch("owner")==0,"return original member");
        CHECK(wait_phase("\"phase\":\"saved\"",180),"returning member automatically resumes durable work");
    } else if (!strcmp(phase,"pin")) {
        pin_done=0;
        CHECK(ink_member_set_pin("owner","","1111",checked_pin,NULL)==0 && WAIT_FOR(pin_done,5000) && pin_result==INK_PIN_OK,"set synthetic local PIN");
        char token[33]=""; preview(token); unsigned confirm=request_apply(token);
        CHECK(ink_confirm_decide(confirm,true,INK_SRC_TOUCH)!=0,"ordinary tap cannot bypass required parent PIN");
        pin_done=0;
        CHECK(ink_confirm_accept_with_pin(confirm,"2222",checked_pin,NULL)==0 && WAIT_FOR(pin_done,5000) && pin_result==INK_PIN_WRONG,"wrong PIN cannot authorize restore");
        CHECK(status("\"phase\":\"preview\""),"wrong PIN leaves original state");
        pin_done=0;
        CHECK(ink_confirm_accept_with_pin(confirm,"1111",checked_pin,NULL)==0 && WAIT_FOR(pin_done,5000) && pin_result==INK_PIN_OK,"person's correct PIN authorizes restore");
        CHECK(wait_phase("\"phase\":\"done\"",180),"PIN-authorized restore completes");
    } else if (!strcmp(phase,"stale")) {
        char token[33]=""; preview(token); unsigned confirm=request_apply(token);
        save_record("{\"text\":\"预览后新改动\"}");
        CHECK(ink_confirm_decide(confirm,true,INK_SRC_TOUCH)==0,"confirm formerly valid preview");
        CHECK(wait_phase("\"phase\":\"failed\"",10),"changed local content invalidates preview (%s)",g_result);
        ink_db_t *db=ink_db_open(&NOTES,NULL); char value[128]="";
        CHECK(db && ink_db_get(db,41,NULL,value,sizeof value)==0 && strstr(value,"预览后新改动"),"stale confirmation never overwrites a newer edit"); ink_db_close(db);
    } else if (!strcmp(phase,"member-confirm")) {
        char token[33]=""; preview(token); unsigned confirm=request_apply(token);
        CHECK(ink_member_switch("kid")==0,"switch while confirmation is pending");
        CHECK(ink_confirm_decide(confirm,true,INK_SRC_TOUCH)!=0,"another member cannot accept old overwrite");
        CHECK(status("\"phase\":\"none\""),"child sees only own journal");
        CHECK(AI("restore.apply","{\"preview\":\"00000000000000000000000000000000\"}") && g_rc.status==INK_RC_PENDING_CONFIRMATION,"invalid token still passes through human gate first");
        unsigned child=0; sscanf(g_rc.object,"confirm:%u",&child); CHECK(ink_confirm_decide(child,false,INK_SRC_TOUCH)==0,"decline child invalid request");
    } else if (!strcmp(phase,"corrupt")) {
        set_fault(2); CHECK(AI("restore.preview",NULL) && g_rc.status==INK_RC_QUEUED,"fetch corrupt manifest fixture");
        CHECK(wait_phase("\"phase\":\"failed\"",60),"corrupt manifest rejected before overwrite");
        ink_db_t *db=ink_db_open(&NOTES,NULL); CHECK(db && ink_db_count(db)==0,"corrupt restore leaves local collection untouched"); ink_db_close(db);
    } else if (!strcmp(phase,"flash-cut")) {
        char token[33]=""; preview(token); unsigned confirm=request_apply(token);
        const char *n=getenv("INK_BACKUP_CUT"); int nth=n?atoi(n):1;
        vtk_event_at(vtk_now()+SIM_MS(1),arm_cut,(void *)(intptr_t)nth); delay_ms(2);
        CHECK(ink_confirm_decide(confirm,true,INK_SRC_TOUCH)==0,"authorize fault-injected restore");
        wait_phase("\"phase\":\"done\"",180); return;
    } else if (!strcmp(phase,"flash-verify")) {
        if (status("\"phase\":\"preview\"")) { char token[33]=""; preview(token); unsigned confirm=request_apply(token); CHECK(ink_confirm_decide(confirm,true,INK_SRC_TOUCH)==0,"retry durable preview after interrupted intent commit"); }
        CHECK(wait_phase("\"phase\":\"done\"",180),"torn-flash restore finishes (%s)",g_result); verify("owner");
    }
    ink_probe_count("test.finished");
    hal_power_reboot();
}
void ink_main(void) {
    phase=getenv("INK_BACKUP_PHASE"); if(!phase)phase="seed";
    product_main();
    CHECK(ink_db_register(&NOTES)==0,"register synthetic member collection");
    if (!strcmp(phase,"seed")) CHECK(ink_db_register(&MISSING)==0,"register a collection only present at backup time");
    osal_task_cfg_t cfg={.name="backup-test",.fn=run,.stack_bytes=16384,.priority=OSAL_PRIO_NORMAL,.core=-1,.flags=OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg,NULL)==OSAL_OK,"backup test task");
}
