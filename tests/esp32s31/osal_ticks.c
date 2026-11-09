#include <assert.h>
#include "../../ports/esp32s31/osal_ticks.h"
int main(void) {
    assert(ink_s31_wait_ticks(9999, 0, 1, 100, UINT32_MAX) == 1);
    assert(ink_s31_wait_ticks(9999, 0, 2, 100, UINT32_MAX) == 2);
    assert(ink_s31_wait_ticks(10000, 1, 10001, 100, UINT32_MAX) == 2);
    assert(ink_s31_wait_ticks(10000, 1, 0, 100, UINT32_MAX) == 0);
    assert(ink_s31_wait_ticks(10000, 1, -1, 100, UINT32_MAX) == UINT32_MAX);
    assert(ink_s31_wait_ticks((uint64_t)UINT32_MAX * 10000 + 9999, UINT32_MAX, 2, 100, UINT32_MAX) == 2);
    assert(ink_s31_wait_ticks(10000, 0, 1, 100, UINT32_MAX) == 2);
    assert(ink_s31_wait_ticks(0, 0, INT64_MAX, 100, UINT32_MAX) == UINT32_MAX - 1);
    return 0;
}
