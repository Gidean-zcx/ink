/* Feeder line preparation variants on the ISS (ESP32-S31 model): upstream S31 C path (verbatim from
 * epdiy_riscv_ve_stub.c), an optimized C variant, and the no-op line. Standalone: see
 * docs/research/simulator/iss-calibration.md 2.6 for how it was built and run.
 *
 * up_lut() and up_mask() are copied from CloudZao/Metalio-E-INK4-Plus,
 * main/boards/metalio-e-ink4-plus/display/epdiy_riscv_ve_stub.c (MIT License,
 * Copyright (c) 2025 Shenzhen Xinzhi Future Technology Co., Ltd., Copyright (c) 2025 Project Contributors;
 * the license text is in that repository's LICENSE). Measurement only: not part of any build.
 * Input: 1 byte/px difference image ((to<<4)|from) in PSRAM, LUT + queue in internal SRAM.
 * W = 1216 px per native line, every other line (one feeder). */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "iss_rt.h"

#define W 1216
#define H 684
#define LB (W / 4)

/* 1. upstream verbatim (epdiy_riscv_ve_stub.c) */
__attribute__((optimize("O3"))) void up_lut(const uint32_t *ld, uint8_t *out, const uint8_t *lut, uint32_t w) {
    const uint32_t n = w >> 2;
    uint32_t j = 0;
#define LUT4(in) ((uint8_t)(lut[((in) & 0xFFu) << 2] << 6 | lut[(((in) >> 8) & 0xFFu) << 2] << 4 \
               | lut[(((in) >> 16) & 0xFFu) << 2] << 2 | lut[((in) >> 24) << 2]))
    for (; j + 4 <= n; j += 4) {
        const uint32_t in0 = ld[j], in1 = ld[j + 1], in2 = ld[j + 2], in3 = ld[j + 3];
        out[j] = LUT4(in0); out[j + 1] = LUT4(in1); out[j + 2] = LUT4(in2); out[j + 3] = LUT4(in3);
    }
    for (; j < n; j++) out[j] = LUT4(ld[j]);
#undef LUT4
}
__attribute__((optimize("O3"))) void up_mask(uint8_t *line, const uint8_t *mask, int mask_len) {
    uint32_t *l32 = (uint32_t *)line;
    const uint32_t *m32 = (const uint32_t *)mask;
    const int words = mask_len >> 2;
    int i = 0;
    for (; i + 4 <= words; i += 4) {
        l32[i] &= m32[i]; l32[i + 1] &= m32[i + 1]; l32[i + 2] &= m32[i + 2]; l32[i + 3] &= m32[i + 3];
    }
    for (; i < words; i++) l32[i] &= m32[i];
}

/* 2. four pre-shifted 256-byte tables (built once per phase): out = T0[b0]|T1[b1]|T2[b2]|T3[b3];
 *    the line mask is folded into the same pass (one store per output byte). */
__attribute__((optimize("O3"))) void t4_lut_mask(const uint32_t *ld, uint8_t *out, const uint8_t *T, const uint8_t *mask, uint32_t w) {
    const uint8_t *T0 = T, *T1 = T + 256, *T2 = T + 512, *T3 = T + 768;
    const uint32_t n = w >> 2;
    for (uint32_t j = 0; j < n; j += 4) {
        uint32_t a = ld[j], b = ld[j + 1], c = ld[j + 2], d = ld[j + 3];
        uint32_t o = (uint32_t)(T0[a & 0xff] | T1[(a >> 8) & 0xff] | T2[(a >> 16) & 0xff] | T3[a >> 24]);
        o |= (uint32_t)(T0[b & 0xff] | T1[(b >> 8) & 0xff] | T2[(b >> 16) & 0xff] | T3[b >> 24]) << 8;
        o |= (uint32_t)(T0[c & 0xff] | T1[(c >> 8) & 0xff] | T2[(c >> 16) & 0xff] | T3[c >> 24]) << 16;
        o |= (uint32_t)(T0[d & 0xff] | T1[(d >> 8) & 0xff] | T2[(d >> 16) & 0xff] | T3[d >> 24]) << 24;
        *(uint32_t *)(out + j) = o & *(const uint32_t *)(mask + j);
    }
}

/* 3. 16-bit pair tables in SRAM: two 4-bit inputs -> 4 output bits. The difference byte holds
 *    (to<<4)|from; with gray levels quantized to 16 that is 256 states per pixel, so pairs need
 *    64 Ki entries (too big for internal RAM). Skipped. */

/* skip line: zeros (no-op), as upstream for rows outside the area */
__attribute__((optimize("O3"))) void skip_line(uint8_t *buf) { memset(buf, 0, LB); }

volatile uint32_t sink;
static uint32_t rnd = 12345;
static uint32_t xr(void) { rnd ^= rnd << 13; rnd ^= rnd >> 17; rnd ^= rnd << 5; return rnd; }

int main(void) {
    uint8_t *diff = iss_psram_alloc((size_t)W * H, 64);
    for (int i = 0; i < W * H; i += 4) { uint32_t v = xr(); memcpy(diff + i, &v, 4); }
    uint8_t *lut = iss_sram_alloc(1024, 16), *T = iss_sram_alloc(1024, 16), *mask = iss_sram_alloc(LB, 16);
    uint8_t *q = iss_sram_alloc(32 * 304, 16);
    for (int i = 0; i < 1024; i++) lut[i] = (uint8_t)(xr() & 3);
    for (int k = 0; k < 4; k++)
        for (int i = 0; i < 256; i++) T[k * 256 + i] = (uint8_t)(lut[i << 2] << (6 - 2 * k));
    memset(mask, 0xff, LB);
    const int NL = 128;

    iss_cache(ISS_CACHE_WB | ISS_CACHE_INV_D);
    iss_begin("feed.upstream");
    for (int l = 0; l < NL; l++) {
        uint8_t *buf = q + (l % 32) * 304;
        up_lut((const uint32_t *)(diff + (size_t)l * 2 * W), buf, lut, W);
        up_mask(buf, mask, LB);
    }
    iss_end(NL);

    iss_cache(ISS_CACHE_WB | ISS_CACHE_INV_D);
    iss_begin("feed.t4");
    for (int l = 0; l < NL; l++) {
        uint8_t *buf = q + (l % 32) * 304;
        t4_lut_mask((const uint32_t *)(diff + (size_t)l * 2 * W), buf, T, mask, W);
    }
    iss_end(NL);

    iss_cache(ISS_CACHE_WB | ISS_CACHE_INV_D);
    iss_begin("feed.skip");
    for (int l = 0; l < NL; l++) skip_line(q + (l % 32) * 304);
    iss_end(NL);

    /* check: both paths agree */
    uint8_t a[LB], b[LB];
    up_lut((const uint32_t *)diff, a, lut, W);
    up_mask(a, mask, LB);
    t4_lut_mask((const uint32_t *)diff, b, T, mask, W);
    printf("agree %d\n", memcmp(a, b, LB) == 0);
    sink = q[5];
    return 0;
}
