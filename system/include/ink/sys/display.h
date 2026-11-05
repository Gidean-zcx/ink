/*
 * Display service: the ONLY owner of the panel (hal_display).
 *
 * Producers (the UI task) draw into the service's native framebuffer and
 * commit dirty regions with a refresh INTENT ("why it changed"); the service's
 * refresh scheduler decides when and with which waveform to drive the panel:
 * it merges regions, keeps only the newest content while the panel is busy,
 * puts touch feedback first, and schedules ghost-clearing refreshes.
 * See docs/system/foundation.md §4.1.
 */
#ifndef INK_SYS_DISPLAY_H
#define INK_SYS_DISPLAY_H

#include <stdbool.h>
#include <stdint.h>

#include "ink/hal_display.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    INK_REFRESH_A2 = 0,   /* black/white dynamic areas: keyboard, progress */
    INK_REFRESH_FAST,     /* press feedback, highlight inversion */
    INK_REFRESH_UI,       /* mixed UI (default when unsure) */
    INK_REFRESH_TEXT,     /* body text page turns (counted for ghost cleanup) */
    INK_REFRESH_FLASH_UI, /* dialog appears/disappears: flashing clean */
    INK_REFRESH_FULL,     /* full-screen clean (wake, member switch, periodic) */
    INK_REFRESH_COUNT
} ink_refresh_t;

typedef struct {
    hal_rect_t rect;      /* native panel coordinates */
    ink_refresh_t intent;
} ink_region_t;

int ink_display_init(void);
void ink_display_native_size(int *w, int *h);
/** Native 8-bit framebuffer (PSRAM). Write only between lock/unlock. */
uint8_t *ink_display_fb(void);
int ink_display_stride(void);
void ink_display_lock(void);
void ink_display_unlock(void);

/** Hand over finished regions of one frame. `cause_us` = time of the user action that
 *  caused the change (osal_now_us clock), 0 if none; used for latency probes. */
void ink_display_commit(const ink_region_t *regions, int n, uint64_t cause_us);
/** Same, with a probe name: when the frame is physically visible, `cause_tag` records the latency. */
void ink_display_commit_tagged(const ink_region_t *regions, int n, uint64_t cause_us, const char *cause_tag);
/** Page transition: show a binary preview, then refine the latest framebuffer's gray levels. */
void ink_display_commit_staged(const ink_region_t *regions, int n, uint64_t cause_us, const char *cause_tag);

/** Interactive phase (typing, menus): keep the panel powered between updates. */
void ink_display_interactive(bool on);

/** Text refreshes between automatic ghost-clearing flashes (KOReader default 6). */
void ink_display_set_text_cleanup_every(int n);

/** Frames committed since boot. */
uint32_t ink_display_commits(void);
/** Number of committed-but-not-yet-stable regions (0 = screen settled). */
int ink_display_busy(void);

/** Flash guard (storage service): while held, no new refresh starts, so an
 *  internal-flash write never freezes a playing waveform. Blocks until the
 *  panel is idle when taking it. */
void ink_display_flash_guard(bool hold);

/** Front light settings (display.light, display.warmth); after the settings table started. */
int ink_light_init(void);

#ifdef __cplusplus
}
#endif
#endif
