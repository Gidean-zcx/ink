#include "product_probe.h"
extern void sim_net_set_link(const char *);
static void online(void *arg) { (void)arg; sim_net_set_link("wifi"); }
static void audit_task(void *arg) {
    (void)arg; delay_ms(1800);
    CHECK(!ink_hub_online(), "offline fixture");
    CHECK(ink_fs_write_atomic("/me/AUDIT-owner-only.txt", "synthetic", 9) == INK_FS_OK, "owner creates private file");
    CHECK(ink_member_switch("kid") == 0, "child takes device");
    vtk_event_at(vtk_now()+SIM_MS(1), online, NULL); ink_hub_demand();
    CHECK(WAIT_FOR(ink_hub_online(), 30000), "child online"); delay_ms(2000);
    int leaked=0, without_member=0;
    for (int i=0;i<sim_hub_events_seen();i++) {
        const char *e=sim_hub_event_at(i);
        if(e && strstr(e,"AUDIT-owner-only")) { leaked++; without_member += strstr(e,"\"member\"") == NULL; }
    }
    printf("AUDIT recipient=%s private_file_events=%d unscoped=%d\n",ink_member_current()->id,leaked,without_member);
    CHECK(leaked > 0 && without_member > 0,"reproduce event across members");
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t c = {.name="audit-events", .fn=audit_task, .stack_bytes=16384, .priority=OSAL_PRIO_NORMAL, .core=-1, .flags=OSAL_TASK_STACK_EXTERNAL};
    osal_task_create(&c,NULL);
}
