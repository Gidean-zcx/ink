/* Whole-product clock checks on synthetic flash and virtual time. */
#include "ink/ui_shell.h"
#include "ink/sys/time.h"
#define ink_main product_main
#include "../system/boot/main.c"
#undef ink_main
#include "integration/whole_product.h"
#include <stdlib.h>

static char phase[16], snapshot[8192], field[128], target[64];
static ink_json_t json;
static uint64_t sampled_at;
static uint32_t version;
static bool deleted, valid;
static int ordinal, seen, tx, ty;
static void sample(void) {
    sampled_at = osal_now_us(); version = ink_ui_content_version();
    valid = ink_ui_snapshot(snapshot, sizeof snapshot, NULL) > 0;
}
static void read_page(void) {
    sample(); CHECK(valid, "page exports AI snapshot");
    CHECK(ink_ui_context_snapshot(g_snap, sizeof g_snap, NULL) > 0, "page fits production AI context");
}
static int64_t number(const char *name) {
    long long v = -1; char key[64]; on_ui(sample); snprintf(key, sizeof key, "\"%s\":", name);
    const char *p = strstr(snapshot, key);
    CHECK(valid && p && sscanf(p + strlen(key), "%lld", &v) == 1, "snapshot has %s", name); return (int64_t)v;
}
static bool running(void) { on_ui(sample); return valid && strstr(snapshot, "\"running\":true"); }
static bool field_value(const char *id) {
    on_ui(sample); field[0] = 0;
    char key[96]; snprintf(key, sizeof key, "{\"id\":\"%s\"", id);
    const char *p = strstr(snapshot, key), *end = p ? strchr(p, '}') : NULL;
    return end && ink_json_parse(&json, p, (size_t)(end - p + 1)) == 0 &&
        ink_json_str(&json, ink_json_find(&json, 0, "value"), field, sizeof field) >= 0;
}
static bool field_has(const char *id, const char *text) { return field_value(id) && strstr(field, text); }
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void quiet(void) { CHECK(WAIT_FOR(settled(), 10000), "display settles"); }
static void ok(const char *cap, const char *args) { CHECK(AI(cap, args) && g_rc.status == INK_RC_DONE, "%s done (%s)", cap, g_rc.detail); }
static void setting(const char *id, int value) {
    char args[96]; snprintf(args, sizeof args, "{\"id\":\"%s\",\"value\":%d}", id, value); ok("settings.set", args);
    CHECK(WAIT_FOR(ink_setting_get(id) == value, 3000), "setting %s applied", id);
}
static void open_page(const char *cap) { ok("system.home", NULL); ok("app.open", "{\"id\":\"clock\"}"); ok(cap, NULL); quiet(); on_ui(read_page); }
static void control_deleted(lv_event_t *e) { (void)e; deleted = true; }
static void watch_controls(lv_obj_t *o) {
    const char *cap = ink_shell_bound_cap(o);
    if (cap && !strncmp(cap, "clock.", 6)) lv_obj_add_event_cb(o, control_deleted, LV_EVENT_DELETE, NULL);
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) watch_controls(lv_obj_get_child(o, (int32_t)i));
}
static void observe(void) { deleted = false; watch_controls(lv_screen_active()); }
static void locate_at(lv_obj_t *o) {
    const char *cap = ink_shell_bound_cap(o);
    if (cap && !strcmp(cap, target) && seen++ == ordinal) {
        lv_area_t a; lv_obj_get_coords(o, &a); tx = (a.x1 + a.x2) / 2; ty = (a.y1 + a.y2) / 2;
        CHECK(a.x1 >= 0 && a.x2 < ink_ui_width() && a.y1 >= 0 && a.y2 < ink_ui_height(), "touch target fits the display");
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) locate_at(lv_obj_get_child(o, (int32_t)i));
}
static void locate(void) { tx = ty = -1; seen = 0; lv_obj_update_layout(lv_screen_active()); locate_at(lv_screen_active()); }
static void press(const char *cap, int index) {
    quiet(); snprintf(target, sizeof target, "%s", cap); ordinal = index; on_ui(locate);
    CHECK(tx >= 0 && ty >= 0, "visible touch binding: %s #%d", cap, index);
    double delivered = sim_metric_value("input.touch_delivery_ms");
    if (tx >= 0) tap(tx, ty);
    CHECK(WAIT_FOR(sim_metric_value("input.touch_delivery_ms") > delivered, 1000), "driver received the touch"); quiet();
}
static void capture(void *arg) { sim_display_snapshot(arg); }
static void check_button_labels(lv_obj_t *o) {
    if (lv_obj_check_type(o, &lv_button_class) && lv_obj_get_child_count(o)) {
        lv_obj_t *label = lv_obj_get_child(o, 0);
        if (lv_obj_check_type(label, &lv_label_class)) {
            lv_point_t size; lv_text_get_size(&size, lv_label_get_text(label), lv_obj_get_style_text_font(label, 0), 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
            CHECK(size.x <= lv_obj_get_width(label), "button text fits without ellipsis: %.80s", lv_label_get_text(label));
        }
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) check_button_labels(lv_obj_get_child(o, (int32_t)i));
}
static void check_labels(void) { lv_obj_update_layout(lv_screen_active()); check_button_labels(lv_screen_active()); }
static void evidence(const char *name) { quiet(); on_ui(check_labels); vtk_event_at(vtk_now() + SIM_MS(1), capture, (void *)name); delay_ms(5); }
static void wait_us(uint64_t duration) { uint64_t until = osal_now_us() + duration; CHECK(WAIT_FOR(osal_now_us() >= until, (int)(duration / 1000) + 100), "virtual duration passes"); }
static uint64_t shown_us(const char *id) {
    unsigned long long h = 0; unsigned m = 0, s = 0, tenth = 0;
    CHECK(field_value(id) && sscanf(field, "%llu:%u:%u.%u", &h, &m, &s, &tenth) == 4, "paused or lap value includes tenths: %.100s", field);
    return ((uint64_t)h * 3600 + m * 60 + s) * 1000000 + tenth * 100000;
}
static void in_window(uint64_t value, uint64_t low, uint64_t high, const char *what) {
    CHECK(value + 100000 >= low && value <= high + 100000, "%s agrees with virtual clock within 0.1 s: %llu in [%llu,%llu]", what,
        (unsigned long long)value, (unsigned long long)low, (unsigned long long)high);
}
static void stopwatch(void) {
    ok("app.open", "{\"id\":\"clock\"}"); quiet(); on_ui(read_page);
    CHECK(strstr(snapshot, "clock.stopwatch") && strstr(snapshot, "clock.world"), "clock home exposes both new entries");
    press("clock.stopwatch", 0); CHECK(number("elapsed_ms") == 0 && !running(), "stopwatch starts cleared");
    uint64_t begin_lo = osal_now_us(); ok("clock.stopwatch_start", NULL); uint64_t begin_hi = osal_now_us();
    CHECK(running(), "AI starts the stopwatch"); quiet(); on_ui(observe);
    CHECK(field_value("elapsed") && !strchr(field, '.'), "running display shows whole seconds"); uint32_t first_version = version;
    wait_us(2100000); uint64_t us = (uint64_t)number("elapsed_ms") * 1000;
    in_window(us, sampled_at - begin_hi, sampled_at - begin_lo, "running elapsed");
    CHECK(version - first_version >= 2 && version - first_version <= 3, "visible digits update once per second");
    CHECK(!deleted, "second ticks update only the digits, preserving controls");
    for (unsigned i = 1; i <= 3; i++) {
        wait_us(230000); uint64_t lo = osal_now_us();
        if (i == 1) press("clock.stopwatch_lap", 0); else ok("clock.stopwatch_lap", NULL);
        uint64_t hi = osal_now_us(); char id[24]; snprintf(id, sizeof id, "第 %u 次", i);
        in_window(shown_us(id), lo - begin_hi, hi - begin_lo, "lap");
    }
    uint64_t pause_lo = osal_now_us(); ok("clock.stopwatch_pause", NULL); uint64_t pause_hi = osal_now_us();
    uint64_t paused = (uint64_t)number("elapsed_ms") * 1000;
    in_window(paused, pause_lo - begin_hi, pause_hi - begin_lo, "pause");
    CHECK(!running() && number("total") == 3, "pause preserves three laps");
    CHECK(llabs((long long)shown_us("elapsed") - (long long)paused) < 100000, "paused display is accurate to one tenth");
    evidence("stopwatch-zh"); setting("system.language", 1); quiet(); on_ui(read_page);
    CHECK(strstr(snapshot, "Stopwatch") && strstr(snapshot, "Resume") && strstr(snapshot, "Lap 3"), "stopwatch is translated"); evidence("stopwatch-en");
    ok("system.home", NULL); ok("app.open", "{\"id\":\"clock\"}"); evidence("clock-home-en"); press("clock.stopwatch", 0);
    setting("system.language", 0); wait_us(1200000);
    CHECK(number("elapsed_ms") * 1000 == (int64_t)paused, "paused time does not advance");
    uint64_t resume_lo = osal_now_us(); press("clock.stopwatch_start", 0); uint64_t resume_hi = osal_now_us();
    ok("shell.back", NULL); CHECK(!strcmp(ink_ui_page_name(), "Clock"), "leave stopwatch for clock home"); wait_us(2100000);
    press("clock.stopwatch", 0); us = (uint64_t)number("elapsed_ms") * 1000;
    in_window(us - paused, sampled_at - resume_hi, sampled_at - resume_lo, "off-page elapsed");
    key(HAL_KEY_POWER); CHECK(WAIT_FOR(ink_power_state() == INK_PWR_LOCKED, 3000), "lock while stopwatch runs"); wait_us(2100000);
    key(HAL_KEY_POWER); CHECK(WAIT_FOR(ink_power_state() != INK_PWR_LOCKED, 3000), "wake stopwatch");
    us = (uint64_t)number("elapsed_ms") * 1000; in_window(us - paused, sampled_at - resume_hi, sampled_at - resume_lo, "locked elapsed");
    for (int i = 3; i < 99; i++) ok("clock.stopwatch_lap", NULL);
    CHECK(number("total") == 99, "99 laps accepted");
    CHECK(AI("clock.stopwatch_lap", NULL) && g_rc.status == INK_RC_REJECTED && number("total") == 99, "100th lap rejected without losing data");
    for (int i = 0; i < 32; i++) ok("clock.extra_next", NULL);
    CHECK(number("page_number") == 33 && field_value("第 99 次"), "all 99 laps are reachable by paging");
    ok("clock.extra_prev", NULL); CHECK(number("page_number") == 32 && field_value("第 94 次"), "lap previous page works");
    press("clock.stopwatch_reset", 0); CHECK(number("elapsed_ms") == 0 && number("total") == 0 && !running(), "reset clears time and laps");
    ok("clock.stopwatch_start", NULL); /* Deliberately running at restart. */
}
static void set_utc(int64_t utc) { char a[64]; snprintf(a, sizeof a, "{\"utc_s\":%lld}", (long long)utc); ok("time.set", a); quiet(); }
static void city(int zone) { char a[32]; snprintf(a, sizeof a, "{\"zone\":%d}", zone); ok("clock.world_add", a); }
static int64_t utc(int y, int m, int d, int h, int minute, int s) { ink_tm_t t = {.year = y, .month = m, .day = d, .hour = h, .min = minute, .sec = s}; return ink_time_from_local(&t); }
static void transition(int64_t at, const char *zone, const char *before, const char *after) {
    set_utc(at - 4); CHECK(field_has(zone, before), "before DST: %s %.100s", zone, field); on_ui(observe);
    CHECK(WAIT_FOR(field_has(zone, after), 6500), "after DST: %s %.100s", zone, field);
    CHECK(!deleted, "minute tick preserves world-clock controls");
}
static void world(void) {
    setting("time.auto", 0); setting("time.zone", 1); open_page("clock.world");
    CHECK(number("total") == 0, "world clocks initially empty"); city(7);
    CHECK(number("total") == 1, "AI adds New York through the city capability");
    press("clock.world_pick", 0); CHECK(number("total") == 9, "picker offers all nine cities");
    press("clock.extra_next", 0); press("clock.world_add", 2); /* London is the third city on page two. */
    press("clock.world_pick", 0); CHECK(number("total") == 2 && field_value("Europe/London"), "touch adds London through the same capability");
    transition(utc(2027, 3, 14, 7, 0, 0), "America/New_York", "01:59 · -5 小时", "03:00 · -4 小时");
    transition(utc(2027, 11, 7, 6, 0, 0), "America/New_York", "01:59 · -4 小时", "01:00 · -5 小时");
    transition(utc(2027, 3, 28, 1, 0, 0), "Europe/London", "00:59 · +0 小时", "02:00 · +1 小时");
    transition(utc(2027, 10, 31, 1, 0, 0), "Europe/London", "01:59 · +1 小时", "01:00 · +0 小时");
    set_utc(utc(2027, 3, 13, 7, 0, 0)); CHECK(field_has("America/New_York", "02:00 · -5 小时"), "same-minute date setup uses standard time");
    set_utc(utc(2027, 3, 15, 7, 0, 0)); CHECK(field_has("America/New_York", "03:00 · -4 小时"), "same device minute still refreshes the changed date");
    int64_t jan1 = utc(2027, 1, 1, 1, 0, 0), late = utc(2027, 1, 1, 23, 30, 0);
    setting("time.zone", 0); set_utc(jan1);
    CHECK(field_has("America/New_York", "20:00 · -13 小时 · 前一天"), "previous day handles year boundary relative to device");
    city(2); setting("time.zone", 8); set_utc(late);
    CHECK(field_has("Asia/Tokyo", "08:30 · +17 小时 · 第二天"), "next day and positive offset are relative to device");
    CHECK(!strcmp(ink_time_zone(), "America/Los_Angeles"), "world conversion leaves device zone unchanged");
    evidence("world-zh"); setting("system.language", 1); quiet(); on_ui(read_page);
    CHECK(strstr(snapshot, "World Clock") && strstr(snapshot, "Next day") && strstr(snapshot, "+17 hours"), "world page and time descriptions are translated"); evidence("world-en");
    setting("system.language", 0); press("clock.world_remove", 1); /* Tokyo, London, New York. */
    CHECK(number("total") == 2 && !field_value("Europe/London"), "touch removes the selected city");
    city(5); city(0); city(1); city(3);
    CHECK(number("total") == 6, "six cities accepted");
    CHECK(AI("clock.world_add", "{\"zone\":4}") && g_rc.status == INK_RC_FAILED && number("total") == 6, "seventh city refused without losing choices");
    press("clock.extra_next", 0); CHECK(number("page_number") == 2 && field_value("America/New_York"), "world cities can be paged");
}
static void member_switch(const char *id) {
    char a[48]; snprintf(a, sizeof a, "{\"id\":\"%s\"}", id);
    CHECK(BY_HAND("system.switch_member", a) && g_rc.status == INK_RC_DONE, "person switches to %s", id); quiet();
}
static void members(void) {
    open_page("clock.stopwatch"); int64_t before = number("elapsed_ms"); CHECK(before > 0 && running(), "original stopwatch keeps running during world-clock use");
    member_switch("kid"); open_page("clock.stopwatch");
    CHECK(number("elapsed_ms") == 0 && number("total") == 0 && !running(), "child gets an independent stopwatch");
    ok("clock.stopwatch_start", NULL); wait_us(300000); ok("clock.stopwatch_lap", NULL); ok("clock.stopwatch_pause", NULL);
    CHECK(number("total") == 1 && !running(), "child AI can use stopwatch actions");
    open_page("clock.world"); CHECK(number("total") == 0, "child does not see adult cities"); city(4); city(8);
    CHECK(number("total") == 2, "child AI can select cities independently");
    member_switch("owner"); open_page("clock.world"); CHECK(number("total") == 6, "adult choices survive member switching");
    open_page("clock.stopwatch"); CHECK(running() && number("elapsed_ms") > before + 300, "adult stopwatch advances while another member uses the device");
}
static void restart(void) {
    open_page("clock.stopwatch"); CHECK(number("elapsed_ms") == 0 && number("total") == 0 && !running(), "restart clears running adult stopwatch");
    open_page("clock.world"); CHECK(number("total") == 6 && field_value("Asia/Shanghai"), "adult cities survive process restart");
    ok("clock.extra_next", NULL); CHECK(field_value("Europe/London") && field_value("America/New_York"), "persisted DST cities survive restart");
    member_switch("kid"); open_page("clock.world"); CHECK(number("total") == 2 && field_value("Asia/Singapore") && field_value("America/Los_Angeles"), "child selections survive restart in their own collection");
    open_page("clock.stopwatch"); CHECK(number("elapsed_ms") == 0 && number("total") == 0 && !running(), "restart clears child stopwatch too");
}
static void host_phase(void *arg) { (void)arg; const char *p = getenv("INK_CLOCK_EXTRAS_PHASE"); snprintf(phase, sizeof phase, "%s", p ? p : "create"); }
static void run(void *arg) {
    (void)arg; CHECK(WAIT_FOR(ink_member_space_open(), 10000), "member space opens"); test_start_home();
    if (!strcmp(phase, "restart")) restart(); else { stopwatch(); world(); members(); }
    ink_db_flush_all(); CHECK(WAIT_FOR(ink_db_idle() && !ink_storage_pending(), 10000), "choices reach flash before exit"); ink_probe_count("test.finished");
}
void ink_main(void) {
    vtk_host_call(host_phase, NULL); product_main();
    osal_task_cfg_t cfg = {.name = "clock-extras-test", .fn = run, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start test driver");
}
