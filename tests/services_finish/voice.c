#include "product.h"
static osal_sem_t *ui_in, *ui_go;
static ink_exec_job_t held;
static int started;
static void focus_and_start(void) {
    ink_text_cfg_t cfg = {.type=INK_TEXT_PLAIN, .initial="", .max=1024};
    CHECK(ink_text_focus(&cfg, NULL, NULL) > 0, "text focus");
    started = ink_text_voice_start();
}
static void busy(void *arg) { (void)arg; osal_sem_give(ui_in); osal_sem_take(ui_go, OSAL_WAIT_FOREVER); }
static void run(void *arg) {
    (void)arg; delay_ms(1800); ink_hub_demand();
    CHECK(WAIT_FOR(ink_hub_online() && ink_cap_available("asr.stream"), 20000), "ASR available");
    sim_hub_asr_script("test", NULL, 0);
    on_ui(focus_and_start);
    CHECK(started == 0, "dictation starts");
    CHECK(WAIT_FOR(sim_metric_value("hub.asr_frames") > 10, 5000), "real uplink");
    ink_exec_post(INK_EXEC_UI, &held, busy, NULL);
    CHECK(osal_sem_take(ui_in, 2000000) == OSAL_OK, "UI held");
    int call = ink_audio_focus_acquire(INK_AUDIO_PRIO_CALL, NULL, NULL, INK_EXEC_DIRECT);
    CHECK(call > 0, "call focus");
    delay_ms(600); /* Network packets accepted before the cut drain first. */
    double before = sim_metric_value("hub.asr_frames");
    delay_ms(1600);
    CHECK(!ink_audio_capture_busy() && sim_metric_value("hub.asr_frames") == before, "AUD18-201: UI congestion cannot keep dictation collecting or uploading");
    osal_sem_give(ui_go);
    ink_audio_focus_release(call);
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    ui_in = osal_sem_create(0, 1); ui_go = osal_sem_create(0, 1);
    osal_task_cfg_t cfg = {.name="finish-voice", .fn=run, .stack_bytes=16384, .priority=OSAL_PRIO_NORMAL, .core=-1, .flags=OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "test task");
}
