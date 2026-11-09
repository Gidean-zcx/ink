/* Exercise the exact UI counter, including retention and deletion semantics. */
#include "ink/sys/data.h"
static unsigned scans;
static int measured_next(ink_db_t *db, ink_db_cursor_t *c, ink_rec_t *r, void *buf, size_t cap) {
    scans++;
    return ink_db_next(db, c, r, buf, cap);
}
#define ink_db_next measured_next
#include "../../system/ui/shell/ai_pages.c"
#undef ink_db_next
size_t perf_log_count(ink_db_t *db, int64_t keep_s, unsigned *steps) {
    L.db = db; L.keep_s = keep_s; scans = 0;
    size_t n = list_count();
    *steps = scans; L.db = NULL;
    return n;
}
