#ifndef INK_TEST_TASK_SOURCE
#define INK_TEST_TASK_SOURCE "../../system/services/res/task.c"
#endif
#include INK_TEST_TASK_SOURCE
#ifdef INK_TEST_OLD_TASK
void ink_task_forget_member(const char *id) { (void)id; }
#endif
