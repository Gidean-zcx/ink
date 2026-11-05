/* Same bounded slow-writer arrangement as integration-media. No product modification. */
#include "product_probe.h"
#include "../../../../system/services/audio/ogg.h"
static osal_sem_t *in, *go;
static ink_exec_job_t held;
static volatile int done;
static int64_t recorded_ms;
static char reason[32];
static void rec_done(void *ctx, const char *id, const ink_args_t *a) {
    (void)ctx; (void)id;
    ink_args_get_int(a, "ms", &recorded_ms);
    ink_args_get_str(a, "why", reason, sizeof reason);
    done++;
}
static void hold_writer(void *arg) { (void)arg; osal_sem_give(in); osal_sem_take(go, OSAL_WAIT_FOREVER); }
static void audit_task(void *arg) {
    (void)arg; delay_ms(1800);
    ink_cap_subscribe("audio.record_done", rec_done, NULL, INK_EXEC_DIRECT);
    CHECK(BY_HAND("audio.record", "{\"path\":\"/me/call-boundary.opus\"}") && g_rc.status == INK_RC_DONE, "record starts");
    uint64_t start = osal_now_us();
    delay_ms(1200);
    ink_exec_post(INK_EXEC_MEDIA, &held, hold_writer, NULL);
    CHECK(osal_sem_take(in,2000000) == OSAL_OK, "writer temporarily busy");
    uint64_t cut = osal_now_us();
    int call = ink_audio_focus_acquire(INK_AUDIO_PRIO_CALL, NULL, NULL, INK_EXEC_DIRECT);
    CHECK(call > 0, "call has priority");
    delay_ms(2000);
    printf("AUDIT during_call busy=%d recording=%d elapsed_before_call_ms=%llu\n", ink_audio_capture_busy(), ink_audio_recording(), (unsigned long long)((cut-start)/1000));
    CHECK(ink_audio_capture_busy(), "reproduced: microphone still collecting during the call");
    osal_sem_give(go);
    CHECK(WAIT_FOR(done == 1, 3000), "record ends when the writer becomes free");
    printf("AUDIT call_recorded_ms=%lld expected_cut_by_ms=%llu reason=%s\n", (long long)recorded_ms, (unsigned long long)((cut-start)/1000+180), reason);
    CHECK(recorded_ms > (int64_t)((cut-start)/1000)+1000, "reproduced: post-interruption audio persisted");
    ogg_reader_t *r = NULL; int err = 0;
    CHECK(ogg_open("/me/call-boundary.opus", &r, &err) == OGG_OK, "the result is playable");
    if (r) { printf("AUDIT file_duration_ms=%u\n", ogg_duration_ms(r)); ogg_close(r); }
    ink_audio_focus_release(call);
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main(); in=osal_sem_create(0,1); go=osal_sem_create(0,1);
    osal_task_cfg_t c={.name="audit-rec-call",.fn=audit_task,.stack_bytes=16384,.priority=OSAL_PRIO_NORMAL,.core=-1,.flags=OSAL_TASK_STACK_EXTERNAL};
    osal_task_create(&c,NULL);
}
