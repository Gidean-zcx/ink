#include "product_probe.h"
#include "ink/hal_ota.h"
extern int audit_package(const char *);
extern int audit_dl_offset(void), audit_dl_job(void), audit_dl_task(void), audit_dl_waiting(void);
extern void audit_update_dump(void);
static void change_offer(void *arg) {
    (void)arg;
    setenv("INK_SIM_UPDATE_PKG", getenv("AUDIT_NEXT_PACKAGE"), 1);
}
static void audit_task(void *arg) {
    (void)arg;
    const char *phase = getenv("AUDIT_PHASE");
    delay_ms(1500);
    if (phase && !strcmp(phase, "install")) {
        char install_args[128];
        snprintf(install_args,sizeof install_args,"{\"file\":\"%s\"}",getenv("AUDIT_INSTALL_FILE")?getenv("AUDIT_INSTALL_FILE"):"next.inkfw");
        CHECK(BY_HAND("system.upgrade_from_card", install_args) && g_rc.status == INK_RC_PENDING_CONFIRMATION, "installation asks");
        unsigned token = 0;
        sscanf(g_rc.object, "confirm:%u", &token);
        ink_probe_count("test.finished"); /* this run's expected end is reboot */
        CHECK(ink_confirm_decide(token, true, INK_SRC_TOUCH) == 0, "accept installation");
        delay_ms(30000);
        CHECK(0, "installation should reboot");
        return;
    }
    if (phase && !strcmp(phase, "packages")) {
        const char *names[] = {"normal", "overlap", "duplicate", "unknown", "zero", "past_eof", "truncated", "overflow", "unsigned_damage"};
        for (unsigned i=0; i<sizeof names/sizeof names[0]; i++) {
            char p[128];
            snprintf(p,sizeof p,"/content/cases/%s.inkfw",names[i]);
            printf("AUDIT package %s result=%d\n",names[i],audit_package(p));
        }
    } else if (phase && !strcmp(phase, "download")) {
        ink_hub_demand();
        CHECK(WAIT_FOR(ink_hub_online(),30000),"online");
        CHECK(AI("update.check",NULL) && g_rc.status==INK_RC_DONE,"offer");
        CHECK(AI("update.download",NULL) && g_rc.status==INK_RC_DONE,"start");
        CHECK(WAIT_FOR(audit_dl_offset()>0,10000),"some original data");
        printf("AUDIT download_before_change off=%d\n",audit_dl_offset());
        vtk_event_at(vtk_now()+SIM_MS(1),change_offer,NULL);
        delay_ms(10);
        CHECK(AI("update.check",NULL) && g_rc.status==INK_RC_DONE,"new offer during download");
        audit_update_dump();
        CHECK(WAIT_FOR(!audit_dl_job(),60000),"download ends");
        char state[32];
        ink_state_get_str(ink_state_find("update.state"),state,sizeof state);
        printf("AUDIT download_after_change state=%s bad=%g partial=%lld\n",state,sim_metric_value("update.download_bad"),(long long)hal_fs_size("/content/.update/2.0.0.inkfw.part"));
        hal_file_t *f=hal_fs_open("/content/.update/2.0.0.inkfw",HAL_O_READ);
        unsigned char h[76]; uint32_t actual=0;
        if(f && hal_fs_read(f,h,sizeof h)==sizeof h) memcpy(&actual,h+72,4);
        if(f) hal_fs_close(f);
        printf("AUDIT downloaded_package header_code=%u size=%lld\n",actual,(long long)hal_fs_size("/content/.update/2.0.0.inkfw"));
    } else {
        delay_ms(6000);
        audit_update_dump();
        char saved[512]="";
        int n=ink_storage_read("/user/sys/update.json",saved,sizeof saved-1);
        if(n>=0) saved[n]=0;
        printf("AUDIT persisted_state len=%d json=%s marked_valid=%g resource_bytes=%g resource_failed=%g\n",n,saved,sim_metric_value("ota.marked_valid"),sim_metric_value("ota.resource_bytes"),sim_metric_value("update.resources_failed"));
        if (getenv("AUDIT_PRIOR")) printf("AUDIT prior_failed_package result=%d\n",audit_package("/content/prior.inkfw"));
    }
    ink_probe_count("test.finished");
}
static bool audit_self_fails(void) { return false; }
void ink_main(void) {
    if(getenv("AUDIT_SELF_FAIL")) ink_update_self_check_add(audit_self_fails);
    product_main();
    osal_task_cfg_t cfg={.name="audit-update",.fn=audit_task,.stack_bytes=16384,.priority=OSAL_PRIO_NORMAL,.core=-1,.flags=OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg,NULL)==OSAL_OK,"audit task");
}
