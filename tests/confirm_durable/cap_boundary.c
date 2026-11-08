#ifndef INK_TEST_CAP_SOURCE
#define INK_TEST_CAP_SOURCE "../../system/policy/cap.c"
#endif
#include INK_TEST_CAP_SOURCE
void review_tools(const ink_json_t *j) { on_tools(NULL, 101, "tools/call", j, 0); }
