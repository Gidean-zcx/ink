/* tap-loss-1: taps and flash erases. A 4 KiB erase stops both cores (Puya, IDF without erase suspend); this runs
 * with the datasheet's worst erase (--flash-timing max, 240 ms). Writes keep the queue full (background work forced
 * past its quiet wait, and waiting writers), while a real finger taps an object that draws nothing (the panel stays
 * quiet, so writes may start whenever the storage service lets them):
 *   1. taps in a row, each gap shorter than the storage service's TAP_GAP (300 ms): every tap arrives once;
 *   2. the same for a short burst with waiting (foreground) writers queued;
 *   3. a press that comes during an erase and lifts after it: arrives once, at its place;
 *   4. a tap that falls whole into an erase that already started: lost (counted, the report says why). */
#include <stdlib.h>
#include "ink/sys/input.h"
#include "ink/sys/storage.h"
#include "ink/ui_theme.h"
#define ink_main product_main
#include "../configured_boot.c"
#undef ink_main
#include "../integration/whole_product.h"

static bool settled(void) { return !ink_input_touch_held() && !ink_ui_busy() && !ink_display_busy(); }

static int g_clicks, g_cx, g_cy, g_lx, g_ly;
static void clicked(lv_event_t *e) {
    g_clicks++;
    lv_point_t p; lv_indev_get_point(lv_indev_active(), &p);
    g_lx = p.x; g_ly = p.y;
    (void)e;
}
static void tp_create(lv_obj_t *screen, void *arg) {
    (void)arg;
    lv_obj_t *o = lv_obj_create(screen);
    lv_obj_remove_style_all(o); /* nothing to draw when pressed: the panel stays quiet */
    lv_obj_set_size(o, ink_ui_px(300), ink_ui_px(300));
    lv_obj_center(o);
    lv_obj_add_flag(o, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(o, clicked, LV_EVENT_CLICKED, NULL);
}
static const ink_activity_t TAP_PAGE = {.name = "TapErase", .app = "tap-erase", .create = tp_create};
static void tp_open(void) {
    ink_ui_push(&TAP_PAGE, NULL);
    lv_obj_t *o = lv_obj_get_child(lv_screen_active(), 0);
    lv_obj_update_layout(lv_screen_active());
    lv_area_t a; lv_obj_get_coords(o, &a);
    g_cx = (a.x1 + a.x2) / 2; g_cy = (a.y1 + a.y2) / 2;
}

/* --------------------------------------------------------------- writers */
static volatile int g_mode; /* 0 off, 1 background (forced by a full queue), 2 waiting writers */
static uint8_t g_blob[8192];
static volatile int g_fg_inflight;
static void fg_done(void *ctx, int r) { (void)ctx; (void)r; __atomic_sub_fetch(&g_fg_inflight, 1, __ATOMIC_ACQ_REL); }
static void writer(void *arg) {
    (void)arg;
    unsigned n = 0;
    for (;;) {
        osal_task_delay_us(20000);
        if (g_mode == 1) {
            while (ink_storage_pending() < 6) {
                char path[40]; snprintf(path, sizeof path, "/user/tap-erase/b%u.bin", n++ % 8);
                g_blob[0] = (uint8_t)n;
                if (ink_storage_write_atomic(path, g_blob, sizeof g_blob) != 0) break;
            }
        } else if (g_mode == 2) {
            while (__atomic_load_n(&g_fg_inflight, __ATOMIC_ACQUIRE) < 2) {
                char path[40]; snprintf(path, sizeof path, "/user/tap-erase/f%u.bin", n++ % 4);
                __atomic_add_fetch(&g_fg_inflight, 1, __ATOMIC_ACQ_REL);
                if (ink_storage_write_borrow(path, g_blob, sizeof g_blob, fg_done, NULL, NULL) != 0) {
                    __atomic_sub_fetch(&g_fg_inflight, 1, __ATOMIC_ACQ_REL);
                    break;
                }
            }
        }
    }
}

static double delivered(void) { return sim_metric_value("input.touch_delivery_ms"); }

/* The writes start coming with the first press (a write already under way is case 4's: nothing can catch a tap that
 * falls whole into it). The taps are timed events, as a finger is: the test task itself stops in every erase. */
static void finger_cb(void *arg);
static volatile int g_rhythm_mode;
static void rhythm_press(void *arg) { (void)arg; g_mode = g_rhythm_mode; sim_input_touch(0, true, g_cx, g_cy); }
static void rhythm(const char *what, int mode, int taps) {
    static const int intervals_ms[] = {150, 220, 300, 370}; /* press to press: gaps 70-290 ms after an 80 ms tap */
    g_mode = 0;
    CHECK(WAIT_FOR(!ink_storage_pending(), 20000), "%s: no write under way before the first tap", what);
    int clicks = g_clicks;
    double d = delivered(), ops = sim_metric_value("flash.stall_ms");
    g_rhythm_mode = mode;
    uint64_t at = SIM_MS(5), total = 0;
    vtk_lock();
    for (int i = 0; i < taps; i++) {
        vtk_event_after(at, rhythm_press, NULL);
        vtk_event_after(at + SIM_MS(80), finger_cb, NULL);
        at += SIM_MS(intervals_ms[i % 4]);
    }
    vtk_unlock();
    total = at / 1000000;
    delay_ms((int)total + 600);
    CHECK(g_clicks == clicks + taps && delivered() == d + 2 * taps, "%s: %d taps in a row with writes queued, each arrives once (%d clicks, %.0f events)",
          what, taps, g_clicks - clicks, delivered() - d);
    ink_probe_value("test.tap_erase.flash_ops_during_rhythm", sim_metric_value("flash.stall_ms") - ops);
}

/* --------------------------------------------------------------- a tap timed against an erase */
static volatile int g_watch; /* 3: press in an erase, lift after it; 4: tap whole inside one */
static uint64_t g_armed_after, g_hit_t0, g_hit_t1;
static void finger_cb(void *arg) { sim_input_touch(0, arg != NULL, g_cx, g_cy); }
static void watch_cb(void *arg) { /* ISR, every 0.5 ms */
    (void)arg;
    uint64_t t0, t1, now = vtk_isr_now();
    sim_flash_last_erase(&t0, &t1);
    if (g_watch && t0 > g_armed_after && now >= t0 + SIM_MS(5) && now + SIM_MS(45) < t1) {
        g_hit_t0 = t0; g_hit_t1 = t1;
        sim_input_touch(0, true, g_cx, g_cy);
        vtk_event_after(g_watch == 3 ? t1 - now + SIM_MS(40) : SIM_MS(30), finger_cb, NULL);
        g_watch = 0;
        return;
    }
    if (g_watch) vtk_event_after(SIM_US(500), watch_cb, NULL);
}
static bool timed_tap(int which) {
    vtk_lock();
    g_armed_after = vtk_now();
    g_hit_t0 = 0;
    g_watch = which;
    vtk_event_after(SIM_US(500), watch_cb, NULL);
    vtk_unlock();
    bool hit = WAIT_FOR(g_hit_t0 != 0, 10000);
    delay_ms(600);
    return hit;
}

static void run(void *arg) {
    (void)arg;
    CHECK(WAIT_FOR(ink_member_space_open() && settled(), 15000), "boot settles");
    test_start_home(); ink_power_hold(INK_NEED_AWAKE);
    CHECK(WAIT_FOR(settled() && !ink_storage_pending(), 15000), "boot writes done");
    on_ui(tp_open);
    CHECK(WAIT_FOR(settled(), 5000), "the test page is up");
    osal_task_cfg_t w = {.name = "tap-writer", .fn = writer, .stack_bytes = 4096, .priority = OSAL_PRIO_NORMAL, .core = -1};
    CHECK(osal_task_create(&w, NULL) == OSAL_OK, "start the writer");
    rhythm("background writes", 1, 16);
    rhythm("waiting writers", 2, 4); /* a burst shorter than the 1 s a waiting writer gives way */
    g_mode = 1; delay_ms(500);
    int clicks = g_clicks;
    double d = delivered();
    CHECK(timed_tap(3), "an erase to press into");
    CHECK(g_clicks == clicks + 1 && delivered() == d + 2 && abs(g_lx - g_cx) <= 2 && abs(g_ly - g_cy) <= 2,
          "a press during a %.0f ms erase, lifted after it, arrives once at its place (%d clicks, %.0f events, %d,%d)",
          (g_hit_t1 - g_hit_t0) / 1e6, g_clicks - clicks, delivered() - d, g_lx, g_ly);
    clicks = g_clicks; d = delivered();
    CHECK(timed_tap(4), "an erase to tap into");
    /* the controller never reported it: the read after the erase finds no contact (no lift-up frame is modelled) */
    CHECK(g_clicks == clicks && delivered() == d, "a 30 ms tap whole inside a %.0f ms erase is lost (%d clicks, %.0f events)",
          (g_hit_t1 - g_hit_t0) / 1e6, g_clicks - clicks, delivered() - d);
    ink_probe_count("test.tap_erase.lost_inside_erase");
    g_mode = 0;
    ink_probe_count("test.finished");
}

void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name = "tap-erase", .fn = run, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start the test driver");
}
