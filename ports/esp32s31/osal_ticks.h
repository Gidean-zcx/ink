/* Relative wait, rounded up with one extra tick for an unknown tick phase. */
#ifndef INK_S31_OSAL_TICKS_H
#define INK_S31_OSAL_TICKS_H
#include <stdint.h>
static inline uint32_t ink_s31_wait_ticks(uint64_t now, uint32_t tick, int64_t us,
                                         uint32_t hz, uint32_t forever) {
    if (us < 0) return forever;
    if (!us) return 0;
    (void)now; (void)tick; /* Clock origins are independent. */
    uint64_t period = 1000000u / hz;
    uint64_t delta = (uint64_t)us;
    uint64_t ticks = delta / period + (delta % period != 0) + 1;
    return ticks >= forever - 1 ? forever - 1 : (uint32_t)ticks;
}
#endif
