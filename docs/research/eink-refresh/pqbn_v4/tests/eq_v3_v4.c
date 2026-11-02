// Equivalence check: with stiction off (all first-push sizes equal) the v4 plan/execute kernel must
// produce exactly the same drive as the v3 per-phase kernel on random target sequences.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
typedef struct { int W, H, ntx, nty; const uint8_t *lut; uint8_t step[4]; int marg;
  const uint8_t *fb4; uint16_t *st; const uint8_t *tau; uint8_t *tile_dirty; uint16_t *tile_busy; uint16_t *tile_last;
  uint8_t *drive; uint8_t *row_active; int8_t *u_dbg; } v3ctx;
typedef struct { int W, H, ntx, nty; const uint8_t *dec, *exk; uint8_t step[4]; int marg;
  const uint8_t *fb4; uint8_t *hot; uint16_t *cold; const uint8_t *tau; uint8_t *tile_dirty; uint16_t *tile_plan;
  uint8_t *tile_nmax; uint8_t *drive; uint8_t *row_active; int plan_budget, plan_cursor; uint32_t stat_planned; const uint8_t *exk_m, *tau_nib; uint16_t *band; int line_mode, auto_motion, backlog, n_motion; } v4ctx;
int pqbn_v3_build(uint8_t *lut, uint8_t *step_out, float T0, float s0, float taud);
int pqbn_v3_phase(v3ctx *C, uint32_t field);
int pqbn_v4_build(uint8_t *dec, uint8_t *exk, uint8_t *step_out, float T0, float s0, float taud);
int pqbn_v4_phase(v4ctx *C, uint32_t now);
void pqbn_v4_init(v4ctx *C, uint32_t now);
#define W 64
#define H 8
int main(int argc, char **argv) {
  const float s0 = argc > 1 ? atof(argv[1]) : 0.f; const int PH = argc > 2 ? atoi(argv[2]) : 20000;
  static uint8_t lut[32768], dec[16384], exk[1536], tau[4096], fb[H*W/2];
  uint8_t st3[4], st4[4];
  pqbn_v3_build(lut, st3, 7.6f, s0, 120.f);
  if (pqbn_v4_build(dec, exk, st4, 7.6f, s0, 120.f)) { printf("build fail\n"); return 1; }
  for (int i = 0; i < 4096; i++) tau[i] = (uint8_t)((i * 2654435761u) >> 28);
  memset(fb, 0xFF, sizeof fb);
  static uint16_t st[H*W], tb[4], tl[4], tp[4]; static uint8_t td3[4], td4[4], tn[4], d3[H*W/4], d4[H*W/4], r3[H], r4[H], hot[H*W/2];
  static uint16_t cold[H*W]; static int8_t ud[H*W];
  for (int i = 0; i < H*W; i++) st[i] = 0x83FF;
  v3ctx A = {W, H, 2, 2, lut, {st3[0], st3[1], st3[2], st3[3]}, 2, fb, st, tau, td3, tb, tl, d3, r3, ud};
  v4ctx B = {W, H, 2, 2, dec, exk, {st4[0], st4[1], st4[2], st4[3]}, 2, fb, hot, cold, tau, td4, tp, tn, d4, r4};
  static uint16_t bandB[2 * (H / 4)]; B.band = bandB;
  pqbn_v4_init(&B, 1);
  srand(7); long diffs = 0, pushes3 = 0, pushes4 = 0; int first = -1;
  for (int ph = 1; ph <= PH; ph++) {
    if (rand() % 3 == 0) {            // change some pixels
      int nchg = 1 + rand() % 40;
      for (int c = 0; c < nchg; c++) {
        int x = rand() % W, y = rand() % H, r = rand() % 4;
        int t = r == 0 ? 0 : r == 1 ? 15 : rand() % 16;
        uint8_t *b = &fb[y * (W/2) + x/2]; *b = (uint8_t)((*b & ~(15 << ((x&1)*4))) | (t << ((x&1)*4)));
        int ti = (y / 4) * 2 + x / 32; td3[ti] = td4[ti] = 1;
      }
    }
    pqbn_v3_phase(&A, ph); pqbn_v4_phase(&B, ph);
    if (getenv("TRX") && ph <= atoi(getenv("TRP"))) {
      const int x = atoi(getenv("TRX")), y = atoi(getenv("TRY")), t = (fb[y*(W/2)+x/2] >> ((x&1)*4)) & 15, i = y*W+x, ti = (y/4)*2+x/32;
      const int c3 = r3[y] ? (d3[y*(W/4)+x/4] >> (6 - 2*(x&3))) & 3 : 0, c4 = r4[y] ? (d4[y*(W/4)+x/4] >> (6 - 2*(x&3))) & 3 : 0;
      printf("ph %3d t4 %2d | v3 u %d q %3d cls %d dc %3d m %d | v4 u %d hot %x q0 %3d dc0 %3d c0 %d m %d tplan %d nmax %x\n",
             ph, t, c3, st[i] & 255, (st[i] >> 8) & 3, ((int16_t)(st[i] << 1)) >> 11, st[i] >> 15, c4,
             (hot[y*(W/2)+x/2] >> ((x&1)*4)) & 15, cold[i] & 255, (((cold[i] >> 8) & 31) ^ 16) - 16, (cold[i] >> 13) & 3, cold[i] >> 15, tp[ti], tn[ti]);
    }
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
      int c3 = r3[y] ? (d3[y*(W/4)+x/4] >> (6 - 2*(x&3))) & 3 : 0;
      int c4 = r4[y] ? (d4[y*(W/4)+x/4] >> (6 - 2*(x&3))) & 3 : 0;
      pushes3 += c3 != 0; pushes4 += c4 != 0;
      if (c3 != c4) { diffs++; if (first < 0) { first = ph; printf("first diff phase %d at (%d,%d): v3 %d v4 %d  state %04x hot %x cold %04x\n", ph, x, y, c3, c4, st[y*W+x], (hot[y*(W/2)+x/2] >> ((x&1)*4)) & 15, cold[y*W+x]); } }
    }
  }
  printf("s0=%.2f phases %d: pushes v3 %ld v4 %ld, differing pixel-phases %ld\n", s0, PH, pushes3, pushes4, diffs);
  return 0;
}
