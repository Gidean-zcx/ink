#include "product_probe.h"
#include "ink/sys/svc.h"
static volatile int done, sentinel;
static void appended(void *ctx,int result) { (void)ctx; done=result==0?1:-1; }
static void tick(void *ctx) { (void)ctx; sentinel++; }
static ink_svc_deadline_t deadline;
static const ink_db_def_t DEF={.name="audit.lazy",.scope=INK_DB_MEMBER,.schema=1,.json=true,.ai_read=true,.ai_write=true};
static void audit_task(void *arg) {
    (void)arg; delay_ms(1800);
    const char *path="/user/sys/append-cache";
    CHECK(ink_storage_write_atomic(path,"A",1)==0,"write initial");
    CHECK(WAIT_FOR(!ink_storage_pending(),3000),"initial durable");
    ink_storage_hold_writes(true);
    CHECK(ink_storage_append(path,"B",1,appended,NULL)==0,"enqueue append");
    char value[32]={0};
    int before=ink_storage_read(path,value,sizeof value-1);
    ink_storage_hold_writes(false);
    CHECK(WAIT_FOR(done,3000) && done==1,"append completed");
    memset(value,0,sizeof value);
    int after=ink_storage_read(path,value,sizeof value-1);
    int64_t disk=ink_storage_size(path);
    printf("AUDIT append_cache before=%d after=%d disk=%lld value=%s\n",before,after,(long long)disk,value);
    CHECK(after==2 && disk==2 && !strcmp(value,"AB"),"queued append prevents caching an old read");

    CHECK(ink_db_register(&DEF)==0,"register");
    ink_db_t *owner=ink_db_open(&DEF,"owner"),*kid=ink_db_open(&DEF,"kid");
    CHECK(owner && kid,"collections");
    ink_db_write_t w={.data="{\"v\":1}",.len=7,.flags=INK_DB_LAZY};
    uint32_t id=0;
    CHECK(ink_db_put(owner,&w,&id)==0,"owner lazy record");
    ink_svc_at(&deadline,osal_now_us()+1000000,tick,NULL);
    CHECK(ink_db_put(kid,&w,NULL)==0,"kid lazy record");
    ink_db_close(kid);
    bool forget=getenv("AUDIT_KEEP_MEMBER")==NULL;
    if(forget) ink_db_forget_member("kid");
    delay_ms(1500);
    printf("AUDIT forget_member sentinel=%d owner_last=%u owner_durable=%u\n",sentinel,ink_db_last_ver(owner),ink_db_durable_ver(owner));
    CHECK(sentinel==(forget?0:1),"forget versus control deadline");
    /* Keep device active so the independent power/lock flush does not mask the lost 60 s data timer. */
    ink_power_hold(INK_NEED_NO_LOCK);
    delay_ms(61000);
    printf("AUDIT forget_member_after_lazy_timeout owner_last=%u owner_durable=%u\n",ink_db_last_ver(owner),ink_db_durable_ver(owner));
    CHECK(ink_db_durable_ver(owner)==(forget?0u:1u),"forget versus control lazy durability");
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg={.name="audit-storage",.fn=audit_task,.stack_bytes=16384,.priority=OSAL_PRIO_NORMAL,.core=-1,.flags=OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg,NULL)==OSAL_OK,"audit task");
}
