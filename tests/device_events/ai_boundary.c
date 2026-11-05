#include "ink/sys/audio.h"
#include "ink/sys/hub.h"
#include "ink/probe.h"
#include <string.h>
static bool fail_push;
static bool fail_next_push, fail_next_open;
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
    if (fail_next_push) {
        fail_next_push = false;
        ink_probe_count("test.audio.refused");
        return -1;
    }
    return fail_push ? -1 : ink_audio_stream_push(id, opus, len);
}
static int fixture_stream_open(ink_audio_prio_t prio, uint64_t cause_us, const char *heard_probe) {
    if (fail_next_open) {
        fail_next_open = false;
        ink_probe_count("test.audio.open_refused");
        return -1;
    }
    return ink_audio_stream_open(prio, cause_us, heard_probe);
}
#define ink_audio_stream_push fixture_push
#define ink_audio_stream_open fixture_stream_open
#define ink_hub_call_for fixture_call_for
#ifndef INK_TEST_AI_SOURCE
#define INK_TEST_AI_SOURCE "../../system/services/ai/ai_service.c"
#endif
#include INK_TEST_AI_SOURCE
#undef ink_audio_stream_push
#undef ink_audio_stream_open
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

void test_ai_review_voice(void) {
    ink_ai_key(true, osal_now_us());
    CHECK(AI.capturing && AI.session_seq != AI.stream, "real voice uses distinct turn and realtime identities");
}
void test_ai_review_push_refusal(void) {
    fail_next_push = true;
    ink_ai_key(false, osal_now_us());
}
void test_ai_review_open_refusal(void) {
    fail_next_open = true;
    ink_ai_key(false, osal_now_us());
}
void test_ai_review_release(void) { ink_ai_key(false, osal_now_us()); }
void test_ai_review_aborted(void) {
    osal_mutex_lock(AI.lock, OSAL_WAIT_FOREVER);
    CHECK(AI.tts_stream == 0 && AI.tts_done_for == AI.stream, "real downlink refusal aborts the rest of this speech");
    osal_mutex_unlock(AI.lock);
}
void test_ai_review_failure(void) {
    CHECK(AI.open && AI.speech_failed && strstr(lv_label_get_text(AI.status), "声音没能播放") &&
          strstr(lv_label_get_text(AI.status), "回答完成"), "real speech failure remains visible after the answer completes");
}
void test_ai_review_speaker_failure(void) {
    CHECK(AI.tts_opened > 0 && AI.session_seq != AI.stream, "speaker callback belongs to a real completed voice turn");
    on_speech_failed(NULL, AI.tts_opened);
}
void test_ai_review_new_turn(void) {
    int old_turn = AI.session_seq;
    ink_ai_ask_text("SYNTHETIC_NEXT_TYPED_TURN");
    post(old_turn, 4, 0, NULL);
}
void test_ai_review_clean(void) {
    CHECK(AI.open && !AI.speech_failed && !strstr(lv_label_get_text(AI.status), "声音没能播放"),
          "queued speech failure cannot contaminate the next typed turn");
}
void test_ai_review_early_failure(void) { post(AI.session_seq, 4, 0, NULL); }
void test_ai_review_first_text(void) {
    CHECK(AI.open && AI.asking && AI.first_text_seen && AI.speech_failed && strstr(lv_label_get_text(AI.status), "声音没能播放"),
          "speech failure remains visible when the first text arrives");
}
