/* The panel's measured response (docs/hardware/2026-11-06-phase3.md, one phone photo): brightness relative to
 * the paper after n pushes of 12 ms. Smoothed where the photo's noise made it go backwards. */
#ifndef PANEL_CURVE_H
#define PANEL_CURVE_H
static const float PANEL_DOWN[] = {1.00f, .91f, .72f, .54f, .41f, .30f, .27f, .23f, .21f, .205f, .19f, .185f, .18f, .175f, .155f, .15f, .15f, .15f, .15f, .145f, .145f, .14f, .135f, .13f, .13f};
#define PANEL_DOWN_N 25
/* from the factory's black (0.20); below that, two pushes out of the deepest black the wedge reached */
static const float PANEL_UP[] = {.13f, .16f, .20f, .27f, .44f, .63f, .76f, .83f, .86f, .89f, .93f, .95f, .97f, .98f, .985f, .99f, .99f, 1.00f};
#define PANEL_UP_N 18
/* the factory driver's 16 levels */
static const float PANEL_LEVEL[16] = {.20f, .26f, .34f, .41f, .47f, .57f, .65f, .72f, .80f, .86f, .90f, .94f, .96f, .98f, .99f, 1.00f};
/* Pushes from each rail to each level: the curves above read at the levels above. The rails sit a little past
 * the factory's black, so up[] starts about half a push further. */
static const float PANEL_PUSHES_DOWN[16] = {0, 6.25f, 4.64f, 4.0f, 3.54f, 2.83f, 2.39f, 2.0f, 1.58f, 1.26f, 1.05f, .67f, .44f, .22f, .11f, 0};
static const float PANEL_PUSHES_UP[16] = {0, 1.36f, 1.91f, 2.32f, 2.66f, 3.18f, 3.65f, 4.19f, 5.07f, 6.5f, 7.75f, 9.0f, 10.0f, 11.5f, 13.5f, 0};
#endif
