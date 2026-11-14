/* Touch the real pages; ring and restart against synthetic flash only. */
#include <stdlib.h>
#include "ink/ui_shell.h"
#define ink_main product_main
#include "../../system/boot/main.c"
#undef ink_main
#include "../integration/whole_product.h"

static char phase[16], owner[16], snapshot[3072];
static const char *wanted_cap, *wanted_label;
static int pick;
static lv_point_t point;
static bool found;
static volatile int fired;
static char fired_title[48];
static void reminder_event(void *ctx, const char *id, const ink_args_t *args) {
    (void)ctx; (void)id;
    ink_args_get_str(args, "title", fired_title, sizeof fired_title);
    __atomic_add_fetch(&fired, 1, __ATOMIC_RELEASE);
}
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void quiet(void) { CHECK(WAIT_FOR(settled(), 10000), "the page settles"); }
static void call_ok(const char *cap, const char *args) {
    CHECK(AI(cap, args) && g_rc.status == INK_RC_DONE, "%s completes: %s", cap, ink_rc_name(g_rc.status)); quiet();
}
static void read_snapshot(void) {
    CHECK(ink_ui_snapshot(snapshot, sizeof snapshot, NULL) > 0, "the page fits the AI snapshot budget");
    CHECK(ink_ui_context_snapshot(g_snap, sizeof g_snap, NULL) > 0 && !strcmp(snapshot, g_snap), "AI reads the visible page");
}
static void expect(const char *value) { on_ui(read_snapshot); CHECK(strstr(snapshot, value), "snapshot includes %s", value); }
static void absent(const char *value) { on_ui(read_snapshot); CHECK(!strstr(snapshot, value), "snapshot excludes %s", value); }
static void locate(lv_obj_t *root) {
    if (!lv_obj_is_visible(root)) return;
    const char *cap = ink_shell_bound_cap(root);
    bool match = wanted_cap ? cap && !strcmp(cap, wanted_cap) : lv_obj_check_type(root, &lv_label_class) && !strcmp(lv_label_get_text(root), wanted_label);
    if (match && pick-- == 0) {
        lv_area_t a; lv_obj_get_coords(root, &a);
        CHECK(a.x1 >= 0 && a.y1 >= 0 && a.x2 < ink_ui_width() && a.y2 < ink_ui_height(), "touch target fits the screen");
        point = (lv_point_t){(a.x1 + a.x2) / 2, (a.y1 + a.y2) / 2}; found = true;
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); i++) locate(lv_obj_get_child(root, (int32_t)i));
}
static void locate_target(void) { found = false; lv_obj_update_layout(lv_screen_active()); locate(lv_screen_active()); }
static void press(const char *cap, const char *label, int index) {
    quiet(); wanted_cap = cap; wanted_label = label; pick = index; on_ui(locate_target);
    CHECK(found, "touch target %s exists", cap ? cap : label);
    if (found) tap(point.x, point.y);
    quiet();
}
static void label(const char *text) { press(NULL, text, 0); }
static void button(const char *cap) { press(cap, NULL, 0); }
static void input_point(void) {
    read_snapshot(); ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL);
    const char *input = strstr(snapshot, "\"input\":");
    CHECK(j && input && ink_json_parse(j, input + 8, strlen(input + 8) - 1) == 0, "parse the short input's actual rectangle");
    if (!j || !input) { osal_free(j); return; }
    int rect = ink_json_find(j, 0, "rect");
    int64_t x = 0, y = 0, w = 0, h = 0;
    CHECK(ink_json_int(j, ink_json_find(j, rect, "x"), &x) && ink_json_int(j, ink_json_find(j, rect, "y"), &y) &&
          ink_json_int(j, ink_json_find(j, rect, "width"), &w) && ink_json_int(j, ink_json_find(j, rect, "height"), &h), "the input rectangle exists");
    point = (lv_point_t){(lv_coord_t)(x + w / 2), (lv_coord_t)(y + h / 2)}; osal_free(j);
}
static void name(const char *text) {
    on_ui(input_point); tap(point.x, point.y); quiet(); CHECK(ink_text_focused(), "the touch was received by the short input host");
    call_ok("text.select", "{\"start\":0,\"end\":1000}");
    char args[160]; snprintf(args, sizeof args, "{\"text\":\"%s\"}", text); call_ok("text.insert", args);
    expect(text); call_ok("text.submit", NULL); CHECK(!ink_text_focused(), "the keyboard releases input");
}
static int64_t at(int day, int hour, int minute) {
    ink_tm_t t = {.year = 2026, .month = 11, .day = day, .hour = hour, .min = minute}; return ink_time_from_local(&t);
}
static bool get_named(const char *title, ink_alarm_t *a) {
    int cursor = 0;
    while (ink_alarm_next(&cursor, INK_REMINDER, a) == 1) if (!strcmp(a->title, title)) return true;
    return false;
}
static ink_alarm_t reminder(const char *title) {
    ink_alarm_t a = {0}; CHECK(get_named(title, &a), "stored reminder %s exists", title); return a;
}
static void capture_event(void *arg) { CHECK(settled(), "capture a fully presented frame"); sim_display_snapshot(arg); }
static void capture(const char *name_) { quiet(); vtk_event_at(vtk_now() + SIM_MS(1), capture_event, (void *)name_); delay_ms(5); }
static bool notification(const char *title) {
    ink_db_cursor_t cursor = {0}; ink_notify_view_t n;
    while (ink_notify_next(&cursor, &n) == 1) if (!strcmp(n.channel, "reminder") && !strcmp(n.body, title)) return true;
    return false;
}
static void rings(int count, const char *title) {
    CHECK(WAIT_FOR(__atomic_load_n(&fired, __ATOMIC_ACQUIRE) == count, 5000), "the reminder fires at its exact deadline");
    CHECK(!strcmp(fired_title, title), "the fired event identifies the scheduled content");
    CHECK(WAIT_FOR(notification(title), 5000), "the existing high-priority notification reaches the member inbox"); quiet();
}
static void not_ringing(int day, int hour) {
    int before = __atomic_load_n(&fired, __ATOMIC_ACQUIRE);
    ink_time_set(at(day, hour, 0) - 1, false);
    CHECK(WAIT_FOR(ink_time_now_s() >= at(day, hour, 0) + 2, 5000), "cross the exact removed deadline");
    CHECK(__atomic_load_n(&fired, __ATOMIC_ACQUIRE) == before, "the removed deadline does not fire"); quiet();
}
/* calendar-3: adding starts from reminder.new (the day page's 加一件 calls it), a row opens the read-only details first,
 * 编辑 opens the editor, the repeat is a page of rows, a repeating one asks which occurrences, the list is the agenda. */
static void agenda(void) { call_ok("calendar.view", "{\"view\":\"agenda\"}"); CHECK(!strcmp(ink_ui_page_name(), "Agenda"), "the agenda lists what comes"); }
static void edit(const char *row) { label(row); CHECK(!strcmp(ink_ui_page_name(), "EventInfo"), "a row opens the read-only details"); button("reminder.open"); }
static void repeat(const char *word) { button("calendar.repeat"); label(word); if (strcmp(ink_ui_page_name(), "ReminderEdit")) button("shell.back"); }
static void manual_cycle(void) {
    /* calendar-3: new events get the member's default reminder (10 minutes before); these checks ring at the start */
    call_ok("settings.set", "{\"id\":\"calendar.default_reminder\",\"value\":1}");
    agenda();
    call_ok("reminder.new", "{\"date\":\"2026-11-11\"}"); CHECK(!strcmp(ink_ui_page_name(), "ReminderEdit"), "manual Add opens the editor");
    capture("add"); button("reminder.save"); expect("标题不能空");
    name("手动提醒"); button("reminder.time"); press("reminder.draft", NULL, 7); expect("2026-11-11 08:00"); capture("picker");
    press("reminder.draft", NULL, 9); expect("08:01"); press("reminder.draft", NULL, 8); expect("08:00");
    button("shell.back"); button("reminder.save"); CHECK(!strcmp(ink_ui_page_name(), "Agenda"), "saving returns to the agenda");
    ink_alarm_t a = reminder("手动提醒"); CHECK(a.next_s == at(11, 8, 0), "touch saved tomorrow at 08:00");
    label("08:00  手动提醒"); expect("calendar.delete"); expect("reminder.open"); capture("detail"); button("shell.back");
    ink_time_set(at(11, 8, 0) - 1, false); rings(1, "手动提醒");
    ink_time_set(at(10, 6, 0), false); quiet(); call_ok("reminder.new", "{\"date\":\"2026-11-11\"}"); button("reminder.time"); press("reminder.draft", NULL, 7); button("shell.back");
    name("待改提醒"); button("reminder.save");
    a = reminder("待改提醒"); uint32_t original = a.id; edit("08:00  待改提醒");
    name("改好提醒"); button("reminder.time"); press("reminder.draft", NULL, 7); expect("09:00");
    button("shell.back"); capture("modified"); button("reminder.save");
    a = reminder("改好提醒"); CHECK(a.id == original && a.next_s == at(11, 9, 0), "editing reschedules the same reminder to 09:00");
    not_ringing(11, 8);
    call_ok("calendar.view", "{\"view\":\"month\"}"); call_ok("calendar.select", "{\"day\":11}");
    call_ok("calendar.day", NULL); expect("09:00  改好提醒"); absent("08:00  待改提醒"); edit("09:00  改好提醒"); repeat("每天"); button("reminder.save");
    ink_time_set(at(11, 9, 0) - 1, false); rings(2, "改好提醒"); a = reminder("改好提醒");
    CHECK(a.repeat == INK_REPEAT_WEEKDAYS && a.weekdays == 127 && a.next_s == at(12, 9, 0), "daily uses the existing scheduler and next day");
    label("09:00  改好提醒"); button("calendar.delete"); label("全部删掉");
    CHECK(WAIT_FOR(strcmp(ink_ui_page_name(), "EventInfo") && strcmp(ink_ui_page_name(), "Scope"), 5000), "deletion returns to the list without a yes/no card"); quiet();
    ink_alarm_t deleted; CHECK(!get_named("改好提醒", &deleted), "the reminder is deleted"); not_ringing(12, 9);
    call_ok("shell.back", NULL); CHECK(!strcmp(ink_ui_page_name(), "Calendar"), "return from the day to the month");
}
static void ai_and_members(void) {
    agenda();
    call_ok("reminder.add", "{\"at\":\"2026-11-13 08:00\",\"title\":\"AI提醒\"}");
    edit("08:00  AI提醒"); name("AI改好"); button("reminder.time"); press("reminder.draft", NULL, 7); button("shell.back");
    repeat("每周"); repeat("工作日"); repeat("每天"); button("reminder.save");
    ink_alarm_t a = reminder("AI改好");
    CHECK(a.hour == 9 && a.weekdays == 127 && a.next_s == at(13, 9, 0), "AI-created reminder is edited by touch without ringing before the selected date");
    call_ok("reminder.add", "{\"at\":\"2026-11-13 10:00\",\"title\":\"AI待删\"}");
    label("10:00  AI待删"); button("calendar.delete"); quiet(); ink_alarm_t removed;
    CHECK(!get_named("AI待删", &removed), "AI-created reminder is deleted from the detail");
    CHECK(ink_member_switch("kid") == 0, "switch to the synthetic child"); quiet(); call_ok("app.open", "{\"id\":\"upcoming\"}"); agenda();
    expect("还没有接下来的事"); absent("AI改好"); char args[192];
    snprintf(args, sizeof args, "{\"id\":%u,\"at\":\"2026-11-13 09:00\",\"title\":\"越权测试\",\"repeat\":\"none\",\"scope\":\"all\"}", (unsigned)a.id);
    CHECK(AI("reminder.update", args) && g_rc.status == INK_RC_STALE, "the child cannot edit an adult reminder");
    snprintf(args, sizeof args, "{\"id\":%u}", (unsigned)a.id);
    CHECK(AI("reminder.open", args) && g_rc.status == INK_RC_STALE, "the child cannot view an adult detail");
    CHECK(AI("reminder.cancel", args) && g_rc.status != INK_RC_DONE, "the child cannot delete an adult reminder"); quiet();
    call_ok("reminder.new", "{\"date\":\"2026-11-13\"}"); name("孩子事项");
    button("reminder.save"); agenda(); expect("孩子事项");
    CHECK(ink_member_switch(owner) == 0, "switch back to the original owner"); quiet(); call_ok("app.open", "{\"id\":\"upcoming\"}"); agenda();
    expect("AI改好"); absent("孩子事项"); a = reminder("AI改好"); CHECK(a.hour == 9 && a.weekdays == 127, "adult data survived child operations");
}
static void restored(void) {
    agenda(); expect("AI改好"); absent("孩子事项"); absent("AI待删"); absent("改好提醒");
    ink_alarm_t a = reminder("AI改好"); CHECK(a.hour == 9 && a.weekdays == 127 && a.next_s == at(13, 9, 0), "edited title, time, repeat survive restart");
    label("09:00  AI改好"); expect("每天"); expect("calendar.delete"); button("shell.back");
    CHECK(ink_member_switch("kid") == 0, "open the child's persisted data"); quiet(); call_ok("app.open", "{\"id\":\"upcoming\"}"); agenda();
    expect("孩子事项"); absent("AI改好"); a = reminder("孩子事项"); CHECK(a.next_s >= at(13, 0, 0) && a.next_s < at(14, 0, 0), "child reminder survives restart on the 13th");
}
static void host_phase(void *arg) { (void)arg; const char *p = getenv("INK_REMINDER_PHASE"); snprintf(phase, sizeof phase, "%s", p ? p : "create"); }
static void run(void *arg) {
    (void)arg; CHECK(WAIT_FOR(ink_member_space_open(), 10000), "the real member space opens"); quiet(); test_start_home(); quiet();
    ink_time_set(at(!strcmp(phase, "restart") ? 12 : 10, 6, 0), false);
    snprintf(owner, sizeof owner, "%s", ink_member_current()->id); call_ok("app.open", "{\"id\":\"upcoming\"}");
    if (!ink_cap_find("reminder.new")) { CHECK(false, "manual reminder capability is required"); ink_probe_count("test.finished"); return; }
    CHECK(ink_cap_subscribe("reminder.fired", reminder_event, NULL, INK_EXEC_DIRECT) == 0, "observe real reminder events");
    if (!strcmp(phase, "create")) { manual_cycle(); ai_and_members(); }
    else restored();
    ink_db_flush_all(); CHECK(WAIT_FOR(ink_db_idle() && !ink_storage_pending(), 10000), "all writes reached simulated flash");
    ink_probe_count("test.finished");
}
void ink_main(void) {
    vtk_host_call(host_phase, NULL); product_main();
    osal_task_cfg_t cfg = {.name = "reminder-test", .fn = run, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start the test-only driver");
}
