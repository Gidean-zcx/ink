/* MW-R2: authorization deadlines at capture, buffered upload and the transport queue. */
#include <stdio.h>
#include <string.h>

#include "ink/sys/audio.h"
#include "ink/sys/file.h"
#include "ink/sys/hub.h"
#include "ink/sys/media.h"
#include "ink/sys/members.h"
#include "ink/sys/power.h"
#include "ink/sys/rt.h"
#include "ink/sys/textinput.h"

#define ink_main product_main
#include "../../system/boot/main.c"
#undef ink_main

#include "whole_product.h"

int member_wake_ai_buffered(void);
bool member_wake_ai_revoked(void);
int member_wake_dictation_buffered(void);
bool member_wake_dictation_revoked(void);

static volatile int pin_done, pin_result, raw_frames;
static void pin_told(void *ctx, int result, uint32_t wait_s) {
    (void)ctx;
    (void)wait_s;
    pin_result = result;
    pin_done++;
}

static const char *state(const char *id) {
    static char value[24];
    value[0] = 0;
    ink_state_get_str(ink_state_find(id), value, sizeof value);
    return value;
}

static void wait_until(uint64_t at) {
    uint64_t now = osal_now_us();
    if (at > now) osal_task_delay_us(at - now);
}

static uint64_t lock_now(void) {
    ink_power_lock();
    CHECK(WAIT_FOR(!strcmp(power_state(), "locked"), 2000), "lock starts the grace");
    return osal_now_us();
}

static void authenticate(void) {
    ink_power_unlock(osal_now_us());
    CHECK(WAIT_FOR(strcmp(power_state(), "locked"), 2000), "wake the screen");
    int before = pin_done;
    CHECK(ink_member_unlock("owner", "1234", pin_told, NULL) == 0 && WAIT_FOR(pin_done > before, 5000) && pin_result == INK_PIN_OK, "authenticate the synthetic adult");
    ink_hub_demand();
    CHECK(WAIT_FOR(ink_hub_online(), 30000), "the hub is online");
}

static void mic_event(void *arg) { sim_audio_mic_source(arg); }
static void mic(const char *what) {
    vtk_event_at(vtk_now() + SIM_MS(1), mic_event, (void *)what);
    delay_ms(5);
}

static void scan(void) {
    ink_call_opts_t opts = {.src = INK_SRC_AI, .reply_exec = INK_EXEC_DIRECT, .done = rc_done};
    g_done = 0;
    CHECK(ink_cap_call("wifi.scan", NULL, &opts) == 0 && WAIT_FOR(!strcmp(state("wifi.state"), "scanning"), 500), "the real Wi-Fi scan occupies exec.bg");
}

static void raw_frame(void *ctx, const uint8_t *data, size_t len, uint32_t seq) {
    (void)ctx;
    (void)data;
    (void)seq;
    if (len) raw_frames++;
}

static void streaming_scan(void) {
    CHECK(ink_setting_get("member.idle_return_s") == 120, "keep the default 120 second grace");
    ink_receipt_t rc;
    CHECK(ink_setting_set("voice.handsfree", 1, INK_SRC_TOUCH, &rc) == INK_RC_DONE, "listen even while locked");
    uint64_t since = lock_now();
    wait_until(since + 114000000ull);
    double sent = sim_metric_value("hub.binary_bytes");
    mic("wake");
    delay_ms(700);
    mic("speech");
    CHECK(WAIT_FOR(!strcmp(state("audio.mic"), "sending"), 1500), "voice uploads during the grace");
    CHECK(WAIT_FOR(sim_metric_value("hub.binary_bytes") > sent, 2000), "the real voice frames reach the hub");
    wait_until(since + 119000000ull);
    scan();
    wait_until(since + 120500000ull);
    CHECK(!ink_member_space_open() && !strcmp(state("wifi.state"), "scanning"), "the authorization expires during the scan");
    sent = sim_metric_value("hub.binary_bytes");
    delay_ms(500);
    CHECK(sim_metric_value("hub.binary_bytes") == sent, "no voice after expiry during the scan (%.0f bytes)", sim_metric_value("hub.binary_bytes") - sent);
    CHECK(!ink_audio_capture_busy() && strcmp(state("audio.mic"), "sending"), "nonlocal capture stops before exec.bg is free");
    CHECK(WAIT_FOR(g_done, 3000), "the scan completes normally");
    mic("silence");
    authenticate();
    CHECK(ink_setting_set("voice.handsfree", 2, INK_SRC_TOUCH, &rc) == INK_RC_DONE, "disable hands-free for the raw capture cases");
    CHECK(setting_by_hand("member.idle_return_s", 30), "use the existing shorter grace for the boundary cases");
}

static void capture_scan(bool local) {
    if (local) {
        int r = ink_fs_remove("/me/wake-fixture.opus");
        CHECK(r == INK_FS_OK || r == INK_FS_ERR_NOT_FOUND, "clear only this test's previous recording");
        CHECK(BY_HAND("audio.record", "{\"path\":\"/me/wake-fixture.opus\"}") && g_rc.status == INK_RC_DONE, "start a real local recording (%s)", g_rc.detail);
    }
    else CHECK(ink_audio_capture_begin(raw_frame, NULL) == 0, "start a nonlocal capture without the AI UI");
    delay_ms(300);
    int before = raw_frames;
    uint64_t since = lock_now();
    wait_until(since + 29000000ull);
    scan();
    wait_until(since + 30500000ull);
    CHECK(!ink_member_space_open() && !strcmp(state("wifi.state"), "scanning"), "the raw capture deadline crosses the same scan");
    if (local) {
        CHECK(ink_audio_capture_busy() && ink_audio_recording() && !strcmp(state("audio.mic"), "recording"), "the same member's local file keeps recording");
    } else {
        CHECK(raw_frames > before && !ink_audio_capture_busy(), "raw capture delivered frames and then stopped at expiry");
        before = raw_frames;
        delay_ms(500);
        CHECK(raw_frames == before, "no nonlocal callback after expiry");
        CHECK(ink_audio_capture_begin(raw_frame, NULL) != 0, "a closed space cannot restart nonlocal capture");
    }
    CHECK(WAIT_FOR(!strcmp(state("wifi.state"), "idle") || !strcmp(state("wifi.state"), "connected"), 3000), "the scan drains");
    authenticate();
    if (local) CHECK(BY_HAND("audio.record_stop", NULL) && g_rc.status == INK_RC_DONE && WAIT_FOR(!ink_audio_capture_busy(), 2000), "stop the local recording by hand after authentication");
}

static osal_sem_t *hub_in, *hub_go;
static void hold_hub(void) {
    ink_hub_set_tick(NULL);
    osal_sem_give(hub_in);
    osal_sem_take(hub_go, OSAL_WAIT_FOREVER);
}
static void block_hub(void) {
    ink_hub_set_tick(hold_hub);
    ink_hub_wake();
    CHECK(osal_sem_take(hub_in, 2000000) == OSAL_OK, "hold the transport before its final send");
}

static void queued_transport(void) {
    uint64_t since = lock_now();
    wait_until(since + 29000000ull);
    block_hub();
    uint8_t frame[16] = {1};
    CHECK(ink_hub_send_binary(frame, sizeof frame) == 0, "queue a synthetic voice frame before the deadline");
    scan();
    wait_until(since + 30500000ull);
    double sent = sim_metric_value("hub.binary_bytes"), dropped = sim_metric_value("hub.tx_dropped");
    CHECK(!ink_member_space_open(), "the queued frame has expired");
    CHECK(ink_hub_send_binary(frame, sizeof frame) != 0, "reject new binary frames while closed");
    osal_sem_give(hub_go);
    CHECK(WAIT_FOR(sim_metric_value("hub.tx_dropped") > dropped, 500), "the final transport guard drops the queued frame");
    delay_ms(200);
    CHECK(sim_metric_value("hub.binary_bytes") == sent, "the queued voice never reaches the hub");
    CHECK(WAIT_FOR(g_done, 3000), "the background scan finishes");
    authenticate();

    ink_rt_handler_t handler = {0};
    int sid = ink_rt_open("echo", NULL, &handler);
    CHECK(sid > 0 && WAIT_FOR(ink_rt_ready(sid), 3000), "open a session under the owner");
    block_hub();
    CHECK(ink_hub_send_binary(frame, sizeof frame) == 0, "queue before a rapid member round trip");
    CHECK(ink_member_switch("kid") == 0, "hand the device to the child");
    CHECK(ink_rt_send(sid, INK_RT_VOICE_UP, frame, sizeof frame, 0) != 0, "a stale session cannot relabel adult audio as the child's");
    int before = pin_done;
    CHECK(ink_member_unlock("owner", "1234", pin_told, NULL) == 0 && WAIT_FOR(pin_done > before, 5000) && pin_result == INK_PIN_OK, "return to the owner while the frame waits");
    sent = sim_metric_value("hub.binary_bytes");
    dropped = sim_metric_value("hub.tx_dropped");
    osal_sem_give(hub_go);
    CHECK(WAIT_FOR(sim_metric_value("hub.tx_dropped") > dropped, 1000), "switching back does not revive the old frame");
    delay_ms(200);
    CHECK(sim_metric_value("hub.binary_bytes") == sent, "the old frame cannot cross a member round trip");
    ink_rt_close(sid);
}

static void press(void) { ink_ai_key(true, osal_now_us()); }
static void release(void) { ink_ai_key(false, osal_now_us()); }
static void start_dictation(void) {
    ink_text_cfg_t cfg = {.type = INK_TEXT_PLAIN, .initial = "", .max = 128};
    CHECK(ink_text_focus(&cfg, NULL, NULL) > 0 && ink_text_voice_start() == 0, "start real dictation with its session not ready");
}
static void finish_dictation(void) { ink_text_voice_finish(); }
static bool checking_dictation, buffer_revoked;
static int buffered;
static void inspect_buffer(void) {
    buffered = checking_dictation ? member_wake_dictation_buffered() : member_wake_ai_buffered();
    buffer_revoked = checking_dictation ? member_wake_dictation_revoked() : member_wake_ai_revoked();
}

static void buffered_turn(bool dictation) {
    ink_hub_demand();
    CHECK(WAIT_FOR(ink_hub_online(), 30000), "online before buffering");
    uint64_t since = lock_now();
    wait_until(since + 27000000ull);
    block_hub();
    on_ui(dictation ? start_dictation : press);
    delay_ms(600);
    on_ui(dictation ? finish_dictation : release);
    CHECK(WAIT_FOR(!ink_audio_capture_busy(), 1000), "finish capturing before the session is ready");
    checking_dictation = dictation;
    on_ui(inspect_buffer);
    CHECK(buffered > 0, "real microphone frames wait for rt.open");
    wait_until(since + 29000000ull);
    scan();
    wait_until(since + 30500000ull);
    CHECK(!ink_member_space_open() && !strcmp(state("wifi.state"), "scanning"), "buffered turn expires while exec.bg is scanning");
    on_ui(inspect_buffer);
    CHECK(buffered == 0 && buffer_revoked, "discard buffered speech even without a ready session");
    double sent = sim_metric_value("hub.binary_bytes");
    osal_sem_give(hub_go);
    delay_ms(500);
    CHECK(sim_metric_value("hub.binary_bytes") == sent, "late rt.open cannot upload expired buffered speech");
    CHECK(WAIT_FOR(g_done, 3000), "the buffer-case scan finishes");
    authenticate();
}

static void test_task(void *arg) {
    (void)arg;
    delay_ms(1500);
    ink_power_hold(INK_NEED_AWAKE);
    ink_hub_demand();
    CHECK(WAIT_FOR(ink_hub_online(), 30000), "online");
    ink_hub_keep(true);
    int before = pin_done;
    CHECK(ink_member_set_pin("owner", NULL, "1234", pin_told, NULL) == 0 && WAIT_FOR(pin_done > before, 5000) && pin_result == INK_PIN_OK, "set a synthetic PIN");
    hub_in = osal_sem_create(0, 1);
    hub_go = osal_sem_create(0, 1);
    CHECK(hub_in && hub_go, "transport test barriers");
    streaming_scan();
    capture_scan(false);
    capture_scan(true);
    queued_transport();
    buffered_turn(false);
    buffered_turn(true);
    ink_hub_keep(false);
    ink_power_release(INK_NEED_AWAKE);
    ink_probe_count("test.finished");
}

void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name = "wakestream", .fn = test_task, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    if (osal_task_create(&cfg, NULL) != OSAL_OK) osal_panic("test task");
}
