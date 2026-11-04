/* Unchanged remote host and normal product boot; open the real capability while power is locked. */
#include <stdio.h>
#include "ink/osal.h"
#include "ink/probe.h"
#include "ink/sys/power.h"
#include "sim.h"
#include "../../../../system/apps/remote/remote.c"
#define ink_main product_main
#include "../../../../system/boot/main.c"
#undef ink_main

#define CHECK(c, msg) do { if (c) ink_probe_count("test.pass"); else { sim_warn("AUDIT_CHECK %s", msg); ink_probe_count("test.fail"); } } while (0)
static osal_sem_t *done;
static ink_exec_job_t job;
static int sid, suspended, updates, active;

static void inspect(void *arg) {
    (void)arg;
    sid = RS.sid; suspended = RS.suspended; updates = (int)RS.updates; active = RS.img != NULL;
    osal_sem_give(done);
}
static void pop(void *arg) { (void)arg; ink_ui_pop(); osal_sem_give(done); }
static void step(void (*fn)(void *)) {
    CHECK(ink_exec_post(INK_EXEC_UI, &job, fn, NULL) == 0, "UI post");
    CHECK(osal_sem_take(done, 5000000) == OSAL_OK, "UI completion");
}
static void open_locked(void) {
    ink_power_lock();
    osal_task_delay_us(100000);
    ink_call_opts_t opts = {.src=INK_SRC_SYSTEM};
    CHECK(ink_cap_call("remote.open", NULL, &opts) == 0, "open capability accepted while locked");
    osal_task_delay_us(1500000);
    step(inspect);
    CHECK(active && suspended && sid == 0 && ink_rt_open_count() == 0, "locked page has no active RT session");
    printf("AUDIT_REMOTE_LOCK suspended=%d sid=%d rt=%d\n", suspended, sid, ink_rt_open_count());
}
static void audit_task(void *arg) {
    (void)arg;
    osal_task_delay_us(3000000);
    /* Keep the virtual CPU awake only for deterministic observation of the lock state. */
    ink_power_hold(INK_NEED_NET);
    open_locked();
    CHECK(ink_power_unlock(osal_now_us()), "unlock succeeds");
    osal_task_delay_us(9000000);
    step(inspect);
    CHECK(active && !suspended && sid > 0 && updates > 0 && ink_rt_open_count() == 1, "unlock opens exactly one session and shows an update");
    printf("AUDIT_REMOTE_UNLOCK suspended=%d sid=%d updates=%d rt=%d\n", suspended, sid, updates, ink_rt_open_count());
    step(pop);
    osal_task_delay_us(300000);
    CHECK(ink_rt_open_count() == 0, "closing unlocked page releases the session");
    open_locked();
    step(pop);
    osal_task_delay_us(300000);
    CHECK(ink_power_unlock(osal_now_us()), "unlock after page closure");
    osal_task_delay_us(1000000);
    step(inspect);
    CHECK(!active && ink_rt_open_count() == 0 && !RS.px && !RS.back, "closing while locked leaves no page or canvas to resume");
    ink_power_release(INK_NEED_NET);
    ink_probe_count("test.finished");
}
void ink_main(void) {
    done = osal_sem_create(0, 1);
    product_main();
    osal_task_cfg_t cfg = {.name="audit-remote-lock", .fn=audit_task, .stack_bytes=8192, .priority=3, .core=-1, .flags=OSAL_TASK_STACK_EXTERNAL};
    if (osal_task_create(&cfg, NULL) != OSAL_OK) osal_panic("audit task");
}
