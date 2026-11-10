/* Actual synthetic hub downlink, AI key and modelled speech edges. */
#include <stdlib.h>
#include "ink/sys/ai.h"
#include "ink/sys/audio.h"
#include "ink/sys/storage.h"
#define ink_main product_main
#include "../../system/boot/main.c"
#undef ink_main
#include "../integration/whole_product.h"

static bool test_speaking(void) {
    char value[16] = ""; ink_state_get_str(ink_state_find("audio.now_playing"), value, sizeof value); return !strcmp(value, "voice");
}
static void test_mic_event(void *ctx) { sim_audio_mic_source(ctx); }
static void test_mic(const char *value) { vtk_event_at(vtk_now() + SIM_MS(1), test_mic_event, (void *)value); delay_ms(10); }
static void test_ready(void) {
    /* the setting takes effect in memory at once; its flash write waits while the answer plays (D-36), so do not wait for it */
    CHECK(WAIT_FOR(!ink_ui_busy() && !ink_display_busy(), 10000), "the preceding answer frame settles");
}
static void test_ask(void) {
    double requests = sim_metric_value("hub.requests.ai.listen");
    hw(HW_KEY_DOWN, HAL_KEY_AI, 0);
    CHECK(WAIT_FOR(ink_audio_capture_busy() && sim_metric_value("hub.requests.ai.listen") > requests, 10000), "microphone and actual hub voice request are active");
    hw(HW_KEY_UP, HAL_KEY_AI, 0);
    CHECK(WAIT_FOR(test_speaking(), 20000), "actual hub answer is playing");
}
static void run(void *ctx) {
    (void)ctx;
    CHECK(WAIT_FOR(ink_member_space_open(), 10000), "member space opens"); test_start_home();
    ink_hub_demand();
    CHECK(WAIT_FOR(ink_hub_online() && !ink_ui_busy() && !ink_display_busy(), 20000), "voice transport and startup display are ready");
    CHECK(ink_setting_get("voice.interrupt") == 1, "interrupt defaults on");
    test_mic("silence"); test_ask();
    CHECK(AI("settings.set", "{\"id\":\"voice.interrupt\",\"value\":0}") && g_rc.status == INK_RC_DONE, "disable interrupts through shared capability");
    test_ready();
    double presses = sim_metric_value("ai.presses"), barges = sim_metric_value("ai.barge_ins"), heard = sim_audio_played_ms();
    double delivered = sim_metric_value("input.key_delivery_ms");
    hw(HW_KEY_DOWN, HAL_KEY_AI, 0); test_mic("speech");
    CHECK(WAIT_FOR(sim_metric_value("input.key_delivery_ms") != delivered, 1500), "disabled AI press really reaches input driver");
    delay_ms(600);
    CHECK(test_speaking() && sim_audio_played_ms() > heard && !ink_audio_capture_busy(), "disabled key and speech leave the answer playing");
    CHECK(sim_metric_value("ai.presses") == presses && sim_metric_value("ai.barge_ins") == barges, "disabled interruption creates no turn or barge count");
    delivered = sim_metric_value("input.key_delivery_ms"); hw(HW_KEY_UP, HAL_KEY_AI, 0); test_mic("silence");
    CHECK(WAIT_FOR(sim_metric_value("input.key_delivery_ms") > delivered, 1500), "disabled press release reaches driver");
    CHECK(AI("settings.set", "{\"id\":\"voice.interrupt\",\"value\":1}") && g_rc.status == INK_RC_DONE, "enable interrupts through shared capability");
    test_ready();
    hw(HW_KEY_DOWN, HAL_KEY_AI, 0);
    CHECK(WAIT_FOR(sim_metric_value("ai.presses") == presses + 1 && ink_audio_capture_busy(), 1500), "enabled key opens the next turn");
    CHECK(WAIT_FOR(!test_speaking(), 300) && sim_metric_value("ai.barge_ins") == barges + 1, "enabled key stops the answer and counts exactly one barge");
    hw(HW_KEY_UP, HAL_KEY_AI, 0);
    CHECK(WAIT_FOR(test_speaking(), 20000), "replacement answer plays");
    CHECK(AI("settings.set", "{\"id\":\"voice.interrupt\",\"value\":0}"), "disable hands-free interruption");
    test_mic("speech"); delay_ms(600);
    CHECK(test_speaking() && sim_metric_value("ai.barge_ins") == barges + 1, "disabled hands-free speech does not barge in");
    test_mic("silence"); delay_ms(800);
    CHECK(AI("settings.set", "{\"id\":\"voice.interrupt\",\"value\":1}"), "enable hands-free interruption");
    test_mic("speech");
    CHECK(WAIT_FOR(sim_metric_value("ai.barge_ins") == barges + 2 && ink_audio_capture_busy(), 1500), "enabled speech edge counts one barge and starts listening");
    CHECK(!test_speaking(), "enabled hands-free speech stops playback"); test_mic("silence");
    /* Pressed while the panel refreshes: the feeders hold the cores, the audio task falls behind on the microphone, and the
     * answer the press stopped must read as stopped at once, not once that backlog is worked off (CI on 5cc0a6eb, Linux) */
    CHECK(WAIT_FOR(test_speaking(), 20000), "the third answer plays");
    CHECK(WAIT_FOR(ink_display_busy(), 5000), "the panel refreshes while it plays");
    delay_ms(60);
    presses = sim_metric_value("ai.presses");
    hw(HW_KEY_DOWN, HAL_KEY_AI, 0);
    CHECK(WAIT_FOR(sim_metric_value("ai.presses") == presses + 1, 1500), "a press during the refresh opens a turn");
    CHECK(WAIT_FOR(!test_speaking(), 150), "the answer it stopped reads as stopped at once");
    hw(HW_KEY_UP, HAL_KEY_AI, 0);
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name = "small-gaps-voice", .fn = run, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start interruption acceptance test");
}
