/* Real UI controls and synthetic schedules, with no hardware or network calls. */
#include <stdlib.h>
#include "ink/ui_shell.h"
#define ink_main product_main
#include "../../system/boot/main.c"
#undef ink_main
#include "../integration/whole_product.h"

static char snapshot[3072], owner[16], scene[24];
static const char *wanted_cap, *wanted_label;
static lv_point_t point;
static bool found;
static int pick;
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void quiet(void) { CHECK(WAIT_FOR(settled(), 10000), "the real page settles"); }
static void call_ok(const char *cap, const char *args) {
    CHECK(AI(cap, args) && g_rc.status == INK_RC_DONE, "%s completes through the real AI bus (%s)", cap, ink_rc_name(g_rc.status)); quiet();
}
static void read_snapshot(void) {
    CHECK(ink_ui_snapshot(snapshot, sizeof snapshot, NULL) > 0, "the page fits the production AI snapshot budget");
    CHECK(ink_ui_context_snapshot(g_snap, sizeof g_snap, NULL) > 0 && !strcmp(snapshot, g_snap), "AI reads the same visible page");
}
static void locate(lv_obj_t *root) {
    if (!lv_obj_is_visible(root)) return;
    const char *cap = ink_shell_bound_cap(root);
    bool match = wanted_cap ? cap && !strcmp(cap, wanted_cap) : lv_obj_check_type(root, &lv_label_class) && !strcmp(lv_label_get_text(root), wanted_label);
    if (match && pick-- == 0) {
        lv_area_t a; lv_obj_get_coords(root, &a);
        CHECK(a.x1 >= 0 && a.y1 >= 0 && a.x2 < ink_ui_width() && a.y2 < ink_ui_height(), "the target fits the screen");
        point = (lv_point_t){(a.x1 + a.x2) / 2, (a.y1 + a.y2) / 2}; found = true;
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); i++) locate(lv_obj_get_child(root, (int32_t)i));
}
static void locate_target(void) { found = false; lv_obj_update_layout(lv_screen_active()); locate(lv_screen_active()); }
static void press(const char *cap, int index) {
    wanted_cap = cap; pick = index; on_ui(locate_target); CHECK(found, "touch target %s exists", cap);
    if (found) tap(point.x, point.y); quiet();
}
static void visible(const char *label) {
    wanted_cap = NULL; wanted_label = label; pick = 0; on_ui(locate_target); CHECK(found, "visible label: %s", label);
}
static int64_t local_time(int y, int m, int d, int h, int minute) {
    ink_tm_t t = {.year = y, .month = m, .day = d, .hour = h, .min = minute}; return ink_time_from_local(&t);
}
static uint32_t add(int kind, int day, int hour, const char *title, bool daily) {
    ink_alarm_t a = {.kind = (uint8_t)kind, .year = 2026, .month = 11, .day = (uint8_t)day, .hour = (uint8_t)hour,
        .repeat = daily ? INK_REPEAT_WEEKDAYS : INK_REPEAT_NONE, .weekdays = daily ? 127 : 0, .src = INK_SRC_TOUCH};
    snprintf(a.title, sizeof a.title, "%s", title);
    int id = ink_alarm_add(&a); CHECK(id > 0, "create synthetic fixture %s", title); return (uint32_t)id;
}
static void capture_event(void *arg) {
    const char *name = arg; CHECK(settled(), "the captured frame is fully presented");
    if (!strcmp(scene, name)) {
        ink_probe_count("test.snapshot.settled");
        ink_probe_count(!strcmp(name, "Upcoming") ? "test.snapshot.Upcoming" : "test.snapshot.Calendar");
    }
    sim_display_snapshot(!strcmp(scene, name) ? "scene" : name);
}
static void capture(const char *name) {
    if (strcmp(scene, name) && (strcmp(scene, "Calendar") || !strcmp(name, "Upcoming"))) return;
    quiet(); vtk_event_at(vtk_now() + SIM_MS(1), capture_event, (void *)name); delay_ms(5);
}
static void expect(const char *text) { on_ui(read_snapshot); CHECK(strstr(snapshot, text), "snapshot contains %s", text); }
static void absent(const char *text) { on_ui(read_snapshot); CHECK(!strstr(snapshot, text), "snapshot excludes %s", text); }
static lv_area_t month_buttons[2], month_title;
static unsigned month_found[2];
static void check_month_tree(lv_obj_t *obj) {
    if (!lv_obj_is_visible(obj)) return;
    const char *cap = ink_shell_bound_cap(obj);
    if (cap && (!strcmp(cap, "calendar.month_prev") || !strcmp(cap, "calendar.month_next"))) {
        unsigned i = !strcmp(cap, "calendar.month_next"); month_found[i]++;
        lv_area_t *a = &month_buttons[i]; lv_obj_get_coords(obj, a);
        CHECK(a->x1 >= 0 && a->y1 >= 0 && a->x2 < ink_ui_width() && a->y2 < ink_ui_height(), "%s fits the screen", cap);
        CHECK(lv_obj_get_width(obj) >= ink_ui_px(INK_CONTROL) && lv_obj_get_height(obj) >= ink_ui_px(INK_CONTROL), "%s has a full-size touch target", cap);
        lv_obj_t *glyph = lv_obj_get_child(obj, 0); lv_area_t g; lv_obj_get_coords(glyph, &g);
        CHECK(g.x1 >= a->x1 && g.x2 <= a->x2 && g.y1 >= a->y1 && g.y2 <= a->y2, "%s renders its whole glyph inside the button", cap);
        if (lv_obj_check_type(glyph, &lv_label_class)) {
            int need = ink_ui_px(ink_ui_text_width(i ? "下月" : "上月", INK_FONT_LIST, INK_WEIGHT_MEDIUM));
            CHECK(need <= lv_obj_get_width(glyph), "%s renders both characters: need=%d have=%d", cap, need, (int)lv_obj_get_width(glyph));
        } else CHECK(lv_obj_check_type(glyph, &lv_image_class) && lv_image_get_src(glyph), "%s renders its navigation icon", cap);
    }
    if (lv_obj_check_type(obj, &lv_label_class) && !strcmp(lv_label_get_text(obj), "2026年11月")) {
        lv_obj_get_coords(obj, &month_title);
        month_title.x2 = month_title.x1 + ink_ui_px(ink_ui_text_width("2026年11月", INK_FONT_TITLE, INK_WEIGHT_BOLD)) - 1;
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(obj); i++) check_month_tree(lv_obj_get_child(obj, (int32_t)i));
}
static void check_month_controls(void) {
    memset(month_found, 0, sizeof month_found); memset(&month_title, 0, sizeof month_title);
    lv_obj_update_layout(lv_screen_active()); check_month_tree(lv_screen_active());
    for (unsigned i = 0; i < 2; i++) {
        CHECK(month_found[i] == 1, "each month control exists exactly once");
        CHECK(!month_title.x2 || month_buttons[i].y1 > month_title.y2 || month_buttons[i].x1 > month_title.x2,
              "the month control does not cover the full month title");
    }
}
static bool minute_changed(void) { int64_t v = 0; return ink_state_get_int(ink_state_find("time.minute"), &v) && v == 362; }
static void run(void *arg) {
    (void)arg; CHECK(WAIT_FOR(ink_member_space_open(), 10000), "member space opens"); quiet(); test_start_home(); quiet();
    ink_time_set(local_time(2026, 11, 4, 6, 0), false);
    snprintf(owner, sizeof owner, "%s", ink_member_current()->id);
    call_ok("shell.launch", "{\"id\":\"upcoming\"}"); CHECK(!strcmp(ink_ui_page_name(), "Upcoming"), "the home entry opens Upcoming");
    expect("\"total\":0"); visible("还没有接下来的事");
    add(INK_REMINDER, 6, 15, "家长会", false);
    add(INK_ALARM, 5, 7, "起床", false);
    add(INK_REMINDER, 5, 8, "带作业本", false);
    add(INK_REMINDER, 5, 16, "放学取快递", false);
    add(INK_REMINDER, 4, 18, "今晚读书", false);
    uint32_t disabled = add(INK_ALARM, 5, 9, "已关闭测试", false);
    char args[96]; snprintf(args, sizeof args, "{\"id\":%u,\"enabled\":false}", (unsigned)disabled); call_ok("alarm.update", args);
    expect("\"total\":5"); absent("已关闭测试");
    char *a = strstr(snapshot, "今晚读书"), *b = strstr(snapshot, "起床"), *c = strstr(snapshot, "带作业本"), *d = strstr(snapshot, "放学取快递");
    CHECK(a && b && c && d && a < b && b < c && c < d, "mixed alarms and reminders are sorted by real time");
    visible("18:00 今晚读书"); visible("07:00 起床"); capture("Upcoming");
    key(HAL_KEY_NEXT); quiet(); expect("家长会"); absent("今晚读书"); expect("\"page\":2");
    call_ok("calendar.prev", NULL); expect("\"page\":1");
    press("calendar.open", 0); CHECK(!strcmp(ink_ui_page_name(), "Calendar"), "the real month button opens Calendar");
    expect("2026年11月4日"); expect("今晚读书"); absent("带作业本"); capture("Calendar");
    press("calendar.select", 4); expect("2026年11月5日"); expect("起床"); expect("带作业本"); absent("今晚读书");
    call_ok("calendar.next", NULL); expect("放学取快递");
    call_ok("calendar.select", "{\"day\":6}"); expect("家长会"); expect("\"page\":1");
    call_ok("calendar.select", "{\"day\":7}"); expect("\"total\":0"); visible("这一天没有提醒和闹钟");
    press("calendar.month_next", 0); expect("2026年12月1日");
    call_ok("calendar.month_next", NULL); expect("2027年1月1日");
    press("calendar.month_prev", 0); expect("2026年12月1日"); call_ok("calendar.month_prev", NULL); expect("2026年11月1日");
    CHECK(AI("calendar.select", "{\"day\":31}") && g_rc.status == INK_RC_INVALID, "November 31 is rejected"); quiet();
    CHECK(AI("calendar.select", "{\"day\":0}") && g_rc.status == INK_RC_INVALID, "day zero is rejected by the shared schema"); quiet();
    call_ok("calendar.month_prev", NULL); expect("2026年10月1日");
    call_ok("calendar.month_prev", NULL); call_ok("calendar.month_prev", NULL); expect("2026年8月1日");
    press("calendar.select", 30); expect("2026年8月31日"); on_ui(check_month_controls);
    add(INK_ALARM, 4, 7, "每天合成闹钟", true); quiet();
    call_ok("calendar.month_next", NULL); call_ok("calendar.month_next", NULL); call_ok("calendar.month_next", NULL);
    call_ok("calendar.select", "{\"day\":7}"); expect("每天合成闹钟");
    call_ok("shell.back", NULL); expect("今晚读书"); expect("每天合成闹钟");
    CHECK(ink_member_switch("kid") == 0, "switch to the synthetic child"); quiet();
    call_ok("app.open", "{\"id\":\"upcoming\"}"); expect("\"total\":0"); absent("起床"); absent("家长会"); absent("每天合成闹钟");
    add(INK_REMINDER, 5, 12, "孩子的合成提醒", false); quiet(); expect("孩子的合成提醒");
    press("calendar.open", 0); call_ok("calendar.select", "{\"day\":5}"); expect("孩子的合成提醒"); absent("起床"); absent("带作业本");
    on_ui(check_month_controls); capture("ChildCalendar");
    press("calendar.month_prev", 0); call_ok("calendar.month_prev", NULL); call_ok("calendar.month_prev", NULL);
    expect("2026年8月1日"); on_ui(check_month_controls); capture("ChildSixRows");
    CHECK(ink_member_switch(owner) == 0, "return to the original member"); quiet();
    call_ok("app.open", "{\"id\":\"upcoming\"}"); absent("孩子的合成提醒"); expect("今晚读书");
    ink_time_set(local_time(2026, 11, 5, 6, 0), false); quiet(); absent("今晚读书"); expect("起床");
    call_ok("settings.set", "{\"id\":\"display.font_size\",\"value\":3}");
    call_ok("calendar.open", NULL); call_ok("calendar.select", "{\"day\":5}");
    on_ui(check_month_controls); capture("LargeFontCalendar");
    press("calendar.month_next", 0); expect("2026年12月1日"); call_ok("calendar.month_prev", NULL);
    call_ok("calendar.select", "{\"day\":5}");
    ink_time_set(local_time(2026, 11, 5, 6, 1) + 55, false); quiet();
    on_ui(read_snapshot); char before[sizeof snapshot]; memcpy(before, snapshot, sizeof before);
    uint32_t generation = ink_ui_generation(); capture("MinuteBefore"); ink_probe_mark("calendar.minute.before");
    CHECK(WAIT_FOR(minute_changed(), 15000), "the real minute timer reaches 06:02"); quiet();
    ink_probe_mark("calendar.minute.after");
    CHECK(ink_ui_generation() == generation, "a minute-only update keeps the Calendar page (delta=%u)", ink_ui_generation() - generation);
    on_ui(read_snapshot); CHECK(!strcmp(before, snapshot), "minute-only updates preserve the selected day and items");
    visible("06:02"); capture("MinuteAfter");
    ink_db_flush_all(); CHECK(WAIT_FOR(ink_db_idle() && !ink_storage_pending(), 10000), "fixture writes complete");
    ink_probe_count("test.finished");
}
static void host_scene(void *arg) { (void)arg; const char *name = getenv("INK_CALENDAR_SCENE"); snprintf(scene, sizeof scene, "%s", name ? name : ""); }
void ink_main(void) {
    vtk_host_call(host_scene, NULL); product_main();
    osal_task_cfg_t cfg = {.name = "calendar-test", .fn = run, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start the test-only driver");
}
