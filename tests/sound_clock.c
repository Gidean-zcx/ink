/* Synthetic full-product checks, using the real input, audio and interruption services. */
#include <stdlib.h>
#include "ink/ui_shell.h"
#include "ink/sys/audio.h"
#include "ink/sys/notify.h"
#include "ink/sys/interrupt.h"
#define ink_main product_main
#include "../system/boot/main.c"
#undef ink_main
#include "integration/whole_product.h"

static char phase[16], snap[4096];
static const char *wanted;
static lv_point_t pt;
static bool found;
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void quiet(void) { CHECK(WAIT_FOR(settled(), 10000), "the visible frame settles"); }
static void locate(lv_obj_t *root) {
    if (!lv_obj_is_visible(root)) return;
    if (lv_obj_check_type(root, &lv_label_class) && !strcmp(lv_label_get_text(root), wanted)) {
        lv_area_t a; lv_obj_get_coords(root, &a); pt = (lv_point_t){(a.x1 + a.x2) / 2, (a.y1 + a.y2) / 2}; found = true;
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); i++) locate(lv_obj_get_child(root, (int32_t)i));
}
static void locate_label(void) { found = false; lv_obj_update_layout(lv_screen_active()); locate(lv_screen_active()); locate(lv_layer_sys()); }
static void press(const char *label) {
    wanted = label; on_ui(locate_label); CHECK(found, "visible touch target: %s", label);
    if (found) tap(pt.x, pt.y);
    quiet();
}
static void snapshot(void) {
    CHECK(ink_ui_snapshot(snap, sizeof snap, NULL) > 0, "page snapshot exists");
    CHECK(ink_ui_context_snapshot(g_snap, sizeof g_snap, NULL) > 0 && !strcmp(snap, g_snap), "AI reads the same page snapshot");
}
static void call_ok(const char *cap, const char *args) {
    CHECK(AI(cap, args) && g_rc.status == INK_RC_DONE, "%s succeeds (%s)", cap, ink_rc_name(g_rc.status)); quiet();
}
static void set(const char *id, int v, bool ai) {
    char args[128]; snprintf(args, sizeof args, "{\"id\":\"%s\",\"value\":%d}", id, v);
    CHECK(call_as("settings.set", args, ai ? INK_SRC_AI : INK_SRC_TOUCH) && g_rc.status == INK_RC_DONE, "%s=%d through %s", id, v, ai ? "AI" : "human"); quiet();
    int64_t expected = ink_member_current()->role == INK_ROLE_CHILD && !strcmp(id, "audio.volume") && v > ink_setting_get("audio.child_max") ? ink_setting_get("audio.child_max") : v;
    CHECK(ink_setting_get(id) == expected, "setting took effect with child policy");
}
static void scene(void *arg) { CHECK(settled(), "capture only a settled frame"); sim_display_snapshot(arg); }
static void capture(const char *name) { quiet(); vtk_event_at(vtk_now() + SIM_MS(1), scene, (void *)name); delay_ms(5); }
static void note(void) {
    ink_notify_t n = {.title = "合成通知", .body = "测试提示", .src = INK_SRC_SYSTEM, .prio = INK_NOTIFY_HIGH};
    CHECK(ink_notify_post(&n, NULL) == 0, "post a synthetic notification"); quiet();
}
static int alert(ink_int_kind_t kind) {
    ink_interrupt_t it = {.kind = kind, .ringing = true, .title = "合成提醒", .body = "测试响铃", .nactions = 1, .actions = {{.label = "关闭"}}};
    int id = ink_interrupt_raise(&it); CHECK(id > 0, "raise an actual interruption"); quiet(); return id;
}
static void sound(void) {
    call_ok("shell.sound", NULL);
    set("audio.key_tone", 0, true); set("audio.vibration", 0, false);
    double keys = sim_metric_value("audio.key"), vibrations = sim_metric_value("haptics.pulses");
    key(INK_KEY_NEXT); quiet(); key(INK_KEY_PREV); quiet(); note();
    int id = alert(INK_INT_ALARM); ink_interrupt_end(id); quiet();
    CHECK(sim_metric_value("audio.key") == keys && sim_metric_value("haptics.pulses") == vibrations, "disabled switches suppress actual output");
    call_ok("shell.page_next", NULL); press("按键提示音"); press("振动");
    CHECK(ink_setting_get("audio.key_tone") == 1 && ink_setting_get("audio.vibration") == 1, "touch changes both real switches");
    keys = sim_metric_value("audio.key");
    key(INK_KEY_PREV); quiet(); key(INK_KEY_NEXT); quiet();
    CHECK(sim_metric_value("audio.key") == keys + 2, "two physical presses produce exactly two short tones");
    keys = sim_metric_value("audio.key"); press("振动"); press("振动");
    CHECK(sim_metric_value("audio.key") == keys + 2, "two confirmed touches produce exactly two short tones");
    keys = sim_metric_value("audio.key"); press("按键提示音");
    CHECK(sim_metric_value("audio.key") == keys, "disabling the tone takes effect before touch feedback");
    press("按键提示音");
    CHECK(sim_metric_value("audio.key") == keys + 1, "enabling the tone takes effect before touch feedback");
    vibrations = sim_metric_value("haptics.pulses"); note();
    CHECK(WAIT_FOR(sim_metric_value("haptics.pulses") == vibrations + 1, 5000), "enabled notification produces one hardware pulse");
    set("audio.call_ring", 0, true); set("audio.alarm_ring", 0, false);
    press("来电"); CHECK(ink_setting_get("audio.call_ring") == 1, "touch cycles incoming ringtone");
    press("闹钟"); CHECK(ink_setting_get("audio.alarm_ring") == 1, "touch cycles alarm ringtone");
    set("audio.call_ring", 2, true); set("audio.alarm_ring", 2, true);
    set("audio.alarm_ring", 1, false);
    CHECK(sim_metric_value("audio.preview") >= 7, "human and AI ringtone changes really play previews");
    vibrations = sim_metric_value("haptics.pulses"); id = alert(INK_INT_CALL);
    CHECK(sim_metric_last("audio.ring.call") == 2 && sim_metric_value("audio.ring.call") > 0, "call plays its selected tone");
    ink_interrupt_end(id); quiet(); id = alert(INK_INT_ALARM);
    CHECK(sim_metric_last("audio.ring.alarm") == 1 && sim_metric_value("audio.ring.alarm") > 0, "alarm plays its own selected tone");
    ink_interrupt_end(id); quiet();
    CHECK(sim_metric_value("haptics.pulses") == vibrations + 2, "call and alarm each trigger one actual pulse");
    on_ui(snapshot); CHECK(strstr(snap, "audio.key_tone") && strstr(snap, "audio.vibration") && strstr(snap, "audio.call_ring") && strstr(snap, "audio.alarm_ring"), "sound snapshot exposes all settings and actions");
    capture("sound");
    CHECK(ink_member_switch("kid") == 0, "switch to synthetic child"); quiet();
    set("audio.volume", 100, true); CHECK(ink_audio_effective_volume() <= ink_setting_get("audio.child_max"), "child output obeys its volume ceiling");
    set("audio.key_tone", 0, false); set("audio.key_tone", 1, true);
    set("audio.vibration", 0, true); set("audio.vibration", 1, false);
}
static void timer(void) {
    call_ok("app.open", "{\"id\":\"clock\"}"); press("计时");
    on_ui(snapshot); CHECK(strstr(snap, "clock.timer_set") && strstr(snap, "clock.timer_start"), "timer exposes controls to AI");
    capture("timer-idle"); press("1 分钟"); press("开始"); press("暂停");
    on_ui(snapshot); CHECK(strstr(snap, "已暂停"), "touch pauses the countdown");
    delay_ms(1200); on_ui(snapshot); CHECK(strstr(snap, "已暂停"), "paused timer remains paused");
    press("继续"); press("取消"); on_ui(snapshot); CHECK(strstr(snap, "设定时长"), "touch cancels to the duration picker");
    call_ok("clock.timer_set", "{\"seconds\":5}"); call_ok("clock.timer_start", NULL); call_ok("clock.timer_pause", NULL);
    delay_ms(6000); CHECK(strcmp(active_state(), "alarm"), "a paused deadline never rings");
    call_ok("clock.timer_start", NULL);
    CHECK(WAIT_FOR(!strcmp(active_state(), "alarm"), 10000), "resumed countdown reaches its actual deadline"); quiet();
    on_ui(snapshot); CHECK(strstr(snap, "计时结束") && strstr(snap, "关闭"), "expiry uses the existing alarm card");
    CHECK(AI("interrupt.list", NULL) && g_rc.status == INK_RC_DONE && strstr(g_result, "clock.timer_dismiss"), "AI can invoke the same dismissal as the displayed button"); capture("timer-ring");
    press("关闭"); CHECK(WAIT_FOR(strcmp(active_state(), "alarm"), 5000), "touch dismisses countdown ringing"); quiet();
    call_ok("clock.timer_set", "{\"seconds\":2}"); call_ok("clock.timer_start", NULL); call_ok("clock.timer_cancel", NULL);
    delay_ms(3000); CHECK(strcmp(active_state(), "alarm"), "cancelled countdown does not ring");
    call_ok("clock.timer_set", "{\"seconds\":180}"); call_ok("clock.timer_start", NULL); capture("timer-running");
    double refreshes = sim_metric_value("display.queue_wait_ms"); delay_ms(2000);
    CHECK(sim_metric_value("display.queue_wait_ms") == refreshes, "minute countdown does not refresh every second");
    delay_ms(60000); quiet(); on_ui(snapshot); CHECK(strstr(snap, "2 分钟"), "remaining time updates at the next minute boundary");
    CHECK(ink_member_switch("kid") == 0, "child has an independent timer"); quiet();
    call_ok("app.open", "{\"id\":\"clock\"}"); press("计时"); on_ui(snapshot);
    CHECK(strstr(snap, "设定时长") && strstr(snap, "5 分钟"), "child snapshot contains no adult timer state");
    call_ok("clock.timer_set", "{\"seconds\":3}"); call_ok("clock.timer_start", NULL);
    CHECK(WAIT_FOR(!strcmp(active_state(), "alarm"), 10000), "child countdown can ring"); quiet();
    CHECK(sim_metric_last("audio.ring.alarm") == 1, "child countdown uses the selected alarm tone");
    call_ok("clock.timer_dismiss", "{\"id\":1}");
    call_ok("clock.timer_set", "{\"seconds\":300}"); call_ok("clock.timer_start", NULL);
}
static void restart(void) {
    CHECK(ink_setting_get("audio.key_tone") == 1 && ink_setting_get("audio.vibration") == 1 && ink_setting_get("audio.call_ring") == 2 && ink_setting_get("audio.alarm_ring") == 1, "all four sound choices survive a real process restart");
    call_ok("app.open", "{\"id\":\"clock\"}"); press("计时"); on_ui(snapshot);
    CHECK(strstr(snap, "设定时长") && strstr(snap, "之前的计时已取消"), "restart explicitly cancels previous countdowns");
    CHECK(strcmp(active_state(), "alarm"), "no old countdown rings after restart"); capture("timer-restart");
}
static void host_phase(void *arg) { (void)arg; const char *p = getenv("INK_SOUND_PHASE"); snprintf(phase, sizeof phase, "%s", p ? p : "sound"); }
static void run(void *arg) {
    (void)arg; CHECK(WAIT_FOR(ink_member_space_open(), 10000), "member space opens"); quiet();
    if (!strcmp(phase, "sound")) sound(); else if (!strcmp(phase, "timer")) timer(); else restart();
    ink_db_flush_all(); CHECK(WAIT_FOR(ink_db_idle() && !ink_storage_pending(), 10000), "data writes finish");
    if (!strcmp(phase, "sound")) CHECK(WAIT_FOR(sim_metric_value("settings.saved") > 0, 5000), "settings reached flash before exit");
    ink_probe_count("test.finished");
}
void ink_main(void) {
    vtk_host_call(host_phase, NULL); product_main();
    osal_task_cfg_t cfg = {.name = "sound-clock-test", .fn = run, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start test driver");
}
