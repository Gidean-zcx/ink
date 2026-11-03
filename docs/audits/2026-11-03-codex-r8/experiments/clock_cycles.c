/* AUD-011 probe: does advancing the PQ-BN phase clock across rests (as ports/sim/hal_display_sim.c
 * pq_advance_idle does: <= 255 phases per rest, no kernel phase) keep settled pixels' 10-bit
 * timestamps unambiguous? Compares the rest level the kernel would reconstruct for every settled
 * pixel against the true rest, after a realistic pattern: short drives separated by long rests. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pqbn_v4.h"

#define PW 64
#define PH 512 /* one row per timestamp-sweep index */

static pqbn_v4_ctx C;
static uint8_t *alloc(size_t n) { uint8_t *p = calloc(1, n); if (!p) abort(); return p; }
static int lvl(int r) { static const unsigned char t[64] = {0,1,2,2,3,3,3,3,4,4,4,4,4,4,4,4,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
    6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6}; return r >= 64 ? 7 : t[r]; }

static void setup(void) {
    int ntx = PW / PQBN_TPX, nty = PH / PQBN_TPY, nt = ntx * nty;
    uint8_t *dec = alloc(16384), *exk = alloc(6 * 256), *exk_m = alloc(7 * 256), *tau = alloc(4096), *tau_nib = alloc(2048);
    if (pqbn_v4_build(dec, exk, C.step, 7.6f, 0.3f, 120.0f)) { fprintf(stderr, "build failed\n"); exit(2); }
    for (int i = 0; i < 4096; i++) tau[i] = (uint8_t)((((i & 63) * 37) + ((i >> 6) * 101)) & 15);
    pqbn_v4_build_motion(exk_m, tau_nib, tau);
    C.W = PW; C.H = PH; C.ntx = ntx; C.nty = nty; C.marg = 2;
    C.dec = dec; C.exk = exk; C.exk_m = exk_m; C.tau = tau; C.tau_nib = tau_nib;
    uint8_t *fb4 = alloc((size_t)PW * PH / 2); memset(fb4, 0xff, (size_t)PW * PH / 2); C.fb4 = fb4;
    C.hot = alloc((size_t)PW * PH / 2); C.cold = (uint16_t *)alloc((size_t)PW * PH * 2);
    C.tile_dirty = alloc(nt); C.tile_plan = (uint16_t *)alloc(nt * 2); C.tile_nmax = alloc(nt);
    C.band = (uint16_t *)alloc((size_t)nty * 4); C.row_active = alloc(PH); C.drive = alloc((size_t)PW * PH / 4);
    C.plan_budget = 8000; C.auto_motion = 0; C.line_mode = 0;
    pqbn_v4_init(&C, 0);
}

/* the simulator's idle advance (hal_display_sim.c pq_advance_idle), minus the 4096 clamp (never crossed here) */
static uint32_t advance_idle(uint32_t now, uint32_t rest_phases) {
    uint32_t n=rest_phases>255?255:rest_phases;
#ifndef AUDIT_OLD_SKIP
    pqbn_v4_skip(&C,now-1,now-1+n);
#endif
    return now+n;
}

int main(int argc, char **argv) {
    int drive = argc > 1 ? atoi(argv[1]) : 40;     /* phases per drive (e.g. a page turn) */
    int cycles = argc > 2 ? atoi(argv[2]) : 12;    /* drive + long rest, repeated */
    setup();
    uint32_t now = 1;
    for (int c = 0; c < cycles; c++) {
        for (int k = 0; k < drive; k++) pqbn_v4_phase(&C, now++); /* nothing moves; the sweep runs */
        now = advance_idle(now, 100000);                          /* a long rest: capped jump */
    }
    /* every pixel has been settled since phase 0: true rest = now >= 64 -> level 7 */
    long settled = 0, wrong = 0, worst_row = -1;
    for (int y = 0; y < PH; y++)
        for (int x = 0; x < PW; x++) {
            int n0 = (C.hot[(y * PW + x) / 2] >> ((x & 1) * 4)) & 7;
            if (n0 != 7) continue;
            settled++;
            uint16_t s = C.cold[y * PW + x];
            int ts = (s & 255) | ((s >> 5) & 0x300);
            int rest = (int)((now - (uint32_t)ts) & 1023);
            if (lvl(rest) != 7) { wrong++; if (worst_row < 0) worst_row = y; }
        }
    printf("drive=%d phases, cycles=%d, clock=%u phases (true rest of every pixel >= %u)\n", drive, cycles, now, now - 1);
    printf("settled pixels: %ld, reconstructed dwell level != 7 (rest wrapped below 64): %ld (%.1f%%), first row %ld\n",
           settled, wrong, 100.0 * wrong / (settled ? settled : 1), worst_row);
    return wrong ? 1 : 0;
}
