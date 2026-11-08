#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "ink/hal_display.h"
#include "ink/osal.h"
#include "ink/probe.h"
#include "ink/sys/exec.h"
#include "ink/sys/image.h"
#include "panel_e0470a14.h"
#include "image_fixtures.h"

#define CHECK(c, message) do { if (!(c)) osal_panic("image: %s", message); ink_probe_count("test.pass"); } while (0)
#define DECODE(f, out) ink_image_decode(f, sizeof(f), out)
#define THUMB(c, id, v, f, out) ink_image_thumbnail(c, id, v, f, sizeof(f), out)

static void solid(const ink_image_t *im, unsigned value) {
    CHECK(im->pixels != NULL && im->width && im->height, "nonempty image");
    for (size_t i = 0; i < (size_t)im->width * im->height; i++)
        CHECK(im->pixels[i] == value, "expected decoded/dithered pixel value");
}

static void decode(void) {
    ink_image_t im;
    CHECK(DECODE(color_png, &im) == 0 && im.width == 17 && im.height == 9, "real color PNG dimensions");
    solid(&im, 123); ink_image_free(&im);
    CHECK(DECODE(palette_png, &im) == 0, "palette PNG"); solid(&im, 123); ink_image_free(&im);
    CHECK(DECODE(color_jpeg, &im) == 0 && im.width == 17 && im.height == 9, "real baseline color JPEG");
    CHECK(im.pixels[0] >= 120 && im.pixels[0] <= 126, "JPEG pixel brightness"); ink_image_free(&im);
    CHECK(DECODE(gray_jpeg, &im) == 0 && im.width == 19 && im.height == 13, "baseline grayscale JPEG");
    solid(&im, 90); ink_image_free(&im);
    CHECK(DECODE(alpha_png, &im) == 0, "RGBA PNG"); solid(&im, 255); ink_image_free(&im);
    CHECK(DECODE(max_png, &im) == 0 && (size_t)im.width * im.height == INK_IMAGE_MAX_PIXELS,
          "pixel limit is inclusive"); ink_image_free(&im);
    CHECK(DECODE(large_png, &im) == INK_IMAGE_LIMIT && !im.pixels && !im.width && !im.height, "oversized PNG area");
    CHECK(DECODE(large_jpeg, &im) == INK_IMAGE_LIMIT && !im.pixels, "oversized JPEG area");
    CHECK(DECODE(edge_png, &im) != 0 && !im.pixels, "oversized PNG edge");
    CHECK(DECODE(edge_jpeg, &im) != 0 && !im.pixels, "oversized JPEG edge");
    CHECK(ink_image_decode(NULL, 0, &im) != 0 && !im.pixels, "empty input");
    CHECK(ink_image_decode("invalid", 7, &im) != 0 && !im.pixels, "bad signature");
    CHECK(ink_image_decode(color_png, sizeof(color_png)/2, &im) != 0 && !im.pixels, "truncated PNG");
    CHECK(ink_image_decode(color_png, sizeof(color_png)-12, &im) != 0 && !im.pixels, "missing PNG IEND");
    CHECK(ink_image_decode(color_jpeg, sizeof(color_jpeg)/2, &im) != 0 && !im.pixels, "truncated JPEG");
    CHECK(ink_image_decode(color_jpeg, sizeof(color_jpeg)-2, &im) != 0 && !im.pixels, "missing JPEG EOI");
    uint8_t *bad = osal_malloc(sizeof(color_png), OSAL_MEM_EXTERNAL);
    CHECK(bad != NULL, "corruption fixture allocated");
    memcpy(bad, color_png, sizeof(color_png)); bad[29] ^= 1;
    CHECK(ink_image_decode(bad, sizeof(color_png), &im) != 0 && !im.pixels, "PNG CRC error");
    memset(bad + 33, 255, 4);
    CHECK(ink_image_decode(bad, sizeof(color_png), &im) != 0 && !im.pixels, "invalid chunk length");
    osal_free(bad);
    uint8_t *large = osal_malloc(INK_IMAGE_MAX_INPUT + 1u, OSAL_MEM_EXTERNAL);
    CHECK(large != NULL, "encoded input limit fixture allocated");
    CHECK(ink_image_decode(large, INK_IMAGE_MAX_INPUT + 1u, &im) == INK_IMAGE_LIMIT && !im.pixels,
          "encoded input size limit"); osal_free(large);
}

static void dither(void) {
    ink_image_t im = {256, 256, osal_malloc(256u * 256u, OSAL_MEM_EXTERNAL)};
    CHECK(im.pixels != NULL, "gradient allocated in PSRAM");
    for (unsigned l = 0; l < 16; l++)
        CHECK(hal_display_level_luminance(l) == PANEL_LEVEL[l], "HAL reuses calibrated panel table");
    double original = 0, shown = 0;
    for (unsigned y = 0; y < 256; y++) for (unsigned x = 0; x < 256; x++) {
        im.pixels[y * 256 + x] = x;
        original += PANEL_LEVEL[0] + (PANEL_LEVEL[15] - PANEL_LEVEL[0]) * x / 255.;
    }
    ink_image_dither(&im);
    unsigned seen = 0;
    for (size_t i = 0; i < 256u * 256u; i++) {
        CHECK(im.pixels[i] % 17 == 0, "only 16 factory gray codes");
        shown += PANEL_LEVEL[im.pixels[i] / 17]; seen |= 1u << (im.pixels[i] / 17);
    }
    CHECK(seen == 65535u, "gradient uses every factory level");
    CHECK(fabs(shown - original) / (256u * 256u) < .00999, "mean luminance error below smallest panel step");
    ink_probe_value("test.image.mean_error", fabs(shown - original) / (256u * 256u));
    ink_image_free(&im);
    uint8_t tile[16], previous[16];
    im = (ink_image_t){4, 4, tile};
    for (unsigned gray = 0; gray < 256; gray++) {
        memset(tile, gray, sizeof tile); ink_image_dither(&im);
        shown = 0;
        for (unsigned i = 0; i < 16; i++) shown += PANEL_LEVEL[tile[i] / 17];
        double target = PANEL_LEVEL[0] + (PANEL_LEVEL[15] - PANEL_LEVEL[0]) * gray / 255.;
        CHECK(fabs(shown / 16 - target) < .00999, "every flat tile preserves mean brightness");
        memcpy(previous, tile, sizeof tile);
        memset(tile, gray, sizeof tile); ink_image_dither(&im);
        CHECK(!memcmp(previous, tile, sizeof tile), "dither is deterministic");
    }
}

static void cache(void) {
    osal_mem_stats_t before, after, internal, internal_after;
    osal_mem_stats(OSAL_MEM_EXTERNAL, &before); osal_mem_stats(OSAL_MEM_INTERNAL, &internal);
    ink_thumb_cache_t *c = ink_thumb_cache_create();
    CHECK(c != NULL, "cache context allocated");
    ink_image_t im;
    CHECK(THUMB(c, 101, 1, wide_png, &im) == 0 && im.width == 128 && im.height == 32, "wide image aspect ratio");
    CHECK(THUMB(c, 102, 1, tall_png, &im) == 0 && im.width == 32 && im.height == 128, "tall image aspect ratio");
    CHECK(THUMB(c, 103, 1, thin_png, &im) == 0 && im.width == 128 && im.height == 1, "thin image nonzero edge");
    CHECK(THUMB(c, 104, 1, color_png, &im) == 0 && im.width == 17 && im.height == 9, "small image not upscaled");
    ink_thumb_cache_destroy(c); c = ink_thumb_cache_create();
    CHECK(c != NULL, "fresh FIFO cache");
    for (unsigned id = 1; id <= 4; id++) {
        CHECK(THUMB(c, id, 1, black_png, &im) == 0, "fill FIFO");
        CHECK(im.width == INK_THUMB_EDGE && im.height == INK_THUMB_EDGE &&
              (size_t)im.width * im.height <= INK_THUMB_BYTES, "per-thumbnail edge and byte cap");
        solid(&im, 0);
    }
    CHECK(ink_image_thumbnail(c, 1, 1, NULL, 0, &im) == 0, "cache hit without decoding");
    CHECK(THUMB(c, 5, 1, white_png, &im) == 0, "over-capacity insertion succeeds"); solid(&im, 255);
    CHECK(ink_image_thumbnail(c, 1, 1, NULL, 0, &im) != 0 && !im.pixels, "oldest entry evicted despite recent hit");
    CHECK(ink_image_thumbnail(c, 2, 1, NULL, 0, &im) == 0, "next-oldest retained");
    for (unsigned id = 6; id < 70; id++) {
        CHECK(THUMB(c, id, 1, white_png, &im) == 0, "repeated bounded insertion");
        osal_mem_stats(OSAL_MEM_EXTERNAL, &after);
        CHECK(before.free >= after.free && before.free - after.free <= INK_THUMB_CACHE_BYTES + 512u,
              "total live PSRAM includes bounded metadata");
        CHECK(after.alloc_count <= before.alloc_count + INK_THUMB_SLOTS + 1, "no decoded source retained");
    }
    osal_mem_stats(OSAL_MEM_INTERNAL, &internal_after);
    CHECK(internal.free == internal_after.free && internal.alloc_count == internal_after.alloc_count,
          "no internal RAM allocation");
    ink_thumb_cache_destroy(c);
    osal_mem_stats(OSAL_MEM_EXTERNAL, &after);
    CHECK(before.free == after.free && before.alloc_count == after.alloc_count, "destroy frees all image buffers");
}

static void version(void) {
    ink_thumb_cache_t *c = ink_thumb_cache_create(), *other = ink_thumb_cache_create();
    CHECK(c && other, "independent cache contexts");
    ink_image_t im, old;
    CHECK(THUMB(c, 77, 1, black_png, &im) == 0, "version one"); solid(&im, 0);
    CHECK(THUMB(c, 77, 2, white_png, &im) == 0, "same ID new version"); solid(&im, 255);
    CHECK(ink_image_thumbnail(c, 77, 2, NULL, 0, &im) == 0, "new version cached"); solid(&im, 255);
    CHECK(THUMB(other, 77, 2, black_png, &old) == 0, "another caller has independent same-key data"); solid(&old, 0);
    CHECK(ink_image_thumbnail(c, 77, 3, NULL, 0, &im) != 0 && !im.pixels, "failed new version does not return old data");
    CHECK(ink_image_thumbnail(c, 77, 2, NULL, 0, &im) != 0 && !im.pixels, "old entry removed on version change");
    ink_thumb_cache_destroy(c); ink_thumb_cache_destroy(other);
}

static osal_sem_t *done;
static void run(void *arg) {
    const char *which = arg;
    CHECK(ink_exec_is_current(INK_EXEC_MEDIA), "runs on existing media executor");
    CHECK(which != NULL, "test selected");
    osal_mem_stats_t before, after;
    osal_mem_stats(OSAL_MEM_EXTERNAL, &before);
    if (!strcmp(which, "decode")) decode();
    else if (!strcmp(which, "dither")) dither();
    else if (!strcmp(which, "cache")) cache();
    else if (!strcmp(which, "version")) version();
    else CHECK(false, "unknown test");
    osal_mem_stats(OSAL_MEM_EXTERNAL, &after);
    CHECK(before.free == after.free && before.alloc_count == after.alloc_count, "test leaks no PSRAM");
    osal_sem_give(done);
}

void ink_main(void) {
    static ink_exec_job_t job;
    CHECK(ink_exec_init() == 0 && ink_exec_ready(INK_EXEC_MEDIA) == 0, "existing media executor ready");
    done = osal_sem_create(0, 1);
    CHECK(done != NULL, "completion semaphore");
    CHECK(ink_exec_post(INK_EXEC_MEDIA, &job, run, getenv("INK_IMAGE_TEST")) == 0, "post image test");
    CHECK(osal_sem_take(done, 9000000) == OSAL_OK, "image test completed");
    osal_sem_delete(done);
}
