/* The panel's measured response at 12 ms a push, relative to the paper (docs/hardware/2026-11-06-phase3.md).
 *
 * The first wedge (one phone photo, from rest):
 *   from white 1 .91 2 .72 3 .54 4 .41 5 .30 6 .27 7 .23 8 .21; from black 1 .27 2 .44 3 .63 4 .76 5 .83 6 .86 7 .89 8 .93 10 .97
 * The curves below are the second wedge's rows right after a swing to the other rail, which is how a pixel goes to a
 * gray on the single path: one video frame, read against the factory's levels on the same frame, smoothed. */
#ifndef PANEL_CURVE_H
#define PANEL_CURVE_H
static const float PANEL_DOWN[] = {1.00f, .86f, .72f, .53f, .40f, .36f, .31f, .22f, .20f, .19f, .18f, .17f, .16f, .155f, .15f, .15f, .15f, .15f, .15f, .145f, .145f, .14f, .135f, .13f, .13f};
#define PANEL_DOWN_N 25
static const float PANEL_UP[] = {.13f, .16f, .31f, .43f, .63f, .84f, .94f, .95f, .98f, 1.00f};
#define PANEL_UP_N 10
/* the factory driver's 16 levels */
static const float PANEL_LEVEL[16] = {.20f, .26f, .34f, .41f, .47f, .57f, .65f, .72f, .80f, .86f, .90f, .94f, .96f, .98f, .99f, 1.00f};
/* Pushes from each rail to each level: the curves above read at the levels above. */
static const float PANEL_PUSHES_DOWN[16] = {0, 6.55f, 5.4f, 3.92f, 3.46f, 2.79f, 2.37f, 2.0f, 1.43f, 1.0f, .71f, .43f, .29f, .14f, .07f, 0};
static const float PANEL_PUSHES_UP[16] = {0, .55f, 1.27f, 1.87f, 2.2f, 2.7f, 3.1f, 3.43f, 3.8f, 4.2f, 4.6f, 5.0f, 6.0f, 7.0f, 7.5f, 0};
/* A level's share of white pixels in the swing, so it looks as bright as it will end: (level - rail black) / (1 - rail black). */
static const float PANEL_WHITE_SHARE[16] = {0, .12f, .21f, .30f, .37f, .49f, .58f, .67f, .76f, .83f, .88f, .93f, .95f, .98f, .99f, 1};
#endif
