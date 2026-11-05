#include "product.h"
#ifndef INK_TEST_AUDIO_SOURCE
#define INK_TEST_AUDIO_SOURCE "../../system/services/audio/audio_service.c"
#endif
#include INK_TEST_AUDIO_SOURCE
static osal_sem_t *in, *go;
static ink_exec_job_t held;
static volatile int done;
static int64_t recorded_ms;
static void rec_done(void *ctx, const char *id, const ink_args_t *a) {
    (void)ctx; (void)id;
    ink_args_get_int(a, "ms", &recorded_ms);
    done++;
}
static void busy(void *arg) { (void)arg; osal_sem_give(in); osal_sem_take(go, OSAL_WAIT_FOREVER); }
static void add_headset(void *arg) { (void)arg; sim_headset_add("test-headset", "test"); }
static void lose_headset(void *arg) { (void)arg; sim_headset_range("test-headset", false); }
static void run(void *arg) {
    (void)arg;
    delay_ms(1800);
    ink_cap_subscribe("audio.record_done", rec_done, NULL, INK_EXEC_DIRECT);
    CHECK(BY_HAND("audio.record", "{\"path\":\"/me/cut.opus\"}") && g_rc.status == INK_RC_DONE, "record starts");
    delay_ms(1200);
    ink_exec_post(INK_EXEC_MEDIA, &held, busy, NULL);
    CHECK(osal_sem_take(in, 2000000) == OSAL_OK, "writer busy");
    int call = ink_audio_focus_acquire(INK_AUDIO_PRIO_CALL, NULL, NULL, INK_EXEC_DIRECT);
    CHECK(call > 0, "call focus");
    delay_ms(2000);
    CHECK(!ink_audio_capture_busy(), "AUD18-201: capture ends while writer busy");
    osal_sem_give(go);
    CHECK(WAIT_FOR(done == 1, 3000), "record ends");
    CHECK(recorded_ms <= 1500, "AUD18-201: no call samples (%lld ms)", (long long)recorded_ms);
    ink_audio_focus_release(call);
    /* ALERT remains owned when a call temporarily covers it. */
    ink_audio_ring(true);
    call = ink_audio_focus_acquire(INK_AUDIO_PRIO_CALL, NULL, NULL, INK_EXEC_DIRECT);
    ink_audio_focus_release(call);
    CHECK(ink_audio_focus_top() == INK_AUDIO_PRIO_ALERT, "AUD18-205: alarm retains focus");
    CHECK(BY_HAND("audio.record", NULL) && g_rc.status == INK_RC_FAILED, "alarm blocks recording");
    ink_audio_ring(false);
    BY_HAND("audio.record_stop", NULL);
    CHECK(WAIT_FOR(!ink_audio_recording(), 2000), "recording cleaned up");
    /* Route loss while the media executor cannot pause yet. */
    vtk_event_at(vtk_now() + SIM_MS(1), add_headset, NULL);
    delay_ms(50);
    CHECK(BY_HAND("bt.pair", "{\"addr\":\"test-headset\"}") && g_rc.status == INK_RC_DONE, "paired");
    CHECK(BY_HAND("bt.connect", "{\"addr\":\"test-headset\"}") && g_rc.status == INK_RC_DONE, "connected");
    CHECK(WAIT_FOR(ink_audio_route() == INK_ROUTE_HEADSET, 2500), "headset route");
    CHECK(BY_HAND("media.play", "{\"path\":\"/me/cut.opus\",\"start_ms\":0}") && g_rc.status == INK_RC_DONE, "play");
    delay_ms(300);
    ink_exec_post(INK_EXEC_MEDIA, &held, busy, NULL);
    CHECK(osal_sem_take(in, 2000000) == OSAL_OK, "writer busy again");
    double speaker = sim_audio_played_ms();
    vtk_event_at(vtk_now() + SIM_MS(1), lose_headset, NULL);
    CHECK(WAIT_FOR(ink_audio_route() == INK_ROUTE_SPEAKER, 2000), "speaker route");
    delay_ms(1000);
    CHECK(sim_audio_played_ms() == speaker, "AUD18-204: no private samples out loud (%.0f)", sim_audio_played_ms() - speaker);
    osal_sem_give(go);
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    in = osal_sem_create(0, 1); go = osal_sem_create(0, 1);
    osal_task_cfg_t cfg = {.name="finish-audio", .fn=run, .stack_bytes=16384, .priority=OSAL_PRIO_NORMAL, .core=-1, .flags=OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "test task");
}
