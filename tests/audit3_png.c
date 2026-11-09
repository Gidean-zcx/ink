/* Fail every allocation in the real PNG encoder, including reallocations. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ink/osal.h"

static unsigned calls, fail_at, live;
void *osal_malloc(size_t n, uint32_t mem) {
    (void)mem;
    if (++calls == fail_at) return NULL;
    void *p = malloc(n);
    if (p) live++;
    return p;
}
void *osal_realloc(void *p, size_t n, uint32_t mem) {
    (void)mem;
    if (++calls == fail_at) return NULL;
    bool fresh = p == NULL;
    void *out = realloc(p, n);
    if (out && fresh) live++;
    return out;
}
void osal_free(void *p) {
    if (p) live--;
    free(p);
}
uint8_t *ink_camera_png(const uint8_t *rgb, int w, int h, int *bytes);
int main(void) {
    uint8_t rgb[16 * 16 * 3]; unsigned seed = 7;
    for (size_t i = 0; i < sizeof rgb; i++) { seed = seed * 1664525u + 1013904223u; rgb[i] = (uint8_t)(seed >> 24); }
    int bytes = 0;
    uint8_t *png = ink_camera_png(rgb, 16, 16, &bytes);
    if (!png || bytes < 8 || memcmp(png, "\x89PNG\r\n\x1a\n", 8)) return 1;
    unsigned total = calls;
    osal_free(png);
    if (live) return 2;
    printf("B3-01 checking %u allocation failures\n", total);
    fflush(stdout);
    for (unsigned i = 1; i <= total; i++) {
        calls = 0; fail_at = i; bytes = -1;
        png = ink_camera_png(rgb, 16, 16, &bytes);
        if (png || live || calls != i) {
            fprintf(stderr, "B3-01: failure=%u calls=%u live=%u bytes=%d\n", i, calls, live, bytes);
            osal_free(png); return 3;
        }
    }
    calls = fail_at = 0;
    png = ink_camera_png(rgb, 16, 16, &bytes);
    if (!png || bytes <= 0) return 4;
    osal_free(png);
    if (live) return 5;
    printf("B3-01 PASS: all %u allocation failures cleaned up; encoding recovers\n", total);
    return 0;
}
