/* Inside the time service. */
#ifndef INK_TIME_INTERNAL_H
#define INK_TIME_INTERNAL_H

#include <stdint.h>
#include "ink/sys/time.h"

int ink_time_caps_init(void);
int64_t time_days_from_civil(int y, int m, int d);
/* Internal callers keep the identity accepted by the capability bus. */
int ink_time_alarm_add_for(ink_alarm_t *a, uint32_t *written, const char *member);
int ink_time_alarm_cancel_for(uint32_t id, uint32_t revision, const char *member);
int ink_time_alarm_next_for(int *cursor, ink_alarm_kind_t kind, ink_alarm_t *out, const char *member);

#endif
