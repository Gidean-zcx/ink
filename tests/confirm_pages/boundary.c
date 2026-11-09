/* Exercise the actual private hub request entry without exposing it as an AI tool. */
#ifndef INK_TEST_CONFIRM_SOURCE
#define INK_TEST_CONFIRM_SOURCE "../../system/policy/confirm.c"
#endif
#include INK_TEST_CONFIRM_SOURCE

void test_confirm_request(const ink_json_t *j) { on_request(NULL, 100, "confirm/request", j, 0); }
