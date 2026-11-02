// Runs the RV32IM builds of the old event-driven kernel, v3 and v4 on the full-resolution Metalio
// scenarios (1216x684, UI rotated onto the landscape panel) and reports per-phase cost:
// RISC-V instructions, estimated cycles (simple in-order model), and bytes touched in PSRAM vs SRAM.
// Mode "verify": runs v4 natively and in the emulator side by side and compares every state byte.
#include "rv32emu.h"
#include <math.h>

#define W 1216
#define H 684
#define NTX (W / 32)
#define NTY (H / 4)
#define NT (NTX * NTY)

void pqbn_lut2_build(uint8_t *lut2, int *step, float T0, float s0, float taud);
int pqbn_v3_build(uint8_t *lut, uint8_t *step_out, float T0, float s0, float taud);
typedef struct { int W_, H_, ntx, nty; const uint8_t *dec, *exk; uint8_t step[4]; int marg;
    const uint8_t *fb4; uint8_t *hot; uint16_t *cold; const uint8_t *tau; uint8_t *tile_dirty; uint16_t *tile_plan;
    uint8_t *tile_nmax; uint8_t *drive; uint8_t *row_active; int plan_budget, plan_cursor; uint32_t stat_planned;
    const uint8_t *exk_m, *tau_nib; uint16_t *band; int line_mode, auto_motion, backlog, n_motion; } v4ctx;
int pqbn_v4_build(uint8_t *dec, uint8_t *exk, uint8_t *step_out, float T0, float s0, float taud);
void pqbn_v4_build_motion(uint8_t *exk_m, uint8_t *tau_nib, const uint8_t *tau);
int pqbn_v4_phase(v4ctx *C, uint32_t now);
void pqbn_v4_init(v4ctx *C, uint32_t now);

static void w32(uint8_t *M, uint32_t a, uint32_t v) { memcpy(M + a, &v, 4); }
static uint64_t est_cycles(const rv_cpu *c) { return c->instr + c->ld_use + 2 * c->br_taken + 2 * c->jumps + 3 * c->muldiv; }
typedef struct { uint64_t instr, cyc, psram, sram, maxcyc; } acc_t;
static void snap_counts(const rv_cpu *c, uint64_t *ins, uint64_t *cyc, uint64_t *ps, uint64_t *sr) {
    *ins = c->instr; *cyc = est_cycles(c); *ps = *sr = 0;
    for (int i = 0; i < c->nreg; i++) { const uint64_t b = c->reg[i].rd + c->reg[i].wr; if (!strncmp(c->reg[i].name, "PSRAM", 5)) *ps += b; else *sr += b; }
}

static uint8_t *load_scen(const char *dir, const char *name, int *nf) {
    char path[512]; snprintf(path, sizeof path, "%s/%s.bin", dir, name);
    FILE *fp = fopen(path, "rb"); if (!fp) { perror(path); exit(1); }
    if (fread(nf, 4, 1, fp) != 1) exit(1);
    const size_t fr = (size_t)H * (W / 2) + NT;
    uint8_t *frames = malloc((size_t)*nf * fr);
    if (fread(frames, 1, (size_t)*nf * fr, fp) != (size_t)*nf * fr) exit(1);
    fclose(fp);
    return frames;
}

int main(int argc, char **argv) {
    // argv: binary addr_old addr_v3 addr_v4 scen_dir nphases [verify]
    const char *bin = argv[1];
    const uint32_t a_old = (uint32_t)strtoul(argv[2], 0, 16), a_v3 = (uint32_t)strtoul(argv[3], 0, 16), a_v4 = (uint32_t)strtoul(argv[4], 0, 16);
    const char *dir = argv[5]; const int PH = atoi(argv[6]);
    const int verify = argc > 7 && !strcmp(argv[7], "verify");
    const int plan_budget = getenv("BUDGET") ? atoi(getenv("BUDGET")) : 0;
    const int auto_motion = getenv("AUTO") ? atoi(getenv("AUTO")) : 0;
    const uint32_t a_row = getenv("LINE") ? (uint32_t)strtoul(getenv("LINE"), 0, 16) : 0;   // address of pqbn_v4_exec_row
    const int prof = argc > 8 && !strcmp(argv[7], "prof");      // prof <scenario index> <out file>: v4 only, address histogram
    uint8_t *M = calloc(1, RV_MEM);
    FILE *fb = fopen(bin, "rb"); size_t n = fread(M + 0x1000, 1, 1 << 20, fb); fclose(fb); (void)n;

    uint32_t p = 0x00100000;
#define ALLOC(var, sz) const uint32_t var = p; p = (p + (sz) + 63) & ~63u;
    // v4
    ALLOC(CTX4, 128) ALLOC(DEC, 16384) ALLOC(EXK, 1536) ALLOC(TAU, 4096) ALLOC(FB4, H * W / 2) ALLOC(HOT, H * W / 2)
    ALLOC(COLD, H * W * 2) ALLOC(TD4, NT) ALLOC(TP4, NT * 2) ALLOC(TN4, NT) ALLOC(DRV4, H * W / 4) ALLOC(RA4, H) ALLOC(BAND4, NTY * 4) ALLOC(EXKM, 1792) ALLOC(TAUN, 2048)
    // v3
    ALLOC(CTX3, 64) ALLOC(LUT3, 32768) ALLOC(ST3, H * W * 2) ALLOC(TD3, NT) ALLOC(TB3, NT * 2) ALLOC(TL3, NT * 2)
    ALLOC(DRV3, H * W / 4) ALLOC(RA3, H)
    // old
    ALLOC(CTXO, 64) ALLOC(LUT2, 65536) ALLOC(STEP2, 16) ALLOC(TGT, H * W) ALLOC(PRV, H * W) ALLOC(TAUO, H * W)
    ALLOC(Q8, H * W) ALLOC(CLS, H * W) ALLOC(MF, H * W) ALLOC(BZ, H * W) ALLOC(DC, H * W) ALLOC(OUT, H * W / 4) ALLOC(RB, H * 4)
    if (p > 0x02E00000) { fprintf(stderr, "memory map too large\n"); return 1; }
    const uint32_t SP = 0x02F00000;

    rv_cpu co = {0}, c3 = {0}, c4 = {0}; co.m = c3.m = c4.m = M;
    rv_add_region(&c4, FB4, FB4 + H * W / 2, "PSRAM fb4"); rv_add_region(&c4, HOT, HOT + H * W / 2, "PSRAM hot");
    rv_add_region(&c4, COLD, COLD + H * W * 2, "PSRAM cold"); rv_add_region(&c4, DRV4, DRV4 + H * W / 4, "PSRAM drive");
    rv_add_region(&c4, DEC, DEC + 16384, "SRAM dec"); rv_add_region(&c4, EXK, EXK + 1536, "SRAM exk"); rv_add_region(&c4, TAU, TAU + 4096, "SRAM tau");
    rv_add_region(&c4, TD4, RA4 + H, "SRAM tiles"); rv_add_region(&c4, SP - 65536, SP + 64, "SRAM stack"); rv_add_region(&c4, CTX4, CTX4 + 128, "SRAM ctx");
    rv_add_region(&c3, FB4, FB4 + H * W / 2, "PSRAM fb4"); rv_add_region(&c3, ST3, ST3 + H * W * 2, "PSRAM state");
    rv_add_region(&c3, DRV3, DRV3 + H * W / 4, "PSRAM drive"); rv_add_region(&c3, LUT3, LUT3 + 32768, "SRAM lut");
    rv_add_region(&c3, TAU, TAU + 4096, "SRAM tau"); rv_add_region(&c3, TD3, RA3 + H, "SRAM tiles");
    rv_add_region(&c3, SP - 65536, SP + 64, "SRAM stack"); rv_add_region(&c3, CTX3, CTX3 + 64, "SRAM ctx");
    rv_add_region(&co, TGT, DC + H * W, "PSRAM per-pixel arrays"); rv_add_region(&co, OUT, OUT + H * W / 4, "PSRAM drive");
    rv_add_region(&co, LUT2, LUT2 + 65536, "SRAM lut"); rv_add_region(&co, STEP2, STEP2 + 16, "SRAM step");
    rv_add_region(&co, RB, RB + H * 4, "SRAM row_busy"); rv_add_region(&co, SP - 65536, SP + 64, "SRAM stack"); rv_add_region(&co, CTXO, CTXO + 64, "SRAM ctx");

    // tables
    uint8_t step4[4], step3[4]; int step2[4];
    if (pqbn_v4_build(M + DEC, M + EXK, step4, 7.6f, 0.3f, 120.f)) { fprintf(stderr, "v4 build failed\n"); return 1; }
    pqbn_v3_build(M + LUT3, step3, 7.6f, 0.3f, 120.f);
    pqbn_lut2_build(M + LUT2, step2, 7.6f, 0.3f, 120.f); memcpy(M + STEP2, step2, 16);
    for (int i = 0; i < 4096; i++) M[TAU + i] = (uint8_t)((((i & 63) * 37) + ((i >> 6) * 101)) & 15);
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) M[TAUO + y * W + x] = M[TAU + (y & 63) * 64 + (x & 63)];
    pqbn_v4_build_motion(M + EXKM, M + TAUN, M + TAU);
    // ctx structs (ilp32)
    w32(M, CTX4 + 0, W); w32(M, CTX4 + 4, H); w32(M, CTX4 + 8, NTX); w32(M, CTX4 + 12, NTY); w32(M, CTX4 + 16, DEC); w32(M, CTX4 + 20, EXK);
    memcpy(M + CTX4 + 24, step4, 4); w32(M, CTX4 + 28, 2); w32(M, CTX4 + 32, FB4); w32(M, CTX4 + 36, HOT); w32(M, CTX4 + 40, COLD);
    w32(M, CTX4 + 44, TAU); w32(M, CTX4 + 48, TD4); w32(M, CTX4 + 52, TP4); w32(M, CTX4 + 56, TN4); w32(M, CTX4 + 60, DRV4); w32(M, CTX4 + 64, RA4); w32(M, CTX4 + 88, BAND4); w32(M, CTX4 + 80, EXKM); w32(M, CTX4 + 84, TAUN);
    w32(M, CTX3 + 0, W); w32(M, CTX3 + 4, H); w32(M, CTX3 + 8, NTX); w32(M, CTX3 + 12, NTY); w32(M, CTX3 + 16, LUT3);
    memcpy(M + CTX3 + 20, step3, 4); w32(M, CTX3 + 24, 2); w32(M, CTX3 + 28, FB4); w32(M, CTX3 + 32, ST3); w32(M, CTX3 + 36, TAU);
    w32(M, CTX3 + 40, TD3); w32(M, CTX3 + 44, TB3); w32(M, CTX3 + 48, TL3); w32(M, CTX3 + 52, DRV3); w32(M, CTX3 + 56, RA3); w32(M, CTX3 + 60, 0);
    w32(M, CTXO + 0, W); w32(M, CTXO + 4, H); w32(M, CTXO + 8, 2); w32(M, CTXO + 12, LUT2); w32(M, CTXO + 16, STEP2);
    w32(M, CTXO + 20, TGT); w32(M, CTXO + 24, PRV); w32(M, CTXO + 28, TAUO); w32(M, CTXO + 32, Q8); w32(M, CTXO + 36, CLS);
    w32(M, CTXO + 40, MF); w32(M, CTXO + 44, BZ); w32(M, CTXO + 48, DC); w32(M, CTXO + 52, OUT); w32(M, CTXO + 56, RB);

    // host-side v4 (init, and the reference run for verify)
    static uint8_t h_fb4[H * W / 2], h_hot[H * W / 2], h_td[NT], h_tn[NT], h_drv[H * W / 4], h_ra[H];
    static uint16_t h_cold[H * W], h_tp[NT], h_band[2 * NTY];
    v4ctx HC = {W, H, NTX, NTY, M + DEC, M + EXK, {step4[0], step4[1], step4[2], step4[3]}, 2, h_fb4, h_hot, h_cold, M + TAU,
                h_td, h_tp, h_tn, h_drv, h_ra, 0, 0, 0, 0, 0, h_band};

    const char *names[] = {"static", "ai_stream", "page_turn", "scroll_half", "scroll_full", "scroll_full_14fps", "covers_scroll"};
    const char *labels[] = {"static page", "AI text streaming", "page turn", "list scroll, half screen",
                            "full-screen scroll 28 fps", "full-screen scroll 14 fps", "cover grid scroll"};
    const int ns = (verify || prof) ? 1 : 7;
    static uint32_t hist[1 << 16];
    if (prof) c4.pch = hist;
    if (!verify) {
        printf("per 12 ms phase, averaged over %d phases; cycles = simple in-order estimate (instr + load-use + 2/taken branch + 2/jump + 3/mul)\n", PH);
        printf("%-26s | %-31s | %-31s | %-31s | %s\n", "", "old: Minstr  ms@320MHz  PSRAM MB", "v3:  Minstr  ms@320MHz  PSRAM MB",
               "v4:  Minstr  ms@320MHz  PSRAM MB", "v4 worst phase ms");
    }
    for (int si = 0; si < ns; si++) {
        const int s = verify ? 4 : prof ? atoi(argv[8]) : si;
        int nf; uint8_t *frames = load_scen(dir, names[s], &nf);
        const size_t fbB = (size_t)H * (W / 2), frB = fbB + NT;
        // settled start on frame 0
        memcpy(M + FB4, frames, fbB); memcpy(h_fb4, frames, fbB);
        for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
            const int t4 = (M[FB4 + y * (W / 2) + (x >> 1)] >> ((x & 1) << 2)) & 15, i = y * W + x;
            M[TGT + i] = M[PRV + i] = (uint8_t)t4; M[Q8 + i] = (uint8_t)(t4 * 17); M[CLS + i] = 3; M[MF + i] = 1; M[BZ + i] = 0; M[DC + i] = 0;
            const uint16_t sw = (uint16_t)(0x8000 | 0x0300 | (t4 * 17)); memcpy(M + ST3 + 2 * i, &sw, 2);
        }
        memset(M + TD3, 0, NT); memset(M + TB3, 0, NT * 2); memset(M + TL3, 0, NT * 2); memset(M + RB, 0, H * 4);
        pqbn_v4_init(&HC, 0);
        w32(M, CTX4 + 68, (uint32_t)plan_budget); w32(M, CTX4 + 72, 0); w32(M, CTX4 + 76, 0); HC.plan_budget = plan_budget;
        w32(M, CTX4 + 92, a_row ? 1 : 0); w32(M, CTX4 + 96, (uint32_t)auto_motion); w32(M, CTX4 + 100, 0); w32(M, CTX4 + 104, 0);
        HC.exk_m = M + EXKM; HC.tau_nib = M + TAUN; HC.auto_motion = auto_motion;
        double row_sum = 0, row_max = 0, row_phase_max = 0;
        memcpy(M + HOT, h_hot, sizeof h_hot); memcpy(M + COLD, h_cold, sizeof h_cold);
        memcpy(M + TD4, h_td, NT); memcpy(M + TP4, h_tp, NT * 2); memcpy(M + TN4, h_tn, NT); memcpy(M + BAND4, h_band, sizeof h_band);
        acc_t ao = {0}, a3 = {0}, a4 = {0};
        long mism = 0;
        for (int ph = 1; ph <= PH; ph++) {
            if (ph % 3 == 0 && ph / 3 < nf) {
                const uint8_t *fr = frames + (size_t)(ph / 3) * frB;
                memcpy(M + FB4, fr, fbB); memcpy(h_fb4, fr, fbB);
                for (int i = 0; i < NT; i++) { M[TD3 + i] |= fr[fbB + i]; M[TD4 + i] |= fr[fbB + i]; h_td[i] |= fr[fbB + i]; }
                for (int y = 0; y < H; y++) for (int x = 0; x < W; x++)
                    M[TGT + y * W + x] = (M[FB4 + y * (W / 2) + (x >> 1)] >> ((x & 1) << 2)) & 15;
            }
            uint64_t i0, y0, p0, s0_, i1, y1, p1, s1;
            if (!verify && !prof) {
                snap_counts(&co, &i0, &y0, &p0, &s0_); rv_call(&co, a_old, CTXO, ph, 0, 0, SP); snap_counts(&co, &i1, &y1, &p1, &s1);
                ao.instr += i1 - i0; ao.cyc += y1 - y0; ao.psram += p1 - p0; if (y1 - y0 > ao.maxcyc) ao.maxcyc = y1 - y0;
                snap_counts(&c3, &i0, &y0, &p0, &s0_); rv_call(&c3, a_v3, CTX3, ph, 0, 0, SP); snap_counts(&c3, &i1, &y1, &p1, &s1);
                a3.instr += i1 - i0; a3.cyc += y1 - y0; a3.psram += p1 - p0; if (y1 - y0 > a3.maxcyc) a3.maxcyc = y1 - y0;
            }
            snap_counts(&c4, &i0, &y0, &p0, &s0_); rv_call(&c4, a_v4, CTX4, ph, 0, 0, SP); snap_counts(&c4, &i1, &y1, &p1, &s1);
            a4.instr += i1 - i0; a4.cyc += y1 - y0; a4.psram += p1 - p0; a4.sram += s1 - s0_; if (y1 - y0 > a4.maxcyc) a4.maxcyc = y1 - y0;
            if (a_row) {                                   // line mode: the scan task generates each drive line
                double ph_sum = 0;
                for (int y = 0; y < H; y++) {
                    uint64_t r0 = est_cycles(&c4);
                    rv_call(&c4, a_row, CTX4, ph, y, DRV4, SP);          // line buffer: reuse the first drive row
                    const double us = (double)(est_cycles(&c4) - r0) / 320.0;
                    ph_sum += us; if (us > row_max) row_max = us;
                }
                row_sum += ph_sum; if (ph_sum > row_phase_max) row_phase_max = ph_sum;
            }
            if (verify) {
                pqbn_v4_phase(&HC, (uint32_t)ph);
                int bad = memcmp(M + HOT, h_hot, sizeof h_hot) != 0 || memcmp(M + COLD, h_cold, sizeof h_cold) != 0 ||
                          memcmp(M + TP4, h_tp, NT * 2) != 0 || memcmp(M + TN4, h_tn, NT) != 0 || memcmp(M + RA4, h_ra, H) != 0 ||
                          memcmp(M + BAND4, h_band, sizeof h_band) != 0;
                for (int y = 0; y < H; y++) if (h_ra[y] && memcmp(M + DRV4 + y * (W / 4), h_drv + y * (W / 4), W / 4)) bad = 1;
                mism += bad;
            }
        }
        if (prof) {
            FILE *fo = fopen(argv[9], "w");
            for (int i = 0; i < (1 << 16); i++) if (hist[i]) fprintf(fo, "%x %u\n", 0x1000 + 4 * i, hist[i]);
            fclose(fo);
            printf("prof %s: %.2f Minstr/phase\n", names[s], a4.instr / 1e6 / PH);
        } else if (verify) {
            printf("verify %s, %d phases: phases with any state/drive mismatch: %ld; RV32 instructions %.1f M\n", names[s], PH, mism, a4.instr / 1e6);
        } else {
            if (a_row) printf("  line mode: drive lines per phase avg %.2f ms, worst phase %.2f ms, worst line %.1f us (line time ~16 us)\n", row_sum / PH / 1000, row_phase_max / 1000, row_max);
            printf("%-26s | %7.2f %9.2f %9.2f   | %7.2f %9.2f %9.2f   | %7.2f %9.2f %9.2f   | %7.2f\n", labels[s],
                   ao.instr / 1e6 / PH, ao.cyc / 320e3 / PH, ao.psram / 1e6 / PH,
                   a3.instr / 1e6 / PH, a3.cyc / 320e3 / PH, a3.psram / 1e6 / PH,
                   a4.instr / 1e6 / PH, a4.cyc / 320e3 / PH, a4.psram / 1e6 / PH, a4.maxcyc / 320e3);
            fflush(stdout);
        }
        free(frames);
    }
    if (!verify) {
        printf("storage: old %d KB per-pixel arrays + 64 KB LUT; v3 %d KB + 32 KB LUT + %d KB tiles; v4 %d KB (hot %d + cold %d) + 16+1.5 KB tables + %d KB tiles; +4 KB thresholds each\n",
               H * W * 8 / 1024, H * W * 2 / 1024, NT * 5 / 1024, H * W * 5 / 2 / 1024, H * W / 2 / 1024, H * W * 2 / 1024, NT * 4 / 1024);
    }
    return 0;
}
