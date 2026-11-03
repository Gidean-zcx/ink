/* PQ-BN v4 kernel: research copy docs/research/eink-refresh/pqbn_v4/kernel/pqbn_v4.c (PR #7,
 * e77d01c) with the context struct and tile flags moved to pqbn_v4.h and one line split for
 * -Wmisleading-indentation, and PQBN_HOT on the executor (IRAM placement); algorithm unchanged. */
// PQ-BN v4: plan once per change, execute with a table lookup.
//
// v3 re-evaluated every in-flight pixel every phase (table lookup + state update per pixel per phase).
// v4 splits the work:
//   planner  - runs only in tiles the UI invalidated (or whose capped plan just finished). For each
//              pixel that is not settled it decides the whole push sequence at once: n pushes (0..6)
//              in one direction, starting this phase, one per phase.
//   executor - runs every phase, only in tiles whose plan is still running. A pixel pushes in phase
//              k of its tile's plan iff n > k, so the plan nibbles are read-only: 8 pixels are tested
//              with one SWAR expression and converted to drive bits with 4 byte-table lookups.
// Decisions are the same PQ-BN rule as v3 (blue-noise choice between the two reachable outcomes
// that bracket the target; stiction-aware first push; rail margin and DC rebalancing). With
// stiction off the drive is bit-identical to v3 (tests/eq_v3_v4.c).
//
// Stiction class of a pixel's first push comes from its dwell (idle phases before the push):
//   class 0: 0, class 1: 1..7, class 2: 8..63, class 3: >= 64
// Dwell is exact for pixels that pushed in their last plan (from the tile's plan phase and n) and
// for settled pixels (10-bit timestamp). Pixels that rested through their last plan keep a 3-bit
// log2 level (dwell >= 0,1,2,4,8,16,32,64) advanced at per-pixel-offset 2^L boundaries, so it
// tracks dwell without bias however often the tile is replanned.
//
// Per-pixel storage (2.5 B/px):
//   hot  (4 bit, packed like fb4: low nibble = even x)
//        dir<<3 | n   n = pushes planned from the tile's plan phase (0..6), dir 1 = toward white
//        0x7 / 0xF    settled on the black / white rail (target matches, margin done, DC safe)
//   cold (16 bit)
//        n > 0   : [7:0] q1 model optical state after the plan's first push, [12:8] dc0 DC balance at
//                  plan start (signed, saturating +-15), [14:13] 0, [15] plan ends on a rail, margin done
//        n = 0   : [7:0] q, [12:8] dc, [15:13] dwell level   (margin flag implied: q on a rail)
//        settled : [7:0] ts low, [12:8] dc, [14:13] ts high, [15] 1   (ts = first idle phase, 10 bit)
// Per-tile storage (32x4 px): dirty flag (from the UI), phase of the last plan (u16), max n | flags.
//
// Motion mode (fast scroll / fling; the UI switches the tiles of the scrolling container):
//   binary blue-noise targets, 7-push rail-to-rail swings, planned 8 pixels at a time with SWAR.
//   hot  = dir<<3 | r (r = pushes from the tile's plan phase, 0..7); remaining = max(r - k, 0);
//          position from black = dir ? 7 - remaining : remaining, so no per-pixel state is read.
//   cold = [3:0] position at entry, [12:8] DC at entry (untouched while in motion).
//   Exit converts back (DC = entry DC + position change) and marks the tiles for a full PQ-BN replan.
// Tables: 16 KB decision table, 1.5 KB executor table, 4 KB blue-noise thresholds.
#include <stdint.h>
#ifdef PQBN_RV
void *memset(void *d, int v, unsigned long n);
#else
#include <string.h>
#include <math.h>
#include <stdio.h>
#endif

#define TPX 32
#define TPY 4
#define RMAX 6
#define TS_SWEEP 512          // every settled pixel's timestamp is visited at least once per TS_SWEEP phases

#include "pqbn_v4.h"



static inline void band_touch(pqbn_v4_ctx *C, int ti, uint32_t now, int nmax) {
    uint16_t *u = &C->band[2 * (ti / C->ntx)];
    const uint16_t e = (uint16_t)(now + nmax);
    if ((int16_t)(e - *u) > 0) *u = e;
}

#ifndef PQBN_RV
// representative dwell of each stiction class, used for the class's first-push size
float pqbn_v4_class_dwell[4] = {0.f, 3.f, 24.f, 256.f};

// number of pushes the PQ-BN rule chooses from state q toward target t4 (no margin, no cap)
static int decide_n(int t4, int q, int a4, int c, const int *step) {
    const int d = t4 * 17, t = a4 * 16 + 8;
    if (d == q) return 0;
    const int s = d > q ? 1 : -1, D1 = step[c], Dn = step[0];
    int v = q, vprev = q, n = 0;
    while (n < 64) {
        int nv = v + s * (n == 0 ? D1 : Dn);
        if (nv > 252) nv = 255;
        if (nv < 3) nv = 0;
        if (nv == v) break;
        vprev = v; v = nv; n++;
        if ((s > 0 && v >= d) || (s < 0 && v <= d)) break;
    }
    if ((s > 0 && v < d) || (s < 0 && v > d)) return n;
    const int lo = s > 0 ? vprev : v, hi = s > 0 ? v : vprev;
    const int pick_hi = (d - lo) * 256 > t * (hi - lo);
    return ((s > 0) ? pick_hi : !pick_hi) ? n : n - 1;
}

// returns 0 on success, -1 if a table entry cannot be expressed as nb +- (tau < X)
int pqbn_v4_build(uint8_t *dec /*16384*/, uint8_t *exk /*RMAX*256*/, uint8_t *step_out, float T0, float s0, float taud) {
    const float D = 1.0f / T0;
    int step[4];
    for (int c = 0; c < 4; c++) {
        const float st = c ? 1.f - s0 * (1.f - expf(-pqbn_v4_class_dwell[c] / taud)) : 1.f;
        step[c] = (int)lrintf(D * st * 255.0f); step_out[c] = (uint8_t)step[c];
    }
    for (int t4 = 0; t4 < 16; t4++) for (int q = 0; q < 256; q++) for (int c = 0; c < 4; c++) {
        const int d = t4 * 17;
        int nb = 0, X = 0;
        if (d != q) {
            int nv[16], mn = 99, mx = -1;
            for (int a = 0; a < 16; a++) { nv[a] = decide_n(t4, q, a, c, step); if (nv[a] < mn) mn = nv[a]; if (nv[a] > mx) mx = nv[a]; }
            if (mx - mn > 1) return -1;
            const int up = d > q;
            // up: n(a) = mn + (a < X) ; down: n(a) = mx - (a < X)
            for (int a = 0; a < 16; a++) X += up ? (nv[a] == mx && mx != mn) : (nv[a] == mn && mx != mn);
            for (int a = 0; a < 16; a++) if (nv[a] != (up ? mn + (a < X) : mx - (a < X))) return -1;
            nb = up ? mn : mx;
            if (X == 16) { nb += up ? 1 : -1; X = 0; }
            if (nb > 15) return -1;
        }
        dec[(t4 << 10) | (q << 2) | c] = (uint8_t)(nb | (X << 4));
    }
    for (int k = 0; k < RMAX; k++) for (int b = 0; b < 256; b++) {
        int code4 = 0;
        for (int j = 0; j < 2; j++) {
            const int h = (b >> (4 * j)) & 15, n = h & 7;
            const int code = (n != 7 && n > k) ? ((h & 8) ? 2 : 1) : 0;
            code4 |= code << (j ? 0 : 2);
        }
        exk[(k << 8) | b] = (uint8_t)code4;
    }
    return 0;
}

void pqbn_v4_build_motion(uint8_t *exk_m /*7*256*/, uint8_t *tau_nib /*2048*/, const uint8_t *tau /*4096*/) {
    for (int k = 0; k < 7; k++) for (int b = 0; b < 256; b++) {
        int code4 = 0;
        for (int j = 0; j < 2; j++) {
            const int h = (b >> (4 * j)) & 15, n = h & 7;
            const int code = n > k ? ((h & 8) ? 2 : 1) : 0;
            code4 |= code << (j ? 0 : 2);
        }
        exk_m[(k << 8) | b] = (uint8_t)code4;
    }
    memset(tau_nib, 0, 2048);
    for (int y = 0; y < 64; y++) for (int x = 0; x < 64; x++) {
        const int a = (16 * tau[y * 64 + x] + 8) / 17;          // white iff t4 > a  <=>  17*t4 > 16*tau + 8
        tau_nib[y * 32 + (x >> 1)] |= (uint8_t)(a << ((x & 1) * 4));
    }
}
#endif

static inline int sx5(int v) { return ((v & 31) ^ 16) - 16; }
static inline int snapq(int v) { return v > 252 ? 255 : v < 3 ? 0 : v; }
static inline __attribute__((unused)) int cls_of_rest(int r) { return r <= 0 ? 0 : r < 8 ? 1 : r < 64 ? 2 : 3; } // kept from the research file; unused here (clang -Wunused-function)
static const uint8_t lvl_tab[64] = {0, 1, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
                                     6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6};
static inline int lvl_of_rest(int r) { return r >= 64 ? 7 : lvl_tab[r]; }      // r >= 0
static const uint8_t lvl_cls[8] = {0, 1, 1, 1, 2, 2, 2, 3};
static const uint8_t lvl_lo[8] = {0, 1, 2, 4, 8, 16, 32, 64};
// advance a dwell level from phase t to phase now: L -> L+1 at the first phase (after reaching L)
// that is a multiple of 2^L shifted by a per-pixel, per-level offset (mean wait = level width)
static inline int level_up(int L, uint32_t t, uint32_t now, uint32_t h) {
    if (L >= 7) return 7;
    if (L == 0) { L = 1; t++; }                            // now > t always
    h ^= h >> 15; h *= 0x2C1B3C6Du; h ^= h >> 12;
    while (L < 7) {
        const uint32_t g = 1u << L, o = (h >> (3 * L)) & (g - 1);
        const uint32_t b = ((t - o) | (g - 1)) + 1 + o;
        if ((int32_t)(now - b) < 0) break;
        t = b; L++;
    }
    return L;
}

// per-tile reconstruction of the pixels planned in the tile's previous plan, indexed by n0 = 1..6
typedef struct {
    int16_t jD[8];            // (j - 1) * D where j = pushes done = min(k, n0)
    int8_t j[8];
    int8_t live[8];           // still in flight (j < n0): next push is a continuation, margin not done
    uint8_t L[8];             // dwell level after a completed plan
    int16_t rest[8];          // dwell after a completed plan, clamped to 256
} recon_t;

static inline void make_recon(recon_t *R, int k, int D) {
    for (int n0 = 1; n0 <= RMAX; n0++) {
        const int j = k < n0 ? k : n0;
        R->j[n0] = (int8_t)j; R->jD[n0] = (int16_t)((j - 1) * D); R->live[n0] = (int8_t)(j < n0);
        const int rest = k - n0 > 256 ? 256 : k - n0;
        R->rest[n0] = (int16_t)(rest < 0 ? 0 : rest); R->L[n0] = (uint8_t)(rest < 0 ? 0 : lvl_of_rest(rest));
    }
}

// plan one pixel; returns the new hot nibble | n << 4 | capped << 8, and rewrites its cold word
static inline int plan_px(const pqbn_v4_ctx *C, const recon_t *R, const int16_t (*cum)[16], uint16_t *cp,
                          int t4, int h, int a4, uint32_t tplan_old, uint32_t now, uint32_t hseed) {
    const int s = *cp, n0 = h & 7;
    int q, dc = sx5(s >> 8), m, L, rest;
    // ---- reconstruct the current model state and dwell
    if (n0 == 7) {                                        // was settled on a rail
        q = (h & 8) ? 255 : 0; m = 1;
        const int ts = (s & 255) | ((s >> 5) & 0x300);
        rest = (int)((now - (uint32_t)ts) & 1023); L = lvl_of_rest(rest);
    } else if (n0 == 0) {                                 // rested through the last plan
        q = s & 255; m = (unsigned)(q - 1) >= 254u;
        L = level_up(s >> 13, tplan_old, now, hseed); rest = lvl_lo[L];
    } else {                                              // planned: q field holds the state after push 1
        const int jd = R->jD[n0], jj = R->j[n0];
        if (h & 8) { q = snapq((s & 255) + jd); dc += jj; } else { q = snapq((s & 255) - jd); dc -= jj; }
        dc = dc > 15 ? 15 : dc < -15 ? -15 : dc;
        if (R->live[n0]) { m = 0; rest = 0; L = 0; } else { m = s >> 15; rest = R->rest[n0]; L = R->L[n0]; }
    }
    const int cls = lvl_cls[L];
    // ---- decide the push sequence
    const int d = t4 * 17;
    int n = 0, up = 1;
    if (d != q) {
        const int e = C->dec[(t4 << 10) | (q << 2) | cls];
        const int hi = a4 < (e >> 4);
        up = d > q;
        n = up ? (e & 15) + hi : (e & 15) - hi;
    }
    int qe = q, me = m;
    if (n) { const int c = cum[cls][n]; qe = snapq(up ? q + c : q - c); me = 0; }
    if ((unsigned)(qe - 1) >= 254u) {                     // plan ends on a rail: margin push / DC rebalance
        const int sr = qe ? 1 : -1;
        int dce = up ? dc + n : dc - n; dce = dce > 15 ? 15 : dce < -15 ? -15 : dce;
        const int v = qe ? dce : -dce;
        const int e0 = (!me) & (v < C->marg), e = -v > e0 ? -v : e0;     // margin push once, then rebalance to v >= 0
        if (e) { up = sr > 0; n += e; }
        me = 1;
    }
    // ---- write back
    if (n == 0) {
        if (me && t4 == (qe & 15) && (unsigned)(qe - 1) >= 254u) {   // settled on the target rail (DC safe since n == 0)
            if (rest > 256) rest = 256;
            const int ts = (int)((now - (uint32_t)rest) & 1023);
            *cp = (uint16_t)((ts & 255) | ((dc & 31) << 8) | ((ts >> 8) << 13) | 0x8000);
            return qe ? 0xF : 0x7;
        }
        *cp = (uint16_t)(q | ((dc & 31) << 8) | (L << 13));
        return 0;
    }
    int cap = 0;
    if (n > RMAX) { n = RMAX; me = 0; cap = 256; }
    const int c1 = cum[cls][1];
    *cp = (uint16_t)(snapq(up ? q + c1 : q - c1) | ((dc & 31) << 8) | (me << 15));
    return (up << 3) | n | (n << 4) | cap;
}

static int plan_tile(pqbn_v4_ctx *C, const int16_t (*cum)[16], int ti, int x0, int y0, uint32_t now) {
    const int W = C->W, Wh = W >> 1, D = C->step[0];
    const uint32_t tplan_old = C->tile_plan[ti];
    const int k = (uint16_t)(now - tplan_old);
    recon_t R; make_recon(&R, k, D);
    int nmx = 0, cap = 0, visited = 0;
    for (int y = y0; y < y0 + TPY; y++) {
        const uint32_t *fw = (const uint32_t *)(C->fb4 + (size_t)y * Wh + (x0 >> 1));
        uint32_t *hw = (uint32_t *)(C->hot + (size_t)y * Wh + (x0 >> 1));
        uint16_t *cr = C->cold + (size_t)y * W + x0;
        const uint8_t *taur = C->tau + ((y & 63) << 6) + (x0 & 63);
        const uint32_t hrow = (uint32_t)y * 0x85EBCA77u ^ (uint32_t)x0 * 0x9E3779B1u;
        for (int w = 0; w < TPX / 8; w++) {
            const uint32_t T = fw[w], Hn = hw[w];
            const uint32_t a = T & Hn;                            // nibble F: target white, settled white
            const uint32_t b = ~T & (Hn ^ 0x88888888u);           // nibble F: target black, settled black
            const uint32_t sett = ((a & (a >> 1) & (a >> 2) & (a >> 3)) | (b & (b >> 1) & (b >> 2) & (b >> 3))) & 0x11111111u;
            if (sett == 0x11111111u) continue;
            uint32_t Hnew = Hn & (sett * 15u);
            for (int j = 0; j < 8; j++) {
                if ((sett >> (4 * j)) & 1) continue;
                const int kp = w * 8 + j, t4 = (T >> (4 * j)) & 15, h = (Hn >> (4 * j)) & 15, n0 = h & 7;
                int nh;
                visited++;
                // fast path: a completed plan that ended on the target's rail with margin done -> settled
                if (n0 - 1u < (unsigned)RMAX && !R.live[n0] && (cr[kp] & 0x8000) && (t4 == 15 ? (h & 8) : t4 == 0 && !(h & 8))) {
                    const int s = cr[kp];
                    int dc = sx5(s >> 8) + ((h & 8) ? n0 : -n0); dc = dc > 15 ? 15 : dc < -15 ? -15 : dc;
                    const int ts = (int)((now - (uint32_t)R.rest[n0]) & 1023);
                    cr[kp] = (uint16_t)((ts & 255) | ((dc & 31) << 8) | ((ts >> 8) << 13) | 0x8000);
                    nh = t4 ? 0xF : 0x7;
                } else {
                    const int v = plan_px(C, &R, cum, &cr[kp], t4, h, taur[kp], tplan_old, now, hrow + (uint32_t)kp * 0x9E3779B1u);
                    const int n = (v >> 4) & 15;
                    nmx = n > nmx ? n : nmx; cap |= v;
                    nh = v & 15;
                }
                Hnew |= (uint32_t)nh << (4 * j);
            }
            hw[w] = Hnew;
        }
    }
    C->tile_plan[ti] = (uint16_t)now;
    C->tile_nmax[ti] = (uint8_t)((C->tile_nmax[ti] & T_HOT) | nmx | (((cap >> 8) & 1) << 7));
    band_touch(C, ti, now, nmx);
    return visited;
}

static inline int exec_tile(const pqbn_v4_ctx *C, int x0, int y0, int k) {
    const int W = C->W, Wh = W >> 1, Wq = W >> 2;
    const uint8_t *X = C->exk + (k << 8);
    const uint32_t kk = (uint32_t)(k + 1) * 0x11111111u;
    int act = 0;
    for (int y = y0; y < y0 + TPY; y++) {
        const uint32_t *hw = (const uint32_t *)(C->hot + (size_t)y * Wh + (x0 >> 1));
        uint16_t *dr = (uint16_t *)(C->drive + (size_t)y * Wq + (x0 >> 2));
        for (int w = 0; w < TPX / 8; w++) {
            const uint32_t H = hw[w];
            const uint32_t s7 = ((H & 0x77777777u) + 0x11111111u) & 0x88888888u;   // n == 7 (settled)
            const uint32_t pm = ((H | 0x88888888u) - kk) & ~s7 & 0x88888888u;   // n > k
            if (!pm) continue;
            dr[w] = (uint16_t)((X[H & 255] << 4) | X[(H >> 8) & 255] | (((X[(H >> 16) & 255] << 4) | X[H >> 24]) << 8));
            act++;
        }
    }
    return act;
}

// ---------------- motion mode ----------------
static inline uint32_t nib_gt(uint32_t T, uint32_t A) {         // per nibble: T > A -> bit 0 of the nibble
    const uint32_t m = 0x0F0F0F0Fu;
    const uint32_t ge = ((((T & m) | 0x10101010u) - (A & m) - 0x01010101u) >> 4) & 0x01010101u;
    const uint32_t go = ((((T >> 4) & m) | 0x10101010u) - ((A >> 4) & m) - 0x01010101u) & 0x10101010u;
    return ge | go;
}

static void mplan_tile(pqbn_v4_ctx *C, int ti, int x0, int y0, uint32_t now) {
    const int Wh = C->W >> 1;
    int k = (uint16_t)(now - C->tile_plan[ti]); if (k > 7) k = 7;
    const uint32_t kk = (uint32_t)k * 0x11111111u;
    uint32_t any = 0;
    for (int y = y0; y < y0 + TPY; y++) {
        const uint32_t *fw = (const uint32_t *)(C->fb4 + (size_t)y * Wh + (x0 >> 1));
        const uint32_t *aw = (const uint32_t *)(C->tau_nib + ((y & 63) << 5) + ((x0 & 63) >> 1));
        uint32_t *hw = (uint32_t *)(C->hot + (size_t)y * Wh + (x0 >> 1));
        for (int w = 0; w < TPX / 8; w++) {
            const uint32_t H = hw[w];
            const uint32_t t = (H | 0x88888888u) - kk;                            // 8 + r - k per nibble
            const uint32_t rem = t & (((t >> 3) & 0x11111111u) * 7u);             // max(r - k, 0)
            const uint32_t b = nib_gt(fw[w], aw[w]);                             // binary target: 1 = white
            const uint32_t Hn = (b << 3) | (rem ^ ((((H >> 3) & 0x11111111u) ^ b) * 7u));
            hw[w] = Hn; any |= Hn;
        }
    }
    C->tile_plan[ti] = (uint16_t)now;
    C->tile_nmax[ti] = (uint8_t)(T_MOT | (C->tile_nmax[ti] & T_HOT) | ((any & 0x77777777u) ? 7 : 0));
    band_touch(C, ti, now, 7);
}

static inline int exec_tile_m(const pqbn_v4_ctx *C, int x0, int y0, int k) {
    const int W = C->W, Wh = W >> 1, Wq = W >> 2;
    const uint8_t *X = C->exk_m + (k << 8);
    const uint32_t kk = (uint32_t)(k + 1) * 0x11111111u;
    int act = 0;
    for (int y = y0; y < y0 + TPY; y++) {
        const uint32_t *hw = (const uint32_t *)(C->hot + (size_t)y * Wh + (x0 >> 1));
        uint16_t *dr = (uint16_t *)(C->drive + (size_t)y * Wq + (x0 >> 2));
        for (int w = 0; w < TPX / 8; w++) {
            const uint32_t H = hw[w];
            if (!(((H | 0x88888888u) - kk) & 0x88888888u)) continue;            // no r > k
            dr[w] = (uint16_t)((X[H & 255] << 4) | X[(H >> 8) & 255] | (((X[(H >> 16) & 255] << 4) | X[H >> 24]) << 8));
            act++;
        }
    }
    return act;
}

static void band_recount(pqbn_v4_ctx *C, int ty) {          // tiles that need the planner's attention
    int c = 0;
    for (int t = ty * C->ntx; t < (ty + 1) * C->ntx; t++) c += (C->tile_nmax[t] & (T_PEND | T_CAP | T_MOT)) != 0;
    C->band[2 * ty + 1] = (uint16_t)c;
}

// one tile into motion mode (reconstructs each pixel's state once); returns its cost in planner units
static int menter_tile(pqbn_v4_ctx *C, int ti, uint32_t now) {
    const int W = C->W, Wh = W >> 1, D = C->step[0];
    const int x0 = (ti % C->ntx) * TPX, y0 = (ti / C->ntx) * TPY;
    recon_t R; make_recon(&R, (uint16_t)(now - C->tile_plan[ti]), D);
    uint32_t any = 0;
    int work = 0;
    for (int y = y0; y < y0 + TPY; y++) {
        const uint32_t *fw = (const uint32_t *)(C->fb4 + (size_t)y * Wh + (x0 >> 1));
        const uint32_t *aw = (const uint32_t *)(C->tau_nib + ((y & 63) << 5) + ((x0 & 63) >> 1));
        uint32_t *hw = (uint32_t *)(C->hot + (size_t)y * Wh + (x0 >> 1));
        uint16_t *cr = C->cold + (size_t)y * W + x0;
        for (int w = 0; w < TPX / 8; w++) {
            const uint32_t Hn = hw[w], b = nib_gt(fw[w], aw[w]);
            if ((((Hn & 0x77777777u) + 0x11111111u) & 0x88888888u) == 0x88888888u) {
                // all 8 settled on a rail: position 0 or 7 straight from the hot nibbles; keep DC, store position
                const uint32_t pos = ((Hn >> 3) & 0x11111111u) * 7u;
                const uint32_t Hm = (b << 3) | (pos ^ (b * 7u));
                for (int j = 0; j < 8; j++) cr[w * 8 + j] = (uint16_t)((cr[w * 8 + j] & 0x1F00) | ((pos >> (4 * j)) & 7));
                hw[w] = Hm; any |= Hm; work += 1;
                continue;
            }
            uint32_t Hm = 0;
            for (int j = 0; j < 8; j++) {
                const int h = (Hn >> (4 * j)) & 15, n0 = h & 7, s = cr[w * 8 + j];
                int q, dc = sx5(s >> 8);
                if (n0 == 7) q = (h & 8) ? 255 : 0;
                else if (n0 == 0) q = s & 255;
                else {
                    const int jd = R.jD[n0], jj = R.j[n0];
                    if (h & 8) { q = snapq((s & 255) + jd); dc += jj; } else { q = snapq((s & 255) - jd); dc -= jj; }
                    dc = dc > 15 ? 15 : dc < -15 ? -15 : dc;
                }
                const int pos = ((q * 7 + 127) * 257) >> 16;                     // round(q * 7 / 255)
                const int bw = (b >> (4 * j)) & 1;
                Hm |= (uint32_t)((bw << 3) | (bw ? 7 - pos : pos)) << (4 * j);
                cr[w * 8 + j] = (uint16_t)(pos | ((dc & 31) << 8));
            }
            hw[w] = Hm; any |= Hm; work += 4;
        }
    }
    C->tile_plan[ti] = (uint16_t)now;
    C->tile_nmax[ti] = (uint8_t)(T_MOT | (C->tile_nmax[ti] & T_HOT) | ((any & 0x77777777u) ? 7 : 0));
    C->tile_dirty[ti] = 0;
    band_touch(C, ti, now, 7);
    C->n_motion++;
    return work;
}

// one tile back to PQ-BN: pixels on a rail get two saturating pushes, then the tile is replanned by the
// full planner (capped-plan mechanism); returns its cost in planner units
static int mexit_tile(pqbn_v4_ctx *C, int ti, uint32_t now) {
    const int W = C->W, Wh = W >> 1;
    const int x0 = (ti % C->ntx) * TPX, y0 = (ti / C->ntx) * TPY;
    int k = (uint16_t)(now - C->tile_plan[ti]); if (k > 64) k = 64;
    int nmx = 0;
    for (int y = y0; y < y0 + TPY; y++) {
        uint32_t *hw = (uint32_t *)(C->hot + (size_t)y * Wh + (x0 >> 1));
        uint16_t *cr = C->cold + (size_t)y * W + x0;
        for (int w = 0; w < TPX / 8; w++) {
            const uint32_t Hn = hw[w];
            uint32_t Hx = 0;
            for (int j = 0; j < 8; j++) {
                const int h = (Hn >> (4 * j)) & 15, r = h & 7, rem = r > k ? r - k : 0;
                const int pos = (h & 8) ? 7 - rem : rem, s = cr[w * 8 + j];
                int dc = sx5(s >> 8) + pos - (s & 15); dc = dc > 15 ? 15 : dc < -15 ? -15 : dc;
                if (pos == 7 || pos == 0) {
                    // on a motion rail: one more push into it saturates the optical state (motion swings are
                    // 7 pushes, ~0.92 of a full swing); more if the DC sits on the other side (rebalance, as the
                    // PQ-BN rail rule does). Planned as a plan that ends on the rail with the margin done.
                    const int v = pos ? dc : -dc, e = -v > 1 ? -v : 1, n = e > RMAX ? RMAX : e;
                    Hx |= (uint32_t)((pos ? 8 : 0) | n) << (4 * j);
                    if (n > nmx) nmx = n;
                    cr[w * 8 + j] = (uint16_t)((pos ? 255 : 0) | ((dc & 31) << 8) | 0x8000);
                } else {                                  // caught mid-swing: estimate, rest 0
                    cr[w * 8 + j] = (uint16_t)(((pos * 255 + 3) / 7) | ((dc & 31) << 8));
                }
            }
            hw[w] = Hx;
        }
    }
    C->tile_plan[ti] = (uint16_t)now;
    C->tile_nmax[ti] = (uint8_t)(T_CAP | (C->tile_nmax[ti] & T_HOT) | nmx);   // full PQ-BN replan once these pushes are done
    C->tile_dirty[ti] = 0;
    band_touch(C, ti, now, nmx);
    C->n_motion--;
    return 20;
}

// explicit control (optional; with auto_motion the kernel decides by itself):
// switch the tiles with sel[ti] != 0 into motion mode
void pqbn_v4_motion_enter(pqbn_v4_ctx *C, uint32_t now, const uint8_t *sel) {
    for (int ti = 0; ti < C->ntx * C->nty; ti++)
        if (sel[ti] && !(C->tile_nmax[ti] & T_MOT)) menter_tile(C, ti, now);
    for (int ty = 0; ty < C->nty; ty++) band_recount(C, ty);
}
// back to PQ-BN for the tiles with sel[ti] != 0 (all motion tiles if sel is NULL)
void pqbn_v4_motion_exit(pqbn_v4_ctx *C, uint32_t now, const uint8_t *sel) {
    for (int ti = 0; ti < C->ntx * C->nty; ti++)
        if ((!sel || sel[ti]) && (C->tile_nmax[ti] & T_MOT)) mexit_tile(C, ti, now);
    for (int ty = 0; ty < C->nty; ty++) band_recount(C, ty);
}

// keeps settled pixels' 10-bit timestamps unambiguous: ages above 256 are clamped to 256
static void ts_sweep(pqbn_v4_ctx *C, uint32_t now) {
    const int W = C->W, Wh = W >> 1, i = (int)(now & (TS_SWEEP - 1));
    const int r0 = i * C->H / TS_SWEEP, r1 = (i + 1) * C->H / TS_SWEEP;
    for (int y = r0; y < r1; y++) {
        const uint32_t *hw = (const uint32_t *)(C->hot + (size_t)y * Wh);
        uint16_t *cr = C->cold + (size_t)y * W;
        const uint8_t *tn = C->tile_nmax + (y / TPY) * C->ntx;
        for (int w = 0; w < W / 8; w++) {
            const uint32_t H = hw[w];
            const uint32_t s7 = ((H & 0x77777777u) + 0x11111111u) & 0x88888888u;
            if (!s7 || (tn[w >> 2] & T_MOT)) continue;
            for (int j = 0; j < 8; j++) {
                if (!((s7 >> (4 * j + 3)) & 1)) continue;
                uint16_t *cp = &cr[w * 8 + j];
                const int s = *cp, ts = (s & 255) | ((s >> 5) & 0x300);
                if (((now - (uint32_t)ts) & 1023) > 256) {
                    const int nt = (int)((now - 256) & 1023);
                    *cp = (uint16_t)((s & 0x9F00) | (nt & 255) | ((nt >> 8) << 13));
                }
            }
        }
    }
}

int pqbn_v4_phase(pqbn_v4_ctx *C, uint32_t now) {
    const int ntx = C->ntx, Wq = C->W >> 2;
    int words = 0;
    int16_t cum[4][16];                                    // distance covered by n pushes starting in class c
    for (int c = 0; c < 4; c++) { cum[c][0] = 0; for (int n = 1; n < 16; n++) cum[c][n] = (int16_t)(C->step[c] + (n - 1) * C->step[0]); }
    if ((now & 4095) == 0) {                               // tile ages must not wrap (u16)
        for (int i = 0; i < ntx * C->nty; i++)
            if ((uint16_t)(now - C->tile_plan[i]) > 8192) C->tile_plan[i] = (uint16_t)(now - 8192);
        for (int ty = 0; ty < C->nty; ty++)
            if ((int16_t)(C->band[2 * ty] - (uint16_t)now) <= 0) C->band[2 * ty] = (uint16_t)now;
    }
    ts_sweep(C, now);
    // planning: tiles the UI changed, and tiles whose capped plan just finished; resumes at the cursor.
    // Tile rows without a UI mark or a tile needing attention are skipped after a byte scan.
    // With auto_motion: while the planner falls behind (or motion tiles exist), tiles that keep changing
    // (two or more UI updates in quick succession) switch to motion mode; motion tiles idle for EXIT_IDLE
    // phases switch back. One-off changes (page turn, a new line of text) always get full PQ-BN.
    const int nt = ntx * C->nty, budget = C->plan_budget;
    const int episode = C->auto_motion && (C->backlog > 0 || C->n_motion > 0);
    int used = 0, stop = -1, unserved = 0, ti = C->plan_cursor < nt ? C->plan_cursor : 0;
    for (int cnt = 0; cnt < nt; ) {
        const int ty = ti / ntx;
        int tend = (ty + 1) * ntx; if (tend - ti > nt - cnt) tend = ti + (nt - cnt);
        const int n = tend - ti;
        int any = C->band[2 * ty + 1];
        for (int t = ti; t < tend && !any; t++) any = C->tile_dirty[t];
        if (any) {
            for (int t = ti; t < tend; t++) {
                int tn = C->tile_nmax[t];
                if (C->tile_dirty[t]) {                        // acknowledge the UI's mark
                    const int act = (tn & T_HOT) >> 4;
                    const int recent = (tn & T_PEND) || (uint16_t)(now - C->tile_plan[t]) <= HOT_WINDOW;
                    tn = (tn & ~T_HOT) | ((recent ? act + (act < 3) : 1) << 4) | T_PEND;
                    C->tile_nmax[t] = (uint8_t)tn; C->tile_dirty[t] = 0;
                }
                const int x0 = (t % ntx) * TPX, y0 = ty * TPY;
                if (tn & T_MOT) {                              // motion tile: cheap SWAR plan on every UI update
                    if (tn & T_PEND) mplan_tile(C, t, x0, y0, now);
                    else if (C->auto_motion && (uint16_t)(now - C->tile_plan[t]) > EXIT_IDLE && (!budget || used < budget))
                        used += mexit_tile(C, t, now);
                    continue;
                }
                if (episode && (tn & T_PEND) && (tn & T_HOT) >= 0x20 && (!budget || used < budget)) {
                    used += menter_tile(C, t, now);
                    continue;
                }
                const int k = (uint16_t)(now - C->tile_plan[t]);
                if (!((tn & T_PEND) || ((tn & T_CAP) && k >= (tn & T_NMAX)))) continue;
                if (budget && used >= budget) { unserved += (tn & T_PEND) != 0; if (stop < 0) stop = t; continue; }
                used += plan_tile(C, (const int16_t (*)[16])cum, t, x0, y0, now);
            }
            band_recount(C, ty);
        }
        cnt += n; ti = tend == nt ? 0 : tend;
    }
    C->plan_cursor = stop >= 0 ? stop : 0;
    C->backlog = unserved ? C->backlog + 1 : 0;
    C->stat_planned += (uint32_t)used;
    // execution: every tile whose plan is still running
    for (int ty = 0; ty < C->nty && !C->line_mode; ty++) {
        const int y0 = ty * TPY, ti0 = ty * ntx;
        int any = (int16_t)(C->band[2 * ty] - (uint16_t)now) > 0;
        const uint8_t *tn = C->tile_nmax + ti0;
        const uint16_t *tp = C->tile_plan + ti0;
        if (any) { any = 0; for (int tx = 0; tx < ntx; tx++) any |= (int)(uint16_t)(now - tp[tx]) < (tn[tx] & T_NMAX); }
        for (int y = y0; y < y0 + TPY; y++) {
            C->row_active[y] = (uint8_t)any;
            if (any) memset(C->drive + (size_t)y * Wq, 0, Wq);
        }
        if (!any) continue;
        for (int tx = 0; tx < ntx; tx++) {
            const int k = (uint16_t)(now - tp[tx]);
            if (k < (tn[tx] & T_NMAX)) words += (tn[tx] & T_MOT) ? exec_tile_m(C, tx * TPX, y0, k) : exec_tile(C, tx * TPX, y0, k);
        }
    }
    return words;
}

// Line-by-line execution (no drive plane in PSRAM): writes the W/4-byte drive line of row y for phase
// `now` (after pqbn_v4_phase(C, now) planned it); returns 0 if the row has no push (send the zero line).
// Read-only, so it can run in the scan task while the planner works on rows the scan has passed.
PQBN_HOT int pqbn_v4_exec_row(const pqbn_v4_ctx *C, uint32_t now, int y, uint8_t *line) {
    const int ty = y / TPY, ti0 = ty * C->ntx, Wh = C->W >> 1;
    if ((int16_t)(C->band[2 * ty] - (uint16_t)now) <= 0) return 0;
    const uint32_t *hrow = (const uint32_t *)(C->hot + (size_t)y * Wh);
    uint16_t *out = (uint16_t *)line;
    uint32_t any = 0;
    for (int tx = 0; tx < C->ntx; tx++) {
        const int tn = C->tile_nmax[ti0 + tx], k = (uint16_t)(now - C->tile_plan[ti0 + tx]);
        uint16_t *o = out + tx * (TPX / 8);
        if (k >= (tn & T_NMAX)) { for (int w = 0; w < TPX / 8; w++) o[w] = 0; continue; }
        const int motion = tn & T_MOT;
        const uint8_t *X = (motion ? C->exk_m : C->exk) + (k << 8);
        const uint32_t kk = (uint32_t)(k + 1) * 0x11111111u;
        for (int w = 0; w < TPX / 8; w++) {
            const uint32_t H = hrow[tx * (TPX / 8) + w];
            const uint32_t s7 = motion ? 0 : ((H & 0x77777777u) + 0x11111111u) & 0x88888888u;
            const uint32_t pm = ((H | 0x88888888u) - kk) & ~s7 & 0x88888888u;
            o[w] = pm ? (uint16_t)((X[H & 255] << 4) | X[(H >> 8) & 255] | (((X[(H >> 16) & 255] << 4) | X[H >> 24]) << 8)) : 0;
            any |= pm;
        }
    }
    return any != 0;
}

#ifndef PQBN_RV
// initial state: every pixel at rest on its target level (rails settled), long dwell
void pqbn_v4_init(pqbn_v4_ctx *C, uint32_t now) {
    const int W = C->W, H = C->H;
    const int ts = (int)((now - 256) & 1023);
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
        const int t4 = (C->fb4[(size_t)y * (W >> 1) + (x >> 1)] >> ((x & 1) << 2)) & 15;
        int h; uint16_t s;
        if (t4 == 15 || t4 == 0) { h = t4 ? 0xF : 0x7; s = (uint16_t)((ts & 255) | ((ts >> 8) << 13) | 0x8000); }
        else { h = 0; s = (uint16_t)((t4 * 17) | (7 << 13)); }
        uint8_t *hb = C->hot + (size_t)y * (W >> 1) + (x >> 1);
        *hb = (uint8_t)((*hb & ~(15 << ((x & 1) << 2))) | (h << ((x & 1) << 2)));
        C->cold[(size_t)y * W + x] = s;
    }
    for (int i = 0; i < C->ntx * C->nty; i++) { C->tile_plan[i] = (uint16_t)(now - 8192); C->tile_nmax[i] = 0; C->tile_dirty[i] = 0; }
    C->plan_cursor = 0; C->stat_planned = 0; C->backlog = 0; C->n_motion = 0;
    for (int ty = 0; ty < C->nty; ty++) { C->band[2 * ty] = (uint16_t)now; C->band[2 * ty + 1] = 0; }
}
#endif
