/*
 * Notifications (R-052, system services design §6.1).
 *
 * A notification goes to its member's inbox and changes a small icon: on e-ink it
 * never pops up over the page. It is kept, grouped, and can say what capability
 * opens it. What cannot wait is an interruption (ink/sys/interrupt.h), and that is
 * the system's only.
 */
#ifndef INK_SYS_NOTIFY_H
#define INK_SYS_NOTIFY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ink/sys/data.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { INK_NOTIFY_LOW = 0, INK_NOTIFY_NORMAL, INK_NOTIFY_HIGH } ink_notify_prio_t;

#define INK_NOTIFY_ACTIONS_MAX 3
typedef struct { char label[32], cap[40], args[96]; } ink_notify_action_t;
typedef enum { INK_NOTIFY_CALL_NONE, INK_NOTIFY_CALL_ANY, INK_NOTIFY_CALL_CONTACTS, INK_NOTIFY_CALL_STARRED } ink_notify_callers_t;
typedef struct { bool active; int slot; int64_t until_s; const char *reason; } ink_notify_dnd_t;
/** Current member; reason: off/manual/schedule/calendar; until_s=0 for manual. */
void ink_notify_dnd_state(ink_notify_dnd_t *out);

typedef struct {
    const char *channel;     /* "message"; NULL = "system" */
    const char *title, *body;
    const char *group;       /* folded together in the list */
    const char *tag;         /* a new one with the same tag replaces the old */
    const char *action;      /* the capability that opens it, may be NULL */
    const char *action_args; /* its arguments, a JSON object */
    int prio;                /* ink_notify_prio_t, or -1 for the channel's */
    const char *member;      /* whose inbox; NULL = the current member */
    uint8_t src;             /* ink_source_t: who posted it */
    uint32_t *written_ver;   /* current inbox only; version accepted by this post */
    const ink_notify_action_t *actions;
    uint8_t nactions;
    const char *reply_cap, *reply_args; /* JSON template; reply adds the text field */
} ink_notify_t;

typedef struct {
    uint32_t id;
    int64_t when_s;
    uint8_t prio, src;
    bool read;
    char channel[16], title[48], body[160], group[24], tag[24], action[40], action_args[96];
    ink_notify_action_t actions[INK_NOTIFY_ACTIONS_MAX];
    uint8_t nactions;
    char reply_cap[40], reply_args[96];
} ink_notify_view_t;

typedef struct {
    const char *id, *title;
    ink_notify_prio_t prio;
} ink_notify_channel_t;

int ink_notify_init(void);
/** Any task but the timer's; does not wait for the flash. Another member's goes through the background executor. 0, or -1. */
int ink_notify_post(const ink_notify_t *n, uint32_t *out_id);
int ink_notify_cancel(uint32_t id);
int ink_notify_cancel_if_ver(uint32_t id, uint32_t ver);
int ink_notify_cancel_tag(const char *tag);
/** id 0: all of them. */
int ink_notify_mark_read(uint32_t id);
int ink_notify_clear(void);
/** The current member's, newest first: 1, 0 at the end. */
int ink_notify_next(ink_db_cursor_t *c, ink_notify_view_t *out);
int ink_notify_unread(void);
/** A channel of an app (static). Up to 16 in all. */
int ink_notify_channel_define(const ink_notify_channel_t *ch);
const ink_notify_channel_t *ink_notify_channel_at(int i); /* NULL past the last */
int ink_notify_channel_set(const char *id, bool on);
bool ink_notify_channel_on(const char *id);

#ifdef __cplusplus
}
#endif
#endif
