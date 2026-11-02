/*
 * UI runtime: the single task that owns LVGL, the foreground activity stack,
 * refresh-intent hints and density-independent units.
 *
 * Rules (foundation §3): exactly one task touches LVGL; apps never create
 * threads; long work is posted elsewhere and results come back with
 * ink_ui_post(). Coordinates are logical (UI orientation); sizes use dp
 * (1 dp = 1/160 inch) so layouts adapt to any panel density.
 */
#ifndef INK_SYS_UI_H
#define INK_SYS_UI_H

#include <stdbool.h>
#include <stdint.h>

#include "ink/hal_input.h"
#include "ink/sys/display.h"
#include "ink/sys/input.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *name;
    /* build the screen; `screen` is a fresh LVGL screen owned by the runtime */
    void (*create)(lv_obj_t *screen, void *arg);
    void (*destroy)(void);
    /* return true if handled */
    bool (*on_key)(hal_key_t key, bool long_press);
    bool (*on_swipe)(ink_swipe_dir_t dir);
} ink_activity_t;

/** Start the UI task with `home` as the root activity. */
int ink_ui_start(const ink_activity_t *home);
/** Open an activity on top (UI task only). The new screen is drawn with a flashing refresh. */
void ink_ui_push(const ink_activity_t *a, void *arg);
/** Open an activity from a tapped control: the control inverts at once (fast feedback),
 *  the activity is built afterwards (UI task only). */
void ink_ui_open(lv_obj_t *from, const ink_activity_t *a, void *arg);
/** Close the top activity (UI task only). */
void ink_ui_pop(void);
/** Run fn(arg) in the UI task (any task). */
void ink_ui_post(void (*fn)(void *), void *arg);

/** Refresh intent for an area that is about to change (UI task). Without a hint, changes use UI. */
void ink_ui_hint_area(const lv_area_t *logical, ink_refresh_t intent);
void ink_ui_hint_obj(lv_obj_t *obj, ink_refresh_t intent);
/** Intent for everything redrawn in the next frame that has no area hint. */
void ink_ui_hint_frame(ink_refresh_t intent);

/** dp -> pixels for the current panel. */
int ink_dp(int dp);
/** Page generation: increments when the foreground content is replaced (stale input is dropped). */
uint32_t ink_ui_generation(void);

/* Shared look for e-ink: white background, black text, inverted press state. */
void ink_ui_style_screen(lv_obj_t *scr);
void ink_ui_style_button(lv_obj_t *btn);           /* feedback = the action's result (one refresh) */
void ink_ui_style_button_pressable(lv_obj_t *btn); /* also inverts while pressed (costs an extra refresh) */
const lv_font_t *ink_ui_font(int role); /* 0 = body, 1 = title, 2 = small */

#ifdef __cplusplus
}
#endif
#endif
