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
#define FRAME HAL_OPUS_FRAME_SAMPLES

typedef struct {
    uint8_t *data;
    uint16_t len;
} jb_frame_t;

static struct {
    osal_mutex_t *lock;
    osal_sem_t *wake;
    osal_task_t *task;
    hal_audio_info_t info;
    int volume; /* 0..100 */
    /* capture */
    bool cap_on, cap_ending;
    ink_audio_frame_fn cap_fn;
    void *cap_ctx;
    int16_t *ring; /* CAP_RING samples, PSRAM */
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
    /* tone request */
    bool tone_pending;
    ink_tone_t tone;
    uint64_t tone_cause;
    const char *tone_probe;
    const char *heard_probe; /* set while the staged samples are the start of a probed tone/stream */
    const char *stream_probe; /* the current stream's "first sample heard" probe (until used) */
    uint64_t stream_cause;
    uint64_t heard_cause;
    bool holding_writes;
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
    if (AU.cap_on && !AU.cap_ending) {
        if (lost > 0) ink_probe_value("audio.capture_gap_ms", lost * 1000.0 / HAL_AUDIO_RATE);
        for (int pass = 0; pass < 2; pass++) {
            int n = pass ? samples : (lost > CAP_RING / 4 ? CAP_RING / 4 : lost);
            for (int i = 0; i < n; i++) {
                if (AU.wr - AU.rd >= CAP_RING) {
                    AU.rd++; /* ring full (the encoder fell 2 s behind): drop the oldest */
                    ink_probe_count("audio.capture_overflow_samples");
                }
                AU.ring[AU.wr++ % CAP_RING] = pass ? mic[i] : 0;
            }
        }
    }
    unlock();
    osal_sem_give(AU.wake);
}

static bool take_frame(int16_t *out) { /* under lock */
    if (AU.wr - AU.rd < FRAME) return false;
    for (int i = 0; i < FRAME; i++) out[i] = AU.ring[AU.rd++ % CAP_RING];
    return true;
}

static void encode_and_deliver(const int16_t *frame) {
    uint8_t pkt[HAL_OPUS_MAX_PACKET];
    int n = hal_opus_encode(AU.enc, frame, pkt, sizeof pkt);
    if (n > 0 && AU.cap_fn) AU.cap_fn(AU.cap_ctx, pkt, (size_t)n, AU.cap_seq++);
}

/* Encode one frame, or finish an ending capture. Returns true if there may be more to encode. */
static bool capture_step(int16_t *scratch) {
    lock();
    bool on = AU.cap_on, ending = AU.cap_ending, got = on && take_frame(scratch);
    uint32_t left = AU.wr - AU.rd;
    if (!got && on && ending) { /* flush the tail (padded) and signal the end */
        for (int i = 0; i < FRAME; i++) scratch[i] = i < (int)left ? AU.ring[AU.rd++ % CAP_RING] : 0;
    }
    unlock();
    if (got) {
        encode_and_deliver(scratch);
        return true;
    }
    if (on && ending) {
        if (left) encode_and_deliver(scratch);
        hal_audio_capture_stop();
        if (AU.cap_fn) AU.cap_fn(AU.cap_ctx, NULL, 0, AU.cap_seq);
        lock();
        AU.cap_on = AU.cap_ending = false;
        AU.cap_fn = NULL;
        unlock();
        hal_opus_enc_free(AU.enc);
        AU.enc = NULL;
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

static void service_playback(void) {
    lock();
    bool tone = AU.tone_pending;
    ink_tone_t which = AU.tone;
    const char *probe = AU.tone_probe;
    uint64_t cause = AU.tone_cause;
    AU.tone_pending = false;
    if (AU.staged_stream && AU.staged_stream != AU.stream) { /* aborted or replaced: drop its staged audio */
        AU.pcm_off = AU.pcm_len = 0;
        AU.staged_stream = 0;
    }
    unlock();
    if (tone && open_output()) {
        make_tone(which);
        AU.heard_probe = probe;
        AU.heard_cause = cause;
    }
    for (;;) {
        if (AU.pcm_off >= AU.pcm_len && !stage_next()) break; /* nothing ready */
        if (!open_output()) {
            AU.pcm_off = AU.pcm_len = 0;
            break;
        }
        AU.waiting_since = 0;
        if (AU.heard_probe) { /* first samples of a probed tone go out now: when will they be heard? */
            int64_t delay = hal_audio_play_delay_us();
            ink_probe_value(AU.heard_probe, (double)(osal_now_us() + (uint64_t)delay - AU.heard_cause) / 1000.0);
            AU.heard_probe = NULL;
        }
        int w = hal_audio_play_write(AU.pcm + AU.pcm_off, AU.pcm_len - AU.pcm_off, 0);
        AU.pcm_off += w;
        if (AU.pcm_off < AU.pcm_len) break; /* DMA ring full: come back next tick */
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
    bool codec = AU.cap_on || AU.stream;
    unlock();
    if (want != AU.holding_writes) {
        AU.holding_writes = want;
        ink_storage_hold_writes(want);
        if (want) ink_power_hold(INK_NEED_AUDIO);
        else ink_power_release(INK_NEED_AUDIO);
    }
    if (codec != AU.boosting) {
        AU.boosting = codec;
        if (!AU.boost) AU.boost = hal_cpu_boost_create("audio");
        if (codec) hal_cpu_boost_acquire(AU.boost);
        else hal_cpu_boost_release(AU.boost);
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
        bool active = AU.cap_on || AU.stream || AU.tone_pending || AU.out_open || AU.pcm_off < AU.pcm_len;
        unlock();
        osal_sem_take(AU.wake, active ? AU_TICK_US : OSAL_WAIT_FOREVER);
        /* the speaker first and again after every encoded frame: the 60 ms DMA ring must not wait
         * for the encoder to catch up on the capture ring */
        service_playback();
        while (capture_step(scratch)) service_playback();
        update_write_hold();
    }
}

/* ----------------------------------------------------------------- API */

int ink_audio_init(void) {
    hal_audio_get_info(&AU.info);
    AU.volume = 70;
    const char *prev = osal_mem_tag("audio");
    AU.lock = osal_mutex_create();
    AU.wake = osal_sem_create(0, 1);
    AU.ring = osal_malloc(CAP_RING * sizeof(int16_t), OSAL_MEM_EXTERNAL);
    AU.jb = osal_calloc(JB_MAX, sizeof(jb_frame_t), OSAL_MEM_EXTERNAL);
    AU.pcm = osal_malloc(FRAME * 4 * sizeof(int16_t), OSAL_MEM_EXTERNAL);
    osal_mem_tag(prev);
    if (!AU.lock || !AU.wake || !AU.ring || !AU.jb || !AU.pcm) return -1;
    if (!AU.info.has_speaker && !AU.info.has_mic) return 0;
    osal_task_cfg_t cfg = {.name = "audio", .fn = audio_task, .stack_bytes = AU_STACK, .priority = AU_PRIO, .core = -1,
                           .flags = OSAL_TASK_STACK_EXTERNAL};
    return osal_task_create(&cfg, &AU.task) == OSAL_OK ? 0 : -1;
}

bool ink_audio_has_speaker(void) { return AU.info.has_speaker; }
bool ink_audio_has_mic(void) { return AU.info.has_mic; }

int ink_audio_capture_begin(ink_audio_frame_fn fn, void *ctx) {
    if (!AU.info.has_mic || !AU.task) return -1;
    lock();
    bool busy = AU.cap_on;
    unlock();
    if (busy) return -1;
    hal_opus_enc_t *enc = hal_opus_enc_new(BITRATE, 0);
    if (!enc) return -1;
    lock();
    AU.enc = enc;
    AU.cap_fn = fn;
    AU.cap_ctx = ctx;
    AU.cap_seq = 0;
    AU.rd = AU.wr = 0;
    AU.cap_on = true;
    AU.cap_ending = false;
    unlock();
    if (hal_audio_capture_start(capture_cb, NULL) != 0) {
        lock();
        AU.cap_on = false;
        AU.cap_fn = NULL;
        AU.enc = NULL;
        unlock();
        hal_opus_enc_free(enc);
        return -1;
    }
    osal_sem_give(AU.wake);
    return 0;
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
