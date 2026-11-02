// Both phase kernels compiled for RV32IM (freestanding), run inside rv32emu to count real RISC-V work.
#include <stdint.h>
#include <stddef.h>

// word-wise libc helpers (same for both kernels)
void *memset(void *d, int v, unsigned long n) {
    uint8_t *p = d; uint32_t w = (uint8_t)v * 0x01010101u;
    while (n && ((uintptr_t)p & 3)) { *p++ = (uint8_t)v; n--; }
    while (n >= 4) { *(uint32_t *)p = w; p += 4; n -= 4; }
    while (n--) *p++ = (uint8_t)v;
    return d;
}
void *memcpy(void *d, const void *s, unsigned long n) {
    uint8_t *a = d; const uint8_t *b = s;
    if ((((uintptr_t)a | (uintptr_t)b) & 3) == 0) { while (n >= 4) { *(uint32_t *)a = *(const uint32_t *)b; a += 4; b += 4; n -= 4; } }
    while (n--) *a++ = *b++;
    return d;
}
int memcmp(const void *x, const void *y, unsigned long n) {
    const uint8_t *a = x, *b = y;
    if ((((uintptr_t)a | (uintptr_t)b) & 3) == 0) {
        while (n >= 4) { if (*(const uint32_t *)a != *(const uint32_t *)b) break; a += 4; b += 4; n -= 4; }
    }
    while (n--) { if (*a != *b) return *a - *b; a++; b++; }
    return 0;
}

// ---------------- old event-driven kernel (8 byte arrays, prev copy, row memcmp, 64 KB LUT) ----------------
typedef struct {
    int W, H, marg;
    const uint8_t *lut2; const int *step;
    uint8_t *tgt, *prevt, *tau, *q8, *cls, *mflag, *busy; int8_t *dc;
    uint8_t *out; int *row_busy;
} old_ctx;

int old_phase(old_ctx *C, uint32_t field) {
    (void)field;
    const int W = C->W; long ev = 0;
    for (int y = 0; y < C->H; y++) {
        uint8_t *tg = C->tgt + y * W, *pv = C->prevt + y * W;
        const int dirty = memcmp(tg, pv, W) != 0;
        if (!dirty && C->row_busy[y] == 0) continue;
        if (dirty) memcpy(pv, tg, W);
        int rb = 0;
        uint8_t *o = C->out + y * (W / 4);
        uint8_t *q8 = C->q8 + y * W, *cls = C->cls + y * W, *mf = C->mflag + y * W, *bz = C->busy + y * W, *ta = C->tau + y * W;
        int8_t *dc = C->dc + y * W;
        for (int x = 0; x < W; x += 4) {
            uint8_t ob = 0;
            for (int k = 0; k < 4; k++) {
                const int i = x + k;
                int uu = 0;
                if (dirty || bz[i]) {
                    ev++;
                    const int q = q8[i], c = cls[i];
                    const int idx = (tg[i] << 14) | (q << 6) | (ta[i] << 2) | c;
                    const int code = (C->lut2[idx >> 2] >> ((idx & 3) << 1)) & 3;
                    uu = (code == 2) - (code == 1);
                    int nq = q + uu * C->step[c];
                    nq = nq > 252 ? 255 : nq < 3 ? 0 : nq;
                    if (!uu && (q == 255 || q == 0)) {
                        const int sr = q ? 1 : -1;
                        if (!mf[i] && sr * dc[i] < C->marg) { uu = sr; mf[i] = 1; }
                        else if (sr * dc[i] < 0) uu = sr;
                    } else if (uu) mf[i] = 0;
                    q8[i] = (uint8_t)nq;
                    cls[i] = uu ? 0 : (c == 0 ? 1 : c);
                    dc[i] = (int8_t)(dc[i] + uu);
                    bz[i] = (uint8_t)(uu != 0);
                    rb += uu != 0;
                }
                ob = (uint8_t)((ob << 2) | (uu > 0 ? 2 : uu < 0 ? 1 : 0));
            }
            o[x >> 2] = ob;
        }
        C->row_busy[y] = rb;
    }
    return (int)ev;
}

// ---------------- new kernel ----------------
#define PQBN_RV 1
#include "../kernel/pqbn_v3.c"
