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
 */
#ifndef INK_SYS_EXEC_H
#define INK_SYS_EXEC_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { INK_EXEC_UI = 0, INK_EXEC_DIRECT, INK_EXEC_MEDIA, INK_EXEC_BG, INK_EXEC_COUNT } ink_exec_t;

/** Start exec.media and exec.bg. */
int ink_exec_init(void);
/** Run fn(arg) on executor e (DIRECT: now). 0, or -1 when its queue is full or it is not running. */
int ink_exec_post(ink_exec_t e, void (*fn)(void *), void *arg);
/** The calling task is executor e (DIRECT: always). */
bool ink_exec_is_current(ink_exec_t e);

#ifdef __cplusplus
}
#endif
#endif
