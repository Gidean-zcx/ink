/*
 * ink HAL — audio (speaker and microphone).
 *
 * One fixed format for the system: signed 16-bit mono PCM at 16 kHz. That is
 * what the Plus hardware runs (the BH1098G audio chip clocks I2S at 16 kHz)
 * and enough for voice, TTS and spoken-word content; platforms whose hardware
 * runs at another rate resample internally.
 *   device     BH1098G over I2S (the ESP32 is clock slave), DMA rings in
 *              internal RAM, NS4150 amplifier; its power rail is shared with
 *              the 4G module, so the platform's power service owns it
 *   POSIX      ALSA / PulseAudio
 *   simulator  I2S DMA ring model: scheduling delays and flash stalls turn
 *              into lost microphone audio and speaker underruns; what the
 *              speaker played is written to speaker.wav
 */
#ifndef INK_HAL_AUDIO_H
#define INK_HAL_AUDIO_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HAL_AUDIO_RATE 16000

/* Where the sound goes (Plus: BH1098 mode 1/local or mode 2/headset; see hal_bt.h).
 * Blocks while the audio chip changes mode (~0.7 s plus acknowledgement). 0, or -1. */
typedef enum { HAL_ROUTE_SPEAKER = 0, HAL_ROUTE_BT_HEADSET } hal_audio_route_t;
int hal_audio_route_set(hal_audio_route_t route);

typedef struct {
    bool has_speaker;
    bool has_mic;
    bool mic_reference; /* capture also delivers what the speaker played (for echo cancellation) */
    int chunk_samples;  /* capture arrives in chunks of this many samples (240 = 15 ms on the Plus) */
    int runway_ms;      /* how long audio survives without the system servicing it (DMA depth) */
} hal_audio_info_t;

void hal_audio_get_info(hal_audio_info_t *out);

/**
 * Capture callback, called from a platform audio task once per chunk. Copy
 * the samples and return quickly. `ref` is NULL without a reference channel.
 * `lost` = samples dropped just before this chunk because the system did not
 * keep up (overrun); `t_us` = when the first sample of the chunk was recorded.
 */
typedef void (*hal_audio_capture_cb)(void *ctx, const int16_t *mic, const int16_t *ref, int samples, int lost,
                                     uint64_t t_us);

/** Start the microphone (task context). Returns 0, or -1 (no microphone / already capturing / out of
 *  memory / the audio chip cannot be brought up, e.g. right after a wake whose supply is not back yet:
 *  try again later). The first start after the audio chip lost its supply waits for it to come up
 *  (Plus: the BH1098G, up to ~1.5 s). */
int hal_audio_capture_start(hal_audio_capture_cb cb, void *ctx);
/** Stop the microphone; no callbacks after this returns. */
void hal_audio_capture_stop(void);

/** Power up the output path (amplifier, I2S; task context). Returns 0 or -1 (as
 *  hal_audio_capture_start, whose wait for the audio chip it shares). */
int hal_audio_play_start(void);
/**
 * Queue samples for the speaker. Blocks while the DMA ring is full, up to
 * `timeout_us`. Returns the number of samples accepted.
 */
int hal_audio_play_write(const int16_t *pcm, int n, int64_t timeout_us);
/**
 * Time until a sample written now will be heard (queued audio + start-up),
 * like ALSA's delay. Lets the system report "pressed -> heard" latencies.
 */
int64_t hal_audio_play_delay_us(void);
/** Stop output. With `drain`, first wait until everything queued has played. */
void hal_audio_play_stop(bool drain);

#ifdef __cplusplus
}
#endif
#endif
