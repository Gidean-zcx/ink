/*
 * Input service (runs inside the platform's input callback and the timer
 * service; no task of its own, so it costs no stack).
 */
#include <stdlib.h>
#include <string.h>

#include "ink/hal_display.h"
#include "ink/osal.h"
#include "ink/rotation.h"
#include "ink/sys/input.h"

#define SWIPE_MIN_MM 6
#define SWIPE_MAX_MS 900
#define SLOP_MM 3
#define LONGPRESS_MS 600
#define KEY_LONG_MS 800

static struct {
    ink_input_handler_t handler;
    void *ctx;
    hal_panel_info_t panel;
    int px_per_mm10; /* px per 10 mm */
    /* pointer */
    bool down, moved, long_fired;
    int x0, y0, x, y;
    uint64_t t0;
    osal_timer_t *long_timer;
    /* keys */
    hal_key_t key_down;
    bool key_long_fired;
    uint64_t key_t0;
    osal_timer_t *key_timer;
} IS;

static int mm(int v) { return v * IS.px_per_mm10 / 10; }

static void emit(ink_input_event_t *e) {
    if (IS.handler) IS.handler(e, IS.ctx);
}

static void long_cb(void *arg) {
    (void)arg;
    if (!IS.down || IS.moved) return;
    IS.long_fired = true;
    ink_input_event_t e = {.kind = INK_EV_LONGPRESS, .x = (int16_t)IS.x, .y = (int16_t)IS.y, .t_us = IS.t0};
    emit(&e);
}

static void key_long_cb(void *arg) {
    (void)arg;
    if (IS.key_down == HAL_KEY_NONE) return;
    IS.key_long_fired = true;
    ink_input_event_t e = {.kind = INK_EV_KEY_LONG, .key = (uint8_t)IS.key_down, .pressed = true, .t_us = IS.key_t0};
    emit(&e);
}

static void on_touch(const hal_input_event_t *h) {
    int lx, ly;
    ink_native_to_logical(IS.panel.width, IS.panel.height, IS.panel.rotation_deg, h->touch.x, h->touch.y, &lx, &ly);
    ink_input_event_t e = {.kind = INK_EV_POINTER, .x = (int16_t)lx, .y = (int16_t)ly, .t_us = h->t_us};
    switch (h->touch.phase) {
    case HAL_TOUCH_DOWN:
        IS.down = true;
        IS.moved = IS.long_fired = false;
        IS.x0 = IS.x = lx;
        IS.y0 = IS.y = ly;
        IS.t0 = h->t_us;
        e.phase = INK_PTR_PRESS;
        emit(&e);
        if (IS.long_timer) osal_timer_start(IS.long_timer, LONGPRESS_MS * 1000ull, 0);
        break;
    case HAL_TOUCH_MOVE:
        IS.x = lx;
        IS.y = ly;
        if (abs(lx - IS.x0) > mm(SLOP_MM) || abs(ly - IS.y0) > mm(SLOP_MM)) IS.moved = true;
        e.phase = INK_PTR_MOVE;
        emit(&e);
        break;
    case HAL_TOUCH_UP: {
        IS.down = false;
        if (IS.long_timer) osal_timer_stop(IS.long_timer);
        int dx = IS.x - IS.x0, dy = IS.y - IS.y0;
        uint64_t dt_ms = (h->t_us - IS.t0) / 1000;
        bool swipe = !IS.long_fired && dt_ms <= SWIPE_MAX_MS && (abs(dx) >= mm(SWIPE_MIN_MM) || abs(dy) >= mm(SWIPE_MIN_MM));
        e.phase = INK_PTR_RELEASE;
        e.x = (int16_t)IS.x;
        e.y = (int16_t)IS.y;
        if (IS.long_fired) e.flags |= INK_EVF_AFTER_LONG;
        if (swipe) e.flags |= INK_EVF_SWIPE; /* the UI must not turn this release into a click */
        emit(&e);
        if (swipe) {
            ink_input_event_t s = {.kind = INK_EV_SWIPE, .x = (int16_t)IS.x0, .y = (int16_t)IS.y0, .dx = (int16_t)dx,
                                   .dy = (int16_t)dy, .t_us = h->t_us};
            if (abs(dx) >= abs(dy)) s.dir = dx < 0 ? INK_SWIPE_LEFT : INK_SWIPE_RIGHT;
            else s.dir = dy < 0 ? INK_SWIPE_UP : INK_SWIPE_DOWN;
            emit(&s);
        }
        break;
    }
    }
}

static void on_key(const hal_input_event_t *h) {
    ink_input_event_t e = {.kind = INK_EV_KEY, .key = (uint8_t)h->key.key, .pressed = h->key.pressed, .t_us = h->t_us};
    if (h->key.pressed) {
        IS.key_down = h->key.key;
        IS.key_long_fired = false;
        IS.key_t0 = h->t_us;
        if (IS.key_timer) osal_timer_start(IS.key_timer, KEY_LONG_MS * 1000ull, 0);
    } else {
        if (IS.key_timer) osal_timer_stop(IS.key_timer);
        IS.key_down = HAL_KEY_NONE;
        if (IS.key_long_fired) e.flags |= INK_EVF_AFTER_LONG;
    }
    emit(&e);
}

static void hal_sink(const hal_input_event_t *h, void *ctx) {
    (void)ctx;
    if (h->type == HAL_IN_TOUCH) on_touch(h);
    else if (h->type == HAL_IN_KEY) on_key(h);
}

int ink_input_init(ink_input_handler_t handler, void *ctx) {
    IS.handler = handler;
    IS.ctx = ctx;
    hal_display_get_info(&IS.panel);
    IS.px_per_mm10 = IS.panel.dpi * 100 / 254;
    if (IS.px_per_mm10 < 10) IS.px_per_mm10 = 10;
    const char *prev = osal_mem_tag("input-service");
    IS.long_timer = osal_timer_create("longpress", long_cb, NULL);
    IS.key_timer = osal_timer_create("keylong", key_long_cb, NULL);
    osal_mem_tag(prev);
    hal_input_set_sink(hal_sink, NULL);
    return (IS.long_timer && IS.key_timer) ? 0 : -1;
}
