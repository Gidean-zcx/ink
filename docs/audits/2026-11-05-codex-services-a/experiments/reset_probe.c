#include "product_probe.h"
static void audit_task(void *arg) {
    (void)arg;delay_ms(1800);
    if(getenv("AUDIT_RESET_SEED")) {
        CHECK(ink_fs_write_atomic("/me/private.txt","SYNTHETIC",9)==0,"private card file");
        CHECK(ink_storage_write_atomic("/user/m/owner/private.txt","SYNTHETIC",9)==0,"private flash file");
        CHECK(ink_storage_write_atomic("/user/sys/reset.pending","",0)==0,"durable reset marker");
        CHECK(WAIT_FOR(!ink_storage_pending(),5000),"seed durable");
    } else {
        ink_fs_stat_t s;
        int card=ink_fs_stat("/me/private.txt",&s);
        int64_t flash=ink_storage_size("/user/m/owner/private.txt");
        int64_t pending=ink_storage_size("/user/sys/reset.pending"),mark=ink_storage_size("/user/sys/reset.card");
        printf("AUDIT reset card=%d flash=%lld pending=%lld card_mark=%lld done=%g card_done=%g\n",card,(long long)flash,(long long)pending,(long long)mark,sim_metric_value("system.factory_reset_done"),sim_metric_value("system.factory_reset_card_at_boot"));
        CHECK(pending<0 && mark<0,"reset reports no pending work");
        if(getenv("AUDIT_FAIL_CARD_ROOT")) CHECK(card==0 && flash<0,"reproduced: card data survives completed reset");
        else if(getenv("AUDIT_FAIL_USER_READDIR")) CHECK(flash==9 && card==INK_FS_ERR_NOT_FOUND,"reproduced: flash data survives completed reset");
        else CHECK(card==INK_FS_ERR_NOT_FOUND && flash<0,"control reset cleaned both");
    }
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg={.name="audit-reset",.fn=audit_task,.stack_bytes=16384,.priority=OSAL_PRIO_NORMAL,.core=-1,.flags=OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg,NULL)==OSAL_OK,"audit task");
}
