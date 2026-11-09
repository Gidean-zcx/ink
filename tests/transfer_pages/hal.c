/* Keep the real socket HAL; inject only its read-only address result. */
#define hal_transfer_host real_transfer_host
#include "../../ports/sim/hal/hal_transfer_sim.c"
#undef hal_transfer_host
#include <string.h>

int ink_test_transfer_host_mode;
int hal_transfer_host(char *out, size_t cap) {
    if (ink_test_transfer_host_mode == 1) {
        if (cap) out[0] = 0;
        return -1;
    }
    if (ink_test_transfer_host_mode == 2) {
        if (cap < sizeof "192.0.2.42") return -1;
        memcpy(out, "192.0.2.42", sizeof "192.0.2.42");
        return 0;
    }
    return real_transfer_host(out, cap);
}
