/*
 * The interruption service unchanged, with one thing added: a moment, just before it tells the audio service, where a
 * test can have something else happen (tests/integration/races.c).
 */
#include "ink/sys/audio.h"

extern void races_audio_ring(bool on);
#define ink_audio_ring races_audio_ring
#include "../../system/services/notify/interrupt.c"
#undef ink_audio_ring

int races_interrupt_top(void) {
    ink_svc_lock();
    const ink_interrupt_t *top = top_locked();
    int id = top ? top->id : 0;
    ink_svc_unlock();
    return id;
}
