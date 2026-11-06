/* Inside the notification service. */
#ifndef INK_NOTIFY_INTERNAL_H
#define INK_NOTIFY_INTERNAL_H

#include "ink/sys/interrupt.h"
#include "ink/sys/notify.h"

int notify_interrupt_init(void);
int notify_caps_init(void);
int ink_notify_post_for(const ink_notify_t *n, uint32_t *out_id, const char *member);
/** The one of this kind about `ref` (0 for a call or the battery): its id, or 0. */
int notify_interrupt_find(ink_int_kind_t kind, uint32_t ref);

#endif
