/*
 * Metalio E-Ink4 Plus touch layout (config.h @690a023): the FT6336U reports portrait coordinates
 * that match the 684x1216 UI; the cover keys are touch points below the panel at Y~1600, X 100
 * (HOME), 200 (back / previous page) and 300 (next page), +-30.
 */
#ifndef INK_METALIO_TOUCH_H
#define INK_METALIO_TOUCH_H

#include <stdbool.h>

#include "ink/hal_input.h"

#define MT_PANEL_W 684
#define MT_PANEL_H 1216
#define MT_VK_Y 1600
#define MT_VK_TOL 30
#define MT_VK_HOME_X 100
#define MT_VK_PREV_X 200
#define MT_VK_NEXT_X 300

/** HAL_KEY_HOME / PREV / NEXT for a cover-key point, HAL_KEY_NONE otherwise. */
hal_key_t metalio_touch_cover_key(int raw_x, int raw_y);
/** A point on the panel (a finger), as opposed to the cover area. */
bool metalio_touch_on_panel(int raw_x, int raw_y);

#endif
