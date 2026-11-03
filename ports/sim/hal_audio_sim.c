/*
 * Audio model: I2S DMA rings at 16 kHz (the BH1098G clocks I2S on the Plus).
 *
 * Microphone: the hardware completes one DMA descriptor every chunk / 16 kHz
 * (240 frames = 15 ms) into a ring of N descriptors (60 ms). A platform task
 * drains it, converts the 32-bit stereo slots to 16-bit mono (+ reference)
 * and calls the system. If the task falls more than N descriptors behind —
 * the scheduler was suspended for a flash operation, or higher-priority work
 * (the EPD feeders during a refresh) starved it — the oldest audio is
 * overwritten: reported to the system as `lost` and counted.
 *
 * Speaker: writers fill descriptors; once the output path is up (amplifier
 * enable over the shared I2C + settle) the DMA plays one descriptor per
 * period. A descriptor nobody refilled plays as zeros (the TX channel is
 * configured to clear what it sent, as the ESP-IDF driver's auto_clear), so
 * an open output with nothing to play costs no CPU and is silent. A dry
 * descriptor is an underrun only when it cuts into sound (the samples played
 * just before were not silent): the gap lasts until data comes again and is
 * counted with its length; dry time after silence is only "idle". What the
 * speaker played, gaps included, goes to speaker.wav (16 kHz mono) on the
 * run's timeline.
 *
 * The DMA rings come out of DMA-capable internal RAM while a direction is
 * open (upstream tears the I2S channel down when the last user releases it).
 */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ink/hal_audio.h"
#include "ink/hal_power.h"
#include "board/metalio_board.h"
#include "sim.h"

typedef enum { MIC_SPEECH = 0, MIC_SILENCE, MIC_NOISE, MIC_HOST } mic_src_t;

static struct {
    bool init;
    int chunk, N;
    uint64_t period_ns;
    size_t ring_bytes; /* one direction: N descriptors x chunk frames x slot bytes */
    osal_mutex_t *cb_lock;
    osal_task_t *task;
    /* microphone */
    bool cap_on;
    hal_audio_capture_cb cb;
    void *ctx;
    uint64_t cap_t0;            /* start of descriptor 0 */
    uint64_t produced, consumed; /* descriptors */
    int lost_pending;           /* samples dropped since the last delivered chunk */
    vtk_event_id_t cap_ev;
    vtk_waitq_t cap_wq;
    void *rx_dma;
    int16_t *mic, *ref;
    mic_src_t src;
    uint64_t cap_lost_samples, cap_samples, cap_overruns;
    /* speaker */
    bool out_on;
    uint64_t ready_ns;          /* output path usable from */
    bool dma_running, draining;
    uint64_t boundary_ns;       /* end of the descriptor playing now (or start of the first) */
    int16_t *slots;             /* N x chunk */
    int tail, full, partial;    /* next to play; complete descriptors queued; samples in the one being filled */
    vtk_event_id_t play_ev;
    vtk_waitq_t play_wq;
    void *tx_dma;
    bool sounding, in_gap;      /* the last samples played were not silent; a gap that cut sound is going on */
    uint64_t underruns, underrun_samples, idle_samples, played_samples;
    /* speaker.wav */
    FILE *wav;
    uint64_t wav_samples, wav_end_ns;
} A;

/* ------------------------------------------------------------ speaker.wav */

static void wav_header(FILE *f, uint32_t samples) {
    uint32_t data = samples * 2, rate = HAL_AUDIO_RATE, byte_rate = rate * 2;
    uint8_t h[44] = {'R', 'I', 'F', 'F', 0, 0, 0, 0, 'W', 'A', 'V', 'E', 'f', 'm', 't', ' ', 16, 0, 0, 0, 1, 0, 1, 0};
    uint32_t riff = 36 + data;
    memcpy(h + 4, &riff, 4);
    memcpy(h + 24, &rate, 4);
    memcpy(h + 28, &byte_rate, 4);
    h[32] = 2;
    h[34] = 16;
    memcpy(h + 36, "data", 4);
    memcpy(h + 40, &data, 4);
    fseek(f, 0, SEEK_SET);
    fwrite(h, 1, sizeof h, f);
    fseek(f, 0, SEEK_END);
}

/* ISR or task context: append what the speaker plays from t_ns on; gaps while the speaker was
 * off become silence (capped at 2 s each so the file stays small). */
static void wav_put(const int16_t *pcm, int n, uint64_t t_ns) {
    if (!g_sim.out_dir) return;
    if (!A.wav) {
        char path[512];
        snprintf(path, sizeof path, "%s/speaker.wav", g_sim.out_dir);
        A.wav = fopen(path, "wb+");
        if (!A.wav) return;
        wav_header(A.wav, 0);
        A.wav_end_ns = t_ns;
    }
    if (t_ns > A.wav_end_ns) {
        uint64_t gap = (t_ns - A.wav_end_ns) * HAL_AUDIO_RATE / 1000000000ull;
        if (gap > 2 * HAL_AUDIO_RATE) gap = 2 * HAL_AUDIO_RATE;
        static const int16_t zero[256];
        for (uint64_t g = 0; g < gap; g += 256) fwrite(zero, 2, (size_t)(gap - g < 256 ? gap - g : 256), A.wav);
        A.wav_samples += gap;
    }
    fwrite(pcm, 2, (size_t)n, A.wav);
    A.wav_samples += (uint64_t)n;
    if (g_sim.interactive) sim_gui_audio_push(pcm, n); /* the desktop simulator plays it on the computer */
    A.wav_end_ns = t_ns + (uint64_t)n * 1000000000ull / HAL_AUDIO_RATE;
}

/* ------------------------------------------------------------- microphone */

static int16_t host_mic_sample(void) { /* the computer's microphone (desktop simulator), 16 kHz */
    static int16_t buf[256];
    static int pos, len;
    if (pos >= len) {
        len = sim_gui_mic_pull(buf, 256);
        pos = 0;
        if (len <= 0) return 0; /* nothing captured yet: silence */
    }
    return buf[pos++];
}

static int16_t mic_sample(uint64_t n) {
    double t = (double)n / HAL_AUDIO_RATE;
    switch (A.src) {
    case MIC_HOST: return host_mic_sample();
    case MIC_SILENCE: return (int16_t)((int)((n * 2654435761u) >> 27) - 16); /* room noise, a few LSB */
    case MIC_NOISE: return (int16_t)((int)((n * 2654435761u) >> 19) - 4096);
    default: {
        /* voiced speech-like: pitch around 150 Hz with drift, ~4 syllables per second */
        double env = 0.5 + 0.5 * sin(2 * M_PI * 4.0 * t);
        double ph = 2 * M_PI * (150.0 * t - 20.0 / (2 * M_PI * 0.7) * cos(2 * M_PI * 0.7 * t));
        double v = sin(ph) + 0.5 * sin(2 * ph) + 0.25 * sin(3 * ph);
        return (int16_t)(5000.0 * env * env * v);
    }
    }
}

static void cap_tick(void *arg) { /* ISR: one more descriptor recorded */
    (void)arg;
    A.cap_ev = 0;
    if (!A.cap_on) return;
    A.produced++;
    if (A.produced - A.consumed > (uint64_t)A.N) { /* ring wrapped: the oldest descriptor is overwritten */
        A.consumed++;
        A.lost_pending += A.chunk;
        A.cap_overruns++;
        A.cap_lost_samples += (uint64_t)A.chunk;
        sim_trace_instant("audio", "mic overrun", vtk_isr_now(), NULL);
    }
    vtk_isr_wake_one(&A.cap_wq);
    A.cap_ev = vtk_event_at(A.cap_t0 + (A.produced + 1) * A.period_ns, cap_tick, NULL);
}

/* Platform audio task (upstream: audio_input, PSRAM stack): drains the RX ring. */
static void audio_task(void *arg) {
    (void)arg;
    for (;;) {
        vtk_lock();
        while (!A.cap_on || A.consumed >= A.produced) vtk_block_locked(&A.cap_wq, VTK_TIME_INF);
        uint64_t idx = A.consumed++;
        int lost = A.lost_pending;
        A.lost_pending = 0;
        vtk_unlock();
        /* convert the 32-bit stereo slots to 16-bit mono + reference */
        sim_charge_cycles((double)A.chunk * profile_cost("audio.convert_frame"));
        uint64_t first = idx * (uint64_t)A.chunk;
        for (int i = 0; i < A.chunk; i++) {
            A.mic[i] = mic_sample(first + (uint64_t)i);
            A.ref[i] = 0;
        }
        osal_mutex_lock(A.cb_lock, OSAL_WAIT_FOREVER);
        if (A.cap_on && A.cb) A.cb(A.ctx, A.mic, A.ref, A.chunk, lost, (A.cap_t0 + idx * A.period_ns) / 1000);
        osal_mutex_unlock(A.cb_lock);
        A.cap_samples += (uint64_t)A.chunk;
    }
}

void sim_audio_mic_source(const char *name) { /* ISR (script) */
    A.src = !strcmp(name, "silence") ? MIC_SILENCE : !strcmp(name, "noise") ? MIC_NOISE : !strcmp(name, "host") ? MIC_HOST : MIC_SPEECH;
}

/* ---------------------------------------------------------------- speaker */

static void play_tick(void *arg);

#define AUDIBLE_LEVEL 64 /* |sample| above this (after volume) is sound */

/* Sound in the last 2 ms of these samples? A gap after sound is audible, after silence it is not. */
static bool tail_audible(const int16_t *s, int n) {
    for (int i = n > 32 ? n - 32 : 0; i < n; i++)
        if (s[i] > AUDIBLE_LEVEL || s[i] < -AUDIBLE_LEVEL) return true;
    return false;
}

static void dry(uint64_t t, int samples, bool cut) { /* ISR: `samples` of zeros went out */
    if (cut && !A.in_gap) {
        A.in_gap = true;
        A.underruns++;
    }
    if (A.in_gap) {
        A.underrun_samples += (uint64_t)samples;
        sim_trace_instant("audio", "speaker underrun", t, NULL);
    } else {
        A.idle_samples += (uint64_t)samples;
    }
    A.sounding = false;
}

static void maybe_start_dma_locked(uint64_t now) {
    if (A.dma_running || !A.out_on || A.play_ev) return;
    if (A.full == 0 && !(A.draining && A.partial > 0)) return;
    A.dma_running = true;
    uint64_t at = now > A.ready_ns ? now : A.ready_ns;
    A.boundary_ns = at;
    A.play_ev = vtk_event_at(at, play_tick, NULL);
}

static void play_tick(void *arg) { /* ISR: the DMA needs the next descriptor */
    (void)arg;
    uint64_t t = vtk_isr_now();
    A.play_ev = 0;
    if (!A.dma_running) return;
    int16_t *s = A.slots + (size_t)A.tail * A.chunk;
    if (A.full > 0) {
        wav_put(s, A.chunk, t);
        A.tail = (A.tail + 1) % A.N;
        A.full--;
        A.played_samples += (uint64_t)A.chunk;
        A.in_gap = false;
        A.sounding = tail_audible(s, A.chunk);
    } else if (A.partial > 0) { /* a half-written descriptor: the rest plays as zeros */
        bool cut = !A.draining && tail_audible(s, A.partial);
        memset(s + A.partial, 0, (size_t)(A.chunk - A.partial) * 2);
        wav_put(s, A.chunk, t);
        A.played_samples += (uint64_t)A.partial;
        A.in_gap = false;
        if (A.draining) A.sounding = false;
        else dry(t, A.chunk - A.partial, cut);
        A.partial = 0;
        A.tail = (A.tail + 1) % A.N;
    } else if (A.draining) { /* everything played */
        A.dma_running = false;
        vtk_wake_all_locked(&A.play_wq);
        return;
    } else { /* nothing queued: the DMA sends zeros */
        static const int16_t zero[480];
        wav_put(zero, A.chunk <= 480 ? A.chunk : 480, t);
        dry(t, A.chunk, A.sounding);
    }
    vtk_wake_all_locked(&A.play_wq);
    A.boundary_ns = t + A.period_ns;
    A.play_ev = vtk_event_at(A.boundary_ns, play_tick, NULL);
}

/* -------------------------------------------------------------------- API */

static void lazy_init(void) {
    if (A.init) return;
    A.init = true;
    A.chunk = g_profile.audio_dma_frames;
    A.N = g_profile.audio_dma_desc;
    A.period_ns = (uint64_t)A.chunk * 1000000000ull / HAL_AUDIO_RATE;
    A.ring_bytes = (size_t)A.N * (size_t)A.chunk * (size_t)g_profile.audio_slot_bytes;
}

void hal_audio_get_info(hal_audio_info_t *out) {
    lazy_init();
    memset(out, 0, sizeof *out);
    out->has_speaker = g_profile.info.has_audio_out;
    out->has_mic = g_profile.info.has_audio_in;
    out->mic_reference = out->has_mic;
    out->chunk_samples = A.chunk;
    out->runway_ms = (int)((uint64_t)A.N * A.period_ns / 1000000ull);
}

int hal_audio_capture_start(hal_audio_capture_cb cb, void *ctx) {
    lazy_init();
    if (!g_profile.info.has_audio_in || A.cap_on || !cb) return -1;
    const char *prev = osal_mem_tag("i2s-rx");
    A.rx_dma = osal_malloc(A.ring_bytes, OSAL_MEM_DMA);
    osal_mem_tag(prev);
    if (!A.rx_dma) return -1;
    hal_power_request(HAL_PWR_AUDIO_IN, true);
    A.cb = cb;
    A.ctx = ctx;
    vtk_lock();
    A.cap_on = true;
    A.produced = A.consumed = 0;
    A.lost_pending = 0;
    A.cap_t0 = vtk_now();
    A.cap_ev = vtk_event_at(A.cap_t0 + A.period_ns, cap_tick, NULL);
    vtk_unlock();
    sim_trace_instant("audio", "mic on", vtk_now(), NULL);
    return 0;
}

void hal_audio_capture_stop(void) {
    if (!A.cap_on) return;
    vtk_lock();
    A.cap_on = false;
    if (A.cap_ev) vtk_event_cancel(A.cap_ev);
    A.cap_ev = 0;
    vtk_unlock();
    /* no callback after this returns: wait for one in progress */
    osal_mutex_lock(A.cb_lock, OSAL_WAIT_FOREVER);
    A.cb = NULL;
    osal_mutex_unlock(A.cb_lock);
    osal_free(A.rx_dma);
    A.rx_dma = NULL;
    hal_power_request(HAL_PWR_AUDIO_IN, false);
    sim_trace_instant("audio", "mic off", vtk_now(), NULL);
}

int hal_audio_play_start(void) {
    lazy_init();
    if (!g_profile.info.has_audio_out) return -1;
    if (A.out_on) return 0;
    const char *prev = osal_mem_tag("i2s-tx");
    A.tx_dma = osal_malloc(A.ring_bytes, OSAL_MEM_DMA);
    osal_mem_tag(prev);
    if (!A.tx_dma) return -1;
    hal_power_request(HAL_PWR_AUDIO_OUT, true);
    if (metalio_board_ready()) metalio_board_set(0, MB_P0_AMP, MB_P0_AMP); /* amplifier enable (TCA9555 P04) */
    else if (g_profile.flash_stalls_cpu) sim_i2c_transfer("io-expander", 3);
    vtk_lock();
    A.out_on = true;
    A.draining = false;
    A.dma_running = false;
    A.tail = A.full = A.partial = 0;
    A.sounding = A.in_gap = false;
    A.ready_ns = vtk_now() + SIM_US(g_profile.audio_start_us);
    vtk_unlock();
    return 0;
}

int hal_audio_play_write(const int16_t *pcm, int n, int64_t timeout_us) {
    if (vtk_in_isr() || !vtk_self()) osal_panic("hal_audio_play_write must be called from a task");
    if (!A.out_on || n <= 0) return 0;
    sim_charge_cycles((double)n * profile_cost("audio.mix_sample")); /* pack into 32-bit stereo slots */
    uint64_t deadline = timeout_us < 0 ? VTK_TIME_INF : vtk_now() + (uint64_t)timeout_us * 1000ull;
    int done = 0;
    vtk_lock();
    while (done < n && A.out_on) {
        if (A.full >= A.N) {
            uint64_t now = vtk_now();
            if (deadline != VTK_TIME_INF && now >= deadline) break;
            vtk_block_locked(&A.play_wq, deadline == VTK_TIME_INF ? VTK_TIME_INF : deadline - now);
            continue;
        }
        int slot = (A.tail + A.full) % A.N;
        int take = n - done < A.chunk - A.partial ? n - done : A.chunk - A.partial;
        memcpy(A.slots + (size_t)slot * A.chunk + A.partial, pcm + done, (size_t)take * 2);
        A.partial += take;
        done += take;
        if (A.partial == A.chunk) {
            A.full++;
            A.partial = 0;
        }
        maybe_start_dma_locked(vtk_now());
    }
    vtk_unlock();
    return done;
}

int64_t hal_audio_play_delay_us(void) {
    lazy_init();
    if (!A.out_on) return (int64_t)g_profile.audio_start_us;
    uint64_t now = vtk_now(), t;
    vtk_lock();
    if (A.dma_running) t = A.boundary_ns > now ? A.boundary_ns - now : 0;
    else t = A.ready_ns > now ? A.ready_ns - now : 0;
    t += (uint64_t)A.full * A.period_ns;
    vtk_unlock();
    return (int64_t)(t / 1000);
}

void hal_audio_play_stop(bool drain) {
    if (!A.out_on) return;
    vtk_lock();
    if (drain && (A.full > 0 || A.partial > 0 || A.dma_running)) {
        A.draining = true;
        maybe_start_dma_locked(vtk_now());
        while (A.dma_running) vtk_block_locked(&A.play_wq, VTK_TIME_INF);
    }
    if (A.play_ev) vtk_event_cancel(A.play_ev);
    A.play_ev = 0;
    A.dma_running = false;
    A.out_on = false;
    A.full = A.partial = 0;
    vtk_wake_all_locked(&A.play_wq);
    vtk_unlock();
    osal_free(A.tx_dma);
    A.tx_dma = NULL;
    if (metalio_board_ready()) metalio_board_set(0, MB_P0_AMP, 0); /* amplifier off */
    else if (g_profile.flash_stalls_cpu) sim_i2c_transfer("io-expander", 3);
    hal_power_request(HAL_PWR_AUDIO_OUT, false);
}

/* ------------------------------------------------------------- simulator */

void sim_audio_init(void) {
    lazy_init();
    A.mic = sim_host_calloc((size_t)A.chunk, sizeof *A.mic);
    A.ref = sim_host_calloc((size_t)A.chunk, sizeof *A.ref);
    A.slots = sim_host_calloc((size_t)A.N * (size_t)A.chunk, sizeof *A.slots);
    if (!A.mic || !A.ref || !A.slots) abort();
    if (!g_profile.info.has_audio_in && !g_profile.info.has_audio_out) return;
    const char *prev = osal_mem_tag("audio-driver");
    A.cb_lock = osal_mutex_create();
    osal_task_cfg_t cfg = {.name = "audio_in", .fn = audio_task, .stack_bytes = g_profile.audio_task_stack,
                           .priority = g_profile.audio_task_prio, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    if (!A.cb_lock || osal_task_create(&cfg, &A.task) != OSAL_OK) osal_panic("audio driver init failed");
    osal_mem_tag(prev);
}

bool sim_audio_busy(void) { return A.cap_on || A.out_on; }

void sim_audio_speaker_counters(uint64_t *underruns, double *underrun_ms, double *idle_ms) {
    vtk_lock();
    *underruns = A.underruns;
    *underrun_ms = A.underrun_samples * 1000.0 / HAL_AUDIO_RATE;
    *idle_ms = A.idle_samples * 1000.0 / HAL_AUDIO_RATE;
    vtk_unlock();
}

void sim_audio_report(void) {
    sim_metric_set("audio.mic.captured_ms", A.cap_samples * 1000.0 / HAL_AUDIO_RATE);
    sim_metric_set("audio.mic.overruns", (double)A.cap_overruns);
    sim_metric_set("audio.mic.lost_ms", A.cap_lost_samples * 1000.0 / HAL_AUDIO_RATE);
    sim_metric_set("audio.speaker.played_ms", A.played_samples * 1000.0 / HAL_AUDIO_RATE);
    sim_metric_set("audio.speaker.underruns", (double)A.underruns);
    sim_metric_set("audio.speaker.underrun_ms", A.underrun_samples * 1000.0 / HAL_AUDIO_RATE);
    sim_metric_set("audio.speaker.idle_ms", A.idle_samples * 1000.0 / HAL_AUDIO_RATE);
    if (A.wav) {
        wav_header(A.wav, (uint32_t)A.wav_samples);
        fclose(A.wav);
        A.wav = NULL;
    }
}
