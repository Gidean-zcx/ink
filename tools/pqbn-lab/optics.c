/*
 * The kernel on a model of the real panel, on the computer: what would these pictures look like?
 *
 * optics [--snap P,P,...] --out DIR first.raw next.raw ...
 * Pictures are native 4 bpp frames (make_frames.py). The first is drawn by the factory driver, the kernel
 * takes each next one as the lab firmware does. For each it writes DIR/<n>.b8 (brightness, 8 bit, native),
 * DIR/<n>.ref.b8 (what the factory driver would show) and one line of numbers.
 *
 * The panel model is the simulator's: drivers/display/pqbn/panel_e0470a14.h.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "panel_e0470a14.h"
#include "pqbn_v4.h"

enum { W = 1216, H = 684, N = W * H };
static pqbn_v4_ctx C;
static uint32_t now;
static float *B;
static float next_down[4096], next_up[4096];
static uint8_t line[W / 4 + 16] __attribute__((aligned(16)));

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
    int snaps[32], nsnap = 0, first = 0;
    const char *out = ".";
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--out")) out = argv[++i];
        else if (!strcmp(argv[i], "--snap")) { char *t = strtok(argv[++i], ","); while (t && nsnap < 32) { snaps[nsnap++] = atoi(t); t = strtok(NULL, ","); } }
        else { first = i; break; }
    }
    if (!first) { fprintf(stderr, "no pictures\n"); return 1; }
    panel_model_tables(next_down, next_up);
    const size_t nt = (size_t)(W / PQBN_TPX) * (H / PQBN_TPY);
    C.W = W; C.H = H; C.ntx = W / PQBN_TPX; C.nty = H / PQBN_TPY;
    uint8_t *fb = calloc(1, N / 2);
    C.fb4 = fb; C.hot = calloc(1, N / 2); C.cold = calloc(N, 2); C.tile_dirty = calloc(1, nt);
    C.tile_nmax = calloc(1, nt); C.tile_plan = calloc(nt, 2); C.band = calloc(C.nty, 4);
    if (pqbn_v4_build(&C, calloc(1, 16384), calloc(6, 256), calloc(1, 4096), &PQBN_PANEL_E0470A14)) { fprintf(stderr, "tables\n"); return 1; }
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
        const uint32_t planned0 = C.stat_planned, swung0 = C.stat_swung;
        int phases = 0;
        char path[512];
        for (;;) {
            pqbn_v4_phase(&C, now + 1);
            now++;
            apply_scan();
            phases++;
            for (int k = 0; k < nsnap; k++)
                if (snaps[k] == phases) {
                    snprintf(path, sizeof path, "%s/%d.p%03d.b8", out, p - first, phases);
                    write_b8(path, B);
                }
            if (phases >= 3000 || !pqbn_v4_busy_after(&C, now)) break;
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
        printf("%-20s phases=%-4d planned=%-8u swing=%-7u error=%.3f ink=%.3f of the factory's\n", strrchr(argv[p], '/') ? strrchr(argv[p], '/') + 1 : argv[p], phases, C.stat_planned - planned0, C.stat_swung - swung0, err / N, ink / ink_ref);
    }
    return 0;
}
