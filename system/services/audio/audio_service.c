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
#include "ink/sys/members.h"
#include "ink/hal_afe.h"
#include "ink/sys/settings.h"
#include "ink/sys/state.h"
#include "ink/sys/power.h"
#include "ink/sys/storage.h"

#define AU_STACK 26624 /* Opus encoder/decoder (upstream opus task: 26 KB, PSRAM) */
#define AU_PRIO 7
#define AU_TICK_US 10000
#define CAP_RING (HAL_AUDIO_RATE * 2) /* 2 s of PCM */
#define MAX_FOCUS 6
#define JB_MAX 512                    /* jitter buffer: frames (30 s; hubs send TTS faster than real time) */
#define JB_PREBUFFER 5                /* 300 ms before a stream starts playing: over the pessimistic LTE profile 180 ms cut into the
                                       * answer in 2 of 30 random seeds (the frames after the first come late), 300 ms in 0 of 100 */
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
    bool cap_local; /* it stays on the device: a recording */
    bool measure;
    int level;
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
    bool ring_on;    /* a call or an alarm rings */
    uint32_t ring_t; /* samples since it began */
    int ring_focus;
    struct {
        int token;
        ink_audio_prio_t prio;
        ink_focus_fn fn;
        void *ctx;
        ink_exec_t exec;
        bool paused;
    } focus[MAX_FOCUS];
    int next_focus;
    uint32_t stream_played; /* real frames of the stream on staging */
    int last_stream;
    uint32_t last_played;
    ink_audio_route_t route, route_want;
    int child_max;
    bool holding_writes, holding_awake;
    bool boosting;
    hal_cpu_boost_t *boost;
} AU;
static bool g_capture_revoked INK_PSRAM_BSS; /* under AU.lock; a handoff cannot be undone by switching back */

static void lock(void) { osal_mutex_lock(AU.lock, OSAL_WAIT_FOREVER); }
static void unlock(void) { osal_mutex_unlock(AU.lock); }

/* Under the lock: what is left of a stream when it goes. */
static void stream_gone(void) {
    if (!AU.stream) return;
    AU.last_stream = AU.stream;
    AU.last_played = AU.stream_played;
    AU.stream = 0;
}

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
static int focus_top_locked(void);

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
    if (__atomic_load_n(&AU.measure, __ATOMIC_RELAXED)) {
        uint32_t sum = 0;
        for (int i = 0; i < FRAME; i += 4) sum += (uint32_t)(frame[i] < 0 ? -(int32_t)frame[i] : frame[i]);
        osal_cost_units("audio.mix_sample", FRAME / 4);
        __atomic_store_n(&AU.level, (int)(sum / (FRAME / 4)), __ATOMIC_RELAXED);
    }
    uint8_t pkt[HAL_OPUS_MAX_PACKET];
    int n = hal_opus_encode(AU.enc, frame, pkt, sizeof pkt);
    lock();
    bool allowed = !g_capture_revoked && focus_top_locked() < INK_AUDIO_PRIO_ALERT;
    unlock();
    if (allowed && n > 0 && AU.cap_fn) AU.cap_fn(AU.cap_ctx, pkt, (size_t)n, AU.cap_seq++);
}

/* Encode one frame, or finish an ending capture. Returns true if there may be more to encode. */
static bool capture_step(int16_t *scratch) {
    lock();
    bool on = AU.cap_on, ending = AU.cap_ending, listen = AU.listen_on;
    bool cut = on && ending && (g_capture_revoked || focus_top_locked() >= INK_AUDIO_PRIO_ALERT);
    bool got = !cut && (on || listen) && take_frame(scratch);
    uint32_t left = cut ? 0 : AU.wr - AU.rd;
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

int ink_audio_effective_volume(void) {
    const ink_member_t *m = ink_member_current();
    int v = AU.volume, top = AU.child_max > 0 ? AU.child_max : 100;
    return m && m->role == INK_ROLE_CHILD && v > top ? top : v;
}

static void apply_volume(int16_t *pcm, int n) {
    int v = ink_audio_effective_volume(), g = v * v; /* perceptual: (v/100)^2, as upstream */
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

#define RING_LEVEL 7000       /* a fixed level: an alarm must be heard with the volume down */
#define RING_CHUNK (FRAME * 2) /* 120 ms at a time */

/* Two tones, on and off: 400 ms of each, 800 ms of quiet. Service task. */
static void make_ring(void) {
    for (int i = 0; i < RING_CHUNK; i++, AU.ring_t++) {
        uint32_t ms = AU.ring_t / (HAL_AUDIO_RATE / 1000) % 1600;
        double f = ms < 400 ? 880.0 : ms < 800 ? 660.0 : 0.0;
        AU.pcm[i] = f > 0 ? (int16_t)(RING_LEVEL * sin(2 * M_PI * f * AU.ring_t / HAL_AUDIO_RATE)) : 0;
    }
    osal_cost_units("tone.sample", RING_CHUNK);
    AU.pcm_off = 0;
    AU.pcm_len = RING_CHUNK;
    AU.staged_stream = 0;
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
        AU.stream_played++;
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
    stream_gone();
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
        if (AU.pcm_off >= AU.pcm_len) { /* nothing staged: the ringing, a tone, else the stream's next frame when it can play */
            lock();
            bool ring = AU.ring_on;
            unlock();
            if (ring) {
                if (!open_output()) break; /* tried again next tick */
                make_ring();
            } else {
                if (take_tone()) continue;
                if (!stream_wants_output() || !output_for_stream() || !stage_next()) break;
            }
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
    bool idle = !AU.stream && drained && !AU.tone_pending && !AU.ring_on;
    if (done_stream) {
        stream_gone();
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
    bool want = AU.cap_on || AU.stream || AU.out_open || AU.ring_on;
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

static int h_playing = -1, h_listening = -1, h_route = -1, h_mic = -1;

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
static const ink_state_def_t ST_LISTENING = {.id = "audio.listening", .type = INK_ST_BOOL, .title = "在等人叫名字"};
static const ink_state_def_t ST_MIC = {.id = "audio.mic", .type = INK_ST_STR, .title = "麦克风（off 关着、local 只在设备上等人叫名字、sending 声音在送出去、recording 在录音）"};
static const ink_state_def_t ST_ROUTE = {.id = "audio.route", .type = INK_ST_STR, .title = "声音从哪儿出来（speaker、bt_headset）"};

static void publish_playing(void) { /* audio task */
    static const char *const NAME[] = {"none", "story", "voice", "alert", "call"};
    static const char *last;
    lock();
    const char *now = AU.stream && (unsigned)AU.prio < 5 ? NAME[AU.prio] : NAME[0];
    unlock();
    if (now != last) {
        ink_state_set_str(h_playing, now);
        last = now;
    }
}

/* What the microphone is doing, for the status bar and the AI (system UI spec §3.6). Only here, so that the last word is the true one. */
static void publish_mic(void) { /* audio task */
    static const char *last;
    lock();
    const char *now = AU.cap_on ? (AU.cap_local ? "recording" : "sending") : AU.listen_on ? "local" : "off";
    unlock();
    if (now != last) {
        ink_state_set_str(h_mic, now);
        last = now;
    }
}

/* The chip changes mode in this task: the sound stops for that long. Not during a call. */
static void apply_route(void) { /* audio task */
    lock();
    ink_audio_route_t want = AU.route_want;
    bool go = want != AU.route;
    for (int i = 0; i < MAX_FOCUS && go; i++)
        if (AU.focus[i].token && AU.focus[i].prio == INK_AUDIO_PRIO_CALL) go = false;
    unlock();
    if (!go) return;
    if (AU.out_open) close_output(false);
    if (hal_audio_route_set(want == INK_ROUTE_HEADSET ? HAL_ROUTE_BT_HEADSET : HAL_ROUTE_SPEAKER) != 0) {
        ink_probe_count("audio.route_failed");
        lock();
        AU.route_want = AU.route; /* it stays as it was */
        unlock();
        return;
    }
    lock();
    if (want == INK_ROUTE_SPEAKER && AU.stream && AU.prio == INK_AUDIO_PRIO_STORY) {
        free_jb();
        stream_gone();
        AU.finishing = AU.started = false;
        AU.pcm_off = AU.pcm_len = 0;
    }
    AU.route = want;
    unlock();
    ink_state_set_str(h_route, want == INK_ROUTE_HEADSET ? "bt_headset" : "speaker");
    ink_probe_count("audio.route_changes");
}

static void audio_task(void *arg) {
    (void)arg;
    const char *prev = osal_mem_tag("audio");
    int16_t *scratch = osal_malloc(FRAME * sizeof(int16_t), OSAL_MEM_EXTERNAL);
    osal_mem_tag(prev);
    if (!scratch) osal_panic("audio: no memory for the capture scratch buffer");
    for (;;) {
        lock();
        bool active = AU.cap_on || AU.listen_on || AU.stream || AU.tone_pending || AU.out_open || AU.pcm_off < AU.pcm_len || AU.ring_on;
        unlock();
        osal_sem_take(AU.wake, active ? AU_TICK_US : OSAL_WAIT_FOREVER);
        apply_route();
        /* the speaker first and again after every encoded frame: the 60 ms DMA ring must not wait
         * for the encoder to catch up on the capture ring */
        service_playback();
        while (capture_step(scratch)) service_playback();
        update_write_hold();
        publish_playing();
        afe_transition();
        publish_mic();
    }
}

/* ----------------------------------------------------------------- API */

static void child_max_apply(void *ctx, int64_t v) {
    (void)ctx;
    AU.child_max = (int)v;
}

static void volume_apply(void *ctx, int64_t v) {
    (void)ctx;
    ink_audio_set_volume((int)v);
}

static void on_capture_owner(void *ctx, int h, const char *id) { /* direct: the UI and media queues may be busy */
    (void)ctx;
    (void)h;
    bool handoff = !strcmp(id, "member.current"), closed = !ink_member_space_open();
    if (!handoff && strcmp(id, "member.authed")) return;
    lock();
    if (AU.cap_on && (handoff || (closed && !AU.cap_local))) {
        g_capture_revoked = true;
        AU.cap_ending = true;
        AU.rd = AU.wr;
    }
    unlock();
    osal_sem_give(AU.wake);
}

/* When the device listens for its wake word (D-52: a setting, by default only while the screen is
 * on and unlocked). Takes effect with the voice front end (detailed design §7). */
static const char *const HANDSFREE[] = {"screen_on", "always", "off", NULL};

static const ink_setting_def_t AUDIO_SETTINGS[] = {
    {.id = "audio.volume", .title = "音量", .type = INK_SET_INT, .min = 0, .max = 100, .def = 70, .child_may_change = true,
     .apply = volume_apply, .apply_exec = INK_EXEC_DIRECT},
    {.id = "voice.handsfree", .title = "叫名字（什么时候在等）", .type = INK_SET_ENUM, .def = 0, .choices = HANDSFREE},
    {.id = "audio.child_max", .title = "孩子的音量上限", .type = INK_SET_INT, .min = 10, .max = 100, .def = 60, .apply = child_max_apply, .apply_exec = INK_EXEC_DIRECT},
};

int ink_audio_init(void) {
    hal_audio_get_info(&AU.info);
    /* the saved volume; 70 where the settings table is not running (some test programs) */
    AU.volume = 70;
    AU.child_max = 60;
    if (ink_settings_define(AUDIO_SETTINGS, 3) == 0) {
        AU.volume = (int)ink_setting_get("audio.volume");
        AU.child_max = (int)ink_setting_get("audio.child_max");
    }
    h_route = ink_state_define(&ST_ROUTE);
    ink_state_set_str(h_route, "speaker");
    h_playing = ink_state_define(&ST_PLAYING);
    h_listening = ink_state_define(&ST_LISTENING);
    h_mic = ink_state_define(&ST_MIC);
    ink_state_set_str(h_mic, "off");
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
    if (ink_state_watch("member.", on_capture_owner, NULL, INK_EXEC_DIRECT) != 0) return -1;
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

static int capture_begin_locked(ink_audio_frame_fn fn, void *ctx, bool local) { /* under mic_lock */
    lock();
    bool busy = AU.cap_on;
    unlock();
    if (busy) return -1;
    hal_opus_enc_t *enc = hal_opus_enc_new(BITRATE, 0);
    if (!enc) return -1;
    lock();
    if (focus_top_locked() >= INK_AUDIO_PRIO_ALERT) {
        unlock();
        hal_opus_enc_free(enc);
        return -1;
    }
    bool listening = AU.listen_on; /* the microphone already runs: the capture starts from now */
    AU.enc = enc;
    AU.cap_fn = fn;
    AU.cap_ctx = ctx;
    AU.cap_seq = 0;
    if (!listening) AU.rd = AU.wr = 0;
    AU.cap_on = true;
    AU.cap_local = local;
    AU.cap_ending = false;
    g_capture_revoked = false;
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

static int capture_begin(ink_audio_frame_fn fn, void *ctx, bool local) {
    if (!AU.info.has_mic || !AU.task) return -1;
    osal_mutex_lock(AU.mic_lock, OSAL_WAIT_FOREVER);
    int r = capture_begin_locked(fn, ctx, local);
    osal_mutex_unlock(AU.mic_lock);
    if (r == 0) osal_sem_give(AU.wake);
    return r;
}

int ink_audio_capture_begin(ink_audio_frame_fn fn, void *ctx) { return capture_begin(fn, ctx, false); }
int ink_audio_capture_begin_local(ink_audio_frame_fn fn, void *ctx) { return capture_begin(fn, ctx, true); }

bool ink_audio_capture_busy(void) {
    if (!AU.lock) return false;
    lock();
    bool on = AU.cap_on;
    unlock();
    return on;
}

void ink_audio_capture_measure(bool on) { __atomic_store_n(&AU.measure, on, __ATOMIC_RELAXED); }

int ink_audio_capture_level(void) { return __atomic_load_n(&AU.level, __ATOMIC_RELAXED); }

void ink_audio_capture_end(void) {
    lock();
    if (AU.cap_on) AU.cap_ending = true;
    unlock();
    osal_sem_give(AU.wake);
}

/* ---- focus ---- */

static int focus_top_locked(void) {
    int top = 0;
    for (int i = 0; i < MAX_FOCUS; i++)
        if (AU.focus[i].token && (int)AU.focus[i].prio > top) top = (int)AU.focus[i].prio;
    return top;
}

int ink_audio_focus_top(void) {
    if (!AU.lock) return 0;
    lock();
    int t = focus_top_locked();
    unlock();
    return t;
}

typedef struct {
    ink_exec_job_t job;
    ink_focus_fn fn;
    void *ctx;
    ink_focus_event_t ev;
    ink_audio_prio_t by;
} focus_note_t;

static void focus_job(void *arg) {
    focus_note_t *n = arg;
    n->fn(n->ctx, n->ev, n->by);
    osal_free(n);
}

static void focus_tell(ink_focus_fn fn, void *ctx, ink_exec_t exec, ink_focus_event_t ev, ink_audio_prio_t by) {
    if (!fn) return;
    focus_note_t *n = osal_malloc(sizeof *n, OSAL_MEM_EXTERNAL);
    if (!n) {
        ink_probe_count("audio.focus_note_lost");
        return;
    }
    *n = (focus_note_t){.fn = fn, .ctx = ctx, .ev = ev, .by = by};
    if (ink_exec_post(exec, &n->job, focus_job, n) != 0) osal_free(n);
}

int ink_audio_focus_acquire(ink_audio_prio_t prio, ink_focus_fn fn, void *ctx, ink_exec_t exec) {
    if (!AU.lock) return -1;
    struct {
        ink_focus_fn fn;
        void *ctx;
        ink_exec_t exec;
        ink_focus_event_t ev;
    } tell[MAX_FOCUS];
    int ntell = 0, token = -1, free_at = -1;
    bool abort_stream = false;
    lock();
    int top = focus_top_locked();
    if (top > (int)prio && prio != INK_AUDIO_PRIO_STORY) { /* speech and recording do not wait under something higher */
        unlock();
        return -1;
    }
    for (int i = 0; i < MAX_FOCUS; i++) {
        typeof(AU.focus[0]) *h = &AU.focus[i];
        if (!h->token) {
            if (free_at < 0) free_at = i;
            continue;
        }
        if (h->prio == prio) { /* the newer takes over */
            tell[ntell++] = (typeof(tell[0])){h->fn, h->ctx, h->exec, INK_FOCUS_LOST};
            h->token = 0;
            if (free_at < 0) free_at = i;
        } else if (h->prio < prio) {
            if (h->prio == INK_AUDIO_PRIO_STORY || (h->prio == INK_AUDIO_PRIO_ALERT && prio == INK_AUDIO_PRIO_CALL)) {
                if (!h->paused) {
                    h->paused = true;
                    tell[ntell++] = (typeof(tell[0])){h->fn, h->ctx, h->exec, INK_FOCUS_PAUSED};
                }
            } else {
                tell[ntell++] = (typeof(tell[0])){h->fn, h->ctx, h->exec, INK_FOCUS_LOST};
                h->token = 0;
                if (free_at < 0) free_at = i;
            }
        }
    }
    if (free_at >= 0) {
        typeof(AU.focus[0]) *h = &AU.focus[free_at];
        token = ++AU.next_focus;
        *h = (typeof(*h)){.token = token, .prio = prio, .fn = fn, .ctx = ctx, .exec = exec, .paused = top > (int)prio};
    }
    if (token > 0 && prio >= INK_AUDIO_PRIO_ALERT && AU.cap_on) {
        AU.cap_ending = true; /* The audio task stops capture even if the owner's executor is busy. */
        AU.rd = AU.wr;
    }
    if (prio >= INK_AUDIO_PRIO_ALERT && AU.stream && AU.prio < prio) abort_stream = true; /* what is said ends: the words are on the screen */
    bool starts_paused = token > 0 && top > (int)prio;
    ink_audio_prio_t by = (ink_audio_prio_t)top;
    if (abort_stream) {
        free_jb();
        stream_gone();
        AU.finishing = AU.started = false;
    }
    unlock();
    for (int i = 0; i < ntell; i++) focus_tell(tell[i].fn, tell[i].ctx, tell[i].exec, tell[i].ev, prio);
    if (starts_paused) focus_tell(fn, ctx, exec, INK_FOCUS_PAUSED, by);
    if (abort_stream || (token > 0 && prio >= INK_AUDIO_PRIO_ALERT)) osal_sem_give(AU.wake);
    if (token < 0) ink_probe_count("audio.focus_table_full");
    return token;
}

void ink_audio_focus_release(int token) {
    if (!AU.lock || token <= 0) return;
    struct {
        ink_focus_fn fn;
        void *ctx;
        ink_exec_t exec;
    } tell[MAX_FOCUS];
    int ntell = 0;
    lock();
    bool hit = false;
    for (int i = 0; i < MAX_FOCUS; i++)
        if (AU.focus[i].token == token) {
            AU.focus[i].token = 0;
            hit = true;
        }
    int top = focus_top_locked();
    if (hit)
        for (int i = 0; i < MAX_FOCUS; i++) {
            typeof(AU.focus[0]) *h = &AU.focus[i];
            if (h->token && h->paused && (int)h->prio >= top) {
                h->paused = false;
                tell[ntell++] = (typeof(tell[0])){h->fn, h->ctx, h->exec};
            }
        }
    unlock();
    for (int i = 0; i < ntell; i++) focus_tell(tell[i].fn, tell[i].ctx, tell[i].exec, INK_FOCUS_BACK, (ink_audio_prio_t)top);
    osal_sem_give(AU.wake); /* a route may have been waiting for the call to end */
}

int ink_audio_stream_progress(int id, uint32_t *played, uint32_t *queued) {
    lock();
    int r = -1;
    if (id == AU.stream && id) {
        if (played) *played = AU.stream_played;
        if (queued) *queued = (uint32_t)AU.jb_count;
        r = 0;
    } else if (id == AU.last_stream) {
        if (played) *played = AU.last_played;
        if (queued) *queued = 0;
    }
    unlock();
    return r;
}

void ink_audio_set_route(ink_audio_route_t route) {
    lock();
    AU.route_want = route;
    unlock();
    osal_sem_give(AU.wake);
}

ink_audio_route_t ink_audio_route(void) {
    lock();
    ink_audio_route_t r = AU.route;
    unlock();
    return r;
}

int ink_audio_stream_open(ink_audio_prio_t prio, uint64_t cause_us, const char *heard_probe) {
    if (!AU.info.has_speaker || !AU.task) return -1;
    lock();
    if ((AU.stream && AU.prio > prio) || focus_top_locked() > (int)prio) {
        unlock();
        return -1;
    }
    free_jb();
    stream_gone();
    AU.stream = ++AU.next_stream;
    AU.stream_played = 0;
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
        stream_gone();
        AU.finishing = AU.started = false;
    }
    unlock();
    osal_sem_give(AU.wake);
}

void ink_audio_stop_all(void) { /* what is being said; a book being listened to has its own controls and pauses by focus */
    lock();
    if (AU.stream && AU.prio != INK_AUDIO_PRIO_STORY) {
        free_jb();
        stream_gone();
        AU.finishing = AU.started = false;
    }
    unlock();
    osal_sem_give(AU.wake);
}

void ink_audio_tone(ink_tone_t tone, uint64_t cause_us, const char *probe) {
    if (!AU.info.has_speaker || !AU.task) return;
    lock();
    if (focus_top_locked() >= INK_AUDIO_PRIO_ALERT) { /* nothing is said over a call or an alarm */
        unlock();
        ink_probe_count("audio.tones_dropped");
        return;
    }
    AU.tone_pending = true;
    AU.tone = tone;
    AU.tone_cause = cause_us;
    AU.tone_probe = probe;
    unlock();
    osal_sem_give(AU.wake);
}

void ink_audio_ring(bool on) {
    if (!AU.info.has_speaker || !AU.task) return;
    lock();
    bool was = AU.ring_on;
    if (on && !was) AU.ring_t = 0;
    AU.ring_on = on;
    int token = AU.ring_focus;
    if (!on) AU.ring_focus = 0;
    unlock();
    if (on && !was) {
        int t = ink_audio_focus_acquire(INK_AUDIO_PRIO_ALERT, NULL, NULL, INK_EXEC_DIRECT);
        lock();
        AU.ring_focus = t > 0 ? t : 0;
        unlock();
    } else if (!on && token) ink_audio_focus_release(token);
    osal_sem_give(AU.wake);
}

void ink_audio_set_volume(int percent) { AU.volume = percent < 0 ? 0 : percent > 100 ? 100 : percent; }

void ink_audio_on_stream_failed(ink_audio_fail_fn fn, void *ctx) {
    lock();
    AU.fail_fn = fn;
    AU.fail_ctx = ctx;
    unlock();
}
