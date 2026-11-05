#include "ink/osal.h"
static bool fail_next;
static void *test_malloc(size_t n, uint32_t mem);
#define osal_malloc test_malloc
#ifndef INK_TEST_TEXT_SOURCE
#define INK_TEST_TEXT_SOURCE "../../system/services/text/text_service.c"
#endif
#include INK_TEST_TEXT_SOURCE
#undef osal_malloc
#ifdef INK_TEST_OLD_TEXT
int ink_text_target_session(void) { return TS.session; }
uint32_t ink_text_edit_revision(void) { return 1; }
#endif
static void *test_malloc(size_t n, uint32_t mem) {
    if (fail_next) { fail_next = false; return NULL; }
    return osal_malloc(n, mem);
}
void ink_test_fail_snapshot(void) { fail_next = true; }
