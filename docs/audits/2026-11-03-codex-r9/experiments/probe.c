/* Reuse previous audit probes through source inclusion; only this round writes results. */
#include "replay.c"

static unsigned r9_done_n,r9_ok_n;
static uint64_t r9_queued;
static void r9_done(void *ctx,const hal_display_result_t *r) {
    (void)ctx;r9_done_n++;r9_ok_n+=r->ok;
    sim_note("AUD10 callback id=%u ok=%d total_us=%llu drive_us=%llu mean=%.4f",
        r->id,r->ok,(unsigned long long)(r->done_us-r9_queued),
        (unsigned long long)(r->done_us-r->started_us),r5_mean(0,0,1216,684));
}
static void r9_full(int gray,hal_waveform_t wf) {
    static uint8_t px[1216*684];memset(px,gray,sizeof px);
    hal_rect_t a={0,0,1216,684};r9_queued=osal_now_us();
    if(!hal_display_update(&a,px,1216,wf,0,r9_done,NULL))osal_panic("r9 rejected");
    r6_wait_display();
    if(hal_display_pending()||sim_display_playing())osal_panic("r9 display timeout");
}
static void r9_concurrent(bool control) {
    if(ink_display_init()||ink_storage_init()||ink_power_init()||ink_audio_init())osal_panic("r9 init");
    ink_audio_on_stream_failed(r5_audio_failure,NULL);
    ink_power_hold(INK_NEED_NO_LOCK); /* isolate display rest from automatic sleep */
    const uint64_t gaps[]={4000000,60000000,600000000};
    for(int i=0;i<3;i++) {
        osal_task_delay_us(gaps[i]);
        double before=sim_audio_played_ms();r8_varied_stream(100);
        uint64_t until=osal_now_us()+3000000;
        while(sim_audio_played_ms()<before+120&&osal_now_us()<until)osal_task_delay_us(1000);
        if(!control){r9_full(0,HAL_WF_GL16);r9_full(255,HAL_WF_GC16);}
        until=osal_now_us()+12000000;
        while(sim_audio_busy()&&osal_now_us()<until)osal_task_delay_us(10000);
        sim_note("AUD10 concurrent gap_us=%llu played_ms=%.0f callbacks=%u ok=%u busy=%d failures=%u",
            (unsigned long long)gaps[i],sim_audio_played_ms()-before,r9_done_n,r9_ok_n,sim_audio_busy(),r5_audio_failed);
    }
    ink_storage_write_atomic("/user/r9.txt","done",4);osal_task_delay_us(1000000);
    sim_note("AUD10 final played_ms=%.0f callbacks=%u ok=%u pending=%d playing=%d audio_busy=%d storage=%d",
        sim_audio_played_ms(),r9_done_n,r9_ok_n,hal_display_pending(),sim_display_playing(),sim_audio_busy(),ink_storage_pending());
}
void ink_main(void) {
    const char *m=getenv("AUDIT_PROBE");
    if(!m||strncmp(m,"r9_",3)){r8_main();return;}
    osal_task_delay_us(1600000);
    sim_note("AUD10 begin %s",m);
    if(!strncmp(m,"r9_concurrent",13))r9_concurrent(strstr(m,"control")!=NULL);
    sim_note("AUD10 end %s",m);
}
