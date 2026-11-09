/* Absolute microsecond deadline, rounded up to the RTOS tick. */
#ifndef INK_S31_OSAL_TICKS_H
#define INK_S31_OSAL_TICKS_H
#include <stdint.h>
static inline uint32_t ink_s31_wait_ticks(uint64_t now, uint32_t tick, int64_t us,
                                         uint32_t hz, uint32_t forever) {
    if (us < 0) return forever;
    if (!us) return 0;
    uint64_t period = 1000000u / hz;
    uint64_t delta = (uint64_t)us;
    if (delta > UINT64_MAX - now - (period - 1)) return forever - 1;
    uint64_t target = (now + delta + period - 1) / period;
    uint64_t current = now / period;
    uint64_t distance = target - current;
    /* Modular subtraction also handles the FreeRTOS tick counter wrapping. */
    uint32_t phase = (uint32_t)current - tick;
    if (distance >= forever - 1 || phase >= forever - 1 - distance) return forever - 1;
    return phase + (uint32_t)distance;
}
#endif
