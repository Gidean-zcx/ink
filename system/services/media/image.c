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

static uint32_t le32(const uint8_t *p) {
    return (uint32_t)p[3] << 24 | (uint32_t)p[2] << 16 | (uint32_t)p[1] << 8 | p[0];
}

/* EXIF in a JPEG's APP1 (TIFF layout): orientation 0x0112 in IFD0, DateTimeOriginal 0x9003 in the
 * Exif IFD. Whatever is out of bounds or malformed leaves orientation 1 and no time. */
typedef struct { const uint8_t *p; size_t n; bool le; } tiff_t;
static unsigned tiff16(const tiff_t *t, size_t at) {
    return t->le ? (unsigned)(t->p[at] | t->p[at + 1] << 8) : (unsigned)(t->p[at] << 8 | t->p[at + 1]);
}
static uint32_t tiff32(const tiff_t *t, size_t at) {
    return t->le ? tiff16(t, at) | (uint32_t)tiff16(t, at + 2) << 16 : (uint32_t)tiff16(t, at) << 16 | tiff16(t, at + 2);
}
static void exif_ifd(const tiff_t *t, uint32_t ifd, ink_image_info_t *out, bool top) {
    if (ifd > t->n || t->n - ifd < 2 || (t->n - ifd - 2) / 12 < tiff16(t, ifd)) return; /* an IFD past the block is broken whole */
    for (unsigned i = 0, count = tiff16(t, ifd); i < count; i++) {
        size_t e = ifd + 2 + (size_t)i * 12;
        unsigned tag = tiff16(t, e), type = tiff16(t, e + 2);
        uint32_t n = tiff32(t, e + 4), value = tiff32(t, e + 8);
        if (top && tag == 0x0112 && type == 3 && n == 1 && tiff16(t, e + 8) >= 1 && tiff16(t, e + 8) <= 8) out->orientation = tiff16(t, e + 8);
        else if (top && tag == 0x8769 && (type == 4 || type == 13) && n == 1) exif_ifd(t, value, out, false);
        else if (!top && tag == 0x9003 && type == 2 && n >= 19 && n <= 20 && value <= t->n && t->n - value >= 19) {
            const uint8_t *s = t->p + value;
            bool ok = s[4] == ':' && s[7] == ':' && s[10] == ' ' && s[13] == ':' && s[16] == ':' && s[0] != '0';
            for (int k = 0; k < 19; k++) if (k != 4 && k != 7 && k != 10 && k != 13 && k != 16 && (s[k] < '0' || s[k] > '9')) ok = false;
            if (ok) { memcpy(out->taken, s, 19); out->taken[19] = 0; }
        }
    }
}
static void jpeg_exif(const uint8_t *p, size_t n, ink_image_info_t *out) {
    if (n > INK_IMAGE_PROBE_BYTES) n = INK_IMAGE_PROBE_BYTES;
    if (n < 4 || p[0] != 0xff || p[1] != 0xd8) return;
    for (size_t at = 2; n - at >= 4 && p[at] == 0xff;) {
        unsigned marker = p[at + 1];
        if (marker == 0xff) { at++; continue; }
        if (marker == 0xda || marker == 0xd9) return;
        size_t len = (size_t)p[at + 2] << 8 | p[at + 3];
        if (len < 2 || len > n - at - 2) return;
        if (marker == 0xe1 && len >= 16 && !memcmp(p + at + 4, "Exif\0\0", 6)) {
            tiff_t t = {p + at + 10, len - 8, p[at + 10] == 'I'};
            if ((!memcmp(t.p, "II", 2) || !memcmp(t.p, "MM", 2)) && tiff16(&t, 2) == 42) exif_ifd(&t, tiff32(&t, 4), out, true);
            return;
        }
        at += 2 + len;
    }
}
/* Where upright pixel (x, y) comes from in the stored w x h picture (EXIF orientation o). */
static size_t source_at(unsigned o, unsigned x, unsigned y, unsigned w, unsigned h) {
    unsigned sx = x, sy = y;
    switch (o) {
    case 2: sx = w - 1 - x; break;
    case 3: sx = w - 1 - x; sy = h - 1 - y; break;
    case 4: sy = h - 1 - y; break;
    case 5: sx = y; sy = x; break;
    case 6: sx = y; sy = h - 1 - x; break;
    case 7: sx = w - 1 - y; sy = h - 1 - x; break;
    case 8: sx = w - 1 - y; sy = x; break;
    default: break;
    }
    return (size_t)sy * w + sx;
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
    ink_image_info_t info = {.orientation = 1};
    jpeg_exif(data, size, &info);
    if (info.orientation > 1) { /* upright into a new gray buffer, then the RGBA one goes */
        unsigned o = info.orientation, W = o >= 5 ? (unsigned)h : (unsigned)w, H = o >= 5 ? (unsigned)w : (unsigned)h;
        uint8_t *q = osal_malloc((size_t)w * h, OSAL_MEM_EXTERNAL);
        if (!q) { osal_free(p); return INK_IMAGE_ERROR; }
        for (unsigned y = 0; y < H; y++) {
            for (unsigned x = 0; x < W; x++) q[(size_t)y * W + x] = p[source_at(o, x, y, (unsigned)w, (unsigned)h)];
            osal_cost_cycles((uint64_t)W * 16);
        }
        osal_free(p); p = q; w = (int)W; h = (int)H;
    }
    *out = (ink_image_t){(unsigned)w, (unsigned)h, p};
    return INK_IMAGE_OK;
}

int ink_image_probe(const void *data, size_t size, ink_image_info_t *out) {
    if (!out) return INK_IMAGE_ERROR;
    *out = (ink_image_info_t){.orientation = 1};
    const uint8_t *p = data;
    if (!p) return INK_IMAGE_ERROR;
    if (size > INK_IMAGE_PROBE_BYTES) size = INK_IMAGE_PROBE_BYTES;
    if (size >= 3 && p[0] == 0xff && p[1] == 0xd8 && p[2] == 0xff) { out->format = INK_IMAGE_JPEG; jpeg_exif(p, size, out); }
    else if (size >= 8 && !memcmp(p, "\x89PNG\r\n\x1a\n", 8)) out->format = INK_IMAGE_PNG;
    else if (size >= 10 && (!memcmp(p, "GIF87a", 6) || !memcmp(p, "GIF89a", 6))) {
        out->format = INK_IMAGE_GIF; out->width = p[6] | p[7] << 8; out->height = p[8] | p[9] << 8;
    } else if (size >= 26 && p[0] == 'B' && p[1] == 'M') {
        int32_t h = (int32_t)le32(p + 22);
        out->format = INK_IMAGE_BMP; out->width = le32(p + 18); out->height = h < 0 ? 0u - (uint32_t)h : (uint32_t)h;
    } else if (size >= 30 && !memcmp(p, "RIFF", 4) && !memcmp(p + 8, "WEBP", 4)) {
        out->format = INK_IMAGE_WEBP;
        if (!memcmp(p + 12, "VP8X", 4)) { out->width = 1 + (le32(p + 24) & 0xffffff); out->height = 1 + (le32(p + 27) & 0xffffff); }
        else if (!memcmp(p + 12, "VP8 ", 4)) { out->width = (p[26] | p[27] << 8) & 0x3fff; out->height = (p[28] | p[29] << 8) & 0x3fff; }
        else if (!memcmp(p + 12, "VP8L", 4) && size >= 25) { uint32_t b = le32(p + 21); out->width = (b & 0x3fff) + 1; out->height = (b >> 14 & 0x3fff) + 1; }
    } else return INK_IMAGE_ERROR;
    int w, h, channels;
    if ((out->format == INK_IMAGE_JPEG || out->format == INK_IMAGE_PNG) && stbi_info_from_memory(p, (int)size, &w, &h, &channels)) {
        out->width = (unsigned)w; out->height = (unsigned)h;
    }
    if (out->orientation >= 5) { unsigned t = out->width; out->width = out->height; out->height = t; }
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
