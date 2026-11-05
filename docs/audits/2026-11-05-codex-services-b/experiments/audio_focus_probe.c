#include "product_probe.h"
#include "ink/sys/interrupt.h"
static void audit_task(void *arg) {
    (void)arg; delay_ms(1800);
    ink_interrupt_t a={.kind=INK_INT_ALARM,.ringing=true};
    snprintf(a.title,sizeof a.title,"audit alarm");
    int alarm=ink_interrupt_raise(&a);
    delay_ms(500);
    CHECK(ink_audio_focus_top()==INK_AUDIO_PRIO_ALERT,"alarm owns focus");
    int call=ink_audio_focus_acquire(INK_AUDIO_PRIO_CALL,NULL,NULL,INK_EXEC_DIRECT);
    ink_interrupt_t c={.kind=INK_INT_CALL,.ringing=true};
    int incoming=ink_interrupt_raise(&c);
    delay_ms(500);
    ink_audio_focus_release(call); /* same order as comm_service on CALL_ENDED */
    ink_interrupt_end(incoming);
    delay_ms(500);
    double before=sim_audio_played_ms();
    delay_ms(500);
    printf("AUDIT after_call interrupt=%s focus=%d ring_played_ms=%.0f\n",active_state(),ink_audio_focus_top(),sim_audio_played_ms()-before);
    CHECK(ink_audio_focus_top()==0 && !strcmp(active_state(),"alarm"),"reproduced: remaining alarm lost its focus");
    CHECK(BY_HAND("audio.record","{\"path\":\"/me/under-alarm.opus\"}") && g_rc.status==INK_RC_DONE,"reproduced: recording starts while alarm still rings");
    printf("AUDIT record_under_alarm status=%s recording=%d\n",ink_rc_name(g_rc.status),ink_audio_recording());
    BY_HAND("audio.record_stop",NULL); delay_ms(300);
    ink_interrupt_end(alarm);
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t c={.name="audit-focus",.fn=audit_task,.stack_bytes=16384,.priority=OSAL_PRIO_NORMAL,.core=-1,.flags=OSAL_TASK_STACK_EXTERNAL};
    osal_task_create(&c,NULL);
}
