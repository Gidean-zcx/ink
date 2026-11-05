#include "ink/sys/exec.h"
static bool hold_post;
static void (*held_fn)(void *);
static void *held_ctx;
static int test_post(ink_exec_t exec, ink_exec_job_t *job, void (*fn)(void *), void *ctx);
#define ink_exec_post test_post
#ifndef INK_TEST_NOTIFY_SOURCE
#define INK_TEST_NOTIFY_SOURCE "../../system/services/notify/notify_service.c"
#endif
#include INK_TEST_NOTIFY_SOURCE
#undef ink_exec_post
static int test_post(ink_exec_t exec, ink_exec_job_t *job, void (*fn)(void *), void *ctx) {
    if (hold_post && fn == post_away) { hold_post = false; held_fn = fn; held_ctx = ctx; return 0; }
    return ink_exec_post(exec, job, fn, ctx);
}
void ink_test_hold_notify(void) { hold_post = true; }
void ink_test_release_notify(void) { if (held_fn) { held_fn(held_ctx); held_fn = NULL; } }

#ifdef INK_TEST_OLD_NOTIFY
int ink_notify_cancel_if_ver(uint32_t id,uint32_t ver) { (void)ver; return ink_notify_cancel(id); }
#endif
