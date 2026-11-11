#pragma once
#include <ink/sys/app.h>
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
static inline void *heap_caps_malloc(size_t n, unsigned caps) { return ink_app_alloc(n, (caps & MALLOC_CAP_SPIRAM) != 0); }
static inline void heap_caps_free(void *p) { ink_app_free(p); }
