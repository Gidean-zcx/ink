#include "ink/sys/storage.h"
void ink_test_reader_before_submit(void);
static int test_write(const char *path, const void *data, size_t len);
static int test_member_write(const char *path, const void *data, size_t len, uint32_t generation);
#define ink_storage_write_atomic test_write
#define ink_storage_write_member_atomic test_member_write
#ifndef INK_TEST_READER_SOURCE
#define INK_TEST_READER_SOURCE "../../apps/reader/reader.c"
#endif
#include INK_TEST_READER_SOURCE
#undef ink_storage_write_atomic
#undef ink_storage_write_member_atomic

static int test_write(const char *path, const void *data, size_t len) {
    ink_test_reader_before_submit();
    return ink_storage_write_atomic(path, data, len);
}
static int test_member_write(const char *path, const void *data, size_t len, uint32_t generation) {
    ink_test_reader_before_submit();
    return ink_storage_write_member_atomic(path, data, len, generation);
}

/* The pending timer enters on the UI task before queued page destruction. */
void ink_test_reader_save(void) { save_cb(NULL); }
