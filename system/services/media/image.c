#include <string.h>
#include "ink/hal_display.h"
#include "ink/osal.h"
#include "ink/sys/crc.h"
#include "ink/sys/image.h"
#include "stb_image.h"

static uint32_t be32(const uint8_t *p) {
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}

/* stb ignores PNG CRCs; JPEG scan integrity is checked by the patched decoder. */
static bool complete(const uint8_t *p, size_t n) {
    if (n >= 4 && p[0] == 0xff && p[1] == 0xd8) return p[n - 2] == 0xff && p[n - 1] == 0xd9;
    if (n < 8 || memcmp(p, "\x89PNG\r\n\x1a\n", 8)) return false;
    for (size_t pos = 8; n - pos >= 12;) {
        size_t len = be32(p + pos);
        if (len > n - pos - 12 || ink_crc32(0, p + pos + 4, len + 4) != be32(p + pos + 8 + len)) return false;
        if (!memcmp(p + pos + 4, "IEND", 4)) return len == 0;
        pos += len + 12;
    }
    return false;
}

int ink_image_decode(const void *data, size_t size, ink_image_t *out) {
    if (!out) return INK_IMAGE_ERROR;
    *out = (ink_image_t){0};
    if (!data || !size) return INK_IMAGE_ERROR;
    if (size > INK_IMAGE_MAX_INPUT) return INK_IMAGE_LIMIT;
    int w, h, channels;
    if (!stbi_info_from_memory(data, (int)size, &w, &h, &channels)) return INK_IMAGE_ERROR;
    if (w <= 0 || h <= 0 || (unsigned)w > INK_IMAGE_MAX_EDGE || (unsigned)h > INK_IMAGE_MAX_EDGE ||
        (size_t)w * (size_t)h > INK_IMAGE_MAX_PIXELS) return INK_IMAGE_LIMIT;
    if (!complete(data, size)) return INK_IMAGE_ERROR;
    /* Uncalibrated cycle estimates for compressed bytes and decoded pixels. */
    osal_cost_cycles((uint64_t)size * 80 + (uint64_t)w * h * 200);
    uint8_t *p = stbi_load_from_memory(data, (int)size, &w, &h, &channels, 4);
    if (!p) return INK_IMAGE_ERROR;
    for (unsigned y = 0; y < (unsigned)h; y++) {
        for (unsigned x = 0; x < (unsigned)w; x++) {
            size_t i = (size_t)y * w + x;
            unsigned gray = (77u * p[4*i] + 150u * p[4*i+1] + 29u * p[4*i+2] + 128u) >> 8;
            unsigned alpha = p[4*i+3];
            p[i] = (uint8_t)((gray * alpha + 255u * (255u - alpha) + 127u) / 255u);
        }
        osal_cost_cycles((uint64_t)w * 64);
    }
    *out = (ink_image_t){(unsigned)w, (unsigned)h, p};
    return INK_IMAGE_OK;
}

void ink_image_free(ink_image_t *image) {
    if (image) { osal_free(image->pixels); *image = (ink_image_t){0}; }
}

void ink_image_dither(ink_image_t *image) {
    static const uint8_t bayer[16] = {0,8,2,10,12,4,14,6,3,11,1,9,15,7,13,5};
    float levels[16];
    for (unsigned l = 0; l < 16; l++) levels[l] = hal_display_level_luminance(l);
    if (!image || !image->pixels) return;
    for (unsigned y = 0; y < image->height; y++) {
        for (unsigned x = 0; x < image->width; x++) {
            size_t i = (size_t)y * image->width + x;
            float target = levels[0] + (levels[15] - levels[0]) * image->pixels[i] / 255.f;
            unsigned l = 0;
            while (l < 14 && target > levels[l + 1]) l++;
            float fraction = (target - levels[l]) / (levels[l + 1] - levels[l]);
            if (fraction > (bayer[(y % 4) * 4 + x % 4] + .5f) / 16.f) l++;
            image->pixels[i] = (uint8_t)(l * 17);
        }
        osal_cost_cycles((uint64_t)image->width * 128);
        osal_preempt_point();
    }
}

typedef struct { uint64_t object, version; ink_image_t image; } entry_t;
struct ink_thumb_cache { unsigned count; entry_t entries[INK_THUMB_SLOTS]; };

ink_thumb_cache_t *ink_thumb_cache_create(void) {
    return osal_calloc(1, sizeof(ink_thumb_cache_t), OSAL_MEM_EXTERNAL);
}

static void drop(ink_thumb_cache_t *cache, unsigned i) {
    ink_image_free(&cache->entries[i].image);
    memmove(cache->entries + i, cache->entries + i + 1, (--cache->count - i) * sizeof(entry_t));
}

void ink_thumb_cache_destroy(ink_thumb_cache_t *cache) {
    if (!cache) return;
    while (cache->count) drop(cache, 0);
    osal_free(cache);
}
void ink_thumb_cache_remove(ink_thumb_cache_t *cache, uint64_t object) {
    if (cache) for (unsigned i = 0; i < cache->count; i++)
        if (cache->entries[i].object == object) { drop(cache, i); break; }
}

int ink_image_thumbnail(ink_thumb_cache_t *cache, uint64_t object, uint64_t version,
                        const void *data, size_t size, ink_image_t *out) {
    if (!out) return INK_IMAGE_ERROR;
    *out = (ink_image_t){0};
    if (!cache) return INK_IMAGE_ERROR;
    for (unsigned i = 0; i < cache->count; i++) {
        entry_t *e = &cache->entries[i];
        if (e->object != object) continue;
        if (e->version == version) { *out = e->image; return INK_IMAGE_OK; }
        drop(cache, i);
        break;
    }
    ink_image_t source;
    int err = ink_image_decode(data, size, &source);
    if (err) return err;
    unsigned edge = source.width > source.height ? source.width : source.height;
    if (edge < INK_THUMB_EDGE) edge = INK_THUMB_EDGE;
    unsigned w = source.width * INK_THUMB_EDGE / edge, h = source.height * INK_THUMB_EDGE / edge;
    if (!w) w = 1;
    if (!h) h = 1;
    if (cache->count == INK_THUMB_SLOTS) drop(cache, 0);
    ink_image_t thumb = {w, h, osal_malloc((size_t)w * h, OSAL_MEM_EXTERNAL)};
    if (!thumb.pixels) { ink_image_free(&source); return INK_IMAGE_ERROR; }
    for (unsigned y = 0; y < h; y++) {
        for (unsigned x = 0; x < w; x++)
            thumb.pixels[(size_t)y * w + x] = source.pixels[(size_t)(y * source.height / h) * source.width + x * source.width / w];
        osal_cost_cycles((uint64_t)w * 32);
    }
    ink_image_free(&source);
    ink_image_dither(&thumb);
    cache->entries[cache->count++] = (entry_t){object, version, thumb};
    *out = thumb;
    return INK_IMAGE_OK;
}
