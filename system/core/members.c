/*
 * Members (see ink/sys/members.h). Until the hub syncs the family's member
 * records (and the PIN UI exists), a built-in family stands in: the owner
 * (adult) and one child whose whitelist allows reading and the AI.
 */
#include <string.h>

#include "ink/probe.h"
#include "ink/sys/members.h"

static const ink_member_t FAMILY[] = {
    {.id = "owner", .name = "我", .role = INK_ROLE_ADULT},
    {.id = "kid", .name = "小朋友", .role = INK_ROLE_CHILD, .allow = {"reader.", "ai.", "system.home"}},
};

static const ink_member_t *CUR = &FAMILY[0];

const ink_member_t *ink_member_current(void) { return CUR; }

int ink_member_switch(const char *id) {
    for (size_t i = 0; i < sizeof FAMILY / sizeof FAMILY[0]; i++)
        if (!strcmp(FAMILY[i].id, id)) {
            if (CUR->role == INK_ROLE_CHILD && FAMILY[i].role == INK_ROLE_ADULT) return -2; /* needs the PIN */
            CUR = &FAMILY[i];
            ink_probe_count("members.switches");
            return 0;
        }
    return -1;
}

bool ink_member_allows(const char *action_id, bool child_ok) {
    if (CUR->role == INK_ROLE_ADULT) return true;
    if (child_ok) return true;
    for (int i = 0; i < 8 && CUR->allow[i]; i++)
        if (!strncmp(action_id, CUR->allow[i], strlen(CUR->allow[i]))) return true;
    return false;
}
