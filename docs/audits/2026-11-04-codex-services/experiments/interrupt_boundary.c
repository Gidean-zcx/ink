/* Keep all table/state transitions unchanged; schedule a peer before the audio call. */
#include "ink/sys/audio.h"
extern void audit_audio_ring(bool on);
#define ink_audio_ring audit_audio_ring
#include "../../../../system/services/notify/interrupt.c"
#undef ink_audio_ring
int audit_interrupt_top_id(void) {
    ink_svc_lock();
    const ink_interrupt_t *top = top_locked();
    int id = top ? top->id : 0;
    ink_svc_unlock();
    return id;
}
