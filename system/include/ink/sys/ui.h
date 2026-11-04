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

/* Keys, as apps see them (the same values as the HAL's). */
typedef hal_key_t ink_key_t;
#define INK_KEY_AI HAL_KEY_AI
#define INK_KEY_POWER HAL_KEY_POWER
#define INK_KEY_VOL_UP HAL_KEY_VOL_UP
#define INK_KEY_VOL_DOWN HAL_KEY_VOL_DOWN
#define INK_KEY_HOME HAL_KEY_HOME
#define INK_KEY_PREV HAL_KEY_PREV
#define INK_KEY_NEXT HAL_KEY_NEXT
#define INK_KEY_BACK HAL_KEY_BACK

typedef struct {
    const char *name;
    /* build the screen; `screen` is a fresh LVGL screen owned by the runtime */
    void (*create)(lv_obj_t *screen, void *arg);
    void (*destroy)(void);
    /* return true if handled */
    bool (*on_key)(ink_key_t key, bool long_press);
    bool (*on_swipe)(ink_swipe_dir_t dir);
    /* page context for the AI (G-002): plain text of what is on screen; returns bytes written */
    int (*snapshot)(char *buf, size_t cap);
    /* optional: the queued input (and the call just handled) is done and the frame is drawn next:
     * expensive drawing deferred by the handlers happens here, once for the whole batch (e.g. four
     * page turns in a row render the fourth page only) */
    void (*settle)(void);
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
/** Back to the root activity (one rebuild). */
void ink_ui_home(void);
/** Run fn(arg) in the UI task (any task). */
void ink_ui_post(void (*fn)(void *), void *arg);
/** The UI task has been started (ink_ui_post may be used). */
bool ink_ui_started(void);
/** The calling task is the UI task. */
bool ink_ui_is_current(void);
/** The UI has work that may still change the panel: posted calls or input not handled yet, or a
 *  requested LVGL refresh not rendered yet (any task; the power service waits for false before
 *  it sleeps the chip). A rendered frame is reported by ink_display_busy() instead. */
bool ink_ui_busy(void);
/** Messages (input or posted calls) are waiting for the UI task: background work in the UI task
 *  (rendering the next page ahead) waits for them (UI task). */
bool ink_ui_queue_pending(void);

/** Refresh intent for an area that is about to change (UI task). Without a hint, changes use UI. */
void ink_ui_hint_area(const lv_area_t *logical, ink_refresh_t intent);
void ink_ui_hint_obj(lv_obj_t *obj, ink_refresh_t intent);
/** Intent for everything redrawn in the next frame that has no area hint. */
void ink_ui_hint_frame(ink_refresh_t intent);

/** Attribute the next frame to an earlier action: when it is visible, `tag` (static string)
 *  records the latency from `t_us` (e.g. AI key release -> first answer text). */
void ink_ui_cause(uint64_t t_us, const char *tag);
/** Page context of the foreground activity (see ink_activity_t.snapshot). */
int ink_ui_snapshot(char *buf, size_t cap, const char **activity_name);
/** Name of the foreground activity ("" if none). */
const char *ink_ui_top_name(void);

/** dp -> pixels for the current panel. */
int ink_dp(int dp);
/** Page generation: increments when the foreground content is replaced (stale input is dropped). */
uint32_t ink_ui_generation(void);
/** Content version: what a request about the screen was prepared against. It changes when the
 *  foreground content changes, a new screen or a turned page (the page generation above only
 *  changes with a new screen: it drops input aimed at an old one, and a page turn must not).
 *  A tool call prepared for another version is refused as stale; receipts carry the new one. */
uint32_t ink_ui_content_version(void);
/** The foreground activity's content changed without a new screen (a page turned). UI task. */
void ink_ui_content_changed(void);

/* Shared look for e-ink: white background, black text, inverted press state. */
void ink_ui_style_screen(lv_obj_t *scr);
void ink_ui_style_button(lv_obj_t *btn);           /* feedback = the action's result (one refresh) */
void ink_ui_style_button_pressable(lv_obj_t *btn); /* also inverts while pressed (costs an extra refresh) */
const lv_font_t *ink_ui_font(int role); /* 0 = body, 1 = title, 2 = small */

#ifdef __cplusplus
}
#endif
#endif
