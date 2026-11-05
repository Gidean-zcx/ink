#include "product_probe.h"
static osal_sem_t *in, *go;
static ink_exec_job_t held;
static void hold_writer(void *arg) { (void)arg; osal_sem_give(in); osal_sem_take(go, OSAL_WAIT_FOREVER); }
static void audit_task(void *arg) {
    (void)arg; delay_ms(1800);
    CHECK(BY_HAND("audio.record", "{\"path\":\"/me/audit.opus\"}") && g_rc.status==INK_RC_DONE,"recording starts");
    delay_ms(400);
    ink_exec_post(INK_EXEC_MEDIA, &held, hold_writer, NULL);
    CHECK(osal_sem_take(in,2000000)==OSAL_OK,"writer temporarily unavailable");
    delay_ms(8500); /* 128 frames at 60ms; ordinary bounded producer pressure */
    CHECK(BY_HAND("audio.record_stop",NULL) && g_rc.status==INK_RC_DONE,"stop accepted");
    CHECK(WAIT_FOR(!ink_audio_capture_busy(),1000),"capture ended");
    osal_sem_give(go);
    delay_ms(2500);
    printf("AUDIT after_drain recording=%d capture=%d focus=%d lost=%.0f completed=%.0f\n",ink_audio_recording(),ink_audio_capture_busy(),ink_audio_focus_top(),sim_metric_value("media.record_lost_frames"),sim_metric_value("media.recordings"));
    CHECK(ink_audio_recording() && !ink_audio_capture_busy() && sim_metric_value("media.recordings")==0,"lost terminal marker leaves recording stuck");
    CHECK(BY_HAND("audio.record_stop",NULL),"second stop returns"); delay_ms(1000);
    CHECK(ink_audio_recording(),"second stop cannot recover");
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main(); in=osal_sem_create(0,1);go=osal_sem_create(0,1);
    osal_task_cfg_t c={.name="audit-rec-end",.fn=audit_task,.stack_bytes=16384,.priority=OSAL_PRIO_NORMAL,.core=-1,.flags=OSAL_TASK_STACK_EXTERNAL};
    osal_task_create(&c,NULL);
}
