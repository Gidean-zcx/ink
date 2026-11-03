/*
 * Executors (see ink/sys/exec.h): the UI task, "now", and two system tasks
 * with small queues of closures. A full queue is reported, never waited on:
 * the caller decides whether to drop or retry.
 */
#include <stdint.h>

#include "ink/osal.h"
#include "ink/probe.h"
#include "ink/sys/exec.h"
#include "ink/sys/ui.h"

#define MEDIA_STACK 8192 /* image decoding helpers; PSRAM */
#define MEDIA_PRIO 4     /* below the UI: input stays responsive while a remote screen decodes */
#define MEDIA_QUEUE 16
#define BG_STACK 6144
#define BG_PRIO 1 /* above idle only */
#define BG_QUEUE 32

typedef struct {
    void (*fn)(void *);
    void *arg;
} closure_t;

static struct {
    osal_queue_t *q;
    osal_task_t *task;
} EX[INK_EXEC_COUNT];

static void exec_task(void *arg) {
    int e = (int)(intptr_t)arg;
    EX[e].task = osal_task_self();
    for (;;) {
        closure_t c;
        if (osal_queue_recv(EX[e].q, &c, OSAL_WAIT_FOREVER) == OSAL_OK) c.fn(c.arg);
    }
}

static int start(ink_exec_t e, const char *name, uint32_t stack, int prio, size_t qlen) {
    if (EX[e].q) return 0;
    const char *prev = osal_mem_tag("exec");
    EX[e].q = osal_queue_create(sizeof(closure_t), qlen);
    osal_mem_tag(prev);
    if (!EX[e].q) return -1;
    osal_task_cfg_t cfg = {.name = name, .fn = exec_task, .arg = (void *)(intptr_t)e, .stack_bytes = stack, .priority = prio,
                           .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    return osal_task_create(&cfg, &EX[e].task) == OSAL_OK ? 0 : -1;
}

int ink_exec_init(void) {
    if (start(INK_EXEC_MEDIA, "exec.media", MEDIA_STACK, MEDIA_PRIO, MEDIA_QUEUE)) return -1;
    return start(INK_EXEC_BG, "exec.bg", BG_STACK, BG_PRIO, BG_QUEUE);
}

int ink_exec_post(ink_exec_t e, void (*fn)(void *), void *arg) {
    switch (e) {
    case INK_EXEC_DIRECT: fn(arg); return 0;
    case INK_EXEC_UI:
        if (!ink_ui_started()) return -1;
        ink_ui_post(fn, arg);
        return 0;
    case INK_EXEC_MEDIA:
    case INK_EXEC_BG: {
        if (!EX[e].q) return -1;
        closure_t c = {fn, arg};
        if (osal_queue_send(EX[e].q, &c, 0) != OSAL_OK) {
            ink_probe_count(e == INK_EXEC_MEDIA ? "exec.media.full" : "exec.bg.full");
            return -1;
        }
        return 0;
    }
    default: return -1;
    }
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
