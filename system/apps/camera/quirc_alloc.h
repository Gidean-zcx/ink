/* quirc buffers belong to the device's external heap. */
#include <stdlib.h>
#include "ink/osal.h"
#define malloc(n) osal_malloc((n), OSAL_MEM_EXTERNAL)
#define calloc(n, s) osal_calloc((n), (s), OSAL_MEM_EXTERNAL)
#define free(p) osal_free(p)
