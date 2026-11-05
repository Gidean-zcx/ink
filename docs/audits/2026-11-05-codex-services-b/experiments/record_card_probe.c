#include "product_probe.h"
static volatile int completed;
static char reason[32];
static void event(void *ctx,const char *id,const ink_args_t *a) {
    (void)ctx; (void)id; ink_args_get_str(a,"why",reason,sizeof reason); completed++;
}
static void remove_card(void *arg) { (void)arg; sim_sd_set_present(false); }
static void insert_card(void *arg) { (void)arg; sim_sd_set_present(true); }
static void audit_task(void *arg) {
    (void)arg; delay_ms(1800);
    ink_cap_subscribe("audio.record_done",event,NULL,INK_EXEC_DIRECT);
    CHECK(BY_HAND("audio.record","{\"path\":\"/me/card-pull.opus\"}") && g_rc.status==INK_RC_DONE,"record");
    delay_ms(1500);
    hw(HW_USB_ON,0,0);
    int usb=ink_usb_disk_start();
    printf("AUDIT recording_usb_start=%d active=%d\n",usb,ink_usb_disk_active());
    CHECK(usb==INK_FS_ERR_BUSY && !ink_usb_disk_active(),"recording keeps the card from lending");
    vtk_event_at(vtk_now()+SIM_MS(1),remove_card,NULL);
    CHECK(WAIT_FOR(completed==1,3000),"recording ends after card loss");
    printf("AUDIT card_removed reason=%s recording=%d capture=%d\n",reason,ink_audio_recording(),ink_audio_capture_busy());
    CHECK(!ink_audio_recording() && !ink_audio_capture_busy(),"resources released after loss");
    vtk_event_at(vtk_now()+SIM_MS(1),insert_card,NULL); delay_ms(300);
    CHECK(BY_HAND("media.play","{\"path\":\"/me/card-pull.opus\",\"start_ms\":0}") && g_rc.status==INK_RC_DONE,"existing full pages remain playable");
    ink_media_stop();
    CHECK(ink_usb_disk_start()==0 && ink_usb_disk_active(),"card can now be lent");
    CHECK(BY_HAND("audio.record","{\"path\":\"/me/lent.opus\"}") && g_rc.status==INK_RC_FAILED,"new recording refused while lent");
    ink_usb_disk_stop(); hw(HW_USB_OFF,0,0);
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t c={.name="audit-card",.fn=audit_task,.stack_bytes=16384,.priority=OSAL_PRIO_NORMAL,.core=-1,.flags=OSAL_TASK_STACK_EXTERNAL};
    osal_task_create(&c,NULL);
}
