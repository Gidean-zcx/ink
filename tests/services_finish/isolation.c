#include "product.h"
#ifndef INK_TEST_AI_SOURCE
#define INK_TEST_AI_SOURCE "../../system/services/ai/ai_service.c"
#endif
#include INK_TEST_AI_SOURCE
static osal_sem_t *ui_in, *ui_go;
static ink_exec_job_t held;
static int started;
static void focus_and_start(void) {
    ink_text_cfg_t cfg = {.type=INK_TEXT_PLAIN, .initial="", .max=1024};
    CHECK(ink_text_focus(&cfg, NULL, NULL) > 0, "text focus");
    started = ink_text_voice_start();
}
static void press(void) { ink_ai_key(true, osal_now_us()); }
static void busy(void *arg) { (void)arg; osal_sem_give(ui_in); osal_sem_take(ui_go, OSAL_WAIT_FOREVER); }
static void hold_ui(void) {
    ink_exec_post(INK_EXEC_UI, &held, busy, NULL);
    CHECK(osal_sem_take(ui_in, 2000000) == OSAL_OK, "UI held");
}
static int sent(void) {
    osal_mutex_lock(AI.lock, OSAL_WAIT_FOREVER);
    int n = AI.frames_sent;
    osal_mutex_unlock(AI.lock);
    return n;
}
static void run(void *arg) {
    (void)arg; delay_ms(1800); ink_hub_demand();
    CHECK(WAIT_FOR(ink_hub_online() && ink_cap_available("asr.stream"), 20000), "ASR available");
    sim_hub_asr_script("SYNTHETIC_OWNER_WORDS", NULL, 0);
    on_ui(focus_and_start);
    CHECK(started == 0, "dictation starts");
    CHECK(WAIT_FOR(sim_metric_value("hub.asr_frames") > 10, 5000), "real dictation uplink");
    hold_ui();
    CHECK(ink_member_switch("kid") == 0, "dictation handoff");
    delay_ms(600); /* Let packets accepted before the handoff drain. */
    double before = sim_metric_value("hub.asr_frames");
    delay_ms(1600);
    CHECK(!ink_audio_capture_busy() && sim_metric_value("hub.asr_frames") == before,
          "S6: UI congestion cannot capture or upload the next member's dictation");
    osal_sem_give(ui_go);
    delay_ms(300);
    CHECK(ink_member_switch("owner") == 0, "back to owner");
    delay_ms(300);
    ink_hub_demand();
    CHECK(WAIT_FOR(ink_hub_online() && ink_cap_available("voice.chat"), 20000), "voice chat available");
    on_ui(press);
    CHECK(WAIT_FOR(sent() > 10, 4000), "real AI uplink");
    hold_ui();
    CHECK(ink_member_switch("kid") == 0, "AI handoff");
    uint8_t fake_opus = 0;
    on_rt_frame(NULL, AI.stream, INK_RT_VOICE_DOWN, &fake_opus, 1, 0, 0, 0);
    CHECK(AI.tts_stream == 0, "S6: late old-member TTS cannot reopen the speaker");
    delay_ms(600);
    int frames = sent();
    delay_ms(1600);
    CHECK(!ink_audio_capture_busy() && sent() == frames,
          "S6: UI congestion cannot capture or upload the next member's AI speech");
    osal_sem_give(ui_go);
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    ui_in = osal_sem_create(0, 1); ui_go = osal_sem_create(0, 1);
    osal_task_cfg_t cfg = {.name="finish-isolation", .fn=run, .stack_bytes=16384, .priority=OSAL_PRIO_NORMAL, .core=-1, .flags=OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "test task");
}
