#include "../file_index_3/boundary.c"

void ink_test_audit7_reset(void) { max_hold = 0; size_queries = 0; }
uint64_t ink_test_audit7_hold(void) { return max_hold; }
unsigned ink_test_audit7_size_queries(void) { return size_queries; }
