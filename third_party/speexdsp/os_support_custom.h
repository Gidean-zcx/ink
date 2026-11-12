/* Keep the upstream resampler's allocations in PSRAM on both ports. */
#include "ink/osal.h"
#define OVERRIDE_SPEEX_ALLOC
#define OVERRIDE_SPEEX_REALLOC
#define OVERRIDE_SPEEX_FREE
static inline void *speex_alloc(int n) { return osal_calloc(1, (size_t)n, OSAL_MEM_EXTERNAL); }
static inline void *speex_realloc(void *p, int n) { return osal_realloc(p, (size_t)n, OSAL_MEM_EXTERNAL); }
static inline void speex_free(void *p) { osal_free(p); }
