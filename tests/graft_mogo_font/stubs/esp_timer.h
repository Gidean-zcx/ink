#pragma once
#include <stdint.h>
/* No simulated timing or performance claim in this host compatibility test. */
static inline int64_t esp_timer_get_time(void) { return 0; }
