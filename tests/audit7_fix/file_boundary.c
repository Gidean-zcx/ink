#include "../file_index_3/boundary.c"

void ink_test_audit7_reset(void) {
    /* Finish an earlier owner's measurement before timing the requested edit. */
    osal_mutex_lock(FS.edits, OSAL_WAIT_FOREVER);
    max_hold = 0; size_queries = 0;
    osal_mutex_unlock(FS.edits);
}
uint64_t ink_test_audit7_hold(void) { return max_hold; }
unsigned ink_test_audit7_size_queries(void) { return size_queries; }
