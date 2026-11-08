/* Memory-only image helpers. No file access, UI calls or background tasks. */
#ifndef INK_SYS_IMAGE_H
#define INK_SYS_IMAGE_H
#include <stddef.h>
#include <stdint.h>

#define INK_IMAGE_MAX_EDGE 4096u
#define INK_IMAGE_MAX_PIXELS (1024u * 1024u)
#define INK_IMAGE_MAX_INPUT (4u * 1024u * 1024u)
#define INK_THUMB_EDGE 128u
#define INK_THUMB_BYTES (INK_THUMB_EDGE * INK_THUMB_EDGE)
#define INK_THUMB_SLOTS 4u
#define INK_THUMB_CACHE_BYTES (INK_THUMB_SLOTS * INK_THUMB_BYTES)

typedef struct { unsigned width, height; uint8_t *pixels; } ink_image_t;
typedef struct ink_thumb_cache ink_thumb_cache_t;
enum { INK_IMAGE_OK = 0, INK_IMAGE_ERROR = -1, INK_IMAGE_LIMIT = -2 };

/* PNG or baseline JPEG, white alpha background; owned gray8 pixels in PSRAM.
 * Output is zeroed on failure; ERROR includes malformed input/allocation failure.
 * Run on the existing INK_EXEC_MEDIA executor (8 KiB PSRAM stack), never from
 * input or hub callbacks. Cache calls must use that same serialized context. */
int ink_image_decode(const void *data, size_t size, ink_image_t *out);
void ink_image_free(ink_image_t *image);
/* In-place: black..white maps to the panel's measured black..paper range.
 * Output gray8 codes are 0,17,...,255 (factory level indices times 17). */
void ink_image_dither(ink_image_t *image);

/* Caller-owned, serialized, member-scoped context; destroy on member/card change.
 * Key is caller's object ID + content version, independent of file paths. */
ink_thumb_cache_t *ink_thumb_cache_create(void);
void ink_thumb_cache_destroy(ink_thumb_cache_t *cache);
/* Borrowed result, valid until the next cache call. Fixed edge, no upscaling.
 * Hits may omit data; hits do not change FIFO order. A new version invalidates
 * the old entry even if decoding fails. Caller must not free/modify the result. */
int ink_image_thumbnail(ink_thumb_cache_t *cache, uint64_t object, uint64_t version,
                        const void *data, size_t size, ink_image_t *out);
#endif
