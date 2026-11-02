// Per-phase pixel cost on the Metalio panel geometry (1216 x 684), x86 single core, -O2.
// (a) EPDiy-style difference path (my reimplementation of the algorithm they use:
//     interlace to/from 4bpp into 1 byte/px, then 1K LUT lookup to 2-bit drive, then column mask)
// (b) integrator driver: 4-bit target (from the 4bpp fb) -> target position, 4-bit pos state,
//     2-bit drive out. Same input/output formats, plus a per-row "active" count so idle rows and
//     an idle panel can be detected for free.
// gcc -O2 -o bench_metalio bench_metalio.c && ./bench_metalio
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#define W 1216
#define H 684

static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }

// ---- (a) EPDiy-style: interlace + 1K LUT + mask --------------------------------
static void epdiy_style_phase(const uint8_t *to, const uint8_t *from, const uint8_t *lut1k,
                              uint8_t *out, uint8_t *line, uint8_t *coldirty, const uint8_t *mask) {
    for (int y = 0; y < H; y++) {
        const uint8_t *t = to + y * (W / 2), *f = from + y * (W / 2);
        uint32_t dirty = 0;
        for (int p = 0; p < W / 2; p++) {
            uint8_t tb = t[p], fb = f[p];
            uint8_t t0 = tb & 15, t1 = tb >> 4, f0 = fb & 15, f1 = fb >> 4;
            uint8_t d0 = t0 ^ f0, d1 = t1 ^ f1;
            coldirty[p] |= (uint8_t)(d0 | (d1 << 4));
            dirty |= d0 | d1;
            line[2 * p] = (uint8_t)((t0 << 4) | f0);
            line[2 * p + 1] = (uint8_t)((t1 << 4) | f1);
        }
        uint8_t *o = out + y * (W / 4);
        const uint32_t *ld = (const uint32_t *)line;
        for (int j = 0; j < W / 4; j++) {
            uint32_t in = ld[j];
            o[j] = (uint8_t)(lut1k[(in & 0xFF) << 2] << 6 | lut1k[((in >> 8) & 0xFF) << 2] << 4 |
                             lut1k[((in >> 16) & 0xFF) << 2] << 2 | lut1k[(in >> 24) << 2]);
        }
        uint32_t *o32 = (uint32_t *)o; const uint32_t *m32 = (const uint32_t *)mask;
        for (int j = 0; j < W / 16; j++) o32[j] &= m32[j];
        (void)dirty;
    }
}

// ---- (b) integrator ---------------------------------------------------------------
// tgt: 4bpp gray fb (2 px/byte). pos: 4-bit state (2 px/byte). tpos[16]: gray -> target position.
// Returns number of driven pixels (0 => panel idle, scanning can stop and PMIC power down).
static int integrator_phase(const uint8_t *tgt, uint8_t *pos, const uint8_t *tpos, uint8_t *out,
                            uint16_t *row_active) {
    int total = 0;
    for (int y = 0; y < H; y++) {
        const uint8_t *t = tgt + y * (W / 2);
        uint8_t *ps = pos + y * (W / 2);
        uint8_t *o = out + y * (W / 4);
        int act = 0;
        for (int p = 0; p < W / 2; p += 2) {             // 4 pixels -> 1 output byte
            uint8_t ob = 0;
            for (int k = 0; k < 2; k++) {
                uint8_t tb = t[p + k], sb = ps[p + k];
                int g0 = tpos[tb & 15], g1 = tpos[tb >> 4];
                int p0 = sb & 15, p1 = sb >> 4;
                int s0 = (g0 > p0) - (g0 < p0), s1 = (g1 > p1) - (g1 < p1);
                p0 += s0; p1 += s1;
                ps[p + k] = (uint8_t)(p0 | (p1 << 4));
                // EPDiy action codes: 1 = toward black, 2 = toward white, 0 = none
                uint8_t c0 = (uint8_t)(s0 > 0 ? 2 : s0 < 0 ? 1 : 0), c1 = (uint8_t)(s1 > 0 ? 2 : s1 < 0 ? 1 : 0);
                ob = (uint8_t)((ob << 4) | (c0 << 2) | c1);
                act += (s0 != 0) + (s1 != 0);
            }
            o[p >> 1] = ob;
        }
        row_active[y] = (uint16_t)act;
        total += act;
    }
    return total;
}

// ---- (c) integrator as a time-invariant LUT on (target<<4 | pos) ---------------------
// L[(gray<<4)|pos] = (drive_code << 4) | new_pos. Same data flow as EPDiy's difference path:
// interlace two nibble planes, one table lookup per pixel, plus writing the new state back.
static void build_int_lut(uint8_t L[256], const uint8_t tpos[16]) {
    for (int g = 0; g < 16; g++) for (int p = 0; p < 16; p++) {
        int t = tpos[g], s = (t > p) - (t < p), np = p + s;
        if (np < 0) np = 0; if (np > 15) np = 15;
        L[(g << 4) | p] = (uint8_t)(((s > 0 ? 2 : s < 0 ? 1 : 0) << 4) | np);
    }
}
static void integrator_lut_phase(const uint8_t *tgt, uint8_t *pos, const uint8_t *L, uint8_t *out) {
    for (int y = 0; y < H; y++) {
        const uint8_t *t = tgt + y * (W / 2);
        uint8_t *ps = pos + y * (W / 2);
        uint8_t *o = out + y * (W / 4);
        for (int p = 0; p < W / 2; p += 2) {
            uint8_t ta = t[p], sa = ps[p], tb = t[p + 1], sb = ps[p + 1];
            uint8_t c0 = L[((ta & 15) << 4) | (sa & 15)], c1 = L[(ta & 0xF0) | (sa >> 4)];
            uint8_t c2 = L[((tb & 15) << 4) | (sb & 15)], c3 = L[(tb & 0xF0) | (sb >> 4)];
            ps[p] = (uint8_t)((c0 & 15) | (c1 << 4));
            ps[p + 1] = (uint8_t)((c2 & 15) | (c3 << 4));
            o[p >> 1] = (uint8_t)(((c0 >> 4) << 6) | ((c1 >> 4) << 4) | ((c2 >> 4) << 2) | (c3 >> 4));
        }
    }
}

int main(void) {
    const int fb = W * H / 2;
    uint8_t *to = malloc(fb), *from = malloc(fb), *pos = malloc(fb), *out = malloc(W * H / 4);
    uint8_t *line = aligned_alloc(16, W), *coldirty = calloc(W / 2, 1), *mask = aligned_alloc(16, W / 4);
    uint8_t lut1k[1024]; uint16_t row_active[H];
    for (int i = 0; i < 1024; i++) lut1k[i] = (uint8_t)((i * 2654435761u) >> 30);
    memset(mask, 0xff, W / 4);
    srand(1);
    // text-like content: mostly white, ~12% black, some AA grays; 'from' = previous page
    for (int i = 0; i < fb; i++) {
        int r = rand() % 100, r2 = rand() % 100;
        uint8_t a = r < 10 ? 0x00 : r < 14 ? 0x88 : 0xff, b = r2 < 10 ? 0x00 : r2 < 14 ? 0x88 : 0xff;
        to[i] = a; from[i] = b;
    }
    uint8_t tpos_bin[16], tpos_lvl[16];
    for (int g = 0; g < 16; g++) { tpos_bin[g] = g >= 8 ? 8 : 0; tpos_lvl[g] = (uint8_t)((g * 8 + 7) / 15); }
    for (int i = 0; i < fb; i++) pos[i] = 0x88;
    const int REP = 200;
    volatile int sink = 0;
    double t0 = now();
    for (int r = 0; r < REP; r++) epdiy_style_phase(to, from, lut1k, out, line, coldirty, mask);
    double ta = (now() - t0) / REP;
    sink ^= out[123];
    t0 = now();
    for (int r = 0; r < REP; r++) {
        // alternate targets so pixels keep moving (worst case: everything in flight)
        sink += integrator_phase((r & 8) ? to : from, pos, tpos_bin, out, row_active);
    }
    double tb = (now() - t0) / REP;
    t0 = now();
    for (int r = 0; r < REP; r++) sink += integrator_phase((r & 8) ? to : from, pos, tpos_lvl, out, row_active);
    double tc = (now() - t0) / REP;
    uint8_t L[256]; build_int_lut(L, tpos_bin);
    t0 = now();
    for (int r = 0; r < REP; r++) { integrator_lut_phase((r & 8) ? to : from, pos, L, out); sink ^= out[r]; }
    double td = (now() - t0) / REP;
    const double npx = (double)W * H;
    printf("per phase, full %dx%d frame (x86):\n", W, H);
    printf("  EPDiy-style interlace+LUT+mask : %6.2f ms  (%.2f ns/px)\n", ta * 1e3, ta / npx * 1e9);
    printf("  integrator, binary targets     : %6.2f ms  (%.2f ns/px)  ratio %.2fx\n", tb * 1e3, tb / npx * 1e9, tb / ta);
    printf("  integrator, 9 gray levels      : %6.2f ms  (%.2f ns/px)  ratio %.2fx\n", tc * 1e3, tc / npx * 1e9, tc / ta);
    printf("  integrator as 256-entry LUT    : %6.2f ms  (%.2f ns/px)  ratio %.2fx\n", td * 1e3, td / npx * 1e9, td / ta);
    printf("  (sink %d)\n", sink & 1);
    return 0;
}
