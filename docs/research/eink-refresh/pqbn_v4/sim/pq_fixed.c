// Physical-Quantizer e-ink driver, integer-only reference implementation.
//
// One raster pass per field does quantization + drive together:
//   for each pixel: d = input + diffused error
//                   pick the reachable outcome v (stay, or n fields up/down) minimising
//                       (d - v)^2 + MU * n
//                   emit the first field of that plan, diffuse e = d - v
//
// Per-pixel state: 16 bits
//   ph   uint8   model estimate of optical state, 0 = black rail, 255 = white rail
//   meta uint8   [2:0] dwell bucket (~log2 of fields since last drive)
//                [3]   rail margin already applied
//                [7:4] DC balance, signed 4-bit (sum of u, saturating)
// Plus two int16 error line buffers (serpentine Floyd-Steinberg).
//
// Panel model (calibrated per panel type & temperature, not per pixel):
//   D        = 255 / T0           movement per field
//   D1[b]    = D * stiction(b)    movement of the first field after a rest of bucket b
#include <stdint.h>
#include <string.h>

typedef struct {
    int D;          // per-field step (Q8)
    int D1[8];      // first-field step by dwell bucket (Q8)
    int MU;         // drive cost per field, in Q8^2 units
    int maxn;       // planning horizon (fields)
    int marg;       // apply rail margin only while s*dc < marg
    int rails_only; // settle mode: only rail outcomes allowed
} pq_params;

static inline int clamp255(int v) { return v < 0 ? 0 : (v > 255 ? 255 : v); }
static inline int dc_get(uint8_t m) { return ((int8_t)m) >> 4; }               // signed [7:4]
static inline uint8_t dc_set(uint8_t m, int dc) {
    if (dc > 7) dc = 7; if (dc < -8) dc = -8;
    return (uint8_t)((m & 0x0f) | ((dc & 0xf) << 4));
}

// I: input luminance 0..255 (w*h). u: output drive per pixel (-1,0,+1).
// err: 2*(w+2) int16 scratch. field: global field counter (for dwell buckets).
void pq_field_q8(const uint8_t *I, uint8_t *ph, uint8_t *meta, int8_t *u,
                 int w, int h, const pq_params *P, uint32_t field, int16_t *err) {
    memset(err, 0, sizeof(int16_t) * 2 * (w + 2));
    int16_t *cur = err + 1, *nxt = err + (w + 2) + 1;
    const int D = P->D, MU = P->MU;
    for (int y = 0; y < h; y++) {
        const int dir = (y & 1) ? -1 : 1;
        int x = (dir > 0) ? 0 : w - 1;
        for (int i = 0; i < w; i++, x += dir) {
            const int idx = y * w + x;
            int d = (int)I[idx] + cur[x];
            int q = ph[idx];
            uint8_t m = meta[idx];
            int b = m & 7, dc = dc_get(m);
            const int D1 = b ? P->D1[b] : D;

            int bv = q, bn = 0, bs = 0;
            int bc = (P->rails_only && q > 0 && q < 255) ? 0x7fffffff : (d - q) * (d - q);
            for (int s = -1; s <= 1; s += 2) {
                int v = q, prev_c = 0x7fffffff;
                for (int n = 1; n <= P->maxn; n++) {
                    v = clamp255(v + s * (n == 1 ? D1 : D));
                    int rail = (v == 0 || v == 255);
                    if (!(P->rails_only && !rail)) {
                        int c = (d - v) * (d - v) + MU * n;
                        if (c < bc) { bc = c; bv = v; bn = n; bs = s; }
                        if (c > prev_c) break;          // cost is convex in n: stop once rising
                        prev_c = c;
                    }
                    if (rail) break;
                }
            }

            int uu = 0;
            if (bn) {
                uu = bs;
                ph[idx] = (uint8_t)clamp255(q + bs * D1);
                m = (uint8_t)(m & 0xf0);                  // dwell=0, margin flag cleared
            } else if ((q == 255 || q == 0) && !(m & 8) && (q == 255 ? dc : -dc) < P->marg) {
                uu = (q == 255) ? 1 : -1;                // one margin field at the rail
                m = (uint8_t)((m & 0xf0) | 8);
            } else if (b < 7 && (field & ((2u << b) - 1)) == 0) {
                m = (uint8_t)(m + 1);                     // dwell bucket b reached after ~2^b fields
            }
            if (uu) m = dc_set(m, dc + uu);
            meta[idx] = m;
            u[idx] = (int8_t)uu;

            int e = d - bv;
            if (e > 153) e = 153; if (e < -153) e = -153;  // +-0.6
            int e7 = (e * 7 + 8) >> 4, e3 = (e * 3 + 8) >> 4, e5 = (e * 5 + 8) >> 4;
            cur[x + dir] += (int16_t)e7;
            nxt[x - dir] += (int16_t)e3;
            nxt[x]       += (int16_t)e5;
            nxt[x + dir] += (int16_t)(e - e7 - e3 - e5);   // remainder: error is conserved exactly
        }
        int16_t *t = cur; cur = nxt; nxt = t;
        memset(nxt - 1, 0, sizeof(int16_t) * (w + 2));
    }
}

// Integrator driver (zero-cost upgrade for PaperBoy-style drivers).
// pos (3 bits) is the drive-time integral; target level from the frame buffer.
// No change detection, no reset: sum(u) = pos - pos0, so |DC| <= N for ever.
void int_field(const uint8_t *tgt_pos, uint8_t *pos, int8_t *u, int n) {
    for (int i = 0; i < n; i++) {
        int dd = (int)tgt_pos[i] - (int)pos[i];
        int s = (dd > 0) - (dd < 0);
        u[i] = (int8_t)s;
        pos[i] = (uint8_t)(pos[i] + s);
    }
}

void pq_init_params(pq_params *P, float T0, float s0, float taud, float mu, int maxn, int marg) {
    extern double exp(double);
    P->D = (int)(255.0f / T0 + 0.5f);
    for (int b = 0; b < 8; b++) {
        float dwell = (b == 7) ? 512.0f : 1.5f * (float)(1 << b);  // bucket b ~ 2^b..2^(b+1) fields of rest
        float st = 1.0f - s0 * (1.0f - (float)exp(-dwell / taud));
        P->D1[b] = (int)(P->D * st + 0.5f);
    }
    P->D1[0] = P->D;                                        // bucket 0 = moved last field: no stiction
    P->MU = (int)(mu * (255.0f / T0) * 255.0f + 0.5f);     // mu * D, in Q8^2
    P->maxn = maxn; P->marg = marg; P->rails_only = 0;
}

// ---------------------------------------------------------------------------
// Fast closed-form version (same decisions as pq_field_q8 in normal mode).
// Only the direction toward d can win, and for n >= 1 the cost
//     (a - D1 - (n-1)D)^2 + MU*n
// is a parabola in n, so the optimum is the rounded vertex, clamped to the rail.
// "Stay" wins iff a <= thr[b] = D1/2 + MU/(2*D1): one compare, no multiply.
// ---------------------------------------------------------------------------
typedef struct { int thr[8]; int invD; int half_mu_over_D; } pq_fast_tab;

void pq_fast_init(const pq_params *P, pq_fast_tab *T) {
    for (int b = 0; b < 8; b++) {
        int D1 = b ? P->D1[b] : P->D;
        T->thr[b] = (D1 * D1 + P->MU) / (2 * D1);
    }
    T->invD = (1 << 16) / P->D;
    T->half_mu_over_D = P->MU / (2 * P->D);
}

void pq_field_q8_fast(const uint8_t *I, uint8_t *ph, uint8_t *meta, int8_t *u,
                      int w, int h, const pq_params *P, const pq_fast_tab *T,
                      uint32_t field, int16_t *err) {
    if (P->rails_only) { pq_field_q8(I, ph, meta, u, w, h, P, field, err); return; }
    memset(err, 0, sizeof(int16_t) * 2 * (w + 2));
    int16_t *cur = err + 1, *nxt = err + (w + 2) + 1;
    const int D = P->D, MU = P->MU, invD = T->invD, hmd = T->half_mu_over_D;
    for (int y = 0; y < h; y++) {
        const int dir = (y & 1) ? -1 : 1;
        int x = (dir > 0) ? 0 : w - 1;
        for (int i = 0; i < w; i++, x += dir) {
            const int idx = y * w + x;
            const int d = (int)I[idx] + cur[x];
            const int q = ph[idx];
            uint8_t m = meta[idx];
            const int b = m & 7;
            const int s = d > q ? 1 : -1;
            const int a = s * (d - q);
            int v = q, uu = 0;
            const int D1 = b ? P->D1[b] : D;
            const int r = s > 0 ? 255 - q : q;         // distance to the rail
            if (a > T->thr[b] || r < D1) {             // a move may beat staying
                int nr = r <= D1 ? 1 : 1 + (((r - D1) * invD + 65535) >> 16);
                int t = a - hmd - D1 + (D >> 1);
                int n = t <= 0 ? 1 : 1 + ((t * invD) >> 16);
                if (n > nr) n = nr;
                if (n > P->maxn) n = P->maxn;
                int disp = D1 + (n - 1) * D; if (disp > r) disp = r;
                int c = (a - disp) * (a - disp) + MU * n;
                if (c < a * a) {
                    v = q + s * disp; uu = s;
                    ph[idx] = (uint8_t)clamp255(q + s * D1);
                    m = (uint8_t)(m & 0xf0);
                }
            }
            if (!uu) {
                const int dc = dc_get(m);
                if ((q == 255 || q == 0) && !(m & 8) && (q == 255 ? dc : -dc) < P->marg) {
                    uu = (q == 255) ? 1 : -1;
                    m = (uint8_t)((m & 0xf0) | 8);
                } else if (b < 7 && (field & ((2u << b) - 1)) == 0) {
                    m = (uint8_t)(m + 1);
                }
            }
            if (uu) m = dc_set(m, dc_get(m) + uu);
            meta[idx] = m;
            u[idx] = (int8_t)uu;
            int e = d - v;
            if (e > 153) e = 153; if (e < -153) e = -153;
            int e7 = (e * 7 + 8) >> 4, e3 = (e * 3 + 8) >> 4, e5 = (e * 5 + 8) >> 4;
            cur[x + dir] += (int16_t)e7;
            nxt[x - dir] += (int16_t)e3;
            nxt[x]       += (int16_t)e5;
            nxt[x + dir] += (int16_t)(e - e7 - e3 - e5);
        }
        int16_t *tt = cur; cur = nxt; nxt = tt;
        memset(nxt - 1, 0, sizeof(int16_t) * (w + 2));
    }
}

// ---------------------------------------------------------------------------
// Branchless variant of pq_field_q8_fast: the stay/move choice in dithered
// regions is data-random, so compute both and select (cmov) instead of branching.
// Rare paths (rail margin, dwell bucket tick) stay as predictable branches.
// ---------------------------------------------------------------------------
void pq_field_q8_bl(const uint8_t *I, uint8_t *ph, uint8_t *meta, int8_t *u,
                    int w, int h, const pq_params *P, const pq_fast_tab *T,
                    uint32_t field, int16_t *err) {
    if (P->rails_only) { pq_field_q8(I, ph, meta, u, w, h, P, field, err); return; }
    memset(err, 0, sizeof(int16_t) * 2 * (w + 2));
    int16_t *cur = err + 1, *nxt = err + (w + 2) + 1;
    const int D = P->D, MU = P->MU, invD = T->invD, hmd = T->half_mu_over_D, maxn = P->maxn;
    for (int y = 0; y < h; y++) {
        const int dir = (y & 1) ? -1 : 1;
        int x = (dir > 0) ? 0 : w - 1;
        for (int i = 0; i < w; i++, x += dir) {
            const int idx = y * w + x;
            const int d = (int)I[idx] + cur[x];
            const int q = ph[idx];
            uint8_t m = meta[idx];
            const int b = m & 7;
            const int up = d > q;
            const int s = up ? 1 : -1;
            const int a = up ? d - q : q - d;
            const int D1 = P->D1[b];                    // D1[0] == D
            const int r = up ? 255 - q : q;
            int t = a - hmd - D1 + (D >> 1);
            t = t < 0 ? 0 : t;
            int n = 1 + ((t * invD) >> 16);
            int nr = 1 + (((r - D1 > 0 ? r - D1 : 0) * invD + 65535) >> 16);
            n = n < nr ? n : nr;
            n = n < maxn ? n : maxn;
            int disp = D1 + (n - 1) * D; disp = disp < r ? disp : r;
            const int c = (a - disp) * (a - disp) + MU * n;
            const int mv = c < a * a;
            const int uu0 = mv ? s : 0;
            const int v = mv ? q + s * disp : q;
            int d1c = D1 < r ? D1 : r;
            ph[idx] = (uint8_t)(mv ? q + s * d1c : q);
            int uu = uu0;
            m = mv ? (uint8_t)(m & 0xf0) : m;
            if (!mv) {
                if ((uint8_t)(q + 1) <= 1 && !(m & 8)) {     // q == 255 or q == 0, margin pending
                    const int dc = dc_get(m), sr = q ? 1 : -1;
                    if (sr * dc < P->marg) { uu = sr; m = (uint8_t)((m & 0xf0) | 8); }
                } else if (b < 7 && (field & ((2u << b) - 1)) == 0) {
                    m = (uint8_t)(m + 1);
                }
            }
            if (uu) m = dc_set(m, dc_get(m) + uu);
            meta[idx] = m;
            u[idx] = (int8_t)uu;
            int e = d - v;
            e = e > 153 ? 153 : (e < -153 ? -153 : e);
            const int e7 = (e * 7 + 8) >> 4, e3 = (e * 3 + 8) >> 4, e5 = (e * 5 + 8) >> 4;
            cur[x + dir] += (int16_t)e7;
            nxt[x - dir] += (int16_t)e3;
            nxt[x]       += (int16_t)e5;
            nxt[x + dir] += (int16_t)(e - e7 - e3 - e5);
        }
        int16_t *tt = cur; cur = nxt; nxt = tt;
        memset(nxt - 1, 0, sizeof(int16_t) * (w + 2));
    }
}

// ---------------------------------------------------------------------------
// Wavefront variant: left-to-right FS, 4 rows processed together with a 2-pixel
// stagger so the 4 error chains are independent (instruction-level parallelism).
// Decisions are identical to a plain left-to-right raster pass.
// E: (h+1) * (w+2) int16 scratch (error plane).
// ---------------------------------------------------------------------------
static inline void pq_px(int d, uint8_t *php, uint8_t *mp, int8_t *up, int *ev,
                         const pq_params *P, const pq_fast_tab *T, uint32_t field) {
    const int D = P->D, MU = P->MU, invD = T->invD, hmd = T->half_mu_over_D, maxn = P->maxn;
    const int q = *php;
    uint8_t m = *mp;
    const int b = m & 7;
    const int upd = d > q;
    const int s = upd ? 1 : -1;
    const int a = upd ? d - q : q - d;
    const int D1 = P->D1[b];
    const int r = upd ? 255 - q : q;
    int t = a - hmd - D1 + (D >> 1); t = t < 0 ? 0 : t;
    int n = 1 + ((t * invD) >> 16);
    int nr = 1 + (((r - D1 > 0 ? r - D1 : 0) * invD + 65535) >> 16);
    n = n < nr ? n : nr; n = n < maxn ? n : maxn;
    int disp = D1 + (n - 1) * D; disp = disp < r ? disp : r;
    const int c = (a - disp) * (a - disp) + MU * n;
    const int mv = c < a * a;
    const int v = mv ? q + s * disp : q;
    const int d1c = D1 < r ? D1 : r;
    *php = (uint8_t)(mv ? q + s * d1c : q);
    int uu = mv ? s : 0;
    m = mv ? (uint8_t)(m & 0xf0) : m;
    if (!mv) {
        if ((uint8_t)(q + 1) <= 1 && !(m & 8)) {
            const int dc = dc_get(m), sr = q ? 1 : -1;
            if (sr * dc < P->marg) { uu = sr; m = (uint8_t)((m & 0xf0) | 8); }
        } else if (b < 7 && (field & ((2u << b) - 1)) == 0) {
            m = (uint8_t)(m + 1);
        }
    }
    if (uu) m = dc_set(m, dc_get(m) + uu);
    *mp = m; *up = (int8_t)uu;
    int e = d - v;
    *ev = e > 153 ? 153 : (e < -153 ? -153 : e);
}

void pq_field_q8_wf(const uint8_t *I, uint8_t *ph, uint8_t *meta, int8_t *u,
                    int w, int h, const pq_params *P, const pq_fast_tab *T,
                    uint32_t field, int16_t *E) {
    if (P->rails_only) { pq_field_q8(I, ph, meta, u, w, h, P, field, E); return; }
    const int W2 = w + 2, LAG = 2, R = 4;
    memset(E, 0, sizeof(int16_t) * (size_t)(h + 1) * W2);
    for (int y0 = 0; y0 < h; y0 += R) {
        const int rows = (h - y0) < R ? (h - y0) : R;
        for (int tt = 0; tt < w + LAG * (rows - 1); tt++) {
            for (int k = 0; k < rows; k++) {
                const int x = tt - k * LAG;
                if (x < 0 || x >= w) continue;
                const int y = y0 + k, idx = y * w + x;
                int16_t *er = E + (size_t)y * W2 + 1, *en = er + W2;
                int e;
                pq_px((int)I[idx] + er[x], ph + idx, meta + idx, u + idx, &e, P, T, field);
                const int e7 = (e * 7 + 8) >> 4, e3 = (e * 3 + 8) >> 4, e5 = (e * 5 + 8) >> 4;
                er[x + 1] += (int16_t)e7;
                en[x - 1] += (int16_t)e3;
                en[x]     += (int16_t)e5;
                en[x + 1] += (int16_t)(e - e7 - e3 - e5);
            }
        }
    }
}
