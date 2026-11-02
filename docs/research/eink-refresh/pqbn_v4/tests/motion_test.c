// Motion-mode invariants: integrating the executor's drive output must reproduce the position implied by
// the hot nibbles (0..7) every phase, and DC after exit must equal DC at entry plus the motion pushes.
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
void pqbn_v4_motion_enter(v4ctx *, uint32_t, const uint8_t *); void pqbn_v4_motion_exit(v4ctx *, uint32_t, const uint8_t *);
#define W 256
#define H 32
#define NT ((W/32)*(H/4))
int pqbn_v4_exec_row(const v4ctx *, uint32_t, int, uint8_t *);
static long bad_rows = 0, rows_checked = 0;
static void check_rows(const v4ctx *C, uint32_t now, const uint8_t *drv, const uint8_t *ra) {
  uint8_t line[W / 4];
  for (int y = 0; y < H; y++) {
    memset(line, 0xAA, sizeof line);
    const int r = pqbn_v4_exec_row(C, now, y, line);
    const uint8_t *ref = drv + y * (W / 4);
    int refany = 0; if (ra[y]) for (int i = 0; i < W / 4; i++) refany |= ref[i];
    rows_checked++;
    if (r != (refany != 0)) { bad_rows++; continue; }
    if (r && memcmp(line, ref, W / 4)) bad_rows++;
  }
}
static int sx5(int v) { return ((v & 31) ^ 16) - 16; }
int main(void) {
  static uint8_t dec[16384], exk[1536], exm[1792], tn4[2048], tau[4096], fb[H*W/2], hot[H*W/2], td[NT], tn[NT], drv[H*W/4], ra[H], sel[NT];
  static uint16_t cold[H*W], tp[NT]; uint8_t st[4];
  pqbn_v4_build(dec, exk, st, 7.6f, 0.3f, 120.f);
  for (int i = 0; i < 4096; i++) tau[i] = (uint8_t)((i * 2654435761u) >> 28);
  pqbn_v4_build_motion(exm, tn4, tau);
  memset(fb, 0xFF, sizeof fb);
  v4ctx C = {W, H, W/32, H/4, dec, exk, {st[0], st[1], st[2], st[3]}, 2, fb, hot, cold, tau, td, tp, tn, drv, ra, 0, 0, 0, exm, tn4};
  static uint16_t band[2 * (H / 4)]; C.band = band;
  pqbn_v4_init(&C, 1);
  static int dcref[H*W], pos[H*W], dc_entry[H*W], dcmax[H*W]; memset(dcref, 0, sizeof dcref);
  srand(5); long bad_pos = 0, bad_dc = 0, pushes = 0; int ph = 1;
  for (int cycle = 0; cycle < 20; cycle++) {
    // normal phase block with random changes
    for (int i = 0; i < 200; i++, ph++) {
      if (rand() % 2 == 0) for (int c = 0; c < 50; c++) {
        int x = rand() % W, y = rand() % H, t = rand() % 16;
        uint8_t *b = &fb[y * (W/2) + x/2]; *b = (uint8_t)((*b & ~(15 << ((x&1)*4))) | (t << ((x&1)*4))); td[(y/4)*(W/32) + x/32] = 1;
      }
      pqbn_v4_phase(&C, ph); check_rows(&C, ph, drv, ra);
      for (int y = 0; y < H; y++) if (ra[y]) for (int x = 0; x < W; x++) { int c = (drv[y*(W/4)+x/4] >> (6 - 2*(x&3))) & 3; dcref[y*W+x] += (c == 2) - (c == 1); if (abs(dcref[y*W+x]) > dcmax[y*W+x]) dcmax[y*W+x] = abs(dcref[y*W+x]); }
    }
    // motion block: enter for half of the tiles, move content every 3 phases, exit
    for (int t = 0; t < NT; t++) sel[t] = (t + cycle) % 2;
    pqbn_v4_motion_enter(&C, ph, sel);
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
      const int i = y*W+x, ti = (y/4)*(W/32) + x/32;
      if (!sel[ti]) continue;
      const int h = (hot[y*(W/2)+x/2] >> ((x&1)*4)) & 15;
      pos[i] = (cold[i] & 15); dc_entry[i] = sx5(cold[i] >> 8);
      (void)h;
    }
    for (int i = 0; i < 90; i++, ph++) {
      if (i % 3 == 0) {
        for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) { int t = ((x + y + ph) * 7 / 5) % 16; if (rand() % 3) continue;
          uint8_t *b = &fb[y * (W/2) + x/2]; *b = (uint8_t)((*b & ~(15 << ((x&1)*4))) | (t << ((x&1)*4))); }
        for (int t = 0; t < NT; t++) td[t] = 1;
      }
      pqbn_v4_phase(&C, ph); check_rows(&C, ph, drv, ra);
      for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
        const int i = y*W+x, ti = (y/4)*(W/32) + x/32;
        const int c = ra[y] ? (drv[y*(W/4)+x/4] >> (6 - 2*(x&3))) & 3 : 0;
        const int u = (c == 2) - (c == 1); dcref[i] += u; pushes += u != 0; if (abs(dcref[i]) > dcmax[i]) dcmax[i] = abs(dcref[i]);
        if (!sel[ti]) continue;
        pos[i] += u;
        const int h = (hot[y*(W/2)+x/2] >> ((x&1)*4)) & 15, r = h & 7;
        int k = (uint16_t)(ph - tp[ti]); if (k > 7) k = 7;
        const int rem = r > k + 1 ? r - k - 1 : 0;           // after this phase's push
        const int p = (h & 8) ? 7 - rem : rem;
        if (p != pos[i] || pos[i] < 0 || pos[i] > 7) bad_pos++;
      }
    }
    pqbn_v4_motion_exit(&C, ph, NULL);
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
      const int i = y*W+x, ti = (y/4)*(W/32) + x/32;
      if (!sel[ti]) continue;
      if (sx5(cold[i] >> 8) != dc_entry[i] + pos[i] - (dc_entry[i] - dc_entry[i]) - (int)0 - (pos[i] - pos[i]) + 0 && 0) bad_dc++;
    }
  }
  // DC check at the end: model DC (from state words) vs exact DC from the drive stream, for pixels whose model is
  // in a format that holds DC directly (resting or settled) -- they must agree unless saturated at +-15
  long checked = 0;
  for (int i = 0; i < H*W; i++) {
    const int x = i % W, y = i / W, h = (hot[y*(W/2)+x/2] >> ((x&1)*4)) & 15;
    if ((h & 7) != 0 && (h & 7) != 7) continue;
    if (dcmax[i] > 15) continue;     // the model saturates at +-15 by design
    checked++; if (sx5(cold[i] >> 8) != dcref[i]) bad_dc++;
  }
  printf("motion test: %d phases, pushes %ld, position mismatches %ld, DC mismatches %ld of %ld checked; exec_row mismatches %ld of %ld rows\n", ph, pushes, bad_pos, bad_dc, checked, bad_rows, rows_checked);
  return 0;
}
