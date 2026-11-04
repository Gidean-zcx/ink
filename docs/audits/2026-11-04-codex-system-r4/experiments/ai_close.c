/* Component boundary probe: unchanged AI service + normal boot. A test task emulates the RT
 * layer dropping a session and delivering its on_closed callback, before/after upload completion. */
#include <stdio.h>
#include <string.h>
#include "ink/osal.h"
#include "ink/probe.h"
#include "ink/sys/exec.h"
#include "sim.h"
#include "../../../../system/services/ai/ai_service.c"
#define ink_main product_main
#include "../../../../system/boot/main.c"
#undef ink_main

#define CHECK(c, msg) do { if (c) ink_probe_count("test.pass"); else { sim_warn("AUDIT_CHECK %s", msg); ink_probe_count("test.fail"); } } while (0)
static osal_sem_t *step_done;
static ink_exec_job_t step_job;
static struct { int session; bool capturing, asking, ended, up; char status[256]; } observed;

static void read_state(void *arg) {
    (void)arg;
    osal_mutex_lock(AI.lock, OSAL_WAIT_FOREVER);
    observed.session = AI.session;
    observed.up = AI.up_open;
    osal_mutex_unlock(AI.lock);
    observed.capturing = AI.capturing;
    observed.asking = AI.asking;
    observed.ended = AI.turn_over;
    snprintf(observed.status, sizeof observed.status, "%s", AI.open ? lv_label_get_text(AI.status) : "closed");
    osal_sem_give(step_done);
}
static void key(void *pressed) { ink_ai_key(pressed != NULL, osal_now_us()); osal_sem_give(step_done); }
static void step(void (*fn)(void *), void *arg) {
    CHECK(ink_exec_post(INK_EXEC_UI, &step_job, fn, arg) == 0, "UI job accepted");
    CHECK(osal_sem_take(step_done, 5000000) == OSAL_OK, "UI job completed");
}
static void close_from_peer(int sid) {
    ink_rt_close(sid); /* reproduce RT's ownership release before invoking the owner's callback */
    on_rt_closed(NULL, sid, 0);
    osal_task_delay_us(200000);
    step(read_state, NULL);
}

static void audit_task(void *arg) {
    (void)arg;
    osal_task_delay_us(3000000);
    step(key, (void *)1);
    osal_task_delay_us(1800000);
    step(read_state, NULL);
    int first = observed.session;
    CHECK(first > 0 && observed.capturing && observed.up, "the first turn is still recording");
    close_from_peer(first);
    CHECK(!observed.session && !observed.capturing && !observed.asking && observed.ended, "peer close while held ends capture and turn");
    char failed[sizeof observed.status];
    snprintf(failed, sizeof failed, "%s", observed.status);
    double failures = sim_metric_value("ai.failures");
    step(key, NULL);
    step(read_state, NULL);
    CHECK(!strcmp(failed, observed.status), "release keeps the peer-close failure");
    printf("AUDIT_AI_CLOSE held failures=%.0f capture=%d session=%d status=%s\n", failures, observed.capturing, observed.session, observed.status);

    step(key, (void *)1);
    osal_task_delay_us(1200000);
    step(read_state, NULL);
    int second = observed.session;
    on_rt_closed(NULL, first, -1); /* stale closure must not affect the new turn */
    osal_task_delay_us(100000);
    step(read_state, NULL);
    CHECK(second > 0 && observed.session == second && observed.capturing && !observed.ended, "stale close leaves the next turn alive");
    step(key, NULL);
    for (int k = 0; k < 100; k++) {
        osal_task_delay_us(10000);
        step(read_state, NULL);
        if (!observed.up) break;
    }
    CHECK(!observed.up && observed.asking && observed.session == second, "upload finished before the reply");
    close_from_peer(second);
    CHECK(!observed.session && observed.asking && !observed.ended && sim_metric_value("ai.failures") == failures,
          "close after upload waits for the actual reply, without false failure");
    for (int k = 0; k < 2500 && sim_metric_value("ai.answers") < 1; k++) osal_task_delay_us(10000);
    step(read_state, NULL);
    CHECK(sim_metric_value("ai.answers") == 1 && !observed.asking && observed.ended && sim_metric_value("ai.failures") == failures,
          "the real reply completes the second turn");
    printf("AUDIT_AI_CLOSE uploaded answers=%.0f failures=%.0f rt=%d status=%s\n",
           sim_metric_value("ai.answers"), sim_metric_value("ai.failures"), ink_rt_open_count(), observed.status);
    /* Let the intentionally aborted first request reach its real timeout; it must not rewrite the newer turn. */
    uint64_t now = osal_now_us();
    if (now < 50000000) osal_task_delay_us(50000000 - now);
    step(read_state, NULL);
    CHECK(observed.ended && !observed.asking && sim_metric_value("ai.failures") == failures && ink_rt_open_count() == 0,
          "old request timeout cannot overwrite the new answer or retain a session");
    ink_probe_count("test.finished");
}

void ink_main(void) {
    step_done = osal_sem_create(0, 1);
    product_main();
    osal_task_cfg_t cfg = {.name="audit-ai-close", .fn=audit_task, .stack_bytes=8192, .priority=3, .core=-1, .flags=OSAL_TASK_STACK_EXTERNAL};
    if (osal_task_create(&cfg, NULL) != OSAL_OK) osal_panic("audit task");
}
