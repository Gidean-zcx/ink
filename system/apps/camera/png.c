#include "ink/osal.h"
#include <setjmp.h>
#include <stddef.h>
#include <string.h>

/* Only two pointers: the heap promises pointer alignment, not max_align_t (TLSF gives 8 on 64-bit
 * hosts, and a 16-byte type let x86-64 store the header with aligned SSE moves and fault). */
typedef struct ink_png_block ink_png_block_t;
struct ink_png_block { ink_png_block_t *prev, *next; };
/* The media executor serializes encoding; keep cleanup state across longjmp. */
static ink_png_block_t *ink_png_blocks;
static jmp_buf ink_png_failed;
static void ink_png_free(void *ptr) {
    if (!ptr) return;
    ink_png_block_t *b = (ink_png_block_t *)ptr - 1;
    if (b->prev) b->prev->next = b->next;
    else ink_png_blocks = b->next;
    if (b->next) b->next->prev = b->prev;
    osal_free(b);
}
static void *ink_png_realloc(void *ptr, size_t n) {
    ink_png_block_t *b = ptr ? (ink_png_block_t *)ptr - 1 : NULL;
    ink_png_block_t *prev = b ? b->prev : NULL, *next = b ? b->next : ink_png_blocks;
    b = osal_realloc(b, sizeof *b + n, OSAL_MEM_EXTERNAL);
    if (!b) longjmp(ink_png_failed, 1);
    b->prev = prev; b->next = next;
    if (prev) prev->next = b;
    else ink_png_blocks = b;
    if (next) next->prev = b;
    return b + 1;
}
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_WRITE_STATIC
#define STBI_WRITE_NO_STDIO
#define STBIW_MALLOC(n) ink_png_realloc(NULL, n)
#define STBIW_REALLOC(p, n) ink_png_realloc(p, n)
#define STBIW_FREE(p) ink_png_free(p)
#define STBIW_ASSERT(ok) do { if (!(ok)) longjmp(ink_png_failed, 1); } while (0)
#include "stb_image_write.h"
uint8_t *ink_camera_png(const uint8_t *rgb, int w, int h, int *bytes) {
    *bytes = 0;
    if (setjmp(ink_png_failed)) {
        while (ink_png_blocks) ink_png_free(ink_png_blocks + 1);
        *bytes = 0;
        return NULL;
    }
    uint8_t *png = stbi_write_png_to_mem(rgb, w * 3, w, h, 3, bytes);
    if (!png) return NULL;
    ink_png_block_t *b = (ink_png_block_t *)png - 1;
    /* Return the OSAL allocation itself so the caller can use osal_free. */
    memmove(b, png, (size_t)*bytes);
    ink_png_blocks = NULL;
    return (uint8_t *)b;
}
