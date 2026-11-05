#include "ink/osal.h"
static bool fail_next;
static void *test_malloc(size_t n, uint32_t mem);
#define osal_malloc test_malloc
#ifndef INK_TEST_TEXT_SOURCE
#define INK_TEST_TEXT_SOURCE "../../system/services/text/text_service.c"
#endif
#include INK_TEST_TEXT_SOURCE
#undef osal_malloc
static void *test_malloc(size_t n, uint32_t mem) {
    if (fail_next) { fail_next = false; return NULL; }
    return osal_malloc(n, mem);
}
void ink_test_fail_snapshot(void) { fail_next = true; }
