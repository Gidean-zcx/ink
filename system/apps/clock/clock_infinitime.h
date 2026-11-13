/* clock-3: the copied InfiniTime controllers (vendor/infinitime, SOURCE.md) for the C pages. UI task. */
#ifndef CLOCK_INFINITIME_H
#define CLOCK_INFINITIME_H
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
enum { CK_PER_MEMBER = 6, CK_TIMERS = 48, CK_WATCHES = 8, CK_LAPS = 99 };
typedef enum { CK_IDLE, CK_RUNNING, CK_PAUSED, CK_FINISHED } ck_state_t;
enum { CK_HOUR, CK_MINUTE, CK_SECOND }; /* the editor's three counters */
typedef struct {
    void (*edit_label)(int part); /* an editor counter changed: redraw that number */
    int (*ring)(int timer);       /* raise its card: the card's id, or -1 (the table is full) */
    void (*finished)(int timer);  /* it reached zero (not someone's tap) */
} ck_hooks_t;
void ck_init(const ck_hooks_t *hooks);
/* A stopwatch per member slot (StopWatchController). State: 0 cleared, 1 running, 2 paused. */
void ck_watch_start(int w);
void ck_watch_pause(int w);
void ck_watch_clear(int w);
bool ck_watch_lap(int w); /* false: not running, or 99 laps */
int ck_watch_state(int w);
uint32_t ck_watch_ms(int w);
int ck_watch_laps(int w);
uint32_t ck_watch_lap_ms(int w, int number); /* the total at the end of lap `number` (1..) */
/* The editor's counters (Counter: wrap at the ends). */
void ck_edit_set(int part, int value);
int ck_edit_get(int part);
void ck_edit_step(int part, int delta);
/* Timers 0..47 (member slot * 6 + index): Controllers::Timer behind the copied screen functions. */
void ck_timer_set(int t, uint32_t seconds); /* idle at this duration: stops it, its card ends */
int ck_timer_state(int t);
uint32_t ck_timer_duration(int t);
uint32_t ck_timer_shown(int t);   /* the seconds its row shows (0 when it has finished) */
uint32_t ck_timer_left_ms(int t); /* read now, without moving the display */
bool ck_timer_tick(int t);        /* the copied Refresh, once a second while it runs: true when its second changed */
void ck_timer_toggle(int t);      /* start, pause, resume */
void ck_timer_restart(int t);     /* back to its duration and running again */
#ifdef __cplusplus
}
#endif
#endif
