#include "product_probe.h"
#include "ink/sys/textinput.h"
static osal_sem_t *ui_in, *ui_go;
static ink_exec_job_t held;
static int started;
static void focus_and_start(void) {
    ink_text_cfg_t c = {.type=INK_TEXT_PLAIN, .initial="", .max=1024};
    CHECK(ink_text_focus(&c, NULL, NULL) > 0, "text focused");
    started = ink_text_voice_start();
}
static void hold_ui(void *arg) { (void)arg; osal_sem_give(ui_in); osal_sem_take(ui_go, OSAL_WAIT_FOREVER); }
static void speech(void *arg) { (void)arg; sim_audio_mic_source("speech"); }
static void audit_task(void *arg) {
    (void)arg; delay_ms(1800); ink_hub_demand();
    CHECK(WAIT_FOR(ink_hub_online() && ink_cap_available("asr.stream"), 20000), "ASR available");
    sim_hub_asr_script("audit", NULL, 0);
    vtk_event_at(vtk_now()+SIM_MS(1), speech, NULL);
    on_ui(focus_and_start);
    CHECK(started == 0, "start");
    CHECK(WAIT_FOR(sim_metric_value("hub.asr_frames") > 10, 5000), "real uplink");
    ink_exec_post(INK_EXEC_UI, &held, hold_ui, NULL);
    CHECK(osal_sem_take(ui_in, 2000000) == OSAL_OK, "UI held at boundary");
    int f = ink_audio_focus_acquire(INK_AUDIO_PRIO_CALL, NULL, NULL, INK_EXEC_DIRECT);
    CHECK(f > 0, "call owns focus");
    delay_ms(600); /* drain frames which preceded the focus change */
    double before = sim_metric_value("hub.asr_frames");
    delay_ms(1600);
    double after = sim_metric_value("hub.asr_frames");
    printf("AUDIT call_focus=%d capture_busy=%d asr_frames_after_grace=%.0f\n", ink_audio_focus_top(), ink_audio_capture_busy(), after-before);
    CHECK(ink_audio_capture_busy() && after > before + 10, "reproduce capture and upload while call owns focus");
    osal_sem_give(ui_go);
    CHECK(WAIT_FOR(!ink_audio_capture_busy(), 2000), "UI callback eventually stops it");
    ink_audio_focus_release(f);
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main(); ui_in=osal_sem_create(0,1); ui_go=osal_sem_create(0,1);
    osal_task_cfg_t c = {.name="audit-voice", .fn=audit_task, .stack_bytes=16384, .priority=OSAL_PRIO_NORMAL, .core=-1, .flags=OSAL_TASK_STACK_EXTERNAL};
    osal_task_create(&c,NULL);
}
