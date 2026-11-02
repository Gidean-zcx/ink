// rev A (pqbn_v4a.c) vs rev B (pqbn_v4.c): identical hot nibbles and drive on random target sequences with stiction on
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
typedef struct { int W, H, ntx, nty; const uint8_t *dec, *exk; uint8_t step[4]; int marg;
  const uint8_t *fb4; uint8_t *hot; uint16_t *cold; const uint8_t *tau; uint8_t *tile_dirty; uint16_t *tile_plan;
  uint8_t *tile_nmax; uint8_t *drive; uint8_t *row_active; int plan_budget, plan_cursor; uint32_t stat_planned; const uint8_t *exk_m, *tau_nib; uint16_t *band; int line_mode, auto_motion, backlog, n_motion; } v4ctx;
int pqbn_v4_build(uint8_t *, uint8_t *, uint8_t *, float, float, float); int pqbn_v4_phase(v4ctx *, uint32_t); void pqbn_v4_init(v4ctx *, uint32_t);
int pqbn_v4a_build(uint8_t *, uint8_t *, uint8_t *, float, float, float); int pqbn_v4a_phase(v4ctx *, uint32_t); void pqbn_v4a_init(v4ctx *, uint32_t);
#define W 256
#define H 32
#define NT ((W/32)*(H/4))
int main(int argc, char **argv) {
  const int PH = argc > 1 ? atoi(argv[1]) : 30000;
  static uint8_t dec[16384], exk[1536], tau[4096], fb[H*W/2];
  uint8_t st[4], st2[4];
  pqbn_v4_build(dec, exk, st, 7.6f, 0.3f, 120.f);
  static uint8_t deca[16384], exka[1536]; pqbn_v4a_build(deca, exka, st2, 7.6f, 0.3f, 120.f);
  if (memcmp(dec, deca, sizeof dec) || memcmp(exk, exka, sizeof exk)) { printf("tables differ\n"); return 1; }
  for (int i = 0; i < 4096; i++) tau[i] = (uint8_t)((i * 2654435761u) >> 28);
  memset(fb, 0xFF, sizeof fb);
  static uint8_t hA[H*W/2], hB[H*W/2], tdA[NT], tdB[NT], tnA[NT], tnB[NT], dA[H*W/4], dB[H*W/4], rA[H], rB[H];
  static uint16_t cA[H*W], cB[H*W], tpA[NT], tpB[NT];
  v4ctx A = {W, H, W/32, H/4, dec, exk, {st[0], st[1], st[2], st[3]}, 2, fb, hA, cA, tau, tdA, tpA, tnA, dA, rA};
  v4ctx B = {W, H, W/32, H/4, dec, exk, {st[0], st[1], st[2], st[3]}, 2, fb, hB, cB, tau, tdB, tpB, tnB, dB, rB};
  static uint16_t bandB[2 * (H / 4)]; B.band = bandB;
  pqbn_v4a_init(&A, 1); pqbn_v4_init(&B, 1);
  srand(11); long dh = 0, dd = 0, pushes = 0;
  for (int ph = 1; ph <= PH; ph++) {
    const int burst = (ph / 500) % 3;           // vary activity: quiet, moderate, heavy
    if (rand() % (burst == 0 ? 20 : burst == 1 ? 3 : 1) == 0) {
      int nchg = 1 + rand() % (burst == 2 ? 400 : 60);
      for (int c = 0; c < nchg; c++) {
        int x = rand() % W, y = rand() % H, r = rand() % 4, t = r == 0 ? 0 : r == 1 ? 15 : rand() % 16;
        uint8_t *b = &fb[y * (W/2) + x/2]; *b = (uint8_t)((*b & ~(15 << ((x&1)*4))) | (t << ((x&1)*4)));
        tdA[(y / 4) * (W/32) + x / 32] = tdB[(y / 4) * (W/32) + x / 32] = 1;
      }
    }
    pqbn_v4a_phase(&A, ph); pqbn_v4_phase(&B, ph);
    dh += memcmp(hA, hB, sizeof hA) != 0;
    for (int y = 0; y < H; y++) {
      if (rA[y] != rB[y]) { dd++; continue; }
      if (rA[y]) { if (memcmp(dA + y*(W/4), dB + y*(W/4), W/4)) dd++; for (int i = 0; i < W/4; i++) pushes += (dA[y*(W/4)+i] != 0); }
    }
  }
  printf("%d phases: phases with hot-plane differences %ld, drive-row differences %ld (drive bytes with pushes %ld)\n", PH, dh, dd, pushes);
  return 0;
}
