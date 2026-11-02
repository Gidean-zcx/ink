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
//        n > 0   : [7:0] q0 model optical state at plan start, [12:8] dc0 DC balance at plan start
//                  (signed, saturating +-15), [14:13] class of the first push, [15] margin done at plan end
//        n = 0   : [7:0] q, [12:8] dc, [15:13] dwell level   (margin flag implied: q on a rail)
//        settled : [7:0] ts low, [12:8] dc, [14:13] ts high, [15] 1   (ts = first idle phase, 10 bit)
// Per-tile storage (32x4 px): dirty flag (from the UI), phase of the last plan (u16), max n | capped<<7.
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

typedef struct {
    int W, H, ntx, nty;
    const uint8_t *dec;       // 16 KB: [t4<<10 | q<<2 | cls] = nb | X<<4 ; n = up ? nb + (tau < X) : nb - (tau < X)
    const uint8_t *exk;       // RMAX x 256: 4-bit drive codes of a hot byte at plan phase k (even px bits 3:2)
    uint8_t step[4];          // first-push size per stiction class (Q8 of a full swing); step[0] = D
    int marg;
    const uint8_t *fb4;       // target 4 bpp (UI framebuffer), low nibble = even x
    uint8_t *hot;             // plan nibbles
    uint16_t *cold;           // per-pixel state words
    const uint8_t *tau;       // 64x64 blue-noise thresholds 0..15
    uint8_t *tile_dirty;      // set by the UI layer from invalidated areas; cleared here
    uint16_t *tile_plan;      // phase of the tile's last plan
    uint8_t *tile_nmax;       // max n of that plan | 0x80 if some pixel's plan was capped at RMAX
    uint8_t *drive;           // out: 2 bit/px, W/4 bytes/row, 1 = black push, 2 = white push, pixel 0 in bits 7:6
    uint8_t *row_active;      // out: row has drive data this phase (else the DMA sends the zero line)
} pqbn_v4a_ctx;

#ifndef PQBN_RV
// representative dwell of each stiction class, used for the class's first-push size
float pqbn_v4a_class_dwell[4] = {0.f, 3.f, 24.f, 256.f};

// number of pushes the PQ-BN rule chooses from state q toward target t4 (no margin, no cap)
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

// returns 0 on success, -1 if a table entry cannot be expressed as nb +- (tau < X)
int pqbn_v4a_build(uint8_t *dec /*16384*/, uint8_t *exk /*RMAX*256*/, uint8_t *step_out, float T0, float s0, float taud) {
    const float D = 1.0f / T0;
    int step[4];
    for (int c = 0; c < 4; c++) {
        const float st = c ? 1.f - s0 * (1.f - expf(-pqbn_v4a_class_dwell[c] / taud)) : 1.f;
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
#endif

static inline int sx5(int v) { return ((v & 31) ^ 16) - 16; }
static inline int snapq(int v) { return v > 252 ? 255 : v < 3 ? 0 : v; }
static inline int cls_of_rest(int r) { return r <= 0 ? 0 : r < 8 ? 1 : r < 64 ? 2 : 3; }
static inline int lvl_of_rest(int r) { return r <= 0 ? 0 : r < 2 ? 1 : r < 4 ? 2 : r < 8 ? 3 : r < 16 ? 4 : r < 32 ? 5 : r < 64 ? 6 : 7; }
static const uint8_t lvl_cls[8] = {0, 1, 1, 1, 2, 2, 2, 3};
static const uint8_t lvl_lo[8] = {0, 1, 2, 4, 8, 16, 32, 64};
// advance a dwell level from phase t to phase now: L -> L+1 at the first phase (after reaching L)
// that is a multiple of 2^L shifted by a per-pixel, per-level offset (mean wait = level width)
static inline int level_up(int L, uint32_t t, uint32_t now, uint32_t h) {
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

// plan one pixel; returns the new hot nibble and rewrites its cold word
static inline int plan_px(const pqbn_v4a_ctx *C, uint16_t *cp, int t4, int h, int a4, int k,
                          uint32_t tplan_old, uint32_t now, uint32_t hseed, int *nmx, int *capped) {
    const int D = C->step[0];
    const uint16_t s = *cp;
    int q, dc, m, L, rest;
    // ---- reconstruct the current model state and dwell
    if ((h & 7) == 7) {                                   // was settled on a rail
        q = (h & 8) ? 255 : 0; dc = sx5(s >> 8); m = 1;
        const int ts = (s & 255) | ((s >> 5) & 0x300);
        rest = (int)((now - (uint32_t)ts) & 1023); L = lvl_of_rest(rest);
    } else {
        const int n0 = h & 7, q0 = s & 255;
        if (n0 == 0) {
            q = q0; dc = sx5(s >> 8); m = q0 == 0 || q0 == 255;
            L = level_up(s >> 13, tplan_old, now, hseed); rest = lvl_lo[L];
        } else {
            const int sg0 = (h & 8) ? 1 : -1, j = k < n0 ? k : n0;     // pushes done so far (k >= 1)
            q = snapq(q0 + sg0 * (C->step[(s >> 13) & 3] + (j - 1) * D));
            dc = sx5(s >> 8) + sg0 * j; dc = dc > 15 ? 15 : dc < -15 ? -15 : dc;
            if (j < n0) { m = 0; rest = 0; L = 0; }           // in flight: next push is a continuation
            else { m = s >> 15; rest = k - n0; L = lvl_of_rest(rest); }
        }
    }
    const int cls = lvl_cls[L];
    // ---- decide the push sequence
    const int d = t4 * 17;
    int n = 0, sg = 1;
    if (d != q) {
        const int e = C->dec[(t4 << 10) | (q << 2) | cls];
        const int hi = a4 < (e >> 4), up = d > q;
        n = (e & 15) + (up ? hi : -hi);
        sg = up ? 1 : -1;
    }
    int qe = q, me = m;
    if (n) { qe = snapq(q + sg * (C->step[cls] + (n - 1) * D)); me = 0; }
    if (qe == 255 || qe == 0) {                           // plan ends on a rail: margin push / DC rebalance
        const int sr = qe ? 1 : -1;
        int dce = dc + sg * n; dce = dce > 15 ? 15 : dce < -15 ? -15 : dce;
        const int v = sr * dce;
        const int e = !me ? (v < C->marg ? (-v > 1 ? -v : 1) : 0) : (v < 0 ? -v : 0);
        if (e) { sg = sr; n += e; }
        me = 1;
    }
    if (n > RMAX) { n = RMAX; me = 0; *capped = 1; }
    if (n > *nmx) *nmx = n;
    // ---- write back
    if (n == 0) {
        if (me && ((qe == 255 && t4 == 15) || (qe == 0 && t4 == 0))) {   // settled (DC safe since n == 0)
            if (rest > 256) rest = 256;
            const int ts = (int)((now - (uint32_t)rest) & 1023);
            *cp = (uint16_t)((ts & 255) | ((dc & 31) << 8) | ((ts >> 8) << 13) | 0x8000);
            return qe ? 0xF : 0x7;
        }
        *cp = (uint16_t)(q | ((dc & 31) << 8) | (L << 13));
        return 0;
    }
    *cp = (uint16_t)(q | ((dc & 31) << 8) | (cls << 13) | (me << 15));
    return ((sg > 0) << 3) | n;
}

static void plan_tile(pqbn_v4a_ctx *C, int ti, int x0, int y0, uint32_t now) {
    const int W = C->W, Wh = W >> 1;
    const uint32_t tplan_old = C->tile_plan[ti];
    const int k = (uint16_t)(now - tplan_old);
    int nmx = 0, capped = 0;
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
            uint32_t Hnew = Hn;
            for (int j = 0; j < 8; j++) {
                if ((sett >> (4 * j)) & 1) continue;
                const int kp = w * 8 + j;
                const int nh = plan_px(C, &cr[kp], (T >> (4 * j)) & 15, (Hn >> (4 * j)) & 15, taur[kp], k, tplan_old, now,
                                       hrow + (uint32_t)kp * 0x9E3779B1u, &nmx, &capped);
                Hnew = (Hnew & ~(15u << (4 * j))) | ((uint32_t)nh << (4 * j));
            }
            hw[w] = Hnew;
        }
    }
    C->tile_plan[ti] = (uint16_t)now;
    C->tile_nmax[ti] = (uint8_t)(nmx | (capped << 7));
}

static inline int exec_tile(const pqbn_v4a_ctx *C, int x0, int y0, int k) {
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

// keeps settled pixels' 10-bit timestamps unambiguous: ages above 256 are clamped to 256
static void ts_sweep(pqbn_v4a_ctx *C, uint32_t now) {
    const int W = C->W, Wh = W >> 1, i = (int)(now & (TS_SWEEP - 1));
    const int r0 = i * C->H / TS_SWEEP, r1 = (i + 1) * C->H / TS_SWEEP;
    for (int y = r0; y < r1; y++) {
        const uint32_t *hw = (const uint32_t *)(C->hot + (size_t)y * Wh);
        uint16_t *cr = C->cold + (size_t)y * W;
        for (int w = 0; w < W / 8; w++) {
            const uint32_t H = hw[w];
            const uint32_t s7 = ((H & 0x77777777u) + 0x11111111u) & 0x88888888u;
            if (!s7) continue;
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

int pqbn_v4a_phase(pqbn_v4a_ctx *C, uint32_t now) {
    const int ntx = C->ntx, Wq = C->W >> 2;
    int words = 0;
    if ((now & 4095) == 0)                                 // tile ages must not wrap (u16)
        for (int i = 0; i < ntx * C->nty; i++)
            if ((uint16_t)(now - C->tile_plan[i]) > 8192) C->tile_plan[i] = (uint16_t)(now - 8192);
    ts_sweep(C, now);
    for (int ty = 0; ty < C->nty; ty++) {
        const int y0 = ty * TPY, ti0 = ty * ntx;
        uint8_t *td = C->tile_dirty + ti0, *tn = C->tile_nmax + ti0;
        uint16_t *tp = C->tile_plan + ti0;
        int any = 0;
        for (int tx = 0; tx < ntx; tx++) {
            const int k = (uint16_t)(now - tp[tx]), nm = tn[tx] & 127;
            if (td[tx] || ((tn[tx] & 128) && k >= nm)) { plan_tile(C, ti0 + tx, tx * TPX, y0, now); td[tx] = 0; }
            any |= (int)(uint16_t)(now - tp[tx]) < (tn[tx] & 127);
        }
        for (int y = y0; y < y0 + TPY; y++) {
            C->row_active[y] = (uint8_t)any;
            if (any) memset(C->drive + (size_t)y * Wq, 0, Wq);
        }
        if (!any) continue;
        for (int tx = 0; tx < ntx; tx++) {
            const int k = (uint16_t)(now - tp[tx]);
            if (k < (tn[tx] & 127)) words += exec_tile(C, tx * TPX, y0, k);
        }
    }
    return words;
}

#ifndef PQBN_RV
// initial state: every pixel at rest on its target level (rails settled), long dwell
void pqbn_v4a_init(pqbn_v4a_ctx *C, uint32_t now) {
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
}
#endif
