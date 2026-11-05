#include "ink/sys/audio.h"
#include "ink/sys/hub.h"
#include <string.h>
static bool fail_push;
static int stale_status = 100;
static ink_hub_reply_fn stale_reply;
static void *stale_context;
static void stale_told(void *ctx, int status, const ink_json_t *j, int result) {
    (void)ctx;
    stale_status = status;
    stale_reply(stale_context, status, j, result);
}
static int fixture_call_for(const char *method, const char *params, int64_t timeout, ink_hub_reply_fn cb, void *ctx,
                              const char *member, uint32_t epoch) {
    if (strstr(params, "SYNTHETIC_STALE_QUESTION")) {
        stale_reply = cb; stale_context = ctx;
        return ink_hub_call_for(method, params, timeout, stale_told, NULL, member, epoch);
    }
    return ink_hub_call_for(method, params, timeout, cb, ctx, member, epoch);
}
static int fixture_push(int id, const uint8_t *opus, size_t len) {
    return fail_push ? -1 : ink_audio_stream_push(id, opus, len);
}
#define ink_audio_stream_push fixture_push
#define ink_hub_call_for fixture_call_for
#ifndef INK_TEST_AI_SOURCE
#define INK_TEST_AI_SOURCE "../../system/services/ai/ai_service.c"
#endif
#include INK_TEST_AI_SOURCE
#undef ink_audio_stream_push
#undef ink_hub_call_for
#include "../integration/whole_product.h"

void test_ai_downlink(void) {
    osal_mutex_lock(AI.lock, OSAL_WAIT_FOREVER);
    g_turn_revoked = false;
    snprintf(g_turn_member, sizeof g_turn_member, "%s", ink_member_current()->id);
    g_turn_generation = ink_member_generation(g_turn_member);
    g_turn_epoch = ink_hub_member_epoch();
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

static int queued_speech;
void test_ai_queue_speech(void) {
    osal_mutex_lock(AI.lock, OSAL_WAIT_FOREVER);
    g_turn_revoked = false;
    snprintf(g_turn_member, sizeof g_turn_member, "%s", ink_member_current()->id);
    g_turn_generation = ink_member_generation(g_turn_member);
    g_turn_epoch = ink_hub_member_epoch();
    AI.stream = 202; AI.tts_done_for = -1; AI.tts_stream = 0;
    osal_mutex_unlock(AI.lock);
    uint8_t packet[200];
    int n = sim_opus_fake_packet(packet, sizeof packet, 180, 1500, 180, 1);
    on_rt_frame(NULL, 202, INK_RT_VOICE_DOWN, packet, (size_t)n, 0, 0, 0);
    queued_speech = AI.tts_opened;
    for (int i = 0; i < 24; i++) CHECK(ink_audio_stream_push(queued_speech, packet, (size_t)n) == 0, "queue synthetic speech");
    ink_audio_stream_finish(queued_speech);
    AI.tts_stream = 0;
    uint32_t queued = 0;
    CHECK(queued_speech > 0 && !ink_audio_stream_progress(queued_speech, NULL, &queued) && queued > 0,
          "end-of-speech still has audio waiting to play");
}
void test_ai_speech_revoked(void) {
    CHECK(ink_audio_stream_progress(queued_speech, NULL, NULL) < 0, "handoff aborts finished-but-queued speech before the busy UI runs");
}

void test_ai_stale_question(void) {
    osal_mutex_lock(AI.lock, OSAL_WAIT_FOREVER);
    snprintf(g_turn_member, sizeof g_turn_member, "%s", ink_member_current()->id);
    g_turn_generation = ink_member_generation(g_turn_member);
    g_turn_epoch = ink_hub_member_epoch();
    osal_mutex_unlock(AI.lock);
    CHECK(ink_member_switch("kid") == 0 && ink_member_switch("owner") == 0, "member changes after private request preparation");
    snprintf(g_typed, sizeof g_typed, "SYNTHETIC_STALE_QUESTION");
    AI.req_id = request("ai.ask", false);
}
bool test_ai_stale_done(void) { return stale_status != 100; }
void test_ai_stale_result(void) {
    CHECK(stale_status == INK_HUB_ERR_LOST, "private request cannot rebind to a newer member epoch");
}
