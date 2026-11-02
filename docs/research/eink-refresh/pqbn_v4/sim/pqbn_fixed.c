// PQ-BN: parallel physical quantizer, integer-only, branch-light.
// Every pixel is independent (no error diffusion), so the loop vectorises and can be
// restricted to pixels/rows whose input or state changed.
//
// Per pixel state (3 bytes, can be packed to 2):
//   ph    uint8  model optical state, 0 = black rail, 255 = white rail
//   dwb   uint8  [2:0] dwell bucket (~log2 fields since last push), [3] rail margin applied
//   dc    int8   running sum of pushes (DC balance)
// Inputs per phase: I (target luminance 0..255), tau (blue-noise threshold 0..255, screen fixed)
// Output: u in {-1,0,+1}
//
// Decision: outcomes reachable toward the target are q, q+s*D1, q+s*(D1+D), ...
// Take the two that bracket the target and pick the farther one iff the target's
// position inside the bracket exceeds tau (mirrored for downward moves).
#include <stdint.h>
#include <string.h>
#include <math.h>

typedef struct {
    int D;          // Q8 step per push (255/T0)
    int D1[8];      // first push after rest, by dwell bucket (D1[0] == D)
    int invD;       // (1<<16)/D
    int marg;       // rail margin only while s*dc < marg
} pqbn_params;

void pqbn_init(pqbn_params *P, float T0, float s0, float taud, int marg) {
    P->D = (int)(255.0f / T0 + 0.5f);
    for (int b = 0; b < 8; b++) {
        float dwell = (b == 7) ? 512.0f : 1.5f * (float)(1 << b);
        float st = 1.0f - s0 * (1.0f - expf(-dwell / taud));
        P->D1[b] = (int)(P->D * st + 0.5f);
    }
    P->D1[0] = P->D;
    P->invD = (1 << 16) / P->D;
    P->marg = marg;
}

static inline int imin(int a, int b) { return a < b ? a : b; }

// n pixels, contiguous. tau is the per-pixel threshold row (caller tiles the 64x64 mask).
void pqbn_q8(const uint8_t *I, const uint8_t *tau, uint8_t *ph, uint8_t *dwb, int8_t *dc,
             int8_t *u, int n, const pqbn_params *P, uint32_t field) {
    const int D = P->D, invD = P->invD;
    for (int i = 0; i < n; i++) {
        const int q = ph[i], d = I[i];
        const int m = dwb[i], b = m & 7;
        const int up = d > q;
        const int s = up ? 1 : -1;
        const int a = up ? d - q : q - d;                 // distance to target
        const int r = up ? 255 - q : q;                   // distance to rail
        const int D1 = P->D1[b];
        // pushes needed to reach or pass the target: 1 if a <= D1, else 1 + ceil((a-D1)/D)
        const int ex = a - D1;
        const int npass = ex <= 0 ? 1 : 1 + ((ex * invD + 65535) >> 16);
        int hi = D1 + (npass - 1) * D;                    // displacement of the far outcome
        int lo = npass > 1 ? hi - D : 0;                  // displacement of the near outcome
        hi = imin(hi, r);
        if (hi > r - 3) hi = r;                           // within ~1% of the rail: rail
        const int span = hi - lo;
        const int t = up ? tau[i] : 255 - tau[i];
        // far iff (a - lo)/span > t/255; a == 0 -> stay
        const int far = (a > 0) & (span > 0) & ((a - lo) * 255 > t * span);
        const int nchoose = far ? npass : npass - 1;
        int uu = 0, mm = m, dd = dc[i];
        if (nchoose > 0) {
            int step = imin(D1, r);
            int np = q + s * step;
            if (np > 252) np = 255; if (np < 3) np = 0;
            ph[i] = (uint8_t)np;
            uu = s; mm = 0;                               // dwell 0, margin flag cleared
        } else if (q == 255 || q == 0) {
            const int sr = q ? 1 : -1;
            if (!(m & 8) && sr * dd < P->marg) { uu = sr; mm = (m & 7) | 8; }     // arrival margin
            else if (sr * dd < 0) { uu = sr; }                                  // rail rebalancing
            else if (b < 7 && (field & ((2u << b) - 1)) == 0) mm = m + 1;
        } else if (b < 7 && (field & ((2u << b) - 1)) == 0) {
            mm = m + 1;
        }
        if (uu) { dd += uu; if (dd > 127) dd = 127; if (dd < -127) dd = -127; }
        dwb[i] = (uint8_t)mm; dc[i] = (int8_t)dd; u[i] = (int8_t)uu;
    }
}
