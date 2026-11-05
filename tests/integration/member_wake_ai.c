/* Inspect the real AI buffer without adding production test hooks. */
#ifndef INK_TEST_AI_SOURCE
#define INK_TEST_AI_SOURCE "../../system/services/ai/ai_service.c"
#endif
#include INK_TEST_AI_SOURCE

int member_wake_ai_buffered(void) {
    osal_mutex_lock(AI.lock, OSAL_WAIT_FOREVER);
    int n = AI.buffered;
    osal_mutex_unlock(AI.lock);
    return n;
}

bool member_wake_ai_revoked(void) {
    osal_mutex_lock(AI.lock, OSAL_WAIT_FOREVER);
    bool revoked = g_turn_revoked;
    osal_mutex_unlock(AI.lock);
    return revoked;
}
