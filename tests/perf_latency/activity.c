/* Count only the real activity-card requests, without changing the product. */
#include <string.h>
#include "ink/sys/cap.h"
static unsigned reads;
static int measured_call(const char *id, const ink_args_t *args, const ink_call_opts_t *opts) {
    if (!strcmp(id, "job.list")) reads++;
    return ink_cap_call(id, args, opts);
}
#define ink_cap_call measured_call
#include "../../system/ui/shell/activity_card.c"
#undef ink_cap_call
unsigned perf_activity_reads(void) { unsigned n = reads; reads = 0; return n; }
