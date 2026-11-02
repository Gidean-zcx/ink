// Validates the emulator: runs the RV32 build of pqbn_v3_phase and the native x86 build on the same
// scenario for N phases and compares the resulting state words and drive planes byte for byte.
#include "rv32emu.h"
#define W 1216
#define H 684
#define NTX (W / 32)
#define NTY (H / 4)
typedef struct {
    int W_, H_, ntx, nty; const uint8_t *lut; uint8_t step[4]; int marg;
    const uint8_t *fb4; uint16_t *st; const uint8_t *tau;
    uint8_t *tile_dirty; uint16_t *tile_busy; uint16_t *tile_last;
    uint8_t *drive; uint8_t *row_active; int8_t *u_dbg;
} ctx_t;
int pqbn_v3_build(uint8_t *lut, uint8_t *step_out, float T0, float s0, float taud);
int pqbn_v3_phase(ctx_t *C, uint32_t field);
static void w32(uint8_t *M, uint32_t a, uint32_t v) { memcpy(M + a, &v, 4); }
int main(int argc, char **argv) {
    uint32_t entry = (uint32_t)strtoul(argv[2], 0, 16); const int PH = atoi(argv[4]);
    uint8_t *M = calloc(1, RV_MEM);
    FILE *fb = fopen(argv[1], "rb"); size_t n = fread(M + 0x1000, 1, 1 << 20, fb); fclose(fb); (void)n;
    const uint32_t CTX = 0x100000, LUT = 0x110000, TAU = 0x130000, FB4 = 0x140000, ST = 0x200000,
                   TD = 0x400000, TB = 0x410000, TL = 0x420000, DRV = 0x430000, RA = 0x480000, SP = 0x2F00000;
    static uint8_t lut[65536], tau[4096], fb4[H * W / 2], td[NTX * NTY], drv[H * W / 4], ra[H];
    static uint16_t st[H * W], tb[NTX * NTY], tl[NTX * NTY];
    uint8_t step[4]; pqbn_v3_build(lut, step, 7.6f, 0.3f, 120.f);
    for (int i = 0; i < 4096; i++) tau[i] = (uint8_t)((((i & 63) * 37) + ((i >> 6) * 101)) & 15);
    memcpy(M + LUT, lut, 65536); memcpy(M + TAU, tau, 4096);
    w32(M, CTX + 0, W); w32(M, CTX + 4, H); w32(M, CTX + 8, NTX); w32(M, CTX + 12, NTY); w32(M, CTX + 16, LUT);
    memcpy(M + CTX + 20, step, 4); w32(M, CTX + 24, 2); w32(M, CTX + 28, FB4); w32(M, CTX + 32, ST); w32(M, CTX + 36, TAU);
    w32(M, CTX + 40, TD); w32(M, CTX + 44, TB); w32(M, CTX + 48, TL); w32(M, CTX + 52, DRV); w32(M, CTX + 56, RA); w32(M, CTX + 60, 0);
    ctx_t C = {W, H, NTX, NTY, lut, {step[0], step[1], step[2], step[3]}, 2, fb4, st, tau, td, tb, tl, drv, ra, NULL};
    FILE *fp = fopen(argv[3], "rb"); int nf; if (fread(&nf, 4, 1, fp) != 1) return 1;
    const size_t fbB = (size_t)H * (W / 2), tdB = (size_t)NTX * NTY;
    uint8_t *frames = malloc((size_t)nf * (fbB + tdB));
    if (fread(frames, 1, (size_t)nf * (fbB + tdB), fp) != (size_t)nf * (fbB + tdB)) return 1;
    memcpy(fb4, frames, fbB);
    for (int i = 0; i < H * W; i++) { int t4 = (fb4[i >> 1] >> ((i & 1) << 2)) & 15; st[i] = (uint16_t)(0x8000 | 0x0300 | (t4 * 17)); }
    memcpy(M + FB4, fb4, fbB); memcpy(M + ST, st, sizeof st);
    rv_cpu c = {0}; c.m = M;
    long mism_st = 0, mism_drv = 0;
    for (int ph = 1; ph <= PH; ph++) {
        if (ph % 3 == 0 && ph / 3 < nf) {
            const uint8_t *fr = frames + (size_t)(ph / 3) * (fbB + tdB);
            memcpy(fb4, fr, fbB); memcpy(M + FB4, fr, fbB);
            for (size_t i = 0; i < tdB; i++) { td[i] |= fr[fbB + i]; M[TD + i] |= fr[fbB + i]; }
        }
        pqbn_v3_phase(&C, (uint32_t)ph);
        rv_call(&c, entry, CTX, ph, 0, 0, SP);
        mism_st += memcmp(M + ST, st, sizeof st) != 0;
        for (int y = 0; y < H; y++) if (ra[y] && memcmp(M + DRV + y * (W / 4), drv + y * (W / 4), W / 4)) mism_drv++;
    }
    printf("%d phases: phases with state mismatch %ld, drive-row mismatches %ld; RV32 instructions %.1f M\n",
           PH, mism_st, mism_drv, c.instr / 1e6);
    return 0;
}
