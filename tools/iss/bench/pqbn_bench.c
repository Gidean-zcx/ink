/* PQ-BN v4 (drivers/display/pqbn/pqbn_v4.c) on the ISS, ESP32-S31 model: per-phase cost of the planner
 * (pqbn_v4_phase) and per-row cost of the executor (pqbn_v4_exec_row, line mode) on the research's
 * full-resolution scenarios (docs/research/eink-refresh/pqbn_v4/rv/gen_scenarios.py: 1216x684 panel,
 * a UI frame every third phase). Memory as on the device: fb4, hot and cold in PSRAM (through the
 * D-cache), tables, tile state and the line buffer in internal SRAM, code in flash (XIP).
 * The kernel as fixed (D-110); the summary's research columns (motion, drive plane) no longer apply.
 * Frames come from the simulator's -i file (iss_input, untimed); the D-cache is written back and
 * invalidated before each new frame (the UI would have written it through the cache: pessimistic).
 *
 * Output (console): per phase "P ph cycles planned_px words rows_active row_cycles_sum row_cycles_max
 * motion_tiles backlog dirty_tiles dirty_motion_tiles motion_tiles_before" and, in line mode, "R ph c0 c1 ... c683" (cycles per row in scan order) and
 * "T ph t0 ... t170" (per 4-row tile band: -1 idle band, else tiles whose plan runs this phase).
 * Build and run: tools/iss/bench/run_pqbn.sh. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "iss_rt.h"
#include "panel_e0470a14.h"
#include "pqbn_v4.h"

#define PW 1216 /* panel width (native rows) */
#define PH 684  /* panel rows */
#define NTX (PW / PQBN_TPX)
#define NTY (PH / PQBN_TPY)
#define NT (NTX * NTY)
#define FB_BYTES (PH * PW / 2)
#define FRAME_BYTES (FB_BYTES + NT)
#ifndef PHASES
#define PHASES 60
#endif
static uint32_t rowc[PH];

int main(void) {
    uint8_t *fb4 = iss_psram_alloc(FB_BYTES, 64), *hot = iss_psram_alloc(FB_BYTES, 64);
    uint16_t *cold = iss_psram_alloc((size_t)PW * PH * 2, 64);
#ifdef DEC_PSRAM /* the simulator's layout: the planner's tables (decision, thresholds) in PSRAM */
    uint8_t *dec = iss_psram_alloc(16384, 64), *tau = iss_psram_alloc(4096, 64);
#else
    uint8_t *dec = iss_sram_alloc(16384, 64), *tau = iss_sram_alloc(4096, 64);
#endif
    uint8_t *exk = iss_sram_alloc(1536, 64);
    uint8_t *td = iss_sram_alloc(NT, 64), *tn = iss_sram_alloc(NT, 64), *map = iss_sram_alloc(NT, 64);
    uint16_t *tp = iss_sram_alloc(NT * 2, 64), *band = iss_sram_alloc(NTY * 4, 64);
    uint8_t *line = iss_sram_alloc(PW / 4, 64);
    if (!fb4 || !hot || !cold || !dec || !band || !line) {
        printf("alloc failed\n");
        return 1;
    }
    memset(td, 0, NT);
    pqbn_v4_ctx C;
    memset(&C, 0, sizeof C);
    if (pqbn_v4_build(&C, dec, exk, tau, &PQBN_PANEL_E0470A14)) {
        printf("table build failed\n");
        return 1;
    }
    C.W = PW;
    C.H = PH;
    C.ntx = NTX;
    C.nty = NTY;
    C.fb4 = fb4;
    C.hot = hot;
    C.cold = cold;
    C.tile_dirty = td;
    C.tile_plan = tp;
    C.tile_nmax = tn;
    C.band = band;

    int32_t nf = 0;
    iss_cache(ISS_CACHE_WB | ISS_CACHE_INV_D);
    if (iss_input(&nf, 0, 4) != 4 || nf < 1 || iss_input(fb4, 4, FB_BYTES) != FB_BYTES) {
        printf("no scenario input (iss -i scenario.bin)\n");
        return 1;
    }
    pqbn_v4_init(&C, 0);
    iss_cache_cold();
    printf("# W %d H %d phases %d frames %ld\n", PW, PH, PHASES, (long)nf);
    uint32_t planned0 = 0;
    for (int ph = 1; ph <= PHASES; ph++) {
        if (ph % 3 == 0 && ph / 3 < nf) { /* a new UI frame: framebuffer content + its dirty tiles */
            iss_cache(ISS_CACHE_WB | ISS_CACHE_INV_D);
            const uint32_t off = 4u + (uint32_t)(ph / 3) * FRAME_BYTES;
            iss_input(fb4, off, FB_BYTES);
            iss_input(map, off + FB_BYTES, NT);
            for (int i = 0; i < NT; i++) td[i] |= map[i];
        }
        int dirty = 0; /* untimed feature for the simulator's planner cost model */
        for (int i = 0; i < NT; i++) dirty += td[i] != 0;
        const uint64_t c0 = iss_cycles();
        iss_begin("pqbn.plan");
        pqbn_v4_phase(&C, (uint32_t)ph);
        iss_end2(C.stat_planned - planned0, 0);
        const uint64_t c1 = iss_cycles();
        const uint32_t planned = C.stat_planned - planned0;
        planned0 = C.stat_planned;
        uint64_t rsum = 0;
        uint32_t rmax = 0;
        int active = 0;
        iss_begin("pqbn.rows");
        for (int y = 0; y < PH; y++) {
            const uint64_t r0 = iss_cycles();
            active += pqbn_v4_exec_row(&C, (uint32_t)ph, y, line);
            const uint64_t r1 = iss_cycles();
            rowc[y] = (uint32_t)(r1 - r0);
            rsum += rowc[y];
            if (rowc[y] > rmax) rmax = rowc[y];
        }
        iss_end2((uint32_t)active, PH);
        printf("P %d %llu %u 0 %d %llu %u 0 0 %d 0 0\n", ph, (unsigned long long)(c1 - c0), (unsigned)planned, active,
               (unsigned long long)rsum, (unsigned)rmax, dirty);
        {
            printf("R %d", ph);
            for (int y = 0; y < PH; y++) printf(" %u", (unsigned)rowc[y]);
            printf("\n");
            /* untimed: per tile row, -1 if the row band is idle (exec_row returns at once), else the
             * number of tiles whose plan runs this phase (for the simulator's cost model) */
            printf("T %d", ph);
            for (int ty = 0; ty < NTY; ty++) {
                int n = -1;
                if ((int16_t)(C.band[2 * ty] - (uint16_t)ph) > 0) {
                    n = 0;
                    for (int tx = 0; tx < NTX; tx++) {
                        const int ti = ty * NTX + tx, k = (uint16_t)((uint16_t)ph - C.tile_plan[ti]);
                        n += ((C.tile_nmax[ti] & T_SWING) ? k >> 1 : k) < (C.tile_nmax[ti] & T_NMAX);
                    }
                }
                printf(" %d", n);
            }
            printf("\n");
        }
    }
    return 0;
}
