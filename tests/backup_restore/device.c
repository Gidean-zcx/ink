/* Real services, synthetic member records/files, and actual process restarts. */
#include "../services_finish/product.h"
#include "ink/hal_power.h"
#include "ink/sys/backup.h"
#include "ink/sys/backup_format.h"
#include "ink/sys/crc.h"
#include "ink/sys/layout.h"
#include "ink/sys/sha256.h"

void sim_backup_faults(unsigned faults);
static const char *phase;
static size_t file_bytes = 98304;
static const char *file_path = "/me/photos/test.bin";
static const char *file_dir = "/me/photos";
static const char *file_name = "photos/test.bin";
static volatile int pin_done;
static int pin_result;
static void checked_pin(void *ctx, int rc, uint32_t wait_s) { (void)ctx; (void)wait_s; pin_result=rc; pin_done=1; }
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
    CHECK(ink_storage_write_member_atomic(path,"offset=1234",11)==0 && WAIT_FOR(ink_storage_pending()==0,5000),"raw member state durable");
    uint8_t *bytes=osal_malloc(file_bytes,OSAL_MEM_EXTERNAL); CHECK(bytes,"file fixture buffer");
    if (bytes) {
        memset(bytes,!strcmp(who,"owner") ? 'A' : 'K',file_bytes);
        CHECK(ink_fs_mkdir(file_dir)==0 && ink_fs_write_atomic(file_path,bytes,file_bytes)==0,"personal file fixture");
        osal_free(bytes);
    }
    CHECK(ink_fs_write_atomic("/me/empty.txt","",0)==0,"empty personal file fixture");
    if (!strcmp(who,"owner")) {
        CHECK(BY_HAND("reader.open_book","{\"title\":\"测试长书\"}") && g_rc.status==INK_RC_DONE,"open real reader");
        /* reader-one-1: pages count within a chapter; the place is the fifth chapter's second page, with a bookmark there */
        CHECK(BY_HAND("reader.goto_chapter","{\"chapter\":5}") && g_rc.status==INK_RC_DONE,"real reader fifth chapter");
        CHECK(BY_HAND("reader.goto_page","{\"page\":2}") && g_rc.status==INK_RC_DONE,"its second page");
        CHECK(BY_HAND("reader.add_bookmark",NULL) && g_rc.status==INK_RC_DONE,"a bookmark there");
        CHECK(BY_HAND("system.home",NULL) && g_rc.status==INK_RC_DONE,"leave reader and save position");
        CHECK(WAIT_FOR(ink_storage_pending()==0,5000),"reader position durable");
    }
}
static void backup(void) {
    CHECK(AI("backup.now",NULL) && g_rc.status==INK_RC_QUEUED,"start member backup (status %d: %s %s)",(int)g_rc.status,g_rc.say,g_rc.detail);
    CHECK(wait_phase("\"phase\":\"saved\"",180),"backup received (%s)",g_result);
    CHECK(strstr(g_result,"\"received\":true") && strstr(g_result,"\"durable\":false"),"hub receipt is distinct from cloud durability");
    CHECK(AI("backup.manifest",NULL) && g_rc.status==INK_RC_DONE && strstr(g_result,"test.backup") && strstr(g_result,"test.position") && strstr(g_result,file_name),"manifest includes collection, state and files (%s)",g_result);
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
    CHECK(ink_fs_open(file_path,INK_FS_READ,&f)==0,"restored personal file opens");
    uint8_t *bytes=osal_malloc(4096,OSAL_MEM_EXTERNAL); CHECK(bytes,"whole-file verification buffer");
    if (f && bytes) {
        bool correct=true; int64_t n;
        size_t total=0;
        while ((n=ink_fs_read(f,bytes,4096))>0) { total+=(size_t)n; for(int i=0;i<n;i++) if(bytes[i]!=(!strcmp(who,"owner")?'A':'K'))correct=false; }
        CHECK(n==0 && total==file_bytes && correct,"whole file restored, no mixed members or blocks");
    }
    if(f) CHECK(ink_fs_close(f)==0,"restored file closed");
    osal_free(bytes);
    ink_fs_stat_t st; CHECK(ink_fs_stat("/me/empty.txt",&st)==0 && st.size==0,"empty file restored");
}

static volatile int api_done, storage_done;
static int storage_result;
static ink_exec_job_t api_job;
static void restore_done(void *ctx, int rc) { (void)ctx; storage_result=rc; storage_done=1; }
static volatile int edit_done, usb_done;
static int usb_result;
static void api_writer(void *arg) {
    (void)arg;
    size_t len=2u*1024u*1024u;
    uint8_t *bytes=osal_malloc(len,OSAL_MEM_EXTERNAL); CHECK(bytes,"API concurrent writer buffer");
    if(bytes) {
        memset(bytes,'C',len);
        CHECK(ink_fs_write_atomic("/me/cas/guard.txt",bytes,len)==0,"API concurrent atomic write commits");
        osal_free(bytes);
    }
    edit_done=1;
}
static void api_lender(void *arg) { (void)arg; usb_result=ink_usb_disk_start(); usb_done=1; }
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
    CHECK(ink_fs_restore("owner",127,"/me/cas/guard.txt",hash)==INK_FS_ERR_EXISTS,"card edit lock refuses changed target");
    ink_sha256("B",1,hash);
    CHECK(ink_fs_restore("owner",127,"/me/cas/guard.txt",hash)==0,"unchanged card target restores");
    ink_fs_file_t *read=NULL;
    CHECK(ink_fs_open("/me/cas/guard.txt",INK_FS_READ,&read)==0 && ink_fs_read(read,buf,sizeof buf)==1 && buf[0]=='A',"card replacement content");
    if(read)CHECK(ink_fs_close(read)==0,"conditional card read closed");
    f=hal_fs_open("/content/.m/owner/.backup/down127",HAL_O_WRITE|HAL_O_CREATE|HAL_O_TRUNC);
    CHECK(f && hal_fs_write(f,"R",1)==1,"stage revision-guard replacement"); if(f)CHECK(hal_fs_close(f)==0,"revision stage closed");
    uint32_t revision=ink_fs_revision();
    CHECK(ink_fs_write_atomic("/me/cas/guard.txt","C",1)==0,"same-size edit after incremental hash checkpoint");
    CHECK(ink_fs_restore_if_revision("owner",127,"/me/cas/guard.txt",revision)==INK_FS_ERR_EXISTS,"revision restore refuses intervening same-size edit");
    CHECK(ink_fs_open("/me/cas/guard.txt",INK_FS_READ,&read)==0 && ink_fs_read(read,buf,sizeof buf)==1 && buf[0]=='C',"revision guard retains new content");
    if(read)CHECK(ink_fs_close(read)==0,"revision-guard read closed");
    revision=ink_fs_revision();
    CHECK(ink_fs_restore_if_revision("owner",127,"/me/cas/guard.txt",revision)==0,"unchanged revision installs without whole-file lock-time hashing");
    ink_fs_file_t *writer=NULL;
    CHECK(ink_fs_open("/me/cas/guard.txt",INK_FS_WRITE,&writer)==0 && ink_fs_revision()==0,"open writer cannot yield a stable restore revision");
    if(writer) CHECK(ink_fs_close(writer)==0 && ink_fs_revision()!=0,"closing writer permits stable sampling again");
    f=hal_fs_open("/content/.m/owner/.backup/down127",HAL_O_WRITE|HAL_O_CREATE|HAL_O_TRUNC);
    CHECK(f && hal_fs_write(f,"R",1)==1,"stage concurrent lock fixture"); if(f) CHECK(hal_fs_close(f)==0,"lock fixture closed");
    revision=ink_fs_revision();
    osal_task_cfg_t edit_cfg={.name="api-writer",.fn=api_writer,.stack_bytes=4096,.priority=OSAL_PRIO_NORMAL,.core=-1,.flags=OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&edit_cfg,NULL)==OSAL_OK && WAIT_FOR(ink_fs_revision()==0 && !edit_done,1000),"atomic writer owns edit lock before conditional install");
    uint64_t started=osal_now_us();
    int restored=ink_fs_restore_if_revision("owner",127,"/me/cas/guard.txt",revision);
    CHECK(restored==INK_FS_ERR_BUSY && osal_now_us()-started<200000,"conditional restore yields while atomic writer owns edit lock");
    CHECK(WAIT_FOR(edit_done,5000),"concurrent API writer finished");
    hw(HW_USB_ON,0,0);
    CHECK(ink_fs_card_hold()==0,"hold card while USB loan waits");
    revision=ink_fs_revision();
    osal_task_cfg_t usb_cfg={.name="api-usb",.fn=api_lender,.stack_bytes=4096,.priority=OSAL_PRIO_NORMAL,.core=-1,.flags=OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&usb_cfg,NULL)==OSAL_OK && WAIT_FOR(ink_fs_revision()!=revision,1000),"USB borrowing intent changes revision before grant");
    uint32_t waiting_revision=ink_fs_revision();
    CHECK(waiting_revision && !ink_usb_disk_active(),"sample stable revision while card holder postpones USB grant");
    ink_fs_card_release();
    CHECK(WAIT_FOR(usb_done,3000) && usb_result==0 && ink_usb_disk_active() && ink_fs_revision()==0,"lent card refuses stable sampling");
    CHECK(ink_usb_disk_stop()==0 && ink_fs_revision()!=waiting_revision,"USB return invalidates revision sampled before loan grant");
    hw(HW_USB_OFF,0,0);
    CHECK(ink_member_switch("kid")==0,"another member arrives before a delayed directory creation");
    CHECK(ink_fs_mkdir_for("owner","/me/restore-parent")==0,"delayed directory keeps original member");
    ink_fs_stat_t st;
    CHECK(ink_fs_stat("/me/restore-parent",&st)==INK_FS_ERR_NOT_FOUND,"new member receives no directory from the old restore");
    CHECK(ink_member_switch("owner")==0 && ink_fs_stat("/me/restore-parent",&st)==0 && st.is_dir,"directory exists only in original member namespace");
    api_done=1;
}

/* Synthetic authenticated reprovisioning fixture, separate from the member backup under test. */
static const char *family_fixture = "/content/test-backup-family";
static void family_state(void *arg) {
    bool load = (bool)(intptr_t)arg;
    ink_db_t *db = ink_db_open(ink_db_find_def("member.table"), NULL);
    void *data = NULL; size_t len = 0;
    if (!load) {
        /* Export takes only durable, idle state: wait for the member writes before it, retry past a compaction. */
        int exported = -1;
        for (int i = 0; db && exported && i < 50; i++) {
            ink_db_wait(db, ink_db_last_ver(db), 1000000);
            if ((exported = ink_db_export(db, &data, &len)) != 0) osal_task_delay_us(100000);
        }
        CHECK(db && exported==0,"export synthetic family binding independently of backup");
        hal_file_t *f = hal_fs_open(family_fixture, HAL_O_WRITE|HAL_O_CREATE|HAL_O_TRUNC);
        CHECK(f && hal_fs_write(f,data,len)==(ssize_t)len && hal_fs_sync(f)==0,"save test-only reprovisioning fixture outside member backup");
        if(f) CHECK(hal_fs_close(f)==0,"family fixture closed");
    } else {
        int64_t n=hal_fs_size(family_fixture);
        CHECK(n>0 && n<8192,"synthetic binding fixture bounded");
        if(n>0 && n<8192) {
            len=(size_t)n; data=osal_malloc(len,OSAL_MEM_EXTERNAL);
            hal_file_t *f=hal_fs_open(family_fixture,HAL_O_READ);
            CHECK(data && f && hal_fs_read(f,data,len)==(ssize_t)len,"read separate synthetic family binding");
            if(f) CHECK(hal_fs_close(f)==0,"family fixture read closed");
            CHECK(db && data && ink_db_import(db,data,len,ink_db_last_ver(db))==0,"explicit test-only rebinding imports original durable identities");
        }
    }
    osal_free(data); ink_db_close(db); api_done=1;
}
static bool member_exists(const char *id) {
    for(int i=0;;i++) { const ink_member_t *m=ink_member_at(i); if(!m)return false; if(!strcmp(m->id,id))return true; }
}
static void human_member(const char *cap, const char *args, const char *id) {
    unsigned token=0;
    CHECK(BY_HAND(cap,args) && g_rc.status==INK_RC_PENDING_CONFIRMATION,"membership change requires a person");
    CHECK(sscanf(g_rc.object,"confirm:%u",&token)==1 && ink_confirm_decide(token,true,INK_SRC_TOUCH)==0,"human approves exact membership change");
    CHECK(WAIT_FOR(!strcmp(cap,"member.add") ? ink_member_exists(id)>0 : !member_exists(id),10000),"membership background work finished");
}
static void ignore_frame(void *ctx, const uint8_t *data, size_t len, uint32_t seq) { (void)ctx; (void)data; (void)len; (void)seq; }
static void responsive_wait(const char *wanted, int seconds) {
    for(int i=0;i<seconds*5;i++) {
        ink_power_activity(osal_now_us());
        CHECK(AI("backup.status",NULL) && g_rc.status==INK_RC_DONE,"progress query stays responsive during large-file work");
        if(strstr(g_result,wanted))return;
        if(strstr(g_result,"\"phase\":\"failed\""))break;
        pin_done=0;
        CHECK(ink_member_unlock("owner","",checked_pin,NULL)==0 && WAIT_FOR(pin_done,2000) && pin_result==INK_PIN_OK,"PIN executor work gets a turn during large-file work");
        delay_ms(200);
    }
    CHECK(false,"large-file work completes (%s)",g_result);
}

static char race_stage[160];
static size_t race_bytes = 8u * 1024u * 1024u;
static volatile int race_done;
static void race_writer(void *arg) {
    (void)arg;
    uint8_t *bytes=osal_malloc(race_bytes,OSAL_MEM_EXTERNAL);
    CHECK(bytes,"concurrent writer buffer");
    if(bytes) {
        memset(bytes,'C',race_bytes);
        CHECK(WAIT_FOR(hal_fs_size(race_stage)==(int64_t)file_bytes,90000),"restore body arrives before concurrent atomic write");
        ink_probe_count("test.atomic_started");
        CHECK(ink_fs_write_atomic(file_path,bytes,race_bytes)==0,"concurrent atomic write commits successfully");
        osal_free(bytes);
    }
    race_done=1;
}
static void atomic_race(void) {
    if(!strcmp(phase,"atomic-absent")) CHECK(ink_fs_remove(file_path)==0,"target absent before preview");
    else {
        uint8_t *bytes=osal_malloc(file_bytes,OSAL_MEM_EXTERNAL); CHECK(bytes,"local edit buffer");
        if(bytes) { memset(bytes,'B',file_bytes); CHECK(ink_fs_write_atomic(file_path,bytes,file_bytes)==0,"local edit before preview"); osal_free(bytes); }
    }
    char token[33]=""; preview(token);
    ink_backup_manifest_t *manifest=osal_calloc(1,sizeof *manifest,OSAL_MEM_EXTERNAL);
    hal_file_t *f=hal_fs_open("/content/.m/owner/.backup/preview0",HAL_O_READ);
    CHECK(manifest && f && hal_fs_read(f,manifest,sizeof *manifest)>=(ssize_t)offsetof(ink_backup_manifest_t,item),"read real preview item index");
    if(f) CHECK(hal_fs_close(f)==0,"preview index closed");
    unsigned index=128;
    if(manifest) {
        for(unsigned i=0;i<manifest->count && i<128;i++) if(manifest->item[i].kind==3 && !strcmp(manifest->item[i].name,file_name)) index=i;
        osal_free(manifest);
    }
    CHECK(index<128,"find personal file in approved preview");
    snprintf(race_stage,sizeof race_stage,"/content/.m/owner/.backup/down%u",index);
    osal_task_cfg_t cfg={.name="atomic-writer",.fn=race_writer,.stack_bytes=4096,.priority=OSAL_PRIO_NORMAL,.core=-1,.flags=OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg,NULL)==OSAL_OK,"start concurrent atomic writer");
    unsigned confirm=request_apply(token); CHECK(ink_confirm_decide(confirm,true,INK_SRC_TOUCH)==0,"person approves restore before newer write");
    bool terminal=false;
    for(int i=0;i<600;i++) {
        ink_power_activity(osal_now_us());
        if(status("\"phase\":\"failed\"") || strstr(g_result,"\"phase\":\"done\"")) { terminal=true; break; }
        delay_ms(200);
    }
    CHECK(terminal && WAIT_FOR(race_done,15000),"restore and concurrent writer finish");
    ink_fs_file_t *opened=NULL;
    CHECK(ink_fs_open(file_path,INK_FS_READ,&opened)==0,"read newer committed file");
    uint8_t *bytes=osal_malloc(16384,OSAL_MEM_EXTERNAL); CHECK(bytes,"verification buffer");
    size_t total=0; int64_t n=-1; bool correct=true;
    if(opened && bytes) while((n=ink_fs_read(opened,bytes,16384))>0) {
        total+=(size_t)n;
        for(int64_t i=0;i<n;i++) if(bytes[i]!='C') correct=false;
    }
    if(opened) CHECK(ink_fs_close(opened)==0,"newer file closed");
    osal_free(bytes);
    CHECK(n==0 && total==race_bytes && correct,"restore retains whole newer successful atomic write");
    CHECK(strstr(g_result,"\"phase\":\"failed\"") && sim_metric_value("backup.restored")==0,"concurrent edit rejects restore instead of reporting success");
}
static void run(void *arg) {
    (void)arg;
    if (!strcmp(phase,"atomic-race") || !strcmp(phase,"atomic-absent")) {
        atomic_race();
    } else if (!strcmp(phase,"rejoin")) {
        CHECK(AI("restore.preview",NULL) && g_rc.status==INK_RC_QUEUED && wait_phase("\"phase\":\"failed\"",60),"factory-reset identities cannot claim old backups by display id");
        api_done=0;
        CHECK(ink_exec_post(INK_EXEC_BG,&api_job,family_state,(void *)(intptr_t)1)==0 && WAIT_FOR(api_done,10000),"simulate independently authorized family reprovisioning");
    } else if (!strcmp(phase,"prepare-recover")) {
        CHECK(AI("restore.preview",NULL) && g_rc.status==INK_RC_QUEUED,"begin unapproved preview preparation");
        CHECK(wait_phase("\"phase\":\"preparing\"",10),"preview preparation has durable intent");
        CHECK(hal_fs_remove("/content/.m/owner/.backup/preview0")==0,"simulate missing preview checkpoint after interrupted card write");
        CHECK(wait_phase("\"phase\":\"preview\"",60),"unapproved partial preview rebuilds from identity-checked remote manifest");
        CHECK(ink_storage_size("/user/m/owner/test.position")<0,"preview recovery does not install member files");
    } else if (!strcmp(phase,"legacy-journal")) {
        const uint32_t legacy[2]={0x31534249u,1};
        CHECK(ink_storage_write_member_atomic("/user/m/owner/backup.journal",legacy,sizeof legacy)==0 && WAIT_FOR(ink_storage_pending()==0,5000),"synthetic unbound legacy journal persisted");
        CHECK(status("\"phase\":\"failed\"") && strstr(g_result,"请重新备份"),"legacy receipt and restore authority are discarded explicitly");
        CHECK(AI("backup.manifest",NULL) && g_rc.status==INK_RC_DONE && !strstr(g_result,"\"items\""),"legacy journal exposes no unbound manifest");
        CHECK(AI("backup.now",NULL) && g_rc.status==INK_RC_QUEUED && wait_phase("\"phase\":\"saved\"",180),"legacy journal does not block a fresh identity-bound backup");
    } else if (!strcmp(phase,"wrong-receipt")) {
        fixtures("owner"); set_fault(12);
        CHECK(AI("backup.now",NULL) && g_rc.status==INK_RC_QUEUED,"queue backup before wrong-identity reply");
        CHECK(wait_phase("\"phase\":\"failed\"",60),"wrong identity in transfer receipt rejected");
        CHECK(strstr(g_result,"\"received\":false"),"wrong-identity reply never grants a receipt");
    } else if (!strcmp(phase,"collect-cut") || !strcmp(phase,"collect-member") || !strcmp(phase,"collect-held")) {
        fixtures("owner"); set_fault(4);
        CHECK(AI("backup.now",NULL) && g_rc.status==INK_RC_QUEUED,"capture intent returns before large file is read");
        CHECK(WAIT_FOR(sim_metric_value("backup.collect_blocks")>0,15000),"capture has completed a bounded piece");
        if(!strcmp(phase,"collect-cut")) { checkpoint(); return; }
        if(!strcmp(phase,"collect-member")) {
            CHECK(ink_member_switch("kid")==0 && status("\"phase\":\"none\""),"another member cannot observe capture progress");
            CHECK(ink_member_switch("owner")==0,"return to original capture identity");
        } else {
            ink_jobs_hold(true);
            CHECK(status("\"phase\":\"collecting\""),"paused capture leaves status executor responsive");
            ink_jobs_hold(false);
        }
        responsive_wait("\"phase\":\"saved\"",300);
        CHECK(sim_metric_value("cap.timeouts")==0,"capture suspension does not cause capability timeouts");
    } else if (!strcmp(phase,"collect-resume")) {
        responsive_wait("\"phase\":\"saved\"",300);
        CHECK(AI("backup.manifest",NULL) && g_rc.status==INK_RC_DONE && strstr(g_result,file_name),"restart rebuilds capture and uploads complete manifest");
    } else if (!strcmp(phase,"slow-switch") || !strcmp(phase,"slow-pause")) {
        set_fault(4); fixtures("owner");
        CHECK(AI("backup.now",NULL) && g_rc.status==INK_RC_QUEUED,"queue backup before delayed ACK");
        CHECK(WAIT_FOR(sim_metric_value("backup.mock_uploaded_bytes")>0,30000),"a frame reaches hub before delayed ACK");
        if(!strcmp(phase,"slow-switch")) {
            CHECK(ink_member_switch("kid")==0 && status("\"phase\":\"none\""),"new member has no in-flight backup state");
            delay_ms(1000);
            CHECK(ink_member_switch("owner")==0,"original member returns after old ACK");
        } else {
            ink_jobs_hold(true);
            CHECK(status("\"phase\":\"uploading\""),"paused network leaves status responsive");
            delay_ms(1000);
            ink_jobs_hold(false);
        }
        responsive_wait("\"phase\":\"saved\"",300);
        CHECK(sim_metric_value("backup.mock_resumed_upload")>0 && sim_metric_value("cap.timeouts")==0 && sim_metric_value("cap.late")==0,"delayed ACK resumes from durable offset without blocking capabilities");
    } else if (!strcmp(phase,"slow-preview-switch")) {
        CHECK(AI("restore.preview",NULL) && g_rc.status==INK_RC_QUEUED,"queue preview before delayed response");
        CHECK(WAIT_FOR(sim_metric_value("hub.requests.backup/latest")>0,10000),"latest request is in flight");
        CHECK(ink_member_switch("kid")==0 && status("\"phase\":\"none\""),"other member cannot inherit pending latest response");
        delay_ms(1500);
        CHECK(ink_member_switch("owner")==0,"return after stale RPC response");
        responsive_wait("\"phase\":\"preview\"",180);
        CHECK(AI("backup.manifest",NULL) && g_rc.status==INK_RC_DONE && strstr(g_result,file_name),"fresh bound RPC builds original member preview");
        CHECK(sim_metric_value("cap.timeouts")==0 && sim_metric_value("cap.late")==0,"late RPC response leaves shared executor responsive");
    } else if (!strcmp(phase,"large-seed") || !strcmp(phase,"deep") || !strcmp(phase,"atomic-seed")) {
        set_fault(4); fixtures("owner");
        CHECK(AI("backup.now",NULL) && g_rc.status==INK_RC_QUEUED,"large or deep backup returns queued before collection");
        responsive_wait("\"phase\":\"saved\"",300);
        CHECK(AI("backup.manifest",NULL) && g_rc.status==INK_RC_DONE && strstr(g_result,file_name),"supported large or deep file is included");
        CHECK(sim_metric_value("cap.timeouts")==0 && sim_metric_value("cap.late")==0,"large/deep backup has no capability timeout or late completion");
    } else if (!strcmp(phase,"recording")) {
        fixtures("owner");
        CHECK(ink_audio_capture_begin_local(ignore_frame,NULL)==0,"a recording holds the flash back");
        delay_ms(500);
        CHECK(AI("backup.now",NULL) && g_rc.status==INK_RC_REJECTED && sim_metric_value("cap.timeouts")==0,"backup says to wait instead of stopping the executor");
        ink_audio_capture_end();
        delay_ms(1000);
        CHECK(AI("backup.now",NULL) && g_rc.status==INK_RC_QUEUED,"and takes it after the recording");
        responsive_wait("\"phase\":\"saved\"",60);
    } else if (!strcmp(phase,"slow-status")) {
        ink_hub_demand(); CHECK(WAIT_FOR(ink_hub_online(),10000),"connect before cloud receipt refresh");
        CHECK(status("\"phase\":\"saved\"") && strstr(g_result,"\"durable\":false"),"status returns cached receipt before cloud poll");
        responsive_wait("\"durable\":true",30);
        CHECK(sim_metric_value("cap.timeouts")==0 && sim_metric_value("cap.late")==0,"cloud receipt polling leaves shared executor responsive");
    } else if (!strcmp(phase,"large-restore")) {
        uint8_t *bytes=osal_malloc(file_bytes,OSAL_MEM_EXTERNAL); CHECK(bytes,"large overwrite fixture buffer");
        if(bytes) { memset(bytes,'B',file_bytes); CHECK(ink_fs_write_atomic(file_path,bytes,file_bytes)==0,"same-size local edit before preview"); osal_free(bytes); }
        CHECK(AI("restore.preview",NULL) && g_rc.status==INK_RC_QUEUED,"large preview returns promptly");
        responsive_wait("\"phase\":\"preview\"",180);
        ink_json_t *j=osal_malloc(sizeof *j,OSAL_MEM_EXTERNAL); char token[33]="";
        CHECK(j && ink_json_parse(j,g_result,strlen(g_result))==0 && ink_json_str(j,ink_json_find(j,0,"preview"),token,33)==32,"large preview token"); osal_free(j);
        unsigned confirm=request_apply(token); CHECK(ink_confirm_decide(confirm,true,INK_SRC_TOUCH)==0,"approve large restore");
        responsive_wait("\"phase\":\"done\"",300); verify("owner");
        CHECK(sim_metric_value("cap.timeouts")==0 && sim_metric_value("cap.late")==0,"large preview and restore do not block shared executor queries");
    } else if (!strcmp(phase,"api")) {
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
        api_done=0;
        CHECK(ink_exec_post(INK_EXEC_BG,&api_job,family_state,NULL)==0 && WAIT_FOR(api_done,10000),"keep separate synthetic family binding for reset test");
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
        CHECK(AI("reader.read_page",NULL) && g_rc.revision==2 && strstr(g_rc.say,"第五章"),"actual reader returns to the fifth chapter's second page (%u %.80s)",(unsigned)g_rc.revision,g_rc.say);
        CHECK(AI("reader.bookmarks",NULL) && g_rc.status==INK_RC_DONE && strstr(g_result,"\"id\":") && !strstr(strstr(g_result,"\"id\":")+5,"\"id\":"),"the bookmark is restored too (%.150s)",g_result);
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
        CHECK(wait_phase("\"phase\":\"done\"",180),"PIN-authorized restore completes (%s)",g_result);
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
    const char *length=getenv("INK_BACKUP_BYTES"); if(length)file_bytes=(size_t)strtoul(length,NULL,10);
    const char *race_length=getenv("INK_BACKUP_RACE_BYTES"); if(race_length)race_bytes=(size_t)strtoul(race_length,NULL,10);
    if(!strncmp(phase,"atomic-",7)) { file_path="/me/test.bin"; file_dir="/me"; file_name="test.bin"; }
    if(!strcmp(phase,"deep")) {
        file_path="/me/d1/d2/d3/d4/d5/d6/d7/d8/test.bin";
        file_dir="/me/d1/d2/d3/d4/d5/d6/d7/d8";
        file_name="d1/d2/d3/d4/d5/d6/d7/d8/test.bin";
    }
    product_main();
    CHECK(ink_db_register(&NOTES)==0,"register synthetic member collection");
    if (!strcmp(phase,"seed") || !strcmp(phase,"large-seed") || !strcmp(phase,"atomic-seed")) CHECK(ink_db_register(&MISSING)==0,"register a collection only present at backup time");
    osal_task_cfg_t cfg={.name="backup-test",.fn=run,.stack_bytes=16384,.priority=OSAL_PRIO_NORMAL,.core=-1,.flags=OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg,NULL)==OSAL_OK,"backup test task");
}
