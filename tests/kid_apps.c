/* Synthetic family policies through the real bus, UI, clock and flash. */
#include <stdlib.h>
#define ink_main product_main
#include "configured_boot.c"
#undef ink_main
#include "integration/whole_product.h"
#include "ink/sys/appreg.h"
#include "ink/sys/storage.h"
#include "ink/sys/time.h"
#include "ink/ui_shell.h"

static char phase[16];
static volatile bool pin_done;
static int pin_result;
static const char *wanted, *scene;
static lv_point_t point;
static bool found;
static void pin_reply(void *ctx, int result, uint32_t wait) { (void)ctx; (void)wait; pin_result = result; pin_done = true; }
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void quiet(void) { CHECK(WAIT_FOR(settled(), 15000), "the policy page settles"); }
static void inspect(void) { read_screen(); }
static bool page(const char *name) { on_ui(inspect); return !strcmp(g_top, name); }
static void ok(const char *cap, const char *args) {
    CHECK(AI(cap, args) && g_rc.status == INK_RC_DONE, "%s: %s (%.120s)", cap, ink_rc_name(g_rc.status), g_rc.detail); quiet();
}
static void locate(lv_obj_t *obj) {
    if (!lv_obj_is_visible(obj)) return;
    if (lv_obj_check_type(obj, &lv_label_class) && !strcmp(lv_label_get_text(obj), wanted)) {
        lv_area_t a; lv_obj_get_coords(obj, &a); point = (lv_point_t){(a.x1 + a.x2) / 2, (a.y1 + a.y2) / 2}; found = true;
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(obj); i++) locate(lv_obj_get_child(obj, (int32_t)i));
}
static void find_label(void) { found = false; lv_obj_update_layout(lv_screen_active()); locate(lv_screen_active()); locate(lv_layer_sys()); }
static void touch(const char *label) {
    quiet(); wanted = label; on_ui(find_label); CHECK(found, "visible control: %s", label);
    if (found) tap(point.x, point.y);
    quiet();
}
static void approve(void) {
    unsigned token = 0; CHECK(sscanf(g_rc.object, "confirm:%u", &token) == 1, "the policy has an actual confirmation");
    pin_done = false;
    CHECK(ink_confirm_accept_with_pin(token, "24681357", pin_reply, NULL) == 0 && WAIT_FOR(pin_done, 6000) && pin_result == INK_PIN_OK, "the parent enters a real local PIN");
    quiet();
}
static void change(const char *cap, const char *args) {
    CHECK(AI(cap, args) && g_rc.status == INK_RC_PENDING_CONFIRMATION, "adult policy changes require confirmation: %s", cap);
    approve();
}
static void parent(void) {
    pin_done = false;
    CHECK(ink_member_unlock("owner", "24681357", pin_reply, NULL) == 0 && WAIT_FOR(pin_done, 6000) && pin_result == INK_PIN_OK, "return to the authenticated adult"); quiet();
}
static void child(void) { CHECK(ink_member_switch("kid") == 0, "enter the child"); quiet(); }
static void clock_at(int day, int hour, int minute) {
    ink_tm_t t = {.year = 2026, .month = 11, .day = day, .hour = hour, .min = minute};
    ink_time_set(ink_time_from_local(&t), false); quiet();
}
static void capture_event(void *arg) {
    (void)arg; CHECK(settled(), "capture a settled physical frame"); ink_probe_count("test.snapshot.settled"); sim_display_snapshot(scene);
}
static void capture(const char *name) { scene = name; quiet(); vtk_event_at(vtk_now() + SIM_MS(1), capture_event, NULL); delay_ms(5); }
static void home(void) { ink_ui_home(); }
static void all_apps(void) {
    for (int i = 0; i < 2 && strcmp(ink_shell_board(), "KidApps"); i++) ink_shell_home_swipe(INK_SWIPE_LEFT);
}
static void denied(const char *cap, const char *args, bool by_hand) {
    CHECK(call_as(cap, args, by_hand ? INK_SRC_TOUCH : INK_SRC_AI) && g_rc.status == INK_RC_REJECTED, "list denies %s (%s)", cap, ink_rc_name(g_rc.status));
    CHECK(strstr(g_rc.detail, "家长没有打开这个应用"), "denial explains the parent application list: %.150s", g_rc.detail);
}
static void period(const char *args) { change("kid.period_set", args); }
static void policy(void) {
    pin_done = false;
    CHECK(ink_member_set_pin("owner", "", "24681357", pin_reply, NULL) == 0 && WAIT_FOR(pin_done, 6000) && pin_result == INK_PIN_OK, "set the synthetic parent verifier");
    clock_at(10, 12, 0);
    change("member.add", "{\"id\":\"kid2\",\"name\":\"合成孩子二\",\"role\":\"child\"}");
    change("kid.app_set", "{\"id\":\"kid\",\"app\":\"clock\",\"mode\":0}");
    change("kid.app_set", "{\"id\":\"kid\",\"app\":\"memo\",\"mode\":0}");
    CHECK(ink_member_app_mode("kid", "clock") == 0 && ink_member_app_mode("kid2", "clock") == 1, "each child owns a separate app list");
    child(); on_ui(home); quiet(); on_ui(inspect);
    CHECK(strstr(g_snap, "页面=KidHome"), "inspect the actual child home: %.120s", g_snap);
    wanted = "时钟"; on_ui(find_label); CHECK(!found, "KidHome hides the removed app");
    ok("home.next", NULL); on_ui(inspect); CHECK(strstr(g_snap, "KidApps"), "open the actual child All page");
    int at = 0, pages = 0; const char *paging = strstr(g_snap, "工具页=");
    CHECK(paging && sscanf(paging, "工具页=%d/%d", &at, &pages) == 2 && pages > 0, "read the actual All page count");
    for (int i = 0; i < pages; i++) {
        wanted = "时钟"; on_ui(find_label); CHECK(!found, "every All page hides the removed app");
        if (i + 1 < pages) ok("home.next", NULL);
    }
    ok("app.list", NULL); CHECK(!strstr(g_result, "\"id\":\"clock\"") && !strstr(g_result, "\"id\":\"memo\""), "AI directory hides removed apps");
    denied("app.open", "{\"id\":\"clock\"}", false);
    denied("shell.launch", "{\"id\":\"clock\"}", true);
    denied("memo.edit", NULL, false);
    denied("app.open", "{\"id\":\"files\"}", false);
    CHECK(!ink_cap_ai_callable(ink_cap_find("memo.edit")), "AI capability discovery obeys the list");
    CHECK(AI("kid.app_set", "{\"id\":\"kid\",\"app\":\"clock\",\"mode\":1}") && g_rc.status == INK_RC_REJECTED, "child AI cannot change the list");
    CHECK(call_as("kid.period_set", "{\"id\":\"kid\",\"index\":0,\"start\":1260,\"end\":420,\"days\":127}", INK_SRC_TOUCH) && g_rc.status == INK_RC_REJECTED, "child buttons cannot change restricted hours");
    CHECK(call_as("kid.app_set", "{\"id\":\"kid\",\"app\":\"memo\",\"mode\":1}", INK_SRC_CONFIRMED) && g_rc.status == INK_RC_REJECTED, "a confirmed source cannot make the child an adult");
    CHECK(AI("family.kid_apps", NULL) && g_rc.status == INK_RC_REJECTED, "child cannot open the app settings");
    CHECK(AI("shell.kid_settings", NULL) && g_rc.status == INK_RC_REJECTED, "child cannot open SetKid");
    CHECK(AI("kid.policy_get", "{\"id\":\"kid2\"}") && g_rc.status == INK_RC_REJECTED, "child cannot read a sibling policy");
    parent();
    change("kid.app_set", "{\"id\":\"kid\",\"app\":\"clock\",\"mode\":2}");
    period("{\"id\":\"kid\",\"index\":0,\"start\":1260,\"end\":420,\"days\":2}");
    period("{\"id\":\"kid\",\"index\":1,\"start\":480,\"end\":960,\"days\":4}");
    child(); clock_at(9, 20, 59);
    CHECK(!ink_member_time_up(), "Monday before 21:00 is outside the overnight period");
    ok("app.open", "{\"id\":\"reader\"}");
    delay_ms(61000); CHECK(WAIT_FOR(page("KidTimeUp"), 5000), "the existing deadline enters TimeUp at the start boundary");
    key(HAL_KEY_HOME); quiet(); CHECK(page("home"), "restricted hours allow the real HOME key to reach permitted applications");
    on_ui(all_apps); quiet(); wanted = "时钟"; on_ui(find_label); CHECK(found, "the permitted clock tile is visible");
    double touches = sim_metric_value("home.taps");
    if (found) tap(point.x, point.y - ink_ui_px(INK_CIRCLE / 2 + INK_GAP_S + ink_ui_line_height(INK_FONT_NOTE) / 2));
    CHECK(WAIT_FOR(sim_metric_value("home.taps") > touches, 1000), "the real clock tap reaches its shared capability");
    quiet(); CHECK(page("Clock"), "the actual app tile opens an exempt clock during restricted hours: %.60s", g_top);
    ok("app.open", "{\"id\":\"clock\"}"); CHECK(page("Clock"), "an explicitly exempt app opens during restricted hours");
    CHECK(AI("app.open", "{\"id\":\"reader\"}") && g_rc.status == INK_RC_REJECTED && page("KidTimeUp"), "ordinary app opens TimeUp during restricted hours");
    ok("app.open", "{\"id\":\"phone\"}"); CHECK(page("Phone"), "required phone remains available");
    clock_at(10, 6, 59); CHECK(ink_member_time_up(), "Tuesday morning belongs to Monday's overnight period");
    clock_at(10, 7, 0); CHECK(!ink_member_time_up(), "the overnight end boundary is exclusive");
    clock_at(10, 8, 0); CHECK(ink_member_time_up(), "Tuesday's second daytime period applies");
    clock_at(10, 16, 0); CHECK(!ink_member_time_up(), "the daytime end boundary is exclusive");
    clock_at(11, 6, 0); CHECK(!ink_member_time_up(), "an unselected start weekday does not restrict Wednesday morning");
    clock_at(8, 23, 0); CHECK(!ink_member_time_up(), "an unselected Sunday is unrestricted");
    parent(); clock_at(10, 12, 0);
    change("member.time_set", "{\"id\":\"kid\",\"minutes\":1}"); child();
    ok("app.open", "{\"id\":\"clock\"}");
    delay_ms(61000); CHECK(ink_member_time_up() && !ink_member_app_time_allows("clock"), "daily allowance overrides a restricted-hours exemption");
    CHECK(WAIT_FOR(page("KidTimeUp"), 5000), "daily expiry updates an exempt app inside an already restricted period");
    CHECK(AI("app.open", "{\"id\":\"clock\"}") && g_rc.status == INK_RC_REJECTED && page("KidTimeUp"), "daily allowance still blocks the exempt clock");
    key(HAL_KEY_HOME); quiet(); CHECK(page("KidTimeUp"), "the daily allowance still prevents HOME from bypassing TimeUp");
    ok("app.open", "{\"id\":\"message\"}"); CHECK(!strcmp(ink_ui_page_name(), "Sms"), "required SMS survives both restrictions");
    parent(); change("member.time_set", "{\"id\":\"kid\",\"minutes\":0}");
    ok("shell.kid_settings", NULL); touch("应用名单"); CHECK(page("SetKidApps"), "SetKid opens its real application list");
    CHECK(strstr(g_snap, "kid.app_set"), "the application page exposes its actual actions"); capture("SetKidApps");
    bool memo_visible = false;
    for (int i = 0; i < 10 && !memo_visible; i++) {
        wanted = "备忘"; on_ui(find_label); memo_visible = found;
        if (!memo_visible) ok("family.next", NULL);
    }
    CHECK(memo_visible, "the actual application list includes memo");
    touch("备忘"); CHECK(page("confirm"), "tapping the app uses the same parent confirmation");
    touch("输入密码");
    for (const char *p = "24681357"; *p; p++) { char digit[2] = {*p, 0}; touch(digit); }
    touch("确定"); CHECK(WAIT_FOR(ink_member_app_mode("kid", "memo") == 1, 6000), "the actual app-list button changes the policy after local PIN entry");
    ok("shell.back", NULL); ok("family.next", NULL); touch("禁用时段"); CHECK(page("SetKidPeriods"), "SetKid opens restricted hours");
    CHECK(strstr(g_snap, "kid.period_set") && strstr(g_snap, "21:00") && strstr(g_snap, "07:00"), "restricted hours expose actual stored times"); capture("SetKidPeriods");
    ok("family.next", NULL); touch("星期二"); CHECK(page("confirm"), "weekday touch uses kid.period_set");
    touch("输入密码");
    for (const char *p = "24681357"; *p; p++) { char digit[2] = {*p, 0}; touch(digit); }
    touch("确定"); quiet();
    ink_kid_period_t t; ink_member_period("kid", 0, &t); CHECK(t.days == 6, "weekday touch changes the stored weekday mask");
    ink_db_flush_all(); CHECK(WAIT_FOR(ink_db_idle() && !ink_storage_pending(), 15000), "policies are durable before restarting the process");
}
static void restart(void) {
    CHECK(ink_member_has_pin("owner"), "restart preserves the parent's verifier");
    CHECK(ink_member_app_mode("kid", "clock") == 2 && ink_member_app_mode("kid", "memo") == 1, "app list and exemption survive a process restart");
    ink_kid_period_t t; ink_member_period("kid", 0, &t);
    CHECK(t.start == 1260 && t.end == 420 && t.days == 6, "overnight period and weekdays survive restart");
    ink_member_period("kid", 1, &t); CHECK(t.start == 480 && t.end == 960 && t.days == 4, "a second independent period survives restart");
    child(); clock_at(10, 12, 0);
    CHECK(ink_member_time_up() && ink_member_app_time_allows("clock"), "the rebooted policy is enforced");
    ok("app.open", "{\"id\":\"clock\"}"); CHECK(page("Clock"), "the rebooted exception opens");
}
static void locale(void) {
    parent(); ok("settings.set", "{\"id\":\"system.language\",\"value\":1}");
    ok("shell.kid_settings", NULL); ok("family.kid_apps", NULL);
    wanted = "Allowed Apps"; on_ui(find_label); CHECK(found, "the application page has its English heading");
    wanted = "Reading"; on_ui(find_label); CHECK(found, "the application names are translated");
    double missing = sim_metric_value("i18n.missing");
    capture("SetKidAppsEnglish");
    for (int i = 0; i < 3; i++) ok("family.next", NULL);
    CHECK(sim_metric_value("i18n.missing") == missing, "every application row has an English translation");
    ok("family.kid_periods", NULL);
    wanted = "Restricted Hours"; on_ui(find_label); CHECK(found, "the period page has its English heading");
    wanted = "Start time"; on_ui(find_label); CHECK(found, "time labels are translated");
    capture("SetKidPeriodsEnglish");
    for (int i = 0; i < 3; i++) ok("family.next", NULL);
    CHECK(sim_metric_value("i18n.missing") == missing, "all period and weekday rows have English translations");
}
static void owner_line(void *arg) {
    const ink_cap_t *c = arg;
    printf("OWNERSHIP %s %s\n", c->app, c->id);
}
static void forbidden_handler(ink_call_t *c) { (void)c; CHECK(false, "a disabled app handler must never run"); }
static void ownership(void) {
    static const ink_cap_t alias = {.id = "audit14.alias", .app = "memo", .title = "合成归属测试", .child_ok = true, .run = forbidden_handler};
    CHECK(!ink_cap_register(&alias, 1), "register a capability whose name differs from its app");
    pin_done = false;
    CHECK(ink_member_set_pin("owner", "", "24681357", pin_reply, NULL) == 0 && WAIT_FOR(pin_done, 6000) && pin_result == INK_PIN_OK, "set the synthetic parent verifier");
    for (int i = 0; i < ink_app_count(); i++) {
        const ink_app_def_t *a = ink_app_at(i);
        if (!a->child_ok || !strcmp(a->id, "home") || !strcmp(a->id, "phone") || !strcmp(a->id, "contacts") || !strcmp(a->id, "message")) continue;
        char args[96]; snprintf(args, sizeof args, "{\"id\":\"kid\",\"app\":\"%s\",\"mode\":0}", a->id);
        change("kid.app_set", args);
        CHECK(ink_member_app_mode("kid", a->id) == 0, "parent removes %s from the child's list", a->id);
    }
    child();
    CHECK(!strcmp(ink_cap_find("photo.open")->app, "gallery"), "photo.open belongs to Gallery");
    CHECK(!ink_cap_find("shell.ai_stop")->app, "stopping speech remains a system action");
    unsigned total = 0, denied_count = 0;
    for (int i = 0; ink_cap_at(i); i++) {
        const ink_cap_t *c = ink_cap_at(i);
        static const char *const emergency[] = {"phone.emergency", "phone.emergency_open", "phone.emergency_key", "phone.emergency_close", "phone.emergency_hangup"};
        bool public_call = false;
        for (size_t k = 0; k < sizeof emergency / sizeof *emergency; k++) if (!strcmp(c->id, emergency[k])) public_call = true;
        if (public_call) { CHECK(!c->app && c->child_ok, "emergency capability belongs to the system: %s", c->id); continue; }
        /* Coverage guards for application tables, including the previously missed aliases. */
        static const char *const domains[] = {"reader.", "ai.", "calc.", "camera.", "clock.", "files.", "file.", "gallery.", "photo.", "memo.", "message.", "sms.", "phone.", "call.", "contacts.", "calendar.", "reminder.", "alarm.", "remote.", "weather.", "home."};
        for (size_t k = 0; k < sizeof domains / sizeof *domains; k++)
            if (!strncmp(c->id, domains[k], strlen(domains[k]))) CHECK(c->app, "application capability declares its owner: %s", c->id);
        if (!strncmp(c->id, "file.", 5)) CHECK(c->app && !strcmp(c->app, "files"), "every file service capability belongs to Files: %s", c->id);
        if (!strncmp(c->id, "shell.ai_", 9) && strcmp(c->id, "shell.ai_stop")) CHECK(c->app && !strcmp(c->app, "ai"), "AI entry declares its application: %s", c->id);
        if (!c->app) continue;
        vtk_host_call(owner_line, (void *)c); total++;
        bool required = !strcmp(c->app, "phone") || !strcmp(c->app, "message") || !strcmp(c->app, "home");
        if (required) { CHECK(ink_member_app_mode(NULL, c->app) == 2 && ink_member_allows(c->id, true), "required app remains allowed: %s", c->id); continue; }
        CHECK(ink_member_app_mode(NULL, c->app) == 0, "declared application is disabled: %s", c->app);
        CHECK(!ink_cap_ai_callable(c), "AI discovery removes %s", c->id);
        denied(c->id, NULL, false); denied(c->id, NULL, true); denied_count++;
    }
    CHECK(total > 100 && denied_count > 100, "enumerate all owned capabilities (%u owned, %u denied twice)", total, denied_count);
    CHECK(AI("app.open", "{\"id\":\"phone\"}") && g_rc.status == INK_RC_DONE, "required phone still opens");
    CHECK(AI("app.open", "{\"id\":\"message\"}") && g_rc.status == INK_RC_DONE, "required SMS still opens");
}
static void run(void *arg) {
    (void)arg; ink_power_hold(INK_NEED_AWAKE); ink_power_hold(INK_NEED_NO_LOCK); quiet();
    if (!strcmp(phase, "policy")) policy();
    else if (!strcmp(phase, "restart")) restart();
    else if (!strcmp(phase, "ownership")) ownership();
    else locale();
    ink_probe_count("test.finished");
}
void ink_main(void) {
    snprintf(phase, sizeof phase, "%s", getenv("INK_KID_APPS_PHASE")); product_main(); test_start_home();
    osal_task_cfg_t cfg = {.name = "test.kid-apps", .fn = run, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    if (osal_task_create(&cfg, NULL) != OSAL_OK) osal_panic("kid-apps test task");
}
