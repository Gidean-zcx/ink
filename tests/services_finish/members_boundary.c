#ifndef INK_TEST_MEMBERS_SOURCE
#define INK_TEST_MEMBERS_SOURCE "../../system/policy/members.c"
#endif
#include INK_TEST_MEMBERS_SOURCE
#ifdef INK_TEST_OLD_MEMBERS
bool ink_member_removing(const char *id) { slot_t *s=find(id); return s && s->removing; }
uint32_t ink_member_generation(const char *id) { slot_t *s = id ? find(id) : cur(); return s && !s->removing ? 1 : 0; }
#endif
