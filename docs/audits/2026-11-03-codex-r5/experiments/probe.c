/* Fifth-round audit harness: unchanged product libraries, legacy probes retained. */
#include "legacy_probe.c"
#include "ink/rotation.h"
#include "ink/hal_net.h"
#ifndef AUDIT_BEFORE
static uint8_t r5_visible[1216 * 684];
static unsigned r5_callbacks, r5_successes, r5_failures;
static double r5_mean(int x, int y, int w, int h) {
    int lw=0, lh=0;
    if (!sim_display_visible(r5_visible,sizeof r5_visible,&lw,&lh)) return -1;
    double sum=0;
    hal_panel_info_t pi;
    hal_display_get_info(&pi);
    for(int j=y;j<y+h;j++) for(int i=x;i<x+w;i++) {
        int lx,ly;
        ink_native_to_logical(pi.width,pi.height,pi.rotation_deg,i,j,&lx,&ly);
        sum+=r5_visible[(size_t)ly*lw+lx];
    }
    return sum/(w*h);
}
static void r5_done(void *ctx, const hal_display_result_t *r) {
    (void)ctx;
    r5_callbacks++; r5_successes+=r->ok; r5_failures+=!r->ok;
    sim_note("AUD6 display id=%u ok=%d cb=%u pending=%d left_mean=%.2f right_mean=%.2f time_us=%llu",r->id,r->ok,
        r5_callbacks,hal_display_pending(),r5_mean(0,0,128,128),r5_mean(160,0,128,128),(unsigned long long)osal_now_us());
}
static uint32_t r5_update(int x,int gray,hal_waveform_t wf,bool wait) {
    static uint8_t px[128*128];
    memset(px,gray,sizeof px);
    hal_rect_t a={x,0,128,128};
    uint32_t id=hal_display_update(&a,px,128,wf,0,r5_done,NULL);
    if(wait) poll_display(6000000);
    return id;
}
static void r5_cut_task(void *arg) {
    (void)arg;
    while(!sim_display_playing()) osal_task_delay_us(1000);
    osal_task_delay_us(20000);
    metalio_board_set(0,MB_P0_CAM_SCR,0);
    sim_note("AUD6 cut P05 during drive");
}
static void r5_cut_next(void) {
    osal_task_cfg_t c={.name="r5-panel-cut",.fn=r5_cut_task,.priority=10,.core=-1,.stack_bytes=4096};
    osal_task_create(&c,NULL);
}
static void r5_display(const char *mode) {
    if(!strcmp(mode,"r5_gc16_powerup_fail")) {
        metalio_board_set(0,MB_P0_CAM_SCR,0);
        r5_update(0,0,HAL_WF_GC16,true);
        metalio_board_set(0,MB_P0_CAM_SCR,MB_P0_CAM_SCR);
        r5_update(160,0,HAL_WF_GL16,true);
    } else {
        r5_cut_next();
        r5_update(0,0,HAL_WF_GL16,true);
        metalio_board_set(0,MB_P0_CAM_SCR,MB_P0_CAM_SCR);
        if(!strcmp(mode,"r5_redraw_twice")) {
            r5_cut_next();
            r5_update(0,0,HAL_WF_GL16,true);
            metalio_board_set(0,MB_P0_CAM_SCR,MB_P0_CAM_SCR);
            r5_update(0,0,HAL_WF_GL16,true);
        } else if(!strcmp(mode,"r5_redraw_gc16_other")) {
            r5_update(160,0,HAL_WF_GC16,true);
            sim_display_snapshot("after-gc16-other");
            sim_note("AUD6 after GC16 other redraws=%.0f left_mean=%.2f",sim_metric_value("display.redraws_after_supply_loss"),r5_mean(0,0,128,128));
            r5_update(160,0,HAL_WF_GL16,true);
        } else {
            r5_update(0,0,HAL_WF_GL16,false);
            osal_task_delay_us(80000);
            r5_update(0,255,HAL_WF_GL16,false);
            r5_update(160,0,HAL_WF_GL16,false);
            poll_display(6000000);
        }
    }
    sim_display_snapshot("r5-final");
    sim_note("AUD6 display final cb=%u ok=%u fail=%u pending=%d playing=%d redraws=%.0f left=%.2f right=%.2f",r5_callbacks,
        r5_successes,r5_failures,hal_display_pending(),sim_display_playing(),sim_metric_value("display.redraws_after_supply_loss"),
        r5_mean(0,0,128,128),r5_mean(160,0,128,128));
}
static unsigned r5_audio_failed;
static int r5_audio_failed_id;
static uint64_t r5_audio_failed_us;
static void r5_audio_failure(void *ctx,int id) {
    (void)ctx; r5_audio_failed++; r5_audio_failed_id=id; r5_audio_failed_us=osal_now_us();
    /* Re-enter a public audio API: detects callback-under-lock regressions. */
    ink_audio_stream_abort(-1);
    sim_note("AUD6 stream failed id=%d owner=%s at_us=%llu",id,sim_current_owner(),(unsigned long long)r5_audio_failed_us);
}
static int r5_stream(int n,bool finish) {
    static const uint8_t f[8]={0xA5,0x00,0x10,0x30,0x11,0,0,0};
    int id=ink_audio_stream_open(INK_AUDIO_PRIO_VOICE,osal_now_us(),NULL), pushed=0;
    for(int i=0;i<n;i++) pushed+=ink_audio_stream_push(id,f,sizeof f)==0;
    if(finish) ink_audio_stream_finish(id);
    sim_note("AUD6 stream opened id=%d pushed=%d finish=%d at_us=%llu",id,pushed,finish,(unsigned long long)osal_now_us());
    return id;
}
static void r5_audio_state(const char *tag) {
    sim_note("AUD6 audio %s played_ms=%.0f busy=%d mode=%d clock=%d power=%d failures=%u failed_id=%d storage_pending=%d at_us=%llu",tag,
        sim_audio_played_ms(),sim_audio_busy(),sim_bt_audio_mode(),sim_bt_audio_clock(),sim_board_shared_power(),
        r5_audio_failed,r5_audio_failed_id,ink_storage_pending(),(unsigned long long)osal_now_us());
}
static void r5_audio(const char *mode) {
    if(ink_display_init() || ink_storage_init() || ink_power_init() || ink_audio_init()) osal_panic("r5 init");
    ink_audio_on_stream_failed(r5_audio_failure,NULL);
    if(!strcmp(mode,"r5_service_midcut")) {
        r5_stream(50,true);
        uint64_t limit=osal_now_us()+2000000;
        while(sim_audio_played_ms()<120 && osal_now_us()<limit) osal_task_delay_us(10000);
        r5_audio_state("before-cut");
        metalio_board_set(1,MB_P1_SHARED_PWR,0);
        osal_task_delay_us(1000000);
        metalio_board_set(1,MB_P1_SHARED_PWR,MB_P1_SHARED_PWR);
        ink_storage_write_atomic("/user/r5_audio.txt","saved",5);
        osal_task_delay_us(10000000);
        r5_audio_state("10s-after-restore");
        ink_audio_stop_all();
        osal_task_delay_us(1000000);
        r5_audio_state("after-stop");
        r5_stream(5,true);
        osal_task_delay_us(2000000);
        r5_audio_state("new-stream");
    } else {
        metalio_board_set(1,MB_P1_SHARED_PWR,0);
        r5_stream(8,true);
        osal_task_delay_us(1200000);
        ink_storage_write_atomic("/user/r5_audio.txt","saved",5);
        r5_audio_state("waiting");
        if(!strcmp(mode,"r5_service_replace")) {
            osal_task_delay_us(4800000);
            r5_stream(5,true);
            osal_task_delay_us(3000000);
            r5_audio_state("replacement-after-3s");
            metalio_board_set(1,MB_P1_SHARED_PWR,MB_P1_SHARED_PWR);
            osal_task_delay_us(3000000);
        } else {
            osal_task_delay_us(8500000);
            metalio_board_set(1,MB_P1_SHARED_PWR,MB_P1_SHARED_PWR);
            osal_task_delay_us(1000000);
        }
        r5_audio_state("finished");
    }
    hal_sleep_cfg_t sc={.max_us=100000,.wake_mask=HAL_WAKE_TIMER,.keep_modem=true};
    sim_note("AUD6 audio final sleep=%u",hal_power_sleep(&sc));
}
static void r5_display_service(void) {
    if(ink_display_init()) osal_panic("r5 display init");
    metalio_board_set(0,MB_P0_CAM_SCR,0);
    ink_display_lock();
    int st=ink_display_stride();
    for(int y=0;y<128;y++) memset(ink_display_fb()+(size_t)y*st,0,128);
    ink_display_unlock();
    ink_region_t r={.rect={0,0,128,128},.intent=INK_REFRESH_TEXT};
    ink_display_commit(&r,1,osal_now_us());
    osal_task_delay_us(5000000);
    sim_note("AUD6 service P05-off busy=%d pending=%d power_failures=%.0f",ink_display_busy(),hal_display_pending(),sim_metric_value("display.power_failures"));
    metalio_board_set(0,MB_P0_CAM_SCR,MB_P0_CAM_SCR);
    ink_display_commit(&r,1,osal_now_us());
    uint64_t limit=osal_now_us()+20000000;
    osal_task_delay_us(100000);
    while(ink_display_busy() && osal_now_us()<limit) osal_task_delay_us(100000);
    sim_note("AUD6 service restored busy=%d pending=%d left_mean=%.2f",ink_display_busy(),hal_display_pending(),r5_mean(0,0,128,128));
}
static void r5_wait_service(const char *tag) {
    uint64_t t=osal_now_us(),limit=t+22000000;
    osal_task_delay_us(100000);
    while(ink_display_busy() && osal_now_us()<limit) osal_task_delay_us(100000);
    sim_note("AUD6 service %s waited_us=%llu busy=%d pending=%d redraws=%.0f left=%.2f right=%.2f",tag,
        (unsigned long long)(osal_now_us()-t),ink_display_busy(),hal_display_pending(),
        sim_metric_value("display.redraws_after_supply_loss"),r5_mean(0,0,128,128),r5_mean(160,0,128,128));
}
static void r5_service_redraw(bool fault) {
    if(ink_display_init()) osal_panic("r5 display init");
    int w,h; ink_display_native_size(&w,&h);
    ink_display_lock(); memset(ink_display_fb(),0,(size_t)w*h); ink_display_unlock();
    ink_region_t all={.rect={0,0,w,h},.intent=INK_REFRESH_FULL};
    if(fault) r5_cut_next();
    ink_display_commit(&all,1,osal_now_us());
    r5_wait_service("initial");
    if(fault) {
        metalio_board_set(0,MB_P0_CAM_SCR,MB_P0_CAM_SCR);
        ink_display_commit(&all,1,osal_now_us());
        r5_wait_service("recovery-full");
    }
    ink_display_lock();
    for(int y=0;y<128;y++) memset(ink_display_fb()+(size_t)y*w,255,128);
    ink_display_unlock();
    ink_region_t a={.rect={0,0,128,128},.intent=INK_REFRESH_TEXT};
    ink_display_commit(&a,1,osal_now_us());
    r5_wait_service("normal-small");
    sim_display_snapshot("service-redraw-final");
}
static void r5_labels(lv_obj_t *o) {
    if(lv_obj_check_type(o,&lv_label_class)) {
        const char *s=lv_label_get_text(o);
        if(strstr(s,"声音") || strstr(s,"回答完成") || strstr(s,"没听到") || strstr(s,"麦克风") || strstr(s,"正在想"))
            sim_note("AUD6 UI label=%s",s);
    }
    unsigned n=lv_obj_get_child_count(o);
    for(unsigned i=0;i<n;i++) r5_labels(lv_obj_get_child(o,i));
}
static void r5_dump_ui(void *ctx) { (void)ctx; r5_labels(lv_screen_active()); r5_labels(lv_layer_top()); }
static void r5_ai(const char *mode) {
    product_main();
    hal_net_use(HAL_LINK_WIFI); /* simulated Wi-Fi keeps the hub reachable while P14 is off */
    osal_task_delay_us(4000000);
    vtk_lock(); r4_ai_down(NULL); vtk_event_after(SIM_MS(1000),r4_ai_up,NULL); vtk_unlock();
    osal_task_delay_us(1300000);
    bool fault=strcmp(mode,"r5_ai_normal")!=0;
    if(fault) metalio_board_set(1,MB_P1_SHARED_PWR,0);
    if(!strcmp(mode,"r5_ai_speaker_recover")) {
        osal_task_delay_us(4000000);
        metalio_board_set(1,MB_P1_SHARED_PWR,MB_P1_SHARED_PWR);
        osal_task_delay_us(8000000);
    } else if(fault) {
        for(int i=0;i<12;i++) {
            osal_task_delay_us(1000000);
            sim_note("AUD6 AI tick answers=%.0f speech_failed=%.0f",sim_metric_value("ai.answers"),sim_metric_value("ai.speech_failed"));
            ink_ui_post(r5_dump_ui,NULL);
        }
    } else osal_task_delay_us(12000000);
    ink_ui_post(r5_dump_ui,NULL);
    osal_task_delay_us(1000000);
    ink_ui_post(r5_dump_ui,NULL);
    osal_task_delay_us(200000);
    sim_display_snapshot("ai-final");
    sim_note("AUD6 AI end fault=%d answers=%.0f speech_failed=%.0f output_failed=%.0f audio_ms=%.0f",fault,
        sim_metric_value("ai.answers"),sim_metric_value("ai.speech_failed"),sim_metric_value("audio.play_failed"),sim_audio_played_ms());
}
#endif
void ink_main(void) {
#ifndef AUDIT_BEFORE
    const char *mode=getenv("AUDIT_PROBE");
    if(mode && !strncmp(mode,"r5_",3)) {
        if(!strncmp(mode,"r5_ai_",6)) { r5_ai(mode); return; }
        osal_task_delay_us(1600000);
        sim_note("AUD6 begin %s",mode);
        if(!strcmp(mode,"r5_system_redraw")) r5_service_redraw(true);
        else if(!strcmp(mode,"r5_system_redraw_control")) r5_service_redraw(false);
        else if(!strncmp(mode,"r5_service_",11)) r5_audio(mode);
        else if(!strcmp(mode,"r5_display_service")) r5_display_service();
        else r5_display(mode);
        sim_note("AUD6 end %s",mode);
        return;
    }
#endif
    r4_main();
}
