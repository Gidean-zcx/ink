#include "../../2026-11-03-codex-r7/experiments/replay.c"

/* R7 probes link the original product libraries. Observations do not patch product state. */
static void r7_audio(const char *mode) {
    if (ink_display_init() || ink_storage_init() || ink_power_init() || ink_audio_init()) osal_panic("r7 audio init");
    ink_audio_on_stream_failed(r5_audio_failure,NULL);
    bool tone = strstr(mode,"tone") != NULL;
    r5_stream(tone ? 300 : 50,true);
    uint64_t end=osal_now_us()+2000000;
    while(sim_audio_played_ms()<120 && osal_now_us()<end) osal_task_delay_us(1000);
    if(tone) {
        uint64_t t=osal_now_us();
        ink_audio_tone(INK_TONE_ERROR,t,"audit.tone_ms");
        sim_note("AUD8 tone-request at_us=%llu played=%.0f",(unsigned long long)t,sim_audio_played_ms());
        osal_task_delay_us(1000000);
        sim_note("AUD8 tone-after-1s heard_count=%.0f played=%.0f",sim_metric_value("audit.tone_ms"),sim_audio_played_ms());
        osal_task_delay_us(20000000);
        sim_note("AUD8 tone-final heard_count=%.0f played=%.0f busy=%d",sim_metric_value("audit.tone_ms"),sim_audio_played_ms(),sim_audio_busy());
    } else {
        r6_audio_cut(); osal_task_delay_us(2000000); r6_audio_restore();
        osal_task_delay_us(800000);
        double at=sim_audio_played_ms();
        int next=r5_stream(5,true);
        sim_note("AUD8 replace-during-reopen at_played=%.0f new_id=%d",at,next);
        osal_task_delay_us(4000000);
        sim_note("AUD8 replaced-final extra_ms=%.0f failures=%u busy=%d",sim_audio_played_ms()-at,r5_audio_failed,sim_audio_busy());
    }
    ink_storage_write_atomic("/user/r7.txt","done",4);
    osal_task_delay_us(1000000);
    r5_audio_state("r7-final");
}

typedef struct { hal_rect_t a; unsigned count; int latest; } r7_region_t;
static r7_region_t regions[16];
static void r7_region_done(void *ctx,const hal_display_result_t *r) {
    r7_region_t *v=ctx;
    v->count++;
    sim_note("AUD8 region id=%u x=%d y=%d ok=%d callbacks=%u mean=%.2f latest=%d at_us=%llu",r->id,v->a.x,v->a.y,r->ok,v->count,
        r5_mean(v->a.x,v->a.y,v->a.w,v->a.h),v->latest,(unsigned long long)osal_now_us());
}
static void r7_region_push(int i,int gray,hal_waveform_t wf) {
    static uint8_t px[64*64]; memset(px,gray,sizeof px);
    regions[i].latest=gray;
    hal_display_update(&regions[i].a,px,64,wf,0,r7_region_done,&regions[i]);
}
static void r7_cut_redraw(void *arg) {
    (void)arg;
    while(sim_metric_value("display.redraws_after_supply_loss")<1 || !sim_display_playing()) osal_task_delay_us(1000);
    osal_task_delay_us(20000);
    metalio_board_set(0,MB_P0_CAM_SCR,0);
    sim_note("AUD8 cut automatic redraw at_us=%llu",(unsigned long long)osal_now_us());
}
static void r7_display(const char *mode) {
    bool control=strstr(mode,"control")!=NULL, recut=strstr(mode,"recut")!=NULL;
    for(int i=0;i<16;i++) regions[i].a=(hal_rect_t){(i%8)*80,(i/8)*80,64,64};
    if(!control) r5_cut_next();
    for(int i=0;i<16;i++) r7_region_push(i,0,HAL_WF_GL16);
    r6_wait_display();
    metalio_board_set(0,MB_P0_CAM_SCR,MB_P0_CAM_SCR);
    if(recut) {
        osal_task_cfg_t c={.name="r7-redraw-cut",.fn=r7_cut_redraw,.priority=10,.core=-1,.stack_bytes=4096};
        osal_task_create(&c,NULL);
    }
    static uint8_t other[128*128]; memset(other,0,sizeof other);
    hal_rect_t a={800,0,128,128};
    hal_display_update(&a,other,128,HAL_WF_GC16,0,r5_done,NULL);
    osal_task_delay_us(20000);
    for(int i=0;i<16;i++) r7_region_push(i,0,HAL_WF_GL16);
    r6_wait_display();
    if(recut) {
        metalio_board_set(0,MB_P0_CAM_SCR,MB_P0_CAM_SCR);
        for(int i=0;i<16;i++) r7_region_push(i,0,HAL_WF_GL16);
        r6_wait_display();
    }
    for(int round=0;round<3;round++) {
        for(int i=0;i<16;i++) r7_region_push(i,(round&1)?0:255,HAL_WF_GL16);
        r6_wait_display();
    }
    sim_note("AUD8 display-final pending=%d playing=%d redraws=%.0f",hal_display_pending(),sim_display_playing(),sim_metric_value("display.redraws_after_supply_loss"));
}

#ifndef AUDIT_R7_BEFORE
static void r7_rails_state(const char *tag) {
    sim_note("AUD8 rails %s flag=%d p05=%d p14=%d dirs=%02x,%02x at_us=%llu",tag,metalio_board_rails_cut(),
        sim_board_cam_power(),sim_board_shared_power(),sim_tca_config(0),sim_tca_config(1),(unsigned long long)osal_now_us());
}
static bool r7_cancel_sleep(void *ctx) {
    r7_rails_state("at-cancel");
    if(ctx) sim_i2c_set_faults(1,"io-expander");
    return true;
}
static void r7_rails(const char *mode) {
    r7_rails_state("initial");
    if(strstr(mode,"deadline")) {
        metalio_board_cut_rails(true); r7_rails_state("cut");
        for(int i=0;i<3;i++) {
            uint64_t t=osal_now_us(); r5_update(0,0,HAL_WF_GL16,false); r6_wait_display();
            sim_note("AUD8 restoring-off request=%d elapsed_us=%llu",i,(unsigned long long)(osal_now_us()-t));
        }
        metalio_board_restore_rails(); r7_rails_state("restored");
        metalio_board_set(0,MB_P0_CAM_SCR,0);
        uint64_t t=osal_now_us(); r5_update(0,0,HAL_WF_GL16,false); r6_wait_display();
        sim_note("AUD8 unrelated-off elapsed_us=%llu",(unsigned long long)(osal_now_us()-t));
        metalio_board_set(0,MB_P0_CAM_SCR,MB_P0_CAM_SCR);
    } else {
        bool failed=strstr(mode,"fault")!=NULL;
        hal_input_set_sink(sink,NULL);
        hal_sleep_cfg_t sc={.max_us=500000,.wake_mask=HAL_WAKE_POWER_KEY,.keep_modem=false};
        if(strstr(mode,"cancel")) {
            sc.cancel=r7_cancel_sleep; sc.cancel_ctx=failed?(void*)1:NULL;
        } else if(failed) {
            vtk_lock();
            vtk_event_after(SIM_MS(400),power_down_event,NULL);
            vtk_event_after(SIM_MS(401),fault_on,NULL);
            vtk_event_after(SIM_MS(450),power_up_event,NULL);
            vtk_unlock();
        }
        uint32_t why=hal_power_sleep(&sc);
        sim_note("AUD8 sleep-return why=%u errors=%.0f cancelled=%.0f",why,sim_metric_value("power.sleep_restore_errors"),sim_metric_value("power.sleep_cancelled"));
        r7_rails_state("sleep-return");
        if(failed) {
            r5_update(0,0,HAL_WF_GL16,false); r6_wait_display();
            r7_rails_state("still-failing");
            sim_i2c_set_faults(0,NULL);
            osal_task_delay_us(2000000);
        }
        r7_rails_state("settled");
    }
    r5_update(0,0,HAL_WF_GL16,false); r6_wait_display();
    r7_rails_state("final");
}
#endif

static void r7_main(void) {
    const char *m=getenv("AUDIT_PROBE");
    if(!m || strncmp(m,"r7_",3)) { r6_main(); return; }
    osal_task_delay_us(1600000);
    sim_note("AUD8 begin %s",m);
    if(!strncmp(m,"r7_audio_",9)) r7_audio(m);
    else if(!strncmp(m,"r7_display_",11)) r7_display(m);
#ifndef AUDIT_R7_BEFORE
    else if(!strncmp(m,"r7_rails_",9)) r7_rails(m);
#endif
    sim_note("AUD8 end %s",m);
}
