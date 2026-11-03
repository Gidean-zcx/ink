/*
 * Members (see ink/sys/members.h). Until the hub syncs the family's member
 * records (and the PIN UI exists), a built-in family stands in: the owner
 * (adult) and one child whose whitelist allows reading and the AI.
 */
#include <string.h>

#include "ink/probe.h"
#include "ink/sys/members.h"
#include "ink/sys/state.h"

static const ink_member_t FAMILY[] = {
    {.id = "owner", .name = "我", .role = INK_ROLE_ADULT},
    {.id = "kid", .name = "小朋友", .role = INK_ROLE_CHILD, .allow = {"reader.", "ai.", "system.home"}},
};

static const ink_member_t *CUR = &FAMILY[0];

static const ink_state_def_t ST_MEMBER = {.id = "member.current", .type = INK_ST_STR, .title = "当前成员"};
static int h_member = -1;

static void publish(void) {
    if (h_member < 0) h_member = ink_state_define(&ST_MEMBER); /* the first time the table is up */
    ink_state_set_str(h_member, CUR->id);
}

const ink_member_t *ink_member_current(void) { return CUR; }

void ink_members_init(void) { publish(); }

int ink_member_switch(const char *id) {
    for (size_t i = 0; i < sizeof FAMILY / sizeof FAMILY[0]; i++)
        if (!strcmp(FAMILY[i].id, id)) {
            if (CUR->role == INK_ROLE_CHILD && FAMILY[i].role == INK_ROLE_ADULT) return -2; /* needs the PIN */
            CUR = &FAMILY[i];
            ink_probe_count("members.switches");
            publish();
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
