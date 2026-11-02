/*
 * Refresh scheduling policy. Rules (foundation §4.1):
 *  1. intersecting requests merge into their bounding box; the stronger intent wins
 *  2. content is read from the framebuffer at dispatch time, so while the panel is
 *     busy only the newest frame of a region is ever shown (stale frames are skipped)
 *  3. every N text refreshes one is upgraded to a flashing clean (GC16); not on per-pixel
 *     direct-drive panels (async), which converge every pixel and track its DC balance
 *  4. touch feedback (FAST/A2) is dispatched before everything else
 *  5. after a failed update the next one is a full clean
 * An EPDiy-class panel scans the whole frame for any update, so merging disjoint
 * regions into one dispatch costs no panel time (non-flashing waveforms leave
 * unchanged pixels alone).
 */
#include "refresh_policy.h"

#include <string.h>

static bool intersects(hal_rect_t a, hal_rect_t b) {
    return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h;
}

static hal_rect_t bbox(hal_rect_t a, hal_rect_t b) {
    int x0 = a.x < b.x ? a.x : b.x, y0 = a.y < b.y ? a.y : b.y;
    int x1 = a.x + a.w > b.x + b.w ? a.x + a.w : b.x + b.w;
    int y1 = a.y + a.h > b.y + b.h ? a.y + a.h : b.y + b.h;
    return (hal_rect_t){x0, y0, x1 - x0, y1 - y0};
}

static uint64_t earliest(uint64_t a, uint64_t b) {
    if (!a) return b;
    if (!b) return a;
    return a < b ? a : b;
}

/* fold cause b into a (keeps the earliest action and its probe tag) */
static void fold_cause(uint64_t *a, const char **atag, uint64_t b, const char *btag) {
    uint64_t e = earliest(*a, b);
    if (e && e == b && e != *a) *atag = btag;
    else if (!*atag && e == b) *atag = btag;
    *a = e;
}

static bool clip(const rp_state_t *s, hal_rect_t *r) {
    if (r->x < 0) { r->w += r->x; r->x = 0; }
    if (r->y < 0) { r->h += r->y; r->y = 0; }
    if (r->x + r->w > s->panel_w) r->w = s->panel_w - r->x;
    if (r->y + r->h > s->panel_h) r->h = s->panel_h - r->y;
    return r->w > 0 && r->h > 0;
}

void rp_init(rp_state_t *s, int panel_w, int panel_h, uint32_t waveforms, bool lcd) {
    memset(s, 0, sizeof *s);
    s->panel_w = panel_w;
    s->panel_h = panel_h;
    s->waveforms = waveforms;
    s->lcd = lcd;
    s->text_cleanup_every = 6;
}

static void remove_at(rp_state_t *s, int i) {
    s->pending[i] = s->pending[--s->npending];
}

void rp_add(rp_state_t *s, hal_rect_t r, ink_refresh_t intent, uint64_t cause_us, const char *cause_tag) {
    if (intent == INK_REFRESH_FULL) r = (hal_rect_t){0, 0, s->panel_w, s->panel_h};
    if (!clip(s, &r)) return;
    rp_region_t n = {r, intent, cause_us, cause_us ? cause_tag : NULL};
    bool merged;
    do {
        merged = false;
        for (int i = 0; i < s->npending; i++) {
            rp_region_t *p = &s->pending[i];
            if (!intersects(p->rect, n.rect)) continue;
            n.rect = bbox(p->rect, n.rect);
            if (p->intent > n.intent) n.intent = p->intent;
            fold_cause(&n.cause_us, &n.cause_tag, p->cause_us, p->cause_tag);
            remove_at(s, i);
            merged = true;
            break;
        }
    } while (merged);
    if (s->npending == RP_MAX_PENDING) { /* fold everything into one region */
        for (int i = 0; i < s->npending; i++) {
            n.rect = bbox(n.rect, s->pending[i].rect);
            if (s->pending[i].intent > n.intent) n.intent = s->pending[i].intent;
            fold_cause(&n.cause_us, &n.cause_tag, s->pending[i].cause_us, s->pending[i].cause_tag);
        }
        s->npending = 0;
    }
    s->pending[s->npending++] = n;
}

hal_waveform_t rp_waveform_for(const rp_state_t *s, ink_refresh_t intent) {
    if (s->lcd) return HAL_WF_GC16;
    if (s->async) /* the driver converges every pixel by itself; only FULL asks for a flashing clean */
        return intent == INK_REFRESH_FULL ? HAL_WF_GC16 : intent <= INK_REFRESH_FAST ? HAL_WF_DU : HAL_WF_GL16;
    switch (intent) {
    case INK_REFRESH_A2: return (s->waveforms & (1u << HAL_WF_A2)) ? HAL_WF_A2 : HAL_WF_DU;
    case INK_REFRESH_FAST: return HAL_WF_DU;
    case INK_REFRESH_UI:
    case INK_REFRESH_TEXT: return HAL_WF_GL16;
    default: return HAL_WF_GC16;
    }
}

bool rp_next(rp_state_t *s, rp_dispatch_t *out) {
    if (!s->npending) return false;
    memset(out, 0, sizeof *out);
    if (s->force_full_next) {
        /* a full refresh covers every pending region: take them all (their causes too) */
        s->force_full_next = false;
        while (s->npending) {
            fold_cause(&out->cause_us, &out->cause_tag, s->pending[0].cause_us, s->pending[0].cause_tag);
            remove_at(s, 0);
        }
        out->intent = INK_REFRESH_FULL;
        out->rect = (hal_rect_t){0, 0, s->panel_w, s->panel_h};
        out->wf = rp_waveform_for(s, out->intent);
        out->cleanup = true;
        s->text_since_clean = 0;
        return true;
    }
    bool have_fast = false;
    for (int i = 0; i < s->npending; i++)
        if (s->pending[i].intent <= INK_REFRESH_FAST) have_fast = true;
    bool first = true;
    for (int i = 0; i < s->npending;) {
        rp_region_t *p = &s->pending[i];
        if (have_fast && p->intent > INK_REFRESH_FAST) {
            i++;
            continue;
        }
        out->rect = first ? p->rect : bbox(out->rect, p->rect);
        if (first || p->intent > out->intent) out->intent = p->intent;
        fold_cause(&out->cause_us, &out->cause_tag, p->cause_us, p->cause_tag);
        first = false;
        remove_at(s, i);
    }
    out->wf = rp_waveform_for(s, out->intent);
    if (out->intent == INK_REFRESH_TEXT && !s->lcd && !s->async) {
        if (++s->text_since_clean >= s->text_cleanup_every && s->text_cleanup_every > 0) {
            out->wf = HAL_WF_GC16;
            out->cleanup = true;
        }
    }
    if (out->intent >= (s->async ? INK_REFRESH_FULL : INK_REFRESH_FLASH_UI)) out->cleanup = true;
    if (out->cleanup && (out->intent == INK_REFRESH_TEXT || out->intent == INK_REFRESH_FULL)) s->text_since_clean = 0;
    return true;
}

void rp_done(rp_state_t *s, const rp_dispatch_t *d, bool ok) {
    (void)d;
    if (!ok) s->force_full_next = true;
}
