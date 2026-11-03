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
#include "ink/sys/cap.h"
#include "ink/sys/display.h"
#include "ink/sys/json.h"
#include "cJSON.h"
#include "ink/sys/members.h"
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
          "arguments survive a round trip through JSON (%s)", buf);

    /* the tool list is generated from the table */
    n = ink_cap_manifest_json(buf, sizeof buf);
    cJSON *list = n > 0 ? cJSON_Parse(buf) : NULL; /* the whole list: more tokens than the device parser takes */
    CHECK(cJSON_IsArray(list) && cJSON_GetArraySize(list) == 8, "the tool list is a JSON array of the 8 offered to the AI (%d)",
          cJSON_GetArraySize(list));
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
    CHECK(ink_state_json(buf, sizeof buf, 0) > 0 && strstr(buf, "\"test.level\":6") && strstr(buf, "\"test.name\":\"abc\""), "the snapshot (%s)", buf);
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
    CHECK(n > 0 && strstr(buf, "\"test.vol\":7") && strstr(buf, "\"test.mode\":2"), "the values are saved (%s)", buf);
    for (int v = 1; v <= 3; v++) ink_setting_set("test.vol", v, INK_SRC_TOUCH, &rc);
    osal_task_delay_us(1200000);
    drain_storage();
    n = ink_storage_read(INK_SETTINGS_PATH, buf, sizeof buf - 1);
    buf[n > 0 ? n : 0] = 0;
    CHECK(strstr(buf, "\"test.vol\":3"), "a burst of changes is saved once, with the last value (%s)", buf);

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

/* Children: last, a child cannot switch back to an adult without the PIN. */
static void child_checks(void) {
    ink_receipt_t rc;
    CHECK(ink_member_switch("kid") == 0, "switch to the child");
    CHECK(ink_cap_invoke("test.system", NULL, INK_SRC_TOUCH, 0, &rc) == INK_RC_REJECTED, "children never run system capabilities");
    CHECK(ink_cap_invoke("test.grown", NULL, INK_SRC_TOUCH, 0, &rc) == INK_RC_REJECTED, "nor ones off their whitelist");
    CHECK(ink_cap_invoke("test.read", NULL, INK_SRC_TOUCH, 0, &rc) == INK_RC_DONE, "child_ok ones run");
    static char buf[2048];
    CHECK(ink_cap_manifest_json(buf, sizeof buf) > 0 && !strstr(buf, "test.system") && !strstr(buf, "test.grown") && strstr(buf, "test.read"),
          "the child's tool list leaves them out");
    CHECK(ink_setting_set("test.mode", 1, INK_SRC_TOUCH, &rc) == INK_RC_REJECTED, "nor settings children may not change (%s)", ink_rc_name(rc.status));
    CHECK(ink_setting_set("test.vol", 5, INK_SRC_TOUCH, &rc) == INK_RC_DONE, "but the ones they may");
}

static void systest_task(void *arg) {
    (void)arg;
    if (ink_display_init() != 0 || ink_storage_init() != 0 || ink_state_init() != 0 || ink_settings_init() != 0 || ink_power_init() != 0 ||
        ink_audio_init() != 0) {
        CHECK(0, "the display, storage, state, settings, power and audio services start");
        return;
    }
    storage_checks();
    audio_checks();
    cap_checks();
    state_checks();
    settings_checks();
    child_checks();
    ink_probe_mark("systest-done");
}

void ink_main(void) {
    osal_task_cfg_t cfg = {.name = "systest", .fn = systest_task, .stack_bytes = 6144, .priority = OSAL_PRIO_NORMAL, .core = -1};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "create the system test task");
}
