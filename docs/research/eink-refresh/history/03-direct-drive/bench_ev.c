// Per-phase cost of event-driven PQ-BN (64 KB decision LUT) on 1216x684 for typical UI cases.
// Rows that have no changed target and no pixel still pushing are skipped entirely (in a direct
// driver their DMA descriptor points at a shared all-zero line). x86 single core, -O2 -fno-tree-vectorize.
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#define W 1216
#define H 684
void pqbn_lut2_build(uint8_t *lut2, int *step, float T0, float s0, float taud);

static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }

static uint8_t lut2[65536]; static int step[4];
static uint8_t tgt[H][W], prevt[H][W], tau[H][W], q8[H][W], cls[H][W], mflag[H][W], busy[H][W];
static int8_t dc[H][W];
static uint8_t out[H][W / 4];
static int row_busy[H];

// one phase; returns evaluated pixel count
static long phase(uint32_t field, int marg) {
    long ev = 0;
    for (int y = 0; y < H; y++) {
        const int dirty = memcmp(tgt[y], prevt[y], W) != 0;    // or: LVGL invalidated-area list
        if (!dirty && row_busy[y] == 0) continue;               // idle row: zero-line DMA, no CPU
        if (dirty) memcpy(prevt[y], tgt[y], W);
        int rb = 0;
        uint8_t *o = out[y];
        for (int x = 0; x < W; x += 4) {
            uint8_t ob = 0;
            for (int k = 0; k < 4; k++) {
                const int i = x + k;
                int uu = 0;
                if (dirty || busy[y][i]) {
                    ev++;
                    const int q = q8[y][i], c = cls[y][i];
                    const int idx = (tgt[y][i] << 14) | (q << 6) | (tau[y][i] << 2) | c;
                    const int code = (lut2[idx >> 2] >> ((idx & 3) << 1)) & 3;
                    uu = (code == 2) - (code == 1);
                    int nq = q + uu * step[c];
                    nq = nq > 252 ? 255 : nq < 3 ? 0 : nq;
                    if (!uu && (q == 255 || q == 0)) {
                        const int sr = q ? 1 : -1;
                        if (!mflag[y][i] && sr * dc[y][i] < marg) { uu = sr; mflag[y][i] = 1; }
                        else if (sr * dc[y][i] < 0) uu = sr;
                    } else if (uu) mflag[y][i] = 0;
                    q8[y][i] = (uint8_t)nq;
                    cls[y][i] = uu ? 0 : (c == 0 ? 1 : c);
                    dc[y][i] = (int8_t)(dc[y][i] + uu);
                    busy[y][i] = (uint8_t)(uu != 0);
                    rb += uu != 0;
                }
                ob = (uint8_t)((ob << 2) | (uu > 0 ? 2 : uu < 0 ? 1 : 0));
            }
            o[x >> 2] = ob;
        }
        row_busy[y] = rb;
    }
    return ev;
}

static void text_rows(int y0, int y1, int shift) {          // synthetic 4bpp text band
    for (int y = y0; y < y1 && y < H; y++)
        for (int x = 0; x < W; x++) {
            int yy = y + shift, gl = ((x / 9) * 7 + (yy / 22) * 13) % 5;
            int ink = ((yy % 22) > 4) && ((yy % 22) < 18) && gl && ((x % 9) < 6) && (((x * 3 + yy * 5) % 7) < 3);
            int edge = ((x % 9) == 6) && ((yy % 22) > 4) && ((yy % 22) < 18);
            tgt[y][x] = (uint8_t)(ink ? 0 : edge ? 9 : 15);
        }
}

typedef void (*scen_fn)(int content_frame);
static void s_static(int f) { (void)f; }
static void s_stream(int f) {                                // AI answer: one new text line every ~0.5 s,
    int line = (f / 14) % 20;                                // glyphs revealed left to right
    int y0 = 80 + line * 26, x1 = ((f % 14) + 1) * W / 14;
    for (int y = y0; y < y0 + 26; y++)
        for (int x = 0; x < x1; x++) tgt[y][x] = (uint8_t)((((x / 9) + y) % 4 == 0 && (y - y0) > 4 && (y - y0) < 20) ? 0 : 15);
}
static void s_scroll_half(int f) { text_rows(H / 2, H, f * 12); }
static void s_scroll_full(int f) { text_rows(0, H, f * 12); }
static void s_page_turn(int f) { if (f == 1) text_rows(0, H, 4000); }

int main(void) {
    pqbn_lut2_build(lut2, step, 7.6f, 0.3f, 120.f);
    struct { const char *name; scen_fn fn; } sc[] = {
        {"static page (nothing changes)", s_static},
        {"AI text streaming (1 line updating)", s_stream},
        {"page turn (whole screen once)", s_page_turn},
        {"list scroll, half screen, 28 fps", s_scroll_half},
        {"full-screen scroll, 28 fps", s_scroll_full},
    };
    for (unsigned s = 0; s < sizeof sc / sizeof sc[0]; s++) {
        text_rows(0, H, 0);
        for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
            prevt[y][x] = tgt[y][x]; q8[y][x] = tgt[y][x] * 17; cls[y][x] = 3; mflag[y][x] = 1;
            busy[y][x] = 0; dc[y][x] = 0; tau[y][x] = (uint8_t)(((x & 63) * 37 + (y & 63) * 101) & 15);
        }
        memset(row_busy, 0, sizeof row_busy);
        const int PH = 249;                                   // ~3 s of phases (83 Hz)
        double tsum = 0, tmax = 0; long evs = 0;
        for (int p = 0; p < PH; p++) {
            if (p % 3 == 0) sc[s].fn(p / 3);                  // LVGL frame every 3 phases
            double a = now();
            evs += phase((uint32_t)p, 2);
            double dt = now() - a; tsum += dt; if (dt > tmax) tmax = dt;
        }
        printf("%-38s avg %6.3f ms/phase  max %6.3f ms  evaluated %5.1f%% px/phase\n", sc[s].name,
               tsum / PH * 1e3, tmax * 1e3, 100.0 * evs / PH / (W * H));
    }
    return 0;
}
