/*
 * Starts that can go wrong (audit AUD12-003, AUD12-007): a low-priority task starting an executor
 * while higher-priority ones want it too, and a realtime session whose request cannot be made. The
 * executors and the sessions are included unchanged, so that a test can act at exactly those points.
 * Results are the probe counters "test.pass" / "test.fail", as in systest.c.
 */
#include <stdio.h>
#include <string.h>

#include "ink/hal_net.h"
#include "ink/osal.h"
#include "ink/probe.h"
#include "ink/sys/cap.h"
#include "ink/sys/display.h"
#include "ink/sys/exec.h"
#include "ink/sys/hub.h"
#include "ink/sys/members.h"
#include "ink/sys/net.h"
#include "ink/sys/power.h"
#include "ink/sys/res.h"
#include "ink/sys/rt.h"
#include "ink/sys/settings.h"
#include "ink/sys/state.h"
#include "ink/sys/storage.h"
#include "sim.h"

static osal_sem_t *pausing_sem_create(uint32_t initial, uint32_t max);
#define osal_sem_create pausing_sem_create
#include "../../system/services/res/exec.c"
#undef osal_sem_create

static int failing_hub_call(const char *method, const char *params_json, int64_t timeout_us, ink_hub_reply_fn cb, void *ctx);
#define ink_hub_call failing_hub_call
#include "../../system/services/hub/rt.c"
#undef ink_hub_call

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

/* ---- an executor started by a low-priority task, with higher-priority ones wanting it too ---- */

static osal_sem_t *g_go;
static int g_pause_once;
static volatile int g_low_done, g_high_done;

static osal_sem_t *pausing_sem_create(uint32_t initial, uint32_t max) {
    if (g_pause_once) { /* in the middle of starting it: the others run now */
        g_pause_once = 0;
        osal_sem_give(g_go);
        osal_sem_give(g_go);
        osal_task_delay_us(1000);
    }
    return osal_sem_create(initial, max);
}

static void low_task(void *arg) {
    (void)arg;
    if (ink_exec_ready(INK_EXEC_MEDIA) == 0) g_low_done = 1;
}

static void high_task(void *arg) {
    (void)arg;
    osal_sem_take(g_go, OSAL_WAIT_FOREVER);
    if (ink_exec_ready(INK_EXEC_MEDIA) == 0) g_high_done++;
}

static void start_checks(void) {
    g_go = osal_sem_create(0, 2);
    g_pause_once = 1;
    osal_task_cfg_t c = {.name = "t-high", .fn = high_task, .stack_bytes = 4096, .priority = 6, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&c, NULL) == OSAL_OK, "a high-priority caller");
    c.name = "t-higher";
    c.priority = 7;
    CHECK(osal_task_create(&c, NULL) == OSAL_OK, "another");
    c.name = "t-low";
    c.fn = low_task;
    c.priority = 3;
    CHECK(osal_task_create(&c, NULL) == OSAL_OK, "and the low-priority one that starts the executor");
    osal_task_delay_us(200000);
    CHECK(g_low_done == 1 && g_high_done == 2 && EX[INK_EXEC_MEDIA].state == 3, "all get it: the starter is not starved (%d, %d, state %d)", g_low_done, g_high_done,
          EX[INK_EXEC_MEDIA].state);
}

/* ---- a session whose request cannot be made ---- */

static int g_fail_calls;
static volatile int g_closed, g_calls;

static int failing_hub_call(const char *method, const char *params_json, int64_t timeout_us, ink_hub_reply_fn cb, void *ctx) {
    g_calls++;
    if (g_fail_calls > 0) {
        g_fail_calls--;
        return INK_HUB_ERR_NOMEM;
    }
    return ink_hub_call(method, params_json, timeout_us, cb, ctx);
}

static void t_closed(void *ctx, int sid, int reason) {
    (void)ctx;
    (void)sid;
    (void)reason;
    g_closed++;
}

static void t_frame(void *ctx, int sid, uint8_t kind, const uint8_t *p, size_t len, uint16_t seq, uint32_t t_ms, uint8_t flags) {
    (void)ctx;
    (void)sid;
    (void)kind;
    (void)p;
    (void)len;
    (void)seq;
    (void)t_ms;
    (void)flags;
}

static void rt_checks(void) {
    static const ink_rt_handler_t H = {.on_frame = t_frame, .on_closed = t_closed};
    g_fail_calls = 1;
    CHECK(ink_rt_open("echo", NULL, &H) == -1, "no memory for the request: the open says so");
    CHECK(ink_rt_open_count() == 0 && g_closed == 0, "nothing stays open, the owner has its answer (%d)", ink_rt_open_count());
    int sid = ink_rt_open("echo", NULL, &H);
    CHECK(sid > 0, "then it opens");
    for (int w = 0; w < 100 && !ink_rt_ready(sid); w++) osal_task_delay_us(100000);
    CHECK(ink_rt_ready(sid) && ink_rt_is_open(sid), "and is confirmed");
    /* the next connection: its announcement cannot be made */
    g_fail_calls = 1;
    on_online(NULL);
    CHECK(!ink_rt_is_open(sid) && g_closed == 1 && ink_rt_open_count() == 0, "announcing it again fails: closed, the owner told (%d)", g_closed);
    sid = ink_rt_open("echo", NULL, &H);
    for (int w = 0; w < 100 && !ink_rt_ready(sid); w++) osal_task_delay_us(100000);
    CHECK(ink_rt_ready(sid), "a new one works");
    on_online(NULL); /* as on any new connection */
    for (int w = 0; w < 100 && !ink_rt_ready(sid); w++) osal_task_delay_us(100000);
    CHECK(ink_rt_ready(sid) && g_closed == 1, "announced again, confirmed again");
    ink_rt_close(sid);
    CHECK(ink_rt_open_count() == 0, "and closes");
}

static void starts_task(void *arg) {
    (void)arg;
    if (ink_display_init() != 0 || ink_storage_init() != 0 || ink_cap_init() != 0 || ink_exec_init() != 0 || ink_res_init() != 0 || ink_state_init() != 0 ||
        ink_settings_init() != 0 || ink_power_init() != 0 || ink_net_init() != 0 || ink_hub_init("wss://hub.local/v1/device") != 0 || ink_rt_init() != 0) {
        CHECK(0, "the system services start");
        return;
    }
    ink_members_init();
    start_checks();
    rt_checks();
    ink_probe_count("test.finished");
}

void ink_main(void) {
    osal_task_cfg_t cfg = {.name = "starts", .fn = starts_task, .stack_bytes = 8192, .priority = OSAL_PRIO_NORMAL, .core = -1};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "create the test task");
}
