// Runs the RV32IM builds of both phase kernels on the scenario files and reports per-phase cost:
// RISC-V instructions, estimated cycles (simple in-order model), and bytes moved to/from PSRAM vs SRAM.
#include "rv32emu.h"
#include <math.h>

#define W 1216
#define H 684
#define NTX (W / 32)
#define NTY (H / 4)

void pqbn_lut2_build(uint8_t *lut2, int *step, float T0, float s0, float taud);
int pqbn_v3_build(uint8_t *lut, uint8_t *step_out, float T0, float s0, float taud);   // host build (MIRROR must match)

static void w32(uint8_t *M, uint32_t a, uint32_t v) { memcpy(M + a, &v, 4); }

typedef struct { uint64_t instr, cyc, psram, sram; } acc_t;

static uint64_t est_cycles(rv_cpu *c) { return c->instr + c->ld_use + 2 * c->br_taken + 2 * c->jumps + 3 * c->muldiv; }

int main(int argc, char **argv) {
    // argv: binary old_phase_addr new_phase_addr scen_dir nphases
    const char *bin = argv[1];
    uint32_t a_old = (uint32_t)strtoul(argv[2], 0, 16), a_new = (uint32_t)strtoul(argv[3], 0, 16);
    const char *dir = argv[4]; const int PH = atoi(argv[5]);
    uint8_t *M = calloc(1, RV_MEM);
    FILE *fb = fopen(bin, "rb"); size_t n = fread(M + 0x1000, 1, 1 << 20, fb); fclose(fb); (void)n;

    // ---------------- memory map ----------------
    uint32_t p = 0x00100000;
#define ALLOC(var, sz) uint32_t var = p; p = (p + (sz) + 63) & ~63u;
    ALLOC(CTXN, 64) ALLOC(LUTN, 65536) ALLOC(TAU, 4096) ALLOC(FB4, H * W / 2) ALLOC(ST, H * W * 2)
    ALLOC(TD, NTX * NTY) ALLOC(TB, NTX * NTY * 2) ALLOC(TL, NTX * NTY * 2) ALLOC(DRV, H * W / 4) ALLOC(RA, H)
    ALLOC(CTXO, 64) ALLOC(LUT2, 65536) ALLOC(STEP2, 16) ALLOC(TGT, H * W) ALLOC(PRV, H * W) ALLOC(TAUO, H * W)
    ALLOC(Q8, H * W) ALLOC(CLS, H * W) ALLOC(MF, H * W) ALLOC(BZ, H * W) ALLOC(DC, H * W) ALLOC(OUT, H * W / 4) ALLOC(RB, H * 4)
    const uint32_t SP = 0x02F00000;

    rv_cpu co = {0}, cn = {0}; co.m = cn.m = M;
    rv_add_region(&cn, FB4, FB4 + H * W / 2, "PSRAM fb4");  rv_add_region(&cn, ST, ST + H * W * 2, "PSRAM state");
    rv_add_region(&cn, DRV, DRV + H * W / 4, "PSRAM drive");
    rv_add_region(&cn, LUTN, LUTN + 65536, "SRAM lut"); rv_add_region(&cn, TAU, TAU + 4096, "SRAM tau");
    rv_add_region(&cn, TD, RA + H, "SRAM tiles"); rv_add_region(&cn, SP - 65536, SP + 64, "SRAM stack"); rv_add_region(&cn, CTXN, CTXN + 64, "SRAM ctx");
    rv_add_region(&co, TGT, DC + H * W, "PSRAM per-pixel arrays"); rv_add_region(&co, OUT, OUT + H * W / 4, "PSRAM drive");
    rv_add_region(&co, LUT2, LUT2 + 65536, "SRAM lut"); rv_add_region(&co, STEP2, STEP2 + 16, "SRAM step");
    rv_add_region(&co, RB, RB + H * 4, "SRAM row_busy"); rv_add_region(&co, SP - 65536, SP + 64, "SRAM stack"); rv_add_region(&co, CTXO, CTXO + 64, "SRAM ctx");

    // ---------------- tables ----------------
    uint8_t stepn[4]; int lutB = pqbn_v3_build(M + LUTN, stepn, 7.6f, 0.3f, 120.f);
    int step2[4]; pqbn_lut2_build(M + LUT2, step2, 7.6f, 0.3f, 120.f); memcpy(M + STEP2, step2, 16);
    for (int i = 0; i < 4096; i++) M[TAU + i] = (uint8_t)((((i & 63) * 37) + ((i >> 6) * 101)) & 15);
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) M[TAUO + y * W + x] = M[TAU + (y & 63) * 64 + (x & 63)];
    // ctx structs (ilp32 layout)
    w32(M, CTXN + 0, W); w32(M, CTXN + 4, H); w32(M, CTXN + 8, NTX); w32(M, CTXN + 12, NTY); w32(M, CTXN + 16, LUTN);
    memcpy(M + CTXN + 20, stepn, 4); w32(M, CTXN + 24, 2); w32(M, CTXN + 28, FB4); w32(M, CTXN + 32, ST); w32(M, CTXN + 36, TAU);
    w32(M, CTXN + 40, TD); w32(M, CTXN + 44, TB); w32(M, CTXN + 48, TL); w32(M, CTXN + 52, DRV); w32(M, CTXN + 56, RA); w32(M, CTXN + 60, 0);
    w32(M, CTXO + 0, W); w32(M, CTXO + 4, H); w32(M, CTXO + 8, 2); w32(M, CTXO + 12, LUT2); w32(M, CTXO + 16, STEP2);
    w32(M, CTXO + 20, TGT); w32(M, CTXO + 24, PRV); w32(M, CTXO + 28, TAUO); w32(M, CTXO + 32, Q8); w32(M, CTXO + 36, CLS);
    w32(M, CTXO + 40, MF); w32(M, CTXO + 44, BZ); w32(M, CTXO + 48, DC); w32(M, CTXO + 52, OUT); w32(M, CTXO + 56, RB);

    const char *names[] = {"static", "ai_stream", "page_turn", "scroll_half", "scroll_full", "scroll_full_14fps", "covers_scroll"};
    const char *labels[] = {"static page", "AI text streaming", "page turn", "list scroll, half screen",
                            "full-screen scroll 28 fps", "full-screen scroll 14 fps", "cover grid scroll"};
    printf("per phase averages over %d phases (12 ms each); cycles = simple in-order estimate; S31 core 320 MHz\n", PH);
    printf("%-26s | %-44s | %-44s\n", "", "old kernel: Minstr  Mcyc  ms@320MHz  PSRAM MB", "new kernel: Minstr  Mcyc  ms@320MHz  PSRAM MB");
    for (int s = 0; s < 7; s++) {
        char path[512]; snprintf(path, sizeof path, "%s/%s.bin", dir, names[s]);
        FILE *fp = fopen(path, "rb"); int nf; if (fread(&nf, 4, 1, fp) != 1) return 1;
        const size_t fbB = (size_t)H * (W / 2), tdB = (size_t)NTX * NTY;
        uint8_t *frames = malloc((size_t)nf * (fbB + tdB));
        if (fread(frames, 1, (size_t)nf * (fbB + tdB), fp) != (size_t)nf * (fbB + tdB)) return 1;
        fclose(fp);
        // settled start on frame 0
        memcpy(M + FB4, frames, fbB);
        for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
            const int t4 = (M[FB4 + y * (W / 2) + (x >> 1)] >> ((x & 1) << 2)) & 15, i = y * W + x;
            M[TGT + i] = M[PRV + i] = (uint8_t)t4; M[Q8 + i] = (uint8_t)(t4 * 17); M[CLS + i] = 3; M[MF + i] = 1; M[BZ + i] = 0; M[DC + i] = 0;
            uint16_t sw = (uint16_t)(0x8000 | 0x0300 | (t4 * 17)); memcpy(M + ST + 2 * i, &sw, 2);
        }
        memset(M + TD, 0, NTX * NTY); memset(M + TB, 0, NTX * NTY * 2); memset(M + TL, 0, NTX * NTY * 2); memset(M + RB, 0, H * 4);
        acc_t ao = {0}, an = {0};
        for (int ph = 1; ph <= PH; ph++) {
            if (ph % 3 == 0 && ph / 3 < nf) {
                const uint8_t *fr = frames + (size_t)(ph / 3) * (fbB + tdB);
                memcpy(M + FB4, fr, fbB);
                for (size_t i = 0; i < tdB; i++) M[TD + i] |= fr[fbB + i];
                for (int y = 0; y < H; y++) for (int x = 0; x < W; x++)
                    M[TGT + y * W + x] = (M[FB4 + y * (W / 2) + (x >> 1)] >> ((x & 1) << 2)) & 15;
            }
            rv_cpu so = co, sn = cn;   // snapshot counters
            rv_call(&co, a_old, CTXO, ph, 0, 0, SP);
            rv_call(&cn, a_new, CTXN, ph, 0, 0, SP);
            (void)so; (void)sn;
        }
        uint64_t po = 0, pn = 0, so_ = 0, sn_ = 0;
        for (int i = 0; i < co.nreg; i++) { uint64_t b = co.reg[i].rd + co.reg[i].wr; if (!strncmp(co.reg[i].name, "PSRAM", 5)) po += b; else so_ += b; }
        for (int i = 0; i < cn.nreg; i++) { uint64_t b = cn.reg[i].rd + cn.reg[i].wr; if (!strncmp(cn.reg[i].name, "PSRAM", 5)) pn += b; else sn_ += b; }
        const double ci_o = (double)co.instr / PH, ci_n = (double)cn.instr / PH;
        const double cc_o = (double)est_cycles(&co) / PH, cc_n = (double)est_cycles(&cn) / PH;
        printf("%-26s | %7.2f %7.2f %8.2f %8.2f            | %7.2f %7.2f %8.2f %8.2f\n", labels[s],
               ci_o / 1e6, cc_o / 1e6, cc_o / 320e3, po / 1e6 / PH, ci_n / 1e6, cc_n / 1e6, cc_n / 320e3, pn / 1e6 / PH);
        // reset counters for next scenario
        memset(&co.instr, 0, sizeof(uint64_t) * 8); memset(&cn.instr, 0, sizeof(uint64_t) * 8);
        for (int i = 0; i < co.nreg; i++) co.reg[i].rd = co.reg[i].wr = 0;
        for (int i = 0; i < cn.nreg; i++) cn.reg[i].rd = cn.reg[i].wr = 0;
        free(frames);
    }
    printf("state footprint: old %d KB per-pixel arrays + 64 KB LUT; new %d KB state + %d KB LUT + %d KB tiles/thresholds\n",
           H * W * 8 / 1024, H * W * 2 / 1024, lutB / 1024, (NTX * NTY * 5 + 4096) / 1024);
    return 0;
}
