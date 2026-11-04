/* Normal boot and real call capabilities. A semaphore deliberately holds exec.bg for 20 s,
 * matching the queue delay the new 25 s deadline is meant to tolerate. No modem code is changed. */
#include <stdio.h>
#include <string.h>
#include "ink/osal.h"
#include "ink/probe.h"
#include "ink/sys/cap.h"
#include "ink/sys/exec.h"
#include "sim.h"
#define ink_main product_main
#include "../../../../system/boot/main.c"
#undef ink_main

#define CHECK(c, msg) do { if (c) ink_probe_count("test.pass"); else { sim_warn("AUDIT_CHECK %s", msg); ink_probe_count("test.fail"); } } while (0)
static osal_sem_t *gate, *started, *finished;
static ink_exec_job_t blocker_job;
static volatile int replies;
static ink_receipt_t receipt;
static uint64_t received_at;

static void block(void *arg) {
    (void)arg;
    osal_sem_give(started);
    osal_sem_take(gate, OSAL_WAIT_FOREVER);
    osal_sem_give(finished);
}
static void reply(void *ctx, const ink_receipt_t *rc) {
    (void)ctx;
    receipt = *rc;
    received_at = osal_now_us();
    replies++;
}
static void queue_block(void) {
    CHECK(ink_exec_post(INK_EXEC_BG, &blocker_job, block, NULL) == 0, "blocker accepted");
    CHECK(osal_sem_take(started, 1000000) == OSAL_OK, "blocker started");
}
static void unblock(void) {
    osal_sem_give(gate);
    CHECK(osal_sem_take(finished, 1000000) == OSAL_OK, "blocker ended");
}
static void delayed_call(const char *name, int expected_state) {
    queue_block();
    replies = 0;
    ink_call_opts_t opts = {.src=INK_SRC_TOUCH, .reply_exec=INK_EXEC_DIRECT, .done=reply};
    uint64_t accepted = osal_now_us();
    CHECK(ink_cap_call(name, NULL, &opts) == 0, "call accepted behind work");
    osal_task_delay_us(4000000);
    CHECK(replies == 0, "no obsolete 3 s timeout while queued");
    osal_task_delay_us(16000000);
    CHECK(replies == 0, "20 s wait fits the new deadline");
    unblock();
    for (int k=0; k<200 && !replies; k++) osal_task_delay_us(10000);
    CHECK(replies == 1 && receipt.status == INK_RC_DONE && sim_net_call(NULL, 0) == expected_state,
          "after waiting the real modem action succeeds exactly once");
    printf("AUDIT_COMM %s replies=%d rc=%s elapsed_ms=%.3f state=%d detail=%s\n", name, replies,
           ink_rc_name(receipt.status), (double)(received_at-accepted)/1000, sim_net_call(NULL, 0), receipt.detail);
}
static void audit_task(void *arg) {
    (void)arg;
    osal_task_delay_us(6000000); /* the script's call arrives at 5 s */
    ink_power_hold(INK_NEED_NET);
    CHECK(sim_net_call(NULL, 0) == 4, "a simulated incoming call is ringing");
    delayed_call("call.answer", 1);
    delayed_call("call.hangup", 0);

    /* A queued hangup past 25 s must fail once and never execute later. */
    queue_block();
    double runs = sim_metric_value("action.user.call.hangup");
    replies = 0;
    ink_call_opts_t opts = {.src=INK_SRC_TOUCH, .reply_exec=INK_EXEC_DIRECT, .done=reply};
    uint64_t accepted = osal_now_us();
    CHECK(ink_cap_call("call.hangup", NULL, &opts) == 0, "long-wait call accepted");
    osal_task_delay_us(24000000);
    CHECK(replies == 0, "deadline has not passed at 24 s");
    osal_task_delay_us(2000000);
    CHECK(replies == 1 && receipt.status == INK_RC_FAILED && strstr(receipt.detail, "before it started"),
          "25 s deadline replies while still queued");
    CHECK(received_at-accepted >= 25000000 && received_at-accepted < 25100000, "timeout is at 25 s");
    unblock();
    osal_task_delay_us(100000);
    CHECK(replies == 1 && sim_metric_value("action.user.call.hangup") == runs, "expired hangup is not replayed");
    printf("AUDIT_COMM expired replies=%d elapsed_ms=%.3f executed=%.0f detail=%s\n", replies,
           (double)(received_at-accepted)/1000, sim_metric_value("action.user.call.hangup")-runs, receipt.detail);
    ink_power_release(INK_NEED_NET);
    ink_probe_count("test.finished");
}
void ink_main(void) {
    gate=osal_sem_create(0,1); started=osal_sem_create(0,1); finished=osal_sem_create(0,1);
    product_main();
    osal_task_cfg_t cfg = {.name="audit-comm", .fn=audit_task, .stack_bytes=8192, .priority=3, .core=-1, .flags=OSAL_TASK_STACK_EXTERNAL};
    if (osal_task_create(&cfg, NULL) != OSAL_OK) osal_panic("audit task");
}
