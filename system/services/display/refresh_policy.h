/*
 * Refresh scheduling policy (pure logic, no OS calls) — unit-testable on any host.
 */
#ifndef INK_REFRESH_POLICY_H
#define INK_REFRESH_POLICY_H

#include <stdbool.h>
#include <stdint.h>

#include "ink/sys/display.h"

#define RP_MAX_PENDING 16

typedef struct {
    hal_rect_t rect;
    ink_refresh_t intent;
    uint64_t cause_us;     /* earliest user action folded into this region (0 = none) */
    const char *cause_tag; /* probe name for that action's latency (static string) or NULL */
} rp_region_t;

typedef struct {
    rp_region_t pending[RP_MAX_PENDING];
    int npending;
    int text_since_clean;
    int text_cleanup_every;
    bool force_full_next; /* after a failed update or wake */
    uint32_t waveforms;   /* panel capabilities (hal_panel_info_t.waveforms) */
    bool lcd;
    bool async;           /* per-pixel direct drive: no periodic flashing cleanups, screens change without a flash */
    int panel_w, panel_h;
} rp_state_t;

typedef struct {
    hal_rect_t rect;
    hal_waveform_t wf;
    ink_refresh_t intent;
    uint64_t cause_us;
    const char *cause_tag;
    bool cleanup; /* this dispatch clears accumulated ghosting */
} rp_dispatch_t;

void rp_init(rp_state_t *s, int panel_w, int panel_h, uint32_t waveforms, bool lcd);
/** Add a dirty region; merges with intersecting pending regions (higher intent wins). */
void rp_add(rp_state_t *s, hal_rect_t r, ink_refresh_t intent, uint64_t cause_us, const char *cause_tag);
/** Pick the next update to drive (panel idle). Returns false if nothing pending. */
bool rp_next(rp_state_t *s, rp_dispatch_t *out);
/** Report the outcome of a dispatch (failures force a full clean next). */
void rp_done(rp_state_t *s, const rp_dispatch_t *d, bool ok);
hal_waveform_t rp_waveform_for(const rp_state_t *s, ink_refresh_t intent);

#endif
