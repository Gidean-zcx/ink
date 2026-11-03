#include "replay.c"

/* New audit cases. Product libraries and input profiles are linked/read unchanged. */
static int r8_varied_stream(int frames) {
    int id=ink_audio_stream_open(INK_AUDIO_PRIO_VOICE,osal_now_us(),NULL);
    for(int i=0;i<frames;i++) {
        uint8_t p[8]; sim_opus_fake_packet(p,8,211+i*3,2000+i*7,8,(uint32_t)i);
        if(ink_audio_stream_push(id,p,8)) osal_panic("r8 push failed");
    }
    ink_audio_stream_finish(id);return id;
}
static void r8_audio(const char *mode) {
    if(ink_display_init()||ink_storage_init()||ink_power_init()||ink_audio_init()) osal_panic("r8 audio init");
    ink_audio_on_stream_failed(r5_audio_failure,NULL);
    bool fault=strstr(mode,"fault")!=NULL;
    bool control=strstr(mode,"control")!=NULL;
    r8_varied_stream(50);
    uint64_t deadline=osal_now_us()+2000000;
    while(sim_audio_played_ms()<120 && osal_now_us()<deadline) osal_task_delay_us(1000);
    if(fault) {
        r6_audio_cut();osal_task_delay_us(500000);
        ink_audio_tone(INK_TONE_DONE,osal_now_us(),"audit.r8_tone_ms");
        osal_task_delay_us(1500000);r6_audio_restore();osal_task_delay_us(800000);
        double before=sim_audio_played_ms();
        ink_audio_stop_all();
        if(strstr(mode,"replace")) r8_varied_stream(5);
        sim_note("AUD9 tone-interleave cancel_ms=%.0f replace=%d",before,strstr(mode,"replace")!=NULL);
    } else if(!control) {
        for(int i=0;i<5;i++) {
            ink_audio_tone(INK_TONE_DONE,osal_now_us(),"audit.r8_tone_ms");
            osal_task_delay_us(400000);
        }
    }
    osal_task_delay_us(7000000);
    ink_storage_write_atomic("/user/r8.txt","done",4);osal_task_delay_us(1000000);
    sim_note("AUD9 audio-final played_ms=%.0f tones=%.0f stalls=%.0f dropped=%.0f failures=%u busy=%d storage=%d",
             sim_audio_played_ms(),sim_metric_value("audit.r8_tone_ms"),sim_metric_value("audio.output_stalls"),
             sim_metric_value("audio.tones_dropped"),r5_audio_failed,sim_audio_busy(),ink_storage_pending());
}
static void r8_idle(void) {
    static const uint64_t gaps[]={4000000,60000000,600000000,4000000};
    for(int i=0;i<4;i++) {
        r4_update(i&1?255:0);
        osal_task_delay_us(gaps[i]);
        uint64_t t=osal_now_us();r4_update(i&1?0:255);
        sim_note("AUD9 idle-refresh gap_us=%llu elapsed_us=%llu pending=%d playing=%d callbacks=%u ok=%u",
                 (unsigned long long)gaps[i],(unsigned long long)(osal_now_us()-t),hal_display_pending(),sim_display_playing(),r4_n,r4_ok);
    }
}
void ink_main(void) {
    const char *m=getenv("AUDIT_PROBE");
    if(!m||strncmp(m,"r8_",3)){r7_main();return;}
    osal_task_delay_us(1600000);sim_note("AUD9 begin %s",m);
    if(!strncmp(m,"r8_audio_",9))r8_audio(m);
    else if(!strcmp(m,"r8_idle"))r8_idle();
    sim_note("AUD9 end %s",m);
}
