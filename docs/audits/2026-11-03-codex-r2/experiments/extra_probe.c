/* Audit-only harness; links the fixed checkout without editing product files. */
#include "probe.c"
#include "ink/hal_device.h"

static osal_timer_t *rearm_timer;
static osal_sem_t *callback_entered;
static int timer_calls;

static void rearm_cb(void *arg) {
    (void)arg;
    timer_calls++;
    if (timer_calls == 1) {
        osal_sem_give(callback_entered);
        /* CPU work, not a blocking call: let the higher-priority deleting task run. */
        sim_charge_ns_always(SIM_US(100));
        osal_timer_start(rearm_timer, 100000, 0);
        sim_note("AUD3 timer callback rearmed its timer");
    } else {
        sim_note("AUD3 callback AFTER deletion calls=%d", timer_calls);
    }
}

static void delete_timer_task(void *arg) {
    (void)arg;
    osal_sem_take(callback_entered, OSAL_WAIT_FOREVER);
    sim_note("AUD3 timer delete begins");
    osal_timer_delete(rearm_timer);
    sim_note("AUD3 timer delete returned calls=%d", timer_calls);
}

static void cut_panel_task(void *arg) {
    (void)arg;
    while (!sim_display_playing()) osal_task_delay_us(1000);
    int r = metalio_board_set(0, 7, 0);
    sim_note("AUD3 cut panel during waveform rc=%d ready=%d", r, sim_tps65185_panel_ready());
}

static uint64_t audio_start_delay;
static void capture_task(void *arg) {
    (void)arg;
    if (audio_start_delay) osal_task_delay_us(audio_start_delay);
    sim_note("AUD3 concurrent audio start begins");
    int r = hal_audio_capture_start(mic, NULL);
    sim_note("AUD3 concurrent audio start rc=%d mode=%d clock=%d", r, sim_bt_audio_mode(), sim_bt_audio_clock());
}

static void fault_on(void *arg) { (void)arg; sim_i2c_set_faults(1.0, "io-expander"); }
static void fault_off(void *arg) { (void)arg; sim_i2c_set_faults(0, NULL); }
static void volume_down(void *arg) { (void)arg; sim_input_key(HAL_KEY_VOL_UP, true); }
static void volume_up(void *arg) { (void)arg; sim_input_key(HAL_KEY_VOL_UP, false); }

void ink_main(void) {
    const char *mode = getenv("AUDIT_PROBE");
    if (!mode) osal_panic("AUDIT_PROBE missing");
    if (strncmp(mode, "r2_", 3)) { first_round_main(); return; }
    osal_task_delay_us(1600000);
    sim_note("AUD3 begin %s", mode);
    if (!strcmp(mode, "r2_timer_rearm_delete")) {
        callback_entered = osal_sem_create(0, 1);
        rearm_timer = osal_timer_create("rearm", rearm_cb, NULL);
        osal_task_cfg_t c = {.name="deleter", .fn=delete_timer_task, .priority=24, .core=-1, .stack_bytes=4096};
        osal_task_create(&c, NULL);
        osal_timer_start(rearm_timer, 10000, 0);
        osal_task_delay_us(500000);
        sim_note("AUD3 timer observation complete calls=%d", timer_calls);
    } else if (!strcmp(mode, "r2_display_cut")) {
        display_results = osal_queue_create(sizeof(hal_display_result_t), 2);
        osal_task_cfg_t c = {.name="panel-cut", .fn=cut_panel_task, .priority=10, .core=-1, .stack_bytes=4096};
        osal_task_create(&c, NULL);
        static uint8_t pixels[64 * 64]; memset(pixels, 0, sizeof pixels);
        hal_rect_t area = {0, 0, 64, 64};
        sim_display_snapshot("before");
        uint32_t id = hal_display_update(&area, pixels, 64, HAL_WF_GL16, 0, display_done, NULL);
        hal_display_result_t r = {0};
        int rc = osal_queue_recv(display_results, &r, 10000000);
        sim_display_snapshot("after");
        sim_note("AUD3 display id=%u wait=%d callback_ok=%d ready=%d", id, rc, r.ok, sim_tps65185_panel_ready());
    } else if (!strcmp(mode, "r2_camera_deadline")) {
        uint8_t *buf = osal_malloc(GC2145_FRAME_BYTES, OSAL_MEM_EXTERNAL | OSAL_MEM_DMA);
        const unsigned deadlines[] = {0, 1, 100, 600, 1000};
        for (unsigned i=0; i<sizeof deadlines/sizeof deadlines[0]; i++) {
            uint64_t t = osal_now_us();
            int rc = hal_camera_capture(buf, GC2145_FRAME_BYTES, deadlines[i]);
            sim_note("AUD3 camera timeout_ms=%u elapsed_ms=%.3f rc=%d", deadlines[i], (osal_now_us()-t)/1000., rc);
        }
        osal_free(buf);
    } else if (!strcmp(mode, "r2_audio_sleep_race") || !strcmp(mode, "r2_audio_sleep_late") || !strcmp(mode, "r2_audio_sleep_entry")) {
        /* After an ordinary rail cycle, audio startup needs to restore the BH mode. */
        metalio_board_set(1, MB_P1_SHARED_PWR, 0);
        metalio_board_set(1, MB_P1_SHARED_PWR, MB_P1_SHARED_PWR);
        bool during_entry = !strcmp(mode, "r2_audio_sleep_entry");
        audio_start_delay = during_entry ? 20000 : 0;
        osal_task_cfg_t c = {.name="capture-start", .fn=capture_task, .priority=8, .core=-1, .stack_bytes=4096};
        osal_task_create(&c, NULL);
        if (!during_entry) osal_task_delay_us(!strcmp(mode, "r2_audio_sleep_late") ? 600000 : 10000);
        hal_sleep_cfg_t sc = {.max_us=during_entry ? 1000000 : 300000, .wake_mask=HAL_WAKE_POWER_KEY};
        sim_note("AUD3 sleep call begins audio_busy=%d", sim_audio_busy());
        uint32_t cause = hal_power_sleep(&sc);
        sim_note("AUD3 sleep during audio start cause=%u epoch=%u mode=%d", cause, metalio_board_shared_power_epoch(), sim_bt_audio_mode());
        osal_task_delay_us(2000000);
        sim_note("AUD3 audio after race samples=%u mode=%d clock=%d busy=%d", mic_samples, sim_bt_audio_mode(), sim_bt_audio_clock(), sim_audio_busy());
        hal_audio_capture_stop();
    } else if (!strcmp(mode, "r2_input_boot_fault")) {
        hal_input_set_sink(sink, NULL);
        sim_note("AUD3 boot failure board_present=%d board_ready=%d", sim_board_present(), metalio_board_ready());
        vtk_lock();
        vtk_event_after(SIM_MS(20), power_down_event, NULL);
        vtk_event_after(SIM_MS(200), power_up_event, NULL);
        vtk_unlock();
        osal_task_delay_us(500000);
        sim_note("AUD3 boot-fault input keys=%u board_ready=%d", keys, metalio_board_ready());
    } else if (!strcmp(mode, "r2_wake_read_fault") || !strcmp(mode, "r2_wake_control")) {
        hal_input_set_sink(sink, NULL);
        vtk_lock();
        vtk_event_after(SIM_MS(400), power_down_event, NULL);
        if (!strcmp(mode, "r2_wake_read_fault")) vtk_event_after(SIM_MS(401), fault_on, NULL);
        vtk_event_after(SIM_MS(450), power_up_event, NULL);
        vtk_event_after(SIM_MS(650), fault_off, NULL);
        vtk_unlock();
        hal_sleep_cfg_t sc = {.max_us=1000000, .wake_mask=HAL_WAKE_POWER_KEY, .keep_modem=true};
        uint32_t cause = hal_power_sleep(&sc);
        osal_task_delay_us(1000000);
        sim_note("AUD3 wake cause=%u config=%02x,%02x keys=%u", cause, sim_tca_config(0), sim_tca_config(1), keys);
        vtk_lock();
        vtk_event_after(SIM_MS(10), volume_down, NULL);
        vtk_event_after(SIM_MS(200), volume_up, NULL);
        vtk_unlock();
        osal_task_delay_us(500000);
        sim_note("AUD3 after recovered bus and VOL press config=%02x,%02x keys=%u", sim_tca_config(0), sim_tca_config(1), keys);
    }
    sim_note("AUD3 end %s", mode);
}
