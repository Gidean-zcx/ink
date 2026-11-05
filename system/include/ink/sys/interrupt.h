/*
 * The system's interruption channel (system services design §6.2, foundation §7).
 *
 * An incoming call, an alarm or a nearly empty battery does not wait for the foreground
 * app: raising one turns the screen on, starts the ringing in the audio task and takes
 * the input, in the task that raised it, whatever the app or the panel are doing.
 *
 * Mechanism only, and for the system only (apps have notifications): what it looks like
 * is the system shell's (ink_interrupt_set_presenter).
 */
#ifndef INK_SYS_INTERRUPT_H
#define INK_SYS_INTERRUPT_H

#include <stdbool.h>
#include <stdint.h>

#include "ink/sys/exec.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { INK_INT_LOW_BATTERY = 0, INK_INT_ALARM, INK_INT_CALL } ink_int_kind_t; /* the higher wins the screen */

typedef struct {
    const char *label; /* static */
    const char *cap;   /* the capability behind the button; NULL = just close it */
    char args[64];     /* its arguments, JSON */
} ink_int_action_t;

typedef struct {
    int id;            /* given by raise */
    ink_int_kind_t kind;
    uint32_t ref;      /* what it is about (an alarm's id) */
    bool ringing;      /* the sound is on */
    char title[48], body[96];
    ink_int_action_t actions[3];
    int nactions;
    uint64_t raised_us; /* given by raise */
} ink_interrupt_t;

/** The shell: what should be on screen now changed (NULL: nothing). Runs on `exec`; the copy is its own. */
typedef struct {
    void (*show)(const ink_interrupt_t *top);
    ink_exec_t exec;
} ink_int_presenter_t;

/** Any task, the timer's too; never waits. Its id (> 0), or -1 (full: 4 at a time). */
int ink_interrupt_raise(const ink_interrupt_t *it);
/** It is over. */
void ink_interrupt_end(int id);
/** Change what it says and offers, e.g. ringing -> in a call. NULL body or actions: as they were. */
void ink_interrupt_update(int id, const char *body, const ink_int_action_t *actions, int nactions, bool ringing);
/** The button `index` was pressed: its capability runs as `src`. */
int ink_interrupt_action(int id, int index, uint8_t src);
void ink_interrupt_set_presenter(const ink_int_presenter_t *p);
bool ink_interrupt_active(void);
/** The power key while something is on: true if it was for that (silence, snooze, acknowledge). */
bool ink_interrupt_power_key(void);

#ifdef __cplusplus
}
#endif
#endif
