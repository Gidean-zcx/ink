/*
 * Family members and their spaces (foundation §8, system services design §11): who uses the device now, whether an adult's
 * space is open. A PIN is entered on the device only: it never goes through the bus to the AI or the hub, in a log or in
 * a file; what is kept is a salted, slow hash. Wrong guesses cost more and more time, across a restart too.
 */
#ifndef INK_SYS_MEMBERS_H
#define INK_SYS_MEMBERS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { INK_ROLE_ADULT = 0, INK_ROLE_CHILD } ink_role_t;

typedef struct {
    char id[16];
    char name[32];
    ink_role_t role;
    /* child whitelist: action id prefixes allowed beyond the actions marked child_ok ("" = none) */
    const char *allow[8];
} ink_member_t;

#define INK_MEMBERS_MAX 8

/** Publish member.current (after the state table started); read the table if the data store is up. */
void ink_members_init(void);
/** The capabilities and the idle watch: after the data store, the power service and the settings. */
int ink_members_start(void);
const ink_member_t *ink_member_current(void);
/** Who there is. NULL past the last. */
const ink_member_t *ink_member_at(int i);
bool ink_member_has_pin(const char *id);
/** Switch member. Returns 0, -1 (unknown member) or -2 (they have a PIN: it must be entered). */
int ink_member_switch(const char *id);
/** Tests only: back to the first member without a PIN. */
void ink_member_test_reset(void);
/** Does the current member's policy allow this action (child whitelist)? Adults: always. */
bool ink_member_allows(const char *action_id, bool child_ok);
/** Is the current member's space open: a child, an adult without a PIN, or an adult who has entered it and not gone idle. */
bool ink_member_space_open(void);
/** Is any adult's PIN set: what only a parent may do has something to ask for. */
bool ink_member_any_pin(void);

/* What a PIN does, on the background executor. Told how it went: */
enum { INK_PIN_OK = 0, INK_PIN_WRONG = -1, INK_PIN_WAIT = -2, INK_PIN_NO_MEMBER = -3, INK_PIN_BAD_FORM = -4 };
typedef void (*ink_pin_done_fn)(void *ctx, int result, uint32_t wait_s); /* wait_s: until the next try counts */
/** This member's PIN: right, they become the current member with their space open. 0: it will be told, -1: not started. */
int ink_member_unlock(const char *id, const char *pin, ink_pin_done_fn done, void *ctx);
/** Set, change (`old_pin` must be right) or remove (`new_pin` empty) a PIN: 4..8 digits. */
int ink_member_set_pin(const char *id, const char *old_pin, const char *new_pin, ink_pin_done_fn done, void *ctx);
/** Is this a parent's PIN? (a yes on a card that needs one) Wrong guesses count as for an unlock. */
int ink_member_check_parent(const char *pin, ink_pin_done_fn done, void *ctx);

#ifdef __cplusplus
}
#endif
#endif
