#include "product_probe.h"
#include "ink/sys/interrupt.h"
extern int audit_interrupt_top_id(void);
static osal_sem_t *at_boundary,*peer_done;
static bool armed,ring_on;
void audit_audio_ring(bool on) {
    if (armed && on) {
        armed=false; osal_sem_give(at_boundary);
        CHECK(osal_sem_take(peer_done,2000000)==OSAL_OK,"peer ran at the boundary");
    }
    ink_audio_ring(on); ring_on=on;
}
static void peer(void *arg) {
    (void)arg; osal_sem_take(at_boundary,OSAL_WAIT_FOREVER);
    ink_interrupt_end(audit_interrupt_top_id()); osal_sem_give(peer_done);
}
static void audit_task(void *arg) {
    (void)arg; delay_ms(1800); armed=true;
    ink_interrupt_t it={.kind=INK_INT_ALARM,.ringing=true};
    CHECK(ink_interrupt_raise(&it)>0,"raised"); delay_ms(100);
    printf("AUDIT active=%d ring=%d state=%s\n",ink_interrupt_active(),ring_on,active_state());
    CHECK(!ring_on,"the ringing has stopped");
    CHECK(!ink_interrupt_active() && !strcmp(active_state(),"none"),"everything says it is over");
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main(); at_boundary=osal_sem_create(0,1); peer_done=osal_sem_create(0,1);
    osal_task_cfg_t c={.name="audit-interrupt-peer",.fn=peer,.stack_bytes=8192,.priority=OSAL_PRIO_NORMAL,.core=-1,.flags=OSAL_TASK_STACK_EXTERNAL};
    osal_task_create(&c,NULL);
    c.name="audit-interrupt"; c.fn=audit_task; osal_task_create(&c,NULL);
}
