/*
 * ink HAL — input. The platform delivers raw events into a queue owned by
 * the input service; gesture recognition lives in the (portable) service.
 */
#ifndef INK_HAL_INPUT_H
#define INK_HAL_INPUT_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    HAL_IN_TOUCH = 1,
    HAL_IN_KEY = 2,
} hal_input_type_t;

typedef enum {
    HAL_TOUCH_DOWN = 0,
    HAL_TOUCH_MOVE = 1,
    HAL_TOUCH_UP = 2,
} hal_touch_phase_t;

/* Logical keys; each platform maps its physical keys onto these. */
typedef enum {
    HAL_KEY_NONE = 0,
    HAL_KEY_AI,       /* push-to-talk AI key */
    HAL_KEY_POWER,
    HAL_KEY_VOL_UP,
    HAL_KEY_VOL_DOWN,
    HAL_KEY_HOME,
    HAL_KEY_PREV,     /* page back */
    HAL_KEY_NEXT,     /* page forward */
    HAL_KEY_BACK,
    HAL_KEY_COUNT
} hal_key_t;

typedef struct {
    hal_input_type_t type;
    uint64_t t_us;          /* when the hardware sampled it (osal_now_us clock) */
    union {
        struct { hal_touch_phase_t phase; int id; int x, y; } touch; /* native panel coords */
        struct { hal_key_t key; bool pressed; } key;
    };
} hal_input_event_t;

typedef void (*hal_input_sink_fn)(const hal_input_event_t *ev, void *ctx);

/** The input service registers one sink; the platform calls it from its input task. */
void hal_input_set_sink(hal_input_sink_fn fn, void *ctx);

typedef struct {
    bool has_touch;
    int touch_points;
    uint32_t keys; /* bitmask of (1u << hal_key_t) available on this device */
} hal_input_info_t;

void hal_input_get_info(hal_input_info_t *out);

#ifdef __cplusplus
}
#endif
#endif
