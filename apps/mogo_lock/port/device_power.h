/* No PMU clock fallback: book_ticket.c's local_day uses the time service only. */
#pragma once
#include <stdbool.h>
#include <stdint.h>
typedef struct { bool time_synced; int64_t unix_sec; } pmu_snapshot_t;
static inline const pmu_snapshot_t *device_power_get(void) { return 0; }
