/* Long speech keeps its turn and can still be stopped by the usual inputs. */
#include <stdlib.h>
#include "ink/sys/ai.h"
#include "ink/sys/audio.h"
#include "ink/ui_shell.h"
#define ink_main product_main
#include "../../system/boot/main.c"
#undef ink_main
#include "../integration/whole_product.h"

static char mode[24], snapshot[8192];
static bool speaking(void) {
    char state[16] = "";
    ink_state_get_str(ink_state_find("audio.now_playing"), state, sizeof state);
    return !strcmp(state, "voice");
}
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void read_mode(void *ctx) {
    (void)ctx;
    const char *s = getenv("INK_SPEECH_CASE");
    snprintf(mode, sizeof mode, "%s", s ? s : "stop");
}
static void read_snapshot(void) { CHECK(ink_ui_snapshot(snapshot, sizeof snapshot, NULL) > 0, "AI snapshot fits"); }
static void ask_voice(void) {
    CHECK(BY_HAND("ai.panel.listen", NULL) && g_rc.status == INK_RC_DONE, "tap starts real capture");
    CHECK(WAIT_FOR(ink_audio_capture_busy(), 3000), "capture is on");
    delay_ms(300);
    CHECK(BY_HAND("ai.panel.listen", NULL) && g_rc.status == INK_RC_DONE, "tap ends the question");
}
static void verify_stopped(void) {
    CHECK(WAIT_FOR(!speaking(), 300), "speech stops within 300 ms");
    delay_ms(500);  /* Drain the already submitted codec frame before measuring silence. */
    double heard = sim_audio_played_ms();
    delay_ms(1500);
    CHECK(!speaking() && sim_audio_played_ms() - heard < 100, "late frames do not restart old speech");
}
static void run(void *ctx) {
    (void)ctx;
    vtk_host_call(read_mode, NULL);
    delay_ms(1800);
    CHECK(WAIT_FOR(settled(), 12000), "startup settles");
    test_start_home();
    ink_power_hold(INK_NEED_AWAKE); /* The observer must survive the turn releasing its power hold. */
    uint64_t started = osal_now_us();
    ask_voice();
    if (!strcmp(mode, "silent")) {
        CHECK(WAIT_FOR(sim_metric_value("ai.failures") == 1, 48000), "no first frame times out");
        double elapsed = (double)(osal_now_us() - started) / 1000000.0;
        CHECK(elapsed >= 44 && elapsed <= 47, "first-frame wait is 45 s (%.2f)", elapsed);
        CHECK(!speaking() && !ink_audio_capture_busy(), "silent turn releases capture and speech");
        on_ui(read_snapshot);
        CHECK(strstr(snapshot, "AiTimeout"), "silent turn exposes timeout page to AI");
    } else {
        CHECK(WAIT_FOR(speaking(), 20000), "real downlink starts");
        double heard = sim_audio_played_ms();
        if (!strcmp(mode, "gap")) {
            CHECK(WAIT_FOR(sim_metric_value("ai.failures") == 1, 20000), "no more frames ends the turn");
            double elapsed = (double)(osal_now_us() - started) / 1000000.0;
            CHECK(elapsed >= 10 && elapsed < 18, "frame activity replaces 45 s with 10 s (%.2f)", elapsed);
        } else if (!strcmp(mode, "limit")) {
            CHECK(WAIT_FOR(sim_metric_value("ai.failures") == 1, 610000), "continuous frames respect total limit");
            double elapsed = (double)(osal_now_us() - started) / 1000000.0;
            CHECK(elapsed >= 599 && elapsed <= 602, "total limit is ten minutes (%.2f)", elapsed);
            verify_stopped();
        } else {
            bool half = WAIT_FOR(sim_audio_played_ms() - heard >= 220000, 240000);
            CHECK(half && speaking() && sim_metric_value("ai.failures") == 0, "still reading halfway through 6 KB at normal speed");
            if (half && speaking()) {
                on_ui(read_snapshot);
                CHECK(strstr(snapshot, "AiSpeak") && strstr(snapshot, "页码=1/"), "speech stays on its first answer page");
                if (!strcmp(mode, "stop")) {
                    CHECK(BY_HAND("shell.ai_stop", NULL) && g_rc.status == INK_RC_DONE, "stop uses the existing capability");
                    verify_stopped();
                    on_ui(read_snapshot);
                    CHECK(strstr(snapshot, "第1段"), "stopping preserves the answer");
                } else if (!strcmp(mode, "barge")) {
                    double presses = sim_metric_value("ai.presses");
                    hw(HW_KEY_DOWN, HAL_KEY_AI, 0);
                    CHECK(WAIT_FOR(sim_metric_value("ai.presses") > presses && ink_audio_capture_busy(), 1000), "AI key reaches driver and opens new capture");
                    /* the stream stops in the key handler; audio.now_playing is published by the audio task when it
                     * next runs (one frame), so a capture that opens first can still see "voice" for that moment
                     * (Linux CI, after a board settle moved the timeline) */
                    CHECK(WAIT_FOR(!speaking(), 100), "AI key interrupts long speech at once (within 100 ms)");
                    hw(HW_KEY_UP, HAL_KEY_AI, 0);
                    CHECK(WAIT_FOR(sim_metric_value("ai.answers") == 1 && !speaking(), 15000), "replacement question finishes");
                    on_ui(read_snapshot);
                    CHECK(strstr(snapshot, "新的合成回答") && !strstr(snapshot, "第1段"), "only new question's answer remains");
                } else if (!strcmp(mode, "member")) {
                    CHECK(ink_member_switch("kid") == 0, "switch member during long speech");
                    verify_stopped();
                    on_ui(read_snapshot);
                    CHECK(!strstr(snapshot, "第1段"), "old member's answer is gone");
                } else if (!strcmp(mode, "call")) {
                    snprintf(g_hw.num, sizeof g_hw.num, "%s", "00000000002");
                    hw(HW_CALL, 0, 0);
                    CHECK(WAIT_FOR(!strcmp(active_state(), "call"), 1000), "incoming call reaches real interruption service");
                    CHECK(WAIT_FOR(!speaking(), 300), "call stops speech within 300 ms");
                    delay_ms(1500);
                    CHECK(!speaking(), "late frames do not replace the call's audio");
                }
            }
        }
    }
    CHECK(sim_metric_value("ai.downlink_overflow") == 0 && sim_metric_value("audio.jitter_overflow") == 0, "512-frame buffer never overflows");
    ink_probe_value("test.elapsed_s", (double)(osal_now_us() - started) / 1000000.0);
    ink_probe_count("test.finished");
    ink_power_release(INK_NEED_AWAKE);
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name = "ai-speech-test", .fn = run, .stack_bytes = 12288, .priority = OSAL_PRIO_NORMAL,
        .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start speech scenario driver");
}
