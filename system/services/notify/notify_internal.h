/* Inside the notification service. */
#ifndef INK_NOTIFY_INTERNAL_H
#define INK_NOTIFY_INTERNAL_H

#include "ink/sys/interrupt.h"

int notify_interrupt_init(void);
int notify_caps_init(void);
/** The one of this kind about `ref` (0 for a call or the battery): its id, or 0. */
int notify_interrupt_find(ink_int_kind_t kind, uint32_t ref);

#endif
