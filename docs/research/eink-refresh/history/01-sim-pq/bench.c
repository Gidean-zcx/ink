// Compute-cost benchmark: ns per pixel per field, single core, -O2.
// gcc -O2 -o bench bench.c pq_fixed.c -lm && ./bench
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <time.h>

typedef struct { int D; int D1[8]; int MU; int maxn; int marg; int rails_only; } pq_params;
void pq_field_q8(const uint8_t *I, uint8_t *ph, uint8_t *meta, int8_t *u, int w, int h,
                 const pq_params *P, uint32_t field, int16_t *err);
void pq_init_params(pq_params *P, float T0, float s0, float taud, float mu, int maxn, int marg);
typedef struct { int thr[8]; int invD; int half_mu_over_D; } pq_fast_tab;
void pq_fast_init(const pq_params *P, pq_fast_tab *T);
void pq_field_q8_wf(const uint8_t *I, uint8_t *ph, uint8_t *meta, int8_t *u, int w, int h,
                    const pq_params *P, const pq_fast_tab *T, uint32_t field, int16_t *E);
void pq_field_q8_bl(const uint8_t *I, uint8_t *ph, uint8_t *meta, int8_t *u, int w, int h,
                    const pq_params *P, const pq_fast_tab *T, uint32_t field, int16_t *err);
void pq_field_q8_fast(const uint8_t *I, uint8_t *ph, uint8_t *meta, int8_t *u, int w, int h,
                      const pq_params *P, const pq_fast_tab *T, uint32_t field, int16_t *err);

static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }

// ---- PaperBoy msg.c inner loop, ported verbatim (minus DMA/GPIO) -------------
// in: 1 bit/pixel, state: 4 bits/pixel, out: 2 bits/pixel
static void pb_field(const uint8_t *in, uint8_t *statebuf, uint8_t *outbuf, int n) {
    const uint8_t *rdptr = in; uint8_t *stptr = statebuf, *wrptr = outbuf;
    static const uint8_t RESET_CNTR_MASK[4] = {0xfc, 0xe0, 0x1c, 0x00};
    for (int j = 0; j < n / 8; j++) {
        uint8_t incoming_pixels = *rdptr++;
        for (int k = 0; k < 2; k++) {
            uint8_t out = 0;
            for (int l = 0; l < 2; l++) {
                uint8_t state; out <<= 4;
                state = *stptr;
                uint8_t driving_dir = incoming_pixels >> 6;
                uint8_t pixel_diff = (state ^ driving_dir) & 0x3;
                state &= RESET_CNTR_MASK[pixel_diff];
                state |= driving_dir;
                out |= (state & 0x80) ? (0x0) : (driving_dir & 0x2) ? 0x4 : 0x8;
                out |= (state & 0x10) ? (0x0) : (driving_dir & 0x1) ? 0x1 : 0x2;
                uint8_t cntr_incr = ((~state) >> 2) & 0x24;
                state = state + cntr_incr;
                *stptr++ = state;
                incoming_pixels <<= 2;
            }
            *wrptr++ = out;
        }
    }
}

// ---- Integrator, same packing as PaperBoy (2 px per state byte) --------------
// state nibble = pos (0..4); in: 1 bit/pixel (binary) -> target pos 0 or 4
static void int_field_packed(const uint8_t *in, uint8_t *statebuf, uint8_t *outbuf, int n) {
    const uint8_t *rdptr = in; uint8_t *stptr = statebuf, *wrptr = outbuf;
    for (int j = 0; j < n / 8; j++) {
        uint8_t inc = *rdptr++;
        for (int k = 0; k < 2; k++) {
            uint8_t out = 0;
            for (int l = 0; l < 2; l++) {
                uint8_t st = *stptr;
                int p1 = st >> 4, p0 = st & 0xf;
                int t1 = (inc >> 5) & 4, t0 = (inc >> 4) & 4;      // bit7 -> 4, bit6 -> 4
                int s1 = (t1 > p1) - (t1 < p1), s0 = (t0 > p0) - (t0 < p0);
                p1 += s1; p0 += s0;
                *stptr++ = (uint8_t)((p1 << 4) | p0);
                out = (uint8_t)((out << 4) | ((s1 > 0) ? 0x4 : (s1 < 0) ? 0x8 : 0) | ((s0 > 0) ? 0x1 : (s0 < 0) ? 0x2 : 0));
                inc <<= 2;
            }
            *wrptr++ = out;
        }
    }
}

// ---- integer serpentine Floyd-Steinberg -> 1 bit/pixel packed -----------------
static void fs_pack(const uint8_t *I, uint8_t *bits, int w, int h, int16_t *err) {
    memset(err, 0, sizeof(int16_t) * 2 * (w + 2));
    memset(bits, 0, (size_t)w * h / 8);
    int16_t *cur = err + 1, *nxt = err + (w + 2) + 1;
    for (int y = 0; y < h; y++) {
        int dir = (y & 1) ? -1 : 1, x = dir > 0 ? 0 : w - 1;
        for (int i = 0; i < w; i++, x += dir) {
            int idx = y * w + x, d = I[idx] + cur[x], b = d > 127;
            if (b) bits[idx >> 3] |= (uint8_t)(0x80 >> (idx & 7));
            int e = d - (b ? 255 : 0);
            int e7 = (e * 7 + 8) >> 4, e3 = (e * 3 + 8) >> 4, e5 = (e * 5 + 8) >> 4;
            cur[x + dir] += e7; nxt[x - dir] += e3; nxt[x] += e5; nxt[x + dir] += e - e7 - e3 - e5;
        }
        int16_t *t = cur; cur = nxt; nxt = t;
        memset(nxt - 1, 0, sizeof(int16_t) * (w + 2));
    }
}

// ---- blue-noise ordered threshold -> 1 bit/pixel packed ------------------------
static void bn_pack(const uint8_t *I, const uint8_t *bn64, uint8_t *bits, int w, int h) {
    for (int y = 0; y < h; y++) {
        const uint8_t *row = bn64 + (y & 63) * 64;
        for (int x = 0; x < w; x += 8) {
            uint8_t b = 0;
            for (int k = 0; k < 8; k++) b = (uint8_t)((b << 1) | (I[y * w + x + k] > row[(x + k) & 63]));
            bits[(y * w + x) >> 3] = b;
        }
    }
}

static void pack_u(const int8_t *u, uint8_t *out, int n) {   // drive codes, 2 bits/pixel
    for (int i = 0; i < n; i += 4) {
        uint8_t o = 0;
        for (int k = 0; k < 4; k++) o = (uint8_t)((o << 2) | (u[i + k] > 0 ? 1 : u[i + k] < 0 ? 2 : 0));
        out[i >> 2] = o;
    }
}

static void make_frame(uint8_t *I, int w, int h, int t) {    // moving natural-ish content
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            float v = 0.5f + 0.25f * sinf((x + 3 * t) * 0.031f) * cosf((y - 2 * t) * 0.017f)
                    + 0.2f * sinf((x * 0.11f + y * 0.07f) + t * 0.05f);
            unsigned r = (unsigned)(x * 1103515245u + y * 12345u + t * 2654435761u) >> 27;
            int iv = (int)(v * 255.0f) + (int)r - 16;
            I[y * w + x] = (uint8_t)(iv < 0 ? 0 : iv > 255 ? 255 : iv);
        }
}

static void run(int w, int h, int fields) {
    int n = w * h;
    uint8_t **frames = malloc(sizeof(uint8_t *) * 16);
    for (int t = 0; t < 16; t++) { frames[t] = malloc(n); make_frame(frames[t], w, h, t * 4); }
    uint8_t *bits = calloc(n / 8, 1), *st4 = calloc(n / 2, 1), *out = calloc(n / 4, 1);
    uint8_t *ph = malloc(n), *meta = malloc(n), bn[64 * 64];
    int8_t *u = calloc(n, 1); int16_t *err = calloc(2 * (w + 2), sizeof(int16_t));
    for (int i = 0; i < 64 * 64; i++) bn[i] = (uint8_t)((i * 2654435761u) >> 24);  // stand-in mask
    memset(ph, 255, n); memset(meta, 7 | 8, n);
    pq_params P; pq_init_params(&P, 3.8f, 0.3f, 120.f, 0.05f, 6, 3);
    pq_fast_tab T; pq_fast_init(&P, &T);
    uint8_t *ph2 = malloc(n), *meta2 = malloc(n); memset(ph2, 255, n); memset(meta2, 7 | 8, n);
    double tpf = 0, tbl = 0, twf = 0;
    uint8_t *ph4 = malloc(n), *meta4 = malloc(n); memset(ph4, 255, n); memset(meta4, 7 | 8, n);
    int16_t *E = calloc((size_t)(h + 1) * (w + 2), sizeof(int16_t));
    uint8_t *ph3 = malloc(n), *meta3 = malloc(n); memset(ph3, 255, n); memset(meta3, 7 | 8, n);
    double t0, tpb = 0, tint = 0, tfs = 0, tbn = 0, tpq = 0, tpk = 0;
    volatile uint8_t sink = 0;
    for (int f = 0; f < fields; f++) {
        const uint8_t *I = frames[f & 15];
        t0 = now(); fs_pack(I, bits, w, h, err); tfs += now() - t0;
        t0 = now(); pb_field(bits, st4, out, n); tpb += now() - t0;            sink ^= out[f % (n / 4)];
        t0 = now(); int_field_packed(bits, st4, out, n); tint += now() - t0;   sink ^= out[f % (n / 4)];
        t0 = now(); bn_pack(I, bn, bits, w, h); tbn += now() - t0;
        t0 = now(); pq_field_q8(I, ph, meta, u, w, h, &P, (uint32_t)f, err); tpq += now() - t0;
        t0 = now(); pack_u(u, out, n); tpk += now() - t0;                      sink ^= out[f % (n / 4)];
        t0 = now(); pq_field_q8_fast(I, ph2, meta2, u, w, h, &P, &T, (uint32_t)f, err); tpf += now() - t0; sink ^= (uint8_t)u[f % n];
        t0 = now(); pq_field_q8_bl(I, ph3, meta3, u, w, h, &P, &T, (uint32_t)f, err); tbl += now() - t0; sink ^= (uint8_t)u[f % n];
        t0 = now(); pq_field_q8_wf(I, ph4, meta4, u, w, h, &P, &T, (uint32_t)f, E); twf += now() - t0; sink ^= (uint8_t)u[f % n];
    }
    double k = 1e9 / ((double)fields * n);
    printf("%4dx%-4d  PB driver %.2f | integrator driver %.2f | FS dither %.2f | BN dither %.2f | PQ (dither+drive) %.2f (+pack %.2f)  ns/px\n",
           w, h, tpb * k, tint * k, tfs * k, tbn * k, tpq * k, tpk * k);
    printf("           PB+FS total %.2f   PB+BN total %.2f   PQ total %.2f   -> PQ / (PB+FS) = %.2fx, PQ / (PB+BN) = %.2fx\n",
           (tpb + tfs) * k, (tpb + tbn) * k, (tpq + tpk) * k, (tpq + tpk) / (tpb + tfs), (tpq + tpk) / (tpb + tbn));
    printf("           PQ-fast (closed form) %.2f ns/px (+pack %.2f) = %.2f total -> %.2fx PB+FS, %.2fx PB+BN\n",
           tpf * k, tpk * k, (tpf + tpk) * k, (tpf + tpk) / (tpb + tfs), (tpf + tpk) / (tpb + tbn));
    printf("           PQ-branchless %.2f ns/px (+pack %.2f) = %.2f total -> %.2fx PB+FS, %.2fx PB+BN\n",
           tbl * k, tpk * k, (tbl + tpk) * k, (tbl + tpk) / (tpb + tfs), (tbl + tpk) / (tpb + tbn));
    printf("           PQ-wavefront %.2f ns/px (+pack %.2f) = %.2f total -> %.2fx PB+FS, %.2fx PB+BN\n",
           twf * k, tpk * k, (twf + tpk) * k, (twf + tpk) / (tpb + tfs), (twf + tpk) / (tpb + tbn));
    (void)sink;
}

int main(void) {
    run(432, 160, 600);     // PaperBoy active window (drive grid)
    run(1600, 1200, 60);    // 13.3" monitor class
    return 0;
}
