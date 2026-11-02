/*
 * Audio service: the only user of the speaker and the microphone (foundation §7).
 *
 *  - capture: 16 kHz mono PCM -> Opus 60 ms frames -> caller's callback
 *  - playback: Opus frames pushed by the caller (e.g. TTS from the hub) go
 *    through a jitter buffer, are decoded and played; one stream at a time,
 *    a stream of equal or higher priority replaces the current one
 *  - tones: short generated prompts so a key press is acknowledged within
 *    ~150 ms (SR-UX-01a) long before the e-ink panel can show anything
 *  - while it streams it holds flash writes off: the I2S DMA rings bridge
 *    only ~60 ms of frozen tasks, a flash erase takes 45-300 ms
 *
 * Encoding and decoding run in the audio service's own task (PSRAM stack;
 * Opus needs ~26 KB of stack). Callbacks run in that task: keep them short.
 */
#ifndef INK_SYS_AUDIO_H
#define INK_SYS_AUDIO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    INK_AUDIO_PRIO_STORY = 1, /* audiobooks, reading aloud */
    INK_AUDIO_PRIO_VOICE = 2, /* AI replies */
    INK_AUDIO_PRIO_ALERT = 3, /* alarms, reminders */
} ink_audio_prio_t;

typedef enum {
    INK_TONE_LISTEN = 0, /* push-to-talk: the microphone is open */
    INK_TONE_DONE,
    INK_TONE_ERROR,
} ink_tone_t;

int ink_audio_init(void);
/** Speaker / microphone present on this device. */
bool ink_audio_has_speaker(void);
bool ink_audio_has_mic(void);

/* One Opus frame (len > 0), or the end of the utterance (data NULL, len 0) after ink_audio_capture_end(). */
typedef void (*ink_audio_frame_fn)(void *ctx, const uint8_t *data, size_t len, uint32_t seq);
int ink_audio_capture_begin(ink_audio_frame_fn fn, void *ctx);
void ink_audio_capture_end(void);

/**
 * Open a playback stream; returns its id (> 0), or -1 if a higher-priority stream owns the
 * speaker. If `heard_probe` is set, records "cause_us -> first sample of the stream heard" (ms).
 */
int ink_audio_stream_open(ink_audio_prio_t prio, uint64_t cause_us, const char *heard_probe);
/** Queue one Opus frame (copied). */
int ink_audio_stream_push(int id, const uint8_t *opus, size_t len);
/** No more frames: play out what is queued, then close. */
void ink_audio_stream_finish(int id);
void ink_audio_stream_abort(int id);
/** Stop whatever plays (e.g. the user pressed the AI key again). */
void ink_audio_stop_all(void);

/**
 * Play a short prompt. If `probe` is set, records "cause_us -> first sample heard" (ms)
 * under that name (e.g. "ai.press_to_tone_heard_ms").
 */
void ink_audio_tone(ink_tone_t tone, uint64_t cause_us, const char *probe);
/** Speaker volume 0..100 (software gain). */
void ink_audio_set_volume(int percent);

#ifdef __cplusplus
}
#endif
#endif
