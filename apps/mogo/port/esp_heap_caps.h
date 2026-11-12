#pragma once
#include <runtime.h>
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
#define MALLOC_CAP_INTERNAL 4
#define heap_caps_malloc(n,c) ink_mogo_malloc(n)
#define heap_caps_calloc(n,s,c) ink_mogo_calloc(n,s)
#define heap_caps_realloc(p,n,c) ink_app_realloc(p,n)
#define heap_caps_free(p) ink_app_free(p)
size_t ink_mogo_free(void);
#define heap_caps_get_free_size(c) ink_mogo_free()
#define heap_caps_get_largest_free_block(c) ink_mogo_free()
