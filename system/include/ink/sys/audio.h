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

#include "ink/sys/exec.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    INK_AUDIO_PRIO_STORY = 1, /* audiobooks, reading aloud */
    INK_AUDIO_PRIO_VOICE = 2, /* AI replies */
    INK_AUDIO_PRIO_ALERT = 3, /* alarms, reminders */
    INK_AUDIO_PRIO_CALL = 4,  /* a call, ringing or going on */
} ink_audio_prio_t;

typedef enum {
    INK_TONE_LISTEN = 0, /* push-to-talk: the microphone is open */
    INK_TONE_DONE,
    INK_TONE_ERROR,
    INK_TONE_KEY,
} ink_tone_t;

int ink_audio_init(void);
/** Speaker / microphone present on this device. */
bool ink_audio_has_speaker(void);
bool ink_audio_has_mic(void);

/* One Opus frame (len > 0), or the end of the utterance (data NULL, len 0) after ink_audio_capture_end(). */
typedef void (*ink_audio_frame_fn)(void *ctx, const uint8_t *data, size_t len, uint32_t seq);
/** 0, or -1: no microphone, it did not start, or a capture is still running. One that was ended counts
 *  until its end has been delivered, so -1 also from inside that delivery. */
int ink_audio_capture_begin(ink_audio_frame_fn fn, void *ctx);
/** The same for what stays on the device, a recording: state audio.mic tells the two apart. */
int ink_audio_capture_begin_local(ink_audio_frame_fn fn, void *ctx);
void ink_audio_capture_end(void);
/** A capture runs, or has not delivered its end yet: ink_audio_capture_begin would say -1. */
bool ink_audio_capture_busy(void);
/** Have the level of each frame measured, for whoever decides when someone has stopped talking; it costs a little. */
void ink_audio_capture_measure(bool on);
/** The last frame's mean absolute sample, 0..32767. */
int ink_audio_capture_level(void);

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
 * A stream whose speaker output could not be opened (the audio chip unpowered, e.g. right after a
 * wake whose supply is still being restored) keeps its frames and tries again; after about 8 s it
 * fails: its frames are dropped, pushes to it return -1 and this function is called (audio task,
 * keep it short). One listener; NULL removes it.
 */
typedef void (*ink_audio_fail_fn)(void *ctx, int id);
void ink_audio_on_stream_failed(ink_audio_fail_fn fn, void *ctx);

/**
 * Play a short prompt. If `probe` is set, records "cause_us -> first sample heard" (ms)
 * under that name (e.g. "ai.press_to_tone_heard_ms").
 */
void ink_audio_tone(ink_tone_t tone, uint64_t cause_us, const char *probe);
/*
 * Hands-free listening (R-047, D-52): the microphone stays open and the voice front end (hal_afe)
 * hears the wake word and speech, also while an answer plays (echo cancelled). While a voice
 * answer plays, speech stops it at once, here in the audio task (barge-in), and BARGE follows.
 * A capture begun while listening shares the microphone and starts from that moment. Events run
 * in the audio task: keep them short. The chip stays awake while listening.
 */
typedef enum { INK_LISTEN_WAKE = 1, INK_LISTEN_SPEECH, INK_LISTEN_QUIET, INK_LISTEN_BARGE, INK_LISTEN_COMMAND } ink_listen_ev_t;
/* COMMAND: a command word from ink_audio_listen_commands was recognised on the device; which one:
 * ink_audio_last_command(). */
typedef void (*ink_audio_listen_fn)(void *ctx, ink_listen_ev_t ev, uint64_t t_us);
/** The device can listen hands-free (a microphone and a voice front end). */
bool ink_audio_can_listen(void);
int ink_audio_listen(bool on, ink_audio_listen_fn fn, void *ctx);
/** The command words the device recognises by itself (offline quick intents); call before listening. */
int ink_audio_listen_commands(const char *const *phrases, int n);
/** Index of the command word last reported by INK_LISTEN_COMMAND. */
int ink_audio_last_command(void);

/** A call or an alarm rings (generated here, needs no file): it goes on, in this task, whatever the UI does, until
 *  turned off. It plays over a stream, which waits. Any task, never waits. */
void ink_audio_ring(bool on);
void ink_audio_key_tone(void);
void ink_audio_haptic(void);

/** Speaker volume 0..100 (software gain). */
void ink_audio_set_volume(int percent);
/** What plays at now: the volume, under a child's limit (audio.child_max) on every route. */
int ink_audio_effective_volume(void);

/*
 * Audio focus (system services design §10.1): one way to say who may be heard. Same level: the newer takes over. A
 * higher one: media is paused and told when it may go on; speech and recording end. Tones are dropped under a call or an
 * alarm. A mechanism of the system, not a capability.
 */
typedef enum { INK_FOCUS_LOST = 0, INK_FOCUS_PAUSED, INK_FOCUS_BACK } ink_focus_event_t;
typedef void (*ink_focus_fn)(void *ctx, ink_focus_event_t ev, ink_audio_prio_t by);
/** Any task. A token (> 0), or -1: something higher holds it and this level does not wait. Media waits: PAUSED comes at once. `fn` runs on `exec`, may be NULL. */
int ink_audio_focus_acquire(ink_audio_prio_t prio, ink_focus_fn fn, void *ctx, ink_exec_t exec);
void ink_audio_focus_release(int token);
/** The highest level held now; 0 = none. */
int ink_audio_focus_top(void);

/** How far a stream has come: frames played and still queued. -1: not this stream any more (played is still right for the last one). */
int ink_audio_stream_progress(int id, uint32_t *played, uint32_t *queued);

/* Where the sound goes (state audio.route), including during calls. */
typedef enum { INK_ROUTE_SPEAKER = 0, INK_ROUTE_HEADSET } ink_audio_route_t;
void ink_audio_set_route(ink_audio_route_t route);
ink_audio_route_t ink_audio_route(void);

#ifdef __cplusplus
}
#endif
#endif
