#pragma once
#include <stdlib.h>
#include <ink/sys/app.h>
static inline void *ink_reader_malloc(size_t n) { return ink_app_alloc(n,true); }
#define malloc ink_reader_malloc
#define free ink_app_free
#define realloc ink_app_realloc
