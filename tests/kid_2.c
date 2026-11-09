/* Synthetic child time policies, real UI controls and process restart. */
#include <stdlib.h>
#define ink_main product_main
#include "configured_boot.c"
#undef ink_main
#include "integration/whole_product.h"
#include "ink/sys/appreg.h"
#include "ink/ui_shell.h"
#include "ink/sys/audio.h"
#include "ink/sys/notify.h"
#include "ink/sys/storage.h"
#include "ink/sys/time.h"

static char phase[24];
static volatile bool pin_done;
static int pin_result;
static int64_t used, minutes, extra;
static const char *wanted, *scene;
static lv_point_t point;
static lv_area_t label_area;
static bool found;
static void pin_reply(void *ctx, int result, uint32_t wait) { (void)ctx; (void)wait; pin_result = result; pin_done = true; }
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void quiet(void) { CHECK(WAIT_FOR(settled(), 15000), "the child page settles"); }
static void context(void) { read_screen(); CHECK(g_snap[0] && !strstr(g_snap, "24681357"), "the real AI context is present and hides credentials"); }
static bool page(const char *name) { on_ui(context); return !strcmp(g_top, name); }
static void call_ok(const char *cap, const char *args) { CHECK(AI(cap, args) && g_rc.status == INK_RC_DONE, "%s: %s (%s)", cap, ink_rc_name(g_rc.status), g_rc.detail); quiet(); }
static void locate(lv_obj_t *obj) {
    if (!lv_obj_is_visible(obj)) return;
    if (lv_obj_check_type(obj, &lv_label_class) && !strcmp(lv_label_get_text(obj), wanted)) {
        lv_area_t a; lv_obj_get_coords(obj, &a); label_area = a; point = (lv_point_t){(a.x1 + a.x2) / 2, (a.y1 + a.y2) / 2}; found = true;
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(obj); i++) locate(lv_obj_get_child(obj, (int32_t)i));
}
static void find_label(void) { found = false; lv_obj_update_layout(lv_screen_active()); locate(lv_screen_active()); locate(lv_layer_sys()); }
static void touch(const char *label) { quiet(); wanted = label; on_ui(find_label); CHECK(found, "visible button: %s", label); if (found) tap(point.x, point.y); quiet(); }
static void enter(const char *pin) { for (const char *p = pin; *p; p++) { char label[2] = {*p, 0}; touch(label); } touch("确定"); }
static void counters(void) {
    CHECK(AI("member.time_get", "{\"id\":\"kid\"}") && g_rc.status == INK_RC_DONE, "read the synthetic child's policy");
    ink_json_t j; CHECK(ink_json_parse(&j, g_result, strlen(g_result)) == 0, "policy is structured JSON");
    CHECK(ink_json_int(&j, ink_json_find(&j, 0, "used_seconds"), &used) &&
        ink_json_int(&j, ink_json_find(&j, 0, "minutes"), &minutes) &&
        ink_json_int(&j, ink_json_find(&j, 0, "extra_minutes"), &extra), "policy exposes duration, allowance and today's addition");
}
static unsigned request(const char *cap, const char *args) {
    CHECK(AI(cap, args) && g_rc.status == INK_RC_PENDING_CONFIRMATION, "%s cannot run without a parent PIN (%s)", cap, ink_rc_name(g_rc.status));
    unsigned token = 0; CHECK(sscanf(g_rc.object, "confirm:%u", &token) == 1, "the actual bus keeps a confirmation");
    ink_confirm_view_t view[4]; int n = ink_confirm_list(view, 4); bool parent = false;
    for (int i = 0; i < n; i++) if (view[i].token == token) parent = view[i].parent;
    CHECK(parent && ink_confirm_decide(token, true, INK_SRC_TOUCH) != 0, "accepting without the parent's password is refused");
    return token;
}
static void approve(unsigned token, const char *pin, int expected) {
    pin_done = false; CHECK(ink_confirm_accept_with_pin(token, pin, pin_reply, NULL) == 0 && WAIT_FOR(pin_done, 6000), "the real parent checker returns");
    CHECK(pin_result == expected, "parent PIN outcome %d, expected %d", pin_result, expected); quiet();
}
static void change(const char *cap, const char *args) { unsigned token = request(cap, args); approve(token, "24681357", INK_PIN_OK); }
static void save(void) { ink_db_flush_all(); CHECK(WAIT_FOR(ink_db_idle() && !ink_storage_pending(), 15000), "member time and parent additions are durable"); }
static void capture_event(void *arg) {
    (void)arg; CHECK(settled(), "capture the settled physical frame");
    ink_probe_count("test.snapshot.settled"); char name[64]; snprintf(name, sizeof name, "test.snapshot.%s", scene); ink_probe_count(name); sim_display_snapshot(scene);
}
static void capture(const char *name) { scene = name; quiet(); on_ui(context); vtk_event_at(vtk_now() + SIM_MS(1), capture_event, NULL); delay_ms(5); }
static void home(void) { ink_ui_home(); }
static void setup(void) {
    ink_time_set(1794268800, false); /* Synthetic 2026-11-10 local daytime. */
    pin_done = false; CHECK(ink_member_set_pin("owner", "", "24681357", pin_reply, NULL) == 0 && WAIT_FOR(pin_done, 6000) && pin_result == INK_PIN_OK, "create synthetic parent PIN");
    CHECK(ink_member_switch("kid") == 0, "enter the synthetic child's space"); quiet();
}
static void limit(void) {
    setup(); unsigned token = request("member.time_set", "{\"minutes\":2}");
    approve(token, "90817263", INK_PIN_WRONG); counters(); CHECK(minutes == 0, "wrong PIN leaves the daily allowance unchanged");
    approve(token, "24681357", INK_PIN_OK); counters(); CHECK(minutes == 2 && extra == 0, "two-minute policy belongs to this child");
    call_ok("reader.open_book", "{\"title\":\"测试长书\"}"); call_ok("reader.goto_page", "{\"page\":4}");
    delay_ms(30000); counters(); int64_t before = used;
    key(HAL_KEY_POWER); CHECK(WAIT_FOR(ink_power_state() == INK_PWR_LOCKED, 3000), "lock through the real power key");
    delay_ms(90000); counters(); CHECK(used >= before && used - before < 3, "locked time is not charged");
    key(HAL_KEY_POWER); CHECK(WAIT_FOR(ink_power_state() != INK_PWR_LOCKED, 3000), "wake through the real power key"); quiet();
    CHECK(!ink_member_time_up(), "ninety locked seconds do not exhaust the allowance");
    counters(); int left = 120 - (int)used; if (left > 5) delay_ms((left - 5) * 1000);
    CHECK(!ink_member_time_up(), "the child still has time immediately before two minutes");
    delay_ms(6000); CHECK(WAIT_FOR(page("KidTimeUp"), 8000), "two minutes produces KidTimeUp"); capture("KidTimeUp");
    CHECK(strstr(g_snap, "今天的时间到了") && strstr(g_snap, "member.time_get"), "TimeUp exposes its actual state and actions");
    CHECK(!ink_cap_ai_callable(ink_cap_find("memo.edit")), "the AI directory hides actions unavailable after time runs out");
    CHECK(AI("reader.list_books", NULL) && g_rc.status != INK_RC_DONE, "reader operations cannot continue after time runs out");
    CHECK(AI("memo.edit", NULL) && g_rc.status == INK_RC_REJECTED, "noncommunication capabilities are denied even without UI (%s: %s)", ink_rc_name(g_rc.status), g_rc.detail);
    touch("短信"); CHECK(!strcmp(ink_ui_page_name(), "Sms"), "the actual SMS entry remains available by touch");
    call_ok("data.list", "{\"collection\":\"sms.messages\"}");
    CHECK(AI("data.list", "{\"collection\":\"activity.log\"}") && g_rc.status == INK_RC_REJECTED, "data capabilities cannot expose noncommunication collections after expiry");
    call_ok("sms.thread", "{\"peer\":\"00000000000\"}"); call_ok("sms.compose", NULL); call_ok("sms.text", NULL); call_ok("text.insert", "{\"text\":\"合成短信\"}");
    CHECK(strstr(ink_text_value(), "合成短信"), "SMS text entry still works after time runs out");
    CHECK(AI("app.open", "{\"id\":\"clock\"}") && g_rc.status == INK_RC_REJECTED && page("KidTimeUp"), "SMS cannot be used to escape the time guard");
    touch("打电话"); CHECK(!strcmp(ink_ui_page_name(), "Phone"), "the actual phone remains usable (page=%s, top=%s)", ink_ui_page_name(), ink_ui_top_name());
    call_ok("app.open", "{\"id\":\"message\"}"); CHECK(!strcmp(ink_ui_page_name(), "SmsCompose"), "SMS resumes the existing composition");
    CHECK(AI("app.open", "{\"id\":\"clock\"}") && g_rc.status == INK_RC_REJECTED && page("KidTimeUp"), "app.open cannot bypass the time limit");
    touch("爸爸妈妈加时间"); CHECK(page("PinForgot"), "add-time reuses the local parent form");
    enter("90817263"); counters(); CHECK(extra == 0 && ink_member_time_up(), "a wrong local PIN cannot add time");
    enter("24681357"); CHECK(WAIT_FOR(!ink_member_time_up(), 6000), "the right local PIN adds time"); quiet(); counters();
    CHECK(extra == 15 && !strcmp(ink_member_current()->id, "kid"), "the parent adds today's fifteen minutes without switching identity");
    call_ok("app.open", "{\"id\":\"clock\"}"); CHECK(!strcmp(ink_ui_page_name(), "Clock"), "the child resumes ordinary use after addition");
    on_ui(home); quiet(); call_ok("reader.open_book", "{\"title\":\"测试长书\"}");
    CHECK(AI("reader.read_page", NULL) && g_rc.status == INK_RC_DONE && g_rc.revision == 4, "time-up keeps the real reading position at page four (%s, rev=%u, %.60s; top=%s)", ink_rc_name(g_rc.status), (unsigned)g_rc.revision, g_rc.detail, ink_ui_top_name());
    key(HAL_KEY_POWER); counters(); CHECK(used >= 120 && used < 240, "usage includes the elapsed unlocked time"); save();
}
static void restart(void) {
    CHECK(ink_member_has_pin("owner"), "the synthetic parent's verifier survived restart");
    CHECK(ink_member_switch("kid") == 0, "restart in the child's space"); quiet(); counters();
    CHECK(minutes == 2 && extra == 15 && used >= 120, "daily policy, elapsed usage and today's added time survive a separate process");
    ink_tm_t tm; ink_time_to_local(1794268800, &tm); tm.hour = 23; tm.min = 59; tm.sec = 59;
    ink_time_set(ink_time_from_local(&tm), false); counters();
    delay_ms(2500); counters(); CHECK(extra == 0 && used <= 3 && minutes == 2, "local midnight clears only today's usage and additions");
    change("member.time_set", "{\"minutes\":0}"); counters(); CHECK(minutes == 0 && !ink_member_time_up(), "zero means unlimited");
}
static void parent_create(lv_obj_t *screen, void *arg) {
    (void)arg; ink_ui_label(screen, "合成家长应用", INK_FONT_TITLE, INK_WEIGHT_BOLD, INK_MARGIN, INK_STATUS_BAR, INK_CANVAS_W - 2 * INK_MARGIN);
}
static int parent_snapshot(char *out, size_t cap) { return snprintf(out, cap, "合成的仅家长可见应用"); }
static void register_parent_app(void) {
    static const ink_activity_t page = {.name = "TestParent", .app = "test.parents", .create = parent_create, .snapshot = parent_snapshot};
    static const ink_app_def_t app = {.id = "test.parents", .title = "合成家长应用", .entry = &page, .system_app = true, .child_ok = false};
    CHECK(ink_app_register(&app) == 0, "register a synthetic parent-only app");
}
static void blocked(void) {
    setup(); on_ui(register_parent_app);
    call_ok("app.list", NULL); CHECK(!strstr(g_result, "test.parents"), "forbidden apps are absent from the child's directory");
    CHECK(AI("app.open", "{\"id\":\"test.parents\"}") && g_rc.status == INK_RC_REJECTED, "an AI link to a forbidden app is denied");
    CHECK(page("KidLimit"), "forbidden AI navigation presents KidLimit"); capture("KidLimit"); touch("算了"); CHECK(!page("KidLimit"), "cancelling restores the former page");
    ink_notify_t n = {.channel = "system", .title = "合成通知", .body = "打开文件", .action = "app.open", .action_args = "{\"id\":\"test.parents\"}"};
    uint32_t id = 0; CHECK(ink_notify_post(&n, &id) == 0 && id > 0, "post a synthetic actionable notification");
    char args[48]; snprintf(args, sizeof args, "{\"id\":%u}", (unsigned)id); call_ok("shell.notify_open", args);
    CHECK(page("KidLimit"), "a notification link is guarded by the same navigation entry");
    touch("请他们输密码"); CHECK(page("PinForgot"), "one-time permission reuses the local parent form"); enter("90817263");
    CHECK(strcmp(ink_ui_page_name(), "TestParent"), "a wrong password cannot open the forbidden app"); enter("24681357");
    CHECK(WAIT_FOR(!strcmp(ink_ui_page_name(), "TestParent"), 6000), "the right password opens the actual app once"); quiet();
    on_ui(home); quiet(); CHECK(AI("app.open", "{\"id\":\"test.parents\"}") && g_rc.status == INK_RC_REJECTED && page("KidLimit"), "the next AI request still needs its own parent approval");
    call_ok("app.list", NULL); CHECK(!strstr(g_result, "test.parents"), "single-use permission does not change visibility or policy");
}
static void control(void) {
    setup(); call_ok("shell.control", NULL); CHECK(page("KidControl"), "the child receives KidControl");
    CHECK(strstr(g_snap, "亮度=") && strstr(g_snap, "暖光=") && strstr(g_snap, "音量=") && strstr(g_snap, "通知=") &&
        !strstr(g_snap, "网络") && !strstr(g_snap, "勿扰") && !strstr(g_snap, "叫名字") && !strstr(g_snap, "shell.more"), "child control context contains just the four permitted controls"); capture("KidControl");
    call_ok("settings.set", "{\"id\":\"display.light\",\"value\":40}"); call_ok("settings.set", "{\"id\":\"display.warmth\",\"value\":60}");
    call_ok("settings.set", "{\"id\":\"audio.volume\",\"value\":100}"); CHECK(ink_setting_get("audio.volume") <= ink_setting_get("audio.child_max") && ink_audio_effective_volume() <= ink_setting_get("audio.child_max"), "settings and actual audio both enforce child_max");
    call_ok("system.set_volume", "{\"percent\":100}"); CHECK(ink_setting_get("audio.volume") <= ink_setting_get("audio.child_max"), "volume keys and AI share the central cap");
}
static void set_parent_pin(const char *old, const char *next) {
    pin_done = false;
    CHECK(ink_member_set_pin("owner", old, next, pin_reply, NULL) == 0 && WAIT_FOR(pin_done, 6000) && pin_result == INK_PIN_OK, "change the synthetic parent's PIN");
}
static void settings(void) {
    set_parent_pin("", "24681357"); call_ok("shell.kid_settings", NULL);
    CHECK(page("SetKid") && strstr(g_snap, "每天能用多久") && strstr(g_snap, "不限") && strstr(g_snap, "member.time_set") && !strstr(g_snap, "可用时段"), "SetKid shows the real daily policy without a timeslot placeholder");
    touch("每天能用多久"); CHECK(page("confirm") && strstr(g_snap, "ConfirmParent"), "SetKid uses the existing parent confirmation");
    counters(); CHECK(minutes == 0, "opening confirmation does not change the policy");
    touch("输入密码"); enter("90817263"); counters(); CHECK(minutes == 0, "a wrong SetKid PIN leaves the policy unchanged");
    enter("24681357"); quiet(); counters();
    CHECK(minutes == 30 && page("SetKid") && strstr(g_snap, "30 分钟"), "SetKid refreshes its value from member.time_get after confirmation");
    touch("每天能用多久"); touch("输入密码"); enter("24681357"); quiet(); counters();
    CHECK(minutes == 60 && page("SetKid") && strstr(g_snap, "1 小时"), "the parent sets one hour in SetKid"); capture("SetKid");
    CHECK(ink_member_switch("kid") == 0, "use the policy set by the parent"); quiet(); counters();
    int left = 3600 - (int)used;
    if (left > 5) delay_ms((left - 5) * 1000);
    CHECK(!ink_member_time_up(), "SetKid's hour is still usable just before expiry");
    delay_ms(6000); CHECK(WAIT_FOR(page("KidTimeUp"), 8000), "one hour set in SetKid produces KidTimeUp");
}
static void no_pin_notice(void) {
    const char *message = "家里还没有大人设密码，请大人先在设置里设一个";
    quiet(); on_ui(context); CHECK(strstr(g_snap, message), "missing parent PIN is explained in the page snapshot");
    wanted = message; on_ui(find_label); CHECK(found, "missing parent PIN is explained on the visible page");
    CHECK(found && label_area.y1 >= 0 && label_area.y2 < ink_ui_px(INK_CANVAS_H - INK_MARGIN - INK_CONTROL), "the complete no-PIN explanation fits above the page buttons");
    wanted = "确定"; on_ui(find_label); CHECK(!found, "missing PIN is explained without an unusable password keypad");
}
static void dismiss_parent_request(void) {
    call_ok("shell.pin_cancel", NULL);
    ink_confirm_view_t view;
    if (ink_confirm_list(&view, 1)) CHECK(ink_confirm_decide(view.token, false, INK_SRC_TOUCH) == 0, "cancel the unfulfilled parent request");
    quiet();
}
static void no_pin(void) {
    CHECK(!ink_member_any_pin(), "the fixture starts without any parent PIN");
    CHECK(ink_member_switch("kid") == 0, "enter the child without a parent PIN"); quiet(); on_ui(register_parent_app);
    CHECK(AI("app.open", "{\"id\":\"test.parents\"}") && g_rc.status == INK_RC_REJECTED && page("KidLimit"), "no-PIN fixture reaches KidLimit");
    touch("请他们输密码"); no_pin_notice(); capture("NoPinLimit");
    CHECK(call_as("shell.kid_allow", "{\"id\":\"test.parents\"}", INK_SRC_CONFIRMED) && g_rc.status == INK_RC_REJECTED, "no-PIN notice keeps one-time permission denied");
    dismiss_parent_request(); touch("算了");
    CHECK(ink_member_switch("owner") == 0, "return to the adult without a PIN"); quiet(); call_ok("shell.kid_settings", NULL);
    touch("每天能用多久"); no_pin_notice(); capture("NoPinSetKid");
    ink_confirm_view_t view;
    CHECK(ink_confirm_list(&view, 1) == 1 && ink_confirm_decide(view.token, true, INK_SRC_TOUCH) == 0, "ordinary confirmation cannot stand in for a missing parent PIN");
    quiet(); counters(); CHECK(minutes == 0, "SetKid cannot change time without a parent PIN");
    set_parent_pin("", "24681357"); change("member.time_set", "{\"id\":\"kid\",\"minutes\":2}");
    set_parent_pin("24681357", ""); CHECK(!ink_member_any_pin(), "remove the only parent PIN through the real member service");
    CHECK(ink_member_switch("kid") == 0, "return to the child's timed space"); quiet();
    delay_ms(121000); CHECK(WAIT_FOR(page("KidTimeUp"), 8000), "no-PIN fixture reaches KidTimeUp");
    touch("爸爸妈妈加时间"); no_pin_notice();
    CHECK(call_as("member.time_add", "{\"minutes\":15}", INK_SRC_CONFIRMED) && g_rc.status == INK_RC_REJECTED, "no-PIN notice keeps additions denied");
    counters(); CHECK(extra == 0 && ink_member_time_up(), "the no-PIN explanation grants no extra time");
    capture("NoPinTimeUp");
}
static void run(void *arg) {
    (void)arg; ink_power_hold(INK_NEED_AWAKE); ink_power_hold(INK_NEED_NO_LOCK); quiet();
    if (!strcmp(phase, "limit")) limit(); else if (!strcmp(phase, "restart")) restart(); else if (!strcmp(phase, "blocked")) blocked(); else if (!strcmp(phase, "control")) control();
    else if (!strcmp(phase, "settings")) settings(); else if (!strcmp(phase, "no-pin")) no_pin();
    else CHECK(false, "unknown phase");
    ink_probe_count("test.finished");
}
void ink_main(void) {
    snprintf(phase, sizeof phase, "%s", getenv("INK_KID2_PHASE")); product_main(); test_start_home();
    osal_task_cfg_t cfg = {.name = "test.kid-2", .fn = run, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    if (osal_task_create(&cfg, NULL) != OSAL_OK) osal_panic("kid-2 test task");
}
