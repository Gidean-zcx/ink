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
#include "ink/sys/exec.h"
#include "ink/sys/rt.h"
#include "ink/sys/settings.h"
#include "ink/sys/state.h"
#include "ink/sys/ai.h"
#include "ink/sys/audio.h"
#include "ink/sys/font.h"
#include "ink/sys/power.h"
#include "ink/sys/hub.h"
#include "ink/sys/json.h"
#include "ink/sys/ui.h"

#define ANSWER_CAP 2048
#define SNAPSHOT_CAP 3072
#define STATE_CAP 2048 /* the system's state, with every question: what the AI knows without asking */
#define ASK_TIMEOUT_US (45ull * 1000000ull)
#define MAX_BUFFERED_FRAMES 512 /* ~30 s of speech captured before the link is online */

static const char *SIM_TRANSCRIPT = "这一页讲了什么？";

typedef struct frame {
    struct frame *next;
    size_t len;
    uint8_t sid; /* the realtime session of its turn */
    uint8_t data[]; /* Opus */
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
    uint8_t stream;     /* the turn's id: its realtime session's, also after that was closed */
    int session;        /* the turn's realtime session while open (0 none): closed once, see close_session */
    int session_seq;    /* the request (seq) the hub answers last for it */
    frame_t *buf_head, *buf_tail;
    int buffered, frames_sent;
    bool end_pending;
    bool up_open;       /* the hub still has to be told the utterance ended (ai.listen.end) */
    bool dismissed;     /* the sheet was closed during this turn */
    bool drop_frames;   /* the sheet was closed while capturing: the rest of this turn's audio is dropped */
    bool capturing;     /* UI task: this turn's microphone is on */
    bool turn_over;     /* UI task: answered or failed, also if the key is still held: what the sheet says stands */
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
    ink_exec_job_t job;
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

/* The end of a turn's session (architecture §9.2): the hub answered (it answers last, after the
 * speech), the sheet was closed and the hub knows the utterance ended, a new turn began, or the turn
 * could not start. Whoever sees it first closes it, any task: no background work stays held and the
 * hub link is free to go idle. The session is taken under the lock in the same step that found this
 * turn is over; closed afterwards, it cannot be the next turn's. */
static int take_session(void) { /* under the lock */
    int sid = AI.session;
    AI.session = 0;
    return sid;
}

static void close_session(void) {
    osal_mutex_lock(AI.lock, OSAL_WAIT_FOREVER);
    int sid = take_session();
    osal_mutex_unlock(AI.lock);
    if (sid) ink_rt_close(sid);
}

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
    /* closing ends this turn's speech too: frames still on their way are dropped (on_rt_frame), not
     * played after the sheet is gone */
    osal_mutex_lock(AI.lock, OSAL_WAIT_FOREVER);
    AI.tts_stream = 0;
    AI.tts_done_for = AI.stream;
    AI.dismissed = true;
    int sid = AI.up_open ? 0 : take_session(); /* else it closes when the hub has been told (flush_uplink) */
    osal_mutex_unlock(AI.lock);
    if (sid) ink_rt_close(sid);
    ink_audio_stop_all();
    ink_ui_hint_frame(INK_REFRESH_FLASH_UI); /* the page underneath comes back cleanly */
    return true;
}

/* -------------------------------------------------------------- results */

/* Nothing more happens in this turn, even if the key is still held: the microphone goes off. */
static void turn_over(void) {
    AI.asking = false;
    AI.turn_over = true;
    end_capture(true);
}

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
        turn_over();
        set_status(AI.speech_failed ? "回答完成 · 声音没能播放（扬声器没打开）· 按 AI 键继续问" : "回答完成 · 按 AI 键继续问，按 HOME 关闭");
        ink_ui_cause(AI.t_release, "ai.release_to_done_visible_ms");
        ink_probe_count("ai.answers");
    } else {
        turn_over();
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
    ai_post_t *p = osal_calloc(1, sizeof *p + n + 1, OSAL_MEM_EXTERNAL);
    osal_mem_tag(prev);
    if (!p) return;
    p->req_id = req_id;
    p->kind = kind;
    p->status = status;
    memcpy(p->text, text ? text : "", n + 1);
    ink_exec_post(INK_EXEC_UI, &p->job, ui_apply, p);
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

static void on_rt_frame(void *ctx, int sid, uint8_t kind, const uint8_t *opus, size_t len, uint16_t seq, uint32_t t_ms,
                        uint8_t flags) { /* hub task: TTS frames of this turn's session */
    (void)ctx;
    (void)seq;
    (void)t_ms;
    (void)flags;
    if (kind != INK_RT_VOICE_DOWN || !len) return;
    uint8_t st = (uint8_t)sid;
    /* under the lock, so a press (new turn) cannot slip between the check and opening a stream */
    osal_mutex_lock(AI.lock, OSAL_WAIT_FOREVER);
    if (st == AI.stream && AI.tts_done_for != st) { /* the current turn, its speech not ended yet */
        if (AI.tts_stream <= 0 || AI.tts_for != st) {
            AI.tts_stream = ink_audio_stream_open(INK_AUDIO_PRIO_VOICE, AI.t_release, "ai.release_to_first_audio_heard_ms");
            AI.tts_for = st;
            AI.tts_opened = AI.tts_stream > 0 ? AI.tts_stream : 0;
        }
        if (AI.tts_stream > 0) ink_audio_stream_push(AI.tts_stream, opus, len);
    }
    osal_mutex_unlock(AI.lock);
}

static void on_reply(void *ctx, int status, const ink_json_t *j, int result) { /* hub task */
    (void)j;
    (void)result;
    int id = (int)(intptr_t)ctx;
    post(id, status == INK_HUB_OK ? 1 : 2, status, NULL);
    ink_power_release(INK_NEED_NET); /* the turn is over (answer, error or timeout) */
    osal_mutex_lock(AI.lock, OSAL_WAIT_FOREVER);
    int sid = 0;
    if (AI.session && id == AI.session_seq) { /* the current turn: nothing more will come for it */
        free_buffered();
        AI.end_pending = AI.up_open = false;
        AI.drop_frames = true;
        if (AI.tts_stream > 0) ink_audio_stream_finish(AI.tts_stream); /* an answer without an end of speech */
        AI.tts_stream = 0;
        AI.tts_done_for = AI.stream;
        sid = take_session();
    }
    osal_mutex_unlock(AI.lock);
    if (sid) ink_rt_close(sid);
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
    if (!ink_rt_ready(AI.stream)) return; /* the turn's session is not confirmed yet: keep buffering */
    osal_mutex_lock(AI.lock, OSAL_WAIT_FOREVER);
    while (AI.buf_head) {
        frame_t *f = AI.buf_head;
        if (ink_rt_send(f->sid, INK_RT_VOICE_UP, f->data, f->len, 0) != 0) break;
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
        bool told = ink_hub_notify("ai.listen.end", params) == 0;
        osal_mutex_lock(AI.lock, OSAL_WAIT_FOREVER);
        int sid = 0;
        if (AI.stream == stream) { /* still this turn */
            if (told) AI.up_open = false;
            else AI.end_pending = true; /* the flush timer tries again: without it the hub waits for the rest */
            if (told && AI.dismissed) sid = take_session();
        }
        osal_mutex_unlock(AI.lock);
        if (told) ink_probe_value("ai.release_to_upload_done_ms", (double)(osal_now_us() - AI.t_release) / 1000.0);
        if (sid) ink_rt_close(sid);
    }
}

static void on_frame(void *ctx, const uint8_t *data, size_t len, uint32_t seq) { /* audio task */
    (void)ctx;
    if (!len) { /* end of the utterance */
        osal_mutex_lock(AI.lock, OSAL_WAIT_FOREVER);
        AI.end_pending = AI.up_open; /* not if the turn is over already (the hub answered) */
        osal_mutex_unlock(AI.lock);
        flush_uplink();
        return;
    }
    const char *prev = osal_mem_tag("ai");
    frame_t *f = osal_malloc(sizeof *f + len, OSAL_MEM_EXTERNAL);
    osal_mem_tag(prev);
    if (!f) return;
    (void)seq; /* the session numbers its frames */
    f->next = NULL;
    f->len = len;
    memcpy(f->data, data, len);
    osal_mutex_lock(AI.lock, OSAL_WAIT_FOREVER);
    f->sid = AI.stream;
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
    bool idle = !AI.buf_head && !AI.end_pending && !AI.up_open;
    osal_mutex_unlock(AI.lock);
    if (idle && !AI.asking) lv_timer_pause(t); /* resumed at the next key press */
}

/* ------------------------------------------------------------- requests */

static char g_typed[384] INK_PSRAM_BSS; /* a question in words (ink_ai_ask_text) */

/* Build the request params: page snapshot plus either the stream description or the question. */
static int request(const char *method, bool voice) {
    AI.answer_len = 0;
    AI.answer_text[0] = 0;
    AI.first_text_seen = false;
    AI.asking = true;
    osal_mutex_lock(AI.lock, OSAL_WAIT_FOREVER);
    AI.seq++;
    if (voice) AI.session_seq = AI.seq;
    osal_mutex_unlock(AI.lock);
    const char *prev = osal_mem_tag("ai");
    char *snap = osal_malloc(SNAPSHOT_CAP, OSAL_MEM_EXTERNAL);
    char *params = osal_malloc(SNAPSHOT_CAP * 2 + STATE_CAP + 256, OSAL_MEM_EXTERNAL);
    osal_mem_tag(prev);
    if (!snap || !params) {
        osal_free(snap);
        osal_free(params);
        post(AI.seq, 2, INK_HUB_ERR_NOMEM, NULL);
        return -1;
    }
    const char *app = "";
    int n = ink_ui_snapshot(snap, SNAPSHOT_CAP, &app);
    ink_jw_t w;
    ink_jw_init(&w, params, SNAPSHOT_CAP * 2 + STATE_CAP + 256);
    if (voice) {
        char head[96];
        snprintf(head, sizeof head, "{\"stream\":%d,\"format\":\"opus\",\"rate\":16000,\"frame_ms\":60", AI.stream);
        ink_jw_raw(&w, head);
    } else {
        ink_jw_raw(&w, "{\"question\":");
        ink_jw_str(&w, g_typed[0] ? g_typed : SIM_TRANSCRIPT);
    }
    char gen[48];
    snprintf(gen, sizeof gen, ",\"page_gen\":%u", (unsigned)ink_ui_content_version());
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
    ink_jw_raw(&w, "},\"state\":");
    {
        int k = ink_state_json(snap, STATE_CAP, INK_EXPOSE_AI); /* the page's text is in params already: its buffer is free */
        ink_jw_raw(&w, k > 0 ? snap : "{}");
    }
    ink_jw_raw(&w, "}");
    ink_power_hold(INK_NEED_NET); /* no sleep while the answer is on its way */
    int id = ink_hub_call(method, w.overflow ? "{}" : params, ASK_TIMEOUT_US, on_reply, (void *)(intptr_t)AI.seq);
    if (id < 0) { /* no reply will come: the turn ends here */
        ink_power_release(INK_NEED_NET);
        post(AI.seq, 2, id, NULL);
    }
    osal_free(snap);
    osal_free(params);
    return id;
}

/* ------------------------------------------------------------------ key */

static bool voice_ok(void) { return ink_audio_has_mic(); }

/* What the listening tone measures: from the key press, or from the wake word / the interruption. */
static const char *tone_probe = "ai.press_to_tone_heard_ms";

/* The hub refused or ended this turn's session (hub task): nothing more can be sent or heard. If it
 * had not heard the whole utterance the turn has failed; otherwise its answer still decides. */
static void on_rt_closed(void *ctx, int sid, int reason) {
    (void)ctx;
    (void)reason;
    osal_mutex_lock(AI.lock, OSAL_WAIT_FOREVER);
    bool current = sid == AI.session, failed = current && AI.up_open;
    int turn = AI.session_seq;
    if (current) {
        AI.session = 0;
        free_buffered();
        AI.end_pending = AI.up_open = false;
        AI.drop_frames = true;
    }
    osal_mutex_unlock(AI.lock);
    if (failed && turn) post(turn, 2, INK_HUB_ERR_REMOTE, NULL);
}

static const ink_rt_handler_t AI_RT = {.on_frame = on_rt_frame, .on_closed = on_rt_closed};

void ink_ai_key(bool pressed, uint64_t t_us) {
    if (pressed) {
        AI.t_press = t_us;
        /* a new turn: frames and the speech end of the previous one no longer apply */
        close_session(); /* the previous turn's, if the hub has not answered it yet */
        int sid = voice_ok() ? ink_rt_open("voice.chat", NULL, &AI_RT) : -1; /* a typed question is answered in the reply */
        osal_mutex_lock(AI.lock, OSAL_WAIT_FOREVER);
        AI.stream = sid > 0 ? (uint8_t)sid : 0;
        AI.session = sid > 0 ? sid : 0;
        AI.session_seq = 0;
        AI.up_open = AI.dismissed = false;
        AI.tts_stream = AI.tts_opened = 0;
        AI.tts_for = AI.tts_done_for = -1;
        osal_mutex_unlock(AI.lock);
        if (sid > 0 && !ink_rt_is_open(sid)) { /* it ended already, its handler may have found no session to end */
            close_session();
            sid = -1;
        }
        AI.mic_failed = AI.speech_failed = AI.turn_over = false;
        ink_audio_stop_all(); /* talking over the previous answer interrupts it */
        ink_audio_tone(INK_TONE_LISTEN, t_us, tone_probe);
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
            AI.up_open = sid > 0;
            osal_mutex_unlock(AI.lock);
            if (sid > 0 && ink_audio_capture_begin(on_frame, NULL) == 0) {
                AI.capturing = true;
                AI.req_id = request("ai.listen", true);
                if (AI.req_id < 0) { /* the hub link took no request: what it says is on the sheet */
                    end_capture(true);
                    osal_mutex_lock(AI.lock, OSAL_WAIT_FOREVER);
                    AI.up_open = false;
                    osal_mutex_unlock(AI.lock);
                    close_session();
                }
                if (!AI.flush_timer) AI.flush_timer = lv_timer_create(flush_timer_cb, 100, NULL);
                else lv_timer_resume(AI.flush_timer);
            } else { /* nothing will be heard this turn, so nothing will be asked (audit AUD5-003) */
                AI.mic_failed = true;
                ink_probe_count("ai.mic_unavailable");
                osal_mutex_lock(AI.lock, OSAL_WAIT_FOREVER);
                AI.up_open = false;
                osal_mutex_unlock(AI.lock);
                close_session();
                set_status(sid > 0 ? "麦克风没能打开，请稍后再按 AI 键" : "语音通道没能打开，请稍后再按 AI 键");
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
    if (AI.turn_over) return; /* it ended while the key was held */
    if (AI.mic_failed) { /* the microphone did not start: the user's words were not heard */
        set_status("没听到：没能开始听，请稍后再按 AI 键");
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

void ink_ai_ask_text(const char *text) {
    close_session();
    osal_mutex_lock(AI.lock, OSAL_WAIT_FOREVER);
    AI.stream = 0;
    AI.session = 0;
    AI.session_seq = 0;
    AI.up_open = AI.dismissed = false;
    AI.tts_stream = AI.tts_opened = 0;
    AI.tts_for = AI.tts_done_for = -1;
    osal_mutex_unlock(AI.lock);
    AI.mic_failed = AI.speech_failed = AI.turn_over = false;
    ink_audio_stop_all();
    snprintf(g_typed, sizeof g_typed, "%s", text);
    ink_hub_demand();
    sheet_open();
    lv_label_set_text(AI.answer, "");
    lv_label_set_text(AI.question, g_typed);
    ink_ui_hint_obj(AI.sheet, INK_REFRESH_FAST);
    set_status("正在想…");
    AI.req_id = request("ai.ask", false);
    g_typed[0] = 0;
}

/* ------------------------------------------------------------- hands-free (R-047) */

typedef struct {
    ink_exec_job_t job;
    ink_listen_ev_t ev;
    uint64_t t;
} listen_job_t;

static bool g_handsfree_turn; /* UI task: this turn began with the wake word or an interruption */

/* Offline quick intents (architecture §6.4): the commonest commands, recognised on the device and
 * mapped straight to a capability, so they work without the hub. Online the hub understands the
 * whole utterance instead. */
static const char *const INTENT_WORDS[] = {"下一页", "上一页", "读出这一页", "回到主页"};
static const char *const INTENT_CAPS[] = {"reader.next_page", "reader.prev_page", "reader.read_page", "system.home"};

static void offline_intent(int k, uint64_t t) { /* UI task, during a hands-free turn */
    if (k < 0 || k >= (int)(sizeof INTENT_CAPS / sizeof INTENT_CAPS[0]) || ink_hub_online()) return;
    ink_receipt_t rc;
    ink_cap_invoke(INTENT_CAPS[k], NULL, INK_SRC_AI, 0, &rc); /* the AI's policy applies, as for the hub's calls */
    ink_probe_count("ai.offline_intents");
    g_handsfree_turn = false;
    ink_ai_dismiss(); /* the turn ends here: nothing goes to the hub */
    ink_audio_tone(rc.status == INK_RC_DONE ? INK_TONE_DONE : INK_TONE_ERROR, t, NULL);
}

static void listen_job(void *arg) { /* UI task: the voice front end's events drive the same turn as the AI key */
    listen_job_t *j = arg;
    if (j->ev == INK_LISTEN_WAKE && g_handsfree_turn) j->ev = INK_LISTEN_SPEECH; /* already listening to this one */
    if (j->ev == INK_LISTEN_WAKE || j->ev == INK_LISTEN_BARGE) {
        g_handsfree_turn = true;
        ink_probe_count(j->ev == INK_LISTEN_WAKE ? "ai.wakes" : "ai.barge_ins");
        tone_probe = j->ev == INK_LISTEN_WAKE ? "ai.wake_to_tone_heard_ms" : "ai.barge_to_tone_heard_ms";
        ink_ai_key(true, j->t);
        tone_probe = "ai.press_to_tone_heard_ms";
    } else if (j->ev == INK_LISTEN_COMMAND && g_handsfree_turn) {
        offline_intent(ink_audio_last_command(), j->t);
    } else if (j->ev == INK_LISTEN_QUIET && g_handsfree_turn) { /* said what they wanted: like releasing the key */
        g_handsfree_turn = false;
        ink_ai_key(false, j->t);
    }
    osal_free(j);
}

static void on_listen(void *ctx, ink_listen_ev_t ev, uint64_t t_us) { /* audio task */
    (void)ctx;
    if (ev == INK_LISTEN_SPEECH) return; /* speech without the wake word is not for the device */
    listen_job_t *j = osal_calloc(1, sizeof *j, OSAL_MEM_EXTERNAL);
    if (!j) return;
    j->ev = ev;
    j->t = t_us;
    ink_exec_post(INK_EXEC_UI, &j->job, listen_job, j);
}

/* Listen when the setting says so (D-52: by default while the screen is on and unlocked). */
static ink_exec_job_t g_policy_job;

static void listen_policy(void *unused) {
    (void)unused;
    int64_t mode = ink_setting_get("voice.handsfree"); /* 0 screen on, 1 always, 2 off */
    char ps[16] = "";
    ink_state_get_str(ink_state_find("power.state"), ps, sizeof ps);
    bool want = ink_audio_can_listen() && mode != 2 && (mode == 1 || strcmp(ps, "locked") != 0);
    ink_audio_listen(want, on_listen, NULL);
}

static void on_listen_inputs(void *ctx, int h, const char *id) { /* power.state or voice.handsfree changed */
    (void)ctx;
    (void)h;
    (void)id;
    ink_exec_post(INK_EXEC_UI, &g_policy_job, listen_policy, NULL);
}

int ink_ai_init(void) {
    const char *prev = osal_mem_tag("ai");
    AI.answer_text = osal_malloc(ANSWER_CAP, OSAL_MEM_EXTERNAL);
    AI.lock = osal_mutex_create();
    osal_mem_tag(prev);
    if (!AI.answer_text || !AI.lock) return -1;
    AI.answer_text[0] = 0;
    AI.tts_for = AI.tts_done_for = -1;
    ink_audio_on_stream_failed(on_speech_failed, NULL);
    ink_audio_listen_commands(INTENT_WORDS, (int)(sizeof INTENT_WORDS / sizeof INTENT_WORDS[0]));
    ink_state_watch("power.state", on_listen_inputs, NULL, INK_EXEC_DIRECT);
    ink_settings_watch("voice.handsfree", on_listen_inputs, NULL, INK_EXEC_DIRECT);
    listen_policy(NULL);
    return ink_hub_subscribe("ai.", on_notify, NULL);
}
