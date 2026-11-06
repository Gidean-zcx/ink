/* The way a change goes through the PQ-BN kernel (D-110), counted in pushes per pixel on a small panel:
 *   1. a light gray comes from white in a few pushes and never goes through black, so it does not go dark on the way;
 *   2. rail to rail is the swing, 12 pushes, in one plan: the first long plan and one more to settle;
 *   3. a small change leaves still what does not change, a gray area beside it too;
 *   4. a new page moves the grays too, so that nothing stays behind as a ghost;
 *   5. a dark gray comes from black. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "panel_e0470a14.h"
#include "pqbn_v4.h"

#define PW 64
#define PH 512

static int fails;
static pqbn_v4_ctx C;
static uint8_t *fb;
static uint32_t now = 1000;
static short black[PH][PW], white[PH][PW]; /* pushes of the last change */

#define CHECK(c, ...)                                                                                                  \
    do {                                                                                                               \
        if (!(c)) {                                                                                                    \
            fails++;                                                                                                   \
            fprintf(stderr, "FAIL: " __VA_ARGS__);                                                                     \
            fputc('\n', stderr);                                                                                       \
        }                                                                                                              \
    } while (0)

static void *zalloc(size_t n) {
    void *p = calloc(1, n);
    if (!p) abort();
    return p;
}

static void set_px(int x, int y, int level) {
    uint8_t *b = fb + (size_t)y * (PW / 2) + x / 2;
    *b = (x & 1) ? (uint8_t)((*b & 0x0f) | (level << 4)) : (uint8_t)((*b & 0xf0) | level);
}

static void setup(int level) {
    const int nt = (PW / PQBN_TPX) * (PH / PQBN_TPY);
    memset(&C, 0, sizeof C);
    C.W = PW;
    C.H = PH;
    C.ntx = PW / PQBN_TPX;
    C.nty = PH / PQBN_TPY;
    if (!fb) fb = zalloc((size_t)PW * PH / 2);
    memset(fb, level | (level << 4), (size_t)PW * PH / 2);
    C.fb4 = fb;
    C.hot = zalloc((size_t)PW * PH / 2);
    C.cold = zalloc((size_t)PW * PH * 2);
    C.tile_dirty = zalloc(nt);
    C.tile_nmax = zalloc(nt);
    C.tile_plan = zalloc((size_t)nt * 2);
    C.band = zalloc((size_t)C.nty * 4);
    if (pqbn_v4_build(&C, zalloc(16384), zalloc(6 * 256), zalloc(4096), &PQBN_PANEL_E0470A14)) abort();
    now += 300;
    pqbn_v4_init(&C, now);
}

/* run until nothing moves; returns the phases */
static int drive(void) {
    static uint8_t line[PW / 4 + 16] __attribute__((aligned(16)));
    memset(black, 0, sizeof black);
    memset(white, 0, sizeof white);
    int phases = 0;
    do {
        now++;
        phases++;
        pqbn_v4_phase(&C, now);
        for (int y = 0; y < PH; y++) {
            if (!pqbn_v4_exec_row(&C, now, y, line)) continue;
            for (int x = 0; x < PW; x++) {
                const int code = (line[x >> 2] >> (6 - 2 * (x & 3))) & 3;
                if (code == 1) black[y][x]++;
                if (code == 2) white[y][x]++;
            }
        }
    } while (pqbn_v4_busy_after(&C, now) && phases < 200);
    return phases;
}

static void mark_rows(int y0, int y1) {
    for (int ty = y0 / PQBN_TPY; ty <= (y1 - 1) / PQBN_TPY; ty++)
        for (int tx = 0; tx < C.ntx; tx++) C.tile_dirty[ty * C.ntx + tx] = 1;
}

static void fill_rows(int y0, int y1, int level) {
    for (int y = y0; y < y1; y++)
        for (int x = 0; x < PW; x++) set_px(x, y, level);
    mark_rows(y0, y1);
}

int main(void) {
    /* 1. white -> a light gray: a few pushes toward black, none toward white */
    setup(15);
    fill_rows(0, 16, 8);
    int ph = drive();
    int most = 0, least = 99, whites = 0;
    for (int y = 0; y < 16; y++)
        for (int x = 0; x < PW; x++) {
            if (black[y][x] > most) most = black[y][x];
            if (black[y][x] < least) least = black[y][x];
            whites += white[y][x];
        }
    CHECK(most <= 3 && least >= 1, "white to level 8: %d..%d pushes", least, most);
    CHECK(!whites, "white to level 8: %d pushes toward white", whites);
    CHECK(ph < 12, "white to level 8 took %d phases", ph);
    for (int y = 16; y < PH; y++)
        for (int x = 0; x < PW; x++) CHECK(!black[y][x] && !white[y][x], "a pixel that does not change was pushed (%d, %d)", x, y);

    /* 2. rail to rail: the swing */
    setup(15);
    fill_rows(0, 16, 0);
    ph = drive();
    CHECK(black[3][3] == 12 && !white[3][3], "white to black: %d black pushes, %d white", black[3][3], white[3][3]);
    CHECK(ph <= 14, "white to black took %d phases", ph);
    fill_rows(0, 16, 15);
    drive();
    CHECK(white[3][3] == 12 && !black[3][3], "black to white: %d white pushes, %d black", white[3][3], black[3][3]);

    /* 3. a small change in a gray area: the gray beside it stays still */
    setup(8);
    set_px(1, 1, 0);
    mark_rows(0, 4);
    drive();
    CHECK(black[1][1] > 0, "the pixel that changed did not move");
    CHECK(!black[2][2] && !white[2][2], "the gray beside a small change moved: %d black, %d white pushes", black[2][2], white[2][2]);

    /* 4. a new page: the grays move too, to white and back */
    setup(8);
    mark_rows(0, PH);
    drive();
    CHECK(white[5][5] > 0 && black[5][5] > 0, "gray on a new page: %d white, %d black pushes", white[5][5], black[5][5]);

    /* 5. white -> a dark gray goes to black first, and comes up from there */
    setup(15);
    fill_rows(0, 16, 2);
    drive();
    CHECK(black[3][3] == 12 && white[3][3] >= 1 && white[3][3] <= 3, "white to level 2: %d black, %d white pushes", black[3][3], white[3][3]);

    if (fails) return 1;
    printf("pqbn path: grays come the short way, what does not change stays still\n");
    return 0;
}
