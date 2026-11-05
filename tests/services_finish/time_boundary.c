#ifndef INK_TEST_TIME_SOURCE
#define INK_TEST_TIME_SOURCE "../../system/services/time/time_service.c"
#endif
#include INK_TEST_TIME_SOURCE
#ifdef INK_TEST_OLD_TIME
int ink_time_forget_member(const char *id, int64_t timeout) { (void)id; (void)timeout; return 0; }
int ink_time_set_if_revision(int64_t utc, bool hub, uint32_t expected, uint32_t *written) { (void)expected; ink_time_set(utc, hub); if (written) *written = 1; return 0; }
#endif
