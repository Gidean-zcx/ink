/*
 * GC2145 camera on the Plus: SCCB register model and DVP frame timing, and hal_camera on top of our
 * driver (drivers/chips/gc2145), as the device port will do.
 *
 * Registers (official dvp_camera.cc and the vendor VGA table @690a023): product ID 0xF0/0xF1 =
 * 0x21 0x45; 0xF0..0xFF are system registers on every page; page select 0xFE, where writing 0xF0 is
 * a software reset (the configuration is lost); page 0 holds the output format 0x84 (0x06 = RGB565)
 * and the crop window 0x90..0x98 (on, 480 rows, 640 columns), the pad output enable is 0xF2. Losing
 * TCA9555 P05 (camera/screen power) or P12 (shared reset) stops the sensor and clears its setup.
 *
 * Frames: they are only VGA RGB565 when the sensor was set up for it (the vendor sequence in
 * drivers/chips/gc2145); otherwise the DVP receiver does not get the 614400 bytes it waits for. CAM_FPS
 * is an ESTIMATE (the vendor sequence's name says 15 fps at 20 MHz MCLK); a picture is kept after
 * the warm-up frames upstream drops. The picture comes from the source the desktop simulator sets
 * (the computer's camera); without one, a test pattern.
 */
#include <stdlib.h>
#include <string.h>

#include "metalio_plus/metalio_board.h"
#include "gc2145/gc2145.h"
#include "ink/hal_camera.h"
#include "ink/hal_i2c.h"
#include "ink/osal.h"
#include "sim.h"

#define CAM_FPS 15

static struct {
    uint8_t page, ptr, reg0[256], sys[16]; /* page 0 registers; 0xF0..0xFF on every page */
    bool streaming;
    unsigned captures, resets;
    sim_camera_source_fn src;
    void *src_ctx;
    uint8_t *last; /* last picture taken (host memory), for the window */
    uint32_t last_seq;
} CAM;

static hal_i2c_dev_t *g_cam;

static bool powered(void) { return sim_board_cam_power() && !sim_board_shared_reset_held(); }

static void lose_setup(void) {
    memset(CAM.reg0, 0, sizeof CAM.reg0);
    memset(CAM.sys, 0, sizeof CAM.sys);
    CAM.page = 0;
    CAM.streaming = false;
}

void sim_camera_power_lost(void) { lose_setup(); } /* board: P05 or P12 fell; any context */

/* set up for VGA RGB565 as the board's sequence leaves it */
static bool vga_rgb565(void) {
    const uint8_t *r = CAM.reg0;
    return r[0x84] == 0x06 && (r[0x90] & 1) && ((r[0x95] << 8) | r[0x96]) == 480 && ((r[0x97] << 8) | r[0x98]) == 640 &&
           CAM.sys[0xF7 - 0xF0] == 0x1D && CAM.sys[0xF8 - 0xF0] == 0x84;
}

static int cam_xfer(void *state, const uint8_t *w, size_t wlen, uint8_t *r, size_t rlen) {
    (void)state;
    if (!powered()) {
        lose_setup();
        return HAL_I2C_NACK;
    }
    if (wlen) CAM.ptr = w[0];
    for (size_t i = 1; i < wlen; i++, CAM.ptr++) {
        if (CAM.ptr == 0xFE) {
            if (w[i] == 0xF0) { /* software reset */
                lose_setup();
                CAM.resets++;
            } else {
                CAM.page = w[i] & 0x03;
            }
            continue;
        }
        if (CAM.ptr >= 0xF0) {
            CAM.sys[CAM.ptr - 0xF0] = w[i];
            if (CAM.ptr == 0xF2) CAM.streaming = w[i] != 0;
            continue;
        }
        if (CAM.page == 0) CAM.reg0[CAM.ptr] = w[i];
    }
    for (size_t i = 0; i < rlen; i++, CAM.ptr++)
        r[i] = CAM.ptr == 0xF0 ? 0x21 : CAM.ptr == 0xF1 ? 0x45 : CAM.ptr > 0xF1 ? CAM.sys[CAM.ptr - 0xF0] : CAM.page == 0 ? CAM.reg0[CAM.ptr] : 0;
    return HAL_I2C_OK;
}

void sim_camera_set_source(sim_camera_source_fn fn, void *ctx) {
    CAM.src = fn;
    CAM.src_ctx = ctx;
}

/* Colour bars with a square that moves with every picture, RGB565 high byte first. */
static void test_pattern(uint8_t *buf, unsigned n) {
    static const uint16_t bars[8] = {0xFFFF, 0xFFE0, 0x07FF, 0x07E0, 0xF81F, 0xF800, 0x001F, 0x0000};
    int sq = (int)(n * 37 % (GC2145_WIDTH - 80));
    for (int y = 0; y < GC2145_HEIGHT; y++)
        for (int x = 0; x < GC2145_WIDTH; x++) {
            uint16_t c = bars[x * 8 / GC2145_WIDTH];
            if (x >= sq && x < sq + 80 && y >= 200 && y < 280) c = c ? 0x0000 : 0xFFFF;
            buf[2 * (y * GC2145_WIDTH + x)] = (uint8_t)(c >> 8);
            buf[2 * (y * GC2145_WIDTH + x) + 1] = (uint8_t)c;
        }
}

static void fill_on_host(void *arg) {
    uint8_t *buf = arg;
    if (!CAM.src || !CAM.src(buf, GC2145_WIDTH, GC2145_HEIGHT, CAM.src_ctx)) test_pattern(buf, CAM.captures);
    if (!CAM.last) CAM.last = malloc(GC2145_FRAME_BYTES);
    if (CAM.last) memcpy(CAM.last, buf, GC2145_FRAME_BYTES);
    CAM.last_seq++;
}

bool hal_camera_info(hal_camera_info_t *out) {
    if (!g_cam) return false;
    out->width = GC2145_WIDTH;
    out->height = GC2145_HEIGHT;
    out->frame_bytes = GC2145_FRAME_BYTES;
    return true;
}

static int capture(uint8_t *buf, size_t cap, uint32_t timeout_ms) {
    uint64_t deadline = osal_now_us() + (uint64_t)timeout_ms * 1000u; /* covers the whole call */
    if (!g_cam || !buf || cap < GC2145_FRAME_BYTES || !pools_is_external(buf)) return -1; /* DMA writes PSRAM */
    uint16_t pid = 0;
    if (gc2145_probe(g_cam, &pid) != HAL_I2C_OK || pid != GC2145_PID) return -2;
    if (gc2145_start(g_cam) != HAL_I2C_OK) return -3;
    const uint64_t frame_us = 1000000u / CAM_FPS;
    for (int f = 0; f <= GC2145_WARMUP_FRAMES; f++) { /* frames arrive one by one over DVP */
        if (osal_now_us() + frame_us > deadline) {
            gc2145_stop(g_cam);
            return -4;
        }
        osal_task_delay_us(frame_us);
        if (!CAM.streaming) return -5; /* lost power or reset meanwhile: no complete frame */
        if (!vga_rgb565()) {
            gc2145_stop(g_cam);
            return -6; /* the receiver waits for 614400 bytes that never come as such */
        }
    }
    vtk_host_call(fill_on_host, buf);
    gc2145_stop(g_cam);
    CAM.captures++;
    sim_metric_add("camera.captures", 1);
    return GC2145_FRAME_BYTES;
}

static int g_capturing;

int hal_camera_capture(uint8_t *buf, size_t cap, uint32_t timeout_ms) {
    __atomic_add_fetch(&g_capturing, 1, __ATOMIC_SEQ_CST);
    int r = capture(buf, cap, timeout_ms);
    __atomic_sub_fetch(&g_capturing, 1, __ATOMIC_SEQ_CST);
    return r;
}

bool sim_camera_busy(void) { return __atomic_load_n(&g_capturing, __ATOMIC_SEQ_CST) > 0; } /* the sleep must wait */

void sim_camera_init(void) { /* task context (boot) */
    if (!sim_board_present()) return;
    static const sim_i2c_model_t cam = {cam_xfer};
    sim_i2c_attach(0, GC2145_ADDR, &cam, NULL);
    g_cam = hal_i2c_device(0, GC2145_ADDR, "camera");
}

bool sim_camera_last(uint8_t *buf, size_t cap, uint32_t *seq) { /* host work (window) */
    if (!CAM.last || cap < GC2145_FRAME_BYTES || CAM.last_seq == *seq) return false;
    memcpy(buf, CAM.last, GC2145_FRAME_BYTES);
    *seq = CAM.last_seq;
    return true;
}

unsigned sim_camera_captures(void) { return CAM.captures; }
