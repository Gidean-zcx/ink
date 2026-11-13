/*
 * Input service: turns raw HAL events (native panel coordinates) into logical
 * pointer events, swipes, long presses and key events with physical-time
 * stamps. Thresholds are in millimetres, so gestures feel the same on every
 * panel density.
 */
#ifndef INK_SYS_INPUT_H
#define INK_SYS_INPUT_H

#include <stdbool.h>
#include <stdint.h>

#include "ink/hal_input.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    INK_EV_POINTER = 1, /* press / move / release, for the UI toolkit */
    INK_EV_SWIPE,       /* emitted on release when the finger travelled far enough */
    INK_EV_LONGPRESS,   /* finger held still */
    INK_EV_KEY,         /* key press / release */
    INK_EV_KEY_LONG,    /* key held */
} ink_input_kind_t;

typedef enum { INK_PTR_PRESS = 0, INK_PTR_MOVE, INK_PTR_RELEASE } ink_ptr_phase_t;
typedef enum { INK_SWIPE_LEFT = 0, INK_SWIPE_RIGHT, INK_SWIPE_UP, INK_SWIPE_DOWN } ink_swipe_dir_t;

enum {
    INK_EVF_AFTER_LONG = 1, /* release that follows a long press / long key press */
    INK_EVF_SWIPE = 2,      /* release that completes a swipe (an INK_EV_SWIPE follows): not a click */
};

typedef struct {
    uint8_t kind;   /* ink_input_kind_t */
    uint8_t flags;  /* INK_EVF_* */
    uint8_t phase;  /* ink_ptr_phase_t */
    uint8_t dir;    /* ink_swipe_dir_t */
    uint8_t key;    /* hal_key_t */
    bool pressed;
    int16_t x, y;   /* logical coordinates */
    int16_t dx, dy; /* swipe displacement */
    uint64_t t_us;  /* physical time of the action */
} ink_input_event_t;

/* Called from platform contexts (input driver, timer service): must only post. */
typedef void (*ink_input_handler_t)(const ink_input_event_t *e, void *ctx);

int ink_input_init(ink_input_handler_t handler, void *ctx);
/* The UI publishes clipped, front-to-back hit regions; input never calls LVGL. */
#define INK_INPUT_HAPTIC_ZONES 96
typedef struct { int16_t x1, y1, x2, y2; bool enabled; } ink_input_zone_t;
void ink_input_set_zones(const ink_input_zone_t *zones, int count);
/** The power key's short press may be for something on screen first (an interruption): true = used. */
void ink_input_set_power_key_hook(bool (*fn)(void));
/** The power key held down long enough (the timer task, keep it short): the power menu, unless something on screen cannot be
 *  left. Its release does nothing more. */
void ink_input_set_power_long_hook(void (*fn)(void));
/** A finger or a key is down now (any task). */
bool ink_input_touch_held(void);
/** When the last finger or key was let go (osal_now_us), 0 if never (any task). */
uint64_t ink_input_last_release_us(void);
/** A volume key went down: -1 or 1. Locked as well; the keys go nowhere else. The input task: short. */
void ink_input_set_volume_key_hook(void (*fn)(int delta, bool locked));

#ifdef __cplusplus
}
#endif
#endif
