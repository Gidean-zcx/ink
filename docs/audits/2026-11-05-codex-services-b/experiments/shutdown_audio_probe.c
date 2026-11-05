#include "product_probe.h"
static const ink_db_def_t KEPT={.name="audit.shutdown",.scope=INK_DB_DEVICE,.schema=1};
static ink_db_t *db;
extern void (*sim_power_before_shutdown)(void);
static void before_shutdown(void) {
    printf("AUDIT shutdown recording=%d capture=%d db_idle=%d storage_pending=%d durable=%u latest=%u\n", ink_audio_recording(),ink_audio_capture_busy(),ink_db_idle(),ink_storage_pending(),ink_db_durable_ver(db),ink_db_last_ver(db));
    CHECK(ink_db_durable_ver(db)<ink_db_last_ver(db), "reproduced: power requested with unsaved user record");
    ink_probe_count("test.finished");
}
static void audit_task(void *arg) {
    (void)arg; delay_ms(2200);
    ink_db_register(&KEPT); db=ink_db_open(&KEPT,NULL);
    CHECK(db!=NULL,"database open");
    if (getenv("INK_AUDIT_REOPEN")) {
        char v[64]="";
        int rc=ink_db_get(db,2,NULL,v,sizeof v);
        printf("AUDIT reboot get_last_edit_rc=%d text=%s\n",rc,v);
        CHECK(rc!=INK_DB_OK, "reproduced: the accepted edit is missing after reboot");
        ink_probe_count("test.finished"); return;
    }
    ink_db_write_t w={.id=1,.data="before",.len=7};
    CHECK(ink_db_put(db,&w,NULL)==INK_DB_OK,"initial data");
    CHECK(WAIT_FOR(ink_db_idle() && !ink_storage_pending(),5000),"initial data saved");
    CHECK(BY_HAND("audio.record","{\"path\":\"/me/shutdown.opus\"}") && g_rc.status==INK_RC_DONE,"record starts");
    delay_ms(1000);
    w=(ink_db_write_t){.id=2,.data="last_edit",.len=10,.flags=INK_DB_LAZY};
    CHECK(ink_db_put(db,&w,NULL)==INK_DB_OK,"latest user edit accepted");
    sim_power_before_shutdown=before_shutdown;
    BY_HAND("system.shutdown",NULL);
    delay_ms(15000);
    CHECK(false,"shutdown did not happen");
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t c={.name="audit-shutdown",.fn=audit_task,.stack_bytes=16384,.priority=OSAL_PRIO_NORMAL,.core=-1,.flags=OSAL_TASK_STACK_EXTERNAL};
    osal_task_create(&c,NULL);
}
