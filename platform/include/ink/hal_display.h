/*
 * ink HAL — display.
 *
 * Technology-neutral: the same system code drives an e-ink panel (ESP32-S31
 * Plus 1216x684, a PineNote-class RK3566 tablet 1872x1404) or an LCD
 * (desktop Linux). The system describes WHAT changed and WHY (refresh
 * intent); the platform's panel policy decides HOW (waveform, vsync).
 *
 * Pixel interface: 8-bit gray, 0 = black, 255 = white, in the panel's
 * native orientation. Platforms convert to their native format (4bpp for
 * EPDiy, RGB for LCD).
 */
#ifndef INK_HAL_DISPLAY_H
#define INK_HAL_DISPLAY_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    HAL_PANEL_EPD = 0, /* electrophoretic: bistable, slow, waveform-driven */
    HAL_PANEL_LCD = 1, /* emissive/transmissive: fast, continuous refresh */
} hal_panel_tech_t;

/* E-ink waveform families. Not every panel supports every family; see
 * hal_panel_info_t.waveforms. LCD panels ignore these. */
typedef enum {
    HAL_WF_GC16 = 0, /* full 16-gray, flashing: clears ghosting (slowest) */
    HAL_WF_GL16 = 1, /* 16-gray, non-flashing for white-background text */
    HAL_WF_DU = 2,   /* direct update to black/white: fast feedback */
    HAL_WF_A2 = 3,   /* 2-level, fastest: keyboards, dynamic regions */
    HAL_WF_COUNT
} hal_waveform_t;

typedef struct {
    int x, y, w, h;
} hal_rect_t;

typedef struct {
    int width, height;    /* native pixels */
    int dpi;              /* physical density, drives UI scaling */
    int gray_levels;      /* 16 for the Plus panel, 256 for gray LCD */
    bool color;           /* LCD may be color; the system still renders gray first */
    hal_panel_tech_t tech;
    uint32_t waveforms;   /* bitmask of (1u << hal_waveform_t) */
    int rotation_deg;     /* mounting rotation of native panel vs. logical UI (0/90/180/270) */
    bool full_frame_scan; /* true if any partial update still scans the whole panel (EPDiy) */
} hal_panel_info_t;

/* Timing of one update, all on the osal_now_us() clock. */
typedef struct {
    uint32_t id;
    bool ok;
    hal_waveform_t waveform; /* waveform actually used (after fallback) */
    uint64_t queued_us;      /* accepted by hal_display_update() */
    uint64_t started_us;     /* first frame driven: pixels begin to change (EPD) / scan-out starts (LCD) */
    uint64_t done_us;        /* physically stable */
} hal_display_result_t;

typedef void (*hal_display_done_fn)(void *ctx, const hal_display_result_t *r);

enum {
    HAL_UPD_FLAG_NONE = 0,
    HAL_UPD_FLAG_PRIORITY = 1u << 0, /* jump the queue (system dialogs, touch feedback) */
};

void hal_display_get_info(hal_panel_info_t *out);

/**
 * Queue an update of `area` with new pixel content.
 *  - `gray8` points to the top-left pixel of the area; `stride` is bytes per row.
 *  - The platform copies the pixels before returning (the caller may reuse its buffer).
 *  - `done` is called (from a platform task) once the area is physically stable
 *    and readable — NOT when the pixels were merely accepted.
 * Returns an update id (> 0) or 0 on failure.
 */
uint32_t hal_display_update(const hal_rect_t *area, const uint8_t *gray8, int stride,
                            hal_waveform_t wf, uint32_t flags,
                            hal_display_done_fn done, void *ctx);

/** Number of updates queued or in flight. */
int hal_display_pending(void);

/**
 * Keep the panel's high-voltage rails on for `ms` after each update (0 = off
 * right after every update, the lowest-power default). Interactive phases
 * (typing, scrolling, menus) hold power to remove the power-up delay
 * (~140 ms on the Plus) from every touch response. No-op on LCD.
 */
void hal_display_hold_power(uint32_t ms);

/** Ambient temperature hint in Celsius for waveform selection (INT32_MIN = unknown). */
void hal_display_set_temperature(int celsius);

#ifdef __cplusplus
}
#endif
#endif
