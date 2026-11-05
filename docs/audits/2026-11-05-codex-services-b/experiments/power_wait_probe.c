#include "product_probe.h"
extern void (*sim_power_before_shutdown)(void);
static osal_sem_t *ui_entered;
static ink_exec_job_t blocker;
static uint64_t began;
static bool finished;
static void slow_ui(void *arg) { (void)arg; osal_sem_give(ui_entered); delay_ms(9000); }
static void before_shutdown(void) {
    uint64_t elapsed=(osal_now_us()-began)/1000;
    printf("AUDIT power_wait elapsed_ms=%llu late=%.0f on_panel=%.0f low_battery_off=%.0f state=%s\n",(unsigned long long)elapsed,sim_metric_value("ui.off_picture_late"),sim_metric_value("ui.off_picture_on_panel"),sim_metric_value("power.low_battery_off"),power_state());
    CHECK(elapsed>=6000 && elapsed<7500,"panel wait has a six second bound");
    CHECK(sim_metric_value("ui.off_picture_late")>=1 && sim_metric_value("ui.off_picture_on_panel")==0,"unrun UI job is reported late");
    ink_probe_count("audit.shutdown_requests");
    if (!finished) { finished=true; ink_probe_count("test.finished"); }
}
static void audit_task(void *arg) {
    (void)arg; delay_ms(2500);
    ink_exec_post(INK_EXEC_UI,&blocker,slow_ui,NULL);
    CHECK(osal_sem_take(ui_entered,1000000)==OSAL_OK,"UI busy");
    sim_power_before_shutdown=before_shutdown; began=osal_now_us();
    ink_call_opts_t opts={.src=INK_SRC_TOUCH};
    CHECK(ink_cap_call("system.shutdown",NULL,&opts)==0,"manual shutdown queued");
    delay_ms(1000);
    hw(HW_BATTERY,2,0); ink_power_lock();
    delay_ms(300); hw(HW_BATTERY,2,0); ink_power_lock();
    delay_ms(10000); CHECK(false,"shutdown did not finish");
}
void ink_main(void) {
    product_main(); ui_entered=osal_sem_create(0,1);
    osal_task_cfg_t c={.name="audit-power-wait",.fn=audit_task,.stack_bytes=16384,.priority=OSAL_PRIO_NORMAL,.core=-1,.flags=OSAL_TASK_STACK_EXTERNAL};
    osal_task_create(&c,NULL);
}
