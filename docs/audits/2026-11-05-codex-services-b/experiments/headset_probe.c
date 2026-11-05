/* Model identifiers below are deliberately not hardware addresses. */
#include "product_probe.h"
static osal_sem_t *in,*go;
static ink_exec_job_t hold;
static void busy(void *arg) { (void)arg; osal_sem_give(in); osal_sem_take(go,OSAL_WAIT_FOREVER); }
static void headset(void *arg) { (void)arg; sim_headset_add("audit-headset","audit"); }
static void gone(void *arg) { (void)arg; sim_headset_range("audit-headset",false); }
static void audit_task(void *arg) {
    (void)arg; delay_ms(1800);
    CHECK(BY_HAND("audio.record","{\"path\":\"/me/headset.opus\"}") && g_rc.status==INK_RC_DONE,"record");
    delay_ms(5000); BY_HAND("audio.record_stop",NULL);
    CHECK(WAIT_FOR(!ink_audio_recording(),2000),"saved");
    vtk_event_at(vtk_now()+SIM_MS(1),headset,NULL); delay_ms(50);
    CHECK(BY_HAND("bt.pair","{\"addr\":\"audit-headset\"}") && g_rc.status==INK_RC_DONE,"paired");
    CHECK(BY_HAND("bt.connect","{\"addr\":\"audit-headset\"}") && g_rc.status==INK_RC_DONE,"connected");
    CHECK(WAIT_FOR(ink_audio_route()==INK_ROUTE_HEADSET,2500),"headset route");
    CHECK(BY_HAND("media.play","{\"path\":\"/me/headset.opus\",\"start_ms\":0}") && g_rc.status==INK_RC_DONE,"playing");
    delay_ms(800);
    ink_exec_post(INK_EXEC_MEDIA,&hold,busy,NULL);
    CHECK(osal_sem_take(in,2000000)==OSAL_OK,"media busy");
    double speaker=sim_audio_played_ms();
    vtk_event_at(vtk_now()+SIM_MS(1),gone,NULL);
    CHECK(WAIT_FOR(ink_audio_route()==INK_ROUTE_SPEAKER,3000),"route switched");
    delay_ms(1000);
    ink_media_status_t s; ink_media_status(&s);
    printf("AUDIT disconnected speaker_ms=%.0f media_state=%d\n",sim_audio_played_ms()-speaker,s.state);
    CHECK(sim_audio_played_ms()-speaker>500 && s.state==INK_MEDIA_PLAYING,"reproduced: buffered private audio plays out loud before pause job");
    osal_sem_give(go);
    CHECK(WAIT_FOR((ink_media_status(&s),s.state==INK_MEDIA_PAUSED),2000),"eventually paused");
    ink_media_stop();
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main(); in=osal_sem_create(0,1); go=osal_sem_create(0,1);
    osal_task_cfg_t c={.name="audit-headset",.fn=audit_task,.stack_bytes=16384,.priority=OSAL_PRIO_NORMAL,.core=-1,.flags=OSAL_TASK_STACK_EXTERNAL};
    osal_task_create(&c,NULL);
}
