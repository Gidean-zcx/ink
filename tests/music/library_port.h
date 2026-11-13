/* Force-included into music_library.c for the host test: the same names apps/music/port/kiikoread.h maps, without
 * the product (no LVGL, no services). */
#pragma once
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "ink/osal.h"
#include "ink/sys/file.h"
#define bt_trace(...) ((void)0)
#define close_track() ((void)0)
#define time_freeze() ((void)0)
#define set_state(s) atomic_store(&s_state, (int)(s))
#define esp_random ink_music_random
#define heap_caps_malloc(n, caps) malloc(n)
#define heap_caps_free free
#define MALLOC_CAP_SPIRAM 0
#define MALLOC_CAP_8BIT 0
#undef strlcpy
#define strlcpy ink_music_copy
static inline size_t ink_music_copy(char *out, const char *in, size_t n) {
    size_t len = strlen(in), copy = n && len >= n ? n - 1 : len;
    if (n) {
        memcpy(out, in, copy);
        out[copy] = 0;
    }
    return len;
}
uint32_t ink_music_random(void);
