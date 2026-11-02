// PQ-BN v4: plan once, execute cheaply.
//
// Per pixel: hot nibble (4 bit) + cold word (16 bit) = 2.5 bytes.
//   hot  = dir<<3 | r     r = pushes still to go (1..6) -> in flight; r = 0 -> resting;
//          0x7 / 0xF       = settled on the black / white rail (target matches, margin done, DC safe)
//   cold [7:0]  q    model optical state at the END of the current plan
//        [12:8] dc   DC balance at the end of the plan (signed 5 bit, +-15)
//        [13]   m    rail margin decided
// Planner: only in tiles LVGL invalidated (or whose capped plans need continuing). Decides the
//   number of pushes n for each non-settled pixel from a 64 KB table (4-bit n, mirrored half),
//   adds rail margin / DC rebalancing, writes hot + cold. Settled pixels are skipped 8 at a time.
// Executor: every phase, only tiles with pixels in flight: one 256-entry table per hot byte
//   (2 pixels) gives the next hot byte and the 2x2-bit drive codes.
#include <stdint.h>
#ifdef PQBN_RV
void *memset(void *d, int v, unsigned long n);
#else
#include <string.h>
#include <math.h>
#endif

#define TPX 32
#define TPY 4
#define RMAX 6

typedef struct {
    int W, H, ntx, nty;
    const uint8_t *lutn;      // 64 KB, 4-bit n, index t4<<13 | q7<<6 | tau4<<2 | cls  (q <= 127; mirror above)
    const uint16_t *exe;      // 256 entries: [7:0] next hot byte, [11:8] drive (even px [11:10], odd [9:8]), [12] still in flight
    uint8_t step[4];          // first push size per stiction class (Q8); step[0] = D
    int marg;
    const uint8_t *fb4;       // target 4bpp, low nibble = even x
    uint8_t *hot;             // hot nibbles, same packing as fb4
    uint16_t *cold;           // W*H
    const uint8_t *tau;       // 64x64 thresholds 0..15
    uint8_t *tile_dirty;      // set from LVGL invalidated areas; cleared here
    uint8_t *tile_busy;       // tile has pixels in flight
    uint16_t *tile_last;      // last phase with a push in the tile (stiction aging)
    uint16_t *tile_replan;    // phase at which capped plans continue (0 = none)
    uint8_t *drive;           // output 2 bit/px, W/4 bytes per row (1 black push, 2 white push, pixel 0 in bits 7:6)
    uint8_t *row_active;      // row has drive data this phase (else zero-line DMA)
} pqbn_v4_ctx;

#ifndef PQBN_RV
static int decide_n(int t4, int q, int a4, int c, const int *step) {
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
    if ((s > 0 && v < d) || (s < 0 && v > d)) return n;
    const int lo = s > 0 ? vprev : v, hi = s > 0 ? v : vprev;
    const int pick_hi = (d - lo) * 256 > t * (hi - lo);
    return ((s > 0) ? pick_hi : !pick_hi) ? n : n - 1;
}

void pqbn_v4_build(uint8_t *lutn /* 65536 */, uint16_t *exe /* 256 */, uint8_t *step_out, float T0, float s0, float taud) {
    const float D = 1.0f / T0;
    const float dwell_of_class[4] = {0.f, 3.f, 24.f, 256.f};
    int step[4];
    for (int c = 0; c < 4; c++) {
        const float st = c ? 1.f - s0 * (1.f - expf(-dwell_of_class[c] / taud)) : 1.f;
        step[c] = (int)lrintf(D * st * 255.0f); step_out[c] = (uint8_t)step[c];
    }
    memset(lutn, 0, 65536);
    for (int t4 = 0; t4 < 16; t4++) for (int q = 0; q < 128; q++)
    for (int a4 = 0; a4 < 16; a4++) for (int c = 0; c < 4; c++) {
        int n = decide_n(t4, q, a4, c, step); if (n > 15) n = 15;
        const int idx = (t4 << 13) | (q << 6) | (a4 << 2) | c;
        lutn[idx >> 1] |= (uint8_t)(n << ((idx & 1) << 2));
    }
    for (int b = 0; b < 256; b++) {
        int next = 0, drv = 0, still = 0;
        for (int j = 0; j < 2; j++) {
            const int h = (b >> (4 * j)) & 15, r = h & 7, dir = h >> 3;
            const int fly = r != 0 && r != 7;
            const int nh = fly ? h - 1 : h;
            const int code = fly ? (dir ? 2 : 1) : 0;
            next |= nh << (4 * j);
            drv |= code << (j ? 0 : 2);                      // even pixel first (higher bits)
            const int nr = nh & 7; still |= nr != 0 && nr != 7;
        }
        exe[b] = (uint16_t)(next | (drv << 8) | (still << 12));
    }
}
#endif

static inline int sx5(int v) { return (v ^ 16) - 16; }

// SWAR helpers on 8 nibbles
static inline uint32_t nib_allones(uint32_t x) { x = x & (x >> 1) & (x >> 2) & (x >> 3); return x & 0x11111111u; }
static inline uint32_t nib_zero(uint32_t x) { return nib_allones(~x); }

static void plan_tile(pqbn_v4_ctx *C, int tx, int ty, uint32_t now, int *partial) {
    const int W = C->W, Wh = W / 2, x0 = tx * TPX, D = C->step[0];
    const int ti = ty * C->ntx + tx;
    const uint16_t gap = (uint16_t)(now - C->tile_last[ti]);
    const int cls_rest = gap >= 64 ? 3 : gap >= 8 ? 2 : 1;
    for (int y = ty * TPY; y < ty * TPY + TPY; y++) {
        const uint32_t *fw = (const uint32_t *)(C->fb4 + (size_t)y * Wh + (x0 >> 1));
        uint32_t *hw = (uint32_t *)(C->hot + (size_t)y * Wh + (x0 >> 1));
        uint16_t *cr = C->cold + (size_t)y * W + x0;
        const uint8_t *taur = C->tau + ((y & 63) << 6) + (x0 & 63);
        for (int w = 0; w < TPX / 8; w++) {
            const uint32_t T = fw[w];
            uint32_t Hn = hw[w];
            // settled-white: target F and hot F; settled-black: target 0 and hot 7
            const uint32_t triv = nib_allones(T & Hn) | (nib_zero(T) & nib_zero(Hn ^ 0x77777777u));
            if (triv == 0x11111111u) continue;
            for (int j = 0; j < 8; j++) {
                if ((triv >> (4 * j)) & 1) continue;
                const int k = w * 8 + j;
                const int t4 = (T >> (4 * j)) & 15, h = (Hn >> (4 * j)) & 15;
                const uint16_t s = cr[k];
                int q = s & 255, dc = sx5((s >> 8) & 31), m = (s >> 13) & 1, cls = cls_rest;
                const int r = h & 7;
                if (r != 0 && r != 7) {                      // in flight: back out the pushes not yet done
                    const int sd = (h >> 3) ? 1 : -1;
                    q -= sd * r * D; q = q < 0 ? 0 : q > 255 ? 255 : q;
                    dc -= sd * r; cls = 0;
                }
                const int d = t4 * 17;
                int n = 0, sg = d > q ? 1 : -1;
                if (d != q) {
                    const int mir = q >> 7, msk = -mir;
                    const int idx = ((t4 ^ (msk & 15)) << 13) | ((q ^ (msk & 255)) << 6) | ((taur[k] ^ (msk & 15)) << 2) | cls;
                    n = (C->lutn[idx >> 1] >> ((idx & 1) << 2)) & 15;
                }
                int qe = q;
                if (n) {
                    qe = q + sg * (C->step[cls] + (n - 1) * D);
                    qe = qe > 252 ? 255 : qe < 3 ? 0 : qe;
                    m = 0;
                }
                if ((qe == 255 || qe == 0) && !m) {           // rail arrival: margin push, decided once
                    const int sr = qe ? 1 : -1;
                    if (n == 0 || sg == sr) {
                        if (sr * (dc + sg * n) < C->marg) { if (n == 0) sg = sr; n++; }
                        m = 1;
                    }
                }
                if (n == 0 && (q == 255 || q == 0)) {         // resting on a rail with opposite DC: rebalance
                    const int sr = q ? 1 : -1;
                    if (sr * dc < 0) { sg = sr; n = -sr * dc; }
                }
                if (n > RMAX) { n = RMAX; *partial = 1;
                    if (qe != q) { qe = q + sg * (C->step[cls] + (RMAX - 1) * D); qe = qe > 252 ? 255 : qe < 3 ? 0 : qe; } }
                dc += sg * n; dc = dc > 15 ? 15 : dc < -15 ? -15 : dc;
                int nh;
                if (n) nh = ((sg > 0) << 3) | n;
                else {
                    const int safe_w = qe == 255 && t4 == 15 && m && dc >= 0, safe_b = qe == 0 && t4 == 0 && m && dc <= 0;
                    nh = safe_w ? 0xF : safe_b ? 0x7 : 0;
                }
                Hn = (Hn & ~(15u << (4 * j))) | ((uint32_t)nh << (4 * j));
                cr[k] = (uint16_t)(qe | ((dc & 31) << 8) | (m << 13));
            }
            hw[w] = Hn;
        }
    }
}

int pqbn_v4_phase(pqbn_v4_ctx *C, uint32_t now) {
    const int W = C->W, ntx = C->ntx, Wq = W / 4, Wh = W / 2;
    int pushes = 0;
    if ((now & 4095) == 0)
        for (int i = 0; i < ntx * C->nty; i++)
            if ((uint16_t)(now - C->tile_last[i]) > 4096) C->tile_last[i] = (uint16_t)(now - 4096);
    for (int ty = 0; ty < C->nty; ty++) {
        uint8_t *td = C->tile_dirty + ty * ntx, *tb = C->tile_busy + ty * ntx;
        uint16_t *tr = C->tile_replan + ty * ntx;
        int any = 0;
        for (int tx = 0; tx < ntx; tx++) {
            if (tr[tx] && (uint16_t)(now - tr[tx]) < 0x8000) { td[tx] = 1; tr[tx] = 0; }
            any |= td[tx] | tb[tx];
        }
        for (int y = ty * TPY; y < ty * TPY + TPY; y++) {
            C->row_active[y] = (uint8_t)any;
            if (any) memset(C->drive + (size_t)y * Wq, 0, Wq);
        }
        if (!any) continue;
        for (int tx = 0; tx < ntx; tx++) {
            if (td[tx]) {
                int partial = 0;
                plan_tile(C, tx, ty, now, &partial);
                td[tx] = 0; tb[tx] = 1;
                if (partial) tr[tx] = (uint16_t)(now + RMAX);
            }
            if (!tb[tx]) continue;
            // executor
            const int x0 = tx * TPX; int still = 0, pushed = 0;
            for (int y = ty * TPY; y < ty * TPY + TPY; y++) {
                uint8_t *hb = C->hot + (size_t)y * Wh + (x0 >> 1);
                uint8_t *dr = C->drive + (size_t)y * Wq + (x0 >> 2);
                const uint32_t *hw = (const uint32_t *)hb;
                for (int w = 0; w < TPX / 8; w++) {
                    const uint32_t Hn = hw[w];
                    const uint32_t rnz = ((Hn & 0x77777777u) + 0x77777777u) & 0x88888888u;
                    uint32_t r7 = Hn & (Hn >> 1) & (Hn >> 2) & 0x11111111u;
                    if ((rnz & ~(r7 << 3)) == 0) continue;           // nothing in flight in these 8 pixels
                    uint8_t *b = hb + w * 4; uint8_t *o = dr + w * 2;
                    for (int j = 0; j < 4; j += 2) {
                        const uint16_t e0 = C->exe[b[j]], e1 = C->exe[b[j + 1]];
                        b[j] = (uint8_t)e0; b[j + 1] = (uint8_t)e1;
                        const int ob = (((e0 >> 8) & 15) << 4) | ((e1 >> 8) & 15);
                        o[j >> 1] = (uint8_t)ob;
                        still |= (e0 | e1) & 0x1000;
                        pushed |= ob;
                    }
                }
            }
            tb[tx] = still != 0;
            if (pushed) { C->tile_last[ty * ntx + tx] = (uint16_t)now; pushes++; }
        }
    }
    return pushes;
}
