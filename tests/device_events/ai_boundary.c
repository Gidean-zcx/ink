#include "ink/sys/audio.h"
static bool fail_push;
static int fixture_push(int id, const uint8_t *opus, size_t len) {
    return fail_push ? -1 : ink_audio_stream_push(id, opus, len);
}
#define ink_audio_stream_push fixture_push
#ifndef INK_TEST_AI_SOURCE
#define INK_TEST_AI_SOURCE "../../system/services/ai/ai_service.c"
#endif
#include INK_TEST_AI_SOURCE
#undef ink_audio_stream_push
#include "../integration/whole_product.h"

void test_ai_downlink(void) {
    osal_mutex_lock(AI.lock, OSAL_WAIT_FOREVER);
    g_turn_revoked = false;
    snprintf(g_turn_member, sizeof g_turn_member, "%s", ink_member_current()->id);
    g_turn_generation = ink_member_generation(g_turn_member);
    AI.stream = 201; AI.tts_done_for = -1; AI.tts_stream = 0;
    osal_mutex_unlock(AI.lock);
    fail_push = true;
    const uint8_t packet[] = {0x00};
    on_rt_frame(NULL, 201, INK_RT_VOICE_DOWN, packet, sizeof packet, 0, 0, 0);
    fail_push = false;
    CHECK(AI.tts_stream == 0 && AI.tts_done_for == 201 && sim_metric_value("ai.downlink_overflow") == 1,
          "downlink refusal aborts instead of silently losing speech");
}

void test_ai_begin(void) {
    ink_ai_key(true, osal_now_us());
    CHECK(AI.capturing, "capture started");
}
void test_ai_limit(void) {
    CHECK(!AI.capturing && sim_metric_value("ai.capture_limit") == 1, "capture stops at 30 seconds without releasing the key");
    ink_ai_dismiss();
}
