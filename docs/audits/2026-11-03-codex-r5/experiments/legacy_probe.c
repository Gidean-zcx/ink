/* Audit-only experiments; all product implementations are linked unchanged. */
#include "../../2026-11-03-codex-r4/experiments/legacy_probe.c"
#include "ink/hal_codec.h"
#include "ink/sys/audio.h"
#include "ink/sys/power.h"
#define ink_main product_main
#include "../../../../system/boot/main.c"
#undef ink_main

static uint8_t r4_pixels[128 * 128];
static unsigned r4_n, r4_ok;
static void r4_done(void *ctx, const hal_display_result_t *r) {
    (void)ctx;
    r4_n++;
    r4_ok += r->ok;
    sim_note("AUD5 display id=%u ok=%d p05=%d ready=%d latency_us=%llu", r->id, r->ok,
             sim_board_cam_power(), sim_tps65185_panel_ready(),
             (unsigned long long)(r->done_us - r->started_us));
}
static void r4_update(int gray) {
    memset(r4_pixels, gray, sizeof r4_pixels);
    hal_rect_t a = {0,0,128,128};
    unsigned before = r4_n;
    uint64_t t = osal_now_us();
    uint32_t id = hal_display_update(&a,r4_pixels,128,HAL_WF_GL16,0,r4_done,NULL);
    poll_display(5000000);
    sim_note("AUD5 update waited id=%u callbacks=%u elapsed_us=%llu",id,r4_n-before,
             (unsigned long long)(osal_now_us()-t));
}
static void r4_activity(void *ctx) {
    (void)ctx;
    while (!sim_display_playing()) osal_task_delay_us(1000);
    hal_haptic_pulse(50);
    int p = hal_audio_play_start();
    int16_t pcm[960] = {0};
    int w = hal_audio_play_write(pcm,960,200000);
    hal_audio_play_stop(true);
    uint8_t *frame = osal_malloc(GC2145_FRAME_BYTES, OSAL_MEM_EXTERNAL|OSAL_MEM_DMA);
    int cam = hal_camera_capture(frame,GC2145_FRAME_BYTES,2000);
    osal_free(frame);
    sim_note("AUD5 parallel peripheral play=%d written=%d camera=%d",p,w,cam);
}
static void r4_ai_down(void *arg) { (void)arg; sim_input_key(HAL_KEY_AI,true); }
static void r4_ai_up(void *arg) { (void)arg; sim_input_key(HAL_KEY_AI,false); }
static void r4_audio_apis(void) {
    unsigned before = mic_samples;
    uint64_t t = osal_now_us();
    int c = hal_audio_capture_start(mic,NULL);
    int p = hal_audio_play_start();
    static int16_t pcm[2400];
    int w = hal_audio_play_write(pcm,2400,500000);
    osal_task_delay_us(500000);
    hal_audio_capture_stop();
    hal_audio_play_stop(true);
    sim_note("AUD5 audio cap=%d play=%d write=%d samples=%u mode=%d clock=%d elapsed_us=%llu busy=%d",c,p,w,
             mic_samples-before,sim_bt_audio_mode(),sim_bt_audio_clock(),
             (unsigned long long)(osal_now_us()-t),sim_audio_busy());
}
static void r4_long_wake(bool finger) {
    hal_input_set_sink(sink,NULL);
    vtk_lock();
    vtk_event_after(SIM_MS(400),power_down_event,NULL);
    vtk_event_after(SIM_MS(401),fault_on,NULL);
    vtk_event_after(SIM_MS(450),power_up_event,NULL);
    if (finger) vtk_event_after(SIM_MS(470),touch_down_event,NULL);
    vtk_event_after(SIM_MS(1400),fault_off,NULL);
    vtk_unlock();
    hal_sleep_cfg_t sc={.max_us=1000000,.wake_mask=HAL_WAKE_POWER_KEY,.keep_modem=false};
    uint32_t why=hal_power_sleep(&sc);
    sim_note("AUD5 long wake why=%u cfg=%02x,%02x p14=%d",why,sim_tca_config(0),sim_tca_config(1),sim_board_shared_power());
    int c=hal_audio_capture_start(mic,NULL), p=hal_audio_play_start();
    sim_note("AUD5 pending restore starts cap=%d play=%d busy=%d",c,p,sim_audio_busy());
    hal_audio_capture_stop(); hal_audio_play_stop(false);
    osal_task_delay_us(2500000);
    vtk_lock();
    vtk_event_after(SIM_MS(10),volume_down,NULL);
    vtk_event_after(SIM_MS(200),volume_up,NULL);
    vtk_unlock();
    osal_task_delay_us(500000);
    sim_note("AUD5 long held restore cfg=%02x,%02x keys=%u p14=%d",sim_tca_config(0),sim_tca_config(1),keys,sim_board_shared_power());
    if (finger) { vtk_lock(); touch_up_event(NULL); vtk_unlock(); }
    r4_audio_apis();
    sim_note("AUD5 after long restore sleep=%u",hal_power_sleep(&sc));
}
static void r4_frame(void *ctx, const uint8_t *data,size_t n,uint32_t seq) {
    (void)ctx;(void)data;(void)seq;
    sim_note("AUD5 service capture packet bytes=%zu",n);
}
static void r4_service_audio(bool failed) {
    if(ink_display_init() || ink_storage_init() || ink_power_init() || ink_audio_init()) osal_panic("r4 service init");
    if(failed) metalio_board_set(1,MB_P1_SHARED_PWR,0);
    int cap=ink_audio_capture_begin(r4_frame,NULL);
    sim_note("AUD5 service capture begin=%d",cap);
    if(!cap) { osal_task_delay_us(120000); ink_audio_capture_end(); }
    hal_opus_enc_t *enc=hal_opus_enc_new(24000,0);
    static int16_t pcm[HAL_OPUS_FRAME_SAMPLES];
    uint8_t pkt[HAL_OPUS_MAX_PACKET];
    int n=hal_opus_encode(enc,pcm,pkt,sizeof pkt);
    int id=ink_audio_stream_open(INK_AUDIO_PRIO_VOICE,osal_now_us(),"audit.play_heard_ms");
    for(int k=0;k<8;k++) {
        int rc=ink_audio_stream_push(id,pkt,n);
        sim_note("AUD5 service push k=%d id=%d rc=%d",k,id,rc);
    }
    ink_audio_stream_finish(id);
    hal_opus_enc_free(enc);
    osal_task_delay_us(1000000);
    sim_note("AUD5 service before restore busy=%d",sim_audio_busy());
    if(failed) metalio_board_set(1,MB_P1_SHARED_PWR,MB_P1_SHARED_PWR);
    osal_task_delay_us(2500000);
    sim_note("AUD5 service after restore busy=%d mode=%d",sim_audio_busy(),sim_bt_audio_mode());
}
void r4_main(void) {
    const char *mode=getenv("AUDIT_PROBE");
    if(!mode || strncmp(mode,"r4_",3)) { legacy_main(); return; }
    if(!strcmp(mode,"r4_ai_failed")) {
        product_main();
        osal_task_delay_us(2200000);
        metalio_board_set(1,MB_P1_SHARED_PWR,0);
        vtk_lock();
        vtk_event_after(SIM_MS(10),r4_ai_down,NULL);
        vtk_event_after(SIM_MS(700),r4_ai_up,NULL);
        vtk_unlock();
        osal_task_delay_us(2000000);
        metalio_board_set(1,MB_P1_SHARED_PWR,MB_P1_SHARED_PWR);
        osal_task_delay_us(8000000);
        sim_note("AUD5 AI failure end");
        return;
    }
    osal_task_delay_us(1600000);
    sim_note("AUD5 begin %s",mode);
    if(!strcmp(mode,"r4_normal") || !strcmp(mode,"r4_normal_activity")) {
        if(!strcmp(mode,"r4_normal_activity")) {
            osal_task_cfg_t t={.name="r4-peripheral",.fn=r4_activity,.stack_bytes=8192,.priority=10,.core=-1};
            osal_task_create(&t,NULL);
        }
        for(int i=0;i<6;i++) { r4_update(i&1?255:0); osal_task_delay_us(30000); }
        r4_audio_apis();
        hal_sleep_cfg_t sc={.max_us=100000,.wake_mask=HAL_WAKE_TIMER,.keep_modem=false};
        for(int i=0;i<3;i++) {
            uint32_t why=hal_power_sleep(&sc);
            sim_note("AUD5 normal sleep i=%d why=%u cfg=%02x,%02x",i,why,sim_tca_config(0),sim_tca_config(1));
            r4_update(i&1?255:0);
        }
        r4_audio_apis();
        sim_note("AUD5 normal total callbacks=%u ok=%u",r4_n,r4_ok);
    } else if(!strcmp(mode,"r4_held_supply")) {
        hal_display_hold_power(1000);
        r4_update(0);
        hal_haptic_pulse(50);
        metalio_board_set(0,MB_P0_AMP,MB_P0_AMP);
        osal_task_delay_us(80000);
        metalio_board_set(0,MB_P0_AMP,0);
        r4_update(255);
        metalio_board_set(0,MB_P0_CAM_SCR,0);
        osal_task_delay_us(20000);
        metalio_board_set(0,MB_P0_CAM_SCR,MB_P0_CAM_SCR);
        r4_update(0);
        metalio_board_set(0,EPD_PWR_VCOM_CTRL,0);
        osal_task_delay_us(20000);
        metalio_board_set(0,EPD_PWR_VCOM_CTRL,EPD_PWR_VCOM_CTRL);
        r4_update(255);
        hal_display_hold_power(0);
        sim_note("AUD5 held supply callbacks=%u ok=%u",r4_n,r4_ok);
    } else if(!strcmp(mode,"r4_p05_preoff")) {
        metalio_board_set(0,MB_P0_CAM_SCR,0);
        sim_display_snapshot("p05-before");
        r4_update(0); r4_update(255); r4_update(0);
        sim_display_snapshot("p05-unpowered");
        metalio_board_set(0,MB_P0_CAM_SCR,MB_P0_CAM_SCR);
        r4_update(0);
        sim_display_snapshot("p05-restored");
    } else if(!strcmp(mode,"r4_touch_restore")) {
        r4_long_wake(true);
    } else if(!strcmp(mode,"r4_audio_restore")) {
        r4_long_wake(false);
    } else if(!strcmp(mode,"r4_gpio2_accel") || !strcmp(mode,"r4_gpio2_normal")) {
        hal_input_set_sink(sink,NULL);
        bool fault=!strcmp(mode,"r4_gpio2_accel");
        if(fault) sim_i2c_set_faults(1,"accel");
        vtk_lock(); sim_accel_orient("landscape_left"); vtk_unlock();
        osal_task_delay_us(5000000);
        sim_note("AUD5 gpio2 before recover low=%d",sim_gpio2_low());
        sim_i2c_set_faults(0,NULL);
        osal_task_delay_us(1200000);
        vtk_lock(); volume_down(NULL); vtk_event_after(SIM_MS(200),volume_up,NULL); vtk_unlock();
        osal_task_delay_us(400000);
        sim_note("AUD5 gpio2 recovered low=%d keys=%u",sim_gpio2_low(),keys);
    } else if(!strcmp(mode,"r4_tca_init_change")) {
#ifndef AUDIT_BEFORE
        sim_epd_chips_init(); /* the display normally attaches TPS lazily on its first update */
        epd_power_t ep={.ioexp=metalio_ioexp(),.io_lock=metalio_board_lock,.io_unlock=metalio_board_unlock,
            .pmic=hal_i2c_device(0,TPS65185_ADDR,"pmic"),.vcom_mv=METALIO_EPD_VCOM_MV};
        int rc=epd_power_on(&ep);
        uint8_t orig[2]={metalio_board_output(0),metalio_board_output(1)};
        uint8_t off[2]={(uint8_t)(orig[0]&~EPD_PWR_VCOM_CTRL),orig[1]}, dir[2]={0xc0,0xe3};
        metalio_board_lock();
        tca9555_init(metalio_ioexp(),hal_i2c_device(0,TCA9555_ADDR_BASE,"io-expander"),off,dir);
        metalio_board_unlock();
        sim_note("AUD5 tca init supply on_rc=%d ready=%d touched=%d",rc,sim_tps65185_panel_ready(),epd_power_pins_touched(&ep));
        osal_task_delay_us(20000);
        metalio_board_lock();
        tca9555_init(metalio_ioexp(),hal_i2c_device(0,TCA9555_ADDR_BASE,"io-expander"),orig,dir);
        metalio_board_unlock();
        sim_note("AUD5 tca init restored ready=%d touched=%d kept=%d",sim_tps65185_panel_ready(),epd_power_pins_touched(&ep),epd_power_kept(&ep));
        epd_power_off(&ep);
#endif
    } else if(!strcmp(mode,"r4_service_failed") || !strcmp(mode,"r4_service_normal")) {
        r4_service_audio(!strcmp(mode,"r4_service_failed"));
    }
    sim_note("AUD5 end %s",mode);
}
