/* Inside the notification service. */
#ifndef INK_NOTIFY_INTERNAL_H
#define INK_NOTIFY_INTERNAL_H

#include "ink/sys/interrupt.h"
#include "ink/sys/notify.h"
#include "ink/sys/json.h"
#include "ink/sys/cap.h"

int notify_model_init(void);
bool notify_allowed(const char *number);
void notify_call_ring(void *arg);
void notify_actions_write(ink_jw_t *w, const ink_notify_action_t *actions, int count, const char *cap, const char *args);
void notify_actions_read(const ink_json_t *j, ink_notify_view_t *v);

int notify_interrupt_init(void);
int notify_caps_init(void);
int ink_notify_post_for(const ink_notify_t *n, uint32_t *out_id, const char *member);
/** The one of this kind about `ref` (0 for a call or the battery): its id, or 0. */
int notify_interrupt_find(ink_int_kind_t kind, uint32_t ref);

#endif
