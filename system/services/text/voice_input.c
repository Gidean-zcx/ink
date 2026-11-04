/*
 * Dictation (system services design §9.5): speech into the text field that has the focus. The words are the hub's (asr.stream,
 * hub protocol §3.11): the device itself only knows command words, so without the hub there is no dictation, and the person is
 * told so. What is heard shows in the keyboard's panel as it comes; it goes into the field when it is over, and can be edited
 * there like anything typed.
 *
 * The UI task, except where it says otherwise. What comes from the hub task and the audio task is handed over.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ink/osal.h"
#include "ink/probe.h"
#include "ink/sys/audio.h"
#include "ink/sys/cap.h"
#include "ink/sys/exec.h"
#include "ink/sys/hub.h"
#include "ink/sys/media.h"
#include "ink/sys/power.h"
#include "ink/sys/rt.h"
#include "ink/sys/state.h"
#include "ink/sys/svc.h"
#include "ink/sys/textinput.h"
#include "text_internal.h"

#define ASR_CAP "asr.stream"
#define PARAMS "{\"format\":\"opus\",\"rate\":16000,\"frame_ms\":60,\"kind\":\"%s\",\"interim_ms\":800}"
#define TICK_US 200000u
#define CONNECT_MS 6000u      /* to reach the hub */
#define FINISH_MS 8000u       /* for the last of the words, once it has been said */
#define PAUSE_MS 1500u        /* a pause this long, after something was said, is the end */
#define NOTHING_SAID_MS 8000u /* nothing at all */
#define SPEECH_LEVEL 300      /* mean absolute sample: a quiet room a few, a voice hundreds. To be calibrated on the device */
#define MAX_BUFFERED 50       /* 3 s: what is said before the hub has confirmed the session */
#define WORDS_MAX 4095
#define RESULT_MAX 480        /* the hub's limit for one result */
#define SHOWN_MAX 240         /* the panel's fixed area */

typedef struct frame {
    struct frame *next;
    size_t len;
    uint8_t data[];
} frame_t;

typedef struct {
    ink_exec_job_t job;
    int sid, seg;
    bool final;
    char text[RESULT_MAX + 1];
} result_t;

static struct {
    ink_voice_state_t state;
    int sid, focus, focus_gen;
    bool capturing;   /* until the end of the capture was delivered: the microphone is not free before that */
    bool capture_ended_asked, end_sent;
    uint64_t state_since_us;
    int readies;
    /* the audio task's while a capture runs, this task's after it */
    bool drop;
    frame_t *head, *tail;
    int buffered;
    bool heard;
    uint32_t last_voice_ms;
    /* what was heard */
    char words[WORDS_MAX + 1];
    size_t nwords;
    int next_seg;
    char interim[RESULT_MAX + 1];
    char shown[SHOWN_MAX + 1];
    char notice[64];
    char error_code[16];
    ink_exec_job_t ready_job, done_job, error_job, closed_job, lost_job, ended_job, tick_job;
    ink_svc_deadline_t tick_at;
    int h_state;
} VS INK_PSRAM_BSS;

static const char *const STATE_NAME[] = {"idle", "connecting", "listening", "finishing"};

ink_voice_state_t ink_text_voice_state(void) { return VS.state; }
const char *ink_text_voice_heard(void) { return VS.shown; }
const char *ink_text_voice_notice(void) { return VS.notice; }

const char *ink_text_voice_state_name(void) { return STATE_NAME[VS.state]; }

/* A key to show. Offline it stays: tapping it is how the person hears why not. Online the hub's directory says, by member. */
bool ink_text_voice_possible(void) {
    if (!ink_text_focused() || ink_text_type() == INK_TEXT_PASSWORD || !ink_audio_has_mic()) return false;
    return !ink_hub_online() || ink_cap_available(ASR_CAP);
}

void text_voice_clear_notice(void) { VS.notice[0] = 0; }

static void set_notice(const char *s) { snprintf(VS.notice, sizeof VS.notice, "%s", s); }

static void set_state(ink_voice_state_t s) {
    VS.state = s;
    VS.state_since_us = osal_now_us();
    ink_state_set_str(VS.h_state, STATE_NAME[s]);
    text_presenter_kick();
}

/* The end of what was heard, whole characters. */
static void update_shown(void) {
    size_t ni = strlen(VS.interim), total = VS.nwords + ni, skip = total > SHOWN_MAX ? total - SHOWN_MAX : 0;
    size_t n = 0;
    for (size_t i = skip; i < total; i++) {
        char c = i < VS.nwords ? VS.words[i] : VS.interim[i - VS.nwords];
        if (n || ((unsigned char)c & 0xC0) != 0x80) VS.shown[n++] = c;
    }
    VS.shown[n] = 0;
}

static void free_frames(void) {
    while (VS.head) {
        frame_t *f = VS.head;
        VS.head = f->next;
        osal_free(f);
    }
    VS.tail = NULL;
    VS.buffered = 0;
}

/* The audio task while a capture runs, this one after it: never both. */
static void flush_frames(void) {
    while (VS.head) {
        if (ink_rt_send(VS.sid, INK_RT_VOICE_UP, VS.head->data, VS.head->len, 0) != 0) break;
        frame_t *f = VS.head;
        VS.head = f->next;
        if (!VS.head) VS.tail = NULL;
        VS.buffered--;
        osal_free(f);
    }
}

static void capture_ended(void *arg);

static void on_frame(void *ctx, const uint8_t *data, size_t len, uint32_t seq) { /* audio task */
    (void)ctx;
    (void)seq;
    bool drop = __atomic_load_n(&VS.drop, __ATOMIC_ACQUIRE);
    if (!len) { /* the end: the microphone is about to be free. From here on the frames are the UI task's */
        ink_exec_post(INK_EXEC_UI, &VS.ended_job, capture_ended, NULL);
        return;
    }
    if (drop) return;
    if (ink_audio_capture_level() >= SPEECH_LEVEL) {
        __atomic_store_n(&VS.last_voice_ms, (uint32_t)(osal_now_us() / 1000u), __ATOMIC_RELEASE);
        __atomic_store_n(&VS.heard, true, __ATOMIC_RELEASE);
    }
    frame_t *f = osal_malloc(sizeof *f + len, OSAL_MEM_EXTERNAL);
    if (f) {
        f->next = NULL;
        f->len = len;
        memcpy(f->data, data, len);
        if (VS.tail) VS.tail->next = f;
        else VS.head = f;
        VS.tail = f;
        VS.buffered++;
    }
    if (ink_rt_ready(VS.sid)) flush_frames();
    while (VS.buffered > MAX_BUFFERED) { /* the oldest of what could not go */
        frame_t *old = VS.head;
        VS.head = old->next;
        VS.buffered--;
        osal_free(old);
        ink_probe_count("text.voice_frames_dropped");
    }
}

/* Over, one way or another. How: "done", "cancelled" or "failed". Nothing heard stays here. */
static void voice_end(const char *how) {
    if (VS.state == INK_VOICE_IDLE) return;
    __atomic_store_n(&VS.drop, true, __ATOMIC_RELEASE);
    if (VS.capturing && !VS.capture_ended_asked) {
        VS.capture_ended_asked = true;
        ink_audio_capture_end();
    }
    if (!VS.capturing) free_frames();
    if (VS.sid > 0) ink_rt_close(VS.sid);
    VS.sid = 0;
    ink_audio_focus_release(VS.focus);
    VS.focus = 0;
    ink_svc_cancel(&VS.tick_at);
    memset(VS.words, 0, VS.nwords);
    VS.nwords = 0;
    VS.next_seg = 0;
    memset(VS.interim, 0, sizeof VS.interim);
    VS.shown[0] = 0;
    set_state(INK_VOICE_IDLE);
    ink_probe_count("text.voice_ended");
    ink_args_t *a = osal_malloc(sizeof *a, OSAL_MEM_EXTERNAL);
    if (a) {
        ink_args_init(a);
        ink_args_str(a, "how", how);
        if (VS.notice[0]) ink_args_str(a, "why", VS.notice);
        ink_cap_emit("text.voice_ended", a);
        osal_free(a);
    }
}

/* What was heard goes into the field. A field for digits takes what a field for digits takes. */
static void commit_words(void) {
    if (!VS.nwords || !ink_text_focused()) return;
    ink_text_type_t t = ink_text_type();
    if (t == INK_TEXT_NUMBER || t == INK_TEXT_PHONE) {
        char *w = VS.words;
        size_t n = 0;
        for (size_t i = 0; i < VS.nwords; i++) {
            char c = w[i];
            bool ok = (c >= '0' && c <= '9') || (t == INK_TEXT_NUMBER ? c == '.' || c == '-' : c == '+' || c == '*' || c == '#');
            if (ok) w[n++] = c;
        }
        w[n] = 0;
        VS.nwords = n;
    }
    ink_text_commit_utf8(VS.words);
}

/* It cannot go on: what was final stays, the person hears why, the keyboard comes back. */
static void fail_with(const char *notice) {
    commit_words();
    set_notice(notice);
    ink_probe_count("text.voice_failed");
    voice_end("failed");
}

static void send_end_if_ready(void) {
    if (VS.capturing || VS.end_sent || !ink_rt_ready(VS.sid)) return;
    flush_frames();
    free_frames();
    VS.end_sent = true;
    ink_rt_control(VS.sid, "rt.turn", "{\"op\":\"end\"}");
}

static void capture_ended(void *arg) {
    (void)arg;
    VS.capturing = false;
    if (__atomic_load_n(&VS.drop, __ATOMIC_ACQUIRE)) free_frames();
    else if (VS.state == INK_VOICE_FINISHING) send_end_if_ready();
    ink_audio_capture_measure(false);
}

void ink_text_voice_finish(void) {
    if (VS.state != INK_VOICE_LISTENING && VS.state != INK_VOICE_CONNECTING) return;
    set_state(INK_VOICE_FINISHING);
    if (VS.capturing && !VS.capture_ended_asked) {
        VS.capture_ended_asked = true;
        ink_audio_capture_end();
    }
}

void ink_text_voice_cancel(void) {
    if (VS.state != INK_VOICE_IDLE) ink_probe_count("text.voice_cancelled");
    voice_end("cancelled");
}

void text_voice_on_blur(void) { voice_end("cancelled"); }

/* ---- what comes from the hub ---- */

static void ready_job(void *arg) {
    if ((int)(intptr_t)arg != VS.sid) return;
    if (VS.state == INK_VOICE_CONNECTING) set_state(INK_VOICE_LISTENING);
    else if (VS.state == INK_VOICE_FINISHING) send_end_if_ready();
}

static void lost_job(void *arg) {
    if ((int)(intptr_t)arg == VS.sid) fail_with("现在连不上，用键盘吧");
}

static void rt_ready(void *ctx, int sid) { /* hub task. Again means a new connection: dictation does not go on across one */
    (void)ctx;
    if (__atomic_add_fetch(&VS.readies, 1, __ATOMIC_ACQ_REL) > 1) ink_exec_post(INK_EXEC_UI, &VS.lost_job, lost_job, (void *)(intptr_t)sid);
    else ink_exec_post(INK_EXEC_UI, &VS.ready_job, ready_job, (void *)(intptr_t)sid);
}

static void result_job(void *arg) {
    result_t *r = arg;
    if (r->sid == VS.sid && VS.state != INK_VOICE_IDLE && r->seg >= VS.next_seg) { /* a later result replaces, a final one stays */
        if (r->final) {
            size_t n = strlen(r->text);
            if (VS.nwords + n > WORDS_MAX) {
                n = WORDS_MAX - VS.nwords;
                while (n && ((unsigned char)r->text[n] & 0xC0) == 0x80) n--;
            }
            memcpy(VS.words + VS.nwords, r->text, n);
            VS.nwords += n;
            VS.words[VS.nwords] = 0;
            VS.next_seg = r->seg + 1;
            VS.interim[0] = 0;
        } else snprintf(VS.interim, sizeof VS.interim, "%s", r->text);
        update_shown();
        text_presenter_kick();
    }
    memset(r->text, 0, sizeof r->text);
    osal_free(r);
}

static void done_job(void *arg) {
    if ((int)(intptr_t)arg != VS.sid) return;
    if (VS.nwords) {
        commit_words();
        ink_probe_count("text.voice_committed");
    } else set_notice("没听清");
    voice_end("done");
}

static void error_job(void *arg) {
    if ((int)(intptr_t)arg != VS.sid) return;
    const char *c = VS.error_code;
    fail_with(!strcmp(c, "budget") ? "今天的额度用完了，用键盘吧" : !strcmp(c, "not_allowed") ? "这位成员不能用语音输入" : !strcmp(c, "too_long") ? "一次说得太长了，先到这里" : "语音输入暂时用不了");
}

static void closed_job(void *arg) {
    if ((int)(intptr_t)arg == VS.sid) fail_with("语音输入暂时用不了");
}

static void rt_closed(void *ctx, int sid, int reason) { /* hub task: refused or ended by the hub */
    (void)ctx;
    (void)reason;
    ink_exec_post(INK_EXEC_UI, &VS.closed_job, closed_job, (void *)(intptr_t)sid);
}

static void rt_control(void *ctx, int sid, const char *method, const ink_json_t *j, int params) { /* hub task */
    (void)ctx;
    if (!strcmp(method, "rt.asr")) {
        result_t *r = osal_calloc(1, sizeof *r, OSAL_MEM_EXTERNAL); /* a job starts zeroed */
        if (!r) {
            ink_probe_count("text.voice_result_lost");
            return;
        }
        int64_t seg = 0;
        ink_json_int(j, ink_json_find(j, params, "seg"), &seg);
        r->sid = sid;
        r->seg = (int)seg;
        int final_t = ink_json_find(j, params, "final");
        r->final = final_t >= 0 && j->text[j->tok[final_t].start] == 't';
        if (ink_json_str(j, ink_json_find(j, params, "text"), r->text, sizeof r->text) < 0) r->text[0] = 0;
        if (ink_exec_post(INK_EXEC_UI, &r->job, result_job, r) != 0) osal_free(r);
    } else if (!strcmp(method, "rt.asr.done")) {
        ink_exec_post(INK_EXEC_UI, &VS.done_job, done_job, (void *)(intptr_t)sid);
    } else if (!strcmp(method, "rt.asr.error")) {
        ink_svc_lock();
        if (ink_json_str(j, ink_json_find(j, params, "code"), VS.error_code, sizeof VS.error_code) < 0) VS.error_code[0] = 0;
        ink_svc_unlock();
        ink_exec_post(INK_EXEC_UI, &VS.error_job, error_job, (void *)(intptr_t)sid);
    }
}

static const ink_rt_handler_t RT_HANDLER = {.on_control = rt_control, .on_closed = rt_closed, .on_ready = rt_ready};

/* ---- going on ---- */

static void tick_due(void *arg);

static void arm_tick(void) { ink_svc_at(&VS.tick_at, osal_now_us() + TICK_US, tick_due, NULL); }

static void tick_job(void *arg) {
    (void)arg;
    if (VS.state == INK_VOICE_IDLE) return;
    uint64_t now = osal_now_us();
    uint32_t in_state_ms = (uint32_t)((now - VS.state_since_us) / 1000u);
    ink_power_activity(now); /* talking to it is using it */
    switch (VS.state) {
    case INK_VOICE_CONNECTING:
        ink_hub_demand();
        if (in_state_ms >= CONNECT_MS) {
            fail_with("现在连不上，用键盘吧");
            return;
        }
        break;
    case INK_VOICE_LISTENING: {
        if (!ink_hub_online()) { /* a new connection would announce the session again: dictation does not go on across one */
            fail_with("现在连不上，用键盘吧");
            return;
        }
        bool heard = __atomic_load_n(&VS.heard, __ATOMIC_ACQUIRE);
        uint32_t quiet_ms = (uint32_t)(now / 1000u) - __atomic_load_n(&VS.last_voice_ms, __ATOMIC_ACQUIRE);
        if ((heard && quiet_ms >= PAUSE_MS) || (!heard && in_state_ms >= NOTHING_SAID_MS)) {
            ink_probe_count(heard ? "text.voice_pause_ended" : "text.voice_nothing_said");
            ink_text_voice_finish();
        }
        break;
    }
    case INK_VOICE_FINISHING:
        if (!ink_hub_online()) {
            fail_with("现在连不上，用键盘吧");
            return;
        }
        if (in_state_ms >= FINISH_MS) {
            fail_with("语音输入暂时用不了");
            return;
        }
        break;
    default: break;
    }
    arm_tick();
}

static void tick_due(void *arg) { /* the timer task */
    (void)arg;
    ink_exec_post(INK_EXEC_UI, &VS.tick_job, tick_job, NULL);
}

static void on_focus(void *ctx, ink_focus_event_t ev, ink_audio_prio_t by) { /* UI task: a call, an alarm: it is theirs now */
    (void)by;
    if (ev == INK_FOCUS_BACK || (int)(intptr_t)ctx != VS.focus_gen) return; /* not an earlier dictation's */
    VS.focus = 0;
    ink_text_voice_cancel();
}

/* 0, or -1 and a notice (none where there is simply no such thing: no field, a password). */
int ink_text_voice_start(void) {
    if (!ink_text_focused() || ink_text_type() == INK_TEXT_PASSWORD || !ink_audio_has_mic() || VS.state != INK_VOICE_IDLE) return -1;
    text_voice_clear_notice();
    if (ink_audio_recording()) {
        set_notice("正在录音，先停了再说");
        text_presenter_kick();
        return -1;
    }
    if (VS.capturing || ink_audio_capture_busy()) { /* the AI is listening, or the last capture has not ended yet */
        set_notice("麦克风正忙，请稍后再试");
        text_presenter_kick();
        return -1;
    }
    int focus = ink_audio_focus_acquire(INK_AUDIO_PRIO_VOICE, on_focus, (void *)(intptr_t)++VS.focus_gen, INK_EXEC_UI);
    if (focus <= 0) {
        set_notice("现在不能用语音输入");
        text_presenter_kick();
        return -1;
    }
    ink_text_type_t t = ink_text_type();
    char params[INK_RT_PARAMS];
    snprintf(params, sizeof params, PARAMS, t == INK_TEXT_NUMBER || t == INK_TEXT_PHONE ? "number" : "text");
    __atomic_store_n(&VS.readies, 0, __ATOMIC_RELEASE);
    __atomic_store_n(&VS.heard, false, __ATOMIC_RELEASE);
    __atomic_store_n(&VS.drop, false, __ATOMIC_RELEASE);
    ink_hub_demand(); /* connected already while the person starts to speak */
    int sid = ink_rt_open(ASR_CAP, params, &RT_HANDLER);
    if (sid <= 0 || !ink_rt_is_open(sid)) {
        ink_audio_focus_release(focus);
        set_notice("现在连不上，用键盘吧");
        text_presenter_kick();
        return -1;
    }
    VS.sid = sid;
    VS.focus = focus;
    VS.capture_ended_asked = VS.end_sent = false;
    VS.interim[0] = VS.shown[0] = 0;
    ink_audio_capture_measure(true);
    if (ink_audio_capture_begin(on_frame, NULL) != 0) {
        ink_audio_capture_measure(false);
        ink_rt_close(sid);
        VS.sid = 0;
        ink_audio_focus_release(focus);
        VS.focus = 0;
        set_notice("麦克风没能打开，请稍后再试");
        text_presenter_kick();
        return -1;
    }
    VS.capturing = true;
    ink_probe_count("text.voice_started");
    ink_audio_tone(INK_TONE_LISTEN, osal_now_us(), "text.voice_tap_to_tone_heard_ms");
    set_state(ink_rt_ready(sid) ? INK_VOICE_LISTENING : INK_VOICE_CONNECTING);
    arm_tick();
    return 0;
}

static void on_power_state(void *ctx, int h, const char *id) { /* UI task: locked is over */
    (void)ctx;
    (void)id;
    char s[16] = "";
    ink_state_get_str(h, s, sizeof s);
    if (!strcmp(s, "locked")) ink_text_voice_cancel();
}

static const ink_state_def_t ST_VOICE = {.id = "text.voice", .type = INK_ST_STR, .title = "语音输入（idle、connecting、listening、finishing）"};

int text_voice_init(void) {
    VS.h_state = ink_state_define(&ST_VOICE);
    ink_state_set_str(VS.h_state, STATE_NAME[0]);
    return ink_state_watch("power.state", on_power_state, NULL, INK_EXEC_UI);
}
