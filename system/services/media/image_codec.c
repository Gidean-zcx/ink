/* Reuse LVGL v9.5.0's standalone stb_image v2.30; its license is in the header. */
#include "ink/osal.h"
#include "ink/sys/image.h"
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_NO_STDIO
#define STBI_NO_LINEAR
#define STBI_NO_HDR
#define STBI_NO_SIMD
#define STBI_NO_FAILURE_STRINGS
#define STBI_MAX_DIMENSIONS INK_IMAGE_MAX_EDGE
#define STBI_MALLOC(n) osal_malloc(n, OSAL_MEM_EXTERNAL)
#define STBI_REALLOC(p, n) osal_realloc(p, n, OSAL_MEM_EXTERNAL)
#define STBI_FREE(p) osal_free(p)
#include "stb_image.h"
