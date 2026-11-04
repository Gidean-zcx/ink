/*
 * Executors (D-56): the few places work is posted to. Threads belong to the
 * system; apps and capability handlers never create their own, they say
 * where their work runs (architecture §8.1).
 *
 *   UI      the UI task (LVGL, apps, most capability handlers)
 *   DIRECT  right now, in the caller's task: for short, non-blocking handlers
 *           that hand the work to their own service task
 *   MEDIA   exec.media: decoding, image work (the core with the vector unit)
 *   BG      exec.bg: jobs, sync, prefetch (lowest priority)
 *
 * What is posted is a node the caller owns, usually a member of the thing the
 * work is about, so posting cannot run out of anything: a receipt, a setting
 * to apply or a change to deliver cannot be lost to a full queue (audit
 * AUD11-001), and nobody ever waits to post.
 */
#ifndef INK_SYS_EXEC_H
#define INK_SYS_EXEC_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { INK_EXEC_UI = 0, INK_EXEC_DIRECT, INK_EXEC_MEDIA, INK_EXEC_BG, INK_EXEC_COUNT } ink_exec_t;

/* One piece of work to post. Zero-initialised before its first use; opaque after that. */
typedef struct ink_exec_job {
    struct ink_exec_job *next;
    void (*fn)(void *);
    void *arg;
    int queued;
} ink_exec_job_t;

/** Allow exec.media and exec.bg (each starts on its first use). */
int ink_exec_init(void);
/** Executor e can take work (exec.media and exec.bg start here if they have not yet). 0, or -1 if
 *  its task cannot be had: check before accepting work that must not be lost. */
int ink_exec_ready(ink_exec_t e);
/** Run fn(arg) on executor e, in the order posted (DIRECT: now). The node stays the caller's and must
 *  stay valid until fn starts. A node still waiting is not queued twice: fn runs once and should
 *  read the newest state when it runs. Any task; never waits. 0, or -1 only if ink_exec_ready(e) fails. */
int ink_exec_post(ink_exec_t e, ink_exec_job_t *job, void (*fn)(void *), void *arg);
/** The calling task is executor e (DIRECT: always). */
bool ink_exec_is_current(ink_exec_t e);
/** Jobs are waiting for executor e (any task). */
bool ink_exec_waiting(ink_exec_t e);
/** Run what has been posted to e so far: its own task only (the UI task's loop calls it). */
void ink_exec_run(ink_exec_t e);

#ifdef __cplusplus
}
#endif
#endif
