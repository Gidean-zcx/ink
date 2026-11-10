/* Pure input and wake mapping; the host tests the same code used by the port. */
#ifndef INK_S31_INPUT_LOGIC_H
#define INK_S31_INPUT_LOGIC_H

#include "ft6336u/ft6336u.h"
#include "ink/hal_input.h"
#include "ink/hal_power.h"
#include "ink/rotation.h"
#include "metalio_plus/metalio_board.h"
#include "metalio_plus/metalio_touch.h"
#include "s31_board.h"

typedef struct {
    uint32_t stable, candidate;
    uint64_t since;
    bool pending;
} s31_keys_t;

static inline uint32_t s31_key_mask(uint8_t p1, bool ai) {
    return (!(p1 & MB_P1_POWER_KEY) ? 1u << HAL_KEY_POWER : 0) |
           (!(p1 & MB_P1_VOL_UP) ? 1u << HAL_KEY_VOL_UP : 0) |
           (!(p1 & MB_P1_VOL_DOWN) ? 1u << HAL_KEY_VOL_DOWN : 0) |
           (ai ? 1u << HAL_KEY_AI : 0);
}

static inline void s31_keys_report(s31_keys_t *k, uint32_t mask, uint64_t t, hal_input_sink_fn emit, void *ctx) {
    uint32_t changed = mask ^ k->stable;
    k->stable = mask;
    for (int key = HAL_KEY_AI; key < HAL_KEY_COUNT; key++) {
        if (!(changed & (1u << key))) continue;
        hal_input_event_t e = {.type = HAL_IN_KEY, .t_us = t};
        e.key.key = (hal_key_t)key;
        e.key.pressed = (mask & (1u << key)) != 0;
        emit(&e, ctx);
    }
}

static inline void s31_keys_sample(s31_keys_t *k, uint32_t mask, uint64_t now, hal_input_sink_fn emit, void *ctx) {
    if (mask != k->candidate) {
        k->candidate = mask;
        k->since = now;
    }
    k->pending = k->candidate != k->stable;
    if (k->pending && now - k->since >= S31_KEY_DEBOUNCE_US) {
        s31_keys_report(k, mask, k->since, emit, ctx);
        k->pending = false;
    }
}

typedef struct {
    bool down;
    hal_key_t key;
    int x, y;
} s31_contact_t;

static inline void s31_touch_emit(int id, const s31_contact_t *p, hal_touch_phase_t phase, uint64_t t,
                                  hal_input_sink_fn emit, void *ctx) {
    hal_input_event_t e = {.type = p->key ? HAL_IN_KEY : HAL_IN_TOUCH, .t_us = t};
    if (p->key) {
        e.key.key = p->key;
        e.key.pressed = phase != HAL_TOUCH_UP;
    } else {
        e.touch.id = id;
        e.touch.phase = phase;
        ink_logical_to_native(MT_PANEL_H, MT_PANEL_W, S31_PANEL_ROTATION, p->x, p->y, &e.touch.x, &e.touch.y);
    }
    emit(&e, ctx);
}

static inline void s31_touch_frame(s31_contact_t *rp, const ft6336u_point_t *pts, int n, uint64_t t,
                                   hal_input_sink_fn emit, void *ctx) {
    bool seen[FT6336U_MAX_POINTS] = {false};
    for (int i = 0; i < n; i++)
        if (pts[i].id >= 0 && pts[i].id < FT6336U_MAX_POINTS) seen[pts[i].id] = true;
    for (int id = 0; id < FT6336U_MAX_POINTS; id++) {
        if (!rp[id].down || seen[id]) continue;
        s31_touch_emit(id, &rp[id], HAL_TOUCH_UP, t, emit, ctx);
        rp[id].down = false;
    }
    for (int i = 0; i < n; i++) {
        int id = pts[i].id;
        if (id < 0 || id >= FT6336U_MAX_POINTS) continue;
        s31_contact_t *p = &rp[id];
        hal_key_t key = metalio_touch_cover_key(pts[i].x, pts[i].y);
        bool panel = metalio_touch_on_panel(pts[i].x, pts[i].y);
        if (!p->down && (key || panel)) {
            *p = (s31_contact_t){.down = true, .key = key, .x = pts[i].x, .y = pts[i].y};
            s31_touch_emit(id, p, HAL_TOUCH_DOWN, t, emit, ctx);
        } else if (p->down && !p->key && panel && (p->x != pts[i].x || p->y != pts[i].y)) {
            p->x = pts[i].x;
            p->y = pts[i].y;
            s31_touch_emit(id, p, HAL_TOUCH_MOVE, t, emit, ctx);
        }
    }
}

static inline uint32_t s31_wake_gpio2(uint32_t armed, bool read_ok, uint8_t p1, bool moved) {
    uint32_t why = 0;
    if ((armed & HAL_WAKE_RTC) && read_ok && MB_RTC_PENDING(p1)) why |= HAL_WAKE_RTC;
    if ((armed & HAL_WAKE_MOTION) && moved) why |= HAL_WAKE_MOTION;
    if ((armed & HAL_WAKE_POWER_KEY) && (!read_ok || !(p1 & MB_P1_POWER_KEY) || !why)) why |= HAL_WAKE_POWER_KEY;
    return why;
}

#endif
