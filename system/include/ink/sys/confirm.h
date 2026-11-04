/*
 * The system's confirmation card (system services design §7, foundation §6.2). What the AI, the hub or an app asks for
 * that has consequences stops at the policy gate and waits here: the call, kept, and a card only the system can draw.
 * A person's yes runs it, through the gate again, as "confirmed"; nothing else can. No answer in a minute, another
 * member, or the asker changing its mind: it is dropped. Whoever asked hears how it ended in the event confirm.result.
 *
 * Mechanism only: what the card looks like is the system shell's (ink_confirm_set_presenter).
 */
#ifndef INK_SYS_CONFIRM_H
#define INK_SYS_CONFIRM_H

#include <stdbool.h>
#include <stdint.h>

#include "ink/sys/cap.h"
#include "ink/sys/exec.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t token;
    char cap[48], title[48], detail[160];
    uint8_t src;         /* who asked (ink_source_t) */
    uint64_t expires_us;
    bool parent;         /* an adult's PIN is needed too (system services design §11.3) */
} ink_confirm_view_t;

/** The shell: the card to show now (NULL: none; while locked or interrupted there is none). Runs on `exec`; the copy is its own. */
typedef struct {
    void (*show)(const ink_confirm_view_t *top);
    ink_exec_t exec;
} ink_confirm_presenter_t;

int ink_confirm_init(void);
/** The gate, or a handler that found out by itself (settings.set): keep this call for a person's yes. Fills the
 *  receipt: pending with object "confirm:<token>", or rejected (the queue is full, 4 at a time). */
ink_rc_t ink_confirm_hold(const ink_cap_t *c, const ink_args_t *args, ink_source_t src, uint32_t page_gen, ink_receipt_t *rc);
/** What waits, oldest first: how many (up to max). */
int ink_confirm_list(ink_confirm_view_t *out, int max);
/** A person answered: only touch and keys can. 0, or -1 (not waiting any more, or not a person). */
int ink_confirm_decide(uint32_t token, bool accept, ink_source_t src);
/** Whoever asked no longer wants it; a person can drop it too. 0, or -1. */
int ink_confirm_withdraw(uint32_t token, ink_source_t src);
void ink_confirm_set_presenter(const ink_confirm_presenter_t *p);

#ifdef __cplusplus
}
#endif
#endif
