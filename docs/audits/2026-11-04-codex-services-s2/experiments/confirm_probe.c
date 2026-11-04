#include "product_probe.h"
#include "ink/sys/confirm.h"
extern int audit_retired_confirm_has(const char *marker);
static void audit_task(void *arg) {
    (void)arg;
    delay_ms(1800);
    /* Generated synthetic values only; nothing is sent to the network and none is printed. */
    char marker[25], args[128];
    for (int i = 0; i < 24; i++) marker[i] = (char)('a' + i);
    marker[24] = 0;
    snprintf(args, sizeof args, "{\"ssid\":\"%s\",\"password\":\"%s\"}", "audit-fixture", marker);
    CHECK(AI("wifi.connect", args) && g_rc.status == INK_RC_PENDING_CONFIRMATION, "synthetic credential held");
    unsigned token = 0;
    sscanf(g_rc.object, "confirm:%u", &token);
    CHECK(ink_confirm_withdraw(token, INK_SRC_AI) == 0, "withdraw before any connection attempt");
    memset(args, 0, sizeof args);
    delay_ms(65000);
    int left = audit_retired_confirm_has(marker);
    printf("AUDIT confirm unused_slots_with_synthetic_secret=%d after_withdraw_s=65\n", left);
    CHECK(left > 0, "reproduce retained credential after withdrawal and expiry interval");
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t c = {.name="audit-confirm", .fn=audit_task, .stack_bytes=16384, .priority=OSAL_PRIO_NORMAL, .core=-1, .flags=OSAL_TASK_STACK_EXTERNAL};
    osal_task_create(&c, NULL);
}
