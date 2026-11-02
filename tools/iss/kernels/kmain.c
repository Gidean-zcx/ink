/* Calibration kernels: runs every kernel once; each records its measured
 * regions in the iss JSON report (see k.h for the protocol). */
#include <stdio.h>

#include "k.h"

volatile uint32_t k_sink;

int main(void) {
    struct {
        const char *name;
        void (*fn)(void);
    } kernels[] = {
        {"mem", k_mem}, {"gfx", k_gfx}, {"text", k_text}, {"crc", k_crc},
        {"json", k_json_parse}, {"inflate", k_inflate}, {"tlsf", k_tlsf},
    };
    int bad = 0;
    for (unsigned i = 0; i < sizeof kernels / sizeof kernels[0]; i++) {
        k_sink = 0;
        kernels[i].fn();
        int fail = k_sink == 0xdead || k_sink == 0xbad;
        printf("kernel %-8s %s (sink 0x%08lx)\n", kernels[i].name, fail ? "FAILED" : "ok", (unsigned long)k_sink);
        bad |= fail;
    }
    return bad;
}
