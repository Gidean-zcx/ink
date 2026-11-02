// Fling scenarios on the RV32IM emulator: plain v4 vs the hybrid (motion mode for the scrolling container
// while the content moves, then PQ-BN). Reports per-phase cost during motion and the refine after the stop.
#include "rv32emu.h"
#define W 1216
#define H 684
#define NTX (W / 32)
#define NTY (H / 4)
#define NT (NTX * NTY)
typedef struct { int W_, H_, ntx, nty; const uint8_t *dec, *exk; uint8_t step[4]; int marg;
    const uint8_t *fb4; uint8_t *hot; uint16_t *cold; const uint8_t *tau; uint8_t *tile_dirty; uint16_t *tile_plan;
    uint8_t *tile_nmax; uint8_t *drive; uint8_t *row_active; int plan_budget, plan_cursor; uint32_t stat_planned;
    const uint8_t *exk_m, *tau_nib; uint16_t *band; int line_mode, auto_motion, backlog, n_motion; } v4ctx;
int pqbn_v4_build(uint8_t *, uint8_t *, uint8_t *, float, float, float);
void pqbn_v4_build_motion(uint8_t *, uint8_t *, const uint8_t *);
void pqbn_v4_init(v4ctx *, uint32_t);
static void w32(uint8_t *M, uint32_t a, uint32_t v) { memcpy(M + a, &v, 4); }
static uint64_t cyc(const rv_cpu *c) { return c->instr + c->ld_use + 2 * c->br_taken + 2 * c->jumps + 3 * c->muldiv; }
int main(int argc, char **argv) {
    const char *bin = argv[1]; const uint32_t a_ph = strtoul(argv[2], 0, 16), a_en = strtoul(argv[3], 0, 16), a_ex = strtoul(argv[4], 0, 16);
    const char *dir = argv[5];
    uint8_t *M = calloc(1, RV_MEM);
    FILE *fb = fopen(bin, "rb"); size_t nr = fread(M + 0x1000, 1, 1 << 20, fb); fclose(fb); (void)nr;
    uint32_t p = 0x00100000;
#define ALLOC(var, sz) const uint32_t var = p; p = (p + (sz) + 63) & ~63u;
    ALLOC(CTX, 128) ALLOC(DEC, 16384) ALLOC(EXK, 1536) ALLOC(EXKM, 1792) ALLOC(TAUN, 2048) ALLOC(TAU, 4096) ALLOC(SEL, NT)
    ALLOC(FB4, H * W / 2) ALLOC(HOT, H * W / 2) ALLOC(COLD, H * W * 2) ALLOC(TD, NT) ALLOC(TP, NT * 2) ALLOC(TN, NT)
    ALLOC(DRV, H * W / 4) ALLOC(RA, H) ALLOC(BAND, NTY * 4)
    const uint32_t SP = 0x02F00000;
    uint8_t st[4];
    pqbn_v4_build(M + DEC, M + EXK, st, 7.6f, 0.3f, 120.f);
    for (int i = 0; i < 4096; i++) M[TAU + i] = (uint8_t)((((i & 63) * 37) + ((i >> 6) * 101)) & 15);
    pqbn_v4_build_motion(M + EXKM, M + TAUN, M + TAU);
    w32(M, CTX + 0, W); w32(M, CTX + 4, H); w32(M, CTX + 8, NTX); w32(M, CTX + 12, NTY); w32(M, CTX + 16, DEC); w32(M, CTX + 20, EXK);
    memcpy(M + CTX + 24, st, 4); w32(M, CTX + 28, 2); w32(M, CTX + 32, FB4); w32(M, CTX + 36, HOT); w32(M, CTX + 40, COLD);
    w32(M, CTX + 44, TAU); w32(M, CTX + 48, TD); w32(M, CTX + 52, TP); w32(M, CTX + 56, TN); w32(M, CTX + 60, DRV); w32(M, CTX + 64, RA);
    w32(M, CTX + 80, EXKM); w32(M, CTX + 84, TAUN); w32(M, CTX + 88, BAND);
    static uint8_t h_fb4[H * W / 2], h_hot[H * W / 2], h_td[NT], h_tn[NT], h_drv[H * W / 4], h_ra[H]; static uint16_t h_cold[H * W], h_tp[NT], h_band[2 * NTY];
    v4ctx HC = {W, H, NTX, NTY, M + DEC, M + EXK, {st[0], st[1], st[2], st[3]}, 2, h_fb4, h_hot, h_cold, M + TAU, h_td, h_tp, h_tn, h_drv, h_ra, 0, 0, 0, 0, 0, h_band};
    const char *names[] = {"scroll_half", "scroll_full", "scroll_full_14fps", "covers_scroll"};
    const char *labels[] = {"list scroll, half screen", "full-screen scroll 28 fps", "full-screen scroll 14 fps", "cover grid scroll"};
    printf("fling scenarios (70 UI frames at ~28 fps, start 60-70 px/frame, decaying 7%%/frame), 12 ms phases, one RV32IM core @320 MHz\n");
    for (int s = 0; s < 4; s++) {
        char path[512]; snprintf(path, sizeof path, "%s/%s.bin", dir, names[s]);
        FILE *fp = fopen(path, "rb"); int nf; if (fread(&nf, 4, 1, fp) != 1) return 1;
        const size_t fbB = (size_t)H * (W / 2), frB = fbB + NT;
        uint8_t *frames = malloc((size_t)nf * frB); if (fread(frames, 1, (size_t)nf * frB, fp) != (size_t)nf * frB) return 1; fclose(fp);
        int last = 0; memset(M + SEL, 0, NT);
        for (int f = 1; f < nf; f++) { int any = 0; for (int i = 0; i < NT; i++) if (frames[(size_t)f * frB + fbB + i]) { any = 1; M[SEL + i] = 1; } if (any) last = f; }
        const int stop_ph = 3 * last, exit_ph = stop_ph + 3, end_ph = exit_ph + 90;
        const int budget = getenv("BUDGET") ? atoi(getenv("BUDGET")) : 20000;
        for (int mode = 0; mode < 3; mode++) {
            memcpy(M + FB4, frames, fbB); memcpy(h_fb4, frames, fbB);
            pqbn_v4_init(&HC, 0);
            memcpy(M + HOT, h_hot, sizeof h_hot); memcpy(M + COLD, h_cold, sizeof h_cold);
            memcpy(M + TD, h_td, NT); memcpy(M + TP, h_tp, NT * 2); memcpy(M + TN, h_tn, NT); memcpy(M + BAND, h_band, sizeof h_band); w32(M, CTX + 68, 0); w32(M, CTX + 72, 0);
            // mode 0: PQ-BN, no budget; 1: scripted hybrid; 2: automatic modes with a planner budget
            w32(M, CTX + 68, mode == 2 ? (uint32_t)budget : 0); w32(M, CTX + 96, mode == 2); w32(M, CTX + 100, 0); w32(M, CTX + 104, 0);
            double motion_tile_phases = 0;
            rv_cpu c = {0}; c.m = M;
            static uint32_t hist[1 << 16]; const char *pf = getenv("PROF");
            const int do_prof = pf && mode == 1 && s == 1; if (do_prof) memset(hist, 0, sizeof hist);
            double mot_sum = 0, mot_max = 0, post_sum = 0, post_max = 0, tr_cost = 0; int mot_n = 0, post_last = 0;
            for (int ph = 1; ph <= end_ph; ph++) {
                if (ph % 3 == 0 && ph / 3 < nf) {
                    const uint8_t *fr = frames + (size_t)(ph / 3) * frB;
                    memcpy(M + FB4, fr, fbB);
                    for (int i = 0; i < NT; i++) M[TD + i] |= fr[fbB + i];
                }
                uint64_t c0 = cyc(&c);
                if (mode == 1 && ph == 3) rv_call(&c, a_en, CTX, ph, SEL, 0, SP);
                if (mode == 1 && ph == exit_ph) rv_call(&c, a_ex, CTX, ph, 0, 0, SP);
                uint64_t c1 = cyc(&c); tr_cost += (double)(c1 - c0);
                c.pch = (do_prof && ph > 3 && ph <= stop_ph) ? hist : NULL;
                rv_call(&c, a_ph, CTX, ph, 0, 0, SP);
                c.pch = NULL;
                { int32_t nm; memcpy(&nm, M + CTX + 104, 4); motion_tile_phases += nm > 0; }
                const double ms = (double)(cyc(&c) - c0) / 320e3;
                if (ph >= 3 && ph <= stop_ph + 2) { mot_sum += ms; mot_n++; if (ms > mot_max && !(mode == 1 && ph == 3)) mot_max = ms; }
                else if (ph > stop_ph + 2) { post_sum += ms; if (ms > post_max) post_max = ms; if (ms > 1.0) post_last = ph; }
            }
            if (do_prof) { FILE *fo = fopen(pf, "w"); for (int i = 0; i < (1 << 16); i++) if (hist[i]) fprintf(fo, "%x %u\n", 0x1000 + 4 * i, hist[i]); fclose(fo); }
            if (mode == 2) printf("  (auto, budget %d px/phase: motion tiles present in %.0f phases)\n", budget, motion_tile_phases);
            printf("%-26s %-7s | while moving (%3d phases): avg %6.2f ms  max %6.2f ms (excl. switch) | after stop: refine %6.1f ms of work, worst phase %6.1f ms, busy until +%d phases | mode switches %5.1f ms\n",
                   labels[s], mode == 0 ? "PQ-BN" : mode == 1 ? "hybrid" : "auto", mot_n, mot_sum / mot_n, mot_max, post_sum, post_max, post_last ? post_last - stop_ph : 0, tr_cost / 320e3);
            fflush(stdout);
        }
        free(frames);
    }
    return 0;
}
