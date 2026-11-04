/*
 * A voice turn's answer arrives while the next turn starts (audit AUD12-004): the old reply must close
 * its own session, never the new one. The AI service and the boot sequence are included unchanged, so
 * that the test can run the next key press at the lock boundary inside the old reply. Results are the
 * probe counters "test.pass" / "test.fail", as in systest.c.
 */
#include <stdio.h>
#include <string.h>

#include "ink/osal.h"
#include "ink/probe.h"
#include "ink/sys/ai.h"
#include "ink/sys/exec.h"
#include "ink/sys/rt.h"
#include "ink/sys/ui.h"
#include "sim.h"

static void hooked_unlock(osal_mutex_t *m);
#define osal_mutex_unlock hooked_unlock
#include "../../system/services/ai/ai_service.c"
#undef osal_mutex_unlock
#define ink_main product_main
#include "../../system/boot/main.c"
#undef ink_main

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

static volatile int g_armed, g_ran, g_new_sid;
static osal_sem_t *g_turned;
static ink_exec_job_t g_next_job, g_key_job;

static void new_turn(void *arg) { /* UI task: the next press, while the old reply is stopped at its unlock */
    (void)arg;
    ink_ai_key(true, osal_now_us());
    g_new_sid = AI.session;
    osal_sem_give(g_turned);
}

static void hooked_unlock(osal_mutex_t *m) {
    osal_mutex_unlock(m);
    if (g_armed && m == AI.lock && AI.drop_frames && !AI.up_open && !strcmp(osal_task_name(osal_task_self()), "hub")) {
        g_armed = 0;
        ink_exec_post(INK_EXEC_UI, &g_next_job, new_turn, NULL);
        osal_sem_take(g_turned, OSAL_WAIT_FOREVER);
        g_ran = 1;
    }
}

static void key_job(void *pressed) { ink_ai_key(pressed != NULL, osal_now_us()); }

static void race_task(void *arg) {
    (void)arg;
    osal_task_delay_us(3000000);
    ink_exec_post(INK_EXEC_UI, &g_key_job, key_job, (void *)1);
    osal_task_delay_us(1000000);
    g_armed = 1;
    ink_exec_post(INK_EXEC_UI, &g_key_job, key_job, NULL);
    for (int w = 0; w < 3000 && !g_ran; w++) osal_task_delay_us(10000);
    CHECK(g_ran, "the first turn's answer came while the next turn started");
    osal_task_delay_us(100000);
    CHECK(g_new_sid > 0 && AI.session == g_new_sid && ink_rt_is_open(g_new_sid) && ink_rt_open_count() == 1, "the old reply closed its own session only (%d, %d, %d)", g_new_sid, AI.session,
          ink_rt_open_count());
    CHECK(AI.capturing, "the microphone is on for the new turn");
    osal_task_delay_us(1000000);
    CHECK(ink_rt_ready(g_new_sid), "and its session is up");
    ink_exec_post(INK_EXEC_UI, &g_key_job, key_job, NULL);
    for (int w = 0; w < 2000 && sim_metric_value("ai.answers") < 2; w++) osal_task_delay_us(10000); /* the old turn's came first */
    CHECK(sim_metric_value("ai.answers") == 2, "the new turn is answered");
    osal_task_delay_us(200000);
    CHECK(ink_rt_open_count() == 0, "and nothing stays open (%d)", ink_rt_open_count());
    ink_probe_count("test.finished");
}

void ink_main(void) {
    g_turned = osal_sem_create(0, 1);
    product_main();
    osal_task_cfg_t cfg = {.name = "race", .fn = race_task, .stack_bytes = 4096, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    if (osal_task_create(&cfg, NULL) != OSAL_OK) osal_panic("race task");
}
