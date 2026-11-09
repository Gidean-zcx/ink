#include "ink/osal.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_WRITE_STATIC
#define STBI_WRITE_NO_STDIO
#define STBIW_MALLOC(n) osal_malloc(n, OSAL_MEM_EXTERNAL)
#define STBIW_REALLOC(p, n) osal_realloc(p, n, OSAL_MEM_EXTERNAL)
#define STBIW_FREE(p) osal_free(p)
#include "stb_image_write.h"
uint8_t *ink_camera_png(const uint8_t *rgb, int w, int h, int *bytes) {
    return stbi_write_png_to_mem(rgb, w * 3, w, h, 3, bytes);
}
