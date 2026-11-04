/*
 * Audio service (see ink/sys/audio.h).
 *
 * Threads: the platform's audio task delivers microphone chunks (capture
 * callback: copy into a PSRAM ring, wake us); everything else — encoding,
 * decoding, tones, keeping the speaker's DMA ring fed — happens in the
 * service task. API calls from other tasks only touch state under the lock.
 *
 * Timing: the speaker ring holds 60 ms, exactly one Opus frame, so decoded
 * audio waits in a staging buffer and is written as DMA descriptors free up;
 * the task wakes at least every tick (10 ms) while anything is active. During
 * an e-ink refresh the EPD feeders hold most of both cores (~7 % left): the
 * capture ring (2 s) and the jitter buffer absorb that, the speaker is
 * refilled between encoded frames (one frame is ~9 ms of CPU, ~140 ms of wall
 * time at that share), and every tone fits in the DMA ring, so it is written
 * at once and no load can cut it.
 *
 * An open output with nothing to play is left alone: the DMA plays zeros by
 * itself (the TX channel clears what it sent). While a stream waits for data
 * the output stays open for a while, then closes.
 *
 * An output that cannot open (the audio chip unpowered, e.g. right after a wake
 * whose supply is still being restored): a stream's frames stay queued and the
 * open is tried again, backing off to once a second; after OPEN_GIVE_UP_US the
 * stream fails, its frames are dropped and its owner is told
 * (ink_audio_on_stream_failed). A tone that cannot play is dropped. An open
 * output that stops taking samples for STALL_US lost its clock (the chip lost
 * its supply or mode under the stream): it is closed and opened again the same
 * way, which sets the chip up again; the give-up time counts from the first
 * stall until samples flow again. What was already in the DMA ring then (about
 * one ring, 60-70 ms) is lost, and counted (audio.stall_lost_ms).
 *
 * The staging buffer belongs to the service task; the API only changes the
 * stream and the task drops staged audio of a stream that is gone.
 */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ink/hal_audio.h"
#include "ink/hal_codec.h"
#include "ink/hal_power.h"
#include "ink/osal.h"
#include "ink/probe.h"
#include "ink/sys/audio.h"
#include "ink/hal_afe.h"
#include "ink/sys/settings.h"
#include "ink/sys/state.h"
#include "ink/sys/power.h"
#include "ink/sys/storage.h"

#define AU_STACK 26624 /* Opus encoder/decoder (upstream opus task: 26 KB, PSRAM) */
#define AU_PRIO 7
#define AU_TICK_US 10000
#define CAP_RING (HAL_AUDIO_RATE * 2) /* 2 s of PCM */
#define JB_MAX 512                    /* jitter buffer: frames (30 s; hubs send TTS faster than real time) */
#define JB_PREBUFFER 3                /* 180 ms before a stream starts playing */
#define PLC_MAX 3                     /* conceal at most this many missing frames in a row */
#define BITRATE 24000
#define WAIT_HOLD_US 300000 /* a stream waiting for data keeps the output open this long, then it closes */
#define OPEN_GIVE_UP_US 8000000 /* a stream whose output cannot open (or play) for this long fails */
#define STALL_US 1000000        /* an open output that takes nothing this long has lost its clock (the ring drains in 60 ms) */
#define FRAME HAL_OPUS_FRAME_SAMPLES

typedef struct {
    uint8_t *data;
    uint16_t len;
} jb_frame_t;

static struct {
    osal_mutex_t *lock;
    osal_mutex_t *mic_lock; /* who wants the microphone and whether it runs change together: held over the HAL's start and stop (never under `lock`) */
    osal_sem_t *wake;
    osal_task_t *task;
    hal_audio_info_t info;
    int volume; /* 0..100 */
    /* capture */
    bool cap_on, cap_ending;
    ink_audio_frame_fn cap_fn;
    void *cap_ctx;
    int16_t *ring; /* CAP_RING samples, PSRAM */
    /* hands-free listening (R-047): the microphone stays open for the voice front end */
    bool listen_on, afe_speech;
    bool afe_on; /* audio task: the front end is started */
    ink_audio_listen_fn listen_fn;
    void *listen_ctx;
    int16_t *ref_ring;  /* CAP_RING samples of what the speaker played, PSRAM (allocated on first listen) */
    int16_t *ref_frame; /* FRAME samples */
    uint32_t rd, wr; /* sample counters (mod CAP_RING when indexing) */
    uint32_t cap_seq;
    hal_opus_enc_t *enc;
    /* playback */
    int stream, next_stream;
    ink_audio_prio_t prio;
    bool finishing, started;
    jb_frame_t *jb; /* JB_MAX, PSRAM */
    int jb_head, jb_count, plc_run;
    hal_opus_dec_t *dec;
    int dec_stream; /* the stream the decoder state belongs to */
    int16_t *pcm; /* staging: one decoded frame or a tone (PSRAM); service task only */
    int pcm_off, pcm_len;
    int staged_stream; /* stream the staged samples belong to (0 = a tone) */
    bool out_open;
    uint64_t waiting_since; /* output open, the stream has nothing ready since (0 = not waiting) */
    uint64_t open_failing_since, open_retry_at; /* the output did not open or play for the stream (0 = it plays) */
    uint64_t stall_since;                       /* the open output stopped taking samples (0 = it takes them) */
    unsigned open_tries;
    ink_audio_fail_fn fail_fn; /* told when a stream fails */
    void *fail_ctx;
    /* tone request */
    bool tone_pending;
    ink_tone_t tone;
    uint64_t tone_cause;
    const char *tone_probe;
    const char *heard_probe; /* set while the staged samples are the start of a probed tone/stream */
    const char *stream_probe; /* the current stream's "first sample heard" probe (until used) */
    uint64_t stream_cause;
    uint64_t heard_cause;
    bool holding_writes, holding_awake;
    bool boosting;
    hal_cpu_boost_t *boost;
} AU;

static void lock(void) { osal_mutex_lock(AU.lock, OSAL_WAIT_FOREVER); }
static void unlock(void) { osal_mutex_unlock(AU.lock); }

/* --------------------------------------------------------------- capture */

/* Platform audio task: copy the chunk; samples the platform lost become silence so timing holds. */
static void capture_cb(void *ctx, const int16_t *mic, const int16_t *ref, int samples, int lost, uint64_t t_us) {
    (void)ctx;
    (void)ref;
    (void)t_us;
    lock();
    if ((AU.cap_on && !AU.cap_ending) || AU.listen_on) {
        if (lost > 0) ink_probe_value("audio.capture_gap_ms", lost * 1000.0 / HAL_AUDIO_RATE);
        bool with_ref = AU.listen_on && AU.ref_ring;
        for (int pass = 0; pass < 2; pass++) {
            int n = pass ? samples : (lost > CAP_RING / 4 ? CAP_RING / 4 : lost);
            for (int i = 0; i < n; i++) {
                if (AU.wr - AU.rd >= CAP_RING) {
                    AU.rd++; /* ring full (the encoder fell 2 s behind): drop the oldest */
                    ink_probe_count("audio.capture_overflow_samples");
                }
                if (with_ref) AU.ref_ring[AU.wr % CAP_RING] = pass && ref ? ref[i] : 0;
                AU.ring[AU.wr++ % CAP_RING] = pass ? mic[i] : 0;
            }
        }
    }
    unlock();
    osal_sem_give(AU.wake);
}

static bool take_frame(int16_t *out) { /* under lock */
    if (AU.wr - AU.rd < FRAME) return false;
    bool ref = AU.listen_on && AU.ref_ring;
    for (int i = 0; i < FRAME; i++) {
        if (ref) AU.ref_frame[i] = AU.ref_ring[AU.rd % CAP_RING];
        out[i] = AU.ring[AU.rd++ % CAP_RING];
    }
    return true;
}

static void close_output(bool drain);
static void free_jb(void);

static int g_last_command = -1;

static void listen_event(ink_listen_ev_t ev, uint64_t t_us) {
    if (AU.listen_fn) AU.listen_fn(AU.listen_ctx, ev, t_us);
}

/* One frame through the voice front end: wake word, speech edges, barge-in. Audio task. */
static void afe_step(const int16_t *frame) {
    hal_afe_result_t r;
    uint64_t now = osal_now_us();
    hal_afe_feed(frame, AU.ref_frame, FRAME, now, &r);
    if (r.wake) listen_event(INK_LISTEN_WAKE, now);
    if (r.command >= 0) {
        __atomic_store_n(&g_last_command, r.command, __ATOMIC_RELEASE);
        listen_event(INK_LISTEN_COMMAND, now);
    }
    if (r.speech && !AU.afe_speech) {
        lock();
        bool answer = AU.stream && AU.prio == INK_AUDIO_PRIO_VOICE; /* an answer is playing: speech interrupts it */
        if (answer) {
            free_jb();
            AU.stream = 0;
            AU.finishing = AU.started = false;
        }
        unlock();
        if (answer) {
            close_output(false); /* stop now, not after the DMA ring drained */
            ink_probe_value("ai.barge_to_stop_ms", (double)(osal_now_us() - r.speech_since_us) / 1000.0);
            listen_event(INK_LISTEN_BARGE, r.speech_since_us);
        } else listen_event(INK_LISTEN_SPEECH, r.speech_since_us);
    } else if (!r.speech && AU.afe_speech) listen_event(INK_LISTEN_QUIET, now);
    AU.afe_speech = r.speech;
}

static void encode_and_deliver(const int16_t *frame) {
    uint8_t pkt[HAL_OPUS_MAX_PACKET];
    int n = hal_opus_encode(AU.enc, frame, pkt, sizeof pkt);
    if (n > 0 && AU.cap_fn) AU.cap_fn(AU.cap_ctx, pkt, (size_t)n, AU.cap_seq++);
}

/* Encode one frame, or finish an ending capture. Returns true if there may be more to encode. */
static bool capture_step(int16_t *scratch) {
    lock();
    bool on = AU.cap_on, ending = AU.cap_ending, listen = AU.listen_on, got = (on || listen) && take_frame(scratch);
    uint32_t left = AU.wr - AU.rd;
    if (!got && on && ending && !listen) { /* flush the tail (padded) and signal the end */
        for (int i = 0; i < FRAME; i++) scratch[i] = i < (int)left ? AU.ring[AU.rd++ % CAP_RING] : 0;
    }
    unlock();
    if (got && listen && AU.afe_on) afe_step(scratch); /* afe_on: set in this task, once it started */
    if (got && on) encode_and_deliver(scratch);
    if (got && !(on && ending && listen)) return true; /* listening: the capture ends right here */
    if (on && ending) {
        if (!got && !listen && left) encode_and_deliver(scratch);
        /* Taken over here, then cleaned up, then the owner told; only after that is the capture over:
         * a new one cannot begin before this one's end was delivered, nor share anything with it. */
        lock();
        hal_opus_enc_t *enc = AU.enc;
        ink_audio_frame_fn fn = AU.cap_fn;
        void *ctx = AU.cap_ctx;
        uint32_t seq = AU.cap_seq;
        AU.enc = NULL;
        AU.cap_fn = NULL;
        unlock();
        hal_opus_enc_free(enc);
        if (fn) fn(ctx, NULL, 0, seq);
        osal_mutex_lock(AU.mic_lock, OSAL_WAIT_FOREVER);
        lock();
        bool keep_mic = AU.listen_on; /* listening relies on this microphone */
        AU.cap_on = AU.cap_ending = false;
        unlock();
        if (!keep_mic) hal_audio_capture_stop();
        osal_mutex_unlock(AU.mic_lock);
    }
    return false;
}

/* -------------------------------------------------------------- playback */

static void apply_volume(int16_t *pcm, int n) {
    int g = AU.volume * AU.volume; /* perceptual: (v/100)^2, as upstream */
    for (int i = 0; i < n; i++) pcm[i] = (int16_t)((int32_t)pcm[i] * g / 10000);
    osal_cost_units("audio.mix_sample", (uint64_t)n);
}

static bool open_output(void) {
    if (AU.out_open) return true;
    if (hal_audio_play_start() != 0) return false;
    AU.out_open = true;
    return true;
}

static void close_output(bool drain) {
    if (!AU.out_open) return;
    hal_audio_play_stop(drain);
    AU.out_open = false;
}

static void make_tone(ink_tone_t t) {
    /* placeholders until the sound design; each fits in the DMA ring (60 ms here), see the top */
    static const struct { double f; int ms; } T[] = {{880, 60}, {660, 60}, {330, 60}};
    int n = HAL_AUDIO_RATE * T[t].ms / 1000, fade = HAL_AUDIO_RATE * 6 / 1000;
    int ring = AU.info.runway_ms > 0 ? HAL_AUDIO_RATE * AU.info.runway_ms / 1000 : n;
    if (n > ring) n = ring;
    if (n > FRAME * 4) n = FRAME * 4;
    for (int i = 0; i < n; i++) {
        double env = i < fade ? (double)i / fade : i > n - fade ? (double)(n - i) / fade : 1.0;
        AU.pcm[i] = (int16_t)(9000.0 * env * sin(2 * M_PI * T[t].f * i / HAL_AUDIO_RATE));
    }
    osal_cost_units("tone.sample", (uint64_t)n);
    AU.pcm_off = 0;
    AU.pcm_len = n;
    AU.staged_stream = 0;
    apply_volume(AU.pcm, n);
}

static void free_jb(void) { /* under lock */
    while (AU.jb_count) {
        osal_free(AU.jb[AU.jb_head].data);
        AU.jb_head = (AU.jb_head + 1) % JB_MAX;
        AU.jb_count--;
    }
}

/* Fill the staging buffer with the next decoded frame. Returns false if nothing to play yet. */
static bool stage_next(void) {
    lock();
    if (!AU.stream) {
        unlock();
        return false;
    }
    if (!AU.started && AU.jb_count < JB_PREBUFFER && !AU.finishing) {
        unlock();
        return false;
    }
    AU.started = true;
    int sid = AU.stream;
    jb_frame_t f = {0};
    bool have = AU.jb_count > 0;
    if (have) {
        f = AU.jb[AU.jb_head];
        AU.jb_head = (AU.jb_head + 1) % JB_MAX;
        AU.jb_count--;
        AU.plc_run = 0;
    } else if (AU.finishing || AU.plc_run >= PLC_MAX) {
        unlock();
        return false;
    } else {
        AU.plc_run++;
        ink_probe_count("audio.play_concealed_frames"); /* the network did not deliver in time */
    }
    unlock();
    if (AU.dec && AU.dec_stream != sid) { /* a new stream starts from a fresh decoder state */
        hal_opus_dec_free(AU.dec);
        AU.dec = NULL;
    }
    if (!AU.dec) {
        AU.dec = hal_opus_dec_new();
        AU.dec_stream = sid;
    }
    int n = AU.dec ? hal_opus_decode(AU.dec, have ? f.data : NULL, have ? f.len : 0, AU.pcm) : -1;
    osal_free(f.data);
    if (n <= 0) return false;
    apply_volume(AU.pcm, n);
    AU.pcm_off = 0;
    AU.pcm_len = n;
    AU.staged_stream = sid;
    lock();
    if (AU.stream_probe && have) { /* first real frame of the stream is about to go out */
        AU.heard_probe = AU.stream_probe;
        AU.heard_cause = AU.stream_cause;
        AU.stream_probe = NULL;
    }
    unlock();
    return true;
}

/* Would the stream play now if the output were open? (stage_next's conditions, nothing consumed) */
static bool stream_wants_output(void) {
    lock();
    bool want = AU.stream && (AU.started ? AU.jb_count > 0 || (!AU.finishing && AU.plc_run < PLC_MAX)
                                         : AU.jb_count >= JB_PREBUFFER || (AU.finishing && AU.jb_count > 0));
    unlock();
    return want;
}

/* The stream could not play for OPEN_GIVE_UP_US: it fails, its frames go, its owner is told. */
static void stream_failed(void) {
    lock();
    int id = AU.stream;
    free_jb();
    AU.stream = 0;
    AU.finishing = AU.started = false;
    ink_audio_fail_fn fn = AU.fail_fn;
    void *ctx = AU.fail_ctx;
    unlock();
    AU.open_failing_since = AU.open_retry_at = AU.stall_since = 0;
    AU.open_tries = 0;
    if (AU.staged_stream == id) AU.pcm_off = AU.pcm_len = 0;
    ink_probe_count("audio.play_failed");
    if (fn && id) fn(ctx, id);
}

/* The output for the stream: open, or tried again later (the frames wait); after OPEN_GIVE_UP_US of
 * trouble the stream fails. Service task. */
static bool output_for_stream(void) {
    if (AU.out_open) return true;
    uint64_t now = osal_now_us();
    if (now < AU.open_retry_at) return false;
    if (open_output()) { /* the trouble ends when samples flow again (service_playback) */
        AU.open_retry_at = 0;
        AU.open_tries = 0;
        return true;
    }
    if (!AU.open_failing_since) AU.open_failing_since = now;
    uint64_t gap = 100000ull << (AU.open_tries < 4 ? AU.open_tries : 4);
    AU.open_retry_at = now + (gap > 1000000 ? 1000000 : gap);
    AU.open_tries++;
    ink_probe_count("audio.output_open_failures");
    if (now - AU.open_failing_since >= OPEN_GIVE_UP_US) stream_failed();
    return false;
}

/* The open output took nothing for STALL_US although its ring drains every 60 ms: its clock is gone.
 * Closed and opened again (output_for_stream), which sets the audio chip up again; a stream's staged
 * samples wait, a tone's are dropped. Service task. */
static void output_stalled(void) {
    uint64_t now = osal_now_us();
    if (!AU.stall_since) {
        AU.stall_since = now;
        return;
    }
    if (now - AU.stall_since < STALL_US) return;
    ink_probe_count("audio.output_stalls");
    ink_probe_value("audio.stall_lost_ms", hal_audio_play_delay_us() / 1000.0); /* queued in the ring, never heard */
    AU.stall_since = 0;
    close_output(false);
    if (!AU.staged_stream) AU.pcm_off = AU.pcm_len = 0;
    if (!AU.open_failing_since) AU.open_failing_since = now;
    AU.open_retry_at = now + 100000;
    if (now - AU.open_failing_since >= OPEN_GIVE_UP_US) stream_failed();
}

/* Is the staged audio still wanted? A stream aborted or replaced meanwhile (also while an open blocked
 * in the loop below) loses its staged samples. Service task. */
static bool staged_wanted(void) {
    lock();
    bool wanted = !AU.staged_stream || AU.staged_stream == AU.stream;
    if (!wanted) {
        AU.pcm_off = AU.pcm_len = 0;
        AU.staged_stream = 0;
    }
    unlock();
    return wanted;
}

/* The staging buffer is free: a pending tone takes it, ahead of the stream's next frame (a frame
 * staged there always goes out first, so the tone waits at most one frame). True if it did. */
static bool take_tone(void) {
    lock();
    bool tone = AU.tone_pending;
    ink_tone_t which = AU.tone;
    const char *probe = AU.tone_probe;
    uint64_t cause = AU.tone_cause;
    AU.tone_pending = false;
    unlock();
    if (!tone) return false;
    if (!open_output()) {
        ink_probe_count("audio.tones_dropped"); /* a prompt played late would mislead */
        return false;
    }
    make_tone(which);
    AU.heard_probe = probe;
    AU.heard_cause = cause;
    return true;
}

static void service_playback(void) {
    (void)staged_wanted();
    for (;;) {
        if (AU.pcm_off >= AU.pcm_len) { /* nothing staged: a tone first, else the stream's next frame when it can play */
            if (take_tone()) continue;
            if (!stream_wants_output() || !output_for_stream() || !stage_next()) break;
        } else if (!output_for_stream()) {
            break; /* staged samples wait for the output */
        } else if (!staged_wanted()) {
            continue; /* the open blocked and the stream went meanwhile */
        }
        AU.waiting_since = 0;
        if (AU.heard_probe) { /* first samples of a probed tone go out now: when will they be heard? */
            int64_t delay = hal_audio_play_delay_us();
            ink_probe_value(AU.heard_probe, (double)(osal_now_us() + (uint64_t)delay - AU.heard_cause) / 1000.0);
            AU.heard_probe = NULL;
        }
        int w = hal_audio_play_write(AU.pcm + AU.pcm_off, AU.pcm_len - AU.pcm_off, 0);
        AU.pcm_off += w;
        if (w > 0) AU.stall_since = AU.open_failing_since = 0; /* it plays */
        if (AU.pcm_off < AU.pcm_len) { /* DMA ring full: come back next tick */
            if (!w) output_stalled();
            break;
        }
    }
    lock();
    bool drained = AU.pcm_off >= AU.pcm_len;
    bool done_stream = AU.stream && AU.finishing && AU.jb_count == 0 && drained;
    bool idle = !AU.stream && drained && !AU.tone_pending;
    if (done_stream) {
        AU.stream = 0;
        AU.finishing = AU.started = false;
    }
    bool no_stream = !AU.stream;
    unlock();
    if (done_stream || (idle && AU.out_open)) {
        AU.waiting_since = 0;
        close_output(true); /* play out what is queued */
    } else if (AU.out_open && drained) {
        /* the stream waits for data (prebuffering, a network gap): the DMA plays zeros by itself */
        uint64_t now = osal_now_us();
        if (!AU.waiting_since) AU.waiting_since = now;
        else if (now - AU.waiting_since > WAIT_HOLD_US) {
            AU.waiting_since = 0;
            close_output(false); /* everything queued has long been played */
        }
    }
    if (no_stream && AU.dec) { /* finished or aborted: give the decoder's memory back */
        hal_opus_dec_free(AU.dec);
        AU.dec = NULL;
    }
}

/* ---------------------------------------------------------------- task */

/* While playing or recording: flash writes wait (they would freeze the DMA refill), the chip stays
 * awake (also when the device is locked: an audiobook keeps playing) and the codec runs at full clock. */
static void update_write_hold(void) {
    lock();
    bool want = AU.cap_on || AU.stream || AU.out_open;
    bool awake = want || AU.listen_on; /* listening keeps the chip up but lets the flash be written (a gap, not a stop) */
    bool codec = AU.cap_on || AU.stream || AU.listen_on;
    unlock();
    if (want != AU.holding_writes) {
        AU.holding_writes = want;
        ink_storage_hold_writes(want);
    }
    if (awake != AU.holding_awake) {
        AU.holding_awake = awake;
        if (awake) ink_power_hold(INK_NEED_AUDIO);
        else ink_power_release(INK_NEED_AUDIO);
    }
    if (codec != AU.boosting) {
        AU.boosting = codec;
        if (!AU.boost) AU.boost = hal_cpu_boost_create("audio");
        if (codec) hal_cpu_boost_acquire(AU.boost);
        else hal_cpu_boost_release(AU.boost);
    }
}

static int h_playing = -1, h_listening = -1;

/* The front end starts and stops in this task (hal_afe.h); a start that fails ends the listening. */
static void afe_transition(void) {
    lock();
    bool want = AU.listen_on;
    unlock();
    if (want == AU.afe_on) return;
    if (want && hal_afe_start() != 0) {
        lock();
        AU.listen_on = false;
        bool mic = AU.cap_on;
        unlock();
        if (!mic) hal_audio_capture_stop();
        ink_probe_count("audio.listen_failed");
        ink_state_set_bool(h_listening, false);
        return;
    }
    if (!want) hal_afe_stop();
    AU.afe_on = want;
}

/* System state: what plays (published on change only), and whether the device listens (hands-free, step 6). */
static const ink_state_def_t ST_PLAYING = {.id = "audio.now_playing", .type = INK_ST_STR, .title = "正在播放"};
static const ink_state_def_t ST_LISTENING = {.id = "audio.listening", .type = INK_ST_BOOL, .title = "在听（免按键）"};

static void publish_playing(void) { /* audio task */
    static const char *const NAME[] = {"none", "story", "voice", "alert"};
    static const char *last;
    lock();
    const char *now = AU.stream && (unsigned)AU.prio < 4 ? NAME[AU.prio] : NAME[0];
    unlock();
    if (now != last) {
        ink_state_set_str(h_playing, now);
        last = now;
    }
}

static void audio_task(void *arg) {
    (void)arg;
    const char *prev = osal_mem_tag("audio");
    int16_t *scratch = osal_malloc(FRAME * sizeof(int16_t), OSAL_MEM_EXTERNAL);
    osal_mem_tag(prev);
    if (!scratch) osal_panic("audio: no memory for the capture scratch buffer");
    for (;;) {
        lock();
        bool active = AU.cap_on || AU.listen_on || AU.stream || AU.tone_pending || AU.out_open || AU.pcm_off < AU.pcm_len;
        unlock();
        osal_sem_take(AU.wake, active ? AU_TICK_US : OSAL_WAIT_FOREVER);
        /* the speaker first and again after every encoded frame: the 60 ms DMA ring must not wait
         * for the encoder to catch up on the capture ring */
        service_playback();
        while (capture_step(scratch)) service_playback();
        update_write_hold();
        publish_playing();
        afe_transition();
    }
}

/* ----------------------------------------------------------------- API */

static void volume_apply(void *ctx, int64_t v) {
    (void)ctx;
    ink_audio_set_volume((int)v);
}

/* When the device listens for its wake word (D-52: a setting, by default only while the screen is
 * on and unlocked). Takes effect with the voice front end (detailed design §7). */
static const char *const HANDSFREE[] = {"screen_on", "always", "off", NULL};

static const ink_setting_def_t AUDIO_SETTINGS[] = {
    {.id = "audio.volume", .title = "音量", .type = INK_SET_INT, .min = 0, .max = 100, .def = 70, .child_may_change = true,
     .apply = volume_apply, .apply_exec = INK_EXEC_DIRECT},
    {.id = "voice.handsfree", .title = "免按键（什么时候在听）", .type = INK_SET_ENUM, .def = 0, .choices = HANDSFREE},
};

int ink_audio_init(void) {
    hal_audio_get_info(&AU.info);
    /* the saved volume; 70 where the settings table is not running (some test programs) */
    AU.volume = ink_settings_define(AUDIO_SETTINGS, 2) == 0 ? (int)ink_setting_get("audio.volume") : 70;
    h_playing = ink_state_define(&ST_PLAYING);
    h_listening = ink_state_define(&ST_LISTENING);
    ink_state_set_str(h_playing, "none");
    ink_state_set_bool(h_listening, false);
    const char *prev = osal_mem_tag("audio");
    AU.lock = osal_mutex_create();
    AU.mic_lock = osal_mutex_create();
    AU.wake = osal_sem_create(0, 1);
    AU.ring = osal_malloc(CAP_RING * sizeof(int16_t), OSAL_MEM_EXTERNAL);
    AU.jb = osal_calloc(JB_MAX, sizeof(jb_frame_t), OSAL_MEM_EXTERNAL);
    AU.pcm = osal_malloc(FRAME * 4 * sizeof(int16_t), OSAL_MEM_EXTERNAL);
    osal_mem_tag(prev);
    if (!AU.lock || !AU.mic_lock || !AU.wake || !AU.ring || !AU.jb || !AU.pcm) return -1;
    if (!AU.info.has_speaker && !AU.info.has_mic) return 0;
    osal_task_cfg_t cfg = {.name = "audio", .fn = audio_task, .stack_bytes = AU_STACK, .priority = AU_PRIO, .core = -1,
                           .flags = OSAL_TASK_STACK_EXTERNAL};
    return osal_task_create(&cfg, &AU.task) == OSAL_OK ? 0 : -1;
}

int ink_audio_listen_commands(const char *const *phrases, int n) { return hal_afe_set_commands(phrases, n); }
int ink_audio_last_command(void) { return __atomic_load_n(&g_last_command, __ATOMIC_ACQUIRE); }

bool ink_audio_can_listen(void) { return AU.info.has_mic && AU.task && hal_afe_available(); }

/* 1 changed, 0 nothing to do, -1 failed. Under mic_lock. */
static int listen_locked(bool on, ink_audio_listen_fn fn, void *ctx) {
    lock();
    bool was = AU.listen_on, mic = AU.cap_on;
    unlock();
    if (on == was) return 0;
    if (on) {
        if (!AU.ref_ring) {
            const char *prev = osal_mem_tag("audio");
            AU.ref_ring = osal_malloc(CAP_RING * sizeof(int16_t), OSAL_MEM_EXTERNAL);
            AU.ref_frame = osal_malloc(FRAME * sizeof(int16_t), OSAL_MEM_EXTERNAL);
            osal_mem_tag(prev);
        }
        if (!AU.ref_ring || !AU.ref_frame) return -1;
        lock();
        AU.listen_fn = fn;
        AU.listen_ctx = ctx;
        AU.afe_speech = false;
        if (!mic) AU.rd = AU.wr = 0;
        AU.listen_on = true;
        unlock();
        if (!mic && hal_audio_capture_start(capture_cb, NULL) != 0) {
            lock();
            AU.listen_on = false;
            unlock();
            return -1;
        }
    } else {
        lock();
        AU.listen_on = false;
        unlock();
        if (!mic) hal_audio_capture_stop();
    }
    return 1;
}

int ink_audio_listen(bool on, ink_audio_listen_fn fn, void *ctx) {
    if (!ink_audio_can_listen()) return -1;
    osal_mutex_lock(AU.mic_lock, OSAL_WAIT_FOREVER);
    int r = listen_locked(on, fn, ctx);
    osal_mutex_unlock(AU.mic_lock);
    if (r <= 0) return r;
    ink_state_set_bool(h_listening, on);
    ink_probe_count(on ? "audio.listen_on" : "audio.listen_off");
    osal_sem_give(AU.wake);
    return 0;
}

bool ink_audio_has_speaker(void) { return AU.info.has_speaker; }
bool ink_audio_has_mic(void) { return AU.info.has_mic; }

static int capture_begin_locked(ink_audio_frame_fn fn, void *ctx) { /* under mic_lock */
    lock();
    bool busy = AU.cap_on;
    unlock();
    if (busy) return -1;
    hal_opus_enc_t *enc = hal_opus_enc_new(BITRATE, 0);
    if (!enc) return -1;
    lock();
    bool listening = AU.listen_on; /* the microphone already runs: the capture starts from now */
    AU.enc = enc;
    AU.cap_fn = fn;
    AU.cap_ctx = ctx;
    AU.cap_seq = 0;
    if (!listening) AU.rd = AU.wr = 0;
    AU.cap_on = true;
    AU.cap_ending = false;
    unlock();
    if (!listening && hal_audio_capture_start(capture_cb, NULL) != 0) {
        lock();
        AU.cap_on = false;
        AU.cap_fn = NULL;
        AU.enc = NULL;
        unlock();
        hal_opus_enc_free(enc);
        return -1;
    }
    return 0;
}

int ink_audio_capture_begin(ink_audio_frame_fn fn, void *ctx) {
    if (!AU.info.has_mic || !AU.task) return -1;
    osal_mutex_lock(AU.mic_lock, OSAL_WAIT_FOREVER);
    int r = capture_begin_locked(fn, ctx);
    osal_mutex_unlock(AU.mic_lock);
    if (r == 0) osal_sem_give(AU.wake);
    return r;
}

void ink_audio_capture_end(void) {
    lock();
    if (AU.cap_on) AU.cap_ending = true;
    unlock();
    osal_sem_give(AU.wake);
}

int ink_audio_stream_open(ink_audio_prio_t prio, uint64_t cause_us, const char *heard_probe) {
    if (!AU.info.has_speaker || !AU.task) return -1;
    lock();
    if (AU.stream && AU.prio > prio) {
        unlock();
        return -1;
    }
    free_jb();
    AU.stream = ++AU.next_stream;
    AU.prio = prio;
    AU.open_failing_since = AU.open_retry_at = 0; /* a new stream tries the output at once */
    AU.open_tries = 0;
    AU.finishing = AU.started = false;
    AU.plc_run = 0;
    AU.stream_probe = heard_probe;
    AU.stream_cause = cause_us;
    int id = AU.stream;
    unlock();
    osal_sem_give(AU.wake);
    return id;
}

int ink_audio_stream_push(int id, const uint8_t *opus, size_t len) {
    if (!len || len > 0xffff) return -1;
    const char *prev = osal_mem_tag("audio");
    uint8_t *copy = osal_malloc(len, OSAL_MEM_EXTERNAL);
    osal_mem_tag(prev);
    if (!copy) return -1;
    memcpy(copy, opus, len);
    lock();
    if (AU.stream != id || AU.finishing || AU.jb_count >= JB_MAX) {
        unlock();
        osal_free(copy);
        if (AU.jb_count >= JB_MAX) ink_probe_count("audio.jitter_overflow");
        return -1;
    }
    AU.jb[(AU.jb_head + AU.jb_count) % JB_MAX] = (jb_frame_t){.data = copy, .len = (uint16_t)len};
    AU.jb_count++;
    unlock();
    osal_sem_give(AU.wake);
    return 0;
}

void ink_audio_stream_finish(int id) {
    lock();
    if (AU.stream == id) AU.finishing = true;
    unlock();
    osal_sem_give(AU.wake);
}

void ink_audio_stream_abort(int id) {
    lock();
    if (AU.stream == id) {
        free_jb();
        AU.stream = 0;
        AU.finishing = AU.started = false;
    }
    unlock();
    osal_sem_give(AU.wake);
}

void ink_audio_stop_all(void) {
    lock();
    free_jb();
    AU.stream = 0;
    AU.finishing = AU.started = false;
    unlock();
    osal_sem_give(AU.wake);
}

void ink_audio_tone(ink_tone_t tone, uint64_t cause_us, const char *probe) {
    if (!AU.info.has_speaker || !AU.task) return;
    lock();
    AU.tone_pending = true;
    AU.tone = tone;
    AU.tone_cause = cause_us;
    AU.tone_probe = probe;
    unlock();
    osal_sem_give(AU.wake);
}

void ink_audio_set_volume(int percent) { AU.volume = percent < 0 ? 0 : percent > 100 ? 100 : percent; }

void ink_audio_on_stream_failed(ink_audio_fail_fn fn, void *ctx) {
    lock();
    AU.fail_fn = fn;
    AU.fail_ctx = ctx;
    unlock();
}
