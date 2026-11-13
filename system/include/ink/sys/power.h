/*
 * Power service (foundation §7): one place decides when the device is
 * active, idle or locked, and when the chip may sleep.
 *
 *   ACTIVE  user input in the last few seconds, or a transfer the user waits for:
 *           CPU at full speed.
 *   IDLE    the page just sits there (reading): CPU at its idle clock, the
 *           touch controller in monitor mode, background polling slowed.
 *   LOCKED  power key, or long without input (lying flat and still: sooner):
 *           the screen keeps its image with a small lock mark; only the power
 *           key, the AI key and lifting it from flat (power.lift_wake) wake
 *           it. While nothing needs the chip (no audio, no transfer, nothing
 *           queued for the panel or the flash) it light-sleeps in slices;
 *           otherwise (e.g. an audiobook keeps playing) it stays awake,
 *           locked.
 *
 * Services declare what they are doing with holds instead of keeping the
 * device awake by side effect; the policy, not the services, turns that into
 * clock, sleep and network decisions. Upstream's PowerPolicy evaluates every
 * second; this service only wakes on events and deadlines.
 */
#ifndef INK_SYS_POWER_H
#define INK_SYS_POWER_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { INK_PWR_ACTIVE = 0, INK_PWR_IDLE, INK_PWR_LOCKED } ink_power_state_t;

typedef enum {
    INK_NEED_AUDIO = 0, /* playing or recording: the chip stays awake (also while locked) */
    INK_NEED_NET,       /* a transfer that must finish (AI turn, sync, download): active (full clock), no sleep until done */
    INK_NEED_NO_LOCK,   /* no automatic locking (USB disk, upgrade screen) */
    INK_NEED_AWAKE,     /* the chip stays awake, also when locked (USB disk, an interrupt on screen, an upgrade) */
    INK_NEED_COUNT
} ink_power_need_t;

int ink_power_init(void);
ink_power_state_t ink_power_state(void);
/** A requested state change has not yet reached its listeners. */
bool ink_power_transition_pending(void);

/** User input happened at t_us (any task). Keeps the device active. */
void ink_power_activity(uint64_t t_us);
/** Reference-counted needs (any task). */
void ink_power_hold(ink_power_need_t need);
void ink_power_release(ink_power_need_t need);

/** The last input. */
uint64_t ink_power_last_activity(void);

/** Lock now (power key short press). */
void ink_power_lock(void);
/** A wake key was pressed at t_us while locked: unlock. Returns false if it was not locked. */
bool ink_power_unlock(uint64_t t_us);
/** The chip sleeps in a slice or is still coming out of it (a wake key's unlock may yet come). A redraw only the
 *  locked screen needs can wait for the busy checks, which run before the chip sleeps again. Any task. */
bool ink_power_waking(void);

/** The next time something is due (alarms): a sleep slice ends there, and the chip does not sleep past it until
 *  the owner moves it, for a few seconds at most. 0 = nothing. Any task. */
void ink_power_wake_at(uint64_t t_us);

/** The system is up, whoever listens for `battery.low` is listening: a low battery is acted on from now on. */
void ink_power_battery_policy_start(void);

/** State changes, called in the power task after the change. Up to 8 listeners. */
typedef void (*ink_power_fn)(void *ctx, ink_power_state_t from, ink_power_state_t to, uint64_t cause_us);
int ink_power_subscribe(ink_power_fn fn, void *ctx);

/** Work that must settle before the chip may sleep while locked, e.g. the UI drawing the lock
 *  mark (asked from the power task: cheap, non-blocking). Up to 6. The panel (refreshes queued
 *  or playing) and the flash (writes queued) are always checked. */
int ink_power_add_busy_check(bool (*busy)(void));
/** What to leave on the panel just before the power goes, what waited to be written being written: it returns when that is done,
 *  or it gave up. why: "shutdown" or "battery". One. */
void ink_power_set_off_picture(void (*fn)(const char *why));
/** Presenter acknowledges its committed frame after all panel callbacks succeeded. */
void ink_power_off_picture_result(bool ok);

#ifdef __cplusplus
}
#endif
#endif
