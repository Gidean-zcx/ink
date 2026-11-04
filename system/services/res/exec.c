/*
 * Executors (see ink/sys/exec.h): the UI task, "now", and two system tasks.
 * Jobs are nodes their callers own, linked into a list here: a post cannot
 * fail for lack of room and never waits. The two tasks are created on their
 * first job: until something needs them they cost no internal RAM (task
 * control block and semaphore).
 */
#include <stdint.h>

#include "ink/osal.h"
#include "ink/sys/exec.h"
#include "ink/sys/ui.h"

#define MEDIA_STACK 8192 /* image decoding helpers; PSRAM */
#define MEDIA_PRIO 4     /* below the UI: input stays responsive while a remote screen decodes */
#define BG_STACK 8192 /* PSRAM: AT replies (settings applied here) and job steps; gcc host estimates reach ~6.8 KiB */
#define BG_PRIO 1 /* above idle only */

static struct {
    ink_exec_job_t *jobs; /* newest first: any task pushes, the executor takes them all */
    osal_sem_t *wake;
    osal_task_t *task;
    int state; /* 0 off, 1 allowed (ink_exec_init), 3 running, -1 failed to start */
} EX[INK_EXEC_COUNT];

static osal_mutex_t *g_starting; /* a task waiting for a start lends its priority to the one doing it */

static const struct {
    const char *name;
    uint32_t stack;
    int prio;
} CFG[INK_EXEC_COUNT] = {
    [INK_EXEC_MEDIA] = {"exec.media", MEDIA_STACK, MEDIA_PRIO},
    [INK_EXEC_BG] = {"exec.bg", BG_STACK, BG_PRIO},
};

void ink_exec_run(ink_exec_t e) {
    ink_exec_job_t *newest = __atomic_exchange_n(&EX[e].jobs, NULL, __ATOMIC_SEQ_CST), *j = NULL;
    while (newest) { /* oldest first */
        ink_exec_job_t *n = newest->next;
        newest->next = j;
        j = newest;
        newest = n;
    }
    while (j) {
        ink_exec_job_t *cur = j;
        void (*fn)(void *) = cur->fn;
        void *arg = cur->arg;
        j = cur->next;
        __atomic_store_n(&cur->queued, 0, __ATOMIC_RELEASE); /* posted from here on: it runs again */
        fn(arg); /* may free the node */
    }
}

static void exec_task(void *arg) {
    int e = (int)(intptr_t)arg;
    EX[e].task = osal_task_self();
    for (;;)
        if (osal_sem_take(EX[e].wake, OSAL_WAIT_FOREVER) == OSAL_OK) ink_exec_run((ink_exec_t)e);
}

/* The task, on the first job. False if it cannot be had. */
static bool running(ink_exec_t e) {
    int st = __atomic_load_n(&EX[e].state, __ATOMIC_ACQUIRE);
    if (st == 3) return true;
    if (st != 1) return false;
    osal_mutex_lock(g_starting, OSAL_WAIT_FOREVER);
    if (EX[e].state == 1) {
        const char *prev = osal_mem_tag("exec");
        EX[e].wake = osal_sem_create(0, 1);
        osal_mem_tag(prev);
        osal_task_cfg_t cfg = {.name = CFG[e].name, .fn = exec_task, .arg = (void *)(intptr_t)e, .stack_bytes = CFG[e].stack,
                               .priority = CFG[e].prio, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
        bool ok = EX[e].wake && osal_task_create(&cfg, &EX[e].task) == OSAL_OK;
        __atomic_store_n(&EX[e].state, ok ? 3 : -1, __ATOMIC_RELEASE);
    }
    st = EX[e].state;
    osal_mutex_unlock(g_starting);
    return st == 3;
}

int ink_exec_init(void) {
    if (!g_starting) {
        const char *prev = osal_mem_tag("exec");
        g_starting = osal_mutex_create();
        osal_mem_tag(prev);
    }
    if (!g_starting) return -1;
    for (int e = INK_EXEC_MEDIA; e <= INK_EXEC_BG; e++) {
        int off = 0;
        __atomic_compare_exchange_n(&EX[e].state, &off, 1, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);
    }
    return 0;
}

int ink_exec_ready(ink_exec_t e) {
    switch (e) {
    case INK_EXEC_DIRECT:
    case INK_EXEC_UI: return 0; /* before the UI task runs, its jobs wait for it */
    case INK_EXEC_MEDIA:
    case INK_EXEC_BG: return running(e) ? 0 : -1;
    default: return -1;
    }
}

int ink_exec_post(ink_exec_t e, ink_exec_job_t *job, void (*fn)(void *), void *arg) {
    if (ink_exec_ready(e) != 0) return -1;
    if (e == INK_EXEC_DIRECT) {
        fn(arg);
        return 0;
    }
    if (__atomic_exchange_n(&job->queued, 1, __ATOMIC_ACQ_REL)) return 0; /* still waiting: it will see the newest state */
    job->fn = fn;
    job->arg = arg;
    ink_exec_job_t *head = __atomic_load_n(&EX[e].jobs, __ATOMIC_RELAXED);
    do job->next = head;
    while (!__atomic_compare_exchange_n(&EX[e].jobs, &head, job, true, __ATOMIC_SEQ_CST, __ATOMIC_RELAXED));
    if (e == INK_EXEC_UI) ink_ui_kick();
    else osal_sem_give(EX[e].wake);
    return 0;
}

bool ink_exec_is_current(ink_exec_t e) {
    switch (e) {
    case INK_EXEC_DIRECT: return true;
    case INK_EXEC_UI: return ink_ui_is_current();
    case INK_EXEC_MEDIA:
    case INK_EXEC_BG: return EX[e].task && osal_task_self() == EX[e].task;
    default: return false;
    }
}

bool ink_exec_waiting(ink_exec_t e) { return __atomic_load_n(&EX[e].jobs, __ATOMIC_SEQ_CST) != NULL; }
