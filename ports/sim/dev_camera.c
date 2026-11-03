/*
 * GC2145 camera on the Plus: SCCB register model and DVP frame timing, and hal_camera on top of our
 * driver (drivers/gc2145), as the device port will do.
 *
 * Registers (official dvp_camera.cc and the vendor VGA table @690a023): product ID 0xF0/0xF1 =
 * 0x21 0x45 on any page; page select 0xFE, where writing 0xF0 is a software reset; page 0 holds the
 * pad output enable 0xF2 (non-zero streams) and the output format 0x84 (0x06 = RGB565). The sensor
 * answers only while TCA9555 P05 (camera/screen power) and P12 (shared reset) are high.
 *
 * Frames: VGA RGB565, high byte first, at CAM_FPS (ESTIMATE: 15 fps from the 20 MHz MCLK); a
 * picture is ready after the warm-up frames upstream drops. The picture comes from the source the
 * desktop simulator sets (the computer's camera); without one, a test pattern.
 */
#include <stdlib.h>
#include <string.h>

#include "board/metalio_board.h"
#include "gc2145/gc2145.h"
#include "ink/hal_camera.h"
#include "ink/hal_i2c.h"
#include "ink/osal.h"
#include "sim.h"

#define CAM_FPS 15

static struct {
    uint8_t page, ptr, reg0[256];
    bool streaming;
    unsigned captures, resets;
    sim_camera_source_fn src;
    void *src_ctx;
    uint8_t *last; /* last picture taken (host memory), for the window */
    uint32_t last_seq;
} CAM;

static hal_i2c_dev_t *g_cam;

static bool powered(void) { return sim_board_cam_power() && !sim_board_shared_reset_held(); }

static int cam_xfer(void *state, const uint8_t *w, size_t wlen, uint8_t *r, size_t rlen) {
    (void)state;
    if (!powered()) {
        CAM.streaming = false;
        return HAL_I2C_NACK;
    }
    if (wlen) CAM.ptr = w[0];
    for (size_t i = 1; i < wlen; i++, CAM.ptr++) {
        if (CAM.ptr == 0xFE) {
            if (w[i] == 0xF0) { /* software reset */
                memset(CAM.reg0, 0, sizeof CAM.reg0);
                CAM.page = 0;
                CAM.streaming = false;
                CAM.resets++;
            } else {
                CAM.page = w[i] & 0x03;
            }
            continue;
        }
        if (CAM.page != 0) continue;
        CAM.reg0[CAM.ptr] = w[i];
        if (CAM.ptr == 0xF2) CAM.streaming = w[i] != 0;
    }
    for (size_t i = 0; i < rlen; i++, CAM.ptr++)
        r[i] = CAM.ptr == 0xF0 ? 0x21 : CAM.ptr == 0xF1 ? 0x45 : CAM.page == 0 ? CAM.reg0[CAM.ptr] : 0;
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

int hal_camera_capture(uint8_t *buf, size_t cap, uint32_t timeout_ms) {
    if (!g_cam) return -1;
    if (cap < GC2145_FRAME_BYTES) return -1;
    uint16_t pid = 0;
    if (gc2145_probe(g_cam, &pid) != HAL_I2C_OK || pid != GC2145_PID) return -2;
    if (gc2145_start(g_cam) != HAL_I2C_OK) return -3;
    uint64_t need_us = (uint64_t)(GC2145_WARMUP_FRAMES + 1) * 1000000u / CAM_FPS;
    if (need_us / 1000 > timeout_ms) {
        gc2145_stop(g_cam);
        return -4;
    }
    osal_task_delay_us(need_us); /* the warm-up frames and the one kept arrive over DVP by DMA */
    if (!CAM.streaming) return -5; /* lost power or reset meanwhile */
    vtk_host_call(fill_on_host, buf);
    gc2145_stop(g_cam);
    CAM.captures++;
    sim_metric_add("camera.captures", 1);
    return GC2145_FRAME_BYTES;
}

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
