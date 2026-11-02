// PQ-BN as a single 64K-entry lookup table: the whole physical-quantizer decision per pixel
// per phase becomes "build a 16-bit index, read one byte".
//
// index = target4[15:12] | q6[11:6] | tau4[5:2] | stc[1:0]
//   target4  4bpp target gray from the framebuffer (0 black .. 15 white)
//   q6       model optical state, 6 bits (0 black rail .. 63 white rail)
//   tau4     blue-noise threshold, 4 bits (screen-fixed mask)
//   stc      stiction class (0 moved last phase, 1 short rest, 2 medium, 3 long)
// entry    = drive[7:6] (0 none, 1 black, 2 white) | new q6[5:0]
//
// Per-pixel state byte: q6 | stc<<6.  DC (int8) and the rail-margin flag live in a second byte;
// they only matter at the rails (rare branch).
#include <stdint.h>
#include <string.h>
#include <math.h>

static inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

// Build with the same decision rule as pqbn_q8, evaluated on the quantized grid.
void pqbn_lut_build(uint8_t *lut /* 65536 */, float T0, float s0, float taud) {
    const float D = 1.0f / T0;
    const float dwell_of_class[4] = {0.f, 3.f, 24.f, 256.f};
    for (int t4 = 0; t4 < 16; t4++)
    for (int q6 = 0; q6 < 64; q6++)
    for (int a4 = 0; a4 < 16; a4++)
    for (int c = 0; c < 4; c++) {
        const float d = t4 / 15.0f, q = q6 / 63.0f, tau = (a4 + 0.5f) / 16.0f;
        const float st = c ? 1.f - s0 * (1.f - expf(-dwell_of_class[c] / taud)) : 1.f;
        const float D1 = D * st;
        int s = d > q ? 1 : -1, n = 0, nbest = 0;
        if (fabsf(d - q) > 1e-6f) {
            float v = q, vprev = q;
            while (n < 32) {
                float nv = clampf(v + s * (n == 0 ? D1 : D), 0.f, 1.f);
                if (nv > 0.99f) nv = 1.f; if (nv < 0.01f) nv = 0.f;
                if (nv == v) break;
                vprev = v; v = nv; n++;
                if ((s > 0 && v >= d) || (s < 0 && v <= d)) break;
            }
            if ((s > 0 && v < d) || (s < 0 && v > d)) nbest = n;
            else {
                float lo = s > 0 ? vprev : v, hi = s > 0 ? v : vprev;
                float frac = (d - lo) / (hi - lo + 1e-9f);
                int pick_hi = frac > tau;
                nbest = ((s > 0) ? pick_hi : !pick_hi) ? n : n - 1;
            }
        }
        uint8_t e;
        if (nbest > 0) {
            float np = clampf(q + s * D1, 0.f, 1.f);
            if (np > 0.99f) np = 1.f; if (np < 0.01f) np = 0.f;
            int nq = (int)lrintf(np * 63.0f);
            if (nq == q6) nq += s;                       // a push must move the model state
            if (nq < 0) nq = 0; if (nq > 63) nq = 63;
            e = (uint8_t)(((s > 0 ? 2 : 1) << 6) | nq);
        } else e = (uint8_t)q6;
        lut[(t4 << 12) | (q6 << 6) | (a4 << 2) | c] = e;
    }
}

// One phase over n pixels. tgt4: one target nibble per byte (unpacked for clarity),
// tau4: per-pixel 4-bit threshold, st: q6|stc<<6, dcm: dc (bits 6:0 signed via int8) ,
// mflag: rail margin applied. Returns pushes issued.
int pqbn_lut_phase(const uint8_t *lut, const uint8_t *tgt4, const uint8_t *tau4, uint8_t *st,
                   int8_t *dc, uint8_t *mflag, int8_t *u, int n, uint32_t field, int marg) {
    int pushes = 0;
    for (int i = 0; i < n; i++) {
        const uint8_t s = st[i];
        const int q6 = s & 63, c = s >> 6;
        const uint8_t e = lut[(tgt4[i] << 12) | (q6 << 6) | (tau4[i] << 2) | c];
        int code = e >> 6, uu = code == 2 ? 1 : code == 1 ? -1 : 0;
        int nq = e & 63, nc = c;
        if (uu) { nc = 0; mflag[i] = 0; }
        else {
            if (q6 == 63 || q6 == 0) {
                const int sr = q6 ? 1 : -1;
                if (!mflag[i] && sr * dc[i] < marg) { uu = sr; mflag[i] = 1; }        // arrival margin
                else if (sr * dc[i] < 0) uu = sr;                                     // rail rebalancing
            }
            if (!uu || !(q6 == 63 || q6 == 0)) {
                // stiction class ticks: 0->1 next rest phase, 1->2 after ~8, 2->3 after ~64
                if (c == 0) nc = 1;
                else if (c == 1 && (field & 7) == 0) nc = 2;
                else if (c == 2 && (field & 63) == 0) nc = 3;
            }
        }
        if (uu) { int dd = dc[i] + uu; dc[i] = (int8_t)(dd > 127 ? 127 : dd < -127 ? -127 : dd); pushes++; }
        st[i] = (uint8_t)(nq | (nc << 6));
        u[i] = (int8_t)uu;
    }
    return pushes;
}


// Generalised widths: target TB bits, model state QB bits, threshold 4 bits, stiction class 2 bits.
// Entry is 16-bit: drive[15:14] | new q[QB-1:0].
void pqbn_lutg_build(uint16_t *lut, int TB, int QB, float T0, float s0, float taud) {
    const float D = 1.0f / T0;
    const float dwell_of_class[4] = {0.f, 3.f, 24.f, 256.f};
    const int TM = (1 << TB) - 1, QM = (1 << QB) - 1;
    for (int t = 0; t <= TM; t++)
    for (int qq = 0; qq <= QM; qq++)
    for (int a4 = 0; a4 < 16; a4++)
    for (int c = 0; c < 4; c++) {
        const float d = (float)t / TM, q = (float)qq / QM, tau = (a4 + 0.5f) / 16.0f;
        const float st = c ? 1.f - s0 * (1.f - expf(-dwell_of_class[c] / taud)) : 1.f;
        const float D1 = D * st;
        int s = d > q ? 1 : -1, n = 0, nbest = 0;
        if (fabsf(d - q) > 1e-6f) {
            float v = q, vprev = q;
            while (n < 64) {
                float nv = clampf(v + s * (n == 0 ? D1 : D), 0.f, 1.f);
                if (nv > 0.99f) nv = 1.f; if (nv < 0.01f) nv = 0.f;
                if (nv == v) break;
                vprev = v; v = nv; n++;
                if ((s > 0 && v >= d) || (s < 0 && v <= d)) break;
            }
            if ((s > 0 && v < d) || (s < 0 && v > d)) nbest = n;
            else {
                float lo = s > 0 ? vprev : v, hi = s > 0 ? v : vprev;
                float frac = (d - lo) / (hi - lo + 1e-9f);
                int pick_hi = frac > tau;
                nbest = ((s > 0) ? pick_hi : !pick_hi) ? n : n - 1;
            }
        }
        uint16_t e;
        if (nbest > 0) {
            float np = clampf(q + s * D1, 0.f, 1.f);
            if (np > 0.99f) np = 1.f; if (np < 0.01f) np = 0.f;
            int nq = (int)lrintf(np * QM);
            if (nq == qq) nq += s;
            if (nq < 0) nq = 0; if (nq > QM) nq = QM;
            e = (uint16_t)(((s > 0 ? 2 : 1) << 14) | nq);
        } else e = (uint16_t)qq;
        lut[(((t << QB) | qq) << 6) | (a4 << 2) | c] = e;
    }
}

int pqbn_lutg_phase(const uint16_t *lut, int TB, int QB, const uint8_t *tgt, const uint8_t *tau4,
                    uint16_t *q, uint8_t *cls, int8_t *dc, uint8_t *mflag, int8_t *u, int n,
                    uint32_t field, int marg) {
    const int QM = (1 << QB) - 1; int pushes = 0; (void)TB;
    for (int i = 0; i < n; i++) {
        const int qq = q[i], c = cls[i];
        const uint16_t e = lut[(((tgt[i] << QB) | qq) << 6) | (tau4[i] << 2) | c];
        int code = e >> 14, uu = code == 2 ? 1 : code == 1 ? -1 : 0;
        int nq = e & QM, nc = c;
        if (uu) { nc = 0; mflag[i] = 0; }
        else {
            if (qq == QM || qq == 0) {
                const int sr = qq ? 1 : -1;
                if (!mflag[i] && sr * dc[i] < marg) { uu = sr; mflag[i] = 1; }
                else if (sr * dc[i] < 0) uu = sr;
            }
            if (!uu || !(qq == QM || qq == 0)) {
                if (c == 0) nc = 1;
                else if (c == 1 && (field & 7) == 0) nc = 2;
                else if (c == 2 && (field & 63) == 0) nc = 3;
            }
        }
        if (uu) { int dd = dc[i] + uu; dc[i] = (int8_t)(dd > 127 ? 127 : dd < -127 ? -127 : dd); pushes++; }
        q[i] = (uint16_t)nq; cls[i] = (uint8_t)nc; u[i] = (int8_t)uu;
    }
    return pushes;
}

// ---------------------------------------------------------------------------
// Recommended form: 2-bit decision LUT (64 KB) + arithmetic state update.
// idx = t4<<14 | q8<<6 | tau4<<2 | cls ; code = 2 bits (0 none, 1 black push, 2 white push)
// q8 update: q += s*step[cls] (rounded calibrated first-push size), snapped to rails.
// ---------------------------------------------------------------------------
void pqbn_lut2_build(uint8_t *lut2 /* 65536 bytes */, int *step /* [4] */, float T0, float s0, float taud) {
    const float D = 1.0f / T0;
    const float dwell_of_class[4] = {0.f, 3.f, 24.f, 256.f};
    memset(lut2, 0, 65536);
    for (int c = 0; c < 4; c++) {
        const float st = c ? 1.f - s0 * (1.f - expf(-dwell_of_class[c] / taud)) : 1.f;
        step[c] = (int)lrintf(D * st * 255.0f);
    }
    for (int t4 = 0; t4 < 16; t4++)
    for (int q8 = 0; q8 < 256; q8++)
    for (int a4 = 0; a4 < 16; a4++)
    for (int c = 0; c < 4; c++) {
        // integer version of the same rule, on the exact state grid the runtime uses
        const int d = t4 * 17, q = q8;
        const int t = (a4 * 16 + 8);
        int code = 0;
        if (d != q) {
            const int s = d > q ? 1 : -1;
            const int D1 = step[c], Dn = step[0];
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
            if (nbest > 0) code = s > 0 ? 2 : 1;
        }
        const int idx = (t4 << 14) | (q8 << 6) | (a4 << 2) | c;
        lut2[idx >> 2] |= (uint8_t)(code << ((idx & 3) << 1));
    }
}

// One phase. tgt4: 4bpp target, one nibble per byte; tau4 per pixel; q8 state; cls 2-bit class.
int pqbn_lut2_phase(const uint8_t *lut2, const int *step, const uint8_t *tgt4, const uint8_t *tau4,
                    uint8_t *q8, uint8_t *cls, int8_t *dc, uint8_t *mflag, int8_t *u, int n,
                    uint32_t field, int marg) {
    int pushes = 0;
    for (int i = 0; i < n; i++) {
        const int q = q8[i], c = cls[i];
        const int idx = (tgt4[i] << 14) | (q << 6) | (tau4[i] << 2) | c;
        const int code = (lut2[idx >> 2] >> ((idx & 3) << 1)) & 3;
        int uu = code == 2 ? 1 : code == 1 ? -1 : 0, nq = q, nc = c;
        if (uu) {
            nq = q + uu * step[c];
            if (nq > 252) nq = 255; if (nq < 3) nq = 0;
            nc = 0; mflag[i] = 0;
        } else {
            if (q == 255 || q == 0) {
                const int sr = q ? 1 : -1;
                if (!mflag[i] && sr * dc[i] < marg) { uu = sr; mflag[i] = 1; }
                else if (sr * dc[i] < 0) uu = sr;
            }
            if (!uu || !(q == 255 || q == 0)) {
                if (c == 0) nc = 1;
                else if (c == 1 && (field & 7) == 0) nc = 2;
                else if (c == 2 && (field & 63) == 0) nc = 3;
            }
        }
        if (uu) { int dd = dc[i] + uu; dc[i] = (int8_t)(dd > 127 ? 127 : dd < -127 ? -127 : dd); pushes++; }
        q8[i] = (uint8_t)nq; cls[i] = (uint8_t)nc; u[i] = (int8_t)uu;
    }
    return pushes;
}

// Event-driven variant: a pixel at rest whose target did not change is not evaluated at all
// (its decision is frozen until the target changes). Returns the number of pixels evaluated,
// i.e. the real per-phase work. prev_tgt holds last phase's target.
int pqbn_lut2_phase_ev(const uint8_t *lut2, const int *step, const uint8_t *tgt4, uint8_t *prev_tgt,
                       const uint8_t *tau4, uint8_t *q8, uint8_t *cls, int8_t *dc, uint8_t *mflag,
                       uint8_t *busy, int8_t *u, int n, uint32_t field, int marg, int *evaluated) {
    int pushes = 0, ev = 0;
    for (int i = 0; i < n; i++) {
        const int changed = tgt4[i] != prev_tgt[i];
        prev_tgt[i] = tgt4[i];
        if (!changed && !busy[i]) {
            u[i] = 0;
            // lazy dwell: class advances without touching the pixel's decision
            const int c = cls[i];
            if (c == 0) cls[i] = 1;
            else if (c == 1 && (field & 7) == 0) cls[i] = 2;
            else if (c == 2 && (field & 63) == 0) cls[i] = 3;
            continue;
        }
        ev++;
        const int q = q8[i], c = cls[i];
        const int idx = (tgt4[i] << 14) | (q << 6) | (tau4[i] << 2) | c;
        const int code = (lut2[idx >> 2] >> ((idx & 3) << 1)) & 3;
        int uu = code == 2 ? 1 : code == 1 ? -1 : 0, nq = q, nc = c;
        if (uu) {
            nq = q + uu * step[c];
            if (nq > 252) nq = 255; if (nq < 3) nq = 0;
            nc = 0; mflag[i] = 0;
        } else if (q == 255 || q == 0) {
            const int sr = q ? 1 : -1;
            if (!mflag[i] && sr * dc[i] < marg) { uu = sr; mflag[i] = 1; }
            else if (sr * dc[i] < 0) uu = sr;
        }
        if (!uu && c == 0) nc = 1;
        if (uu) { int dd = dc[i] + uu; dc[i] = (int8_t)(dd > 127 ? 127 : dd < -127 ? -127 : dd); pushes++; }
        busy[i] = (uint8_t)(uu != 0);       // stays active while it is still pushing
        q8[i] = (uint8_t)nq; cls[i] = (uint8_t)nc; u[i] = (int8_t)uu;
    }
    *evaluated = ev;
    return pushes;
}
