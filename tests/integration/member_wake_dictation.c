/* Inspect the real dictation buffer only after capture ended, on the UI task. */
#ifndef INK_TEST_VOICE_SOURCE
#define INK_TEST_VOICE_SOURCE "../../system/services/text/voice_input.c"
#endif
#include INK_TEST_VOICE_SOURCE

int member_wake_dictation_buffered(void) { return VS.buffered; }
bool member_wake_dictation_revoked(void) { return __atomic_load_n(&VS.revoked, __ATOMIC_ACQUIRE); }
