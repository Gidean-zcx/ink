/* Real services, physical input and AI paths for F6. */
#include <stdlib.h>
#include "ink/sys/media.h"
#include "ink/sys/task.h"
#include "ink/ui_shell.h"
#define ink_main product_main
#include "../system/boot/main.c"
#undef ink_main
#include "integration/whole_product.h"

static char snap[4096], home_activity[512], frame[64];
static const char *wanted_cap, *wanted_text;
static int x, y, job, stopped;
static volatile bool captured;
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void quiet(void) { CHECK(WAIT_FOR(settled(), 10000), "activity frame settles"); }
static void read_page(void) {
    CHECK(ink_ui_snapshot(snap, sizeof snap, NULL) > 0, "page has AI snapshot");
    CHECK(ink_ui_context_snapshot(g_snap, sizeof g_snap, NULL) > 0, "snapshot fits actual AI context");
}
static void inspect(void) { quiet(); on_ui(read_page); }
static void locate_in(lv_obj_t *o) {
    if (lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN)) return;
    const char *bound = ink_shell_bound_cap(o);
    if ((wanted_cap && bound && !strcmp(bound, wanted_cap)) ||
        (wanted_text && lv_obj_check_type(o, &lv_label_class) && strstr(lv_label_get_text(o), wanted_text))) {
        lv_area_t a; lv_obj_update_layout(o); lv_obj_get_coords(o, &a);
        x = (a.x1 + a.x2) / 2; y = (a.y1 + a.y2) / 2;
        CHECK(a.x1 >= 0 && a.y1 >= 0 && a.x2 < INK_CANVAS_W && a.y2 < INK_CANVAS_H, "activity control stays on screen");
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) locate_in(lv_obj_get_child(o, (int32_t)i));
}
static void locate(void) { x = y = -1; locate_in(lv_screen_active()); locate_in(lv_layer_sys()); }
static void click_cap(const char *cap) {
    wanted_cap = cap; wanted_text = NULL; on_ui(locate);
    CHECK(x >= 0, "visible action %s is bound to the real capability", cap);
    if (x >= 0) tap(x, y);
    quiet();
}
static bool has_text(const char *text) {
    wanted_cap = NULL; wanted_text = text; on_ui(locate); return x >= 0;
}
static void capture(void *arg) { sim_display_snapshot(arg); captured = true; }
static void screenshot(const char *name) {
    quiet(); captured = false; snprintf(frame, sizeof frame, "%s", name);
    vtk_event_at(vtk_now(), capture, frame);
    CHECK(WAIT_FOR(captured, 1000), "capture settled activity scene");
    char counter[96]; snprintf(counter, sizeof counter, "test.snapshot.%s", name); ink_probe_count(counter);
}
static ink_media_status_t media(void) { ink_media_status_t st; ink_media_status(&st); return st; }
static void cancelled(void *ctx) { (void)ctx; stopped++; ink_task_end(job, INK_TASK_CANCELLED, "已取消", NULL); }
static void fixed_clock(void) { ink_time_set(1793803620, false); }
static void home(void) { CHECK(BY_HAND("system.home", NULL) && g_rc.status == INK_RC_DONE, "return home"); inspect(); }
static void media_case(void) {
    CHECK(BY_HAND("audio.record", "{\"path\":\"/me/星星邮差.opus\"}") && g_rc.status == INK_RC_DONE, "record synthetic audio through service");
    delay_ms(60000);
    CHECK(BY_HAND("audio.record_stop", NULL) && g_rc.status == INK_RC_DONE, "stop real recording");
    CHECK(WAIT_FOR(!ink_audio_recording(), 4000), "recording file finishes"); delay_ms(500);
    CHECK(AI("media.play", "{\"path\":\"/me/星星邮差.opus\",\"start_ms\":0}") && g_rc.status == INK_RC_DONE, "AI starts actual file");
    CHECK(WAIT_FOR(media().state == INK_MEDIA_PLAYING, 4000), "audio is playing");
    home(); CHECK(strstr(snap, "正在进行=星星邮差；") && strstr(snap, "media.pause"), "home exposes extension-free title and pause");
    CHECK(has_text("星星邮差") && !has_text("星星邮差.opus"), "home visibly renders the extension-free media title");
    const char *a = strstr(snap, "正在进行=");
    const char *end = a ? strstr(a, " 动作：") : NULL;
    size_t bytes = end ? (size_t)(end - a) : 0;
    CHECK(bytes && bytes < sizeof home_activity, "complete activity fits the comparison buffer");
    home_activity[0] = 0;
    if (bytes && bytes < sizeof home_activity) { memcpy(home_activity, a, bytes); home_activity[bytes] = 0; }
    screenshot("MainPlaying");
    CHECK(AI("shell.control", NULL) && g_rc.status == INK_RC_DONE, "AI opens control centre"); inspect();
    CHECK(home_activity[0] && strstr(snap, home_activity), "home and control share identical activity title and action");
    CHECK(has_text("星星邮差") && !has_text("星星邮差.opus"), "control visibly renders the same extension-free media title");
    screenshot("ControlPlaying"); click_cap("media.pause");
    CHECK(WAIT_FOR(media().state == INK_MEDIA_PAUSED, 4000), "touch really pauses media"); inspect();
    CHECK(strstr(snap, "已暂停") && strstr(snap, "media.resume"), "control updates after pause");
    CHECK(BY_HAND("shell.close", NULL), "close control"); inspect();
    CHECK(strstr(snap, "已暂停") && strstr(snap, "media.resume"), "home updates after pause");
    CHECK(AI("media.resume", NULL) && g_rc.status == INK_RC_DONE, "AI uses the same resume capability"); quiet();
    wanted_cap = "media.pause"; wanted_text = NULL; on_ui(locate);
    CHECK(x >= 0, "home pause is available before locking");
    int pause_x = x, pause_y = y;
    CHECK(BY_HAND("shell.lock", NULL) && g_rc.status == INK_RC_DONE, "lock while playing"); quiet();
    CHECK(ink_power_state() == INK_PWR_LOCKED && media().state == INK_MEDIA_PLAYING, "locking keeps audio running");
    CHECK(has_text("正在听，按电源键或 AI 键唤醒"), "LockPlaying uses the listening mark");
    inspect(); CHECK(strstr(snap, "设备已锁定") && !strstr(snap, "星星邮差") && !strstr(snap, "media.pause"), "lock snapshot hides private activity and pause action");
    screenshot("LockPlaying");
    if (pause_x >= 0) tap(pause_x, pause_y);
    quiet();
    CHECK(ink_power_state() == INK_PWR_LOCKED && media().state == INK_MEDIA_PLAYING, "locked page has no tappable pause action");
    CHECK(call_as("media.pause", NULL, INK_SRC_REMOTE) && g_rc.status == INK_RC_DONE, "locked remote pause reaches the existing media capability");
    CHECK(WAIT_FOR(media().state == INK_MEDIA_PAUSED, 4000) && ink_power_state() == INK_PWR_LOCKED, "remote pause changes real media without waking");
    CHECK(call_as("media.resume", NULL, INK_SRC_REMOTE) && g_rc.status == INK_RC_DONE, "remote resume remains available while locked");
    CHECK(WAIT_FOR(media().state == INK_MEDIA_PLAYING, 4000) && ink_power_state() == INK_PWR_LOCKED, "remote resume keeps the device locked");
    key(HAL_KEY_POWER); quiet();
    CHECK(ink_power_state() != INK_PWR_LOCKED, "power key wakes device");
    click_cap("media.pause"); CHECK(WAIT_FOR(media().state == INK_MEDIA_PAUSED, 4000), "pause works after wake");
    CHECK(AI("media.stop", NULL) && g_rc.status == INK_RC_DONE, "stop real media"); inspect();
    CHECK(!strstr(snap, "星星邮差") && !strstr(snap, "media.resume"), "stopped activity disappears from home");
    CHECK(AI("shell.control", NULL), "reopen control after stop"); inspect();
    CHECK(!strstr(snap, "星星邮差") && !strstr(snap, "media.resume"), "stopped activity disappears from control");
    CHECK(BY_HAND("shell.close", NULL), "close control after stop");
}
static void call_case(void) {
    CHECK(BY_HAND("call.dial", "{\"number\":\"00000000001\"}") && g_rc.status == INK_RC_DONE, "dial fake modem number");
    CHECK(WAIT_FOR(sim_net_call(NULL, 0) == 1, 10000), "fake modem connects");
    home(); CHECK(strstr(snap, "回到通话") && strstr(snap, "phone.return"), "home exposes return to actual call");
    screenshot("MainInCall"); click_cap("phone.return"); inspect();
    CHECK(!strcmp(ink_ui_page_name(), "PhoneCall") && strstr(snap, "call.hangup"), "return button reaches active call page");
    home(); CHECK(AI("phone.return", NULL) && g_rc.status == INK_RC_DONE, "AI uses same return capability"); quiet();
    CHECK(!strcmp(ink_ui_page_name(), "PhoneCall"), "AI reaches the same call page");
    CHECK(BY_HAND("call.hangup", NULL) && g_rc.status == INK_RC_DONE, "hang up fake call");
    CHECK(WAIT_FOR(sim_net_call(NULL, 0) == 0, 10000), "modem call ends");
    home(); CHECK(!strstr(snap, "phone.return"), "ended call disappears");
}
static void tasks_case(void) {
    job = ink_task_begin("test", "小王子", false, cancelled, NULL);
    ink_task_progress(job, -1, "正在准备");
    int next = ink_task_begin("test", "昆虫记", false, NULL, NULL);
    ink_task_progress(next, -1, "等网络");
    int done = ink_task_begin("test", "星星邮差", false, NULL, NULL);
    ink_task_end(done, INK_TASK_DONE, "可以读了", NULL);
    CHECK(BY_HAND("shell.control", NULL) && g_rc.status == INK_RC_DONE, "open Tasks entry in control centre"); quiet();
    click_cap("shell.tasks"); inspect();
    CHECK(!strcmp(ink_ui_page_name(), "Tasks"), "control centre opens actual Tasks page");
    CHECK(strstr(snap, "小王子") && strstr(snap, "正在准备") && strstr(snap, "做好了") && strstr(snap, "可以读了"), "Tasks lists real stages and finished results in two groups");
    CHECK(strstr(snap, "job.cancel") && !strstr(snap, "percent"), "snapshot has actual cancel and no invented progress");
    screenshot("Tasks");
    ink_task_progress(job, -1, "正在整理"); delay_ms(200); inspect();
    CHECK(strstr(snap, "正在整理") && !strstr(snap, "正在准备"), "open Tasks updates real stage without count change");
    click_cap("job.cancel");
    CHECK(WAIT_FOR(stopped == 1, 2000), "touch invokes job.cancel and stops actual worker"); inspect();
    CHECK(strstr(snap, "已取消"), "cancel outcome comes back from service");
    CHECK(AI("job.list", NULL) && strstr(g_result, "cancelled"), "AI job.list observes actual cancellation");
    job = ink_task_begin("test", "第二个作业", false, cancelled, NULL);
    char args[64]; snprintf(args, sizeof args, "{\"id\":%d}", job);
    CHECK(AI("job.cancel", args) && g_rc.status == INK_RC_DONE && stopped == 2, "AI cancellation uses same callback");
    CHECK(ink_member_switch("kid") == 0, "switch to synthetic child"); quiet();
    CHECK(AI("shell.tasks", NULL) && g_rc.status == INK_RC_DONE, "child opens own Tasks"); inspect();
    CHECK(!strstr(snap, "小王子") && !strstr(snap, "昆虫记") && !strstr(snap, "星星邮差"), "child cannot read adult jobs");
    CHECK(AI("job.cancel", args) && g_rc.status != INK_RC_DONE, "child cannot cancel adult job");
    CHECK(ink_member_switch("owner") == 0, "return to synthetic adult");
    ink_task_end(next, INK_TASK_DONE, "可以读了", NULL);
}
static void run(void *arg) {
    (void)arg;
    CHECK(WAIT_FOR(ink_member_space_open(), 10000), "synthetic member space opens"); test_start_home(); quiet(); on_ui(fixed_clock);
    media_case(); call_case(); tasks_case();
    CHECK(WAIT_FOR(ink_db_idle(), 10000), "all service writes finish");
    ink_probe_count("test.snapshot.settled"); ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name = "activity-test", .fn = run, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start activity acceptance driver");
}
