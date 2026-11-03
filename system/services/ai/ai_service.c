/*
 * AI service and answer sheet (UI task), push-to-talk voice flow (D-17,
 * foundation §6.5).
 *
 * Key down: listening tone (heard within ~150 ms, long before the panel can
 *   change), microphone on, hub link pre-connect, `ai.listen` request with
 *   the page snapshot, sheet opens in "listening" state (FAST refresh).
 *   Opus frames stream to the hub as binary messages [1][stream][seq] once
 *   the link is online; frames captured before that are buffered (PSRAM).
 * Key up: microphone off; after the last frame `ai.listen.end` follows.
 * Closing the sheet ends the turn wherever it is: a capture still running
 *   stops at once (the key may still be held), frames not yet sent are
 *   dropped, `ai.listen.end` still goes out so the hub finishes the turn,
 *   and its answer and speech are ignored.
 * Hub -> device: `ai.stt` (what was understood), `ai.delta` (answer text),
 *   binary TTS frames [2][stream][seq] played as a VOICE stream, `ai.tts.end`,
 *   then the reply to `ai.listen`. Errors are shown in place.
 * Without a microphone (or a hub that cannot listen) the same sheet works
 * with a typed/fixed question through `ai.ask`.
 */
#include <stdio.h>
#include <string.h>

#include "ink/hal_display.h"
#include "ink/osal.h"
#include "ink/probe.h"
#include "ink/sys/cap.h"
#include "ink/sys/ai.h"
#include "ink/sys/audio.h"
#include "ink/sys/font.h"
#include "ink/sys/power.h"
#include "ink/sys/hub.h"
#include "ink/sys/json.h"
#include "ink/sys/ui.h"

#define ANSWER_CAP 2048
#define SNAPSHOT_CAP 3072
#define ASK_TIMEOUT_US (45ull * 1000000ull)
#define MAX_BUFFERED_FRAMES 512 /* ~30 s of speech captured before the link is online */

static const char *SIM_TRANSCRIPT = "这一页讲了什么？";

typedef struct frame {
    struct frame *next;
    size_t len;
    uint8_t data[]; /* [kind][stream][seq lo][seq hi] + Opus */
} frame_t;

static struct {
    lv_obj_t *sheet, *status, *question, *answer;
    bool open, asking;
    uint64_t t_press, t_release;
    int req_id; /* hub request id of the current turn (matches params.id of notifications) */
    int seq;    /* local turn counter (matches the reply callback's ctx) */
    bool first_text_seen;
    char *answer_text; /* PSRAM */
    size_t answer_len;
    /* voice uplink (touched by the audio task and the UI task) */
    osal_mutex_t *lock;
    uint8_t stream;
    frame_t *buf_head, *buf_tail;
    int buffered, frames_sent;
    bool end_pending;
    bool drop_frames;   /* the sheet was closed while capturing: the rest of this turn's audio is dropped */
    bool capturing;     /* UI task: this turn's microphone is on */
    bool mic_failed;    /* UI task: this turn's microphone did not start: nothing is heard, nothing asked */
    bool speech_failed; /* UI task: this turn's answer could not be spoken: the status keeps saying so */
    lv_timer_t *flush_timer;
    /* voice downlink (hub task; with `stream`, changed under `lock`) */
    int tts_stream;   /* audio stream playing the current turn's speech (0 none) */
    int tts_for;      /* turn the audio stream belongs to (-1 none) */
    int tts_done_for; /* turn whose speech has ended (-1 none): frames after the end are dropped */
    int tts_opened;   /* the last audio stream opened for the current turn (0 none): a failure told later */
} AI;

typedef struct {
    int req_id;
    int kind; /* 0 delta, 1 done, 2 error, 3 stt, 4 the answer's speech could not play */
    int status;
    char text[];
} ai_post_t;

/* ---------------------------------------------------------------- sheet */

static const lv_font_t *font_pt(double pt) {
    hal_panel_info_t pi;
    hal_display_get_info(&pi);
    ink_font_t *f = ink_font_for_pt(pt, pi.dpi);
    return f ? ink_font_lvgl(f) : ink_ui_font(0);
}

static void sheet_open(void) {
    if (AI.open) return;
    lv_obj_t *top = lv_layer_top();
    AI.sheet = lv_obj_create(top);
    lv_obj_remove_style_all(AI.sheet);
    lv_obj_set_size(AI.sheet, lv_pct(100), lv_pct(58));
    lv_obj_align(AI.sheet, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(AI.sheet, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(AI.sheet, LV_OPA_COVER, 0);
    lv_obj_set_style_border_side(AI.sheet, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_border_width(AI.sheet, ink_dp(2), 0);
    lv_obj_set_style_border_color(AI.sheet, lv_color_black(), 0);
    lv_obj_set_style_pad_all(AI.sheet, ink_dp(16), 0);
    lv_obj_set_flex_flow(AI.sheet, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(AI.sheet, ink_dp(10), 0);
    lv_obj_remove_flag(AI.sheet, LV_OBJ_FLAG_SCROLLABLE);

    AI.status = lv_label_create(AI.sheet);
    lv_obj_set_style_text_font(AI.status, font_pt(6), 0);
    lv_obj_set_style_text_color(AI.status, lv_color_hex(0x444444), 0);
    AI.question = lv_label_create(AI.sheet);
    lv_obj_set_style_text_font(AI.question, font_pt(7), 0);
    lv_obj_set_width(AI.question, lv_pct(100));
    lv_label_set_long_mode(AI.question, LV_LABEL_LONG_MODE_WRAP);
    lv_label_set_text(AI.question, "");
    AI.answer = lv_label_create(AI.sheet);
    lv_obj_set_style_text_font(AI.answer, font_pt(8), 0);
    lv_obj_set_width(AI.answer, lv_pct(100));
    lv_label_set_long_mode(AI.answer, LV_LABEL_LONG_MODE_WRAP);
    lv_label_set_text(AI.answer, "");
    AI.open = true;
}

static void set_status(const char *s) {
    if (!AI.open) return;
    lv_label_set_text(AI.status, s);
    ink_ui_hint_obj(AI.status, INK_REFRESH_FAST);
}

static void free_buffered(void);

/* Microphone off for this turn (UI task); idempotent. discard: the frames not yet sent and the rest
 * the encoder still delivers are dropped (the end notification still goes out). */
static void end_capture(bool discard) {
    if (!AI.capturing) return;
    AI.capturing = false;
    if (discard) {
        osal_mutex_lock(AI.lock, OSAL_WAIT_FOREVER);
        free_buffered();
        AI.drop_frames = true;
        osal_mutex_unlock(AI.lock);
        ink_probe_count("ai.capture_cancelled");
    }
    ink_audio_capture_end();
}

bool ink_ai_dismiss(void) {
    if (!AI.open) return false;
    end_capture(true); /* closing while the key is still held ends the recording too */
    lv_obj_delete(AI.sheet);
    AI.sheet = NULL;
    AI.open = false;
    AI.asking = false;
    /* closing ends this turn's speech too: frames still on their way are dropped (on_binary), not
     * played after the sheet is gone */
    osal_mutex_lock(AI.lock, OSAL_WAIT_FOREVER);
    AI.tts_stream = 0;
    AI.tts_done_for = AI.stream;
    osal_mutex_unlock(AI.lock);
    ink_audio_stop_all();
    ink_ui_hint_frame(INK_REFRESH_FLASH_UI); /* the page underneath comes back cleanly */
    return true;
}

/* -------------------------------------------------------------- results */

static void ui_apply(void *arg) { /* UI task */
    ai_post_t *p = arg;
    bool match = (p->kind == 0 || p->kind == 3) ? p->req_id == AI.req_id
                 : p->kind == 4                 ? p->req_id == AI.stream
                                                : p->req_id == AI.seq;
    if (!AI.open || !match) {
        osal_free(p);
        return;
    }
    if (p->kind == 3) {
        char q[600];
        snprintf(q, sizeof q, "你问：%s", p->text);
        lv_label_set_text(AI.question, q);
        ink_ui_hint_obj(AI.question, INK_REFRESH_FAST);
        ink_ui_cause(AI.t_release, "ai.release_to_question_visible_ms");
    } else if (p->kind == 0) {
        size_t n = strlen(p->text);
        if (AI.answer_len + n < ANSWER_CAP) {
            memcpy(AI.answer_text + AI.answer_len, p->text, n + 1);
            AI.answer_len += n;
        }
        lv_label_set_text(AI.answer, AI.answer_text);
        ink_ui_hint_obj(AI.answer, INK_REFRESH_UI);
        if (!AI.first_text_seen) {
            AI.first_text_seen = true;
            set_status("回答中");
            ink_ui_cause(AI.t_release, "ai.release_to_first_text_visible_ms");
        }
    } else if (p->kind == 4) { /* the speaker could not start for the answer (the text is on the sheet) */
        AI.speech_failed = true;
        set_status(AI.asking ? "声音没能播放：扬声器没打开，文字回答在上面" : "回答完成 · 声音没能播放（扬声器没打开）· 按 AI 键继续问");
        ink_probe_count("ai.speech_failed");
    } else if (p->kind == 1) {
        AI.asking = false;
        set_status(AI.speech_failed ? "回答完成 · 声音没能播放（扬声器没打开）· 按 AI 键继续问" : "回答完成 · 按 AI 键继续问，按 HOME 关闭");
        ink_ui_cause(AI.t_release, "ai.release_to_done_visible_ms");
        ink_probe_count("ai.answers");
    } else {
        AI.asking = false;
        const char *msg = p->status == INK_HUB_ERR_OFFLINE ? "现在没有网络，稍后再问我"
                          : p->status == INK_HUB_ERR_TIMEOUT ? "中枢没有回应，请再试一次"
                          : p->status == INK_HUB_ERR_LOST    ? "网络断了，回答没收全，请再试一次"
                                                             : "出了点问题，请再试一次";
        set_status(msg);
        ink_ui_cause(AI.t_release, "ai.release_to_error_visible_ms");
        ink_probe_count("ai.failures");
        ink_audio_tone(INK_TONE_ERROR, 0, NULL);
    }
    osal_free(p);
}

static void post(int req_id, int kind, int status, const char *text) {
    size_t n = text ? strlen(text) : 0;
    const char *prev = osal_mem_tag("ai");
    ai_post_t *p = osal_malloc(sizeof *p + n + 1, OSAL_MEM_EXTERNAL);
    osal_mem_tag(prev);
    if (!p) return;
    p->req_id = req_id;
    p->kind = kind;
    p->status = status;
    memcpy(p->text, text ? text : "", n + 1);
    ink_ui_post(ui_apply, p);
}

static void on_notify(void *ctx, const char *method, const ink_json_t *j, int params) { /* hub task */
    (void)ctx;
    int64_t id = 0;
    if (!ink_json_int(j, ink_json_find(j, params, "id"), &id)) return;
    if (!strcmp(method, "ai.delta") || !strcmp(method, "ai.stt")) {
        char text[512];
        if (ink_json_str(j, ink_json_find(j, params, "text"), text, sizeof text) < 0) return;
        post((int)id, method[3] == 'd' ? 0 : 3, 0, text);
    } else if (!strcmp(method, "ai.tts.end")) {
        int64_t st;
        if (!ink_json_int(j, ink_json_find(j, params, "stream"), &st)) return;
        osal_mutex_lock(AI.lock, OSAL_WAIT_FOREVER);
        if (st == AI.stream) { /* the end of an older turn (the user already asked again) changes nothing */
            if (AI.tts_stream > 0) ink_audio_stream_finish(AI.tts_stream);
            AI.tts_stream = 0;
            AI.tts_done_for = (int)st;
        }
        osal_mutex_unlock(AI.lock);
    }
}

/* The audio service gave up on a stream (its speaker output could not open for seconds): if it was
 * this turn's speech, the sheet says so. Audio task. */
static void on_speech_failed(void *ctx, int id) {
    (void)ctx;
    osal_mutex_lock(AI.lock, OSAL_WAIT_FOREVER);
    bool ours = id > 0 && id == AI.tts_opened;
    int turn = AI.stream;
    osal_mutex_unlock(AI.lock);
    if (ours) post(turn, 4, 0, NULL);
}

static void on_binary(void *ctx, const uint8_t *d, size_t len) { /* hub task: TTS frames */
    (void)ctx;
    if (len < 5 || d[0] != 2) return;
    /* under the lock, so a press (new turn) cannot slip between the check and opening a stream */
    osal_mutex_lock(AI.lock, OSAL_WAIT_FOREVER);
    if (d[1] == AI.stream && AI.tts_done_for != d[1]) { /* the current turn, its speech not ended yet */
        if (AI.tts_stream <= 0 || AI.tts_for != d[1]) {
            AI.tts_stream = ink_audio_stream_open(INK_AUDIO_PRIO_VOICE, AI.t_release, "ai.release_to_first_audio_heard_ms");
            AI.tts_for = d[1];
            AI.tts_opened = AI.tts_stream > 0 ? AI.tts_stream : 0;
        }
        if (AI.tts_stream > 0) ink_audio_stream_push(AI.tts_stream, d + 4, len - 4);
    }
    osal_mutex_unlock(AI.lock);
}

static void on_reply(void *ctx, int status, const ink_json_t *j, int result) { /* hub task */
    (void)j;
    (void)result;
    int id = (int)(intptr_t)ctx;
    post(id, status == INK_HUB_OK ? 1 : 2, status, NULL);
    ink_power_release(INK_NEED_NET); /* the turn is over (answer, error or timeout) */
}

/* --------------------------------------------------------- voice uplink */

static void free_buffered(void) { /* under lock */
    while (AI.buf_head) {
        frame_t *f = AI.buf_head;
        AI.buf_head = f->next;
        osal_free(f);
    }
    AI.buf_tail = NULL;
    AI.buffered = 0;
}

/* Send buffered frames (and the end notification once the last frame went out) if online. */
static void flush_uplink(void) {
    if (!ink_hub_online()) return;
    osal_mutex_lock(AI.lock, OSAL_WAIT_FOREVER);
    while (AI.buf_head) {
        frame_t *f = AI.buf_head;
        if (ink_hub_send_binary(f->data, f->len) != 0) break;
        AI.buf_head = f->next;
        if (!AI.buf_head) AI.buf_tail = NULL;
        AI.buffered--;
        AI.frames_sent++;
        osal_free(f);
    }
    bool send_end = AI.end_pending && !AI.buf_head;
    if (send_end) AI.end_pending = false;
    int frames = AI.frames_sent, stream = AI.stream;
    osal_mutex_unlock(AI.lock);
    if (send_end) {
        char params[64];
        snprintf(params, sizeof params, "{\"stream\":%d,\"frames\":%d}", stream, frames);
        ink_hub_notify("ai.listen.end", params);
        ink_probe_value("ai.release_to_upload_done_ms", (double)(osal_now_us() - AI.t_release) / 1000.0);
    }
}

static void on_frame(void *ctx, const uint8_t *data, size_t len, uint32_t seq) { /* audio task */
    (void)ctx;
    if (!len) { /* end of the utterance */
        osal_mutex_lock(AI.lock, OSAL_WAIT_FOREVER);
        AI.end_pending = true;
        osal_mutex_unlock(AI.lock);
        flush_uplink();
        return;
    }
    const char *prev = osal_mem_tag("ai");
    frame_t *f = osal_malloc(sizeof *f + len + 4, OSAL_MEM_EXTERNAL);
    osal_mem_tag(prev);
    if (!f) return;
    f->next = NULL;
    f->len = len + 4;
    f->data[0] = 1;
    f->data[2] = (uint8_t)seq;
    f->data[3] = (uint8_t)(seq >> 8);
    memcpy(f->data + 4, data, len);
    osal_mutex_lock(AI.lock, OSAL_WAIT_FOREVER);
    f->data[1] = AI.stream;
    if (AI.drop_frames) { /* the sheet was closed: nothing more of this turn goes out */
        osal_mutex_unlock(AI.lock);
        osal_free(f);
        return;
    }
    if (AI.buffered >= MAX_BUFFERED_FRAMES) {
        osal_mutex_unlock(AI.lock);
        osal_free(f);
        ink_probe_count("ai.uplink_frames_dropped");
        return;
    }
    if (AI.buf_tail) AI.buf_tail->next = f;
    else AI.buf_head = f;
    AI.buf_tail = f;
    AI.buffered++;
    osal_mutex_unlock(AI.lock);
    flush_uplink();
}

static void flush_timer_cb(lv_timer_t *t) { /* UI task: the link may come up after the user stopped talking */
    flush_uplink();
    osal_mutex_lock(AI.lock, OSAL_WAIT_FOREVER);
    bool idle = !AI.buf_head && !AI.end_pending;
    osal_mutex_unlock(AI.lock);
    if (idle && !AI.asking) lv_timer_pause(t); /* resumed at the next key press */
}

/* ------------------------------------------------------------- requests */

/* Build the request params: page snapshot plus either the stream description or the question. */
static int request(const char *method, bool voice) {
    const char *prev = osal_mem_tag("ai");
    char *snap = osal_malloc(SNAPSHOT_CAP, OSAL_MEM_EXTERNAL);
    char *params = osal_malloc(SNAPSHOT_CAP * 2 + 256, OSAL_MEM_EXTERNAL);
    osal_mem_tag(prev);
    if (!snap || !params) {
        osal_free(snap);
        osal_free(params);
        return -1;
    }
    const char *app = "";
    int n = ink_ui_snapshot(snap, SNAPSHOT_CAP, &app);
    ink_jw_t w;
    ink_jw_init(&w, params, SNAPSHOT_CAP * 2 + 256);
    if (voice) {
        char head[96];
        snprintf(head, sizeof head, "{\"stream\":%d,\"format\":\"opus\",\"rate\":16000,\"frame_ms\":60", AI.stream);
        ink_jw_raw(&w, head);
    } else {
        ink_jw_raw(&w, "{\"question\":");
        ink_jw_str(&w, SIM_TRANSCRIPT);
    }
    char gen[48];
    snprintf(gen, sizeof gen, ",\"page_gen\":%u", (unsigned)ink_ui_generation());
    ink_jw_raw(&w, gen);
    ink_jw_raw(&w, ",\"page\":{\"app\":");
    ink_jw_str(&w, app);
    ink_jw_raw(&w, ",\"text\":");
    ink_jw_strn(&w, snap, n > 0 ? (size_t)n : 0);
    ink_jw_raw(&w, ",\"actions\":");
    {
        char ids[768];
        int k = ink_cap_ids_json(ids, sizeof ids);
        ink_jw_raw(&w, k > 0 ? ids : "[]");
    }
    ink_jw_raw(&w, "}}");
    AI.answer_len = 0;
    AI.answer_text[0] = 0;
    AI.first_text_seen = false;
    AI.asking = true;
    AI.seq++;
    ink_power_hold(INK_NEED_NET); /* no sleep while the answer is on its way */
    int id = ink_hub_call(method, w.overflow ? "{}" : params, ASK_TIMEOUT_US, on_reply, (void *)(intptr_t)AI.seq);
    if (id < 0) ink_power_release(INK_NEED_NET);
    osal_free(snap);
    osal_free(params);
    return id;
}

/* ------------------------------------------------------------------ key */

static bool voice_ok(void) { return ink_audio_has_mic(); }

void ink_ai_key(bool pressed, uint64_t t_us) {
    if (pressed) {
        AI.t_press = t_us;
        /* a new turn: frames and the speech end of the previous one no longer apply */
        osal_mutex_lock(AI.lock, OSAL_WAIT_FOREVER);
        AI.stream++;
        AI.tts_stream = AI.tts_opened = 0;
        AI.tts_for = AI.tts_done_for = -1;
        osal_mutex_unlock(AI.lock);
        AI.mic_failed = AI.speech_failed = false;
        ink_audio_stop_all(); /* talking over the previous answer interrupts it */
        ink_audio_tone(INK_TONE_LISTEN, t_us, "ai.press_to_tone_heard_ms");
        ink_hub_demand(); /* pre-connect while the user speaks */
        if (AI.open && !AI.asking) {
            lv_label_set_text(AI.answer, "");
            lv_label_set_text(AI.question, "");
        }
        sheet_open();
        set_status("正在听…松开 AI 键结束");
        /* the sheet appears or is cleared for a new question: drive it with the fast binary waveform
         * (black on white); later updates use GL16 and clean up the anti-aliased text */
        ink_ui_hint_obj(AI.sheet, INK_REFRESH_FAST);
        ink_ui_cause(t_us, "ai.press_to_listening_visible_ms");
        ink_probe_count("ai.presses");
        if (voice_ok()) {
            osal_mutex_lock(AI.lock, OSAL_WAIT_FOREVER);
            free_buffered();
            AI.frames_sent = 0;
            AI.end_pending = false;
            AI.drop_frames = false;
            osal_mutex_unlock(AI.lock);
            if (ink_audio_capture_begin(on_frame, NULL) == 0) {
                AI.capturing = true;
                AI.req_id = request("ai.listen", true);
                if (!AI.flush_timer) AI.flush_timer = lv_timer_create(flush_timer_cb, 100, NULL);
                else lv_timer_resume(AI.flush_timer);
            } else { /* nothing will be heard this turn, so nothing will be asked (audit AUD5-003) */
                AI.mic_failed = true;
                ink_probe_count("ai.mic_unavailable");
                set_status("麦克风没能打开，请稍后再按 AI 键");
                ink_audio_tone(INK_TONE_ERROR, 0, NULL);
            }
        }
        return;
    }
    bool was_capturing = AI.capturing;
    end_capture(false); /* also when the sheet was closed meanwhile (then it already ended) */
    if (!AI.open) return;
    osal_mutex_lock(AI.lock, OSAL_WAIT_FOREVER);
    AI.t_release = t_us; /* the hub task reads it for the first-audio probe */
    osal_mutex_unlock(AI.lock);
    if (AI.mic_failed) { /* the microphone did not start: the user's words were not heard */
        set_status("没听到：麦克风没能打开，请稍后再按 AI 键");
        return;
    }
    if (was_capturing || (voice_ok() && AI.asking)) {
        lv_label_set_text(AI.question, "…");
        set_status("正在想…");
        return;
    }
    if (voice_ok()) { /* a microphone that did not record this turn: there is nothing to ask */
        set_status("没听到，请再按 AI 键说一遍");
        return;
    }
    /* a device without a microphone: a sample question stands in, and says so */
    char q[128];
    snprintf(q, sizeof q, "示例问题（这台设备没有麦克风）：%s", SIM_TRANSCRIPT);
    lv_label_set_text(AI.question, q);
    ink_ui_hint_obj(AI.question, INK_REFRESH_FAST);
    set_status("正在想…");
    AI.req_id = request("ai.ask", false);
}

int ink_ai_init(void) {
    const char *prev = osal_mem_tag("ai");
    AI.answer_text = osal_malloc(ANSWER_CAP, OSAL_MEM_EXTERNAL);
    AI.lock = osal_mutex_create();
    osal_mem_tag(prev);
    if (!AI.answer_text || !AI.lock) return -1;
    AI.answer_text[0] = 0;
    AI.tts_for = AI.tts_done_for = -1;
    ink_hub_on_binary(on_binary, NULL);
    ink_audio_on_stream_failed(on_speech_failed, NULL);
    return ink_hub_subscribe("ai.", on_notify, NULL);
}
