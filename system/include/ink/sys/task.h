/*
 * What goes on in the background, as people and the AI see it (system services design §13.1, system UI spec §7.7): a title, how
 * far, whether it can be stopped, how it ended. The work itself is whatever it is (ink_job_*, a file job, a download); whoever
 * runs it says so here. It stays in memory: after a restart the list is empty.
 */
#ifndef INK_SYS_TASK_H
#define INK_SYS_TASK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { INK_TASK_RUNNING = 0, INK_TASK_DONE, INK_TASK_FAILED, INK_TASK_CANCELLED } ink_task_state_t;

/** After the capability bus. */
int ink_task_init(void);
/** Any task. Its id (> 0), or 0 when the list is full of running ones: the other calls take 0 and do nothing, the work goes on
 *  unseen. kind: "file.copy", "update.download". for_everyone: not the current member's own, so it is listed for all of them and
 *  only an adult stops it. cancel may be NULL: it cannot be stopped. It is asked to stop; ink_task_end says when it has. */
int ink_task_begin(const char *kind, const char *title, bool for_everyone, void (*cancel)(void *ctx), void *ctx);
/** An accepted private job keeps its member across blocking setup and handoff. */
int ink_task_begin_for(const char *kind, const char *title, const char *member, void (*cancel)(void *ctx), void *ctx);
/** percent 0..100, or -1 not known. stage: a word for the screen to turn into its steps, or NULL to leave it. */
void ink_task_progress(int id, int percent, const char *stage);
void ink_task_forget_member(const char *member);
/** Over. result: a sentence for a person, or NULL. object: what it made, in the form receipts use ("file:/me/a.txt"), or NULL. */
void ink_task_end(int id, ink_task_state_t how, const char *result, const char *object);

#ifdef __cplusplus
}
#endif
#endif
