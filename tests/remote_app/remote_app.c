/* Real remote host, deterministic link and keyboard; synthetic inputs only. */
#include <stdlib.h>
#include <string.h>
#include "ink/sys/rt.h"
#include "ink/sys/textinput.h"
#include "ink/osal.h"
static bool hold_credit;
static unsigned input_count, submit_count;
static uint8_t submitted[256];
static size_t submitted_len;
static int observed_send(int sid, uint8_t kind, const void *data, size_t len, uint8_t flags) {
    if (kind == INK_RT_CREDIT && hold_credit) return 0;
    if (kind == INK_RT_INPUT) {
        input_count++;
        if (len && ((const uint8_t *)data)[0] == 4) {
            submit_count++;
            submitted_len = len - 1;
            if (len <= sizeof submitted) memcpy(submitted, (const uint8_t *)data + 1, len - 1);
        }
    }
    return ink_rt_send(sid, kind, data, len, flags);
}
#define ink_rt_send observed_send
#include "../../system/apps/remote/remote.c"
#undef ink_rt_send
#define ink_main product_main
#include "../../system/boot/main.c"
#undef ink_main
#include "../integration/whole_product.h"

const char *test_remote_hub_url(void);

static char snapshot[8192];
static uint32_t picture_hash, before_outage;
static void hash_picture(void) {
    picture_hash = 2166136261u;
    for (int i = 0; i < RS.w * RS.h; i++) picture_hash = (picture_hash ^ RS.px[i]) * 16777619u;
}
static const char *scene;
static bool big_font;
static volatile bool captured;
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void quiet(void) { CHECK(WAIT_FOR(settled(), 12000), "panel settles"); }
static void read_snapshot(void) { CHECK(ink_ui_snapshot(snapshot, sizeof snapshot, NULL) > 0, "AI snapshot exists"); }
static void take(void *arg) {
    (void)arg;
    if (!settled()) { vtk_event_at(vtk_now() + SIM_MS(5), take, NULL); return; }
    CHECK(settled(), "capture is fully visible");
    char metric[80]; snprintf(metric, sizeof metric, "test.snapshot.%s", scene);
    ink_probe_count(metric); sim_display_snapshot(scene); captured = true;
}
static void save_snapshot(void *arg) {
    (void)arg; char path[800];
    int n = snprintf(path, sizeof path, "%s/%s.json", g_sim.out_dir, scene);
    if (n < 0 || (size_t)n >= sizeof path) abort();
    FILE *f = fopen(path, "w");
    if (!f || fputs(snapshot, f) == EOF || fclose(f)) abort();
}
static void capture(const char *board) {
    scene = board; quiet(); on_ui(read_snapshot);
    CHECK(strstr(snapshot, !strcmp(board, "RemotePassword") ? "RemoteType" : !strcmp(board, "RemoteEnded") ? "RemoteOffline" : board), "snapshot identifies %s", board);
    vtk_host_call(save_snapshot, NULL);
    captured = false;
    vtk_event_at(vtk_now() + SIM_MS(1), take, NULL);
    CHECK(WAIT_FOR(captured, 10000), "settled capture completes");
}
static void release_credit(void) { hold_credit = false; CHECK(send_credit(2) == 0, "release delayed first update"); }
static void control(const char *json) {
    ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL);
    CHECK(j && ink_json_parse(j, json, strlen(json)) == 0, "parse hub control");
    if (j) { on_control(NULL, RS.sid, "rt.nav", j, 0); osal_free(j); }
}
static void type_control(void) { control("{\"input\":\"text\"}"); }
static void password_control(void) { control("{\"input\":\"password\"}"); }
static void blocked_control(void) { control("{\"blocked\":true}"); }
static void password_typed(void) { ink_text_commit_utf8("SYNTHETIC_SECRET"); }
static void finish_password(void) { ink_text_enter(); }
static void outage(void *arg) { sim_net_outage(SIM_MS((uintptr_t)arg)); }
static void disconnect_ms(int ms) { vtk_event_at(vtk_now() + SIM_MS(1), outage, (void *)(uintptr_t)ms); delay_ms(5); }
static lv_obj_t *find_label(lv_obj_t *root, const char *text) {
    if (!lv_obj_is_visible(root)) return NULL;
    if (lv_obj_check_type(root, &lv_label_class) && strstr(lv_label_get_text(root), text)) return root;
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); i++) {
        lv_obj_t *label = find_label(lv_obj_get_child(root, (int32_t)i), text);
        if (label) return label;
    }
    return NULL;
}
static void target_visible(void) {
    const char *text = RS.page == TYPE ? (RS.secret ? "向 example.com 输入密码" : "向 example.com 输入文字") : "example.com";
    lv_obj_update_layout(lv_screen_active()); lv_obj_t *label = find_label(lv_screen_active(), text);
    CHECK(label, "UX12 E01 E03 website is visible: %s", text);
    if (label) {
        lv_area_t a, keyboard; lv_obj_get_coords(label, &a); ink_shell_keyboard_area(&keyboard);
        lv_point_t need; lv_text_get_size(&need, lv_label_get_text(label), lv_obj_get_style_text_font(label, 0), 0, 0, lv_obj_get_width(label), LV_TEXT_FLAG_NONE);
        CHECK(need.y <= lv_obj_get_height(label) && a.y1 >= 0 && a.x2 < ink_ui_width(), "UX12 target label fits whole");
        CHECK(!ink_text_focused() || a.y2 < keyboard.y1, "UX12 target remains visible above the keyboard");
    }
}
static void locked_open(void) {
    ink_power_hold(INK_NEED_AWAKE); /* Keep the test driver runnable while the screen is locked. */
    ink_power_lock();
    CHECK(WAIT_FOR(!strcmp(power_state(), "locked"), 2000), "lock before opening remote page");
    double sessions = sim_metric_value("hub.remote.sessions");
    double checks = sim_metric_value("remote.link_checks");
    CHECK(AI("remote.open", "{\"url\":\"https://example.com/locked\"}") && g_rc.status == INK_RC_DONE, "AI opens URL while screen is locked");
    delay_ms(1600);
    CHECK(RS.suspended && RS.sid == 0 && sim_metric_value("hub.remote.sessions") == sessions, "locked AI open sends no hub session");
    CHECK(sim_metric_value("remote.link_checks") == checks, "locked open pauses link checks");
    CHECK(ink_power_unlock(osal_now_us()), "unlock pending URL");
    CHECK(WAIT_FOR(sim_metric_value("hub.remote.sessions") == sessions + 1 && RS.first_shown, 12000), "unlock opens exactly one pending session");
    CHECK(!strcmp(test_remote_hub_url(), "https://example.com/locked"), "hub receives original pending URL after unlock");
    ink_power_lock();
    CHECK(WAIT_FOR(!strcmp(power_state(), "locked") && RS.suspended && RS.sid == 0, 2000), "locking existing remote page closes its session");
    sessions = sim_metric_value("hub.remote.sessions");
    CHECK(AI("remote.open", "{\"url\":\"https://example.com/edited\"}") && g_rc.status == INK_RC_DONE, "AI replaces pending URL while locked");
    delay_ms(1600);
    CHECK(RS.sid == 0 && sim_metric_value("hub.remote.sessions") == sessions, "locked address replacement sends no hub session");
    CHECK(ink_power_unlock(osal_now_us()), "unlock edited URL");
    CHECK(WAIT_FOR(sim_metric_value("hub.remote.sessions") == sessions + 1 && RS.first_shown, 12000), "unlock resumes edited pending session");
    CHECK(!strcmp(test_remote_hub_url(), "https://example.com/edited"), "hub receives edited pending URL after unlock");
    CHECK(AI("remote.close", NULL) && g_rc.status == INK_RC_DONE, "close lock regression page");
    ink_power_release(INK_NEED_AWAKE);
}
static void run(void *arg) {
    (void)arg; delay_ms(1800); quiet();
    CHECK(AI("settings.set", big_font ? "{\"id\":\"display.font_size\",\"value\":3}" : "{\"id\":\"display.font_size\",\"value\":1}") && g_rc.status == INK_RC_DONE, "choose remote font size"); quiet();
    CHECK(AI("remote.open", NULL) && g_rc.status == INK_RC_DONE, "open address editor through bus");
    CHECK(WAIT_FOR(ink_text_focused() && ink_text_type() == INK_TEXT_URL, 5000), "system URL keyboard");
    unsigned before = input_count;
    CHECK(AI("text.insert", "{\"text\":\"https://example.com/synthetic\"}") && g_rc.status == INK_RC_DONE, "enter synthetic address");
    CHECK(input_count == before && RS.sid == 0, "unfinished address never opens a session");
    capture("RemoteAddress");
    hold_credit = true;
    CHECK(AI("text.submit", NULL) && g_rc.status == INK_RC_DONE, "address enter opens hub session");
    CHECK(strstr(RS.params, "https://example.com/synthetic"), "opening request contains entered URL");
    delay_ms(1500);
    CHECK(!RS.first_shown && RS.page == LOADING && osal_now_us() - RS.opened_us <= 2000000ull,
          "slow first screen shows Loading in the 1.5 to 2 second window");
    capture("RemoteLoading");
    on_ui(release_credit);
    CHECK(WAIT_FOR(RS.first_shown && RS.page == REMOTE, 12000), "frame replaces Loading");
    capture("Remote");
    on_ui(target_visible); CHECK(strstr(snapshot, "example.com"), "UX12 E01 website reaches the AI snapshot");
    double checks = sim_metric_value("remote.link_checks");
    delay_ms(10000);
    checks = sim_metric_value("remote.link_checks") - checks;
    ink_probe_value("test.remote.link_checks_10s", checks);
    CHECK(RS.page == REMOTE && checks > 0 && checks <= 25, "open page checks link at most 25 times in ten seconds");
    double pages = sim_metric_value("hub.remote.pages");
    key(HAL_KEY_NEXT); quiet(); key(HAL_KEY_PREV); quiet();
    CHECK(WAIT_FOR(sim_metric_value("hub.remote.pages") == pages + 2, 5000), "both physical paging keys reach hub");
    hw(HW_PRESS, 340, 300); delay_ms(900); hw(HW_RELEASE, 340, 300); quiet();
    CHECK(WAIT_FOR(sim_metric_value("hub.remote.long_presses") == 1, 5000), "long press reaches hub");
    CHECK(sim_metric_value("hub.remote.taps") == 0, "long press does not also send a tap");
    on_ui(type_control); quiet();
    before = input_count;
    CHECK(AI("text.insert", "{\"text\":\"合成\"}") && g_rc.status == INK_RC_DONE, "first local edit");
    CHECK(AI("text.insert", "{\"text\":\"文字\"}") && g_rc.status == INK_RC_DONE, "second local edit");
    CHECK(input_count == before && sim_metric_value("hub.remote.submits") == 0, "no text leaves device while typing");
    capture("RemoteType");
    on_ui(target_visible); CHECK(strstr(snapshot, "向 example.com 输入文字"), "UX12 E03 ordinary field names its website");
    CHECK(AI("text.submit", NULL) && g_rc.status == INK_RC_DONE, "finish uses system enter");
    CHECK(WAIT_FOR(sim_metric_value("hub.remote.submits") == 1, 5000), "hub receives one final submission");
    CHECK(submit_count == 1 && submitted_len == strlen("合成文字") && !memcmp(submitted, "合成文字", submitted_len), "one complete UTF-8 value");
    on_ui(password_control); quiet(); on_ui(password_typed); on_ui(read_snapshot);
    CHECK(!strstr(snapshot, "SYNTHETIC_SECRET"), "password never appears in AI snapshot");
    on_ui(target_visible); capture("RemotePassword");
    CHECK(strstr(snapshot, "向 example.com 输入密码") && !strstr(snapshot, "SYNTHETIC_SECRET"), "UX12 E03 secret field exposes its target only");
    CHECK(AI("text.state", NULL) && !strstr(g_result, "SYNTHETIC_SECRET"), "UX12 secret stays out of the text state");
    CHECK(AI("text.insert", "{\"text\":\"forbidden\"}") && g_rc.status == INK_RC_REJECTED, "AI cannot enter password");
    CHECK(AI("remote.submit", NULL) && g_rc.status == INK_RC_REJECTED, "AI cannot submit password through remote action");
    CHECK(AI("text.submit", NULL) && g_rc.status == INK_RC_REJECTED, "AI cannot submit password through keyboard");
    on_ui(finish_password); quiet();
    CHECK(submit_count == 2 && submitted_len == strlen("SYNTHETIC_SECRET"), "human submits password only at completion");
    unsigned before_turn = RS.updates;
    key(HAL_KEY_NEXT);
    CHECK(WAIT_FOR(RS.updates > before_turn, 10000), "page turn frame arrives before outage");
    quiet(); on_ui(hash_picture); before_outage = picture_hash;
    disconnect_ms(6000);
    CHECK(WAIT_FOR(RS.page == OFFLINE, 5000), "disconnect shows Offline");
    capture("RemoteOffline");
    CHECK(strstr(snapshot, "重新连接") && !strstr(snapshot, "重新打开"), "UX12 E05 recoverable page offers reconnect");
    int same_sid = RS.sid;
    CHECK(AI("remote.retry", NULL) && g_rc.status == INK_RC_DONE && RS.sid == same_sid, "UX12 E05 manual reconnect retains the existing session");
    CHECK(WAIT_FOR(RS.page == REMOTE && ink_rt_ready(RS.sid), 20000), "within five minutes reconnect automatically");
    quiet(); on_ui(hash_picture);
    CHECK(picture_hash == before_outage, "reconnect restores the same browsed page");
    on_ui(blocked_control); quiet();
    CHECK(RS.page == BLOCKED && RS.sid == 0, "hub block ends rendering session");
    capture("RemoteBlocked");
    CHECK(AI("remote.retry", NULL) && g_rc.status == INK_RC_DONE, "retry explicitly starts a session");
    CHECK(WAIT_FOR(RS.first_shown, 12000), "retry gets frame");
    disconnect_ms(310000);
    CHECK(WAIT_FOR(RS.page == OFFLINE, 5000), "long outage starts recovery window");
    delay_ms(301000);
    CHECK(RS.ended && RS.sid == 0 && RS.page == OFFLINE, "five minute limit closes old session honestly");
    delay_ms(20000);
    CHECK(RS.ended && RS.sid == 0, "late return does not silently reopen timed-out page");
    capture("RemoteEnded");
    CHECK(strstr(snapshot, "重新打开") && !strstr(snapshot, "重新连接"), "UX12 E06 ended page offers reopen");
    CHECK(AI("remote.close", NULL) && g_rc.status == INK_RC_DONE, "close through same bus");
    locked_open();
    CHECK(ink_member_switch("kid") == 0, "switch to child"); quiet();
    CHECK(AI("remote.open", "{\"url\":\"https://example.com\"}") && g_rc.status == INK_RC_REJECTED, "child direct call denied");
    CHECK(AI("app.open", "{\"id\":\"remote\"}") && g_rc.status == INK_RC_REJECTED, "child app entry denied");
    for (int i = 0; i < ink_app_count(); i++) if (!strcmp(ink_app_at(i)->id, "remote")) CHECK(!ink_app_visible(ink_app_at(i)), "child cannot see entry");
    ink_probe_count("test.finished");
}
static void read_font(void *arg) { (void)arg; const char *font = getenv("INK_UI_FIX_FONT"); big_font = font && !strcmp(font, "120"); }
void ink_main(void) {
    vtk_host_call(read_font, NULL);
    product_main();
    osal_task_cfg_t cfg = {.name="remote-test", .fn=run, .stack_bytes=16384, .priority=OSAL_PRIO_NORMAL, .core=-1, .flags=OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start test driver");
}
