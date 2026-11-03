/*
 * System service checks on the simulated device: what a service promises its callers, beyond what
 * the scenarios exercise. Results are the probe counters "test.pass" / "test.fail", as in
 * tests/simulator/selftest.c; the budget file requires zero failures.
 */
#include <stdio.h>
#include <string.h>

#include "ink/osal.h"
#include "ink/probe.h"
#include "ink/sys/audio.h"
#include "ink/sys/camera.h"
#include "ink/sys/comm.h"
#include "ink/sys/cap.h"
#include "ink/sys/display.h"
#include "ink/sys/exec.h"
#include "ink/sys/hub.h"
#include "ink/sys/json.h"
#include "ink/sys/lz4.h"
#include "cJSON.h"
#include "ink/sys/members.h"
#include "ink/sys/net.h"
#include "ink/sys/res.h"
#include "ink/sys/rt.h"
#include "ink/sys/settings.h"
#include "ink/sys/state.h"
#include "ink/sys/power.h"
#include "ink/sys/storage.h"
#include "metalio_plus/metalio_board.h"
#include "sim.h"

#define CHECK(cond, ...)                                                 \
    do {                                                                 \
        if (cond) {                                                      \
            ink_probe_count("test.pass");                                \
        } else {                                                         \
            char _m[300];                                                \
            snprintf(_m, sizeof _m, __VA_ARGS__);                        \
            sim_warn("CHECK FAILED (%s:%d) %s", __FILE__, __LINE__, _m); \
            ink_probe_count("test.fail");                                \
        }                                                                \
    } while (0)

static void drain_storage(void) {
    for (int i = 0; i < 500 && ink_storage_pending() > 0; i++) osal_task_delay_us(20000);
}

/* The read cache never returns an older version than the last write (audit AUD2-002: a small file
 * replaced by one too large for the cache kept reading as the old small file). */
static void storage_checks(void) {
    static char big[1500];
    char rb[32];
    const char *path = "/user/apps/systest-cache";
    memset(big, 'N', sizeof big);
    CHECK(ink_storage_write_atomic(path, "OLD", 3) == 0, "a small write is queued");
    drain_storage();
    memset(rb, 0, sizeof rb);
    CHECK(ink_storage_read(path, rb, sizeof rb) == 3 && !memcmp(rb, "OLD", 3), "it reads back (%.3s)", rb);
    CHECK(ink_storage_write_atomic(path, big, sizeof big) == 0, "a write too large for the cache is queued");
    drain_storage();
    memset(rb, 0, sizeof rb);
    int n = ink_storage_read(path, rb, sizeof rb);
    CHECK(n == (int)sizeof rb && rb[0] == 'N', "after it is written a read returns the new content, not the cached old one (%d, %c)", n,
          rb[0] ? rb[0] : '0');
    CHECK(ink_storage_write_atomic(path, "NEW", 3) == 0, "a small write again");
    memset(rb, 0, sizeof rb);
    CHECK(ink_storage_read(path, rb, sizeof rb) == 3 && !memcmp(rb, "NEW", 3), "while queued, a read sees it (%.3s)", rb);
    drain_storage();
    memset(rb, 0, sizeof rb);
    CHECK(ink_storage_read(path, rb, sizeof rb) == 3 && !memcmp(rb, "NEW", 3), "and after it is written (%.3s)", rb);
    CHECK(ink_storage_failed() == 0, "no write failed");
}

/* Playback while the speaker cannot start (the Bluetooth audio chip, which clocks the I2S, unpowered:
 * audit AUD5-002): the stream's frames wait instead of being taken and dropped; they all play once it
 * can start; a stream that cannot start for long fails and its owner is told. */
static volatile int g_failed_id;

static void on_failed(void *ctx, int id) {
    (void)ctx;
    g_failed_id = id;
}

static int push_frames(int id, int n) {
    int ok = 0;
    for (int i = 0; i < n; i++) {
        const uint8_t f[8] = {0xA5, 0x00, 0x10, 0x30, 0x11, 0, 0, 0}; /* the stand-in codec: a 440 Hz tone */
        if (ink_audio_stream_push(id, f, sizeof f) == 0) ok++;
    }
    return ok;
}

static bool playing_for(double since, double ms) { /* waits up to 4 s until `ms` more were played */
    for (int i = 0; i < 400 && sim_audio_played_ms() - since < ms; i++) osal_task_delay_us(10000);
    return sim_audio_played_ms() - since >= ms;
}

static void audio_checks(void) {
    ink_audio_on_stream_failed(on_failed, NULL);
    metalio_board_set(1, MB_P1_SHARED_PWR, 0);
    double played0 = sim_audio_played_ms();
    int id = ink_audio_stream_open(INK_AUDIO_PRIO_VOICE, 0, NULL);
    CHECK(id > 0 && push_frames(id, 10) == 10, "a stream takes 10 frames while the speaker cannot start");
    ink_audio_stream_finish(id);
    osal_task_delay_us(1500000);
    CHECK(sim_audio_played_ms() == played0 && g_failed_id == 0, "they wait: nothing played, the stream has not failed");
    metalio_board_set(1, MB_P1_SHARED_PWR, MB_P1_SHARED_PWR);
    osal_task_delay_us(4000000);
    double played = sim_audio_played_ms() - played0;
    CHECK(played >= 590 && g_failed_id == 0, "once the speaker can start all of them play (%.0f ms of 600)", played);
    metalio_board_set(1, MB_P1_SHARED_PWR, 0);
    id = ink_audio_stream_open(INK_AUDIO_PRIO_VOICE, 0, NULL);
    push_frames(id, 5);
    osal_task_delay_us(10000000);
    CHECK(g_failed_id == id && push_frames(id, 1) == 0, "a stream whose speaker stays off fails after a while, its owner is told (%d, %d)",
          g_failed_id, id);
    metalio_board_set(1, MB_P1_SHARED_PWR, MB_P1_SHARED_PWR);
    osal_task_delay_us(2000000);

    /* the chip's supply going away under a stream that plays, and coming back (audit AUD6-001): the
     * output stalls, is opened again (the chip set up again) and the rest plays; gone for good, the
     * stream fails and nothing keeps the device awake (amplifier off first, as the wiring needs) */
    g_failed_id = 0;
    played0 = sim_audio_played_ms();
    id = ink_audio_stream_open(INK_AUDIO_PRIO_VOICE, 0, NULL);
    push_frames(id, 50); /* 3 s */
    ink_audio_stream_finish(id);
    bool was_playing = playing_for(played0, 200);
    double stalls0 = sim_metric_value("audio.output_stalls");
    metalio_board_set(0, MB_P0_AMP, 0);
    metalio_board_set(1, MB_P1_SHARED_PWR, 0);
    osal_task_delay_us(1000000);
    metalio_board_set(1, MB_P1_SHARED_PWR, MB_P1_SHARED_PWR);
    osal_task_delay_us(8000000);
    played = sim_audio_played_ms() - played0;
    CHECK(was_playing && sim_metric_value("audio.output_stalls") > stalls0 && played >= 3000 - 61 && played <= 3000 && !sim_audio_busy() &&
              g_failed_id == 0,
          "a supply cut under a playing stream: it stalls, is opened again and all but the ring's 60 ms plays (%.0f ms of 3000)", played);
    played0 = sim_audio_played_ms();
    id = ink_audio_stream_open(INK_AUDIO_PRIO_VOICE, 0, NULL);
    push_frames(id, 50);
    ink_audio_stream_finish(id);
    CHECK(playing_for(played0, 200), "the next stream plays");
    metalio_board_set(0, MB_P0_AMP, 0);
    metalio_board_set(1, MB_P1_SHARED_PWR, 0);
    osal_task_delay_us(11000000);
    CHECK(g_failed_id == id && !sim_audio_busy(), "cut for good: the stream fails, its owner is told and the output is off (%d, %d)", g_failed_id,
          id);
    metalio_board_set(1, MB_P1_SHARED_PWR, MB_P1_SHARED_PWR);
    osal_task_delay_us(2000000);

    /* cancelled while its stalled output is being opened again (that waits for the chip to boot and take
     * its mode, ~1.4 s): nothing more of it plays (audit AUD7-003) */
    played0 = sim_audio_played_ms();
    id = ink_audio_stream_open(INK_AUDIO_PRIO_VOICE, 0, NULL);
    push_frames(id, 50);
    ink_audio_stream_finish(id);
    bool playing = playing_for(played0, 200);
    metalio_board_set(0, MB_P0_AMP, 0);
    metalio_board_set(1, MB_P1_SHARED_PWR, 0);
    osal_task_delay_us(1300000); /* stalled and closed; opening fails while P14 is off */
    metalio_board_set(1, MB_P1_SHARED_PWR, MB_P1_SHARED_PWR);
    osal_task_delay_us(500000); /* the next try is waiting for the chip */
    double at_cancel = sim_audio_played_ms();
    ink_audio_stop_all();
    osal_task_delay_us(3000000);
    CHECK(playing && sim_audio_played_ms() == at_cancel && !sim_audio_busy(),
          "a stream cancelled while its output is opened again plays nothing more (%.0f ms after the cancel)",
          sim_audio_played_ms() - at_cancel);

    /* a tone while a long stream plays normally: it goes out after the frame staged now, not after the
     * stream (audit AUD8-001), and none of the stream is lost to it */
    played0 = sim_audio_played_ms();
    id = ink_audio_stream_open(INK_AUDIO_PRIO_VOICE, 0, NULL);
    push_frames(id, 50);
    ink_audio_stream_finish(id);
    bool streaming = playing_for(played0, 300);
    ink_audio_tone(INK_TONE_DONE, osal_now_us(), "systest.tone_heard_ms");
    osal_task_delay_us(5000000);
    double heard = sim_metric_last("systest.tone_heard_ms"), total = sim_audio_played_ms() - played0;
    CHECK(streaming && heard > 0 && heard < 200 && total >= 3060 - 2 && total <= 3060 + 2,
          "a tone in a playing stream is heard after %.0f ms, and the stream plays in full (%.0f ms of 3000 + 60)", heard, total);
    ink_audio_on_stream_failed(NULL, NULL);
}


/* ---- step 1: capabilities, arguments, state and settings (detailed design §2.7) ---- */

static int g_runs;

static void t_run(ink_call_t *c) {
    g_runs++;
    int64_t n;
    if (ink_args_get_int(&c->args, "n", &n)) c->rc.revision = (uint32_t)n;
    char name[16];
    if (ink_args_get_str(&c->args, "name", name, sizeof name) >= 0) snprintf(c->rc.object, sizeof c->rc.object, "name:%s", name);
}

static const ink_cap_t TEST_CAPS[] = {
    {.id = "test.read", .title = "读", .effect = INK_FX_READ, .child_ok = true, .run = t_run},
    {.id = "test.args",
     .title = "参数",
     .input = "{\"type\":\"object\",\"properties\":{\"n\":{\"type\":\"integer\",\"minimum\":1,\"maximum\":9},"
              "\"name\":{\"type\":\"string\",\"maxLength\":3},\"mode\":{\"type\":\"string\",\"enum\":[\"a\",\"b\"]},"
              "\"on\":{\"type\":\"boolean\"}},\"required\":[\"n\"],\"additionalProperties\":false}",
     .effect = INK_FX_LOCAL_WRITE,
     .child_ok = true,
     .run = t_run},
    {.id = "test.external", .title = "外发", .effect = INK_FX_EXTERNAL, .child_ok = true, .run = t_run},
    {.id = "test.system", .title = "系统", .effect = INK_FX_SYSTEM, .run = t_run},
    {.id = "test.hidden", .title = "不给 AI", .effect = INK_FX_READ, .child_ok = true, .expose = INK_EXPOSE_APP, .run = t_run},
    {.id = "test.page", .title = "页面", .effect = INK_FX_NAVIGATE, .child_ok = true, .activity = "nowhere", .run = t_run},
    {.id = "test.upgrade", .title = "升级", .effect = INK_FX_NAVIGATE, .run = t_run},
    {.id = "test.grown", .title = "成人的", .effect = INK_FX_READ, .run = t_run},
};

static ink_rc_t json_call(const char *id, const char *args, ink_receipt_t *rc) { return ink_cap_invoke_json(id, args, INK_SRC_AI, 0, rc); }

static void cap_checks(void) {
    static char buf[4096];
    ink_receipt_t rc;
    CHECK(ink_cap_register(TEST_CAPS, (int)(sizeof TEST_CAPS / sizeof TEST_CAPS[0])) == 0, "test capabilities register");
    CHECK(ink_cap_invoke("test.read", NULL, INK_SRC_TOUCH, 0, &rc) == INK_RC_DONE && g_runs == 1, "a read runs (%s)", ink_rc_name(rc.status));
    CHECK(ink_cap_invoke("test.nope", NULL, INK_SRC_TOUCH, 0, &rc) == INK_RC_INVALID, "an unknown id is invalid");
    CHECK(!ink_cap_available("test.page") && ink_cap_available("test.read"), "page-bound capabilities are available only on their page");
    CHECK(ink_cap_invoke("test.page", NULL, INK_SRC_TOUCH, 0, &rc) == INK_RC_INVALID, "a page-bound capability off its page is invalid");
    int runs = g_runs;
    CHECK(ink_cap_invoke("test.hidden", NULL, INK_SRC_AI, 0, &rc) == INK_RC_INVALID && g_runs == runs, "not offered to the AI: invalid, not run");
    CHECK(ink_cap_invoke("test.hidden", NULL, INK_SRC_APP, 0, &rc) == INK_RC_DONE, "offered to apps: runs for an app");
    CHECK(ink_cap_invoke("test.external", NULL, INK_SRC_AI, 0, &rc) == INK_RC_PENDING_CONFIRMATION && g_runs == runs + 1,
          "an external effect asked by the AI waits for confirmation and does not run (%s)", ink_rc_name(rc.status));
    CHECK(ink_cap_invoke("test.external", NULL, INK_SRC_TOUCH, 0, &rc) == INK_RC_DONE, "the same by touch runs");
    CHECK(ink_cap_invoke("test.upgrade", NULL, INK_SRC_AI, 0, &rc) == INK_RC_REJECTED, "the AI never starts an upgrade");

    /* arguments from JSON, checked against the schema */
    CHECK(json_call("test.args", "{\"n\":5}", &rc) == INK_RC_DONE && rc.revision == 5, "an integer argument arrives (%s)", rc.detail);
    CHECK(json_call("test.args", "{\"n\":0}", &rc) == INK_RC_INVALID && strstr(rc.detail, ">= 1"), "below minimum is refused (%s)", rc.detail);
    CHECK(json_call("test.args", "{\"n\":10}", &rc) == INK_RC_INVALID, "above maximum is refused");
    CHECK(json_call("test.args", "{}", &rc) == INK_RC_INVALID && strstr(rc.detail, "n required"), "a missing required argument is refused (%s)",
          rc.detail);
    CHECK(json_call("test.args", "{\"n\":\"5\"}", &rc) == INK_RC_INVALID, "a string for an integer is refused");
    CHECK(json_call("test.args", "{\"n\":1.5}", &rc) == INK_RC_INVALID, "a fraction for an integer is refused");
    CHECK(json_call("test.args", "{\"n\":1,\"zz\":1}", &rc) == INK_RC_INVALID, "an unexpected argument is refused when the schema is closed");
    CHECK(json_call("test.args", "{\"n\":1,\"name\":\"abcd\"}", &rc) == INK_RC_INVALID, "a string longer than maxLength is refused");
    CHECK(json_call("test.args", "{\"n\":1,\"name\":\"汉字汉\"}", &rc) == INK_RC_DONE && !strcmp(rc.object, "name:汉字汉"),
          "maxLength counts characters, not bytes (%s)", rc.detail);
    CHECK(json_call("test.args", "{\"n\":1,\"mode\":\"c\"}", &rc) == INK_RC_INVALID, "a value outside the enum is refused");
    CHECK(json_call("test.args", "{\"n\":1,\"mode\":\"b\",\"on\":true}", &rc) == INK_RC_DONE, "enum and boolean values pass");
    CHECK(json_call("test.args", "[1]", &rc) == INK_RC_INVALID, "arguments that are not an object are refused");

    /* the same capability from local C values: no JSON on the way */
    static ink_args_t a, back; /* static: the test task's stack is small */
    ink_args_init(&a);
    ink_args_int(&a, "n", 3);
    CHECK(ink_cap_invoke("test.args", &a, INK_SRC_APP, 0, &rc) == INK_RC_DONE && rc.revision == 3, "local arguments arrive as values");
    ink_args_init(&a);
    ink_args_int(&a, "n", 0);
    CHECK(ink_cap_invoke("test.args", &a, INK_SRC_APP, 0, &rc) == INK_RC_INVALID, "local arguments are checked as well");
    ink_args_init(&a);
    ink_args_int(&a, "n", 3);
    ink_args_str(&a, "name", "a\"b");
    int n = ink_args_to_json(&a, buf, sizeof buf);
    char why[96], name[8];
    CHECK(n > 0 && ink_args_from_json(&back, buf, (size_t)n, NULL, why, sizeof why) == 0 && ink_args_get_str(&back, "name", name, sizeof name) == 3 &&
              !strcmp(name, "a\"b"),
          "arguments survive a round trip through JSON (%.120s)", buf);

    /* the tool list is generated from the table */
    n = ink_cap_manifest_json(buf, sizeof buf);
    cJSON *list = n > 0 ? cJSON_Parse(buf) : NULL; /* the whole list: more tokens than the device parser takes */
    CHECK(cJSON_IsArray(list) && cJSON_GetArraySize(list) >= 8, "the tool list is a JSON array: the 8 test and settings capabilities offered to "
          "the AI, and the services' own (%d)", cJSON_GetArraySize(list));
    cJSON_Delete(list);
    CHECK(strstr(buf, "\"name\":\"test.args\"") && strstr(buf, "\"maxLength\":3") && !strstr(buf, "test.hidden") && !strstr(buf, "test.page"),
          "it lists what the AI may use now, with the schemas");
    n = ink_cap_ids_json(buf, sizeof buf);
    CHECK(n > 0 && strstr(buf, "\"test.read\"") && !strstr(buf, "test.hidden"), "the id list for snapshots matches");
}

static int g_seen[3];
static int64_t g_slow_seen;

static void on_state(void *ctx, int h, const char *id) {
    (void)ctx;
    (void)h;
    if (!strcmp(id, "test.level")) g_seen[0]++;
    else if (!strcmp(id, "test.name")) g_seen[1]++;
    else if (!strcmp(id, "test.slow")) {
        g_seen[2]++;
        ink_state_get_int(h, &g_slow_seen);
    }
}

static void state_checks(void) {
    static const ink_state_def_t LEVEL = {.id = "test.level", .type = INK_ST_INT, .title = "等级"};
    static const ink_state_def_t NAME = {.id = "test.name", .type = INK_ST_STR, .title = "名字"};
    static const ink_state_def_t SLOW = {.id = "test.slow", .type = INK_ST_INT, .title = "慢", .min_interval_ms = 500};
    int h = ink_state_define(&LEVEL), hn = ink_state_define(&NAME), hs = ink_state_define(&SLOW);
    CHECK(h >= 0 && hn >= 0 && hs >= 0 && ink_state_find("test.name") == hn, "state items are defined and found");
    CHECK(ink_state_watch("test.", on_state, NULL, INK_EXEC_DIRECT) == 0, "a watcher subscribes");
    int64_t v = -1;
    CHECK(!ink_state_get_int(h, &v), "an item has no value until its service sets one");
    ink_state_set_int(h, 5);
    CHECK(ink_state_get_int(h, &v) && v == 5 && g_seen[0] == 1, "a change is stored and told once (%d)", g_seen[0]);
    ink_state_set_int(h, 5);
    CHECK(g_seen[0] == 1, "an unchanged value tells nobody");
    ink_state_set_int(h, 6);
    char s[8];
    ink_state_set_str(hn, "abc");
    CHECK(g_seen[0] == 2 && g_seen[1] == 1 && ink_state_get_str(hn, s, sizeof s) == 3 && !strcmp(s, "abc"), "integers and strings");
    ink_state_set_int(hs, 1);
    ink_state_set_int(hs, 2);
    ink_state_set_int(hs, 3);
    CHECK(g_seen[2] == 1 && g_slow_seen == 1, "within its minimum interval a change waits (%d)", g_seen[2]);
    osal_task_delay_us(600000);
    CHECK(g_seen[2] == 2 && g_slow_seen == 3, "after it, one delivery with the latest value (%d, %lld)", g_seen[2], (long long)g_slow_seen);
    static char buf[512];
    CHECK(ink_state_json(buf, sizeof buf, 0) > 0 && strstr(buf, "\"test.level\":6") && strstr(buf, "\"test.name\":\"abc\""), "the snapshot (%.120s)", buf);
}

static int64_t g_applied = -1;
static int g_set_seen;

static void apply_vol(void *ctx, int64_t v) {
    (void)ctx;
    g_applied = v;
}

static void on_setting(void *ctx, int i, const char *id) {
    (void)ctx;
    (void)i;
    (void)id;
    g_set_seen++;
}

static const char *const MODES[] = {"a", "b", "c", NULL};
static const ink_setting_def_t TEST_SETTINGS[] = {
    {.id = "test.vol", .title = "测试音量", .type = INK_SET_INT, .min = 0, .max = 10, .def = 3, .child_may_change = true, .apply = apply_vol,
     .apply_exec = INK_EXEC_DIRECT},
    {.id = "test.mode", .title = "模式", .type = INK_SET_ENUM, .def = 0, .choices = MODES},
    {.id = "test.sys", .title = "系统开关", .type = INK_SET_BOOL, .def = 0, .system = true},
};

static void settings_checks(void) {
    static char buf[1024];
    ink_receipt_t rc;
    CHECK(ink_settings_define(TEST_SETTINGS, 3) == 0 && ink_setting_get("test.vol") == 3, "settings start at their defaults");
    CHECK(ink_settings_watch("test.", on_setting, NULL, INK_EXEC_DIRECT) == 0, "a settings watcher subscribes");
    CHECK(ink_setting_set("test.vol", 11, INK_SRC_TOUCH, &rc) == INK_RC_INVALID && ink_setting_get("test.vol") == 3, "out of range is refused");
    CHECK(ink_setting_set("test.vol", 7, INK_SRC_TOUCH, &rc) == INK_RC_DONE && g_applied == 7 && g_set_seen == 1 && ink_setting_get("test.vol") == 7,
          "a change is applied by its service and told (%lld, %d)", (long long)g_applied, g_set_seen);
    CHECK(ink_cap_invoke_json("settings.set", "{\"id\":\"test.mode\",\"value\":\"c\"}", INK_SRC_AI, 0, &rc) == INK_RC_DONE &&
              ink_setting_get("test.mode") == 2,
          "the AI changes a setting through settings.set, enum values by name (%s)", rc.detail);
    CHECK(ink_cap_invoke_json("settings.set", "{\"id\":\"test.sys\",\"value\":true}", INK_SRC_AI, 0, &rc) == INK_RC_PENDING_CONFIRMATION &&
              ink_setting_get("test.sys") == 0,
          "a system-level setting asked by the AI waits for confirmation (%s)", ink_rc_name(rc.status));
    CHECK(ink_setting_set("test.sys", 1, INK_SRC_TOUCH, &rc) == INK_RC_DONE, "the same by touch is done");
    CHECK(ink_cap_invoke_json("settings.get", "{\"id\":\"test.mode\"}", INK_SRC_AI, 0, &rc) == INK_RC_DONE && strstr(rc.say, "c"), "settings.get (%s)",
          rc.say);
    CHECK(ink_settings_json(buf, sizeof buf) > 0 && strstr(buf, "\"id\":\"test.mode\"") && strstr(buf, "\"choices\":[\"a\",\"b\",\"c\"]"),
          "the table for the settings app");
    osal_task_delay_us(1200000); /* one write for the changes above (budget: settings.saved == 2) */
    drain_storage();
    int n = ink_storage_read(INK_SETTINGS_PATH, buf, sizeof buf - 1);
    buf[n > 0 ? n : 0] = 0;
    CHECK(n > 0 && strstr(buf, "\"test.vol\":7") && strstr(buf, "\"test.mode\":2"), "the values are saved (%.120s)", buf);
    for (int v = 1; v <= 3; v++) ink_setting_set("test.vol", v, INK_SRC_TOUCH, &rc);
    osal_task_delay_us(1200000);
    drain_storage();
    n = ink_storage_read(INK_SETTINGS_PATH, buf, sizeof buf - 1);
    buf[n > 0 ? n : 0] = 0;
    CHECK(strstr(buf, "\"test.vol\":3"), "a burst of changes is saved once, with the last value (%.120s)", buf);

    /* start-up: saved values come back; one out of range falls back to the default */
    static const char saved[] = "{\"test.vol\":3,\"test.loaded\":5,\"test.bad\":99,\"test.future\":1}";
    CHECK(ink_storage_write_atomic(INK_SETTINGS_PATH, saved, sizeof saved - 1) == 0, "a settings file is written");
    drain_storage();
    static const ink_setting_def_t LATER[] = {
        {.id = "test.loaded", .title = "读回", .type = INK_SET_INT, .min = 0, .max = 9, .def = 1},
        {.id = "test.bad", .title = "坏值", .type = INK_SET_INT, .min = 0, .max = 9, .def = 4},
    };
    CHECK(ink_settings_init() == 0 && ink_settings_define(LATER, 2) == 0 && ink_setting_get("test.loaded") == 5 && ink_setting_get("test.bad") == 4,
          "saved values are read back at start; out of range ones fall back (%lld, %lld)", (long long)ink_setting_get("test.loaded"),
          (long long)ink_setting_get("test.bad"));
}

/* ---- step 2: the soft bus and the executors (detailed design §3.5) ---- */

static volatile int g_bus_done;
static ink_receipt_t g_bus_rc;
static bool g_bus_on_bg;
static uint64_t g_bus_t;
static int g_counted;
static ink_call_t *g_parked;

static void bus_done(void *ctx, const ink_receipt_t *rc) {
    (void)ctx;
    g_bus_rc = *rc;
    g_bus_on_bg = ink_exec_is_current(INK_EXEC_BG);
    g_bus_t = osal_now_us();
    g_bus_done++;
}

static void t_where(ink_call_t *c) { c->rc.revision = ink_exec_is_current(INK_EXEC_BG) ? 1 : ink_exec_is_current(INK_EXEC_MEDIA) ? 2 : 0; }
static void t_count(ink_call_t *c) { c->rc.revision = (uint32_t)++g_counted; }
static void t_park(ink_call_t *c) {
    g_parked = c;
    ink_cap_defer(c);
}

static const ink_cap_t BUS_CAPS[] = {
    {.id = "test.on_bg", .title = "后台", .effect = INK_FX_READ, .child_ok = true, .exec = INK_EXEC_BG, .run = t_where},
    {.id = "test.on_media", .title = "媒体", .effect = INK_FX_READ, .child_ok = true, .exec = INK_EXEC_MEDIA, .run = t_where},
    {.id = "test.count", .title = "计数", .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .exec = INK_EXEC_DIRECT, .run = t_count},
    {.id = "test.park", .title = "不回", .effect = INK_FX_READ, .child_ok = true, .exec = INK_EXEC_DIRECT, .timeout_ms = 200, .run = t_park},
};

static bool wait_done(int n, uint64_t max_us) {
    for (uint64_t w = 0; g_bus_done < n && w < max_us; w += 1000) osal_task_delay_us(1000);
    return g_bus_done >= n;
}

static void bus_checks(void) {
    CHECK(ink_cap_register(BUS_CAPS, (int)(sizeof BUS_CAPS / sizeof BUS_CAPS[0])) == 0, "bus test capabilities register");
    ink_call_opts_t o = {.src = INK_SRC_APP, .reply_exec = INK_EXEC_DIRECT, .done = bus_done};
    uint64_t t0 = osal_now_us();
    CHECK(ink_cap_call("test.count", NULL, &o) == 0 && g_bus_done == 1 && g_bus_rc.revision == 1, "a direct call is answered at once");
    CHECK(g_bus_t - t0 < 200, "within 0.2 ms (%llu us)", (unsigned long long)(g_bus_t - t0));
    o.reply_exec = INK_EXEC_BG;
    t0 = osal_now_us();
    CHECK(ink_cap_call("test.on_bg", NULL, &o) == 0 && wait_done(2, 100000) && g_bus_rc.revision == 1 && g_bus_on_bg,
          "a background capability runs on exec.bg and its receipt comes back where asked");
    CHECK(g_bus_t - t0 < 2000, "across tasks in < 2 ms (%llu us)", (unsigned long long)(g_bus_t - t0));
    o.reply_exec = INK_EXEC_DIRECT;
    CHECK(ink_cap_call("test.on_media", NULL, &o) == 0 && wait_done(3, 100000) && g_bus_rc.revision == 2, "a media capability runs on exec.media");
    o.request_id = "rq-1";
    CHECK(ink_cap_call("test.count", NULL, &o) == 0 && ink_cap_call("test.count", NULL, &o) == 0 && wait_done(5, 100000) && g_counted == 2 &&
              g_bus_rc.revision == 2,
          "a repeated request id runs once and gets the same receipt (%d runs)", g_counted);
    o.request_id = NULL;
    CHECK(ink_cap_call("test.park", NULL, &o) == 0 && g_bus_done == 5, "a deferred call is not answered at once");
    osal_task_delay_us(300000);
    CHECK(g_bus_done == 6 && g_bus_rc.status == INK_RC_FAILED && strstr(g_bus_rc.detail, "timed out"), "after its deadline: failed, timed out (%s)",
          g_bus_rc.detail);
    ink_cap_done(g_parked); /* late: dropped (budget: cap.late == 1) */
    osal_task_delay_us(1000);
    CHECK(g_bus_done == 6, "its late completion is dropped");
    ink_receipt_t rc;
    CHECK(ink_cap_invoke("test.on_bg", NULL, INK_SRC_APP, 0, &rc) == INK_RC_QUEUED, "waiting on a background capability answers queued");

    /* the remote proxy: a capability of the (simulated) hub */
    ink_args_t a;
    ink_args_init(&a);
    ink_args_int(&a, "x", 42);
    CHECK(ink_cap_call("hub/echo", &a, &o) == 0 && wait_done(7, 15000000) && g_bus_rc.status == INK_RC_DONE && strstr(g_bus_rc.say, "\"x\":42"),
          "a hub capability through the remote proxy (%s, %s)", ink_rc_name(g_bus_rc.status), g_bus_rc.say);
    CHECK(ink_cap_call("hub/nope", NULL, &o) == 0 && wait_done(8, 15000000) && g_bus_rc.status == INK_RC_FAILED && g_bus_rc.detail[0],
          "an unknown hub capability fails with the hub's reason (%s)", g_bus_rc.detail);
}

/* ---- step 3: the services' state, settings, capabilities and events (detailed design §4.4) ---- */

static volatile int g_sms_events;
static char g_sms_text[64];

static void on_message(void *ctx, const char *id, const ink_args_t *payload) {
    (void)ctx;
    (void)id;
    ink_args_get_str(payload, "text", g_sms_text, sizeof g_sms_text);
    g_sms_events++;
}

static bool state_is(const char *id, const char *want) {
    char s[32];
    return ink_state_get_str(ink_state_find(id), s, sizeof s) >= 0 && !strcmp(s, want);
}

static void service_checks(void) {
    int64_t v = -1;
    char ps[16] = "";
    ink_state_get_str(ink_state_find("power.state"), ps, sizeof ps);
    CHECK(!strcmp(ps, "active") || !strcmp(ps, "idle"), "power.state is published (%s)", ps);
    CHECK(ink_state_get_int(ink_state_find("battery.level"), &v) && v > 0 && v <= 100, "battery.level is published (%lld %%)", (long long)v);
    CHECK(ink_state_get_int(ink_state_find("net.online"), &v) && ink_state_find("net.type") >= 0, "net.online and net.type are published");
    CHECK(state_is("member.current", "owner") && state_is("audio.now_playing", "none"), "member.current and audio.now_playing are published");
    static char buf[8192];
    CHECK(ink_settings_json(buf, sizeof buf) > 0 && strstr(buf, "\"audio.volume\"") && strstr(buf, "\"voice.handsfree\"") &&
              strstr(buf, "\"power.lock_after_s\"") && strstr(buf, "\"display.light\"") && strstr(buf, "\"net.prefer\"") && strstr(buf, "\"cell.sim_slot\""),
          "the services' settings are in the table");
    CHECK(ink_setting_get("voice.handsfree") == 0, "hands-free listens only while the screen is on, by default (D-52)");
    ink_receipt_t rc;
    CHECK(ink_setting_set("power.lock_after_s", 30, INK_SRC_TOUCH, &rc) == INK_RC_INVALID, "auto-lock below a minute is refused");
    CHECK(ink_setting_set("display.light", 40, INK_SRC_TOUCH, &rc) == INK_RC_DONE, "the front light is a setting");
    /* internal mechanisms are not offered (architecture §4.1) */
    CHECK(ink_cap_manifest_json(buf, sizeof buf) > 0 && !strstr(buf, "modem") && !strstr(buf, "AT+") && !strstr(buf, "rail") && !strstr(buf, "refresh_sched"),
          "no AT commands, rails or refresh scheduling among the capabilities");
    CHECK(ink_state_json(buf, sizeof buf, 0) > 0 && !strstr(buf, "rail") && !strstr(buf, "modem"), "nor in the state");

    /* camera */
    ink_call_opts_t o = {.src = INK_SRC_APP, .reply_exec = INK_EXEC_DIRECT, .done = bus_done};
    int before = g_bus_done;
    CHECK(ink_cap_call("camera.capture", NULL, &o) == 0 && wait_done(before + 1, 6000000) && g_bus_rc.status == INK_RC_DONE &&
              !strcmp(g_bus_rc.object, "photo:1"),
          "camera.capture takes a picture (%s, %s)", ink_rc_name(g_bus_rc.status), g_bus_rc.detail);
    CHECK(ink_cap_call("camera.capture", NULL, &o) == 0 && wait_done(before + 2, 6000000) && !strcmp(g_bus_rc.object, "photo:2"),
          "a second one queues behind it (%s)", g_bus_rc.object);

    /* text messages (subscribed at the start: the script's message may come before these checks) */
    ink_args_t a;
    ink_args_init(&a);
    ink_args_str(&a, "to", "10086");
    ink_args_str(&a, "text", "查询余额");
    o.src = INK_SRC_AI;
    before = g_bus_done;
    CHECK(ink_cap_call("message.send", &a, &o) == 0 && wait_done(before + 1, 100000) && g_bus_rc.status == INK_RC_PENDING_CONFIRMATION,
          "a message the AI wants to send waits for confirmation (%s)", ink_rc_name(g_bus_rc.status));
    o.src = INK_SRC_TOUCH;
    CHECK(ink_cap_call("message.send", &a, &o) == 0 && wait_done(before + 2, 30000000) && g_bus_rc.status == INK_RC_DONE,
          "sent by touch (%s, %s)", ink_rc_name(g_bus_rc.status), g_bus_rc.detail);
    for (int w = 0; w < 200 && !g_sms_events; w++) osal_task_delay_us(100000); /* the script sends one at 24 s */
    CHECK(g_sms_events == 1 && strstr(g_sms_text, "测试短信"), "an incoming message is an event with its text (%d, %s)", g_sms_events, g_sms_text);
    CHECK(ink_cap_invoke("message.list", NULL, INK_SRC_AI, 0, &rc) == INK_RC_DONE && rc.revision == 1 && strstr(rc.say, "10086"), "message.list (%s)",
          rc.say);

    /* the card in use is a setting; the switch is the driver's (R-044) */
    CHECK(ink_setting_set("cell.sim_slot", 1, INK_SRC_TOUCH, &rc) == INK_RC_DONE, "choose the internal card");
    for (int w = 0; w < 300 && !state_is("cell.sim", "internal"); w++) osal_task_delay_us(100000);
    CHECK(state_is("cell.sim", "internal"), "the modem switched online and cell.sim says so");
}

/* ---- step 4: resource management (detailed design §5.3) ---- */

static void *g_spare;     /* what the test cache gives back */
static int g_reclaims;
static int g_steps;
static bool g_steps_on_bg = true;
static int g_finished, g_cancelled;

static size_t test_reclaim(void *ctx, size_t want) {
    (void)ctx;
    (void)want;
    g_reclaims++;
    if (!g_spare) return 0;
    osal_free(g_spare);
    g_spare = NULL;
    return 1536u * 1024u;
}

static ink_job_step_t three_steps(void *ctx) {
    (void)ctx;
    g_steps_on_bg = g_steps_on_bg && ink_exec_is_current(INK_EXEC_BG);
    return ++g_steps >= 3 ? INK_JOB_DONE : INK_JOB_MORE;
}
static ink_job_step_t forever(void *ctx) {
    (void)ctx;
    osal_task_delay_us(5000);
    return INK_JOB_MORE;
}
static void job_finished(void *ctx, ink_job_step_t how, bool cancelled) {
    (void)ctx;
    (void)how;
    if (cancelled) g_cancelled++;
    else g_finished++;
}

static bool wait_for(volatile int *v, int want, uint64_t max_us) {
    for (uint64_t w = 0; *v < want && w < max_us; w += 2000) osal_task_delay_us(2000);
    return *v >= want;
}

static void res_checks(void) {
    /* quotas */
    CHECK(ink_mem_budget("test.mem", INK_MEM_PSRAM, 64 * 1024) == 0, "a quota is declared");
    void *a = ink_mem_alloc("test.mem", INK_MEM_PSRAM, 48 * 1024);
    void *b = ink_mem_alloc("test.mem", INK_MEM_PSRAM, 32 * 1024);
    CHECK(a && !b && ink_mem_used("test.mem", INK_MEM_PSRAM) == 48 * 1024, "past its quota a component gets nothing");
    ink_mem_free(a);
    b = ink_mem_alloc("test.mem", INK_MEM_PSRAM, 32 * 1024);
    CHECK(b && ink_mem_used("test.mem", INK_MEM_PSRAM) == 32 * 1024, "after a free it fits again");
    ink_mem_free(b);

    /* jobs: steps on exec.bg, cancel, hold */
    ink_job_t j = {.name = "three", .step = three_steps, .finish = job_finished};
    CHECK(ink_job_submit(&j) > 0 && wait_for(&g_finished, 1, 2000000) && g_steps == 3 && g_steps_on_bg, "a three-step job runs on exec.bg (%d)",
          g_steps);
    ink_job_t f = {.name = "forever", .step = forever, .finish = job_finished};
    int id = ink_job_submit(&f);
    osal_task_delay_us(50000);
    ink_job_cancel(id);
    CHECK(id > 0 && wait_for(&g_cancelled, 1, 2000000) && ink_jobs_pending() == 0, "a cancelled job stops and is told so");
    ink_jobs_hold(true);
    g_steps = 0;
    CHECK(ink_job_submit(&j) > 0, "a job is queued while jobs are held");
    osal_task_delay_us(200000);
    CHECK(g_steps == 0, "a held job does not run");
    ink_jobs_hold(false);
    CHECK(wait_for(&g_finished, 2, 2000000) && g_steps == 3, "released, it runs");

    /* pressure: reclaim first, then pause background work */
    CHECK(ink_mem_reclaimer("test-cache", 0, test_reclaim, NULL) == 0, "a reclaimable cache registers");
    osal_mem_stats_t st;
    osal_mem_stats(OSAL_MEM_EXTERNAL, &st);
    static void *hog[24];
    int nh = 0;
    while (st.free > 3u * 1024u * 1024u && nh < 24) { /* fill PSRAM to just under 3 MiB free */
        hog[nh++] = osal_malloc(1024u * 1024u, OSAL_MEM_EXTERNAL);
        osal_mem_stats(OSAL_MEM_EXTERNAL, &st);
    }
    g_spare = osal_malloc(1536u * 1024u, OSAL_MEM_EXTERNAL); /* now under 2 MiB: tight */
    CHECK(!ink_mem_admit(INK_MEM_PSRAM, 512 * 1024), "a large block is not admitted when it would make PSRAM tight");
    CHECK(ink_mem_check() == INK_PRESSURE_NORMAL && g_reclaims >= 1 && !g_spare, "tight: caches give memory back and it is normal again (%d)",
          g_reclaims);
    static void *keep[16]; /* take all but ~700 KiB (free space may be in pieces); twice: the first check */
    int nk = 0;             /* lets every cache (the camera's last photo too) give back what it has */
    for (int round = 0; round < 2; round++) {
        for (osal_mem_stats(OSAL_MEM_EXTERNAL, &st); st.free > 760u * 1024u && nk < 16; osal_mem_stats(OSAL_MEM_EXTERNAL, &st)) {
            size_t want = st.free - 700u * 1024u;
            keep[nk++] = osal_malloc(want < st.largest_free ? want : st.largest_free, OSAL_MEM_EXTERNAL);
        }
        if (!round) ink_mem_check();
    }
    ink_pressure_t lv = ink_mem_check();
    osal_mem_stats(OSAL_MEM_EXTERNAL, &st);
    CHECK(lv >= INK_PRESSURE_LOW, "with nothing left to give, pressure stays (%s; PSRAM free %zu, %d blocks)", ink_pressure_name(lv), st.free, nk);
    g_steps = 0;
    CHECK(ink_job_submit(&j) > 0, "a job is queued under pressure");
    osal_task_delay_us(200000);
    CHECK(g_steps == 0, "it waits while memory is low");
    for (int i = 0; i < nk; i++) osal_free(keep[i]);
    for (int i = 0; i < nh; i++) osal_free(hog[i]);
    CHECK(ink_mem_check() == INK_PRESSURE_NORMAL && wait_for(&g_finished, 3, 2000000) && g_steps == 3, "freed, pressure is normal and it runs");
}

/* ---- step 5: realtime sessions (detailed design §6.6) ---- */

static volatile int g_echoes;
static uint8_t g_echo_kind[32];
static uint64_t g_echo_t[32];

static void on_echo(void *ctx, int sid, uint8_t kind, const uint8_t *payload, size_t len, uint16_t seq, uint32_t t_ms, uint8_t flags) {
    (void)ctx;
    (void)sid;
    (void)payload;
    (void)len;
    (void)seq;
    (void)t_ms;
    (void)flags;
    if (g_echoes < 32) {
        g_echo_kind[g_echoes] = kind;
        g_echo_t[g_echoes] = osal_now_us();
    }
    g_echoes++;
}

static void rt_checks(void) {
    static const ink_rt_handler_t H = {.on_frame = on_echo};
    int sid = ink_rt_open("echo", NULL, &H);
    CHECK(sid > 0 && ink_rt_open_count() == 1, "a realtime session opens (sid %d)", sid);
    for (int w = 0; w < 100 && !ink_rt_ready(sid); w++) osal_task_delay_us(100000);
    CHECK(ink_hub_online() && ink_rt_ready(sid), "an open session connects the hub and is confirmed by it");
    uint8_t voice[160];
    memset(voice, 0x55, sizeof voice);
    uint64_t t0 = osal_now_us();
    CHECK(ink_rt_send(sid, INK_RT_VOICE_UP, voice, sizeof voice, 0) == 0 && wait_for(&g_echoes, 1, 5000000) && g_echo_kind[0] == INK_RT_VOICE_UP,
          "a frame goes out and comes back");
    CHECK(g_echo_t[0] - t0 < 200000, "round trip through the hub in %llu ms (Wi-Fi model)", (unsigned long long)((g_echo_t[0] - t0) / 1000));

    /* priorities: background bulk queued first, a voice frame after it, the voice frame leaves first */
    static uint8_t bulk[2048 + INK_RT_HDR];
    memset(bulk, 0, sizeof bulk);
    bulk[0] = INK_RT_SCREEN;
    bulk[1] = (uint8_t)sid;
    for (int i = 0; i < 16; i++) ink_hub_send_frame(bulk, sizeof bulk, INK_HUB_TX_BACKGROUND);
    ink_rt_send(sid, INK_RT_VOICE_UP, voice, sizeof voice, 0);
    CHECK(wait_for(&g_echoes, 18, 10000000), "all 17 come back (%d)", g_echoes - 1);
    int at = -1;
    for (int i = 1; i < 18 && i < 32; i++)
        if (g_echo_kind[i] == INK_RT_VOICE_UP) at = i - 1;
    CHECK(at >= 0 && at <= 1, "the voice frame queued last left first (position %d of 17)", at);

    /* background jobs wait while a session is open */
    g_steps = 0;
    ink_job_t j = {.name = "three", .step = three_steps, .finish = job_finished};
    int done = g_finished;
    CHECK(ink_job_submit(&j) > 0, "a job is queued during a session");
    osal_task_delay_us(200000);
    CHECK(g_steps == 0, "it waits while the session is open");
    ink_rt_close(sid);
    CHECK(ink_rt_open_count() == 0 && wait_for(&g_finished, done + 1, 2000000) && g_steps == 3, "closed, the job runs");
    CHECK(ink_rt_send(sid, INK_RT_VOICE_UP, voice, sizeof voice, 0) != 0, "nothing goes out on a closed session");
}

/* ---- step 7: the LZ4 decoder of the remote screen (detailed design §8.4) ---- */

static void lz4_checks(void) {
    static uint8_t src[4096], enc[4400], dec[4096];
    static uint32_t work[INK_LZ4_WORK / 4];
    for (int i = 0; i < (int)sizeof src; i++) src[i] = (uint8_t)((i % 97) < 60 ? 0xFF : (i * 7) & 0x3F); /* text-like runs */
    int n = ink_lz4_encode(src, sizeof src, enc, sizeof enc, work);
    CHECK(n > 0 && n < (int)sizeof src / 2, "4 KiB of text-like gray compresses (%d bytes)", n);
    CHECK(ink_lz4_decode(enc, (size_t)n, dec, sizeof dec) == (int)sizeof src && !memcmp(src, dec, sizeof src), "and decodes back unchanged");
    CHECK(ink_lz4_decode(enc, (size_t)n - 3, dec, sizeof dec) < 0, "a truncated block is refused");
    CHECK(ink_lz4_decode(enc, (size_t)n, dec, sizeof dec - 1) < 0, "an output buffer one byte short is refused, not overrun");
    static const uint8_t far[] = {0x1F, 'a', 0x10, 0x00, 0x00}; /* a match 16 bytes back after 1 byte of output */
    CHECK(ink_lz4_decode(far, sizeof far, dec, sizeof dec) < 0, "a match reaching before the start is refused");
    static const uint8_t zero[] = {0x1F, 'a', 0x00, 0x00, 0x00};
    CHECK(ink_lz4_decode(zero, sizeof zero, dec, sizeof dec) < 0, "a zero offset is refused");
}

/* ---- step 8: the hub's directory (detailed design §9) ---- */

static void dir_checks(void) {
    for (int w = 0; w < 100 && !ink_cap_available("tts.speak"); w++) osal_task_delay_us(100000);
    CHECK(ink_cap_available("tts.speak") && ink_cap_available("hub/echo") && ink_cap_available("browser.session"),
          "the hub's server capabilities are available while it is reachable");
    CHECK(!ink_cap_available("hub/nope") && !ink_cap_available("nope"), "what neither has is not");
    ink_args_t a;
    ink_args_init(&a);
    ink_args_str(&a, "q", "hi");
    ink_call_opts_t o = {.src = INK_SRC_APP, .reply_exec = INK_EXEC_DIRECT, .done = bus_done};
    int before = g_bus_done;
    CHECK(ink_cap_call("echo", &a, &o) == 0 && wait_done(before + 1, 15000000) && g_bus_rc.status == INK_RC_DONE && strstr(g_bus_rc.say, "\"q\":\"hi\""),
          "a capability only the hub has is called by its bare name (%s)", g_bus_rc.say);
}

/* Children: last, a child cannot switch back to an adult without the PIN. */
static void child_checks(void) {
    ink_receipt_t rc;
    CHECK(ink_member_switch("kid") == 0, "switch to the child");
    CHECK(ink_cap_invoke("test.system", NULL, INK_SRC_TOUCH, 0, &rc) == INK_RC_REJECTED, "children never run system capabilities");
    CHECK(ink_cap_invoke("test.grown", NULL, INK_SRC_TOUCH, 0, &rc) == INK_RC_REJECTED, "nor ones off their whitelist");
    CHECK(ink_cap_invoke("test.read", NULL, INK_SRC_TOUCH, 0, &rc) == INK_RC_DONE, "child_ok ones run");
    static char buf[8192];
    CHECK(ink_cap_manifest_json(buf, sizeof buf) > 0 && !strstr(buf, "test.system") && !strstr(buf, "test.grown") && strstr(buf, "test.read"),
          "the child's tool list leaves them out");
    CHECK(ink_setting_set("test.mode", 1, INK_SRC_TOUCH, &rc) == INK_RC_REJECTED, "nor settings children may not change (%s)", ink_rc_name(rc.status));
    CHECK(ink_setting_set("test.vol", 5, INK_SRC_TOUCH, &rc) == INK_RC_DONE, "but the ones they may");
}

static void systest_task(void *arg) {
    (void)arg;
    if (ink_display_init() != 0 || ink_storage_init() != 0 || ink_cap_init() != 0 || ink_exec_init() != 0 || ink_res_init() != 0 || ink_storage_reclaimable() != 0 || ink_state_init() != 0 ||
        ink_settings_init() != 0 || ink_power_init() != 0 || ink_net_init() != 0 || ink_light_init() != 0 ||
        ink_hub_init("wss://hub.local/v1/device") != 0 || ink_rt_init() != 0 || ink_tools_init() != 0 || ink_audio_init() != 0 || ink_camera_init() != 0 || ink_comm_init() != 0) {
        CHECK(0, "the system services start");
        return;
    }
    CHECK(ink_cap_subscribe("message.", on_message, NULL, INK_EXEC_DIRECT) == 0, "subscribe to message events");
    storage_checks();
    audio_checks();
    cap_checks();
    state_checks();
    settings_checks();
    bus_checks();
    ink_members_init();
    service_checks();
    res_checks();
    rt_checks();
    lz4_checks();
    dir_checks();
    child_checks();
    ink_probe_mark("systest-done");
}

void ink_main(void) {
    osal_task_cfg_t cfg = {.name = "systest", .fn = systest_task, .stack_bytes = 8192, .priority = OSAL_PRIO_NORMAL, .core = -1};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "create the system test task");
}
