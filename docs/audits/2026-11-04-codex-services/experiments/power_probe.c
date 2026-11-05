#define ink_main product_main
#include "../../../../system/boot/main.c"
#undef ink_main
#include "../../../../tests/integration/whole_product.h"

static void run_audit(void *arg) {
    (void)arg;
    delay_ms(1500);
    /* A legitimate background consumer keeps the chip awake but permits locking. */
    ink_power_hold(INK_NEED_AWAKE);
    ink_power_lock();
    delay_ms(1000);
    int64_t before = -1, after = -1;
    ink_state_get_int(ink_state_find("battery.level"), &before);
    hw(HW_BATTERY, 1, 0);
    delay_ms(75000);
    ink_state_get_int(ink_state_find("battery.level"), &after);
    double off = sim_metric_value("power.low_battery_off");
    printf("AUDIT locked battery before=%lld hardware=1 after75s=%lld shutdowns=%.0f state=%s\n", (long long)before, (long long)after, off, power_state());
    CHECK(after == before && after > 2 && off == 0 && !strcmp(power_state(), "locked"), "reproduce missing battery polling");
    ink_probe_count("test.finished");
    /* Release resumes polling, so do it after recording the failed policy. */
    ink_power_release(INK_NEED_AWAKE);
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name = "audit-power", .fn = run_audit, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "create audit");
}
