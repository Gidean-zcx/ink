/* Normal product boot, public setting API, scripted Wi-Fi loss during a SIM switch.
 * net_service.c is unchanged, included only to inspect its pending decision. */
#include <stdio.h>
#include "ink/osal.h"
#include "ink/sys/exec.h"
#include "ink/sys/settings.h"
#include "sim.h"
#include "../../../../system/services/net/net_service.c"
#define ink_main product_main
#include "../../../../system/boot/main.c"
#undef ink_main

static void until_s(int s) {
    uint64_t t = (uint64_t)s * 1000000, now = osal_now_us();
    if (t > now) osal_task_delay_us(t - now);
}

static void snapshot(const char *where) {
    hal_link_status_t st;
    hal_net_link_status(&st);
    printf("AUDIT_NET %s at=%.3f type=%d up=%d wifi_available=%d lte_available=%d down_since=%llu hub=%d\n",
           where, osal_now_us() / 1e6, st.type, st.up, hal_net_available(HAL_LINK_WIFI),
           hal_net_available(HAL_LINK_LTE), (unsigned long long)NS.down_since, ink_hub_online());
}

static void audit_task(void *arg) {
    (void)arg;
    ink_receipt_t rc;
    until_s(8);
    ink_hub_demand();
    snapshot("before_switch");
    until_s(9);
    ink_setting_set("cell.sim_slot", 1, INK_SRC_TOUCH, &rc);
    until_s(11);
    snapshot("wifi_gone_card_registering");
    until_s(17);
    snapshot("card_registered");
    until_s(40);
    snapshot("still_waiting");
    /* Positive control: setting the already-selected policy must prod the decision. */
    ink_setting_set("net.prefer", 0, INK_SRC_TOUCH, &rc);
    until_s(44);
    snapshot("same_auto_set_again");
    ink_probe_count("audit.finished");
}

void ink_main(void) {
    product_main();
    osal_task_cfg_t c = {.name="audit-net", .fn=audit_task, .stack_bytes=8192, .priority=3, .core=-1, .flags=OSAL_TASK_STACK_EXTERNAL};
    if (osal_task_create(&c, NULL) != OSAL_OK) osal_panic("audit task");
}
