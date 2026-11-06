#ifndef INK_TEST_MEMBERS_SOURCE
#define INK_TEST_MEMBERS_SOURCE "../../system/policy/members.c"
#endif
#include INK_TEST_MEMBERS_SOURCE
#ifdef INK_TEST_OLD_MEMBERS
bool ink_member_removing(const char *id) { slot_t *s=find(id); return s && s->removing; }
uint32_t ink_member_exists(const char *id) { slot_t *s = id ? find(id) : cur(); return s && !s->removing ? 1 : 0; }
#endif
#ifdef INK_TEST_OLD_MEMBER_IO
bool ink_member_io_begin(const char *id) { return ink_member_exists(id); }
void ink_member_io_end(const char *id) { (void)id; }
#endif
