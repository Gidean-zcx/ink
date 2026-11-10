#include <assert.h>
#include <stdio.h>
#include "s31_input_logic.h"

#define EVENT_CAP 64
static hal_input_event_t events[EVENT_CAP];
static unsigned count;
static void collect(const hal_input_event_t *e, void *ctx) {
    (void)ctx;
    assert(count < EVENT_CAP);
    events[count++] = *e;
}

static void keys(void) {
    s31_keys_t k = {0};
    uint32_t power = 1u << HAL_KEY_POWER;
    assert(s31_key_mask(0xFF, false) == 0);
    assert(s31_key_mask((uint8_t)~MB_P1_KEYS, true) ==
           (power | (1u << HAL_KEY_AI) | (1u << HAL_KEY_VOL_UP) | (1u << HAL_KEY_VOL_DOWN)));
    s31_keys_sample(&k, power, 1000, collect, NULL);
    s31_keys_sample(&k, 0, 10000, collect, NULL); /* bounce back */
    s31_keys_sample(&k, power, 11000, collect, NULL);
    s31_keys_sample(&k, power, 30999, collect, NULL);
    assert(count == 0);
    s31_keys_sample(&k, power, 31000, collect, NULL);
    assert(count == 1 && events[0].key.key == HAL_KEY_POWER && events[0].key.pressed && events[0].t_us == 11000);
    s31_keys_sample(&k, power, 3011000, collect, NULL); /* long hold: one edge, service owns long press */
    assert(count == 1);
    s31_keys_sample(&k, 0, 3020000, collect, NULL);
    s31_keys_sample(&k, 0, 3040000, collect, NULL);
    assert(count == 2 && !events[1].key.pressed && events[1].t_us - events[0].t_us > 3000000);
    count = 0;
    uint32_t combo = power | (1u << HAL_KEY_VOL_UP);
    s31_keys_sample(&k, combo, 4000000, collect, NULL);
    s31_keys_sample(&k, combo, 4020000, collect, NULL);
    assert(count == 2 && events[0].key.key == HAL_KEY_POWER && events[1].key.key == HAL_KEY_VOL_UP);
    assert(events[0].t_us == events[1].t_us && events[0].key.pressed && events[1].key.pressed);
    s31_keys_sample(&k, power, 4100000, collect, NULL);
    s31_keys_sample(&k, power, 4120000, collect, NULL);
    assert(count == 3 && events[2].key.key == HAL_KEY_VOL_UP && !events[2].key.pressed && k.stable == power);
}

static void touch(void) {
    count = 0;
    s31_contact_t rp[FT6336U_MAX_POINTS] = {0};
    ft6336u_point_t pts[] = {{.id = 1, .x = MT_VK_HOME_X, .y = MT_VK_Y}, {.id = 0, .x = 0, .y = 0}};
    _Static_assert(sizeof pts / sizeof pts[0] <= FT6336U_MAX_POINTS, "contact table fits");
    s31_touch_frame(rp, pts, 2, 10, collect, NULL);
    assert(count == 2 && events[0].type == HAL_IN_KEY && events[0].key.key == HAL_KEY_HOME);
    assert(events[1].touch.id == 0 && events[1].touch.phase == HAL_TOUCH_DOWN && events[1].touch.x == 0 && events[1].touch.y == 683);
    s31_touch_frame(rp, pts, 2, 20, collect, NULL);
    assert(count == 2); /* unchanged coordinates do not generate moves */
    pts[1].x = MT_PANEL_W - 1;
    pts[1].y = MT_PANEL_H - 1;
    s31_touch_frame(rp, pts, 2, 30, collect, NULL);
    assert(count == 3 && events[2].touch.phase == HAL_TOUCH_MOVE && events[2].touch.x == 1215 && events[2].touch.y == 0);
    s31_touch_frame(rp, pts, 1, 40, collect, NULL);
    assert(count == 4 && events[3].touch.id == 0 && events[3].touch.phase == HAL_TOUCH_UP && events[3].touch.x == 1215);
    s31_touch_frame(rp, pts, 0, 50, collect, NULL);
    assert(count == 5 && events[4].type == HAL_IN_KEY && !events[4].key.pressed);
    pts[0] = (ft6336u_point_t){.id = 0, .x = -1, .y = 20};
    s31_touch_frame(rp, pts, 1, 60, collect, NULL);
    assert(count == 5);
    pts[0] = (ft6336u_point_t){.id = 1, .x = 320, .y = 500};
    s31_touch_frame(rp, pts, 1, 70, collect, NULL);
    assert(count == 6 && events[5].touch.id == 1 && events[5].touch.x == 500 && events[5].touch.y == 363);
    assert(metalio_touch_cover_key(MT_VK_PREV_X + MT_VK_TOL, MT_VK_Y) == HAL_KEY_PREV);
    assert(metalio_touch_cover_key(MT_VK_NEXT_X, MT_VK_Y - MT_VK_TOL) == HAL_KEY_NEXT);
    assert(metalio_touch_cover_key(MT_VK_HOME_X + MT_VK_TOL + 1, MT_VK_Y) == HAL_KEY_NONE);
    count = 0;
    for (int i = 0; i < FT6336U_MAX_POINTS; i++) rp[i] = (s31_contact_t){0};
    pts[0] = (ft6336u_point_t){.id = 0, .x = MT_VK_NEXT_X, .y = MT_VK_Y};
    pts[1] = (ft6336u_point_t){.id = 1, .x = MT_VK_HOME_X, .y = MT_VK_Y};
    s31_touch_frame(rp, pts, 1, 100, collect, NULL);
    s31_touch_frame(rp, pts, 2, 110, collect, NULL);
    assert(count == 2 && events[0].key.key == HAL_KEY_NEXT && events[1].key.key == HAL_KEY_HOME);
    s31_touch_frame(rp, pts, 1, 120, collect, NULL);
    assert(count == 3 && events[2].key.key == HAL_KEY_HOME && !events[2].key.pressed && rp[0].down);
    s31_touch_frame(rp, pts, 0, 130, collect, NULL);
    assert(count == 4 && events[3].key.key == HAL_KEY_NEXT && !events[3].key.pressed);
}

static void wake(void) {
    uint32_t all = HAL_WAKE_POWER_KEY | HAL_WAKE_RTC | HAL_WAKE_MOTION;
    assert(s31_wake_gpio2(all, true, 0xFF, false) == HAL_WAKE_POWER_KEY); /* short press already released */
    assert(s31_wake_gpio2(all, true, (uint8_t)~MB_P1_RTC_INT, false) == HAL_WAKE_RTC);
    assert(s31_wake_gpio2(all, true, 0xFF, true) == HAL_WAKE_MOTION);
    assert(s31_wake_gpio2(all, true, (uint8_t)~(MB_P1_RTC_INT | MB_P1_POWER_KEY), true) == all);
    assert(s31_wake_gpio2(all, false, 0xFF, false) == HAL_WAKE_POWER_KEY);
    assert(s31_wake_gpio2(HAL_WAKE_MOTION, true, 0xFF, true) == HAL_WAKE_MOTION);
    assert(s31_wake_gpio2(HAL_WAKE_RTC, true, 0xFF, false) == 0); /* no fabricated RTC cause */
    assert(s31_wake_gpio2(0, true, 0, true) == 0);
}

int main(void) {
    keys();
    touch();
    wake();
    puts("s31 input: debounce, long-hold edges, chord, touch and wake mapping passed");
    return 0;
}
