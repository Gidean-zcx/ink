/*
 * Members (foundation §8): who is using the device right now, and what a
 * child may do. Records, AI sessions and policies are per member. Switching
 * from a child to an adult space needs the adult's PIN (not modelled yet:
 * such switches are refused here); adult -> child is free.
 */
#ifndef INK_SYS_MEMBERS_H
#define INK_SYS_MEMBERS_H

#include <stdbool.h>

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

/** Publish member.current (after the state table started). */
void ink_members_init(void);
const ink_member_t *ink_member_current(void);
/** Switch member. Returns 0, -1 (unknown member) or -2 (child -> adult needs a PIN). */
int ink_member_switch(const char *id);
/** Tests only: back to the first member without a PIN. */
void ink_member_test_reset(void);
/** Does the current member's policy allow this action (child whitelist)? Adults: always. */
bool ink_member_allows(const char *action_id, bool child_ok);

#ifdef __cplusplus
}
#endif
#endif
