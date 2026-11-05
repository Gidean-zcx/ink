#include "product.h"

#ifndef INK_TEST_AI_SOURCE
#define INK_TEST_AI_SOURCE "../../system/services/ai/ai_service.c"
#endif
#include INK_TEST_AI_SOURCE
static osal_sem_t *review_in, *review_go;
static ink_exec_job_t review_hold;
static ink_exec_job_t media_hold;
static osal_sem_t *media_in, *media_go;
static void review_busy(void *arg) {
    (void)arg; osal_sem_give(review_in); osal_sem_take(review_go, OSAL_WAIT_FOREVER);
}
static void review_ask(void) { ink_ai_ask_text("SYNTHETIC_OWNER_REQUEST"); }
static void media_busy(void *arg) {
    (void)arg; osal_sem_give(media_in); osal_sem_take(media_go, OSAL_WAIT_FOREVER);
}
static void review_run(void *arg) {
    (void)arg; delay_ms(1800);
    on_ui(review_ask);
    review_in = osal_sem_create(0, 1); review_go = osal_sem_create(0, 1);
    ink_exec_post(INK_EXEC_UI, &review_hold, review_busy, NULL);
    CHECK(osal_sem_take(review_in, 2000000) == OSAL_OK, "UI held");
    uint8_t packet[160];
    int n = sim_opus_fake_packet(packet, sizeof packet, 440, 0.2, 120, 1);
    for (int i=0; i<100; i++) on_rt_frame(NULL, AI.stream, INK_RT_VOICE_DOWN, packet, (size_t)n, 0, 0, 0);
    CHECK(WAIT_FOR(sim_audio_played_ms() > 200, 2000), "owner TTS is playing");
    if (getenv("INK_REVIEW_CONTROL")) ink_audio_stream_abort(AI.tts_stream);
    CHECK(ink_member_switch("kid") == 0, "switch to child while UI busy");
    delay_ms(200);
    double before = sim_audio_played_ms();
    delay_ms(1000);
    double extra = sim_audio_played_ms() - before;
    ink_probe_value("review.private_audio_after_handoff_ms", extra);
    CHECK(extra == 0, "owner TTS continues after handoff: %.0f ms", extra);
    osal_sem_give(review_go);
    delay_ms(300);
    CHECK(ink_member_switch("owner") == 0, "back to owner");
    delay_ms(300);
    media_in = osal_sem_create(0, 1); media_go = osal_sem_create(0, 1);
    ink_exec_post(INK_EXEC_MEDIA, &media_hold, media_busy, NULL);
    CHECK(osal_sem_take(media_in, 2000000) == OSAL_OK, "media executor held");
    int stream = ink_audio_stream_open(INK_AUDIO_PRIO_STORY, 0, NULL);
    CHECK(stream > 0, "private media stream");
    for (int i = 0; i < 80; i++) ink_audio_stream_push(stream, packet, (size_t)n);
    before = sim_audio_played_ms();
    CHECK(WAIT_FOR(sim_audio_played_ms() > before + 200, 2000), "private media is buffered and playing");
    CHECK(ink_member_switch("kid") == 0, "media handoff while its executor is busy");
    delay_ms(200);
    before = sim_audio_played_ms();
    delay_ms(1000);
    CHECK(sim_audio_played_ms() == before, "buffered private media cannot cross the handoff");
    CHECK(ink_audio_stream_push(stream, packet, (size_t)n) != 0, "old media stream stays invalid");
    osal_sem_give(media_go);
    ink_probe_count("test.finished");
}

void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name="review-regression", .fn=review_run,
        .stack_bytes=16384, .priority=OSAL_PRIO_NORMAL, .core=-1,
        .flags=OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "review task");
}
