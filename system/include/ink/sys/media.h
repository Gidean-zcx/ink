/*
 * Media playback and recording to a file (system services design §10.2, §10.3). Ogg Opus, 16 kHz mono (D-67): a standard
 * format a computer can play, written and read here. One playback at a time, owned by the system: it goes on when the app
 * goes away and when the device locks. It pauses for what comes first (a call, an alarm, the AI speaking), and goes on by
 * itself afterwards unless the person paused it or too long has passed. It remembers where it was, per member and per file.
 *
 * Calls may come from any task; what takes time is done on the media executor.
 */
#ifndef INK_SYS_MEDIA_H
#define INK_SYS_MEDIA_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { INK_MEDIA_IDLE = 0, INK_MEDIA_PLAYING, INK_MEDIA_PAUSED } ink_media_state_t;
typedef enum { INK_MEDIA_BY_USER = 0, INK_MEDIA_BY_FOCUS, INK_MEDIA_BY_ROUTE } ink_media_pause_t;

#define INK_MEDIA_RESUME UINT32_MAX /* where it was last time */

typedef struct {
    ink_media_state_t state;
    ink_media_pause_t paused_by;
    char path[128];
    uint32_t position_ms, duration_ms;
} ink_media_status_t;

int ink_media_init(void);
/** 0: on its way, and media.state and the event say how it goes. */
int ink_media_play(const char *path, uint32_t start_ms);
void ink_media_pause(void);
void ink_media_resume(void);
int ink_media_seek(uint32_t ms);
void ink_media_stop(void);
void ink_media_status(ink_media_status_t *out);

bool ink_audio_recording(void);

#ifdef __cplusplus
}
#endif
#endif
