/* Fail exactly one allocation made by the unchanged text service. */
#include "ink/osal.h"
static bool fail_next;
static void *audit_text_malloc(size_t n, uint32_t mem);
#define osal_malloc audit_text_malloc
#include "../../../../system/services/text/text_service.c"
#undef osal_malloc
static void *audit_text_malloc(size_t n, uint32_t mem) {
    if (fail_next) { fail_next = false; return NULL; }
    return osal_malloc(n, mem);
}
void audit_text_fail_next(void) { fail_next = true; }
int audit_text_nundo(void) { return TS.nundo; }
int audit_text_nredo(void) { return TS.nredo; }
