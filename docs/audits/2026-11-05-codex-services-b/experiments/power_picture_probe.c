#include "product_probe.h"
extern void (*sim_power_before_shutdown)(void);
static void before_shutdown(void) {
    printf("AUDIT off_picture_on_panel=%.0f late=%.0f failures=%.0f panel_busy=%d ui_busy=%d\n",sim_metric_value("ui.off_picture_on_panel"),sim_metric_value("ui.off_picture_late"),sim_metric_value("display.power_failures"),ink_display_busy(),ink_ui_busy());
    CHECK(sim_metric_value("display.power_failures")>0,"panel power actually failed");
    CHECK(sim_metric_value("ui.off_picture_on_panel")==1,"reproduced: failed refresh still called on_panel");
    ink_probe_count("test.finished");
}
static void audit_task(void *arg) {
    (void)arg; delay_ms(2500);
    sim_power_before_shutdown=before_shutdown;
    BY_HAND("system.shutdown",NULL);
    delay_ms(16000);
    CHECK(false,"should have shut down");
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t c={.name="audit-picture",.fn=audit_task,.stack_bytes=16384,.priority=OSAL_PRIO_NORMAL,.core=-1,.flags=OSAL_TASK_STACK_EXTERNAL};
    osal_task_create(&c,NULL);
}
