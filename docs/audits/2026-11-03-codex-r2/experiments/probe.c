/* Audit probes only. Link unmodified project libraries; no device access. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sim.h"
#include "board/metalio_board.h"
#include "gc2145/gc2145.h"
#include "nt26/nt26.h"
#include "ink/hal_camera.h"
#include "ink/hal_audio.h"
#include "ink/hal_power.h"
#include "ink/hal_storage.h"
#include "ink/sys/display.h"
#include "ink/sys/storage.h"

static unsigned keys, mic_samples;
static osal_queue_t *display_results;
static void display_done(void *ctx, const hal_display_result_t *r) {
    (void)ctx;
    osal_queue_send(display_results, r, OSAL_WAIT_FOREVER);
}
static void sink(const hal_input_event_t *e, void *ctx) {
    (void)ctx;
    if (e->type == HAL_IN_KEY) {
        keys++;
        sim_note("AUDIT key=%d pressed=%d latency_ms=%.3f", e->key.key, e->key.pressed,
                 (osal_now_us() - e->t_us) / 1000.0);
    }
}
static void mic(void *ctx, const int16_t *p, const int16_t *r, int n, int lost, uint64_t t) {
    (void)ctx; (void)p; (void)r; (void)lost; (void)t;
    mic_samples += (unsigned)n;
}
static void power_down_event(void *p) { (void)p; sim_input_key(HAL_KEY_POWER, true); }
static void power_up_event(void *p) { (void)p; sim_input_key(HAL_KEY_POWER, false); }
static void touch_down_event(void *p) { (void)p; sim_input_touch(0, true, 200, 500); }
static void touch_up_event(void *p) { (void)p; sim_input_touch(0, false, 200, 500); }
static void sample_sleep(void *p) {
    (void)p;
    sim_note("AUDIT sleeping=%d bt_powered=%d modem_registered=%d motor=%d camera_power=%d load_ma=%.3f",
             sim_power_sleeping(), sim_bt_audio_powered(), sim_net_modem_registered(),
             sim_board_motor_on(), sim_board_cam_power(), sim_power_load_ma());
}
static void cut_camera(void *p) {
    (void)p;
    osal_task_delay_us(200000);
    int rc = metalio_board_set(0, MB_P0_CAM_SCR, 0);
    sim_note("AUDIT camera supply cut rc=%d powered=%d", rc, sim_board_cam_power());
}
static void drain_storage(void) {
    uint64_t end = osal_now_us() + 10000000;
    while (ink_storage_pending() && osal_now_us() < end) osal_task_delay_us(10000);
}
void first_round_main(void) {
    const char *mode = getenv("AUDIT_PROBE");
    if (!mode) osal_panic("AUDIT_PROBE missing");
    osal_task_delay_us(1600000); /* board boot and BH1098G setup finish */
    sim_note("AUDIT begin %s", mode);
    if (!strcmp(mode, "display_fault")) {
        sim_i2c_set_faults(1.0, "pmic");
        display_results = osal_queue_create(sizeof(hal_display_result_t), 2);
        static uint8_t pixels[64 * 64];
        memset(pixels, 0, sizeof pixels);
        hal_rect_t area = {0, 0, 64, 64};
        uint32_t id = hal_display_update(&area, pixels, 64, HAL_WF_DU, 0, display_done, NULL);
        hal_display_result_t r = {0};
        int rc = osal_queue_recv(display_results, &r, 10000000);
        sim_note("AUDIT display_submit=%u wait_rc=%d callback_ok=%d panel_ready=%d", id, rc, r.ok, sim_tps65185_panel_ready());
    } else if (!strcmp(mode, "nt26_unicode")) {
        const char *bad[] = {"D8000041", "DC00", "004"};
        for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) {
            char out[32] = {0};
            int n = nt26_ucs2_hex_to_utf8(bad[i], out, sizeof out);
            sim_note("AUDIT nt26_hex=%s rc=%d bytes=%02x%02x%02x%02x", bad[i], n,
                     (unsigned char)out[0], (unsigned char)out[1], (unsigned char)out[2], (unsigned char)out[3]);
        }
        char hex[32] = {0};
        int n = nt26_utf8_to_ucs2_hex("\300\200", hex, sizeof hex);
        sim_note("AUDIT nt26_utf8=C080 rc=%d hex=%s", n, hex);
    } else if (!strcmp(mode, "storage_cache")) {
        if (ink_display_init() || ink_storage_init()) osal_panic("probe init failed");
        const char *path = "/user/apps/audit-cache";
        char *large = osal_malloc(1200, OSAL_MEM_EXTERNAL);
        char readback[16] = {0};
        memset(large, 'N', 1200);
        int a = ink_storage_write_atomic(path, "OLD", 3);
        drain_storage();
        int b = ink_storage_write_atomic(path, large, 1200);
        drain_storage();
        int n = ink_storage_read(path, readback, sizeof readback);
        ink_display_flash_guard(true);
        int64_t size = hal_fs_size(path);
        ink_display_flash_guard(false);
        sim_note("AUDIT writes=%d,%d pending=%d failed=%d durable_size=%lld read_n=%d first=%02x%02x%02x",
                 a, b, ink_storage_pending(), ink_storage_failed(), (long long)size, n,
                 (unsigned char)readback[0], (unsigned char)readback[1], (unsigned char)readback[2]);
        osal_free(large);
    } else if (!strcmp(mode, "tca_shadow")) {
        sim_i2c_set_faults(1.0, "io-expander");
        int bad = metalio_board_set(0, MB_P0_MOTOR, MB_P0_MOTOR);
        bool before = sim_board_motor_on();
        sim_i2c_set_faults(0, NULL);
        int good = metalio_board_set(0, MB_P0_AMP, MB_P0_AMP);
        sim_note("AUDIT failed_motor_write=%d motor_before=%d amp_write=%d motor_after=%d",
                 bad, before, good, sim_board_motor_on());
    } else if (!strcmp(mode, "input_fault") || !strcmp(mode, "touch_blocks_power")) {
        hal_input_set_sink(sink, NULL);
        if (!strcmp(mode, "input_fault")) sim_i2c_set_faults(1.0, "io-expander");
        else {
            vtk_event_after(SIM_MS(10), touch_down_event, NULL);
            vtk_event_after(SIM_MS(2000), touch_up_event, NULL);
        }
        vtk_event_after(SIM_MS(200), power_down_event, NULL);
        vtk_event_after(SIM_MS(600), power_up_event, NULL);
        osal_task_delay_us(2500000);
        sim_note("AUDIT keys_delivered=%u", keys);
        sim_i2c_set_faults(0, NULL);
    } else if (!strcmp(mode, "camera_cut") || !strcmp(mode, "camera_timeout")) {
        uint8_t *buf = osal_malloc(GC2145_FRAME_BYTES, OSAL_MEM_EXTERNAL);
        if (!strcmp(mode, "camera_cut")) {
            osal_task_cfg_t cfg = {.name="audit-cut", .fn=cut_camera, .stack_bytes=4096, .priority=10, .core=-1};
            if (osal_task_create(&cfg, NULL) != OSAL_OK) osal_panic("probe task create failed");
        }
        uint64_t t = osal_now_us();
        int rc = hal_camera_capture(buf, GC2145_FRAME_BYTES, !strcmp(mode, "camera_timeout") ? 600 : 2000);
        sim_note("AUDIT capture_rc=%d elapsed_ms=%.3f powered=%d", rc, (osal_now_us()-t)/1000.0, sim_board_cam_power());
        osal_free(buf);
    } else if (!strcmp(mode, "audio_unpowered")) {
        metalio_board_set(1, MB_P1_SHARED_PWR, 0);
        int rc = hal_audio_capture_start(mic, NULL);
        osal_task_delay_us(500000);
        hal_audio_capture_stop();
        sim_note("AUDIT capture_start=%d bt_powered=%d samples=%u", rc, sim_bt_audio_powered(), mic_samples);
    } else if (!strcmp(mode, "sleep_rails")) {
        metalio_board_set(0, MB_P0_MOTOR, MB_P0_MOTOR);
        vtk_event_after(SIM_MS(400), sample_sleep, NULL);
        hal_sleep_cfg_t cfg = {.max_us=1000000, .wake_mask=HAL_WAKE_TIMER, .keep_modem=false};
        unsigned why = hal_power_sleep(&cfg);
        sim_note("AUDIT woke=%u bt_powered=%d motor=%d", why, sim_bt_audio_powered(), sim_board_motor_on());
    } else osal_panic("unknown audit probe");
    sim_note("AUDIT complete %s", mode);
    vtk_stop();
}
