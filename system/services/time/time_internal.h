/* Inside the time service. */
#ifndef INK_TIME_INTERNAL_H
#define INK_TIME_INTERNAL_H

#include <stdint.h>

int ink_time_caps_init(void);
int64_t time_days_from_civil(int y, int m, int d);

#endif
