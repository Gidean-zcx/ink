// E-ink drive / quantization kernels used by the simulator (float reference versions).
// Build: gcc -O2 -shared -fPIC -o libkernels.so kernels.c
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

static inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

// ---------------------------------------------------------------------------
// Quantizers
// ---------------------------------------------------------------------------

// Classic serpentine Floyd-Steinberg, binary, independent per frame.
void fs_binary(const float *I, uint8_t *out, int w, int h, float *err /* 2*(w+2) */) {
    memset(err, 0, sizeof(float) * 2 * (w + 2));
    float *cur = err + 1, *nxt = err + (w + 2) + 1;
    for (int y = 0; y < h; y++) {
        int dir = (y & 1) ? -1 : 1;
        int x0 = (dir > 0) ? 0 : w - 1;
        for (int i = 0; i < w; i++) {
            int x = x0 + dir * i;
            float d = I[y * w + x] + cur[x];
            uint8_t b = d > 0.5f;
            float e = clampf(d - (float)b, -0.6f, 0.6f);
            out[y * w + x] = b;
            cur[x + dir] += e * (7.f / 16);
            nxt[x - dir] += e * (3.f / 16);
            nxt[x] += e * (5.f / 16);
            nxt[x + dir] += e * (1.f / 16);
        }
        float *t = cur; cur = nxt; nxt = t;
        memset(nxt - 1, 0, sizeof(float) * (w + 2));
    }
}

// Hysteretic multi-level error diffusion.
// Each pixel picks level l (of nlev uniformly spaced in [0,1]) minimising
//      (d - l)^2 + mu * |l - l_prev|
// where d = input + diffused error, l_prev = level chosen for this pixel in
// the previous frame. mu = 0 reduces to ordinary multi-level FS.
// The |l - l_prev| term is the number of drive fields the move costs, so this
// is the exact greedy minimiser of "perceptual error + lambda * drive energy".
void hed(const float *I, const uint8_t *prev, uint8_t *out, int w, int h,
         int nlev, float mu, float *err /* 2*(w+2) */) {
    memset(err, 0, sizeof(float) * 2 * (w + 2));
    float *cur = err + 1, *nxt = err + (w + 2) + 1;
    const float step = 1.0f / (float)(nlev - 1);
    for (int y = 0; y < h; y++) {
        int dir = (y & 1) ? -1 : 1;
        int x0 = (dir > 0) ? 0 : w - 1;
        for (int i = 0; i < w; i++) {
            int x = x0 + dir * i;
            int idx = y * w + x;
            float d = I[idx] + cur[x];
            float lp = prev[idx] * step;
            int best = 0; float bc = 1e30f;
            for (int j = 0; j < nlev; j++) {
                float l = j * step;
                float c = (d - l) * (d - l) + mu * fabsf(l - lp);
                if (c < bc) { bc = c; best = j; }
            }
            out[idx] = (uint8_t)best;
            float e = clampf(d - best * step, -0.6f, 0.6f);
            cur[x + dir] += e * (7.f / 16);
            nxt[x - dir] += e * (3.f / 16);
            nxt[x] += e * (5.f / 16);
            nxt[x + dir] += e * (1.f / 16);
        }
        float *t = cur; cur = nxt; nxt = t;
        memset(nxt - 1, 0, sizeof(float) * (w + 2));
    }
}

// ---------------------------------------------------------------------------
// Physical quantizer (PQ): quantization + drive in one raster pass per field.
// Per pixel the controller keeps a model estimate ph (optical state, [0,1]),
// dw (fields since last drive) and sat (rail margin applied: -1/0/+1).
// Candidate outcomes are the states the pixel can actually reach from ph:
//   stay, or n fields up/down, where the first field after a rest moves only
//   st(dw)*D (stiction model) and later fields move D = 1/T0 (calibrated nominal).
// Pick the candidate minimising  (d - v)^2 + mu * n * D   (greedy MPC step),
// emit the first field of that plan, and diffuse e = d - v (serpentine FS).
// ---------------------------------------------------------------------------
void pq_field(const float *I, float *ph, float *dw, int8_t *sat, int8_t *u, int8_t *dc,
              int w, int h, float T0c, float s0c, float taudc, float mu, int maxn, int dcmax,
              float lam, int marg, int rails_only, float *err /* 2*(w+2) */) {
    memset(err, 0, sizeof(float) * 2 * (w + 2));
    float *cur = err + 1, *nxt = err + (w + 2) + 1;
    const float D = 1.0f / T0c;
    for (int y = 0; y < h; y++) {
        int dir = (y & 1) ? -1 : 1;
        int x0 = (dir > 0) ? 0 : w - 1;
        for (int i = 0; i < w; i++) {
            int x = x0 + dir * i;
            int idx = y * w + x;
            float d = I[idx] + cur[x];
            float q = clampf(ph[idx], 0.f, 1.f);
            float st = dw[idx] > 0 ? 1.f - s0c * (1.f - expf(-dw[idx] / taudc)) : 1.f;
            float D1 = D * st;
            // stay (in rails-only mode a mid-grey pixel may not stay)
            float bv = q, bc = (d - q) * (d - q); int bn = 0, bs = 0;
            if (rails_only && q > 0.f && q < 1.f) bc = 1e30f;
            for (int s = -1; s <= 1; s += 2) {
                float v = q;
                // DC guard: never start a move that pushes |sum u| beyond dcmax
                if (s * dc[idx] >= dcmax) continue;
                for (int n = 1; n <= maxn; n++) {
                    v = clampf(v + s * (n == 1 ? D1 : D), 0.f, 1.f);
                    if (rails_only && v > 0.f && v < 1.f) continue;
                    float nd = (float)(dc[idx] + s * n);
                    float c = (d - v) * (d - v) + mu * n * D + lam * D * D * (nd * nd - (float)dc[idx] * dc[idx]);
                    if (c < bc) { bc = c; bv = v; bn = n; bs = s; }
                    if (v <= 0.f || v >= 1.f) break;
                }
            }
            if (bn > 0) {
                u[idx] = (int8_t)bs; dc[idx] += bs;
                ph[idx] = clampf(q + bs * D1, 0.f, 1.f);
                dw[idx] = 0.f; sat[idx] = 0;
            } else if ((q >= 1.f || q <= 0.f) && sat[idx] == 0 && (q >= 1.f ? 1 : -1) * dc[idx] < marg) {
                // just arrived at a rail: one margin field so slow pixels saturate too
                int s = q >= 1.f ? 1 : -1;
                u[idx] = (int8_t)s; sat[idx] = (int8_t)s; dw[idx] = 0.f; dc[idx] += s;
            } else {
                u[idx] = 0; dw[idx] += 1.f;
            }
            float e = clampf(d - bv, -0.6f, 0.6f);
            cur[x + dir] += e * (7.f / 16);
            nxt[x - dir] += e * (3.f / 16);
            nxt[x] += e * (5.f / 16);
            nxt[x + dir] += e * (1.f / 16);
        }
        float *t = cur; cur = nxt; nxt = t;
        memset(nxt - 1, 0, sizeof(float) * (w + 2));
    }
}

// ---------------------------------------------------------------------------
// Drivers: produce u in {-1,0,+1} per pixel for one field.
// ---------------------------------------------------------------------------

// PaperBoy-style (msg.c): per-pixel counter + colour bit. On colour change the
// counter resets and the pixel is driven N fields toward the new colour.
void drive_counter(const uint8_t *tgt, uint8_t *cnt, uint8_t *color, int8_t *u, int n, int N) {
    for (int i = 0; i < n; i++) {
        if (tgt[i] != color[i]) { color[i] = tgt[i]; cnt[i] = 0; }
        if (cnt[i] < N) { u[i] = color[i] ? 1 : -1; cnt[i]++; }
        else u[i] = 0;
    }
}

// Proposed: position integrator. pos in [0,N] is the drive-time integral,
// i.e. the estimated particle position. No change detection, no reset.
// sum(u) == pos - pos0, so |DC| <= N always (DC-balanced by construction),
// and any target level 0..N is reachable (partial drive greys).
void drive_integrator(const uint8_t *tgt_pos, uint8_t *pos, int8_t *u, int n) {
    for (int i = 0; i < n; i++) {
        int d = (int)tgt_pos[i] - (int)pos[i];
        int s = (d > 0) - (d < 0);
        u[i] = (int8_t)s;
        pos[i] = (uint8_t)(pos[i] + s);
    }
}

// ---------------------------------------------------------------------------
// PQ-BN: parallel physical quantizer. No error diffusion: each pixel dithers
// between the two reachable outcomes that bracket its target, using a fixed
// screen-space blue-noise threshold tau(x,y). Pixels are independent, so the
// kernel can be vectorised and only pixels whose input or state changed need it.
//   outcomes in direction s: v_0 = q, v_n = clamp(q + s*(D1 + (n-1)*D))
//   [lo, hi] = adjacent outcomes with lo <= d <= hi (absolute values)
//   pick hi iff (d - lo) > tau * (hi - lo)   (+-hyst toward fewer fields)
// ---------------------------------------------------------------------------
void pqbn_field(const float *I, float *ph, float *dw, int8_t *sat, int8_t *u, int8_t *dc,
                const float *tau, int w, int h, float T0c, float s0c, float taudc,
                float hyst, int maxn, int marg, float lam) {
    const float D = 1.0f / T0c;
    for (int idx = 0; idx < w * h; idx++) {
        float d = clampf(I[idx], 0.f, 1.f);
        float q = clampf(ph[idx], 0.f, 1.f);
        float st = dw[idx] > 0 ? 1.f - s0c * (1.f - expf(-dw[idx] / taudc)) : 1.f;
        float D1 = D * st;
        int s = d > q ? 1 : -1;
        int nbest = 0;
        if (d != q) {
            // walk outcomes toward d until we pass it (or hit the rail)
            float vprev = q, v = q; int n = 0;
            while (n < maxn) {
                float nv = clampf(v + s * (n == 0 ? D1 : D), 0.f, 1.f);
                if (nv > 0.99f) nv = 1.f;                         // float drift: treat as the rail
                if (nv < 0.01f) nv = 0.f;
                if (nv == v) break;                    // rail
                vprev = v; v = nv; n++;
                if ((s > 0 && v >= d) || (s < 0 && v <= d)) break;
            }
            // bracket: vprev (n-1 fields) and v (n fields); if never passed d, v is the best
            if ((s > 0 && v < d) || (s < 0 && v > d)) nbest = n;
            else {
                float lo = s > 0 ? vprev : v, hi = s > 0 ? v : vprev;
                float frac = (d - lo) / (hi - lo + 1e-9f);
                float t = tau[idx];
                // the option needing more fields is hi when s>0, lo when s<0: make it harder by
                // hyst, and by lam*dc when that extra field would grow the DC imbalance
                float pen = hyst + (lam > 0.f ? lam : 0.f) * (float)(dc[idx] * s);
                int pick_hi = s > 0 ? (frac > t + pen) : (frac > t - pen);
                int far = (s > 0) ? pick_hi : !pick_hi;    // chose the n-field outcome?
                nbest = far ? n : n - 1;
            }
        }
        if (nbest > 0) {
            u[idx] = (int8_t)s; dc[idx] += s;
            float np_ = clampf(q + s * D1, 0.f, 1.f);
            if (np_ > 0.99f) np_ = 1.f;
            if (np_ < 0.01f) np_ = 0.f;
            ph[idx] = np_;
            dw[idx] = 0.f; sat[idx] = 0;
        } else if ((q >= 1.f || q <= 0.f) && sat[idx] == 0 && (q >= 1.f ? 1 : -1) * dc[idx] < marg) {
            int sr = q >= 1.f ? 1 : -1;
            u[idx] = (int8_t)sr; sat[idx] = (int8_t)sr; dw[idx] = 0.f; dc[idx] += sr;
        } else if (lam < 0.f && ((q >= 1.f && dc[idx] < 0) || (q <= 0.f && dc[idx] > 0))) {
            // rail rebalancing (lam < 0 enables): pushing further into a saturated rail does not
            // move the pixel but cancels opposite-sign DC. One field per phase, optically free.
            int sr = q >= 1.f ? 1 : -1;
            u[idx] = (int8_t)sr; dc[idx] += sr;            // dwell keeps counting: no particle motion
            dw[idx] += 1.f;
        } else {
            u[idx] = 0; dw[idx] += 1.f;
        }
    }
}
