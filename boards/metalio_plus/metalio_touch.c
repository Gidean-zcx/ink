#include "metalio_plus/metalio_touch.h"

static bool near(int v, int c) { return v >= c - MT_VK_TOL && v <= c + MT_VK_TOL; }

hal_key_t metalio_touch_cover_key(int raw_x, int raw_y) {
    if (!near(raw_y, MT_VK_Y)) return HAL_KEY_NONE;
    if (near(raw_x, MT_VK_HOME_X)) return HAL_KEY_HOME;
    if (near(raw_x, MT_VK_PREV_X)) return HAL_KEY_PREV;
    if (near(raw_x, MT_VK_NEXT_X)) return HAL_KEY_NEXT;
    return HAL_KEY_NONE;
}

bool metalio_touch_on_panel(int raw_x, int raw_y) { return raw_x >= 0 && raw_x < MT_PANEL_W && raw_y >= 0 && raw_y < MT_PANEL_H; }
