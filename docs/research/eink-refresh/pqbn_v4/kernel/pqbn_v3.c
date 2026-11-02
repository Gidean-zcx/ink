// PQ-BN v3: v2 storage layout (one 16-bit state word per pixel) with a leaner phase kernel.
//   * tiles invalidated by LVGL: every pixel checked (settled pixels take a 2-compare path)
//   * tiles that are only "busy": only pixels that pushed last phase (cls == 0 in the state word)
//   * resting-pixel stiction ticks only on tick phases (1 in 8), so most phases write nothing for them
//   * two pixels per framebuffer byte
//   * table: MIRROR=1 -> 32 KB half table + mirror;  MIRROR=0 -> full 64 KB table, no mirror logic
#include <stdint.h>
#ifdef PQBN_RV
void *memset(void *d, int v, unsigned long n);
#else
#include <string.h>
#include <math.h>
#endif

#ifndef MIRROR
#define MIRROR 1
#endif
#define TPX 32
#define TPY 4

typedef struct {
    int W, H, ntx, nty;
    const uint8_t *lut; uint8_t step[4]; int marg;
    const uint8_t *fb4; uint16_t *st; const uint8_t *tau;
    uint8_t *tile_dirty; uint16_t *tile_busy; uint16_t *tile_last;
    uint8_t *drive; uint8_t *row_active; int8_t *u_dbg;
} pqbn_v3_ctx;

static int decide(int t4, int q, int a4, int c, const int *step) {
    const int d = t4 * 17, t = a4 * 16 + 8;
    if (d == q) return 0;
    const int s = d > q ? 1 : -1, D1 = step[c], Dn = step[0];
    int v = q, vprev = q, n = 0;
    while (n < 64) {
        int nv = v + s * (n == 0 ? D1 : Dn);
        if (nv > 252) nv = 255; if (nv < 3) nv = 0;
        if (nv == v) break;
        vprev = v; v = nv; n++;
        if ((s > 0 && v >= d) || (s < 0 && v <= d)) break;
    }
    int nbest;
    if ((s > 0 && v < d) || (s < 0 && v > d)) nbest = n;
    else {
        const int lo = s > 0 ? vprev : v, hi = s > 0 ? v : vprev;
        const int pick_hi = (d - lo) * 256 > t * (hi - lo);
        nbest = ((s > 0) ? pick_hi : !pick_hi) ? n : n - 1;
    }
    return nbest > 0 ? (s > 0 ? 2 : 1) : 0;
}

#ifndef PQBN_RV
// MIRROR=1: 32 KB, index t4<<13|q7<<6|tau<<2|cls (q<=127); MIRROR=0: 64 KB, index t4<<14|q8<<6|tau<<2|cls
int pqbn_v3_build(uint8_t *lut, uint8_t *step_out, float T0, float s0, float taud) {
    const float D = 1.0f / T0;
    const float dwell_of_class[4] = {0.f, 3.f, 24.f, 256.f};
    int step[4];
    for (int c = 0; c < 4; c++) {
        const float st = c ? 1.f - s0 * (1.f - expf(-dwell_of_class[c] / taud)) : 1.f;
        step[c] = (int)lrintf(D * st * 255.0f); step_out[c] = (uint8_t)step[c];
    }
    const int qmax = MIRROR ? 128 : 256, bytes = MIRROR ? 32768 : 65536;
    memset(lut, 0, bytes);
    for (int t4 = 0; t4 < 16; t4++) for (int q = 0; q < qmax; q++)
    for (int a4 = 0; a4 < 16; a4++) for (int c = 0; c < 4; c++) {
        int code;
        if (MIRROR || q < 128) code = decide(t4, q, a4, c, step);
        else { code = decide(15 - t4, 255 - q, 15 - a4, c, step); if (code) code ^= 3; }  // same rule as MIRROR=1
        const int idx = MIRROR ? ((t4 << 13) | (q << 6) | (a4 << 2) | c) : ((t4 << 14) | (q << 6) | (a4 << 2) | c);
        lut[idx >> 2] |= (uint8_t)(code << ((idx & 3) << 1));
    }
    return bytes;
}
#endif

static inline int dc_of(uint16_t s) { return ((int16_t)(s << 1)) >> 11; }

// full evaluation of one pixel; returns drive (-1/0/+1) and updates *sp
static inline int eval_px(const pqbn_v3_ctx *C, uint16_t *sp, int t4, int ta, int aged, int tick) {
    uint16_t s = *sp;
    const int q = s & 0xFF;
    int cl = (s >> 8) & 3;
    if (aged > cl && cl) cl = aged;
#if MIRROR
    const int mir = q >> 7;                         // q > 127
    const int msk = -mir;                           // 0 or -1
    const int tt = t4 ^ (msk & 15), qq = q ^ (msk & 255), aa = ta ^ (msk & 15);   // 15-x == x^15 for 4 bits
    const int idx = (tt << 13) | (qq << 6) | (aa << 2) | cl;
    int code = (C->lut[idx >> 2] >> ((idx & 3) << 1)) & 3;
    code ^= (code ? (msk & 3) : 0);
#else
    const int idx = (t4 << 14) | (q << 6) | (ta << 2) | cl;
    const int code = (C->lut[idx >> 2] >> ((idx & 3) << 1)) & 3;
#endif
    int u = 0;
    if (code) {
        u = code == 2 ? 1 : -1;
        int nq = q + u * C->step[cl];
        nq = nq > 252 ? 255 : nq < 3 ? 0 : nq;
        s = (uint16_t)((s & 0x7C00) | nq);
    } else if (q == 255 || q == 0) {
        const int sr = q ? 1 : -1, dc = dc_of(s), m = s >> 15;
        if (!m && sr * dc < C->marg) { u = sr; s = (uint16_t)((s & 0x7CFF) | 0x8000); }
        else if (sr * dc < 0) { u = sr; s = (uint16_t)(s & 0xFCFF); }
    }
    if (u) {
        int dc = dc_of(s) + u; dc = dc > 15 ? 15 : dc < -15 ? -15 : dc;
        s = (uint16_t)((s & ~0x7C00) | ((dc & 0x1F) << 10));
    } else if (cl == 0 || tick) {
        const int nc = cl == 0 ? 1 : (cl == 1 && (tick & 1)) ? 2 : (cl == 2 && (tick & 2)) ? 3 : cl;
        s = (uint16_t)((s & ~0x0300) | (nc << 8));
    } else if (cl != ((s >> 8) & 3)) {
        s = (uint16_t)((s & ~0x0300) | (cl << 8));   // aged class
    }
    *sp = s;
    return u;
}

int pqbn_v3_phase(pqbn_v3_ctx *C, uint32_t field) {
    const int W = C->W, ntx = C->ntx, Wq = W / 4, Wh = W / 2;
    const int tick = ((field & 7) == 0 ? 1 : 0) | ((field & 63) == 0 ? 2 : 0);
    int pushes = 0;
    if ((field & 4095) == 0)
        for (int i = 0; i < ntx * C->nty; i++)
            if ((uint16_t)(field - C->tile_last[i]) > 4096) C->tile_last[i] = (uint16_t)(field - 4096);
    for (int ty = 0; ty < C->nty; ty++) {
        const int y0 = ty * TPY;
        uint8_t *td = C->tile_dirty + ty * ntx; uint16_t *tb = C->tile_busy + ty * ntx;
        int any = 0;
        for (int tx = 0; tx < ntx; tx++) any |= td[tx] | (tb[tx] != 0);
        for (int y = y0; y < y0 + TPY; y++) {
            C->row_active[y] = (uint8_t)any;
            if (any) memset(C->drive + (size_t)y * Wq, 0, Wq);
        }
        if (!any) continue;
        for (int tx = 0; tx < ntx; tx++) {
            const int dirty = td[tx];
            if (!dirty && !tb[tx]) continue;
            const int ti = ty * ntx + tx;
            const uint16_t gap = (uint16_t)(field - C->tile_last[ti] - 1u);
            const int aged = gap >= 64 ? 3 : gap >= 8 ? 2 : gap >= 1 ? 1 : 0;
            C->tile_last[ti] = (uint16_t)field;
            td[tx] = 0;
            const int full = dirty | (tick != 0) | (aged != 0);   // which pixels need a look
            int busy = 0;
            const int x0 = tx * TPX;
            for (int y = y0; y < y0 + TPY; y++) {
                const uint8_t *fbr = C->fb4 + (size_t)y * Wh + (x0 >> 1);
                const uint8_t *taur = C->tau + ((y & 63) << 6) + (x0 & 63);
                uint16_t *str = C->st + (size_t)y * W + x0;
                uint8_t *dr = C->drive + (size_t)y * Wq + (x0 >> 2);
                int8_t *ud = C->u_dbg ? C->u_dbg + (size_t)y * W + x0 : 0;
                for (int k = 0; k < TPX; k += 2) {
                    const int fbb = fbr[k >> 1];
                    for (int j = 0; j < 2; j++) {
                        const int kk = k + j;
                        const uint16_t s = str[kk];
                        int u = 0;
                        if (full) {
                            const int t4 = (fbb >> (j << 2)) & 15;
                            // settled on the right rail, margin done, DC on the safe side, class ticks not due
                            const int sw = (t4 == 15) & ((s & 0xC0FF) == 0x80FF);
                            const int sb = (t4 == 0) & ((s & 0x80FF) == 0x8000) & (((s & 0x4000) != 0) | ((s & 0x7C00) == 0));
                            if ((sw | sb) && (((s >> 8) & 3) == 3 || (!tick && !aged))) { if (ud) ud[kk] = 0; continue; }
                            u = eval_px(C, &str[kk], t4, taur[kk], aged, tick);
                        } else {
                            if ((s & 0x0300) != 0) { if (ud) ud[kk] = 0; continue; }   // resting: not ours this phase
                            const int t4 = (fbb >> (j << 2)) & 15;
                            u = eval_px(C, &str[kk], t4, taur[kk], 0, 0);
                        }
                        if (u) { dr[kk >> 2] |= (uint8_t)((u > 0 ? 2 : 1) << (6 - ((kk & 3) << 1))); busy++; }
                        if (ud) ud[kk] = (int8_t)u;
                    }
                }
            }
            tb[tx] = (uint16_t)busy;
            pushes += busy;
        }
    }
    return pushes;
}
