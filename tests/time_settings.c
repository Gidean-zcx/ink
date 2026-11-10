/* Synthetic whole-product checks; no host clock, hardware or external server. */
#include "ink/ui_shell.h"
#include "ink/sys/time.h"
#define ink_main product_main
#include "../system/boot/main.c"
#undef ink_main
#include "integration/whole_product.h"
#include <stdlib.h>

static char snapshot[8192], wanted_cap[96];
static bool by_label;
static int ordinal, seen, x, y;
static bool clock_controls_deleted;
static void control_deleted(lv_event_t *e) { (void)e; clock_controls_deleted = true; }
static void watch_clock_controls(lv_obj_t *o) {
    const char *cap = ink_shell_bound_cap(o);
    if (cap && !strcmp(cap, "shell.time_edit")) lv_obj_add_event_cb(o, control_deleted, LV_EVENT_DELETE, NULL);
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) watch_clock_controls(lv_obj_get_child(o, (int32_t)i));
}
static void observe_controls(void) { clock_controls_deleted = false; watch_clock_controls(lv_screen_active()); }
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void quiet(void) { CHECK(WAIT_FOR(settled(), 10000), "display settles"); }
static void read_page(void) {
    CHECK(ink_ui_snapshot(snapshot, sizeof snapshot, NULL) > 0, "page exports AI snapshot");
    CHECK(ink_ui_context_snapshot(g_snap, sizeof g_snap, NULL) > 0, "page fits production AI context");
}
static bool page_has(const char *s) { on_ui(read_page); return strstr(snapshot, s) != NULL; }
static void locate_at(lv_obj_t *o) {
    if (lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN)) return;
    const char *cap = ink_shell_bound_cap(o);
    bool match = by_label ? lv_obj_check_type(o, &lv_label_class) && !strcmp(lv_label_get_text(o), wanted_cap) : cap && !strcmp(cap, wanted_cap);
    if (match && seen++ == ordinal) {
        lv_area_t a; lv_obj_get_coords(o, &a); x = (a.x1 + a.x2) / 2; y = (a.y1 + a.y2) / 2;
        CHECK(!lv_obj_has_state(o, LV_STATE_DISABLED) && a.x1 >= 0 && a.x2 < ink_ui_px(INK_CANVAS_W) &&
            a.y1 >= 0 && a.y2 < ink_ui_px(INK_CANVAS_H), "enabled touch target fits the screen");
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) locate_at(lv_obj_get_child(o, (int32_t)i));
}
static void locate(void) { seen = 0; x = y = -1; lv_obj_update_layout(lv_screen_active()); locate_at(lv_screen_active()); }
static void press(const char *cap, int at) {
    quiet(); by_label = false; snprintf(wanted_cap, sizeof wanted_cap, "%s", cap); ordinal = at; on_ui(locate);
    CHECK(x >= 0 && y >= 0, "touch binding exists: %s #%d", cap, at);
    if (x >= 0) tap(x, y);
    quiet();
}
static void press_label(const char *label) {
    quiet(); by_label = true; snprintf(wanted_cap, sizeof wanted_cap, "%s", label); ordinal = 0; on_ui(locate);
    CHECK(x >= 0 && y >= 0, "visible list label: %s", label);
    if (x >= 0) tap(x, y);
    quiet();
}
static void capture(void *arg) { sim_display_snapshot(arg); }
static void evidence(const char *name) { quiet(); vtk_event_at(vtk_now() + SIM_MS(1), capture, (void *)name); delay_ms(5); }
static void setting(const char *id, int value) {
    char a[96]; snprintf(a, sizeof a, "{\"id\":\"%s\",\"value\":%d}", id, value);
    CHECK(AI("settings.set", a) && g_rc.status == INK_RC_DONE, "AI changes %s (%s)", id, g_rc.detail);
    CHECK(WAIT_FOR(ink_setting_get(id) == value, 3000), "setting reaches requested value");
}
static void set_clock(int64_t utc) {
    char a[64]; snprintf(a, sizeof a, "{\"utc_s\":%lld}", (long long)utc);
    CHECK(AI("time.set", a) && g_rc.status == INK_RC_DONE, "AI uses the same time.set (%s)", g_rc.detail);
}
static int64_t local(int y0, int m, int d, int h, int mi, int s) {
    ink_tm_t t = {.year = y0, .month = m, .day = d, .hour = h, .min = mi, .sec = s}; return ink_time_from_local(&t);
}
static ink_alarm_t find_alarm(uint32_t id) {
    ink_alarm_t a = {0}; int cursor = 0;
    while (ink_alarm_next(&cursor, INK_ALARM, &a) == 1) if (a.id == id) return a;
    CHECK(false, "alarm %u exists", (unsigned)id); return a;
}
static volatile int fired;
static int64_t fired_s;
static void alarm_event(void *ctx, const char *id, const ink_args_t *args) {
    (void)ctx; (void)id; (void)args; fired++; fired_s = ink_time_now_s();
}
static void manual(void) {
    setting("time.auto", 0); setting("time.zone", 1);
    set_clock(local(2026, 11, 10, 10, 58, 0));
    CHECK(AI("shell.about", NULL) && g_rc.status == INK_RC_DONE, "open existing About entry"); quiet();
    CHECK(page_has("shell.time"), "About exposes Date & Time without changing the settings home");
    press_label("日期和时间"); CHECK(!strcmp(ink_ui_page_name(), "SetTime"), "touch opens Date & Time");
    evidence("time-zh");
    setting("system.language", 1); quiet();
    CHECK(page_has("Date & Time") && page_has("Automatic Time Zone"), "new page is translated"); evidence("time-en");
    setting("system.language", 0); quiet(); on_ui(observe_controls);
    CHECK(WAIT_FOR(({ int64_t minute = 0; ink_state_get_int(ink_state_find("time.minute"), &minute) && minute == 659; }), 70000), "minute service reaches the next minute");
    CHECK(page_has("10:59") && !clock_controls_deleted, "minute tick updates only the clock label");
    set_clock(local(2026, 11, 10, 10, 58, 0)); quiet(); press("shell.time_edit", 0);
    CHECK(page_has("time.set") && page_has("shell.time_step"), "editor exposes save and draft actions");
    press("shell.time_step", 5); /* day + */
    CHECK(page_has("日 11"), "touch step changes the day draft");
    const int fields[] = {0, 1, 3, 4};
    for (size_t i = 0; i < sizeof fields / sizeof *fields; i++) {
        char a[48]; snprintf(a, sizeof a, "{\"field\":%d,\"delta\":1}", fields[i]);
        CHECK(AI("shell.time_step", a) && g_rc.status == INK_RC_DONE, "AI uses the same draft capability");
    }
    int64_t expected = local(2027, 12, 11, 11, 59, 0);
    press("time.set", 0);
    CHECK(ink_time_now_s() >= expected && ink_time_now_s() < expected + 5, "touch saves the complete local date and 24-hour time");
    CHECK(WAIT_FOR(({ int64_t v = 0; ink_state_get_int(ink_state_find("time.minute"), &v) && v == 719; }), 3000), "status bar minute follows manual time");
    CHECK(AI("alarm.set", "{\"hour\":12,\"minute\":0,\"title\":\"SYNTH_TIME\"}") && g_rc.status == INK_RC_DONE, "create alarm after manual change");
    unsigned id = 0; sscanf(g_rc.object, "alarm:%u", &id);
    CHECK(find_alarm(id).next_s == expected + 60, "alarm uses manually selected date");
    CHECK(WAIT_FOR(fired == 1, 65000) && fired_s >= expected + 60 && fired_s <= expected + 61, "alarm fires at the displayed manual minute");
    CHECK(AI("alarm.dismiss", NULL) && g_rc.status == INK_RC_DONE, "dismiss manual alarm");
    CHECK(AI("system.home", NULL), "reset navigation for leap-date editing");
    set_clock(local(2028, 2, 28, 12, 0, 0));
    CHECK(AI("shell.time_edit", NULL) && g_rc.status == INK_RC_DONE, "edit a leap-year date");
    CHECK(AI("shell.time_step", "{\"field\":2,\"delta\":1}") && g_rc.status == INK_RC_DONE && page_has("日 29"), "picker accepts February 29 in leap year");
    CHECK(AI("shell.time_step", "{\"field\":0,\"delta\":1}") && g_rc.status == INK_RC_DONE && page_has("日 28"), "picker clamps invalid day when changing the year");
    CHECK(AI("shell.time", NULL), "return to date settings");
    setting("time.auto", 1);
    CHECK(WAIT_FOR(llabs(ink_time_now_s() - (g_sim.wall_start_us / 1000000 + (int64_t)(osal_now_us() / 1000000))) <= 2, 5000), "switching back to auto immediately uses current network time");
    CHECK(AI("shell.time_edit", NULL) && g_rc.status == INK_RC_REJECTED, "automatic mode prevents manual editor");
}
static void zones(void) {
    CHECK(AI("system.home", NULL), "reset navigation between independent scenarios");
    setting("time.auto", 0); setting("time.zone", 1);
    set_clock(local(2026, 11, 10, 0, 59, 0));
    CHECK(AI("alarm.set", "{\"hour\":2,\"minute\":0,\"weekdays\":127,\"title\":\"SYNTH_ZONE\"}") && g_rc.status == INK_RC_DONE, "create local recurring alarm");
    unsigned id = 0; sscanf(g_rc.object, "alarm:%u", &id); int64_t old = find_alarm(id).next_s;
    CHECK(AI("shell.time_zones", NULL) && g_rc.status == INK_RC_DONE, "open city list"); quiet();
    CHECK(page_has("中国 · 上海") && page_has("日本 · 东京"), "city list exports Chinese names and IANA names");
    press_label("日本 · 东京");
    CHECK(ink_setting_get("time.zone") == 2 && !strcmp(ink_time_zone(), "Asia/Tokyo"), "touch selects Tokyo");
    CHECK(page_has("Asia/Tokyo · 已选择"), "city selection has visible and AI-readable feedback");
    ink_alarm_t alarm = find_alarm(id);
    CHECK(alarm.next_s == old + 15 * 3600, "alarm is rescheduled to 02:00 in the new zone");
    CHECK(AI("time.now", NULL) && strstr(g_result, "09:59"), "AI and status time use Tokyo");
    CHECK(AI("calendar.open", NULL) && g_rc.status == INK_RC_DONE, "calendar opens after zone change");
    ink_alarm_t reminder = {.kind = INK_REMINDER, .at_utc = local(2026, 11, 10, 12, 0, 0)};
    snprintf(reminder.title, sizeof reminder.title, "SYNTH_UTC");
    CHECK(ink_alarm_add(&reminder) > 0, "seed a UTC reminder for calendar projection");
    CHECK(WAIT_FOR(page_has("12:00"), 3000), "calendar shows UTC reminder in Tokyo time");
    setting("time.zone", 0);
    CHECK(WAIT_FOR(page_has("11:00"), 3000), "visible calendar follows changed zone through service notification");
    char cancel[48]; snprintf(cancel, sizeof cancel, "{\"id\":%u}", (unsigned)reminder.id);
    CHECK(AI("reminder.cancel", cancel) && g_rc.status == INK_RC_DONE, "remove synthetic reminder");
    setting("time.zone", 2);
    CHECK(AI("shell.time", NULL), "return to time settings");
    setting("time.zone_auto", 1);
    CHECK(!strcmp(ink_time_zone(), "Asia/Tokyo"), "automatic zone follows synthetic hello.time_zone");
    setting("time.zone_auto", 0); setting("time.zone", 0); setting("time.zone_auto", 1);
    CHECK(!strcmp(ink_time_zone(), "Asia/Tokyo"), "automatic and manual zone selections stay separate");
    setting("time.zone_auto", 0); setting("time.zone", 2);
    int before = fired;
    set_clock(alarm.next_s - 1);
    CHECK(WAIT_FOR(fired == before + 1, 3000) && fired_s == alarm.next_s, "alarm fires at its new local time");
    CHECK(AI("alarm.dismiss", NULL), "dismiss zone alarm");
    char args[48]; snprintf(args, sizeof args, "{\"id\":%u}", id); CHECK(AI("alarm.cancel", args), "cancel synthetic repeating alarm");
}
static void dst(void) {
    setting("time.zone", 7);
    ink_tm_t old; ink_time_to_local(local(2006, 3, 12, 2, 30, 0), &old);
    CHECK(old.hour == 2 && !old.dst, "2006 uses April transition rather than the post-2007 March rule");
    ink_time_to_local(local(2006, 4, 2, 2, 30, 0), &old);
    CHECK(old.hour == 3 && old.dst, "manual historical date uses the 2006 US transition");
    int64_t gap = local(2027, 3, 14, 2, 30, 0), fold = local(2027, 11, 7, 1, 30, 0);
    ink_tm_t t; ink_time_to_local(gap, &t);
    CHECK(t.hour == 3 && t.min == 30 && t.dst, "US missing hour moves forward");
    ink_time_to_local(fold, &t); CHECK(t.hour == 1 && t.min == 30 && t.dst, "US repeated hour picks the first occurrence");
    ink_time_to_local(fold + 3600, &t); CHECK(t.hour == 1 && !t.dst, "US repeated hour ends in standard time");
    set_clock(local(2027, 3, 14, 1, 59, 59));
    CHECK(AI("alarm.set", "{\"hour\":2,\"minute\":30,\"weekdays\":127}") && g_rc.status == INK_RC_DONE, "create DST-gap alarm");
    unsigned id = 0; sscanf(g_rc.object, "alarm:%u", &id); CHECK(find_alarm(id).next_s == gap, "gap alarm schedules at 03:30");
    int before = fired; set_clock(gap - 1);
    CHECK(WAIT_FOR(fired == before + 1, 3000), "gap alarm rings once"); CHECK(AI("alarm.dismiss", NULL), "dismiss gap alarm");
    CHECK(find_alarm(id).next_s == local(2027, 3, 15, 2, 30, 0), "gap alarm returns to the usual hour next day");
    char a[48]; snprintf(a, sizeof a, "{\"id\":%u}", id); CHECK(AI("alarm.cancel", a), "cancel gap alarm");
    set_clock(fold - 1); CHECK(AI("alarm.set", "{\"hour\":1,\"minute\":30,\"weekdays\":127}") && g_rc.status == INK_RC_DONE, "create fold alarm");
    sscanf(g_rc.object, "alarm:%u", &id); /* Calling the bus consumes a few virtual milliseconds. */
    CHECK(WAIT_FOR(fired == before + 2, 3000), "fold alarm rings at first occurrence"); CHECK(AI("alarm.dismiss", NULL), "dismiss fold alarm");
    CHECK(find_alarm(id).next_s > fold + 3600, "fold does not ring twice");
    snprintf(a, sizeof a, "{\"id\":%u}", id); CHECK(AI("alarm.cancel", a), "cancel fold alarm");
    setting("time.zone", 5);
    gap = local(2027, 3, 28, 1, 30, 0); ink_time_to_local(gap, &t);
    CHECK(t.hour == 2 && t.dst, "EU missing hour moves forward");
    fold = local(2027, 10, 31, 1, 30, 0); ink_time_to_local(fold, &t);
    CHECK(t.hour == 1 && t.dst, "EU fold picks first occurrence"); ink_time_to_local(fold + 3600, &t);
    CHECK(t.hour == 1 && !t.dst, "EU fold ends in standard time");
    ink_time_to_zone(0, fold, &t); CHECK(t.offset_min == 480 && !strcmp(ink_time_zone(), "Europe/London"), "world-clock API converts without changing selected zone");
}
static volatile int pin_done, pin_result;
static uint64_t locked_at;
static void pin_told(void *ctx, int result, uint32_t wait_s) { (void)ctx; (void)wait_s; pin_result = result; pin_done++; }
static void observe_lock(void *ctx, int h, const char *id) {
    (void)ctx; (void)h; (void)id; if (!strcmp(power_state(), "locked")) locked_at = osal_now_us();
}
static void wake_down(void *arg) { (void)arg; sim_input_key(HAL_KEY_POWER, true); }
static void wake_up(void *arg) { (void)arg; sim_input_key(HAL_KEY_POWER, false); }
static lv_obj_t *saved_screen;
static void save_screen(void) { saved_screen = lv_screen_active(); }
static bool same_screen;
static void compare_screen(void) { same_screen = saved_screen == lv_screen_active(); read_screen(); }
static void grace(void) {
    CHECK(AI("system.home", NULL), "reset navigation before family scenario");
    CHECK(AI("shell.family", NULL), "open family settings"); quiet();
    for (int i = 0; i < 4 && !page_has("shell.return"); i++) { CHECK(AI("family.next", NULL), "find return setting"); quiet(); }
    CHECK(page_has("shell.return"), "family setting exposes return page: %.150s", snapshot); press_label("离开多久要重新验证");
    CHECK(page_has("300") && page_has("900"), "return page offers one, two, five and fifteen minutes");
    press("settings.set", 2);
    ink_confirm_view_t card = {0};
    CHECK(ink_confirm_list(&card, 1) == 1 && !strcmp(card.cap, "settings.set"), "return grace preserves existing identity confirmation");
    CHECK(ink_confirm_decide(card.token, true, INK_SRC_TOUCH) == 0, "person confirms grace change");
    CHECK(WAIT_FOR(ink_setting_get("member.idle_return_s") == 300, 5000), "touch selects five minutes");
    CHECK(AI("settings.set", "{\"id\":\"member.idle_return_s\",\"value\":60}") && g_rc.status == INK_RC_PENDING_CONFIRMATION, "adult AI uses the same identity-setting confirmation");
    CHECK(ink_setting_get("member.idle_return_s") == 300 && ink_confirm_list(&card, 1) == 1, "AI cannot silently change verification grace");
    char confirm[64]; snprintf(confirm, sizeof confirm, "{\"token\":%u}", (unsigned)card.token);
    CHECK(AI("confirm.accept", confirm) && g_rc.status == INK_RC_INVALID, "AI cannot confirm a verification change");
    CHECK(ink_confirm_decide(card.token, false, INK_SRC_TOUCH) == 0, "person keeps the five-minute selection");
    CHECK(ink_member_set_pin("owner", NULL, "1111", pin_told, NULL) == 0 && WAIT_FOR(pin_done > 0, 5000) && pin_result == INK_PIN_OK, "set synthetic PIN");
    CHECK(AI("system.home", NULL), "home is the return target"); quiet(); on_ui(save_screen);
    for (int seconds = 299; seconds <= 301; seconds += 2) {
        key(HAL_KEY_POWER); CHECK(WAIT_FOR(!strcmp(power_state(), "locked"), 3000), "lock starts grace timer");
        uint64_t at = locked_at + (uint64_t)seconds * 1000000;
        vtk_event_at(at * 1000, wake_down, NULL); vtk_event_at((at + 80000) * 1000, wake_up, NULL);
        if (at + 300000 > osal_now_us()) osal_task_delay_us(at + 300000 - osal_now_us());
        CHECK(WAIT_FOR(!strcmp(power_state(), "active"), 3000), "wake key is delivered"); on_ui(compare_screen);
        if (seconds == 299) CHECK(ink_member_space_open() && same_screen && strcmp(g_top, "member_gate"), "4:59 returns directly to the same page");
        else CHECK(!ink_member_space_open() && same_screen && !strcmp(g_top, "member_gate"), "5:01 requires PIN over the original page");
    }
    CHECK(AI("shell.time", NULL) && g_rc.status == INK_RC_REJECTED, "AI cannot bypass the verification gate");
    int before = pin_done; CHECK(ink_member_unlock("owner", "1111", pin_told, NULL) == 0 && WAIT_FOR(pin_done > before, 5000) && pin_result == INK_PIN_OK, "person unlocks synthetic account");
    CHECK(BY_HAND("system.switch_member", "{\"id\":\"kid\"}") && g_rc.status == INK_RC_DONE, "switch to child");
    const char *settings[] = {"time.auto", "time.zone", "time.zone_auto", "member.idle_return_s"};
    const int values[] = {1, 0, 1, 60};
    for (size_t i = 0; i < sizeof values / sizeof *values; i++) {
        char a[96]; snprintf(a, sizeof a, "{\"id\":\"%s\",\"value\":%d}", settings[i], values[i]);
        int64_t was = ink_setting_get(settings[i]);
        CHECK(BY_HAND("settings.set", a) && g_rc.status == INK_RC_REJECTED && ink_setting_get(settings[i]) == was, "child touch cannot change %s", settings[i]);
        CHECK(AI("settings.set", a) && g_rc.status == INK_RC_REJECTED, "child AI cannot change %s", settings[i]);
    }
    CHECK(AI("shell.return", NULL) && g_rc.status == INK_RC_REJECTED, "child cannot open return settings");
    CHECK(AI("time.set", "{\"utc_s\":1794308280}") && g_rc.status == INK_RC_REJECTED, "child cannot change the clock");
}
static void run(void *arg) {
    (void)arg; CHECK(WAIT_FOR(ink_member_space_open(), 10000), "member space opens"); test_start_home();
    CHECK(ink_cap_subscribe("alarm.fired", alarm_event, NULL, INK_EXEC_DIRECT) == 0, "observe alarm events");
    CHECK(ink_state_watch("power.state", observe_lock, NULL, INK_EXEC_DIRECT) == 0, "observe lock timestamp");
    ink_hub_demand(); CHECK(WAIT_FOR(ink_hub_online(), 10000), "synthetic network server connects");
    manual(); zones(); dst(); grace();
    CHECK(WAIT_FOR(ink_storage_pending() == 0, 10000), "writes settle"); ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name = "time-settings", .fn = run, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    if (osal_task_create(&cfg, NULL) != OSAL_OK) osal_panic("time settings test");
}
