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

typedef struct { int D; int D1[8]; int invD; int marg; } pqbn_params;
void pqbn_init(pqbn_params *P, float T0, float s0, float taud, int marg);
void pqbn_q8(const uint8_t *I, const uint8_t *tau, uint8_t *ph, uint8_t *dwb, int8_t *dc,
             int8_t *u, int n, const pqbn_params *P, uint32_t field);

void pqbn_lut2_build(uint8_t *lut2, int *step, float T0, float s0, float taud);
int pqbn_lut2_phase(const uint8_t *lut2, const int *step, const uint8_t *tgt4, const uint8_t *tau4,
                    uint8_t *q8, uint8_t *cls, int8_t *dc, uint8_t *mflag, int8_t *u, int n,
                    uint32_t field, int marg);

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
    // ---- (d) PQ-BN full frame per phase (8-bit target, every pixel evaluated) + 2-bit pack
    {
        pqbn_params P; pqbn_init(&P, 7.6f, 0.3f, 120.f, 2);
        const int n = W * H;
        uint8_t *I0 = malloc(n), *I1 = malloc(n), *tau = malloc(n), *ph = malloc(n), *dwb = malloc(n);
        int8_t *dc = calloc(n, 1), *u = malloc(n);
        for (int i = 0; i < n; i++) {
            int x = i % W, y = i / W;
            I0[i] = (uint8_t)(rand() % 100 < 12 ? 0 : (rand() % 100 < 8 ? rand() % 256 : 255));
            I1[i] = (uint8_t)((x * 7 + y * 3) & 255);
            tau[i] = (uint8_t)(((x & 63) * 37 + (y & 63) * 101) & 255);
            ph[i] = 255; dwb[i] = 15;
        }
        double tp = 0, tk = 0;
        for (int r = 0; r < REP; r++) {
            double a = now();
            pqbn_q8((r & 4) ? I0 : I1, tau, ph, dwb, dc, u, n, &P, (uint32_t)r);
            tp += now() - a; a = now();
            for (int i = 0; i < n; i += 4)
                out[i >> 2] = (uint8_t)(((u[i] & 3) << 6) | ((u[i + 1] & 3) << 4) | ((u[i + 2] & 3) << 2) | (u[i + 3] & 3));
            tk += now() - a; sink ^= out[r];
        }
        printf("  PQ-BN integer (full frame)    : %6.2f ms  (%.2f ns/px) + pack %.2f ns/px   ratio %.2fx\n",
               tp / REP * 1e3, tp / REP / ((double)n) * 1e9, tk / REP / ((double)n) * 1e9, (tp + tk) / REP / ta);
    }
    // ---- (e) PQ-BN 64 KB decision LUT, full frame every phase
    {
        static uint8_t lut2[65536]; int step[4];
        pqbn_lut2_build(lut2, step, 7.6f, 0.3f, 120.f);
        const int n = W * H;
        uint8_t *t0b = malloc(n), *t1b = malloc(n), *tau = malloc(n), *q8 = malloc(n), *cls = malloc(n), *mf = malloc(n);
        int8_t *dc = calloc(n, 1), *u = malloc(n);
        for (int i = 0; i < n; i++) {
            int x = i % W, y = i / W;
            t0b[i] = (uint8_t)(rand() % 100 < 12 ? 0 : (rand() % 100 < 8 ? rand() % 16 : 15));
            t1b[i] = (uint8_t)(((x >> 3) + (y >> 4)) & 15);
            tau[i] = (uint8_t)(((x & 63) * 37 + (y & 63) * 101) & 15);
            q8[i] = 255; cls[i] = 3; mf[i] = 1;
        }
        double tp = 0;
        for (int r = 0; r < REP; r++) {
            double a = now();
            sink += pqbn_lut2_phase(lut2, step, (r & 4) ? t0b : t1b, tau, q8, cls, dc, mf, u, n, (uint32_t)r, 2);
            tp += now() - a;
        }
        printf("  PQ-BN 64KB LUT (full frame)   : %6.2f ms  (%.2f ns/px)   ratio %.2fx\n",
               tp / REP * 1e3, tp / REP / ((double)n) * 1e9, tp / REP / ta);
    }
    const double npx = (double)W * H;
    printf("per phase, full %dx%d frame (x86):\n", W, H);
    printf("  EPDiy-style interlace+LUT+mask : %6.2f ms  (%.2f ns/px)\n", ta * 1e3, ta / npx * 1e9);
    printf("  integrator, binary targets     : %6.2f ms  (%.2f ns/px)  ratio %.2fx\n", tb * 1e3, tb / npx * 1e9, tb / ta);
    printf("  integrator, 9 gray levels      : %6.2f ms  (%.2f ns/px)  ratio %.2fx\n", tc * 1e3, tc / npx * 1e9, tc / ta);
    printf("  integrator as 256-entry LUT    : %6.2f ms  (%.2f ns/px)  ratio %.2fx\n", td * 1e3, td / npx * 1e9, td / ta);
    printf("  (sink %d)\n", sink & 1);
    return 0;
}
