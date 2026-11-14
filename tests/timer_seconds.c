/* Real clock controls, seconds, deadlines and the panel update boundary. */
#include "ink/hal_display.h"
#include "ink/ui_shell.h"
#include "ink/rotation.h"
#include "src/core/lv_obj_draw_private.h"
static bool tracking;
static hal_rect_t digits_area;
static unsigned updates, outside, slow;
static hal_rect_t bad_update, bad_area;
static uint32_t checked_update(const hal_rect_t *r, const uint8_t *px, int stride, hal_waveform_t wf,
                               uint32_t flags, hal_display_done_fn done, void *ctx) {
    if (tracking) {
        updates++;
        if (r->x < digits_area.x || r->y < digits_area.y || r->x + r->w > digits_area.x + digits_area.w ||
            r->y + r->h > digits_area.y + digits_area.h) { outside++; bad_update = *r; bad_area = digits_area; }
        if (wf != HAL_WF_DU && wf != HAL_WF_A2) slow++;
    }
    return hal_display_update(r, px, stride, wf, flags, done, ctx);
}
#define hal_display_update checked_update
#include "../system/services/display/display_service.c"
#undef hal_display_update
#define ink_main product_main
#include "configured_boot.c"
#undef ink_main
#include "integration/whole_product.h"
#include <stdlib.h>

static char snapshot[8192];
static char field_text[128];
static uint64_t sampled_at;
static const char *wanted;
static lv_obj_t *found;
static int px, py;
static bool large, numeric_target;
static const char *wanted_id = "";
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void quiet(void) { CHECK(WAIT_FOR(settled(), 10000), "clock frame settles"); }
/* A turning minute redraws the status bar's time and the lock screen's clock (lock-1): count frames clear of it. */
static void clear_of_minute(void) { while (osal_wall_us() / 1000000 % 60 >= 55) delay_ms(250); quiet(); }
static void read_page(void) {
    sampled_at = osal_now_us();
    CHECK(ink_ui_snapshot(snapshot, sizeof snapshot, NULL) > 0, "clock snapshot exists");
    CHECK(ink_ui_context_snapshot(g_snap, sizeof g_snap, NULL) > 0, "clock fits AI context");
}
static int64_t number(const char *name) {
    on_ui(read_page); long long value = -1; char key[64];
    snprintf(key, sizeof key, "\"%s\":", name); const char *at = strstr(snapshot, key);
    CHECK(at && sscanf(at + strlen(key), "%lld", &value) == 1, "snapshot exports %s", name);
    return (int64_t)value;
}
static void find_label(lv_obj_t *o) {
    if (!lv_obj_is_visible(o)) return;
    if (lv_obj_check_type(o, &lv_label_class) && (numeric_target ? lv_obj_get_style_text_font(o, 0) == ink_ui_font_px(strncmp(wanted_id, "timer:", 6) ? INK_FONT_NUMBER : INK_FONT_HEADING, INK_WEIGHT_REGULAR) : !strcmp(lv_label_get_text(o), wanted))) found = o;
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) find_label(lv_obj_get_child(o, (int32_t)i));
}
static void locate(void) {
    found = NULL; lv_obj_update_layout(lv_screen_active()); find_label(lv_screen_active());
    CHECK(found, "clock label visible: %s", wanted);
    if (!found) return;
    lv_area_t a; lv_obj_get_coords(found, &a); px = (a.x1 + a.x2) / 2; py = (a.y1 + a.y2) / 2;
    CHECK(a.y1 >= 0 && a.y2 < ink_ui_height(), "clock label fits screen: %s", wanted);
    int32_t extra = lv_obj_get_ext_draw_size(found);
    lv_area_increase(&a, extra, extra);
    hal_panel_info_t panel; hal_display_get_info(&panel); int x0, y0, x1, y1;
    ink_logical_to_native(panel.width, panel.height, panel.rotation_deg, a.x1, a.y1, &x0, &y0);
    ink_logical_to_native(panel.width, panel.height, panel.rotation_deg, a.x2, a.y2, &x1, &y1);
    digits_area = (hal_rect_t){x0 < x1 ? x0 : x1, y0 < y1 ? y0 : y1, abs(x1 - x0) + 1, abs(y1 - y0) + 1};
}
static void shown(const char *value) { wanted = value; on_ui(locate); }
static void current_digits(const char *id) {
    on_ui(read_page); char key[64]; snprintf(key, sizeof key, "\"id\":\"%s\"", id);
    const char *at = strstr(snapshot, key); at = at ? strstr(at, "\"value\":\"") : NULL;
    CHECK(at && sscanf(at, "\"value\":\"%127[^\"]", field_text) == 1, "digit field exists: %s", id);
    numeric_target = !strncmp(id, "timer:", 6) || !strcmp(id, "elapsed"); wanted_id = id;
    if (at) shown(field_text);
    numeric_target = false;
}
static void ok(const char *cap, const char *args) { CHECK(AI(cap, args) && g_rc.status == INK_RC_DONE, "%s completes: %d", cap, g_rc.status); }
static void press(const char *label) {
    quiet(); shown(label);
    double delivered = sim_metric_value("input.touch_delivery_ms");
    if (found) tap(px, py);
    CHECK(WAIT_FOR(sim_metric_value("input.touch_delivery_ms") > delivered, 1000), "clock driver receives touch"); quiet();
}
static void frame(void *name) { sim_display_snapshot(name); }
static void capture(const char *name) { quiet(); vtk_event_at(vtk_now() + SIM_MS(1), frame, (void *)name); delay_ms(5); }
static void open_timer(void) { ok("system.home", NULL); ok("app.open", "{\"id\":\"clock\"}"); ok("clock.timer", NULL); quiet(); }
static void check_ticks(void) {
    updates = outside = slow = 0; tracking = true; delay_ms(3100); tracking = false;
    CHECK(updates >= 3 && updates <= 4, "one numeric update each second: %u", updates);
    CHECK(!outside && !slow, "only digits fast-refresh: outside=%u slow=%u area=%d,%d,%d,%d update=%d,%d,%d,%d", outside, slow, bad_area.x, bad_area.y, bad_area.w, bad_area.h, bad_update.x, bad_update.y, bad_update.w, bad_update.h);
}
static void timer_test(void) {
    open_timer(); char row[16], tid[16]; int64_t start_left = number("remaining_seconds");
    snprintf(row, sizeof row, "%lld:%02lld", (long long)(start_left / 60), (long long)(start_left % 60));
    press(row); press("1 分钟"); /* clock-3: the duration is set on the timer's own page */
    for (int i = 0; i < 30; i++) press("秒＋");
    press("保存"); snprintf(tid, sizeof tid, "timer:%lld", (long long)number("current_timer"));
    CHECK(number("remaining_seconds") == 90, "touch selects ninety seconds"); shown("1:30");
    capture(large ? "timer-select-120" : "timer-select-normal");
    ok("clock.timer_set", "{\"seconds\":31}"); quiet(); press("0:31"); press("秒－"); press("保存");
    CHECK(number("remaining_seconds") == 30, "touch selects thirty seconds"); shown("0:30");
    ok("clock.timer_set", "{\"seconds\":3601}"); quiet(); shown("1:00:01");
    ok("clock.timer_set", "{\"seconds\":90}"); quiet(); shown("1:30");
    uint64_t low = osal_now_us(); ok("clock.timer_start", NULL); uint64_t high = osal_now_us(); quiet(); current_digits(tid);
    check_ticks();
    CHECK(number("remaining_seconds") <= 87 && number("remaining_seconds") >= 86, "AI countdown agrees with seconds");
    capture(large ? "timer-running-120" : "timer-running-normal");
    ok("system.home", NULL); quiet();
    clear_of_minute(); uint32_t commits = ink_display_commits(); delay_ms(3100);
    CHECK(ink_display_commits() == commits, "countdown does not redraw off page");
    ok("app.open", "{\"id\":\"clock\"}"); ok("clock.timer", NULL); quiet();
    int64_t left = number("remaining_seconds");
    CHECK(left >= (int64_t)((low + 90000000 - sampled_at) / 1000000) && left <= (int64_t)((high + 90000000 - sampled_at + 999999) / 1000000), "return catches up countdown");
    key(HAL_KEY_POWER); CHECK(WAIT_FOR(ink_power_state() == INK_PWR_LOCKED, 3000), "lock countdown"); quiet();
    clear_of_minute(); commits = ink_display_commits(); delay_ms(2100);
    CHECK(ink_display_commits() == commits, "countdown does not redraw while locked");
    key(HAL_KEY_POWER); CHECK(WAIT_FOR(ink_power_state() != INK_PWR_LOCKED, 3000), "unlock countdown");
    CHECK(number("remaining_seconds") < left - 1, "unlock catches up countdown");
    if (low + 90000000 > osal_now_us()) osal_task_delay_us(low + 90000000 - osal_now_us());
    CHECK(WAIT_FOR(!strcmp(active_state(), "alarm"), 100), "ninety-second deadline rings");
    CHECK(osal_now_us() >= low + 90000000 && osal_now_us() <= high + 90100000, "ring timing agrees within 100 ms");
    ok("clock.timer_dismiss", "{\"id\":0}"); quiet();
}
static void stopwatch_test(void) {
    open_timer(); ok("shell.back", NULL); ok("clock.stopwatch", NULL); ok("clock.stopwatch_reset", NULL);
    uint64_t low = osal_now_us(); ok("clock.stopwatch_start", NULL); uint64_t high = osal_now_us();
    quiet(); current_digits("elapsed"); check_ticks();
    int64_t seconds = number("elapsed_seconds");
    CHECK(seconds >= (int64_t)((sampled_at - high) / 1000000) && seconds <= (int64_t)((sampled_at - low) / 1000000), "stopwatch exposes elapsed seconds");
    ok("clock.stopwatch_lap", NULL); quiet(); on_ui(read_page);
    current_digits("第 1 圈"); unsigned m, s, hundredth; const char *total = strstr(field_text, "累计 ");
    CHECK(total && sscanf(total + strlen("累计 "), "%u:%u.%u", &m, &s, &hundredth) == 3 && m == 0 && s >= (unsigned)seconds && s <= (unsigned)seconds + 2, "lap includes seconds");
    capture(large ? "stopwatch-120" : "stopwatch-normal");
    ok("shell.back", NULL); quiet(); clear_of_minute(); uint32_t commits = ink_display_commits(); delay_ms(2100);
    CHECK(ink_display_commits() == commits, "stopwatch does not redraw off page");
    ok("clock.stopwatch", NULL); CHECK(number("elapsed_seconds") >= 5, "stopwatch catches up on return");
    ok("clock.stopwatch_pause", NULL); quiet();
}
static void run(void *unused) {
    (void)unused; CHECK(WAIT_FOR(ink_member_space_open() && settled(), 15000), "boot settles"); test_start_home(); ink_power_hold(INK_NEED_AWAKE);
    for (int scale = 0; scale < 2; scale++) {
        large = scale != 0;
        ok("settings.set", large ? "{\"id\":\"display.font_size\",\"value\":3}" : "{\"id\":\"display.font_size\",\"value\":1}");
        timer_test(); stopwatch_test();
    }
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name = "seconds-test", .fn = run, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start seconds acceptance");
}
