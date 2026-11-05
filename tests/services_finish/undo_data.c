#include "product.h"

static const ink_db_def_t NOTE={.name="review2.note",.scope=INK_DB_MEMBER,.schema=1,.json=true,.ai_read=true,.ai_write=true};
static void add_ap(void *ctx) { (void)ctx; sim_wifi_ap_add("SYNTHETIC_AP",-40,""); }
static void run(void *arg) {
    (void)arg;delay_ms(1800);
    CHECK(ink_db_register(&NOTE)==0,"collection");
    CHECK(AI("data.put","{\"collection\":\"review2.note\",\"id\":101,\"value\":{\"text\":\"draft\"}}")&&g_rc.status==INK_RC_DONE,"AI draft");
    char undo[160];snprintf(undo,sizeof undo,"{\"object\":\"%s\",\"revision\":%u}",g_rc.object,g_rc.revision);
    CHECK(BY_HAND("data.put","{\"collection\":\"review2.note\",\"id\":101,\"value\":{\"text\":\"SYNTHETIC_HUMAN_EDITS\"}}")&&g_rc.status==INK_RC_DONE,"human edits");
    CHECK(AI("system.undo",undo),"record undo answers");
    CHECK(g_rc.status==INK_RC_STALE||g_rc.status==INK_RC_PENDING_CONFIRMATION,"old inverse deleted newer record: %s",ink_rc_name(g_rc.status));
    CHECK(AI("data.get","{\"collection\":\"review2.note\",\"id\":101}")&&g_rc.status==INK_RC_DONE&&strstr(g_result,"SYNTHETIC_HUMAN_EDITS"),"human record survives: %s",ink_rc_name(g_rc.status));
    CHECK(AI("data.put","{\"collection\":\"review2.note\",\"id\":102,\"value\":{\"text\":\"control\"}}")&&g_rc.status==INK_RC_DONE,"control record");
    snprintf(undo,sizeof undo,"{\"object\":\"%s\",\"revision\":%u}",g_rc.object,g_rc.revision);
    CHECK(AI("system.undo",undo)&&g_rc.status==INK_RC_UNDONE,"unmodified control undo");
    CHECK(AI("file.write","{\"path\":\"/me/draft.txt\",\"text\":\"draft\"}")&&g_rc.status==INK_RC_DONE,"AI file");
    snprintf(undo,sizeof undo,"{\"object\":\"%s\",\"revision\":%u}",g_rc.object,g_rc.revision);
    CHECK(BY_HAND("file.write","{\"path\":\"/me/draft.txt\",\"text\":\"SYNTHETIC_HUMAN_APPEND\",\"append\":true}")&&g_rc.status==INK_RC_DONE,"human append");
    CHECK(AI("system.undo",undo),"file undo answers");
    CHECK(g_rc.status==INK_RC_STALE||g_rc.status==INK_RC_PENDING_CONFIRMATION,"old inverse deleted newer file: %s",ink_rc_name(g_rc.status));
    CHECK(AI("file.read","{\"path\":\"/me/draft.txt\"}")&&g_rc.status==INK_RC_DONE&&strstr(g_result,"SYNTHETIC_HUMAN_APPEND"),"human append survives: %s",ink_rc_name(g_rc.status));
    CHECK(AI("file.write","{\"path\":\"/me/control.txt\",\"text\":\"control\"}")&&g_rc.status==INK_RC_DONE,"untouched file");
    snprintf(undo,sizeof undo,"{\"object\":\"%s\",\"revision\":%u}",g_rc.object,g_rc.revision);
    CHECK(AI("system.undo",undo)&&g_rc.status==INK_RC_UNDONE,"unmodified file inverse works");
    CHECK(AI("file.write","{\"path\":\"/me/replaced.txt\",\"text\":\"draft\"}")&&g_rc.status==INK_RC_DONE,"replacement draft");
    snprintf(undo,sizeof undo,"{\"object\":\"%s\",\"revision\":%u}",g_rc.object,g_rc.revision);
    CHECK(ink_fs_write_atomic("/me/replaced.txt","SYNTHETIC_HUMAN",15)==0,"direct atomic replacement");
    CHECK(AI("system.undo",undo)&&g_rc.status==INK_RC_STALE,"direct replacement invalidates file inverse");
    CHECK(ink_fs_write_atomic("/me/from.txt","draft",5)==0,"move draft");
    CHECK(AI("file.move","{\"from\":\"/me/from.txt\",\"to\":\"/me/to.txt\"}")&&g_rc.status==INK_RC_DONE,"move with inverse");
    snprintf(undo,sizeof undo,"{\"object\":\"%s\",\"revision\":%u}",g_rc.object,g_rc.revision);
    CHECK(ink_fs_write_atomic("/me/to.txt","SYNTHETIC_HUMAN",15)==0,"human changes moved object");
    CHECK(AI("system.undo",undo)&&g_rc.status==INK_RC_STALE,"old move cannot move newer content");
    CHECK(AI("file.move","{\"from\":\"/me/to.txt\",\"to\":\"/me/final.txt\"}")&&g_rc.status==INK_RC_DONE,"untouched move");
    snprintf(undo,sizeof undo,"{\"object\":\"%s\",\"revision\":%u}",g_rc.object,g_rc.revision);
    CHECK(AI("system.undo",undo)&&g_rc.status==INK_RC_UNDONE,"unmodified move inverse works");
    CHECK(AI("data.put","{\"collection\":\"review2.note\",\"id\":103,\"value\":{\"text\":\"draft\"}}")&&g_rc.status==INK_RC_DONE,"recreated-record draft");
    snprintf(undo,sizeof undo,"{\"object\":\"%s\",\"revision\":%u}",g_rc.object,g_rc.revision);
    ink_db_t *db=ink_db_open(&NOTE,NULL);
    ink_db_write_t wr={.id=103,.data="{\"text\":\"SYNTHETIC_HUMAN\"}",.len=26,.src=INK_SRC_TOUCH};
    CHECK(db&&ink_db_del(db,103,INK_SRC_TOUCH)==INK_DB_OK&&ink_db_put(db,&wr,NULL)==INK_DB_OK,"human recreates same record id");
    ink_db_close(db);
    CHECK(AI("system.undo",undo)&&g_rc.status==INK_RC_STALE,"recreated record identity is protected");
    CHECK(AI("notify.post","{\"title\":\"SYNTHETIC_DRAFT\"}")&&g_rc.status==INK_RC_DONE,"notification draft");
    unsigned nid=0; sscanf(g_rc.object,"notification:%u",&nid);
    snprintf(undo,sizeof undo,"{\"object\":\"%s\",\"revision\":%u}",g_rc.object,g_rc.revision);
    char mark[64]; snprintf(mark,sizeof mark,"{\"id\":%u}",nid);
    CHECK(BY_HAND("notify.read",mark)&&g_rc.status==INK_RC_DONE,"human marks notification read");
    CHECK(AI("system.undo",undo)&&g_rc.status==INK_RC_STALE,"notification inverse preserves human change");
    int64_t now=ink_time_now_s(); char time_args[96];
    snprintf(time_args,sizeof time_args,"{\"utc_s\":%lld}",(long long)(now+10));
    CHECK(AI("time.set",time_args)&&g_rc.status==INK_RC_DONE,"AI sets clock");
    snprintf(undo,sizeof undo,"{\"object\":\"%s\",\"revision\":%u}",g_rc.object,g_rc.revision);
    ink_time_set(now+20,false);
    CHECK(AI("system.undo",undo)&&g_rc.status==INK_RC_STALE,"clock inverse preserves later direct correction");
    vtk_event_at(vtk_now()+SIM_MS(1),add_ap,NULL); delay_ms(20);
    CHECK(BY_HAND("wifi.connect","{\"ssid\":\"SYNTHETIC_AP\",\"password\":\"\"}")&&g_rc.status==INK_RC_DONE,"new synthetic Wi-Fi connection");
    snprintf(undo,sizeof undo,"{\"object\":\"%s\",\"revision\":%u}",g_rc.object,g_rc.revision);
    CHECK(BY_HAND("wifi.connect","{\"ssid\":\"SYNTHETIC_AP\"}")&&g_rc.status==INK_RC_DONE,"human refreshes saved network");
    CHECK(AI("system.undo",undo)&&g_rc.status==INK_RC_STALE,"Wi-Fi inverse preserves newer credentials");
    CHECK(AI("wifi.list",NULL)&&strstr(g_result,"SYNTHETIC_AP"),"saved network survives old inverse");
    ink_probe_count("test.finished");
}

void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg={.name="review2-probe",.fn=run,.stack_bytes=16384,
        .priority=OSAL_PRIO_NORMAL,.core=-1,.flags=OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg,NULL)==OSAL_OK,"test task");
}
