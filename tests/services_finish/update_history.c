#include "product.h"
#include "ink/hal_power.h"
#ifndef INK_TEST_UPDATE_SOURCE
#define INK_TEST_UPDATE_SOURCE "../../system/services/update/update_service.c"
#endif
#include INK_TEST_UPDATE_SOURCE
#ifdef INK_TEST_OLD_UPDATE
static pkg_err_t check_rules_for(const pkg_t *p, bool retry) { (void)retry; return check_rules(p); }
#endif
static void run(void *arg) {
    (void)arg; delay_ms(1800);
    const char *phase=getenv("INK_REVIEW_HISTORY");
    if (!strcmp(phase,"legacy")) {
        U.st.blocked_code=200;
        CHECK(save_state(),"legacy last-failed record saved");
    } else if (!strcmp(phase,"rollback")) {
        CHECK(!is_newer(200),"legacy failure imported");
        snprintf(U.st.state,sizeof U.st.state,"trying"); U.st.code=300;
        snprintf(U.st.version,sizeof U.st.version,"3.0.0");
        CHECK(save_state(),"second failing version transaction saved");
        ink_update_boot_check();
        CHECK(!is_newer(300),"new failure blocked");
        CHECK(!is_newer(200),"older failure remains blocked after another rollback");
    } else {
        CHECK(!is_newer(200)&&!is_newer(300),"both failed versions remain blocked after restart");
        CHECK(is_newer(400),"unfailed newer version is offered");
        pkg_t p={.code=200,.nsec=1,.sec={{.kind=SEC_APP,.size=8192}}}; snprintf(p.product,sizeof p.product,"%s",U.product);
        CHECK(check_rules(&p)==PKG_BLOCKED,"automatic path rejects historical failure");
        CHECK(check_rules_for(&p,true)==PKG_OK,"explicit human retry can revalidate failed version");
        snprintf(p.product,sizeof p.product,"SYNTHETIC_WRONG_PRODUCT");
        CHECK(check_rules_for(&p,true)==PKG_WRONG_PRODUCT,"manual retry cannot bypass product validation");
        CHECK(AI("system.upgrade_from_card","{\"file\":\"synthetic.inkfw\",\"retry_failed\":true}")&&g_rc.status==INK_RC_INVALID,"AI cannot authorize retry");
        CHECK(BY_HAND("system.upgrade_from_card","{\"file\":\"synthetic.inkfw\",\"retry_failed\":true}")&&g_rc.status==INK_RC_PENDING_CONFIRMATION,"manual retry still needs device confirmation");
    }
    ink_db_flush_all();
    CHECK(WAIT_FOR(ink_db_idle()&&!ink_storage_pending(),4000),"history durable");
    ink_probe_count("test.finished");
    if (strcmp(phase,"inspect")) hal_power_reboot();
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg={.name="history-regression",.fn=run,.stack_bytes=16384,.priority=OSAL_PRIO_NORMAL,.core=-1,.flags=OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg,NULL)==OSAL_OK,"test task");
}
