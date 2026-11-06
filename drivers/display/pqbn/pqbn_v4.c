/* PQ-BN kernel: see pqbn_v4.h. The planner runs only in tiles the UI changed: it decides a pixel's whole push sequence at
 * once, n pushes (0..6) in one direction, one per phase. The executor runs every phase, only in tiles whose plan is
 * still running: a pixel pushes in phase k of its tile's plan iff n > k, so the plan nibbles are read-only and 8 pixels are
 * tested with one SWAR expression.
 *
 * Dwell (idle phases before a pixel's first push) decides how far that push goes, by class: 0: 0, 1: 1..7, 2: 8..63,
 * 3: >= 64. It is exact for pixels that pushed in their last plan (from the tile's plan phase and n) and for settled
 * pixels (10-bit timestamp). Pixels that rested through their last plan keep a 3-bit log2 level (dwell >= 0,1,2,4,8,16,32,64)
 * advanced at per-pixel-offset 2^L boundaries, so it tracks dwell without bias however often the tile is replanned.
 *
 * Per-pixel storage (2.5 B/px):
 *   hot  (4 bit, packed like fb4: low nibble = even x)
 *        dir<<3 | n   n = pushes planned from the tile's plan phase (0..6), dir 1 = toward white
 *        0x7 / 0xF    settled on the black / white rail (target matches, margin done, DC safe)
 *   cold (16 bit)
 *        n > 0   : [7:0] q1 model state after the plan's first push, [12:8] dc0 DC balance at plan start (signed, saturating
 *                  +-15), [14:13] 0, [15] plan ends on a rail, margin done
 *        n = 0   : [7:0] q, [12:8] dc, [15:13] dwell level   (margin flag implied: q on a rail)
 *        settled : [7:0] ts low, [12:8] dc, [14:13] ts high, [15] 1   (ts = first idle phase, 10 bit)
 * Per-tile storage (32x4 px): dirty flag (from the UI), phase of the last plan (u16), max n | flags.
 * Tables: 16 KB decision table, 1.5 KB executor table, 4 KB thresholds. */
#include <math.h>
#include <stdint.h>
#include <string.h>

#include "pqbn_v4.h"

#define TPX 32
#define TPY 4
#define RMAX 6
#define TS_SWEEP 512          // every settled pixel's timestamp is visited at least once per TS_SWEEP phases

static inline void band_touch(pqbn_v4_ctx *C, int ti, uint32_t now, int nmax) {
    uint16_t *u = &C->band[2 * (ti / C->ntx)];
    const uint16_t e = (uint16_t)(now + nmax);
    if ((int16_t)(e - *u) > 0) *u = e;
}

// number of pushes from state q toward target state d (no margin, no cap): the two reachable states around the
// target, chosen between by the threshold a4
static int decide_n(int d, int q, int a4, int c, const int *step) {
    const int t = a4 * 16 + 8;
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

// one decision table entry: n(a) over the 16 thresholds must be expressible as nb +- (a < X); -1 if not
static int dec_entry(int d, int q, int c, const int *step) {
    int nb = 0, X = 0;
    if (d != q) {
        int nv[16], mn = 99, mx = -1;
        for (int a = 0; a < 16; a++) { nv[a] = decide_n(d, q, a, c, step); if (nv[a] < mn) mn = nv[a]; if (nv[a] > mx) mx = nv[a]; }
        if (mx - mn > 1) return -1;
        const int up = d > q;
        // up: n(a) = mn + (a < X) ; down: n(a) = mx - (a < X)
        for (int a = 0; a < 16; a++) X += up ? (nv[a] == mx && mx != mn) : (nv[a] == mn && mx != mn);
        for (int a = 0; a < 16; a++) if (nv[a] != (up ? mn + (a < X) : mx - (a < X))) return -1;
        nb = up ? mn : mx;
        if (X == 16) { nb += up ? 1 : -1; X = 0; }
        if (nb > 15) return -1;
    }
    return nb | (X << 4);
}

int pqbn_v4_build(pqbn_v4_ctx *C, uint8_t *dec, uint8_t *exk, uint8_t *tau, const pqbn_panel_t *P) {
    const float D = 255.0f / P->scale_pushes;
    int step[4];
    for (int c = 0; c < 4; c++) { step[c] = (int)lrintf(D * P->first_push[c]); C->step[c] = (uint8_t)step[c]; }
    for (int t4 = 0; t4 < 16; t4++) {
        const float n = P->pushes[t4];
        int d = (int)lrintf(t4 > P->split ? 255.0f - n * D : n * D);
        if (t4 > 0 && t4 < 15) d = d > 250 ? 250 : d < 5 ? 5 : d;
        if (t4 == 15) d = 255;
        if (t4 == 0) d = 0;
        C->level[t4] = (uint8_t)d;
        // within a push of it a pixel shows this level; a rail is always moved to or settled on
        const int l = d - step[0], h = d + step[0];
        C->lo[t4] = (uint8_t)(t4 == 0 || t4 == 15 ? d : l < 1 ? 1 : l);
        C->hi[t4] = (uint8_t)(t4 == 0 || t4 == 15 ? d : h > 254 ? 254 : h);
        for (int q = 0; q < 256; q++) for (int c = 0; c < 4; c++) {
            const int e = dec_entry(d, q, c, step);
            if (e < 0) return -1;
            dec[(t4 << 10) | (q << 2) | c] = (uint8_t)e;
        }
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
    // random dots read as noise on the panel; two pixels by two is finer than the eye (D-110)
    static const uint8_t ORDERED[4] = {2, 10, 14, 6};
    for (int i = 0; i < 4096; i++) tau[i] = ORDERED[(i & 1) | ((i >> 5) & 2)];
    C->split_nib = (uint32_t)P->split * 0x11111111u;
    C->swing_step = (255 + P->swing_units - 1) / P->swing_units;
    C->dec = dec; C->exk = exk; C->tau = tau; C->marg = 2;
    return 0;
}

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
    int8_t dcj[8];            // pushes done = j * unit (a swing plans in units of two pushes)
    int8_t live[8];           // still in flight (j < n0): next push is a continuation, margin not done
    uint8_t L[8];             // dwell level after a completed plan
    int16_t rest[8];          // dwell after a completed plan, clamped to 256
} recon_t;

static inline void make_recon(recon_t *R, int k, int D, int unit) {
    for (int n0 = 1; n0 <= RMAX; n0++) {
        const int j = k / unit < n0 ? k / unit : n0;
        R->j[n0] = (int8_t)j; R->jD[n0] = (int16_t)((j - 1) * D); R->live[n0] = (int8_t)(j < n0); R->dcj[n0] = (int8_t)(j * unit);
        const int rest = k - n0 * unit > 256 ? 256 : k - n0 * unit;
        R->rest[n0] = (int16_t)(rest < 0 ? 0 : rest); R->L[n0] = (uint8_t)(rest < 0 ? 0 : lvl_of_rest(rest));
    }
}

// the current model state and dwell of a pixel, from its hot nibble and cold word
static inline void px_state(const recon_t *R, int s, int h, uint32_t tplan_old, uint32_t now, uint32_t hseed,
                            int *qo, int *dco, int *mo, int *Lo, int *resto) {
    const int n0 = h & 7;
    int q, dc = sx5(s >> 8), m, L, rest;
    if (n0 == 7) {                                        // was settled on a rail
        q = (h & 8) ? 255 : 0; m = 1;
        const int ts = (s & 255) | ((s >> 5) & 0x300);
        rest = (int)((now - (uint32_t)ts) & 1023); L = lvl_of_rest(rest);
    } else if (n0 == 0) {                                 // rested through the last plan
        q = s & 255; m = (unsigned)(q - 1) >= 254u;
        L = level_up(s >> 13, tplan_old, now, hseed); rest = lvl_lo[L];
    } else {                                              // planned: q field holds the state after push 1
        const int jd = R->jD[n0], jj = R->dcj[n0];
        if (h & 8) { q = snapq((s & 255) + jd); dc += jj; } else { q = snapq((s & 255) - jd); dc -= jj; }
        dc = dc > 15 ? 15 : dc < -15 ? -15 : dc;
        if (R->live[n0]) { m = 0; rest = 0; L = 0; } else { m = s >> 15; rest = R->rest[n0]; L = R->L[n0]; }
    }
    *qo = q; *dco = dc; *mo = m; *Lo = L; *resto = rest;
}

// plan one pixel; returns the new hot nibble | n << 4 | capped << 8, and rewrites its cold word
static inline int plan_px(const pqbn_v4_ctx *C, const recon_t *R, const int16_t (*cum)[16], uint16_t *cp,
                          int t4, int h, int a4, uint32_t tplan_old, uint32_t now, uint32_t hseed) {
    const int s = *cp;
    int q, dc, m, L, rest;
    px_state(R, s, h, tplan_old, now, hseed, &q, &dc, &m, &L, &rest);
    const int cls = lvl_cls[L];
    // ---- decide the push sequence
    const int d = C->level[t4];
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
    const int k = (uint16_t)(now - tplan_old), tn0 = C->tile_nmax[ti];
    const int swung = tn0 & T_SWING;
    recon_t R; make_recon(&R, k, swung ? C->swing_step : D, swung ? 2 : 1);
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
                    int dc = sx5(s >> 8) + ((h & 8) ? R.dcj[n0] : -R.dcj[n0]); dc = dc > 15 ? 15 : dc < -15 ? -15 : dc;
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
    C->tile_nmax[ti] = (uint8_t)(nmx | (((cap >> 8) & 1) << 7));
    band_touch(C, ti, now, nmx);
    return visited;
}

static inline uint32_t nib_gt(uint32_t T, uint32_t A) {         // per nibble: T > A -> bit 0 of the nibble
    const uint32_t m = 0x0F0F0F0Fu;
    const uint32_t ge = ((((T & m) | 0x10101010u) - (A & m) - 0x01010101u) >> 4) & 0x01010101u;
    const uint32_t go = ((((T >> 4) & m) | 0x10101010u) - ((A >> 4) & m) - 0x01010101u) & 0x10101010u;
    return ge | go;
}

// The first stage of a change. Every pixel goes to the rail its level comes from, in units of two pushes, so one plan covers
// a whole swing. Words already there are skipped without reading a cold word: only pixels that move pay. The planner
// proper then settles the tile on the real target (T_CAP, T_SWING).
static int swing_tile(pqbn_v4_ctx *C, int ti, int x0, int y0, uint32_t now, int page) {
    const int W = C->W, Wh = W >> 1, S = C->swing_step;
    const uint32_t tplan_old = C->tile_plan[ti];
    const int k = (uint16_t)(now - tplan_old), was_swing = C->tile_nmax[ti] & T_SWING;
    recon_t R; make_recon(&R, k, was_swing ? S : C->step[0], was_swing ? 2 : 1);
    int nmx = 0, visited = 0;
    for (int y = y0; y < y0 + TPY; y++) {
        const uint32_t *fw = (const uint32_t *)(C->fb4 + (size_t)y * Wh + (x0 >> 1));
        uint32_t *hw = (uint32_t *)(C->hot + (size_t)y * Wh + (x0 >> 1));
        uint16_t *cr = C->cold + (size_t)y * W + x0;
        const uint32_t hrow = (uint32_t)y * 0x85EBCA77u ^ (uint32_t)x0 * 0x9E3779B1u;
        for (int w = 0; w < TPX / 8; w++) {
            const uint32_t T = fw[w], Hn = hw[w], b = nib_gt(T, C->split_nib);    // its rail: bit 0 of a nibble, 1 = white
            const uint32_t Tb = b * 15u;
            const uint32_t a0 = Tb & Hn, b0 = ~Tb & (Hn ^ 0x88888888u);    // nibble F: settled on the binary target's rail
            const uint32_t x0w = T ^ Tb, exact = ~(x0w | (x0w >> 1) | (x0w >> 2) | (x0w >> 3));  // bit 0 of a nibble: the target is that rail
            const uint32_t sett = ((a0 & (a0 >> 1) & (a0 >> 2) & (a0 >> 3)) | (b0 & (b0 >> 1) & (b0 >> 2) & (b0 >> 3))) & exact & 0x11111111u;
            if (sett == 0x11111111u) continue;
            uint32_t Hnew = Hn & (sett * 15u);
            for (int j = 0; j < 8; j++) {
                if ((sett >> (4 * j)) & 1) continue;
                const int kp = w * 8 + j, t4 = (T >> (4 * j)) & 15, h = (Hn >> (4 * j)) & 15, bw = (b >> (4 * j)) & 1;
                int q, dc, m, L, rest;
                visited++;
                px_state(&R, cr[kp], h, tplan_old, now, hrow + (uint32_t)kp * 0x9E3779B1u, &q, &dc, &m, &L, &rest);
                const int d = bw ? 255 : 0, dist = bw ? 255 - q : q;
                // On its rail already, or already showing its level: a small change leaves it still, or a gray area would
                // blink around it. A new page moves it too, or what stayed would show as a ghost beside what moved.
                if (!dist || (!page && q >= C->lo[t4] && q <= C->hi[t4])) {
                    cr[kp] = (uint16_t)(q | ((dc & 31) << 8) | (L << 13));
                    continue;
                }
                // from between the rails, one unit more: what is left of a gray must not show on the rail
                int units = (dist + S - 1) / S + (q != 0 && q != 255);
                if (units > RMAX) units = RMAX;
                if (units > nmx) nmx = units;
                cr[kp] = (uint16_t)(snapq(d ? q + S : q - S) | ((dc & 31) << 8) | 0x8000); // the plan ends on a rail, margin done
                Hnew |= (uint32_t)((bw << 3) | units) << (4 * j);
            }
            hw[w] = Hnew;
        }
    }
    C->tile_plan[ti] = (uint16_t)now;
    C->tile_nmax[ti] = (uint8_t)(T_SWING | T_CAP | nmx);
    band_touch(C, ti, now, 2 * nmx);
    return visited;
}

// keeps settled pixels' 10-bit timestamps unambiguous: ages above 256 are clamped to 256
static void ts_sweep(pqbn_v4_ctx *C, uint32_t now) {
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

static void age_clamp(pqbn_v4_ctx *C, uint32_t now) { // at multiples of 4096: tile ages must not wrap (u16)
    for (int i = 0; i < C->ntx * C->nty; i++)
        if ((uint16_t)(now - C->tile_plan[i]) > 8192) C->tile_plan[i] = (uint16_t)(now - 8192);
    for (int ty = 0; ty < C->nty; ty++)
        if ((int16_t)(C->band[2 * ty] - (uint16_t)now) <= 0) C->band[2 * ty] = (uint16_t)now;
}

// A rest longer than TS_SWEEP phases, in one pass: where the phases would have run, a settled pixel's
// timestamp ends as (the last phase its row was swept in) - 256 if its age exceeded 256 then, and a tile's
// plan age is clamped at the last multiple of 4096. The true ages at `from` are unambiguous (the sweeps
// keep timestamps younger than 256 + TS_SWEEP, the clamps keep tile ages below 8192 + 4096), so the
// final state follows from them without running the phases.
static void skip_long(pqbn_v4_ctx *C, uint32_t from, uint32_t to) {
    const int W = C->W, Wh = W >> 1;
    for (int i = 0; i < TS_SWEEP; i++) {
        const int r0 = i * C->H / TS_SWEEP, r1 = (i + 1) * C->H / TS_SWEEP;
        if (r0 == r1) continue;
        const uint32_t last = to - ((to - (uint32_t)i) & (TS_SWEEP - 1)); // the last phase of sweep index i
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
                    if ((uint64_t)((from - (uint32_t)ts) & 1023) + (last - from) > 256) {
                        const int nt = (int)((last - 256) & 1023);
                        *cp = (uint16_t)((s & 0x9F00) | (nt & 255) | ((nt >> 8) << 13));
                    }
                }
            }
        }
    }
    const uint32_t mark = to & ~4095u; // the last multiple of 4096 the rest passed, if any
    if (to - mark >= to - from) return;
    for (int t = 0; t < C->ntx * C->nty; t++)
        if ((uint64_t)(uint16_t)(from - C->tile_plan[t]) + (mark - from) > 8192) C->tile_plan[t] = (uint16_t)(mark - 8192);
    for (int ty = 0; ty < C->nty; ty++)
        if ((int64_t)(int16_t)(C->band[2 * ty] - (uint16_t)from) - (int64_t)(mark - from) <= 0) C->band[2 * ty] = (uint16_t)mark;
}

int pqbn_v4_skip(pqbn_v4_ctx *C, uint32_t from, uint32_t to) {
    const uint32_t n = to - from;
    if (n > TS_SWEEP) {
        skip_long(C, from, to);
        return C->H; // every row once
    }
    int rows = 0;
    for (uint32_t p = from + 1; p != to + 1; p++) {
        if ((p & 4095) == 0) age_clamp(C, p);
        ts_sweep(C, p);
        const int i = (int)(p & (TS_SWEEP - 1));
        rows += (i + 1) * C->H / TS_SWEEP - i * C->H / TS_SWEEP;
    }
    return rows;
}

static void band_recount(pqbn_v4_ctx *C, int ty) {          // tiles that need the planner's attention
    int c = 0;
    for (int t = ty * C->ntx; t < (ty + 1) * C->ntx; t++) c += (C->tile_nmax[t] & (T_PEND | T_CAP)) != 0;
    C->band[2 * ty + 1] = (uint16_t)c;
}

static int bands_quiet(const pqbn_v4_ctx *C, uint32_t now) {   // nothing pushes in phase now
    for (int ty = 0; ty < C->nty; ty++)
        if ((int16_t)(C->band[2 * ty] - (uint16_t)now) > 0) return 0;
    return 1;
}

int pqbn_v4_plan_due(const pqbn_v4_ctx *C, uint32_t now) {
    const int nt = C->ntx * C->nty, quiet = bands_quiet(C, now);
    for (int i = 0; i < nt; i++) {
        const int tn = C->tile_nmax[i];
        if (C->tile_dirty[i] || (tn & T_PEND) || (quiet && (tn & T_CAP))) return 1;
    }
    return 0;
}

int pqbn_v4_busy_after(const pqbn_v4_ctx *C, uint32_t now) {
    const int nt = C->ntx * C->nty;
    for (int i = 0; i < nt; i++)
        if (C->tile_dirty[i] || (C->tile_nmax[i] & (T_PEND | T_CAP))) return 1;
    return !bands_quiet(C, now + 1);
}

void pqbn_v4_phase(pqbn_v4_ctx *C, uint32_t now) {
    const int ntx = C->ntx;
    int16_t cum[4][16];                                    // distance covered by n pushes starting in class c
    for (int c = 0; c < 4; c++) { cum[c][0] = 0; for (int n = 1; n < 16; n++) cum[c][n] = (int16_t)(C->step[c] + (n - 1) * C->step[0]); }
    if ((now & 4095) == 0) age_clamp(C, now);
    ts_sweep(C, now);
    // A change swings at once. The settling waits until nothing pushes, so all of it is planned together and a plan
    // never holds a push. Tile rows without a UI mark or a tile needing attention are skipped after a byte scan.
    const int quiet = bands_quiet(C, now);
    int used = 0, marked = 0;
    for (int t = 0; t < ntx * C->nty; t++) marked += C->tile_dirty[t];
    const int page = marked * 3 > ntx * C->nty;            // a new page rather than a small change
    for (int ty = 0; ty < C->nty; ty++) {
        const int t0 = ty * ntx, t1 = t0 + ntx;
        int any = C->band[2 * ty + 1];
        for (int t = t0; t < t1 && !any; t++) any = C->tile_dirty[t];
        if (!any) continue;
        for (int t = t0; t < t1; t++) {
            int tn = C->tile_nmax[t];
            if (C->tile_dirty[t]) {                        // acknowledge the UI's mark
                tn |= T_PEND;
                C->tile_nmax[t] = (uint8_t)tn; C->tile_dirty[t] = 0;
            }
            const int x0 = (t - t0) * TPX, y0 = ty * TPY;
            if (tn & T_PEND) {
                const int v = swing_tile(C, t, x0, y0, now, page);
                used += v;
                C->stat_swung += (uint32_t)v;
            }
            else if ((tn & T_CAP) && quiet) used += plan_tile(C, (const int16_t (*)[16])cum, t, x0, y0, now);
        }
        band_recount(C, ty);
    }
    C->stat_planned += (uint32_t)used;
}

// writes the W/4-byte drive line of row y for phase `now` (after pqbn_v4_phase(C, now) planned it); returns 0 if the row
// has no push (send the zero line).
PQBN_HOT int pqbn_v4_exec_row(const pqbn_v4_ctx *C, uint32_t now, int y, uint8_t *line) {
    const int ty = y / TPY, ti0 = ty * C->ntx, Wh = C->W >> 1;
    if ((int16_t)(C->band[2 * ty] - (uint16_t)now) <= 0) return 0;
    const uint32_t *hrow = (const uint32_t *)(C->hot + (size_t)y * Wh);
    uint16_t *out = (uint16_t *)line;
    uint32_t any = 0;
    for (int tx = 0; tx < C->ntx; tx++) {
        const int tn = C->tile_nmax[ti0 + tx], k0 = (uint16_t)(now - C->tile_plan[ti0 + tx]);
        const int k = (tn & T_SWING) ? k0 >> 1 : k0;      // a swing's unit is two pushes
        uint16_t *o = out + tx * (TPX / 8);
        if (k >= (tn & T_NMAX)) { for (int w = 0; w < TPX / 8; w++) o[w] = 0; continue; }
        const uint8_t *X = C->exk + (k << 8);
        const uint32_t kk = (uint32_t)(k + 1) * 0x11111111u;
        for (int w = 0; w < TPX / 8; w++) {
            const uint32_t H = hrow[tx * (TPX / 8) + w];
            const uint32_t s7 = ((H & 0x77777777u) + 0x11111111u) & 0x88888888u;
            const uint32_t pm = ((H | 0x88888888u) - kk) & ~s7 & 0x88888888u;
            o[w] = pm ? (uint16_t)((X[H & 255] << 4) | X[(H >> 8) & 255] | (((X[(H >> 16) & 255] << 4) | X[H >> 24]) << 8)) : 0;
            any |= pm;
        }
    }
    return any != 0;
}

// initial state: every pixel at rest on its target level (rails settled), long dwell
void pqbn_v4_init(pqbn_v4_ctx *C, uint32_t now) {
    const int W = C->W, H = C->H;
    const int ts = (int)((now - 256) & 1023);
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
        const int t4 = (C->fb4[(size_t)y * (W >> 1) + (x >> 1)] >> ((x & 1) << 2)) & 15;
        int h; uint16_t s;
        if (t4 == 15 || t4 == 0) { h = t4 ? 0xF : 0x7; s = (uint16_t)((ts & 255) | ((ts >> 8) << 13) | 0x8000); }
        else { h = 0; s = (uint16_t)(C->level[t4] | (7 << 13)); }
        uint8_t *hb = C->hot + (size_t)y * (W >> 1) + (x >> 1);
        *hb = (uint8_t)((*hb & ~(15 << ((x & 1) << 2))) | (h << ((x & 1) << 2)));
        C->cold[(size_t)y * W + x] = s;
    }
    for (int i = 0; i < C->ntx * C->nty; i++) { C->tile_plan[i] = (uint16_t)(now - 8192); C->tile_nmax[i] = 0; C->tile_dirty[i] = 0; }
    C->stat_planned = 0; C->stat_swung = 0;
    for (int ty = 0; ty < C->nty; ty++) { C->band[2 * ty] = (uint16_t)now; C->band[2 * ty + 1] = 0; }
}
