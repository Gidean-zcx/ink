/* Batch4 regressions: real services and UI, synthetic media, notices and hub. */
#include <stdlib.h>
#include <string.h>
#include "ink/sys/rt.h"
#include "ink/sys/media.h"
#include "ink/sys/notify.h"
#include "ink/sys/textinput.h"
static unsigned submissions;
static char submitted[256];
static int observe_send(int sid, uint8_t kind, const void *data, size_t len, uint8_t flags) {
    if (kind == INK_RT_INPUT && len && ((const uint8_t *)data)[0] == 4) {
        submissions++;
        if (len <= sizeof submitted) {
            memcpy(submitted, (const uint8_t *)data + 1, len - 1);
            submitted[len - 1] = 0;
        }
    }
    return ink_rt_send(sid, kind, data, len, flags);
}
#define ink_rt_send observe_send
#ifndef INK_TEST_REMOTE_SOURCE
#define INK_TEST_REMOTE_SOURCE "../system/apps/remote/remote.c"
#endif
#include INK_TEST_REMOTE_SOURCE
#undef ink_rt_send
#define ink_main product_main
#include "../system/boot/main.c"
#undef ink_main
#include "integration/whole_product.h"

static int which;
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void quiet(void) { CHECK(WAIT_FOR(settled(), 15000), "panel settles"); }
static ink_media_status_t media(void) { ink_media_status_t st; ink_media_status(&st); return st; }
static volatile bool pin_done;
static int pin_result;
static void pin_reply(void *ctx, int result, uint32_t wait) {
    (void)ctx; (void)wait; pin_result = result; pin_done = true;
}
static void parent_time(const char *cap, const char *args) {
    CHECK(AI(cap, args) && g_rc.status == INK_RC_PENDING_CONFIRMATION, "parent approval required");
    unsigned token = 0;
    CHECK(sscanf(g_rc.object, "confirm:%u", &token) == 1, "real confirmation token");
    pin_done = false;
    CHECK(ink_confirm_accept_with_pin(token, "24681357", pin_reply, NULL) == 0 &&
        WAIT_FOR(pin_done, 6000) && pin_result == INK_PIN_OK, "parent grants time with synthetic PIN");
    quiet();
}
static void media_limit(void) {
    CHECK(BY_HAND("audio.record", "{\"path\":\"/family/audit4.opus\"}") && g_rc.status == INK_RC_DONE, "record synthetic shared audio");
    delay_ms(90000);
    CHECK(BY_HAND("audio.record_stop", NULL) && g_rc.status == INK_RC_DONE &&
        WAIT_FOR(!ink_audio_recording(), 4000), "finish synthetic audio file");
    pin_done = false;
    CHECK(ink_member_set_pin("owner", "", "24681357", pin_reply, NULL) == 0 &&
        WAIT_FOR(pin_done, 6000) && pin_result == INK_PIN_OK, "set synthetic parent PIN");
    CHECK(ink_member_switch("kid") == 0, "enter child's space"); quiet();
    parent_time("member.time_set", "{\"minutes\":1}");
    CHECK(BY_HAND("media.play", "{\"path\":\"/family/audit4.opus\",\"start_ms\":0}") &&
        g_rc.status == INK_RC_DONE && media().state == INK_MEDIA_PLAYING, "child starts real playback");
    CHECK(WAIT_FOR(ink_member_time_up(), 65000), "daily allowance expires while playing");
    CHECK(WAIT_FOR(media().state == INK_MEDIA_PAUSED, 300), "B4-01 expiry pauses playback immediately");
    delay_ms(200);
    double heard = sim_audio_played_ms();
    delay_ms(600);
    CHECK(sim_audio_played_ms() <= heard + 120, "B4-01 speaker stops after expiry");
    CHECK(BY_HAND("media.pause", NULL) && g_rc.status == INK_RC_DONE, "B4-01 touch pause remains allowed");
    CHECK(BY_HAND("media.resume", NULL) && g_rc.status == INK_RC_REJECTED, "expired child cannot resume");
    CHECK(BY_HAND("app.open", "{\"id\":\"phone\"}") && g_rc.status == INK_RC_DONE, "phone remains accessible");
    CHECK(BY_HAND("app.open", "{\"id\":\"message\"}") && g_rc.status == INK_RC_DONE, "SMS remains accessible");
    parent_time("member.time_add", "{\"minutes\":1}");
    CHECK(!ink_member_time_up() && media().state == INK_MEDIA_PAUSED, "B4-01 added time never resumes automatically");
    CHECK(BY_HAND("media.resume", NULL) && g_rc.status == INK_RC_DONE &&
        media().state == INK_MEDIA_PLAYING, "child manually resumes after addition");
    CHECK(BY_HAND("media.pause", NULL) && g_rc.status == INK_RC_DONE, "pause before synthetic spoken answer");
    heard = sim_audio_played_ms();
    CHECK(BY_HAND("shell.ai_key", "{\"pressed\":true}") && g_rc.status == INK_RC_DONE, "start synthetic voice turn");
    delay_ms(800);
    CHECK(BY_HAND("shell.ai_key", "{\"pressed\":false}") && g_rc.status == INK_RC_DONE, "finish synthetic voice question");
    CHECK(WAIT_FOR(sim_audio_played_ms() > heard + 400, 15000), "synthetic long answer is audible");
    CHECK(WAIT_FOR(ink_member_time_up(), 65000), "added allowance expires during speech");
    delay_ms(300); heard = sim_audio_played_ms(); delay_ms(800);
    CHECK(sim_audio_played_ms() <= heard + 120, "B4-01 expiry also stops spoken answer");
    CHECK(BY_HAND("media.stop", NULL) && g_rc.status == INK_RC_DONE &&
        media().state == INK_MEDIA_IDLE, "B4-01 touch stop remains allowed after expiry");
}

static void open_notice(ink_source_t src, const char *setting, int value) {
    char action_args[128];
    snprintf(action_args, sizeof action_args, "{\"id\":\"%s\",\"value\":%d}", setting, value);
    ink_notify_t notice = {.channel="system", .title="合成通知", .body="合成设置动作", .action="settings.set", .action_args=action_args};
    uint32_t id = 0;
    CHECK(ink_notify_post(&notice, &id) == 0 && id, "post synthetic action notification");
    char args[40]; snprintf(args, sizeof args, "{\"id\":%u}", (unsigned)id);
    CHECK(call_as("shell.notify_open", args, src) && g_rc.status == INK_RC_DONE, "open notification through bus");
}
static int activity_count(void) {
    CHECK(AI("activity.list", NULL) && g_rc.status == INK_RC_DONE, "read activity via public capability");
    ink_json_t j;
    CHECK(ink_json_parse(&j, g_result, strlen(g_result)) == 0, "activity result is structured JSON");
    int items = ink_json_find(&j, 0, "items");
    return items >= 0 ? j.tok[items].size : -1;
}
static void notice_source(void) {
    CHECK(activity_count() == 0, "synthetic member starts with empty activity");
    open_notice(INK_SRC_AI, "display.light", 70);
    CHECK(WAIT_FOR(ink_setting_get("display.light") == 70, 3000), "AI notification changes brightness");
    CHECK(activity_count() == 1 && strstr(g_result, "settings.set") && strstr(g_result, "done"),
        "B4-05 AI notification action is recorded");
    open_notice(INK_SRC_TOUCH, "display.light", 30);
    CHECK(WAIT_FOR(ink_setting_get("display.light") == 30, 3000), "human notification still changes brightness");
    CHECK(activity_count() == 1, "human notification keeps ordinary touch logging policy");
    open_notice(INK_SRC_AI, "ai.read_clipboard", 1); quiet();
    ink_confirm_view_t view;
    CHECK(ink_setting_get("ai.read_clipboard") == 0 && ink_confirm_list(&view, 1) == 1,
        "notification preserves high-risk setting confirmation");
    CHECK(ink_confirm_decide(view.token, false, INK_SRC_TOUCH) == 0, "cancel synthetic high-risk request");
}

static bool password_mode;
static char secret_snapshot[8192] INK_PSRAM_BSS;
static void input_control(void) {
    ink_json_t j;
    const char *json = password_mode ? "{\"input\":\"password\"}" : "{\"input\":\"text\"}";
    CHECK(ink_json_parse(&j, json, strlen(json)) == 0, "parse synthetic browser input request");
    on_control(NULL, RS.sid, "rt.nav", &j, 0);
}
static void outage(void *arg) { sim_net_outage(SIM_MS((uintptr_t)arg)); }
static void disconnect_ms(int ms) {
    vtk_event_at(vtk_now() + SIM_MS(1), outage, (void *)(uintptr_t)ms); delay_ms(5);
}
static void check_editor(void) {
    CHECK(RS.page == TYPE && RS.field && ink_text_focused(), "B4-06 reconnect restores active input page");
    CHECK(RS.field && !strcmp(ink_textfield_text(RS.field), "合成文字半段") &&
        !strcmp(ink_text_value(), "合成文字半段"), "B4-06 unfinished UTF-8 text survives exactly");
    size_t start = 0, end = 0; ink_text_selection(&start, &end);
    CHECK(start == 1 && end == 3, "selection survives in original input state");
}
static void type_password(void) { ink_text_commit_utf8("SYNTHETIC_SECRET"); }
static void check_secret(void) {
    CHECK(ink_ui_snapshot(secret_snapshot, sizeof secret_snapshot, NULL) > 0 &&
        !strstr(secret_snapshot, "SYNTHETIC_SECRET"), "password remains hidden from AI");
}
static void remote_draft(void) {
    CHECK(AI("remote.open", "{\"url\":\"https://example.com/synthetic\"}") && g_rc.status == INK_RC_DONE, "open synthetic web session");
    CHECK(WAIT_FOR(RS.first_shown && ink_rt_ready(RS.sid), 12000), "initial page arrives");
    password_mode = false; on_ui(input_control); quiet();
    CHECK(AI("text.insert", "{\"text\":\"合成文字半段\"}") && g_rc.status == INK_RC_DONE, "type without submitting");
    CHECK(AI("text.select", "{\"start\":1,\"end\":3}") && g_rc.status == INK_RC_DONE, "select part of unfinished input");
    CHECK(submissions == 0 && sim_metric_value("hub.remote.submits") == 0, "no draft bytes reach hub");
    unsigned updates = RS.updates;
    disconnect_ms(6000);
    CHECK(WAIT_FOR(RS.down_shown, 5000), "short outage is detected");
    CHECK(WAIT_FOR(RS.updates > updates && ink_rt_ready(RS.sid), 20000), "six-second outage recovers in same session");
    quiet(); on_ui(check_editor);
    CHECK(submissions == 0 && sim_metric_value("hub.remote.submits") == 0, "B4-06 recovery sends no unfinished text");
    CHECK(BY_HAND("remote.submit", NULL) && g_rc.status == INK_RC_DONE, "human submits recovered draft");
    CHECK(WAIT_FOR(sim_metric_value("hub.remote.submits") == 1, 5000) && submissions == 1 &&
        !strcmp(submitted, "合成文字半段"), "one complete original draft reaches hub on submission");
    password_mode = true; on_ui(input_control); quiet(); on_ui(type_password); on_ui(check_secret);
    CHECK(AI("remote.submit", NULL) && g_rc.status == INK_RC_REJECTED, "AI cannot submit secret");
    disconnect_ms(310000);
    CHECK(WAIT_FOR(RS.down_shown, 5000), "long outage starts recovery window");
    delay_ms(301000);
    CHECK(RS.ended && RS.sid == 0 && RS.field == NULL && !ink_text_focused(), "session expiry discards editor and secret");
    CHECK(submissions == 1, "secret is never submitted by expiry");
    CHECK(AI("remote.close", NULL) && g_rc.status == INK_RC_DONE, "close ended session");
}
static void run(void *arg) {
    (void)arg; ink_power_hold(INK_NEED_AWAKE); ink_power_hold(INK_NEED_NO_LOCK);
    test_start_home(); quiet();
    if (which == 1) media_limit();
    else if (which == 5) notice_source();
    else if (which == 6) remote_draft();
    else CHECK(false, "unknown audit4 case");
    ink_probe_count("test.finished");
}
void ink_main(void) {
    which = atoi(getenv("INK_AUDIT4_CASE")); product_main();
    osal_task_cfg_t cfg = {.name="test.audit4", .fn=run, .stack_bytes=16384, .priority=OSAL_PRIO_NORMAL, .core=-1, .flags=OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start audit4 driver");
}
