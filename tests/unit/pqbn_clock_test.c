/* PQ-BN kernel across rests (audit AUD-011, follow-up F-8): the simulator moves the phase clock by up
 * to 255 phases over a rest without running drive phases; pqbn_v4_skip must do the upkeep those phases
 * would have done. Checks, on a 64 x 512 panel (one row per timestamp-sweep index):
 *   1. short drives separated by long rests: every settled pixel still reads as long settled (its
 *      10-bit timestamp did not wrap; without the upkeep 92 % of them read as just moved);
 *   2. a rest by pqbn_v4_skip leaves the same state as running those phases with nothing to drive,
 *      also across a multiple of 4096. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pqbn_v4.h"

#define PW 64
#define PH 512

static int fails;

static uint8_t *alloc(size_t n) {
    uint8_t *p = calloc(1, n);
    if (!p) abort();
    return p;
}

static int lvl(int r) {
    static const unsigned char t[64] = {0, 1, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5,
                                        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
                                        6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6};
    return r >= 64 ? 7 : t[r];
}

static void setup(pqbn_v4_ctx *C, uint32_t now) {
    int ntx = PW / PQBN_TPX, nty = PH / PQBN_TPY, nt = ntx * nty;
    memset(C, 0, sizeof *C);
    uint8_t *dec = alloc(16384), *exk = alloc(6 * 256), *exk_m = alloc(7 * 256), *tau = alloc(4096), *tau_nib = alloc(2048);
    if (pqbn_v4_build(dec, exk, C->step, 7.6f, 0.3f, 120.0f)) abort();
    for (int i = 0; i < 4096; i++) tau[i] = (uint8_t)((((i & 63) * 37) + ((i >> 6) * 101)) & 15);
    pqbn_v4_build_motion(exk_m, tau_nib, tau);
    C->W = PW;
    C->H = PH;
    C->ntx = ntx;
    C->nty = nty;
    C->marg = 2;
    C->dec = dec;
    C->exk = exk;
    C->exk_m = exk_m;
    C->tau = tau;
    C->tau_nib = tau_nib;
    uint8_t *fb4 = alloc((size_t)PW * PH / 2);
    memset(fb4, 0xff, (size_t)PW * PH / 2); /* white everywhere, as at init */
    C->fb4 = fb4;
    C->hot = alloc((size_t)PW * PH / 2);
    C->cold = (uint16_t *)alloc((size_t)PW * PH * 2);
    C->tile_dirty = alloc(nt);
    C->tile_plan = (uint16_t *)alloc(nt * 2);
    C->tile_nmax = alloc(nt);
    C->band = (uint16_t *)alloc((size_t)nty * 4);
    C->row_active = alloc(PH);
    C->drive = alloc((size_t)PW * PH / 4);
    C->plan_budget = 8000;
    pqbn_v4_init(C, now);
}

static long wrapped(const pqbn_v4_ctx *C, uint32_t now) { /* settled pixels whose rest reads below 64 phases */
    long wrong = 0;
    for (int y = 0; y < PH; y++)
        for (int x = 0; x < PW; x++) {
            if (((C->hot[(y * PW + x) / 2] >> ((x & 1) * 4)) & 7) != 7) continue;
            uint16_t s = C->cold[y * PW + x];
            int ts = (s & 255) | ((s >> 5) & 0x300);
            if (lvl((int)((now - (uint32_t)ts) & 1023)) != 7) wrong++;
        }
    return wrong;
}

#define CHECK(c, ...)                                                                                                  \
    do {                                                                                                               \
        if (!(c)) {                                                                                                    \
            fails++;                                                                                                   \
            fprintf(stderr, "FAIL: " __VA_ARGS__);                                                                     \
            fputc('\n', stderr);                                                                                       \
        }                                                                                                              \
    } while (0)

int main(void) {
    static pqbn_v4_ctx A, B;
    /* 1. drives of 20 phases, rests capped at 255 phases (pq_advance_idle), three cycles and more */
    setup(&A, 0);
    uint32_t now = 1;
    for (int c = 0; c < 12; c++) {
        for (int k = 0; k < 20; k++) pqbn_v4_phase(&A, now++);
        pqbn_v4_skip(&A, now - 1, now - 1 + 255);
        now += 255;
    }
    long w = wrapped(&A, now - 1);
    CHECK(w == 0, "after drives and rests %ld settled pixels read as just moved", w);

    /* 2. a rest by pqbn_v4_skip == the same phases run with nothing to drive (from 4000: across 4096) */
    setup(&A, 3990);
    setup(&B, 3990);
    for (uint32_t p = 3991; p <= 4000; p++) {
        pqbn_v4_phase(&A, p);
        pqbn_v4_phase(&B, p);
    }
    for (uint32_t p = 4001; p <= 4255; p++) pqbn_v4_phase(&A, p);
    pqbn_v4_skip(&B, 4000, 4255);
    CHECK(!memcmp(A.cold, B.cold, (size_t)PW * PH * 2), "the settled pixels' timestamps differ after a skipped rest");
    CHECK(!memcmp(A.hot, B.hot, (size_t)PW * PH / 2), "the pixel states differ after a skipped rest");
    CHECK(!memcmp(A.tile_plan, B.tile_plan, (size_t)(PW / PQBN_TPX) * (PH / PQBN_TPY) * 2), "the tile ages differ after a skipped rest");
    CHECK(!memcmp(A.band, B.band, (size_t)(PH / PQBN_TPY) * 4), "the band clocks differ after a skipped rest");
    if (fails) return 1;
    printf("pqbn clock: rests keep timestamps unambiguous and equal running the phases\n");
    return 0;
}
