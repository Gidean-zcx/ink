// PQ-BN v2: storage- and compute-optimised physical quantizer for a direct EPD driver.
//
// Storage per pixel: ONE 16-bit state word (plus the 4bpp LVGL framebuffer that exists anyway).
//   [7:0]   q    model optical state (0 = black rail .. 255 = white rail)
//   [9:8]   cls  stiction class: 0 pushed last phase, 1 short rest, 2 medium, 3 long
//   [14:10] dc   running sum of pushes, signed 5 bit, saturating at +-15
//   [15]    m    rail margin already applied
// Global tables: 32 KB decision LUT (half of the 64 KB table, the other half by mirror symmetry),
// 64x64 blue-noise thresholds (4 KB), per 32x4 tile: dirty flag, busy count, last processed phase.
//
// Work per phase: only tiles that LVGL invalidated or that still contain pushing pixels.
// Inside a tile, pixels already resting on the rail their target asks for take a 3-compare path.
#include <stdint.h>
#include <string.h>
#include <math.h>

#define TPX 32            // tile width (pixels)  -> 16 framebuffer bytes, 64 state bytes
#define TPY 4             // tile height (rows)

typedef struct {
    int W, H, ntx, nty;
    const uint8_t *lut;       // 32 KB, 2-bit codes, index t4<<13 | q7<<6 | tau4<<2 | cls  (q <= 127)
    uint8_t step[4];          // push size per stiction class (Q8)
    int marg;                 // rail margin allowed while s*dc < marg
    const uint8_t *fb4;       // target, 4bpp, 2 px per byte, low nibble = even x
    uint16_t *st;             // state words, W*H
    const uint8_t *tau;       // 64*64 thresholds 0..15
    uint8_t *tile_dirty;      // set by caller from LVGL invalidated areas; cleared here
    uint16_t *tile_busy;      // pixels that pushed in the tile's last processed phase
    uint16_t *tile_last;      // phase index of the tile's last processing
    uint8_t *drive;           // output: 2 bit per pixel, W/4 bytes per row (1 = black push, 2 = white push)
    uint8_t *row_active;      // output: row has processed tiles (else DMA can point at a zero line)
    int8_t *u_dbg;            // optional per-pixel drive for simulation (may be NULL)
} pqbn_v2_ctx;

// ---------------------------------------------------------------------------------------------
// Table build: identical rule to pqbn_lut2_build(), stored for q <= 127 only.
// Mirror: (t, q, tau) -> (15-t, 255-q, 15-tau) with black/white swapped gives the same decision.
// ---------------------------------------------------------------------------------------------
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

void pqbn_v2_build(uint8_t *lut32k, uint8_t *step_out, float T0, float s0, float taud) {
    const float D = 1.0f / T0;
    const float dwell_of_class[4] = {0.f, 3.f, 24.f, 256.f};
    int step[4];
    for (int c = 0; c < 4; c++) {
        const float st = c ? 1.f - s0 * (1.f - expf(-dwell_of_class[c] / taud)) : 1.f;
        step[c] = (int)lrintf(D * st * 255.0f);
        step_out[c] = (uint8_t)step[c];
    }
    memset(lut32k, 0, 32768);
    for (int t4 = 0; t4 < 16; t4++)
    for (int q = 0; q < 128; q++)
    for (int a4 = 0; a4 < 16; a4++)
    for (int c = 0; c < 4; c++) {
        const int code = decide(t4, q, a4, c, step);
        const int idx = (t4 << 13) | (q << 6) | (a4 << 2) | c;
        lut32k[idx >> 2] |= (uint8_t)(code << ((idx & 3) << 1));
    }
}

// Exhaustive check of the mirror symmetry against the direct rule (returns mismatching entries).
int pqbn_v2_check_mirror(const uint8_t *step_u8) {
    int step[4] = {step_u8[0], step_u8[1], step_u8[2], step_u8[3]}, bad = 0;
    for (int t4 = 0; t4 < 16; t4++)
    for (int q = 128; q < 256; q++)
    for (int a4 = 0; a4 < 16; a4++)
    for (int c = 0; c < 4; c++) {
        int direct = decide(t4, q, a4, c, step);
        int m = decide(15 - t4, 255 - q, 15 - a4, c, step);
        if (m) m ^= 3;
        bad += direct != m;
    }
    return bad;
}

// ---------------------------------------------------------------------------------------------
// One phase.
// ---------------------------------------------------------------------------------------------
static inline int dc_of(uint16_t s) { return ((int16_t)(s << 1)) >> 11; }

int pqbn_v2_phase(pqbn_v2_ctx *C, uint32_t field) {
    const int W = C->W, ntx = C->ntx;
    const uint8_t *lut = C->lut;
    int pushes = 0;
    if ((field & 4095) == 0)            // keep 16-bit tile timestamps of long-idle tiles from wrapping
        for (int i = 0; i < ntx * C->nty; i++)
            if ((uint16_t)(field - C->tile_last[i]) > 4096) C->tile_last[i] = (uint16_t)(field - 4096);
    for (int ty = 0; ty < C->nty; ty++) {
        const int y0 = ty * TPY, y1 = (y0 + TPY < C->H) ? y0 + TPY : C->H;
        int any = 0;
        for (int tx = 0; tx < ntx; tx++) any |= C->tile_dirty[ty * ntx + tx] | (C->tile_busy[ty * ntx + tx] != 0);
        for (int y = y0; y < y1; y++) {
            C->row_active[y] = (uint8_t)any;
            if (any) memset(C->drive + (size_t)y * (W / 4), 0, W / 4);
        }
        if (!any) continue;
        for (int tx = 0; tx < ntx; tx++) {
            const int ti = ty * ntx + tx;
            if (!C->tile_dirty[ti] && !C->tile_busy[ti]) continue;
            // stiction aging for the phases this tile was skipped
            const uint32_t gap = field - C->tile_last[ti] - 1u;
            const int aged = (uint16_t)gap >= 64 ? 3 : (uint16_t)gap >= 8 ? 2 : (uint16_t)gap >= 1 ? 1 : 0;
            C->tile_last[ti] = (uint16_t)field;
            C->tile_dirty[ti] = 0;
            int busy = 0;
            const int x0 = tx * TPX;
            for (int y = y0; y < y1; y++) {
                const uint8_t *fbr = C->fb4 + (size_t)y * (W / 2) + x0 / 2;
                const uint8_t *taur = C->tau + (y & 63) * 64;
                uint16_t *str = C->st + (size_t)y * W + x0;
                uint8_t *dr = C->drive + (size_t)y * (W / 4) + x0 / 4;
                int8_t *ud = C->u_dbg ? C->u_dbg + (size_t)y * W + x0 : 0;
                for (int k = 0; k < TPX; k++) {
                    const int t4 = (fbr[k >> 1] >> ((k & 1) << 2)) & 15;
                    uint16_t s = str[k];
                    const int q = s & 0xFF;
                    int cl = (s >> 8) & 3;
                    if (aged && cl && aged > cl) cl = aged;
                    int u = 0;
                    // fast path: resting on the rail the target asks for, margin done, DC on the safe side
                    const int settled_w = (t4 == 15) & (q == 255) & ((s & 0xC000) == 0x8000);
                    const int settled_b = (t4 == 0) & (q == 0) & ((s & 0x8000) != 0) & (((s & 0x4000) != 0) | ((s & 0x7C00) == 0));
                    if (!(settled_w | settled_b)) {
                        const int mir = q > 127;
                        const int tt = mir ? 15 - t4 : t4, qq = mir ? 255 - q : q;
                        const int ta = taur[(x0 + k) & 63], aa = mir ? 15 - ta : ta;
                        const int idx = (tt << 13) | (qq << 6) | (aa << 2) | cl;
                        int code = (lut[idx >> 2] >> ((idx & 3) << 1)) & 3;
                        if (mir && code) code ^= 3;
                        int dc = dc_of(s), m = s >> 15;
                        if (code) {
                            u = code == 2 ? 1 : -1;
                            int nq = q + u * C->step[cl];
                            nq = nq > 252 ? 255 : nq < 3 ? 0 : nq;
                            s = (uint16_t)((s & 0x7C00) | nq);             // cls 0, m 0
                        } else if (q == 255 || q == 0) {
                            const int sr = q ? 1 : -1;
                            if (!m && sr * dc < C->marg) { u = sr; m = 1; }          // arrival margin
                            else if (sr * dc < 0) u = sr;                             // rail rebalancing
                            if (u) s = (uint16_t)((s & 0x7CFF) | (m << 15));           // cls 0
                        }
                        if (u) {
                            dc += u; dc = dc > 15 ? 15 : dc < -15 ? -15 : dc;
                            s = (uint16_t)((s & ~0x7C00) | ((dc & 0x1F) << 10));
                            dr[k >> 2] |= (uint8_t)((u > 0 ? 2 : 1) << (6 - ((k & 3) << 1)));
                            busy++;
                        }
                    }
                    if (!u) {                                   // resting: stiction class ticks
                        int nc = cl;
                        if (cl == 0) nc = 1;
                        else if (cl == 1 && (field & 7) == 0) nc = 2;
                        else if (cl == 2 && (field & 63) == 0) nc = 3;
                        s = (uint16_t)((s & ~0x0300) | (nc << 8));
                    }
                    if (s != str[k]) str[k] = s;
                    if (ud) ud[k] = (int8_t)u;
                }
            }
            C->tile_busy[ti] = (uint16_t)busy;
            pushes += busy;
        }
    }
    return pushes;
}
