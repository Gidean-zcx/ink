/*
 * PQ-BN v4 direct-drive e-ink kernel (planner + executor), hardware independent.
 *
 * Provenance: docs/research/eink-refresh/pqbn_v4/kernel/pqbn_v4.c (research archived in PR #7,
 * commit e77d01c, 2026-10-28 design by the assistant for this project). drivers/epd/pqbn_v4.c is
 * that file with this header split out; the algorithm is unchanged. See
 * docs/research/eink-refresh-algorithm.md for the design and docs/system/platform-and-simulator.md
 * for how the simulator and the ports use it.
 *
 * Call sequence: pqbn_v4_build() + pqbn_v4_build_motion() once (tables), pqbn_v4_init() with the
 * current framebuffer, then per drive phase: the UI marks tile_dirty[] for changed 32x4 tiles,
 * pqbn_v4_phase() plans (and, without line_mode, executes into the drive plane), and in line_mode
 * the scan task calls pqbn_v4_exec_row() for every panel row. Drive codes: 2 bit/px, 1 = push black,
 * 2 = push white, pixel 0 in bits 7:6 of byte 0.
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

typedef struct {
    int W, H, ntx, nty;
    const uint8_t *dec;       // 16 KB: [t4<<10 | q<<2 | cls] = nb | X<<4 ; n = up ? nb + (tau < X) : nb - (tau < X)
    const uint8_t *exk;       // RMAX x 256: 4-bit drive codes of a hot byte at plan phase k (even px bits 3:2)
    uint8_t step[4];          // first-push size per stiction class (Q8 of a full swing); step[0] = D
    int marg;
    const uint8_t *fb4;       // target 4 bpp (UI framebuffer), low nibble = even x
    uint8_t *hot;             // plan nibbles
    uint16_t *cold;           // per-pixel state words
    const uint8_t *tau;       // 64x64 blue-noise thresholds 0..15
    uint8_t *tile_dirty;      // set by the UI layer from invalidated areas; cleared here
    uint16_t *tile_plan;      // phase of the tile's last plan
    uint8_t *tile_nmax;       // per-tile state byte, see T_* below
    uint8_t *drive;           // out: 2 bit/px, W/4 bytes/row, 1 = black push, 2 = white push, pixel 0 in bits 7:6
    uint8_t *row_active;      // out: row has drive data this phase (else the DMA sends the zero line)
    int plan_budget;          // max pixels the planner may visit per phase (0 = unlimited); excess tiles wait
    int plan_cursor;          // tile where planning resumes when the budget ran out
    uint32_t stat_planned;    // statistics: pixels visited by the planner (accumulates)
    const uint8_t *exk_m;     // motion mode: 7 x 256 executor tables (n up to 7, no settled code)
    const uint8_t *tau_nib;   // motion mode: 64 x 32 bytes, binary thresholds packed like fb4
    uint16_t *band;           // per tile row: [2*ty] phase until which plans run, [2*ty+1] tiles with a capped plan
    int line_mode;            // 1: no drive plane; the display driver calls pqbn_v4_exec_row() per scan line
    int auto_motion;          // 1: tiles switch to motion mode and back by themselves (needs plan_budget > 0)
    int backlog;              // phases in a row that ended with UI changes the planner could not serve
    int n_motion;             // tiles currently in motion mode
} pqbn_v4_ctx;

// per-tile state byte (tile_nmax)
#define T_NMAX 0x07           // max pushes of the running plan (its tile is active while k < this)
#define T_PEND 0x08           // a UI change is waiting to be planned
#define T_HOT  0x30           // activity: UI changes arriving in quick succession (saturating count 1..3)
#define T_MOT  0x40           // tile is in motion mode
#define T_CAP  0x80           // some plan was capped at RMAX pushes: replan when it finishes
#define HOT_WINDOW 9          // a UI change within this many phases of the previous one continues the activity
#define EXIT_IDLE 12          // a motion tile without UI changes for this many phases goes back to PQ-BN

/* Tables: dec 16384 B, exk 6 x 256 B, step[4]; T0 = pushes for a full swing, s0/taud = stiction model. */
int pqbn_v4_build(uint8_t *dec, uint8_t *exk, uint8_t *step_out, float T0, float s0, float taud);
/* Motion-mode tables: exk_m 7 x 256 B, tau_nib 2048 B (from the 64x64 threshold map tau). */
void pqbn_v4_build_motion(uint8_t *exk_m, uint8_t *tau_nib, const uint8_t *tau);
/* Every pixel settled on the current framebuffer (fb4), long dwell. */
void pqbn_v4_init(pqbn_v4_ctx *C, uint32_t now);
/* One drive phase: acknowledge UI marks, plan within the budget, switch motion tiles, execute
 * (drive plane mode). Returns the drive words written (drive plane mode). */
int pqbn_v4_phase(pqbn_v4_ctx *C, uint32_t now);
/* Line mode: drive line of row y for phase now (after pqbn_v4_phase). Returns 0 for a zero line. */
PQBN_HOT int pqbn_v4_exec_row(const pqbn_v4_ctx *C, uint32_t now, int y, uint8_t *line);
/* Optional hints (auto_motion decides by itself). */
void pqbn_v4_motion_enter(pqbn_v4_ctx *C, uint32_t now, const uint8_t *sel);
void pqbn_v4_motion_exit(pqbn_v4_ctx *C, uint32_t now, const uint8_t *sel);

#ifdef __cplusplus
}
#endif
#endif
