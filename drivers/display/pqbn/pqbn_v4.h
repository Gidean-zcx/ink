/*
 * PQ-BN direct-drive e-ink kernel (planner + executor), hardware independent.
 *
 * Grew from the research's kernel (docs/research/eink-refresh/pqbn_v4/kernel/pqbn_v4.c, PR #7); fixed as it is here
 * after five runs on the real panel (docs/hardware/2026-11-06-phase3.md, D-110). There are no modes: every change
 * goes the same way, so nothing has to tell a page turn from a scroll.
 *   1. The swing. Each pixel that has to change goes to the rail its target level comes from, in one plan. A level
 *      above `split` comes from white, the rest from black, each by the fewest pushes, so a light gray area never
 *      goes through black and does not go dark on the way. The page is readable here, in black and white.
 *   2. The settling, when nothing pushes any more: each pixel goes from its rail to its level, by the measured
 *      number of pushes. A fraction of a push is a 2 x 2 pattern, finer than the eye.
 *
 * Call sequence: pqbn_v4_build() once (tables), pqbn_v4_init() with the current framebuffer, then per drive phase:
 * the UI marks tile_dirty[] for changed 32x4 tiles, pqbn_v4_phase() plans, and the scan task calls pqbn_v4_exec_row()
 * for every panel row. Drive codes: 2 bit/px, 1 = push black, 2 = push white, pixel 0 in bits 7:6 of byte 0.
 *
 * A pixel holds its push until the next scan writes it, so a phase's length is the strength of its push. The port must
 * scan at a steady cadence (the measurements are at 12 ms) and, when pqbn_v4_plan_due() says a plan is coming, run one
 * empty scan first: the planner can take 0.1 s, and that must not lengthen a push.
 */
#ifndef INK_PQBN_V4_H
#define INK_PQBN_V4_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The port puts the scan-time executor in internal RAM (IRAM_ATTR on the ESP32-S31): it runs once
 * per panel row and must not wait for flash code fetches. */
#ifndef PQBN_HOT
#define PQBN_HOT
#endif

#define PQBN_TPX 32 /* tile width (px) */
#define PQBN_TPY 4  /* tile height (rows) */

/* What the tables are built from: the panel's measured response (panel_e0470a14.h). */
typedef struct {
    float scale_pushes;   // pushes the model state takes from rail to rail
    int swing_units;      // a rail-to-rail swing, in units of two pushes (at most 6)
    int split;            // levels above this come from the white rail, the rest from the black one
    float first_push[4];  // how far the first push goes after a rest, by dwell class: 0, 1..7, 8..63, >= 64 phases
    float pushes[16];     // pushes from its rail to each level
} pqbn_panel_t;

typedef struct {
    int W, H, ntx, nty;
    uint8_t *dec;             // 16 KB: [t4<<10 | q<<2 | cls] = nb | X<<4 ; n = up ? nb + (tau < X) : nb - (tau < X)
    uint8_t *exk;             // RMAX x 256: 4-bit drive codes of a hot byte at plan phase k (even px bits 3:2)
    uint8_t step[4];          // first-push size per stiction class (Q8 of a full swing); step[0] = D
    int marg;
    const uint8_t *fb4;       // target 4 bpp (UI framebuffer), low nibble = even x
    uint8_t *hot;             // plan nibbles
    uint16_t *cold;           // per-pixel state words
    uint8_t *tau;             // 64x64 thresholds 0..15: the 2 x 2 pattern a fraction of a push is drawn with
    uint8_t *tile_dirty;      // set by the UI layer from invalidated areas; cleared here
    uint16_t *tile_plan;      // phase of the tile's last plan
    uint8_t *tile_nmax;       // per-tile state byte, see T_* below
    uint32_t stat_planned;    // statistics: pixels visited by the planner (accumulates)
    uint32_t stat_swung;      // statistics: those visited for a swing, which costs less
    uint32_t split_nib;       // the split in every nibble of a word: white iff t4 > split
    uint16_t *band;           // per tile row: [2*ty] phase until which plans run, [2*ty+1] tiles waiting for the planner
    int swing_step;           // state moved per unit of a swing
    uint8_t level[16];        // the state each level rests at
    uint8_t lo[16], hi[16];   // a pixel between these already shows that level and stays still
} pqbn_v4_ctx;

// per-tile state byte (tile_nmax)
#define T_NMAX  0x07          // max pushes of the running plan (its tile is active while k < this)
#define T_PEND  0x08          // a UI change is waiting to be planned
#define T_SWING 0x10          // the running plan is a swing: T_NMAX counts units of two pushes
#define T_CAP   0x80          // the tile needs the planner once nothing pushes (after a swing, or a plan capped at RMAX pushes)

/* Tables (dec 16384 B, exk 6 x 256 B, tau 4096 B); fills C's pointers to them and everything derived from P. */
int pqbn_v4_build(pqbn_v4_ctx *C, uint8_t *dec, uint8_t *exk, uint8_t *tau, const pqbn_panel_t *P);
/* Every pixel settled on the current framebuffer (fb4), long dwell. */
void pqbn_v4_init(pqbn_v4_ctx *C, uint32_t now);
/* Will the plan for phase `now` take long? Ask before pqbn_v4_phase: then the port runs one empty scan first. */
int pqbn_v4_plan_due(const pqbn_v4_ctx *C, uint32_t now);
/* One drive phase: acknowledge UI marks and plan what is due. */
void pqbn_v4_phase(pqbn_v4_ctx *C, uint32_t now);
/* Does anything still have to happen after phase `now`? */
int pqbn_v4_busy_after(const pqbn_v4_ctx *C, uint32_t now);
/* A rest: the phase clock moves from `from` to `to` without driving. Does the upkeep the skipped
 * phases would have done (the settled pixels' timestamp sweep, the tile-age clamp at multiples of
 * 4096), so a rest is equivalent to running those phases with nothing to drive. Returns the rows
 * swept (for the cost; a rest longer than 512 phases sweeps every row once). */
int pqbn_v4_skip(pqbn_v4_ctx *C, uint32_t from, uint32_t to);
/* Drive line of row y for phase now (after pqbn_v4_phase). Returns 0 for a zero line. */
PQBN_HOT int pqbn_v4_exec_row(const pqbn_v4_ctx *C, uint32_t now, int y, uint8_t *line);

#ifdef __cplusplus
}
#endif
#endif
