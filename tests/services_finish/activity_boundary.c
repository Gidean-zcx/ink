#include "ink/sys/exec.h"
static bool hold_flush;
static void (*held_fn)(void *);
static void *held_ctx;
static int test_post(ink_exec_t exec, ink_exec_job_t *job, void (*fn)(void *), void *ctx);
#define ink_exec_post test_post
#ifndef INK_TEST_CAP_SOURCE
#define INK_TEST_CAP_SOURCE "../../system/policy/cap.c"
#endif
#include INK_TEST_CAP_SOURCE
#undef ink_exec_post
static int test_post(ink_exec_t exec, ink_exec_job_t *job, void (*fn)(void *), void *ctx) {
    if (hold_flush && fn == activity_flush) { hold_flush = false; held_fn = fn; held_ctx = ctx; return 0; }
    return ink_exec_post(exec, job, fn, ctx);
}
void ink_test_hold_activity(void) { hold_flush = true; }
void ink_test_release_activity(void) { if (held_fn) { held_fn(held_ctx); held_fn = NULL; } }
