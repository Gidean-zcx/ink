#include "product.h"
#include "ink/sys/display.h"
extern bool ink_test_panel_fail;
extern void (*sim_power_before_shutdown)(void);
static void off(void) {
    CHECK(!ink_test_panel_fail,"shutdown only after successful panel dispatch");
    CHECK(sim_metric_value("ui.off_picture_on_panel")==1,"successful retry acknowledges its own off frame");
    ink_probe_count("test.finished");
}
static void run(void *arg) {
    (void)arg; delay_ms(2200);
    sim_power_before_shutdown=off;
    ink_test_panel_fail=true;
    CHECK(BY_HAND("system.shutdown",NULL)&&g_rc.status==INK_RC_FAILED,"failed off frame refuses ordinary shutdown");
    CHECK(sim_metric_value("ui.off_picture_on_panel")==0,"empty display queue cannot prove successful frame");
    ink_test_panel_fail=false;
    CHECK(BY_HAND("system.shutdown",NULL),"retry accepted");
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg={.name="off-frame",.fn=run,.stack_bytes=16384,.priority=OSAL_PRIO_NORMAL,.core=-1,.flags=OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg,NULL)==OSAL_OK,"test task");
}
