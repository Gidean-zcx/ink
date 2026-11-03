/*
 * JPEG stand-in for the simulator (see ink/hal_jpeg.h): "SJ" w h (u16 LE),
 * then the picture at half resolution in each direction, 8-bit gray. It is
 * lossy like JPEG and about as small for photos; what matters here is the
 * data volume and the decoder's cost (the S31's hardware: ~11 ns per pixel,
 * from Espressif's 96 fps for 720x1280 grayscale).
 */
#include <string.h>

#include "ink/hal_jpeg.h"
#include "ink/osal.h"

int hal_jpeg_decode_gray(const uint8_t *src, size_t n, uint8_t *dst, int w, int h) {
    if (n < 6 || src[0] != 'S' || src[1] != 'J' || (src[2] | src[3] << 8) != w || (src[4] | src[5] << 8) != h) return -1;
    int hw = (w + 1) / 2, hh = (h + 1) / 2;
    if (n < 6 + (size_t)hw * (size_t)hh) return -1;
    osal_cost_cycles((uint64_t)w * (uint64_t)h * 4u); /* ~3.6 cycles per pixel at 320 MHz */
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) dst[(size_t)y * (size_t)w + (size_t)x] = src[6 + (size_t)(y / 2) * (size_t)hw + (size_t)(x / 2)];
    return 0;
}
