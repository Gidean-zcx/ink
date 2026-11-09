#include <assert.h>
#include "../../ports/esp32s31/osal_ticks.h"
int main(void) {
    const uint64_t origins[] = {0, 50000, 123456789, UINT64_MAX - 9999};
    for (unsigned i = 0; i < sizeof origins / sizeof *origins; i++) {
        assert(ink_s31_wait_ticks(origins[i] + 9999, 0, 1001, 100, UINT32_MAX) == 2);
        assert(ink_s31_wait_ticks(origins[i], UINT32_MAX, 10000, 100, UINT32_MAX) == 2);
    }
    assert(ink_s31_wait_ticks(9999, 0, 1, 100, UINT32_MAX) == 2);
    assert(ink_s31_wait_ticks(9999, 0, 2, 100, UINT32_MAX) == 2);
    assert(ink_s31_wait_ticks(10000, 1, 10001, 100, UINT32_MAX) == 3);
    assert(ink_s31_wait_ticks(10000, 1, 0, 100, UINT32_MAX) == 0);
    assert(ink_s31_wait_ticks(10000, 1, -1, 100, UINT32_MAX) == UINT32_MAX);
    assert(ink_s31_wait_ticks((uint64_t)UINT32_MAX * 10000 + 9999, UINT32_MAX, 2, 100, UINT32_MAX) == 2);
    assert(ink_s31_wait_ticks(10000, 0, 1, 100, UINT32_MAX) == 2);
    assert(ink_s31_wait_ticks(0, 0, INT64_MAX, 100, UINT32_MAX) == UINT32_MAX - 1);
    assert(ink_s31_wait_ticks(0, 0, 20000, 100, UINT32_MAX) == 3);
    assert(ink_s31_wait_ticks(0, 0, 1, 1000, UINT32_MAX) == 2);
    return 0;
}
