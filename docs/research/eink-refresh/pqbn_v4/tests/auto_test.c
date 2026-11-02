// Automatic mode selection: with a planner budget and auto_motion on, the kernel must
//   - keep one-off changes (a page turn) in PQ-BN even while the planner has a backlog,
//   - switch a region that keeps changing faster than it can be planned (a scroll) to motion mode,
//     and back to PQ-BN after it stops,
//   - keep a small region that keeps changing (a blinking cursor) in PQ-BN, since the planner keeps up.
// It also checks the DC bookkeeping against the exact drive stream throughout.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
typedef struct { int W, H, ntx, nty; const uint8_t *dec, *exk; uint8_t step[4]; int marg;
  const uint8_t *fb4; uint8_t *hot; uint16_t *cold; const uint8_t *tau; uint8_t *tile_dirty; uint16_t *tile_plan;
  uint8_t *tile_nmax; uint8_t *drive; uint8_t *row_active; int plan_budget, plan_cursor; uint32_t stat_planned;
  const uint8_t *exk_m, *tau_nib; uint16_t *band; int line_mode, auto_motion, backlog, n_motion; } v4ctx;
int pqbn_v4_build(uint8_t *, uint8_t *, uint8_t *, float, float, float); int pqbn_v4_phase(v4ctx *, uint32_t);
void pqbn_v4_init(v4ctx *, uint32_t); void pqbn_v4_build_motion(uint8_t *, uint8_t *, const uint8_t *);
#define W 320
#define H 240
#define NTX (W / 32)
#define NTY (H / 4)
#define NT (NTX * NTY)
static uint8_t fb[H * W / 2], prev[H * W / 2], td[NT];
static int nib(const uint8_t *p, int i) { return (p[i >> 1] >> ((i & 1) * 4)) & 15; }
static void setpx(int x, int y, int t) { uint8_t *b = &fb[y * (W / 2) + x / 2]; *b = (uint8_t)((*b & ~(15 << ((x & 1) * 4))) | (t << ((x & 1) * 4))); }
static void mark_changes(void) {          // what LVGL's invalidated areas would give, at tile granularity
    for (int i = 0; i < W * H; i++) if (nib(fb, i) != nib(prev, i)) td[((i / W) / 4) * NTX + (i % W) / 32] = 1;
    memcpy(prev, fb, sizeof fb);
}
static int page(int x, int y, int off, int seed) {   // text-like content: dark strokes on white, a few grays
    const int yy = y + off;
    const unsigned h = (unsigned)((x / 3) * 2654435761u ^ (yy / 5) * 40503u ^ (unsigned)seed * 0x9E3779B1u);
    return ((yy % 17) < 11 && (h >> 28) < 6) ? ((h >> 20) & 1 ? 0 : (int)((h >> 22) & 15)) : 15;
}
static int sx5(int v) { return ((v & 31) ^ 16) - 16; }
int main(void) {
    static uint8_t dec[16384], exk[1536], exm[1792], tn4[2048], tau[4096], hot[H * W / 2], tn[NT], drv[H * W / 4], ra[H];
    static uint16_t cold[H * W], tp[NT], band[2 * NTY]; uint8_t st[4];
    pqbn_v4_build(dec, exk, st, 7.6f, 0.3f, 120.f);
    for (int i = 0; i < 4096; i++) tau[i] = (uint8_t)((i * 2654435761u) >> 28);
    pqbn_v4_build_motion(exm, tn4, tau);
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) setpx(x, y, page(x, y, 0, 1));
    memcpy(prev, fb, sizeof fb);
    v4ctx C = {W, H, NTX, NTY, dec, exk, {st[0], st[1], st[2], st[3]}, 2, fb, hot, cold, tau, td, tp, tn, drv, ra,
               1800, 0, 0, exm, tn4, band, 0, 1, 0, 0};
    pqbn_v4_init(&C, 1);
    static int dcref[H * W], dcmax[H * W];
    int max_motion[4] = {0}, first_motion = -1, last_motion = -1, backlog_max[4] = {0};
    for (int ph = 2; ph <= 700; ph++) {
        int seg = ph < 100 ? 0 : ph < 300 ? 1 : ph < 450 ? 2 : 3;
        if (ph == 31) {                                    // page turn: whole screen changes once
            for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) setpx(x, y, page(x, y, 0, 2));
            mark_changes();
        }
        if (ph >= 100 && ph < 220 && ph % 3 == 0) {        // scroll: 6 px per UI frame at 28 fps
            const int off = (ph - 99) * 2;
            for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) setpx(x, y, page(x, y, off, 3));
            mark_changes();
        }
        if (ph >= 300 && ph < 420 && ph % 3 == 0) {        // blinking cursor: one 32x4 tile region toggles
            for (int y = 100; y < 104; y++) for (int x = 64; x < 96; x++) setpx(x, y, (ph / 3) & 1 ? 0 : 15);
            mark_changes();
        }
        const uint32_t sp0 = C.stat_planned;
        pqbn_v4_phase(&C, (uint32_t)ph);
        if (getenv("DBG") && ((ph >= 30 && ph <= 50) || (ph >= 100 && ph <= 112) || (ph >= 215 && ph <= 280 && ph % 5 == 0)))
            printf("ph %3d planned %5u backlog %2d motion %3d\n", ph, C.stat_planned - sp0, C.backlog, C.n_motion);
        for (int y = 0; y < H; y++) if (ra[y]) for (int x = 0; x < W; x++) {
            const int c = (drv[y * (W / 4) + x / 4] >> (6 - 2 * (x & 3))) & 3, i = y * W + x;
            dcref[i] += (c == 2) - (c == 1); if (abs(dcref[i]) > dcmax[i]) dcmax[i] = abs(dcref[i]);
        }
        if (C.n_motion > max_motion[seg]) max_motion[seg] = C.n_motion;
        if (C.backlog > backlog_max[seg]) backlog_max[seg] = C.backlog;
        if (C.n_motion && first_motion < 0) first_motion = ph;
        if (C.n_motion) last_motion = ph;
    }
    // at the end everything is static: no motion tiles, and the DC in the state words matches the drive stream
    long checked = 0, bad = 0, unsettled = 0;
    for (int i = 0; i < H * W; i++) {
        const int h = nib(hot, i);
        if ((h & 7) != 0 && (h & 7) != 7) { unsettled++; continue; }
        if (dcmax[i] > 15) continue;
        checked++; if (sx5(cold[i] >> 8) != dcref[i]) bad++;
    }
    printf("page turn   : max motion tiles %3d (expect 0), max backlog %d phases\n", max_motion[0], backlog_max[0]);
    printf("scroll      : max motion tiles %3d of %d, motion from phase %d to %d (scroll 102..219), max backlog %d\n",
           max_motion[1], NT, first_motion, last_motion, backlog_max[1]);
    printf("cursor blink: max motion tiles %3d (expect 0), max backlog %d\n", max_motion[2], backlog_max[2]);
    printf("end         : motion tiles %d, pixels still planned %ld, DC mismatches %ld of %ld\n", C.n_motion, unsettled, bad, checked);
    return 0;
}
