/* Synthetic schedules exercise the real capability bus, pages and notification path. */
#include <stdlib.h>
#include <stddef.h>
#include "ink/ui_shell.h"
#define ink_main product_main
#include "../../system/boot/main.c"
#undef ink_main
#include "../integration/whole_product.h"

static char snapshot[3072], owner[16], phase[16];
static const char *wanted;
static int pick;
static bool found;
static lv_point_t point;
static volatile int fired;
static void event(void *ctx, const char *id, const ink_args_t *args) { (void)ctx; (void)id; (void)args; __atomic_add_fetch(&fired, 1, __ATOMIC_RELEASE); }
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void quiet(void) { CHECK(WAIT_FOR(settled(), 10000), "the real page settles"); }
static void call_ok(const char *id, const char *args) {
    CHECK(AI(id, args) && g_rc.status == INK_RC_DONE, "%s completes (%s): %.100s", id, ink_rc_name(g_rc.status), g_rc.detail); quiet();
}
static int64_t at(int day, int hour, int min) {
    return ink_time_from_local(&(ink_tm_t){.year = 2026, .month = 11, .day = day, .hour = hour, .min = min});
}
static bool get(const char *title, ink_alarm_t *a) {
    int cursor = 0;
    while (ink_alarm_next(&cursor, INK_REMINDER, a) == 1) if (!strcmp(a->title, title)) return true;
    return false;
}
static void read_snapshot(void) { CHECK(ink_ui_context_snapshot(snapshot, sizeof snapshot, NULL) > 0, "AI page snapshot fits"); }
static void expect(const char *text) { on_ui(read_snapshot); CHECK(strstr(snapshot, text), "page contains %s", text); }
static void absent(const char *text) { on_ui(read_snapshot); CHECK(!strstr(snapshot, text), "page excludes %s", text); }
static void locate(lv_obj_t *o) {
    if (!lv_obj_is_visible(o)) return;
    const char *cap = ink_shell_bound_cap(o);
    if (cap && !strcmp(cap, wanted) && pick-- == 0) {
        lv_area_t a; lv_obj_get_coords(o, &a);
        CHECK(a.x1 >= 0 && a.y1 >= 0 && a.x2 < ink_ui_width() && a.y2 < ink_ui_height(), "touch target %s fits", wanted);
        point = (lv_point_t){(a.x1 + a.x2) / 2, (a.y1 + a.y2) / 2}; found = true;
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) locate(lv_obj_get_child(o, (int32_t)i));
}
static void locate_ui(void) { found = false; lv_obj_update_layout(lv_screen_active()); locate(lv_screen_active()); }
static void press(const char *id, int index) {
    quiet(); wanted = id; pick = index; on_ui(locate_ui); CHECK(found, "touch target %s exists", id);
    if (found) tap(point.x, point.y);
    quiet();
}
static bool marker_event, marker_reminder;
static void markers(lv_obj_t *o) {
    const char *cap = ink_shell_bound_cap(o);
    if (cap && !strcmp(cap, "calendar.select") && lv_obj_get_child_count(o) >= 3) {
        const char *text = lv_label_get_text(lv_obj_get_child(o, 0));
        if (!strcmp(text, "11") && lv_obj_get_child_count(o) == 4) marker_event = true;
        if (!strcmp(text, "12") && lv_obj_get_child_count(o) == 3 && lv_obj_get_style_border_width(lv_obj_get_child(o, 2), 0) > 0) marker_reminder = true;
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) markers(lv_obj_get_child(o, (int32_t)i));
}
static void check_markers(void) { marker_event = marker_reminder = false; markers(lv_screen_active()); CHECK(marker_event && marker_reminder, "month distinguishes event and reminder markers"); }
static void capture_event(void *arg) { CHECK(settled(), "screenshot is fully presented"); sim_display_snapshot(arg); }
static void capture(const char *name) { quiet(); vtk_event_at(vtk_now() + SIM_MS(1), capture_event, (void *)name); delay_ms(5); }
static void range(const char *from, const char *until) {
    char args[96]; snprintf(args, sizeof args, "{\"from\":\"%s\",\"until\":\"%s\"}", from, until); call_ok("calendar.list", args);
}
static void fixtures(void) {
    call_ok("reminder.add", "{\"at\":\"2026-11-11 23:00\",\"end\":\"2026-11-12 02:00\",\"title\":\"跨天日程\",\"ring\":false}");
    ink_alarm_t a = {0}; CHECK(get("跨天日程", &a) && a.silent && a.duration_s == 10800, "AI creates silent event with end");
    call_ok("reminder.add", "{\"at\":\"2026-11-11 23:00\",\"end\":\"2026-11-11 23:01\",\"title\":\"响铃对照\"}");
    CHECK(get("响铃对照", &a) && !a.silent, "legacy add defaults to audible");
    call_ok("reminder.add", "{\"at\":\"2026-11-13 09:00\",\"title\":\"全天日程\",\"ring\":false,\"all_day\":true}");
    CHECK(get("全天日程", &a) && a.all_day && a.hour == 0 && a.duration_s == 86400, "all-day is local midnight to next midnight");
    call_ok("reminder.add", "{\"at\":\"2026-11-14 08:00\",\"end\":\"2026-11-14 09:00\",\"title\":\"重复日程\",\"ring\":false,\"repeat\":\"daily\"}");
    CHECK(AI("reminder.add", "{\"at\":\"2026-11-14 09:00\",\"end\":\"2026-11-14 08:00\",\"title\":\"无效日程\",\"ring\":false}") && g_rc.status == INK_RC_INVALID, "end before start is refused");
}
static void ai_queries(void) {
    range("2026-11-12", "2026-11-13");
    CHECK(strstr(g_result, "跨天日程") && strstr(g_result, "2026-11-12 02:00") && strstr(g_result, "\"ring\":false"), "range includes overlapping silent event and exclusive end");
    range("2026-11-13", "2026-11-14");
    CHECK(strstr(g_result, "全天日程") && strstr(g_result, "\"all_day\":true") && strstr(g_result, "2026-11-14 00:00"), "AI lists all-day bounds");
    CHECK(!strstr(g_result, "跨天日程"), "range excludes finished event");
    range("2026-11-14", "2026-11-16");
    CHECK(strstr(g_result, "2026-11-14 08:00") && strstr(g_result, "2026-11-15 08:00"), "range expands recurring events");
    CHECK(!strstr(g_result, "全天日程"), "exclusive all-day end does not enter next date");
}
static void month(void) { /* calendar-3: a day's events are listed on its own page (the month shows marks and a count) */
    call_ok("calendar.open", NULL); call_ok("calendar.select", "{\"day\":11}"); expect("这天 2 件事"); on_ui(check_markers); capture("month");
    call_ok("calendar.day", NULL); expect("跨天日程"); expect("日程"); expect("响铃对照");
    call_ok("calendar.day_next", NULL); expect("跨天日程"); absent("响铃对照"); capture("cross-day");
    call_ok("calendar.day_next", NULL); expect("全天日程"); absent("跨天日程"); capture("all-day");
    call_ok("shell.back", NULL);
}
static void silent_deadline(void) {
    CHECK(ink_cap_subscribe("reminder.fired", event, NULL, INK_EXEC_DIRECT) == 0, "observe real reminder events");
    ink_time_set(at(11, 22, 59) + 59, false);
    CHECK(WAIT_FOR(ink_time_now_s() >= at(11, 23, 0) + 3, 5000), "cross silent and audible deadline");
    CHECK(WAIT_FOR(__atomic_load_n(&fired, __ATOMIC_ACQUIRE) >= 1, 5000), "audible control fires");
    CHECK(__atomic_load_n(&fired, __ATOMIC_ACQUIRE) == 1, "silent event emits no fired event");
    ink_alarm_t a = {0}; CHECK(get("跨天日程", &a), "silent event survives its start");
    CHECK(get("响铃对照", &a) && a.next_s == 0 && a.duration_s == 60, "fired timed reminder stays recorded");
    char args[64]; snprintf(args, sizeof args, "{\"id\":%u}", (unsigned)a.id); call_ok("reminder.open", args);
    expect("2026-11-11 23:00"); expect("2026-11-11 23:01"); call_ok("reminder.save", NULL);
    CHECK(get("响铃对照", &a) && a.next_s == 0 && a.duration_s == 60, "saving fired reminder preserves its date without ringing again");
    ink_time_set(at(12, 1, 0), false); quiet(); call_ok("calendar.day", "{\"date\":\"2026-11-12\"}"); expect("跨天日程"); call_ok("shell.back", NULL);
    range("2026-11-12", "2026-11-13"); CHECK(strstr(g_result, "跨天日程"), "ongoing event remains queryable after start");
}
static void isolation(void) {
    ink_alarm_t a = {0}; CHECK(get("跨天日程", &a), "adult event exists before switch"); uint32_t id = a.id;
    CHECK(ink_member_switch("kid") == 0, "switch to synthetic child"); quiet();
    call_ok("app.open", "{\"id\":\"upcoming\"}"); range("2026-11-10", "2026-11-16");
    CHECK(!strstr(g_result, "跨天日程") && !strstr(g_result, "全天日程"), "child range excludes adult records");
    call_ok("calendar.open", NULL); call_ok("calendar.day", "{\"date\":\"2026-11-12\"}"); absent("跨天日程"); call_ok("shell.back", NULL);
    char args[192]; snprintf(args, sizeof args, "{\"id\":%u,\"at\":\"2026-11-12 23:00\",\"title\":\"越权\",\"repeat\":\"none\",\"ring\":false}", (unsigned)id);
    CHECK(AI("reminder.update", args) && g_rc.status == INK_RC_STALE, "child cannot change adult event");
    snprintf(args, sizeof args, "{\"id\":%u}", (unsigned)id);
    CHECK(AI("reminder.cancel", args) && g_rc.status != INK_RC_DONE, "child cannot cancel adult event");
    call_ok("reminder.add", "{\"at\":\"2026-11-13 12:00\",\"title\":\"孩子日程\",\"ring\":false}");
    CHECK(ink_member_switch(owner) == 0, "return to adult"); quiet(); call_ok("app.open", "{\"id\":\"upcoming\"}");
    range("2026-11-13", "2026-11-14"); CHECK(!strstr(g_result, "孩子日程"), "adult range excludes child records");
}
static void editing(void) {
    call_ok("reminder.add", "{\"at\":\"2026-11-13 09:00\",\"title\":\"编辑日程\",\"ring\":false,\"all_day\":true}");
    ink_alarm_t a = {0}; CHECK(get("编辑日程", &a), "event exists for shared editor");
    char args[64]; snprintf(args, sizeof args, "{\"id\":%u}", (unsigned)a.id); call_ok("reminder.open", args);
    expect("提醒"); expect("结束"); expect("2026-11-13"); capture("editor"); /* an all-day end is its last day */
    press("reminder.draft", 0); /* all-day off */
    press("calendar.reminders", 0); press("calendar.reminder_add", 0); press("reminder.draft", 2); press("shell.back", 0); /* rings at its start */
    press("reminder.time", 1); CHECK(!strcmp(ink_ui_page_name(), "ReminderTime"), "end uses existing picker");
    press("reminder.draft", 7); /* hour +1 */
    press("shell.back", 0); press("reminder.save", 0);
    CHECK(get("编辑日程", &a) && !a.silent && !a.all_day && a.duration_s == 90000, "touch edits ring, all-day and end on same record");
    snprintf(args, sizeof args, "{\"id\":%u}", (unsigned)a.id); call_ok("reminder.open", args);
    press("reminder.time", 1); press("reminder.draft", 10); press("reminder.save", 0); /* 不要结束时间 */
    CHECK(get("编辑日程", &a) && a.duration_s == 0, "touch can clear optional end");
    snprintf(args, sizeof args, "{\"id\":%u}", (unsigned)a.id); call_ok("reminder.cancel", args);
    CHECK(!get("编辑日程", &a), "same cancel capability deletes event");
}
static void legacy_record(void) {
    static const ink_db_def_t def = {.name = "time.alarms", .title = "闹钟和提醒", .scope = INK_DB_DEVICE, .schema = 1};
    ink_db_t *db = ink_db_open(&def, NULL); CHECK(db, "open existing alarm collection");
    if (!db) return;
    ink_alarm_t old = {.kind = INK_REMINDER, .enabled = true, .year = 2026, .month = 11, .day = 16, .hour = 10, .next_s = at(16, 10, 0), .src = INK_SRC_TOUCH};
    snprintf(old.owner, sizeof old.owner, "%s", owner); snprintf(old.title, sizeof old.title, "旧版提醒"); old.id = ink_db_new_id(db);
    ink_db_write_t w = {.id = old.id, .sort = old.next_s, .data = &old, .len = offsetof(ink_alarm_t, silent), .src = old.src};
    CHECK(ink_db_put(db, &w, NULL) == INK_DB_OK, "write exact legacy record length"); ink_db_close(db);
}
static void restored(void) {
    ink_alarm_t a = {0}; CHECK(get("跨天日程", &a) && a.silent && a.duration_s == 10800, "silent duration survives restart after start");
    CHECK(get("旧版提醒", &a) && !a.silent && !a.all_day && !a.duration_s, "legacy records stay audible after upgrade");
    ai_queries(); call_ok("calendar.open", NULL); call_ok("calendar.select", "{\"day\":12}"); expect("跨天日程"); capture("restored");
}
static void host_phase(void *arg) { (void)arg; const char *s = getenv("INK_CALENDAR_PHASE"); snprintf(phase, sizeof phase, "%s", s ? s : "create"); }
static void run(void *arg) {
    (void)arg; CHECK(WAIT_FOR(ink_member_space_open(), 10000), "member space opens"); quiet(); test_start_home(); quiet();
    ink_time_set(at(!strcmp(phase, "restart") ? 12 : 10, 6, 0), false);
    snprintf(owner, sizeof owner, "%s", ink_member_current()->id); call_ok("app.open", "{\"id\":\"upcoming\"}");
    if (!strcmp(phase, "create")) { fixtures(); ai_queries(); month(); silent_deadline(); isolation(); editing(); legacy_record(); }
    else restored();
    ink_db_flush_all(); CHECK(WAIT_FOR(ink_db_idle() && !ink_storage_pending(), 10000), "writes reach flash"); ink_probe_count("test.finished");
}
void ink_main(void) {
    vtk_host_call(host_phase, NULL); product_main();
    osal_task_cfg_t cfg = {.name = "calendar-events-test", .fn = run, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start test-only driver");
}
