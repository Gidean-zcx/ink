// Host-side statistics for v4 on the full-resolution scenarios: how many pixels the planner visits,
// how many of those had a target change, and how the hot nibbles are distributed.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#define W 1216
#define H 684
#define NTX (W / 32)
#define NTY (H / 4)
#define NT (NTX * NTY)
typedef struct { int W_, H_, ntx, nty; const uint8_t *dec, *exk; uint8_t step[4]; int marg;
    const uint8_t *fb4; uint8_t *hot; uint16_t *cold; const uint8_t *tau; uint8_t *tile_dirty; uint16_t *tile_plan;
    uint8_t *tile_nmax; uint8_t *drive; uint8_t *row_active; int plan_budget, plan_cursor; uint32_t stat_planned; const uint8_t *exk_m, *tau_nib; uint16_t *band; int line_mode, auto_motion, backlog, n_motion; } v4ctx;
int pqbn_v4_build(uint8_t *dec, uint8_t *exk, uint8_t *step_out, float T0, float s0, float taud);
int pqbn_v4_phase(v4ctx *C, uint32_t now);
void pqbn_v4_init(v4ctx *C, uint32_t now);
static int nib(const uint8_t *p, int i) { return (p[i >> 1] >> ((i & 1) << 2)) & 15; }
int main(int argc, char **argv) {
    const char *dir = argv[1]; const int PH = atoi(argv[2]);
    static uint8_t dec[16384], exk[1536], tau[4096], fb4[H*W/2], prev[H*W/2], hot[H*W/2], td[NT], tn[NT], drv[H*W/4], ra[H];
    static uint16_t cold[H*W], tp[NT]; uint8_t st[4];
    pqbn_v4_build(dec, exk, st, 7.6f, 0.3f, 120.f);
    for (int i = 0; i < 4096; i++) tau[i] = (uint8_t)((((i & 63) * 37) + ((i >> 6) * 101)) & 15);
    v4ctx C = {W, H, NTX, NTY, dec, exk, {st[0], st[1], st[2], st[3]}, 2, fb4, hot, cold, tau, td, tp, tn, drv, ra};
    const char *names[] = {"static", "ai_stream", "page_turn", "scroll_half", "scroll_full", "scroll_full_14fps", "covers_scroll"};
    for (int s = 0; s < 7; s++) {
        char path[512]; snprintf(path, sizeof path, "%s/%s.bin", dir, names[s]);
        FILE *fp = fopen(path, "rb"); int nf; if (fread(&nf, 4, 1, fp) != 1) return 1;
        const size_t fbB = (size_t)H * (W / 2), frB = fbB + NT;
        uint8_t *frames = malloc((size_t)nf * frB); if (fread(frames, 1, (size_t)nf * frB, fp) != (size_t)nf * frB) return 1; fclose(fp);
        static uint16_t band[2 * NTY]; C.band = band;
        memcpy(fb4, frames, fbB); memcpy(prev, frames, fbB); pqbn_v4_init(&C, 0);
        long rs_n0 = 0, rs_done = 0, rs_rail = 0; long dirty_tiles = 0, visited = 0, changed = 0, inflight_same = 0, rest_same = 0, settled_px = 0, gray_t = 0, content = 0, pushes = 0;
        for (int ph = 1; ph <= PH; ph++) {
            if (ph % 3 == 0 && ph / 3 < nf) {
                const uint8_t *fr = frames + (size_t)(ph / 3) * frB;
                memcpy(fb4, fr, fbB);
                for (int i = 0; i < NT; i++) td[i] |= fr[fbB + i];
            }
            // what the planner will see this phase
            for (int t = 0; t < NT; t++) {
                const int k = (uint16_t)(ph - tp[t]), nm = tn[t] & 127;
                if (!(td[t] || ((tn[t] & 128) && k >= nm))) continue;
                dirty_tiles++;
                const int x0 = (t % NTX) * 32, y0 = (t / NTX) * 4;
                for (int y = y0; y < y0 + 4; y++) for (int x = x0; x < x0 + 32; x++) {
                    const int i = y * W + x, t4 = nib(fb4, i), h = nib(hot, i);
                    const int sett = (t4 == 15 && h == 15) || (t4 == 0 && h == 7);
                    if (sett) { settled_px++; continue; }
                    visited++;
                    if (t4 != nib(prev, i)) changed++;
                    else if ((h & 7) != 7 && (h & 7) > k) inflight_same++;
                    else { rest_same++; if ((h & 7) == 0) rs_n0++; else rs_done++; if (t4 == 0 || t4 == 15) rs_rail++; }
                    if (t4 != 0 && t4 != 15) gray_t++;
                }
            }
            if (ph % 3 == 0) content++;
            pushes += 0;
            pqbn_v4_phase(&C, (uint32_t)ph);
            if (ph % 3 == 0 && ph / 3 < nf) memcpy(prev, fb4, fbB);
        }
        printf("%-18s resting-same breakdown per phase: n0==0 %7.0f, completed plan %7.0f, rail target %7.0f\n", names[s], rs_n0 / (double)PH, rs_done / (double)PH, rs_rail / (double)PH);
        printf("%-18s per phase: tiles planned %6.0f  px visited %7.0f (target changed %7.0f, in flight same target %6.0f, resting same target %7.0f; gray target %7.0f)  settled px skipped %7.0f\n",
               names[s], dirty_tiles / (double)PH, visited / (double)PH, changed / (double)PH, inflight_same / (double)PH, rest_same / (double)PH, gray_t / (double)PH, settled_px / (double)PH);
        free(frames);
    }
    return 0;
}
