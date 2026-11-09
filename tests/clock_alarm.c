/* Real pages, synthetic schedules, and process-restart persistence. */
#include <stdlib.h>
#include "ink/ui_shell.h"
#define ink_main product_main
#include "../system/boot/main.c"
#undef ink_main
#include "integration/whole_product.h"

static char phase[16], snapshot[4096];
static const char *wanted;
static lv_point_t point;
static bool found;
static lv_obj_t *hit;
static bool edited;
static ink_alarm_t alarms[8];
static int count;
static uint32_t deleted;
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void quiet(void) { CHECK(WAIT_FOR(settled(), 10000), "the real page settles"); }
static void locate(lv_obj_t *root) {
    if (!lv_obj_is_visible(root)) return;
    if (lv_obj_check_type(root, &lv_label_class) && !strcmp(lv_label_get_text(root), wanted)) {
        lv_area_t a; lv_obj_get_coords(root, &a); point = (lv_point_t){(a.x1 + a.x2) / 2, (a.y1 + a.y2) / 2}; found = true;
        hit = root;
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); i++) locate(lv_obj_get_child(root, (int32_t)i));
}
static void locate_label(void) { found = false; lv_obj_update_layout(lv_screen_active()); locate(lv_screen_active()); locate(lv_layer_sys()); }
static void press(const char *label) {
    wanted = label; on_ui(locate_label); CHECK(found, "the actual page has the button %s", label);
    if (found) tap(point.x, point.y);
    quiet();
}
static void visible(const char *label) {
    wanted = label; locate_label(); CHECK(found, "the visible LVGL labels contain %s", label);
}
static void locate_switch(void) {
    locate_label();
    if (found) {
        lv_obj_t *track = lv_obj_get_child(lv_obj_get_child(lv_obj_get_parent(hit), 0), 1);
        lv_obj_t *target = lv_obj_get_parent(track);
        CHECK(lv_obj_has_flag(target, LV_OBJ_FLAG_CLICKABLE) && lv_obj_get_width(target) >= ink_ui_px(82) && lv_obj_get_height(target) >= ink_ui_px(82), "the switch has a full touch target");
        lv_area_t a; lv_obj_get_coords(track, &a); point = (lv_point_t){(a.x1 + a.x2) / 2, (a.y1 + a.y2) / 2};
    }
}
static void toggle(const char *time) {
    wanted = time; on_ui(locate_switch); CHECK(found, "the real list has the switch beside %s", time);
    if (found) tap(point.x, point.y);
    quiet();
    CHECK(!strcmp(ink_ui_page_name(), "Clock"), "toggling stays on the list");
}
static void read_snapshot(void) {
    CHECK(ink_ui_snapshot(snapshot, sizeof snapshot, NULL) > 0, "the page exposes its snapshot");
    CHECK(ink_ui_context_snapshot(g_snap, sizeof g_snap, NULL) > 0 && !strcmp(snapshot, g_snap), "AI and the visible page share the same snapshot");
}
static void read_alarms(void) {
    int cursor = 0; count = 0;
    while (count < 8 && ink_alarm_next(&cursor, INK_ALARM, &alarms[count]) == 1) count++;
}
static void call_ok(const char *cap, const char *args) {
    CHECK(AI(cap, args) && g_rc.status == INK_RC_DONE, "%s completed through the real bus (%s)", cap, ink_rc_name(g_rc.status)); quiet();
}
static void name_input(const char *name, const char *expected) {
    call_ok("clock.focus", NULL);
    call_ok("text.select", "{\"start\":0,\"end\":1000}");
    char json[160]; snprintf(json, sizeof json, "{\"text\":\"%s\"}", name);
    call_ok("text.insert", json); on_ui(read_snapshot); CHECK(strstr(snapshot, expected), "the short input host exposes the actual name");
    call_ok("text.submit", NULL);
}
static void name_alarm(const char *name) { name_input(name, name); }
static void verify_rows(void) {
    read_alarms();
    CHECK(count == 3, "exactly the three saved alarms remain, including after restart");
    bool work = false, custom = false, daily = false;
    for (int i = 0; i < count; i++) {
        ink_alarm_t *a = &alarms[i]; CHECK(a->enabled == !(edited && !strcmp(a->title, "周末")), "the saved enabled state survives restart");
        CHECK(a->repeat == INK_REPEAT_WEEKDAYS, "the existing repeat rule is preserved");
        if (!strcmp(a->title, edited ? "晨起" : "起床")) { work = true; CHECK(a->hour == (edited ? 8 : 7) && a->min == (edited ? 31 : 30) && a->weekdays == 62, "edited time, name and workday mask persist"); }
        if (!strcmp(a->title, "周末")) { custom = true; CHECK(a->hour == 9 && a->min == 0 && a->weekdays == 65, "custom Sunday and Saturday bits persist"); }
        if (!strcmp(a->title, "读书时间")) { daily = true; CHECK(a->hour == 21 && a->min == 30 && a->weekdays == 127, "every day saves all seven bits"); }
        CHECK(a->id != deleted, "the deleted alarm is absent");
    }
    CHECK(work && custom && daily, "all three names persist without example data in product code");
}
static void verify_list(void) {
    read_snapshot();
    CHECK(strstr(snapshot, edited ? "8:31" : "7:30") && strstr(snapshot, "工作日") && strstr(snapshot, "每天") && strstr(snapshot, "周日 周六"), "the list snapshot contains times and all repeat labels");
    CHECK(strstr(snapshot, "开启") && strstr(snapshot, "\"total\":3") && strstr(snapshot, "next_alarm"), "the same snapshot includes enabled state, count and next alarm");
    visible(edited ? "8:31" : "7:30"); visible(edited ? "晨起 · 工作日" : "起床 · 工作日");
    visible("9:00"); visible("周末 · 周日 周六"); visible("21:30"); visible("读书时间 · 每天");
    if (edited) CHECK(strstr(snapshot, "关闭"), "AI sees the disabled alarm on the same page");
}
static void take_scene(void *arg) {
    CHECK(settled(), "only fully presented frames are compared");
    if (!strcmp(arg, "scene")) { ink_probe_count("test.snapshot.settled"); ink_probe_count("test.snapshot.Clock"); }
    sim_display_snapshot(arg);
}
static void capture(const char *name) { quiet(); vtk_event_at(vtk_now() + SIM_MS(1), take_scene, (void *)name); delay_ms(5); }
static void save_scene(void) { capture("scene"); }
static void create_alarms(void) {
    on_ui(read_snapshot); CHECK(strstr(snapshot, "\"total\":0"), "an empty collection has no fabricated alarms");
    press("+ 加闹钟"); CHECK(!strcmp(ink_ui_page_name(), "ClockAlarm"), "touch opens the real new-alarm form");
    press("时-"); on_ui(read_snapshot); CHECK(strstr(snapshot, "06:00"), "the supported minus button decrements hours"); press("时＋");
    press("时＋"); on_ui(read_snapshot); CHECK(strstr(snapshot, "08:00"), "touch changes the same draft exposed to AI");
    press("分-"); on_ui(read_snapshot); CHECK(strstr(snapshot, "08:59"), "the supported minus button wraps minutes"); press("分＋");
    call_ok("clock.draft", "{\"hour\":7,\"minute\":30}"); press("工作日"); name_alarm("起床"); press("保存闹钟");
    CHECK(!strcmp(ink_ui_page_name(), "Clock"), "saving returns to the list");
    call_ok("clock.new", NULL); call_ok("clock.draft", "{\"hour\":9}"); press("周日"); press("周六"); name_alarm("周末"); call_ok("clock.save", NULL);
    call_ok("clock.new", NULL); call_ok("clock.draft", "{\"hour\":21,\"minute\":30}"); press("每天"); name_alarm("读书时间"); call_ok("clock.save", NULL);
    on_ui(verify_rows); on_ui(verify_list);
    call_ok("clock.new", NULL); name_alarm("删除测试"); press("保存闹钟");
    on_ui(read_alarms); CHECK(count == 4, "create a fourth schedule for paging and deletion"); deleted = alarms[count - 1].id;
    key(INK_KEY_NEXT); quiet(); on_ui(read_snapshot); CHECK(strstr(snapshot, "\"page\":2") && strstr(snapshot, "删除测试"), "the hardware key uses F4 pagination");
    call_ok("clock.prev", NULL); on_ui(read_snapshot); CHECK(strstr(snapshot, "\"page\":1"), "AI uses the same previous-page capability");
    char args[48]; snprintf(args, sizeof args, "{\"id\":%u}", (unsigned)deleted); call_ok("clock.open", args);
    on_ui(read_snapshot); CHECK(strstr(snapshot, "alarm.cancel") && strstr(snapshot, "clock.save") && strstr(snapshot, "关闭"), "detail exposes deletion, editing and enabling through real capabilities");
    press("删除闹钟"); CHECK(WAIT_FOR(!strcmp(ink_ui_page_name(), "Clock"), 5000), "deletion refreshes and returns to the list"); quiet(); on_ui(verify_rows);
}
static void input_limits(void) {
    press("+ 加闹钟"); name_alarm("1234567890123456"); press("保存闹钟");
    CHECK(!strcmp(ink_ui_page_name(), "ClockAlarm"), "overlong names stay in the form");
    on_ui(read_alarms); CHECK(count == 0, "invalid input does not create an alarm");
    wanted = "名称最多15个字，请缩短后保存"; on_ui(locate_label); CHECK(found, "touch failure displays a Chinese correction");
    on_ui(read_snapshot); CHECK(strstr(snapshot, wanted), "AI sees the same visible error");
    capture("name-error");
    CHECK(AI("clock.save", NULL) && g_rc.status == INK_RC_INVALID, "AI receives the same name validation failure"); quiet();
    name_alarm("一二三四五六七八九十一二三四五"); press("保存闹钟");
    on_ui(read_alarms); CHECK(count == 1 && !strcmp(alarms[0].title, "一二三四五六七八九十一二三四五"), "fifteen Han characters save without truncation");
    press("7:00"); name_input("一二三四五六七八九十一二三四五六", "一二三四五六七八九十一二三四五");
    on_ui(read_snapshot); CHECK(strstr(snapshot, "一二三四五六七八九十一二三四五") && !strstr(snapshot, "�"), "the byte limit preserves a whole UTF-8 prefix");
    press("保存闹钟"); on_ui(read_alarms);
    CHECK(count == 1 && strlen(alarms[0].title) == 45, "sixteen Han characters cannot split the stored UTF-8 title");
    press("7:00"); press("删除闹钟"); quiet(); on_ui(read_alarms); CHECK(count == 0, "the boundary fixture is deleted through the page");
}
static void edit_alarms(void) {
    on_ui(read_alarms); uint32_t original = alarms[0].id;
    press("7:30"); CHECK(!strcmp(ink_ui_page_name(), "ClockAlarm"), "touching a row opens the actual edit form");
    on_ui(read_snapshot); capture("edit");
    press("时＋"); press("分＋"); press("每天"); name_alarm("晨起"); press("保存闹钟");
    on_ui(read_alarms); CHECK(count == 3 && alarms[0].id == original && alarms[0].hour == 8 && alarms[0].min == 31 &&
        alarms[0].weekdays == 127 && !strcmp(alarms[0].title, "晨起"), "editing changes all fields while retaining the original ID");
    press("8:31"); press("工作日"); press("关闭"); press("保存闹钟"); on_ui(read_alarms);
    CHECK(!alarms[0].enabled && alarms[0].next_s == 0 && alarms[0].weekdays == 62, "detail saves its repeat and disabled state");
    press("8:31"); press("开启"); press("保存闹钟"); on_ui(read_alarms);
    CHECK(alarms[0].enabled && alarms[0].next_s > ink_time_now_s(), "detail can enable the same alarm again");
    toggle("9:00"); on_ui(read_alarms); CHECK(!alarms[1].enabled && alarms[1].next_s == 0, "the list switch disables and unschedules the alarm");
    toggle("9:00"); on_ui(read_alarms); CHECK(alarms[1].enabled && alarms[1].next_s > ink_time_now_s(), "the list switch enables and schedules the same alarm");
    toggle("9:00"); edited = true; on_ui(verify_rows); on_ui(verify_list); save_scene();
    char args[64]; snprintf(args, sizeof args, "{\"id\":%u,\"minute\":60}", (unsigned)original);
    CHECK(AI("alarm.update", args) && g_rc.status == INK_RC_INVALID, "the update capability preserves time validation");
    CHECK(AI("alarm.update", "{\"id\":4294967295,\"enabled\":false}") && g_rc.status == INK_RC_STALE, "absent IDs cannot create an alarm");
    char owner[16]; snprintf(owner, sizeof owner, "%s", ink_member_current()->id);
    CHECK(ink_member_switch("kid") == 0, "switch to the synthetic child for update isolation"); quiet();
    call_ok("app.open", "{\"id\":\"clock\"}"); on_ui(read_snapshot); CHECK(strstr(snapshot, "\"total\":0"), "the child sees no adult alarms");
    snprintf(args, sizeof args, "{\"id\":%u,\"enabled\":false}", (unsigned)original);
    CHECK(AI("alarm.update", args) && g_rc.status == INK_RC_STALE, "the child cannot edit or toggle the adult alarm");
    CHECK(ink_member_switch(owner) == 0, "return to the original member"); quiet(); call_ok("app.open", "{\"id\":\"clock\"}");
    on_ui(verify_rows); on_ui(verify_list);
}
static void ring_actions(void) {
    on_ui(read_alarms); ink_alarm_t first = alarms[0];
    for (int i = 1; i < count; i++) if (alarms[i].next_s < first.next_s) first = alarms[i];
    ink_time_set(first.next_s, false);
    CHECK(WAIT_FOR(!strcmp(active_state(), "alarm"), 5000), "the existing service raises a real alarm interruption"); quiet(); on_ui(read_snapshot);
    CHECK(strstr(snapshot, first.title) && strstr(snapshot, "再睡 10 分钟") && strstr(snapshot, "这次不再响"), "ringing snapshot exposes the displayed alarm and buttons");
    CHECK(AI("interrupt.list", NULL) && g_rc.status == INK_RC_DONE && strstr(g_result, "alarm.snooze") && strstr(g_result, "alarm.dismiss"), "both ringing buttons use existing capabilities");
    int64_t before = ink_time_now_s();
    press("再睡 10 分钟"); CHECK(WAIT_FOR(strcmp(active_state(), "alarm"), 5000), "touch snoozes the interruption");
    int64_t after = ink_time_now_s();
    on_ui(read_alarms); ink_alarm_t snoozed = {0};
    for (int i = 0; i < count; i++) if (alarms[i].id == first.id) snoozed = alarms[i];
    CHECK(snoozed.snooze_until >= before + 600 && snoozed.snooze_until <= after + 600 && snoozed.next_s == snoozed.snooze_until, "touch saved ten minutes from its actual invocation time");
    ink_time_set(snoozed.next_s, false); CHECK(WAIT_FOR(!strcmp(active_state(), "alarm"), 5000), "the snoozed alarm rings again"); quiet();
    char args[48]; snprintf(args, sizeof args, "{\"id\":%u}", (unsigned)first.id); call_ok("alarm.dismiss", args);
    CHECK(WAIT_FOR(strcmp(active_state(), "alarm"), 5000), "AI dismisses through the same existing ringing capability");
    quiet(); CHECK(!strcmp(ink_ui_page_name(), "Clock"), "the clock page survives the interruption");
    on_ui(verify_rows);
}
static void host_phase(void *arg) { (void)arg; const char *p = getenv("INK_CLOCK_PHASE"); snprintf(phase, sizeof phase, "%s", p ? p : "create"); }
static void run(void *arg) {
    (void)arg; CHECK(WAIT_FOR(ink_member_space_open(), 10000), "the real member space opens"); quiet();
    ink_time_set(1793803620, false); call_ok("app.open", "{\"id\":\"clock\"}");
    if (!strcmp(phase, "create")) { input_limits(); create_alarms(); ring_actions(); edit_alarms(); }
    else { edited = true; on_ui(verify_rows); on_ui(verify_list); }
    ink_db_flush_all(); CHECK(WAIT_FOR(ink_db_idle() && !ink_storage_pending(), 10000), "all schedule writes reached simulated flash");
    ink_probe_count("test.finished");
}
void ink_main(void) {
    vtk_host_call(host_phase, NULL); product_main();
    osal_task_cfg_t cfg = {.name = "clock-test", .fn = run, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start the test-only driver");
}
