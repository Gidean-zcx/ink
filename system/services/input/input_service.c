/*
 * Input service (runs inside the platform's input callback and the timer
 * service; no task of its own, so it costs no stack).
 */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "ink/hal_display.h"
#include "ink/osal.h"
#include "ink/rotation.h"
#include "ink/sys/input.h"
#include "ink/sys/power.h"
#include "ink/sys/svc.h"

#define SWIPE_MIN_MM 6
#define SWIPE_MAX_MS 900
#define SLOP_MM 3
#define LONGPRESS_MS 600
#define KEY_LONG_MS 800
#define POWER_LONG_MS 1500
/* Touch and key edges almost always lead to a refresh (feedback first), so
 * start the panel power-up right away; the UI renders meanwhile. */
#define WAKE_HOLD_MS 1000

static struct {
    ink_input_handler_t handler;
    void *ctx;
    hal_panel_info_t panel;
    int px_per_mm10; /* px per 10 mm */
    /* The platform input task and the timer service task both touch the state below:
     * everything is read and written under `lock`; events are emitted after unlocking. */
    osal_mutex_t *lock;
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
    hal_key_t swallow_release; /* the release of a key press the power policy consumed */
    bool (*power_key_hook)(void);
    void (*power_long_hook)(void);
    void (*volume_key_hook)(int delta, bool locked);
    bool power_down, power_long_fired;
    ink_svc_deadline_t power_at;
} IS;

static int mm(int v) { return v * IS.px_per_mm10 / 10; }

static void emit(ink_input_event_t *e) {
    if (IS.handler) IS.handler(e, IS.ctx);
}

static void lock(void) { osal_mutex_lock(IS.lock, OSAL_WAIT_FOREVER); }
static void unlock(void) { osal_mutex_unlock(IS.lock); }

/* A timer callback can be on its way while a new press re-arms the timer: it only acts if the
 * CURRENT press has really been held long enough. */
static void long_cb(void *arg) {
    (void)arg;
    lock();
    bool fire = IS.down && !IS.moved && !IS.long_fired && osal_now_us() >= IS.t0 + LONGPRESS_MS * 1000ull;
    if (fire) IS.long_fired = true;
    ink_input_event_t e = {.kind = INK_EV_LONGPRESS, .x = (int16_t)IS.x, .y = (int16_t)IS.y, .t_us = IS.t0};
    unlock();
    if (fire) emit(&e);
}

static void key_long_cb(void *arg) {
    (void)arg;
    lock();
    bool fire = IS.key_down != HAL_KEY_NONE && !IS.key_long_fired && osal_now_us() >= IS.key_t0 + KEY_LONG_MS * 1000ull;
    if (fire) IS.key_long_fired = true;
    ink_input_event_t e = {.kind = INK_EV_KEY_LONG, .key = (uint8_t)IS.key_down, .pressed = true, .t_us = IS.key_t0};
    unlock();
    if (fire) emit(&e);
}

static void on_touch(const hal_input_event_t *h) {
    int lx, ly;
    ink_native_to_logical(IS.panel.width, IS.panel.height, IS.panel.rotation_deg, h->touch.x, h->touch.y, &lx, &ly);
    ink_input_event_t e = {.kind = INK_EV_POINTER, .x = (int16_t)lx, .y = (int16_t)ly, .t_us = h->t_us};
    ink_input_event_t s = {0};
    bool swipe = false;
    lock();
    switch (h->touch.phase) {
    case HAL_TOUCH_DOWN:
        IS.down = true;
        IS.moved = IS.long_fired = false;
        IS.x0 = IS.x = lx;
        IS.y0 = IS.y = ly;
        IS.t0 = h->t_us;
        e.phase = INK_PTR_PRESS;
        if (IS.long_timer) osal_timer_start(IS.long_timer, LONGPRESS_MS * 1000ull, 0);
        break;
    case HAL_TOUCH_MOVE:
        IS.x = lx;
        IS.y = ly;
        if (abs(lx - IS.x0) > mm(SLOP_MM) || abs(ly - IS.y0) > mm(SLOP_MM)) IS.moved = true;
        e.phase = INK_PTR_MOVE;
        break;
    case HAL_TOUCH_UP: {
        IS.down = false;
        if (IS.long_timer) osal_timer_stop(IS.long_timer);
        int dx = IS.x - IS.x0, dy = IS.y - IS.y0;
        uint64_t dt_ms = (h->t_us - IS.t0) / 1000;
        swipe = !IS.long_fired && dt_ms <= SWIPE_MAX_MS && (abs(dx) >= mm(SWIPE_MIN_MM) || abs(dy) >= mm(SWIPE_MIN_MM));
        e.phase = INK_PTR_RELEASE;
        e.x = (int16_t)IS.x;
        e.y = (int16_t)IS.y;
        if (IS.long_fired) e.flags |= INK_EVF_AFTER_LONG;
        if (swipe) {
            e.flags |= INK_EVF_SWIPE; /* the UI must not turn this release into a click */
            s = (ink_input_event_t){.kind = INK_EV_SWIPE, .x = (int16_t)IS.x0, .y = (int16_t)IS.y0, .dx = (int16_t)dx,
                                    .dy = (int16_t)dy, .t_us = h->t_us};
            if (abs(dx) >= abs(dy)) s.dir = dx < 0 ? INK_SWIPE_LEFT : INK_SWIPE_RIGHT;
            else s.dir = dy < 0 ? INK_SWIPE_UP : INK_SWIPE_DOWN;
        }
        break;
    }
    }
    unlock();
    emit(&e);
    if (swipe) emit(&s);
}

static void on_key(const hal_input_event_t *h) {
    ink_input_event_t e = {.kind = INK_EV_KEY, .key = (uint8_t)h->key.key, .pressed = h->key.pressed, .t_us = h->t_us};
    lock();
    if (h->key.pressed) {
        IS.key_down = h->key.key;
        IS.key_long_fired = false;
        IS.key_t0 = h->t_us;
        if (IS.key_timer) osal_timer_start(IS.key_timer, KEY_LONG_MS * 1000ull, 0);
    } else if (h->key.key == IS.key_down) { /* releasing another key (overlapping presses) keeps this one */
        if (IS.key_timer) osal_timer_stop(IS.key_timer);
        IS.key_down = HAL_KEY_NONE;
        if (IS.key_long_fired) e.flags |= INK_EVF_AFTER_LONG;
    }
    unlock();
    emit(&e);
}

static void power_long_due(void *arg) { /* the timer task */
    (void)arg;
    lock();
    bool fire = IS.power_down;
    if (fire) IS.power_long_fired = true;
    unlock();
    if (fire && IS.power_long_hook) IS.power_long_hook();
}

/* Power policy first: while locked only a wake key gets through (it unlocks); the power key locks on
 * release, held down it is the power menu (locked too: it wakes, then that). Returns true if the event was consumed. */
static bool power_filter(const hal_input_event_t *h) {
    bool is_key = h->type == HAL_IN_KEY;
    if (is_key && h->key.key == HAL_KEY_POWER) {
        lock();
        bool was_long = IS.power_long_fired;
        IS.power_down = h->key.pressed;
        IS.power_long_fired = false;
        unlock();
        if (h->key.pressed) ink_svc_at(&IS.power_at, osal_now_us() + POWER_LONG_MS * 1000ull, power_long_due, NULL);
        else {
            ink_svc_cancel(&IS.power_at);
            if (was_long) {
                IS.swallow_release = HAL_KEY_NONE;
                return true;
            }
        }
    }
    if (is_key && !h->key.pressed && IS.swallow_release == h->key.key) {
        IS.swallow_release = HAL_KEY_NONE;
        return true;
    }
    if (is_key && (h->key.key == HAL_KEY_VOL_UP || h->key.key == HAL_KEY_VOL_DOWN)) { /* only the volume (D-19), and they wake nothing */
        bool locked = ink_power_state() == INK_PWR_LOCKED;
        if (h->key.pressed && IS.volume_key_hook) IS.volume_key_hook(h->key.key == HAL_KEY_VOL_UP ? 1 : -1, locked);
        if (!locked) ink_power_activity(h->t_us);
        return true;
    }
    if (ink_power_state() == INK_PWR_LOCKED) {
        if (is_key && h->key.pressed && (h->key.key == HAL_KEY_POWER || h->key.key == HAL_KEY_AI)) {
            hal_display_wake(WAKE_HOLD_MS); /* the lock mark comes off right away */
            IS.swallow_release = h->key.key;
            ink_power_unlock(h->t_us);
        }
        return true;
    }
    if (is_key && h->key.key == HAL_KEY_POWER) {
        if (!h->key.pressed && !(IS.power_key_hook && IS.power_key_hook())) ink_power_lock();
        return true;
    }
    ink_power_activity(h->t_us);
    return false;
}

static void hal_sink(const hal_input_event_t *h, void *ctx) {
    (void)ctx;
    if (power_filter(h)) return;
    if ((h->type == HAL_IN_TOUCH && h->touch.phase != HAL_TOUCH_MOVE) || h->type == HAL_IN_KEY)
        hal_display_wake(WAKE_HOLD_MS);
    if (h->type == HAL_IN_TOUCH) on_touch(h);
    else if (h->type == HAL_IN_KEY) on_key(h);
}

void ink_input_set_power_key_hook(bool (*fn)(void)) { IS.power_key_hook = fn; }
void ink_input_set_power_long_hook(void (*fn)(void)) { IS.power_long_hook = fn; }
void ink_input_set_volume_key_hook(void (*fn)(int delta, bool locked)) { IS.volume_key_hook = fn; }

int ink_input_init(ink_input_handler_t handler, void *ctx) {
    IS.handler = handler;
    IS.ctx = ctx;
    hal_display_get_info(&IS.panel);
    IS.px_per_mm10 = IS.panel.dpi * 100 / 254;
    if (IS.px_per_mm10 < 10) IS.px_per_mm10 = 10;
    const char *prev = osal_mem_tag("input-service");
    IS.lock = osal_mutex_create();
    IS.long_timer = osal_timer_create("longpress", long_cb, NULL);
    IS.key_timer = osal_timer_create("keylong", key_long_cb, NULL);
    osal_mem_tag(prev);
    if (!IS.lock || !IS.long_timer || !IS.key_timer) return -1;
    hal_input_set_sink(hal_sink, NULL);
    return 0;
}
