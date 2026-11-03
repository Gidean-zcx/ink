/* Third-round read-only experiments; product sources and profiles are unmodified. */
#define ink_main replay_main
#include "../../2026-11-03-codex-r3/experiments/extra_probe.c"
#undef ink_main
#include "metalio_plus/metalio_epd_power.h"
#include "tps65185/tps65185.h"

static int cut_kind;
static bool cut_complete;
static unsigned cb_count[64], cb_ok[64];
static uint64_t last_cb_us;

static void counted_done(void *ctx, const hal_display_result_t *r) {
    (void)ctx;
    if (r->id < 64) { cb_count[r->id]++; cb_ok[r->id] += r->ok; }
    last_cb_us = osal_now_us();
    sim_note("AUD4 display callback id=%u ok=%d count=%u ready=%d pending=%d", r->id, r->ok,
             r->id < 64 ? cb_count[r->id] : 999, sim_tps65185_panel_ready(), hal_display_pending());
}

static void panel_cut_restore(void *arg) {
    (void)arg;
    while (!sim_display_playing()) osal_task_delay_us(1000);
    osal_task_delay_us(20000); /* several phases into the operation */
    if (cut_kind == 3) {
        metalio_board_set(0, MB_P0_CAM_SCR, 0);
        sim_note("AUD4 P05 cut rail=%d panel_ready=%d",sim_board_cam_power(),sim_tps65185_panel_ready());
    } else if (cut_kind == 1) {
        metalio_board_set(0, EPD_PWR_VCOM_CTRL, 0);
        sim_note("AUD4 panel VCOM cut ready=%d", sim_tps65185_panel_ready());
        osal_task_delay_us(20000);
        metalio_board_set(0, EPD_PWR_VCOM_CTRL, EPD_PWR_VCOM_CTRL);
    } else {
        metalio_board_set(0, EPD_PWR_PWRUP, 0);
        sim_note("AUD4 panel rails cut ready=%d", sim_tps65185_panel_ready());
        if (cut_kind == 2) {
            osal_task_delay_us(20000);
            metalio_board_set(0, EPD_PWR_PWRUP, EPD_PWR_PWRUP);
            hal_i2c_dev_t *pmic = hal_i2c_device(0, TPS65185_ADDR, "pmic");
            tps65185_write(pmic, TPS_REG_ENABLE, 0);
            tps65185_write(pmic, TPS_REG_ENABLE, TPS_ENABLE_ALL_RAILS);
            osal_task_delay_us(40000);
        }
    }
    sim_note("AUD4 panel cut task finished ready=%d playing=%d", sim_tps65185_panel_ready(), sim_display_playing());
    cut_complete = true;
}

static void poll_display(uint64_t limit_us) {
    uint64_t end = osal_now_us() + limit_us;
    while (hal_display_pending() && osal_now_us() < end) osal_task_delay_us(10000);
}

static void audio_after_restore(void) {
    int r = hal_audio_capture_start(mic, NULL);
    sim_note("AUD4 capture start rc=%d mode=%d clock=%d ready=%d", r, sim_bt_audio_mode(), sim_bt_audio_clock(), metalio_board_ready());
    osal_task_delay_us(2500000);
    sim_note("AUD4 capture observed samples=%u mode=%d clock=%d power=%d busy=%d", mic_samples,
             sim_bt_audio_mode(), sim_bt_audio_clock(), sim_board_shared_power(), sim_audio_busy());
    hal_audio_capture_stop();
    hal_sleep_cfg_t sc = {.max_us=100000, .wake_mask=HAL_WAKE_POWER_KEY, .keep_modem=true};
    sim_note("AUD4 sleep after stop cause=%u busy=%d", hal_power_sleep(&sc), sim_audio_busy());
}

static void start_play_after_sleep(void *arg) {
    (void)arg;
    osal_task_delay_us(20000);
    int r = hal_audio_play_start();
    sim_note("AUD4 playback start rc=%d mode=%d clock=%d",r,sim_bt_audio_mode(),sim_bt_audio_clock());
}

void legacy_main(void) {
    const char *mode = getenv("AUDIT_PROBE");
    if (!mode || strncmp(mode, "r3_", 3)) { replay_main(); return; }
    osal_task_delay_us(1600000);
    sim_note("AUD4 begin %s driver=%d", mode, g_profile.display_driver);
    if (!strncmp(mode, "r3_display_", 11)) {
        cut_kind = !strcmp(mode,"r3_display_vcom") ? 1 : !strcmp(mode,"r3_display_transient") ? 2 : !strcmp(mode,"r3_display_p05") ? 3 : 0;
        static uint8_t pixels[128 * 128];
        hal_rect_t a = {0,0,128,128};
        sim_display_snapshot("before");
        osal_task_cfg_t c = {.name="cut-restore",.fn=panel_cut_restore,.priority=10,.core=-1,.stack_bytes=4096};
        osal_task_create(&c,NULL);
        unsigned n = cut_kind ? 1 : 4;
        for (unsigned i=0;i<n;i++) { a.x = (int)i * 160; hal_display_update(&a,pixels,128,HAL_WF_GL16,0,counted_done,NULL); }
        poll_display(8000000);
        while (!cut_complete) osal_task_delay_us(10000);
        sim_display_snapshot("after-cut");
        sim_note("AUD4 after cut pending=%d playing=%d last_callback_us=%llu",hal_display_pending(),sim_display_playing(),(unsigned long long)last_cb_us);
        a.x=0;
        hal_display_update(&a,pixels,128,HAL_WF_GL16,0,counted_done,NULL);
        poll_display(8000000);
        sim_display_snapshot("after-retry");
        sim_note("AUD4 after retry pending=%d ready=%d",hal_display_pending(),sim_tps65185_panel_ready());
    } else if (!strcmp(mode,"r3_wake_touch") || !strcmp(mode,"r3_wake_audio") || !strcmp(mode,"r3_wake_permanent")) {
        bool touch = !strcmp(mode,"r3_wake_touch");
        bool permanent = !strcmp(mode,"r3_wake_permanent");
        hal_input_set_sink(sink,NULL);
        vtk_lock();
        vtk_event_after(SIM_MS(400),power_down_event,NULL);
        vtk_event_after(SIM_MS(401),fault_on,NULL);
        vtk_event_after(SIM_MS(450),power_up_event,NULL);
        if (touch) vtk_event_after(SIM_MS(470),touch_down_event,NULL);
        if (!permanent) vtk_event_after(SIM_MS(touch ? 650 : 1400),fault_off,NULL);
        vtk_unlock();
        hal_sleep_cfg_t sc = {.max_us=1000000,.wake_mask=HAL_WAKE_POWER_KEY,.keep_modem=touch || permanent};
        uint32_t cause = hal_power_sleep(&sc);
        sim_note("AUD4 wake returned cause=%u cfg=%02x,%02x keys=%u power=%d",cause,sim_tca_config(0),sim_tca_config(1),keys,sim_board_shared_power());
        if (!touch && !permanent) audio_after_restore();
        else {
            osal_task_delay_us(2500000);
            if (touch)
                sim_note("AUD4 after bus recovered while touch held cfg=%02x,%02x keys=%u",sim_tca_config(0),sim_tca_config(1),keys);
            else
                sim_note("AUD4 persistent bus fault cfg=%02x,%02x keys=%u",sim_tca_config(0),sim_tca_config(1),keys);
            vtk_lock();
            vtk_event_after(SIM_MS(10),volume_down,NULL);
            vtk_event_after(SIM_MS(200),volume_up,NULL);
            vtk_unlock();
            osal_task_delay_us(500000);
            sim_note("AUD4 after volume cfg=%02x,%02x keys=%u",sim_tca_config(0),sim_tca_config(1),keys);
            if (touch) {
                vtk_lock(); touch_up_event(NULL); vtk_unlock();
                osal_task_delay_us(500000);
                sim_note("AUD4 after touch release cfg=%02x,%02x keys=%u",sim_tca_config(0),sim_tca_config(1),keys);
            }
        }
    } else if (!strcmp(mode,"r3_audio_oom")) {
        static void *held[4096];
        int n=0;
        while (n<4096 && (held[n]=osal_malloc(1024,OSAL_MEM_INTERNAL))) n++;
        int cap=hal_audio_capture_start(mic,NULL), play=hal_audio_play_start();
        for (int i=0;i<n;i++) osal_free(held[i]);
        hal_sleep_cfg_t sc={.max_us=100000,.wake_mask=HAL_WAKE_POWER_KEY,.keep_modem=true};
        sim_note("AUD4 allocation failure blocks=%d capture=%d play=%d busy=%d sleep_cause=%u",n,cap,play,sim_audio_busy(),hal_power_sleep(&sc));
    } else if (!strcmp(mode,"r3_play_sleep_entry")) {
        metalio_board_set(1,MB_P1_SHARED_PWR,0);
        metalio_board_set(1,MB_P1_SHARED_PWR,MB_P1_SHARED_PWR);
        osal_task_cfg_t c={.name="play-start",.fn=start_play_after_sleep,.priority=8,.core=-1,.stack_bytes=4096};
        osal_task_create(&c,NULL);
        hal_sleep_cfg_t sc={.max_us=1000000,.wake_mask=HAL_WAKE_POWER_KEY};
        uint32_t cause=hal_power_sleep(&sc);
        osal_task_delay_us(2000000);
        static int16_t pcm[2400];
        int n=hal_audio_play_write(pcm,2400,500000);
        hal_audio_play_stop(true);
        uint32_t after=hal_power_sleep(&sc);
        sim_note("AUD4 playback entry cause=%u written=%d busy=%d after_stop_sleep=%u",cause,n,sim_audio_busy(),after);
    } else if (!strcmp(mode,"r3_boot_audio")) {
        sim_note("AUD4 boot recovery before ready=%d",metalio_board_ready());
        hal_input_set_sink(sink,NULL);
        sim_i2c_set_faults(0,NULL);
        vtk_lock();
        vtk_event_after(SIM_MS(20),power_down_event,NULL);
        vtk_event_after(SIM_MS(200),power_up_event,NULL);
        vtk_unlock();
        osal_task_delay_us(1800000);
        sim_note("AUD4 boot recovery after ready=%d keys=%u mode=%d",metalio_board_ready(),keys,sim_bt_audio_mode());
        audio_after_restore();
    }
    sim_note("AUD4 end %s",mode);
}
