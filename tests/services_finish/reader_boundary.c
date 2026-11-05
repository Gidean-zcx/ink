#ifndef INK_TEST_READER_SOURCE
#define INK_TEST_READER_SOURCE "../../apps/reader/reader.c"
#endif
#include INK_TEST_READER_SOURCE

/* The pending timer enters on the UI task before queued page destruction. */
void ink_test_reader_save(void) { save_cb(NULL); }
