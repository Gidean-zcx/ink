/*
 * The kernel on a model of the real panel, on the computer: what would these pictures look like?
 *
 * optics [--t0 N] [--scans N] [--s0 X] [--budget N] [--measured] [--stage] [--units N] [--lattice] [--keep] [--snap P,P,...] --out DIR first.raw next.raw ...
 * Pictures are native 4 bpp frames (make_frames.py). The first is drawn by the factory driver, the kernel
 * takes each next one as the lab firmware does. For each it writes DIR/<n>.b8 (brightness, 8 bit, native),
 * DIR/<n>.ref.b8 (what the factory driver would show) and one line of numbers.
 *
 * The panel model is the wedge of 2026-11-06 (docs/hardware/2026-11-06-phase3.md): brightness after n pushes
 * of 12 ms from each rail, relative to the paper. A pixel's next push is read off its direction's curve from
 * where its brightness is now. Assumed, not measured: that this also holds after a reversal or a rest.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pqbn_v4.h"
#include "pqbn_tau.h"
#include "panel_curve.h"

enum { W = 1216, H = 684, N = W * H };
static pqbn_v4_ctx C;
static uint32_t now;
static float *B;
static float next_down[4096], next_up[4096];
static uint8_t line[W / 4 + 16] __attribute__((aligned(16)));

static float curve_at(const float *c, int n, float x) {
    if (x <= 0) return c[0];
    if (x >= n - 1) return c[n - 1];
    const int i = (int)x;
    return c[i] + (c[i + 1] - c[i]) * (x - i);
}

/* one more push along a falling or rising curve, from brightness b */
static float step_on(const float *c, int n, float b, int falling) {
    float x = 0;
    for (int i = 0; i + 1 < n; i++) {
        const float a0 = c[i], a1 = c[i + 1];
        if (falling ? (b <= a0 && b >= a1) : (b >= a0 && b <= a1)) {
            x = i + (a1 != a0 ? (b - a0) / (a1 - a0) : 1.f);
            return curve_at(c, n, x + 1);
        }
    }
    return falling ? (b > c[0] ? c[0] : c[n - 1]) : (b < c[0] ? c[0] : c[n - 1]);
}

static void build_steps(void) {
    for (int i = 0; i < 4096; i++) {
        const float b = (float)i / 4095.f;
        next_down[i] = step_on(PANEL_DOWN, PANEL_DOWN_N, b, 1);
        next_up[i] = step_on(PANEL_UP, PANEL_UP_N, b, 0);
    }
}

static int work_left(void) {
    if (C.backlog || C.n_motion) return 1;
    const int nt = C.ntx * C.nty;
    for (int i = 0; i < nt; i++)
        if (C.tile_dirty[i] || (C.tile_nmax[i] & (T_PEND | T_CAP))) return 1;
    for (int ty = 0; ty < C.nty; ty++)
        if ((int16_t)(C.band[2 * ty] - (uint16_t)(now + 1)) > 0) return 1;
    return 0;
}

static void apply_scan(void) {
    for (int y = 0; y < H; y++) {
        if (!pqbn_v4_exec_row(&C, now, y, line)) continue;
        for (int x = 0; x < W; x++) {
            const int code = (line[x >> 2] >> (6 - 2 * (x & 3))) & 3;
            if (!code) continue;
            float *b = &B[(size_t)y * W + x];
            const int i = (int)(*b * 4095.f + .5f);
            *b = code == 1 ? next_down[i] : next_up[i];
        }
    }
}

static void write_b8(const char *path, const float *v) {
    FILE *f = fopen(path, "wb");
    for (int i = 0; i < N; i++) { const int a = (int)(v[i] * 255.f + .5f); fputc(a < 0 ? 0 : a > 255 ? 255 : a, f); }
    fclose(f);
}

static void load(const char *path, uint8_t *to) {
    FILE *f = fopen(path, "rb");
    if (!f || fread(to, 1, N / 2, f) != N / 2) { fprintf(stderr, "cannot read %s\n", path); exit(1); }
    fclose(f);
}

static float level_at(const uint8_t *fb, int i) {
    const int y = i / W, x = i % W;
    const int v = (fb[(size_t)y * (W / 2) + x / 2] >> ((x & 1) * 4)) & 15;
    return PANEL_LEVEL[v];
}

int main(int argc, char **argv) {
    float t0 = 14, s0 = 0.3f;
    int scans = 1, budget = 20000, measured = 0, stage = 0, snaps[32], nsnap = 0, first = 0, units = 6, lattice = 0, keep = 0, bynumber = 0;
    const char *out = ".";
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--t0")) t0 = (float)atof(argv[++i]);
        else if (!strcmp(argv[i], "--s0")) s0 = (float)atof(argv[++i]);
        else if (!strcmp(argv[i], "--scans")) scans = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--budget")) budget = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--measured")) measured = 1;
        else if (!strcmp(argv[i], "--units")) units = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--lattice")) lattice = 1;
        else if (!strcmp(argv[i], "--keep")) keep = 1;
        else if (!strcmp(argv[i], "--by-number")) bynumber = 1;
        else if (!strcmp(argv[i], "--stage")) stage = 1;
        else if (!strcmp(argv[i], "--out")) out = argv[++i];
        else if (!strcmp(argv[i], "--snap")) { char *t = strtok(argv[++i], ","); while (t && nsnap < 32) { snaps[nsnap++] = atoi(t); t = strtok(NULL, ","); } }
        else { first = i; break; }
    }
    if (!first) { fprintf(stderr, "no pictures\n"); return 1; }
    if (stage) budget = 0;
    build_steps();
    const size_t nt = (size_t)(W / PQBN_TPX) * (H / PQBN_TPY);
    C.W = W; C.H = H; C.ntx = W / PQBN_TPX; C.nty = H / PQBN_TPY; C.marg = 2; C.plan_budget = budget; C.line_mode = 1;
    uint8_t *fb = calloc(1, N / 2);
    C.fb4 = fb; C.hot = calloc(1, N / 2); C.cold = calloc(N, 2); C.dec = calloc(1, 16384); C.tau = calloc(1, 4096);
    C.tau_nib = calloc(1, 2048); C.exk = calloc(6, 256); C.exk_m = calloc(7, 256); C.tile_dirty = calloc(1, nt);
    C.tile_nmax = calloc(1, nt); C.tile_plan = calloc(nt, 2); C.band = calloc(C.nty, 4); C.row_active = calloc(1, H);
    uint8_t *tau = (uint8_t *)C.tau;
    for (int i = 0; i < 4096; i++) tau[i] = lattice ? (uint8_t)((((i & 63) * 37) + ((i >> 6) * 101)) & 15) : PQBN_TAU[i];
    pqbn_v4_build_motion((uint8_t *)C.exk_m, (uint8_t *)C.tau_nib, C.tau);
    if (measured || stage) {
        if (pqbn_v4_build_measured((uint8_t *)C.dec, (uint8_t *)C.exk, C.step, C.level, C.lo, C.hi, t0, PANEL_PUSHES_DOWN, PANEL_PUSHES_UP)) { fprintf(stderr, "tables\n"); return 1; }
    } else if (pqbn_v4_build((uint8_t *)C.dec, (uint8_t *)C.exk, C.step, t0, s0, 120.0f)) { fprintf(stderr, "tables\n"); return 1; }
    if (stage && !bynumber) pqbn_v4_build_binary((uint8_t *)C.tau_nib, C.tau, PANEL_WHITE_SHARE);
    C.two_stage = stage;
    C.swing_units = units;
    C.keep = keep;
    B = malloc(sizeof(float) * N);
    float *ref = malloc(sizeof(float) * N);
    load(argv[first], fb);
    pqbn_v4_init(&C, 0);
    for (int i = 0; i < N; i++) B[i] = level_at(fb, i);
    for (int p = first + 1; p < argc; p++) {
        const uint32_t rest = 200;
        pqbn_v4_skip(&C, now, now + rest);
        now += rest;
        load(argv[p], fb);
        memset(C.tile_dirty, 1, nt);
        const uint32_t planned0 = C.stat_planned;
        int phases = 0, seen = 0;
        char path[512];
        for (;;) {
            pqbn_v4_phase(&C, now + 1);
            now++;
            for (int s = 0; s < scans; s++) apply_scan();
            phases++;
            for (int k = 0; k < nsnap; k++)
                if (snaps[k] == phases) {
                    snprintf(path, sizeof path, "%s/%d.p%03d.b8", out, p - first, phases);
                    write_b8(path, B);
                    seen++;
                }
            if (phases >= 3000 || !work_left()) break;
        }
        double err = 0, ink = 0, ink_ref = 0;
        for (int i = 0; i < N; i++) {
            ref[i] = level_at(fb, i);
            err += fabs(B[i] - ref[i]);
            ink += 1 - B[i];
            ink_ref += 1 - ref[i];
        }
        snprintf(path, sizeof path, "%s/%d.b8", out, p - first);
        write_b8(path, B);
        snprintf(path, sizeof path, "%s/%d.ref.b8", out, p - first);
        write_b8(path, ref);
 printf("%-20s phases=%-4d planned=%-8u error=%.3f ink=%.3f of the factory's\n", strrchr(argv[p], '/') ? strrchr(argv[p], '/') + 1 : argv[p], phases, C.stat_planned - planned0, err / N, ink / ink_ref);
    }
    return 0;
}
