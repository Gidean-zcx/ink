// Old event-driven kernel (8 byte arrays, prev-frame copy, row memcmp, 64 KB LUT) vs v2
// (one 16-bit state word, 32 KB mirrored LUT, 32x4 tiles from LVGL dirty areas, settled fast path)
// on realistic full-resolution UI scenarios in panel orientation. x86 single core, scalar build.
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#define W 1216
#define H 684
#define NTX (W / 32)
#define NTY (H / 4)

static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }

// ---------------- old kernel (from bench_ev.c) ----------------
void pqbn_lut2_build(uint8_t *lut2, int *step, float T0, float s0, float taud);
static uint8_t lut2[65536]; static int step2[4];
static uint8_t tgt[H][W], prevt[H][W], tau_o[H][W], q8[H][W], cls[H][W], mflag[H][W], busy[H][W];
static int8_t dc[H][W];
static uint8_t out_o[H][W / 4];
static int row_busy[H];

static long old_phase(uint32_t field, int marg) {
    long ev = 0;
    for (int y = 0; y < H; y++) {
        const int dirty = memcmp(tgt[y], prevt[y], W) != 0;
        if (!dirty && row_busy[y] == 0) continue;
        if (dirty) memcpy(prevt[y], tgt[y], W);
        int rb = 0;
        uint8_t *o = out_o[y];
        for (int x = 0; x < W; x += 4) {
            uint8_t ob = 0;
            for (int k = 0; k < 4; k++) {
                const int i = x + k;
                int uu = 0;
                if (dirty || busy[y][i]) {
                    ev++;
                    const int q = q8[y][i], c = cls[y][i];
                    const int idx = (tgt[y][i] << 14) | (q << 6) | (tau_o[y][i] << 2) | c;
                    const int code = (lut2[idx >> 2] >> ((idx & 3) << 1)) & 3;
                    uu = (code == 2) - (code == 1);
                    int nq = q + uu * step2[c];
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

// ---------------- v2 ----------------
typedef struct {
    int W_, H_, ntx, nty; const uint8_t *lut; uint8_t step[4]; int marg;
    const uint8_t *fb4; uint16_t *st; const uint8_t *tau;
    uint8_t *tile_dirty; uint16_t *tile_busy; uint16_t *tile_last;
    uint8_t *drive; uint8_t *row_active; int8_t *u_dbg;
} pqbn_v3_ctx;
int pqbn_v3_build(uint8_t *lut, uint8_t *step_out, float T0, float s0, float taud);
int pqbn_v3_phase(pqbn_v3_ctx *C, uint32_t field);
static uint8_t lut32[65536], tau64[64 * 64];
static uint8_t fb4[H][W / 2];
static uint16_t st[H * W];
static uint8_t tdirty[NTX * NTY]; static uint16_t tbusy[NTX * NTY], tlast[NTX * NTY];
static uint8_t drive[H * W / 4], rowact[H];

int main(int argc, char **argv) {
    pqbn_lut2_build(lut2, step2, 7.6f, 0.3f, 120.f);
    pqbn_v3_ctx C = {W, H, NTX, NTY, lut32, {0}, 2, &fb4[0][0], st, tau64, tdirty, tbusy, tlast, drive, rowact, NULL};
    int lutB = pqbn_v3_build(lut32, C.step, 7.6f, 0.3f, 120.f);
    for (int i = 0; i < 64 * 64; i++) tau64[i] = (uint8_t)((((i & 63) * 37) + ((i >> 6) * 101)) & 15);
    const char *names[] = {"static", "ai_stream", "page_turn", "scroll_half", "scroll_full", "scroll_full_14fps", "covers_scroll"};
    const char *labels[] = {"static page", "AI text streaming", "page turn", "list scroll, half screen",
                            "full-screen scroll, 28 fps", "full-screen scroll, 14 fps", "cover grid scroll (images)"};
    printf("%-28s | %-30s | %-38s | speedup\n", "scenario", "old: avg/max ms, px evaluated", "new: avg/max ms, tiles touched");
    for (int s = 0; s < 7; s++) {
        char path[512]; snprintf(path, sizeof path, "%s/%s.bin", argv[1], names[s]);
        FILE *fp = fopen(path, "rb"); int nf; if (fread(&nf, 4, 1, fp) != 1) return 1;
        const size_t fbB = (size_t)H * (W / 2), tdB = (size_t)NTX * NTY;
        uint8_t *frames = malloc((size_t)nf * (fbB + tdB));
        if (fread(frames, 1, (size_t)nf * (fbB + tdB), fp) != (size_t)nf * (fbB + tdB)) return 1;
        fclose(fp);
        // settled start on frame 0 for both kernels
        memcpy(fb4, frames, fbB);
        for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
            int t4 = (fb4[y][x >> 1] >> ((x & 1) << 2)) & 15;
            tgt[y][x] = prevt[y][x] = (uint8_t)t4; q8[y][x] = (uint8_t)(t4 * 17); cls[y][x] = 3; mflag[y][x] = 1;
            busy[y][x] = 0; dc[y][x] = 0; tau_o[y][x] = tau64[(y & 63) * 64 + (x & 63)];
            st[y * W + x] = (uint16_t)(0x8000 | 0x0300 | (t4 * 17));
        }
        memset(row_busy, 0, sizeof row_busy); memset(tdirty, 0, sizeof tdirty); memset(tbusy, 0, sizeof tbusy);
        memset(tlast, 0, sizeof tlast);
        double to_sum = 0, to_max = 0, tn_sum = 0, tn_max = 0; long ev_old = 0, tiles_new = 0;
        double to_c = 0, tn_c = 0;
        const int PHS = nf * 3;
        for (int p = 1; p < PHS; p++) {
            if (p % 3 == 0) {                       // new LVGL frame
                const uint8_t *fr = frames + (size_t)(p / 3) * (fbB + tdB);
                memcpy(fb4, fr, fbB);
                for (int i = 0; i < (int)tdB; i++) tdirty[i] |= fr[fbB + i];
                for (int y = 0; y < H; y++) for (int x = 0; x < W; x++)
                    tgt[y][x] = (fb4[y][x >> 1] >> ((x & 1) << 2)) & 15;   // old kernel's unpacked copy (untimed)
            }
            for (int i = 0; i < NTX * NTY; i++) tiles_new += tdirty[i] | (tbusy[i] != 0);
            double a = now(); ev_old += old_phase((uint32_t)p, 2); double b = now();
            pqbn_v3_phase(&C, (uint32_t)p); double c = now();
            if (p % 3 == 0) { to_c += b - a; tn_c += c - b; }
            to_sum += b - a; if (b - a > to_max) to_max = b - a;
            tn_sum += c - b; if (c - b > tn_max) tn_max = c - b;
        }
        printf("%-28s | %6.3f / %6.3f  %5.1f%% px  | %6.3f / %6.3f  %5.1f%% tiles (%5.1f%% px) | %5.1fx\n",
               labels[s], to_sum / (PHS - 1) * 1e3, to_max * 1e3, 100.0 * ev_old / (PHS - 1) / (W * H),
               tn_sum / (PHS - 1) * 1e3, tn_max * 1e3, 100.0 * tiles_new / (PHS - 1) / (NTX * NTY),
               100.0 * tiles_new * 128 / (PHS - 1) / (W * H), to_sum / tn_sum);
        printf("%-28s   content-frame phases: old %.3f ms, new %.3f ms | other phases: old %.3f ms, new %.3f ms\n", "",
               to_c / ((PHS - 1) / 3) * 1e3, tn_c / ((PHS - 1) / 3) * 1e3,
               (to_sum - to_c) / (PHS - 1 - (PHS - 1) / 3) * 1e3, (tn_sum - tn_c) / (PHS - 1 - (PHS - 1) / 3) * 1e3);
        free(frames);
    }
    printf("\nstate memory: old 8 B/px = %d KB (+64 KB LUT); new 2 B/px = %d KB (+%d KB LUT, 4 KB thresholds, %d KB tile tables)\n",
           W * H * 8 / 1024, W * H * 2 / 1024, lutB / 1024, (NTX * NTY * 5) / 1024);
    return 0;
}
