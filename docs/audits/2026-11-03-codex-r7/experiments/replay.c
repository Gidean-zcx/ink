/* R6: reuse prior probes without modifying their files or results. */
#include "../../2026-11-03-codex-r6/experiments/legacy_probe.c"

static void r6_display_state(const char *tag) {
    sim_note("AUD7 display %s pending=%d playing=%d cb=%u ok=%u fail=%u redraws=%.0f left=%.2f right=%.2f at_us=%llu",
        tag,hal_display_pending(),sim_display_playing(),r5_callbacks,r5_successes,r5_failures,
        sim_metric_value("display.redraws_after_supply_loss"),r5_mean(0,0,128,128),r5_mean(160,0,128,128),
        (unsigned long long)osal_now_us());
}
static void r6_wait_display(void) {
    uint64_t end=osal_now_us()+20000000;
    do { osal_task_delay_us(10000); } while((hal_display_pending() || sim_display_playing()) && osal_now_us()<end);
}
static void r6_display(const char *mode) {
    if(!strcmp(mode,"r6_p05_off")) {
        metalio_board_set(0,MB_P0_CAM_SCR,0);
        for(int i=0;i<3;i++) {
            uint64_t t=osal_now_us();
            r5_update(0,0,HAL_WF_GL16,false); r6_wait_display();
            sim_note("AUD7 P05 off request=%d elapsed_us=%llu",i,(unsigned long long)(osal_now_us()-t));
        }
        metalio_board_set(0,MB_P0_CAM_SCR,MB_P0_CAM_SCR);
        r5_update(0,0,HAL_WF_GL16,false); r6_wait_display();
    } else if(!strcmp(mode,"r6_gc16_start_fail")) {
        metalio_board_set(0,MB_P0_CAM_SCR,0);
        r5_update(0,0,HAL_WF_GC16,false); r6_wait_display();
        metalio_board_set(0,MB_P0_CAM_SCR,MB_P0_CAM_SCR);
        r5_update(160,0,HAL_WF_GL16,false); r6_wait_display();
        r6_display_state("after-recovery");
        r5_update(0,255,HAL_WF_GL16,false); r6_wait_display();
    } else {
        r5_cut_next(); r5_update(0,0,HAL_WF_GL16,false); r6_wait_display();
        metalio_board_set(0,MB_P0_CAM_SCR,MB_P0_CAM_SCR);
        if(!strcmp(mode,"r6_gc16_overlap_lost") || !strcmp(mode,"r6_gc16_large_other")) {
            if(!strcmp(mode,"r6_gc16_large_other")) {
                static uint8_t p[1056*684]; memset(p,0,sizeof p);
                hal_rect_t a={160,0,1056,684};
                hal_display_update(&a,p,1056,HAL_WF_GC16,0,r5_done,NULL);
            } else r5_update(160,0,HAL_WF_GC16,false);
            osal_task_delay_us(20000);
            r5_update(0,0,HAL_WF_GL16,false);
        } else if(!strcmp(mode,"r6_gc16_partial")) {
            static uint8_t p[128*128]; memset(p,0,sizeof p);
            hal_rect_t a={64,0,128,128};
            hal_display_update(&a,p,128,HAL_WF_GC16,0,r5_done,NULL);
            osal_task_delay_us(20000);
            r5_update(0,0,HAL_WF_GL16,false);
        } else if(!strcmp(mode,"r6_gc16_recuts")) {
            r5_cut_next(); r5_update(160,0,HAL_WF_GC16,false); r6_wait_display();
            metalio_board_set(0,MB_P0_CAM_SCR,MB_P0_CAM_SCR);
            r5_update(0,0,HAL_WF_GC16,false);
        }
        r6_wait_display();
    }
    r6_display_state("final");
    sim_display_snapshot("r6-final");
}

static void r6_audio_cut(void) {
    metalio_board_set(0,MB_P0_AMP,0);
    metalio_board_set(1,MB_P1_SHARED_PWR,0);
    sim_note("AUD7 cut P14 at_us=%llu",(unsigned long long)osal_now_us());
}
static void r6_audio_restore(void) {
    metalio_board_set(1,MB_P1_SHARED_PWR,MB_P1_SHARED_PWR);
    sim_note("AUD7 restored P14 at_us=%llu",(unsigned long long)osal_now_us());
}
static void r6_audio(const char *mode) {
    if(ink_display_init() || ink_storage_init() || ink_power_init() || ink_audio_init()) osal_panic("r6 audio init");
    ink_audio_on_stream_failed(r5_audio_failure,NULL);
    if(!strcmp(mode,"r6_audio_network")) {
        int id=r5_stream(5,false);
        osal_task_delay_us(4000000);
        static const uint8_t f[8]={0xA5,0x00,0x10,0x30,0x11,0,0,0};
        for(int i=0;i<5;i++) ink_audio_stream_push(id,f,sizeof f);
        ink_audio_stream_finish(id);
        osal_task_delay_us(3000000);
    } else {
        r5_stream(50,true);
        uint64_t end=osal_now_us()+2000000;
        while(sim_audio_played_ms()<120 && osal_now_us()<end) osal_task_delay_us(1000);
        if(!strcmp(mode,"r6_audio_display")) {
            static uint8_t px[1216*684]; memset(px,0,sizeof px);
            hal_rect_t a={0,0,1216,684};
            hal_display_update(&a,px,1216,HAL_WF_GC16,0,r5_done,NULL);
            r6_wait_display();
        } else if(!strcmp(mode,"r6_audio_flash")) {
            vtk_lock(); vtk_stall_begin_locked(vtk_now()+SIM_MS(1500)); vtk_unlock();
            osal_task_delay_us(5000000);
        } else {
            r6_audio_cut();
            if(!strcmp(mode,"r6_audio_permanent")) osal_task_delay_us(12000000);
            else if(!strcmp(mode,"r6_audio_tone_stall")) {
                osal_task_delay_us(500000); ink_audio_tone(INK_TONE_ERROR,0,NULL);
                osal_task_delay_us(1500000); r6_audio_restore(); osal_task_delay_us(8000000);
            } else if(!strcmp(mode,"r6_audio_tone_wait")) {
                osal_task_delay_us(3000000); ink_audio_tone(INK_TONE_ERROR,0,NULL);
                osal_task_delay_us(1000000); r6_audio_restore(); osal_task_delay_us(8000000);
            } else if(!strcmp(mode,"r6_audio_newstream")) {
                osal_task_delay_us(7000000); r5_stream(5,true);
                osal_task_delay_us(3000000); r5_audio_state("new-waited-3s");
                r6_audio_restore(); osal_task_delay_us(4000000);
            } else if(!strcmp(mode,"r6_audio_cancel_reopen")) {
                osal_task_delay_us(2000000); r6_audio_restore();
                osal_task_delay_us(800000); ink_audio_stop_all();
                r5_audio_state("cancel-during-reopen"); osal_task_delay_us(3000000);
            } else if(!strcmp(mode,"r6_audio_refault")) {
                osal_task_delay_us(2000000); r6_audio_restore();
                while(sim_audio_played_ms()<300 && osal_now_us()<end+8000000) osal_task_delay_us(10000);
                r6_audio_cut(); osal_task_delay_us(12000000);
            }
        }
    }
    ink_storage_write_atomic("/user/r6.txt","done",4);
    osal_task_delay_us(1000000);
    r5_audio_state("r6-final");
    sim_note("AUD7 audio stalls=%.0f open_failures=%.0f play_failed=%.0f tones_dropped=%.0f",
        sim_metric_value("audio.output_stalls"),sim_metric_value("audio.output_open_failures"),
        sim_metric_value("audio.play_failed"),sim_metric_value("audio.tones_dropped"));
}

static void r6_ai(const char *mode) {
    product_main(); hal_net_use(HAL_LINK_WIFI); osal_task_delay_us(4000000);
    vtk_lock(); r4_ai_down(NULL); vtk_event_after(SIM_MS(1000),r4_ai_up,NULL); vtk_unlock();
    osal_task_delay_us(1300000);
    if(!strcmp(mode,"r6_ai_late_failure")) {
        uint64_t end=osal_now_us()+20000000;
        while(sim_metric_value("ai.answers")<1 && osal_now_us()<end) osal_task_delay_us(100000);
        ink_ui_post(r5_dump_ui,NULL); r6_audio_cut();
        osal_task_delay_us(12000000); ink_ui_post(r5_dump_ui,NULL);
    } else {
        r6_audio_cut(); osal_task_delay_us(15000000); ink_ui_post(r5_dump_ui,NULL);
        r6_audio_restore(); osal_task_delay_us(2500000);
        vtk_lock(); r4_ai_down(NULL); vtk_event_after(SIM_MS(1000),r4_ai_up,NULL); vtk_unlock();
        osal_task_delay_us(16000000); ink_ui_post(r5_dump_ui,NULL);
    }
    osal_task_delay_us(1000000);
    sim_note("AUD7 AI answers=%.0f speech_failed=%.0f output_failed=%.0f played=%.0f",sim_metric_value("ai.answers"),
        sim_metric_value("ai.speech_failed"),sim_metric_value("audio.play_failed"),sim_audio_played_ms());
    sim_display_snapshot("r6-ai-final");
}
static void r6_main(void) {
    const char *m=getenv("AUDIT_PROBE");
    if(!m || strncmp(m,"r6_",3)) {r5_main(); return;}
    if(!strncmp(m,"r6_ai_",6)) {r6_ai(m); return;}
    osal_task_delay_us(1600000);
    sim_note("AUD7 begin %s",m);
    if(!strncmp(m,"r6_audio_",9)) r6_audio(m); else r6_display(m);
    sim_note("AUD7 end %s",m);
}
