/*
 * System service checks on the simulated device: what a service promises its callers, beyond what
 * the scenarios exercise. Results are the probe counters "test.pass" / "test.fail", as in
 * tests/sim/selftest.c; the budget file requires zero failures.
 */
#include <stdio.h>
#include <string.h>

#include "ink/osal.h"
#include "ink/probe.h"
#include "ink/sys/display.h"
#include "ink/sys/storage.h"
#include "sim.h"

#define CHECK(cond, ...)                                                 \
    do {                                                                 \
        if (cond) {                                                      \
            ink_probe_count("test.pass");                                \
        } else {                                                         \
            char _m[300];                                                \
            snprintf(_m, sizeof _m, __VA_ARGS__);                        \
            sim_warn("CHECK FAILED (%s:%d) %s", __FILE__, __LINE__, _m); \
            ink_probe_count("test.fail");                                \
        }                                                                \
    } while (0)

static void drain_storage(void) {
    for (int i = 0; i < 500 && ink_storage_pending() > 0; i++) osal_task_delay_us(20000);
}

/* The read cache never returns an older version than the last write (audit AUD2-002: a small file
 * replaced by one too large for the cache kept reading as the old small file). */
static void storage_checks(void) {
    static char big[1500];
    char rb[32];
    const char *path = "/user/apps/systest-cache";
    memset(big, 'N', sizeof big);
    CHECK(ink_storage_write_atomic(path, "OLD", 3) == 0, "a small write is queued");
    drain_storage();
    memset(rb, 0, sizeof rb);
    CHECK(ink_storage_read(path, rb, sizeof rb) == 3 && !memcmp(rb, "OLD", 3), "it reads back (%.3s)", rb);
    CHECK(ink_storage_write_atomic(path, big, sizeof big) == 0, "a write too large for the cache is queued");
    drain_storage();
    memset(rb, 0, sizeof rb);
    int n = ink_storage_read(path, rb, sizeof rb);
    CHECK(n == (int)sizeof rb && rb[0] == 'N', "after it is written a read returns the new content, not the cached old one (%d, %c)", n,
          rb[0] ? rb[0] : '0');
    CHECK(ink_storage_write_atomic(path, "NEW", 3) == 0, "a small write again");
    memset(rb, 0, sizeof rb);
    CHECK(ink_storage_read(path, rb, sizeof rb) == 3 && !memcmp(rb, "NEW", 3), "while queued, a read sees it (%.3s)", rb);
    drain_storage();
    memset(rb, 0, sizeof rb);
    CHECK(ink_storage_read(path, rb, sizeof rb) == 3 && !memcmp(rb, "NEW", 3), "and after it is written (%.3s)", rb);
    CHECK(ink_storage_failed() == 0, "no write failed");
}

static void systest_task(void *arg) {
    (void)arg;
    if (ink_display_init() != 0 || ink_storage_init() != 0) {
        CHECK(0, "the display and storage services start");
        return;
    }
    storage_checks();
    ink_probe_mark("systest-done");
}

void ink_main(void) {
    osal_task_cfg_t cfg = {.name = "systest", .fn = systest_task, .stack_bytes = 6144, .priority = OSAL_PRIO_NORMAL, .core = -1};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "create the system test task");
}
