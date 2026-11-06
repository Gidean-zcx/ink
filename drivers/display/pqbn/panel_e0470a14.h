/*
 * What was measured of this panel (E0470A14-AF on the Metalio E-Ink4 Plus), at 12 ms a push, relative to the paper:
 * docs/hardware/2026-11-06-phase3.md. From phone photos and video frames, read against the factory's own 16 levels on the
 * same picture, so good for relative heights, not as reflectances. No lab instrument has seen this panel.
 */
#ifndef INK_PANEL_E0470A14_H
#define INK_PANEL_E0470A14_H

#include "pqbn_v4.h"

/* The factory driver's 16 levels. */
static const float PANEL_LEVEL[16] = {.20f, .26f, .34f, .41f, .47f, .57f, .65f, .72f, .80f, .86f, .90f, .94f, .96f, .98f, .99f, 1.00f};

/* Brightness after n pushes right after a swing to the other rail (the second wedge), smoothed. */
static const float PANEL_DOWN[] = {1.00f, .86f, .72f, .53f, .40f, .36f, .31f, .22f, .20f, .19f, .18f, .17f, .16f, .155f, .15f, .15f, .15f, .15f, .15f, .145f, .145f, .14f, .135f, .13f, .13f};
#define PANEL_DOWN_N 25
static const float PANEL_UP[] = {.13f, .16f, .31f, .43f, .63f, .84f, .94f, .95f, .98f, 1.00f};
#define PANEL_UP_N 10

static const pqbn_panel_t PQBN_PANEL_E0470A14 = {
    .scale_pushes = 12.0f,
    .swing_units = 6, /* 12 pushes: full white and deep black, tried on the real panel */
    .split = 4,
    /* From rest the first push goes about half as far as right after a swing: 0.94 against 0.86 from white, and level 8
     * takes about 4 pushes from black at rest against 3.8. */
    .first_push = {1.0f, 1.0f, 0.8f, 0.6f},
    /* The curves above read at the levels above; levels 1..4 from black, 5..14 from white. */
    .pushes = {0, .55f, 1.27f, 1.87f, 2.2f, 2.79f, 2.37f, 2.0f, 1.43f, 1.0f, .71f, .43f, .29f, .14f, .07f, 0},
};

/* The model of the panel (simulator and tools/pqbn-lab): a pixel's next push is read off its direction's curve from
 * where its brightness is now. Assumed, not measured: that this holds after a reversal too, and it knows no rests. */
static inline float panel_curve_next(const float *c, int n, float b, int falling) {
    for (int i = 0; i + 1 < n; i++) {
        const float a0 = c[i], a1 = c[i + 1];
        if (falling ? (b <= a0 && b >= a1) : (b >= a0 && b <= a1)) {
            const float x = i + (a1 != a0 ? (b - a0) / (a1 - a0) : 1.f) + 1;
            if (x >= n - 1) return c[n - 1];
            const int k = (int)x;
            return c[k] + (c[k + 1] - c[k]) * (x - k);
        }
    }
    return falling ? (b > c[0] ? c[0] : c[n - 1]) : (b < c[0] ? c[0] : c[n - 1]);
}

/* next[0 .. 4095]: brightness after one more push, from brightness i / 4095 */
static inline void panel_model_tables(float *next_down, float *next_up) {
    for (int i = 0; i < 4096; i++) {
        next_down[i] = panel_curve_next(PANEL_DOWN, PANEL_DOWN_N, (float)i / 4095.f, 1);
        next_up[i] = panel_curve_next(PANEL_UP, PANEL_UP_N, (float)i / 4095.f, 0);
    }
}
#endif
