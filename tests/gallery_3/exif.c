/* gallery-3 exif_orientation: ink_image_decode turns JPEG pixels upright for all eight EXIF orientations, and
 * ink_image_probe reads the same head; broken, out-of-range and cut EXIF counts as no orientation and never crashes.
 * The decoder is compiled into this driver so that run.py --reverse can swap it for a mutated copy. */
#include <stdlib.h>
#include <string.h>
#include "ink/hal_audio.h"
#include "ink/osal.h"
#include "ink/probe.h"
#include "ink/sys/exec.h"
#include "../../system/services/media/image.c"
#include "gallery3_fixtures.h"

#define CHECK(c, ...) do { if (!(c)) osal_panic("gallery-3 exif: " __VA_ARGS__); ink_probe_count("test.pass"); } while (0)
static const unsigned QUARTERS[4] = {0, 255, 96, 192}; /* top-left, top-right, bottom-left, bottom-right */

static unsigned at(const ink_image_t *im, unsigned x, unsigned y) { return im->pixels[(size_t)y * im->width + x]; }
static bool near(unsigned a, unsigned b) { return (a > b ? a - b : b - a) < 16; }
static void upright(const uint8_t *data, size_t size, unsigned o) {
    ink_image_t im; ink_image_info_t info;
    CHECK(ink_image_decode(data, size, &im) == INK_IMAGE_OK, "orientation %u decodes", o);
    CHECK(im.width == 32 && im.height == 16, "orientation %u is 32 x 16 upright (got %u x %u)", o, im.width, im.height);
    unsigned corner[4] = {at(&im, 2, 2), at(&im, 29, 2), at(&im, 2, 13), at(&im, 29, 13)};
    for (int k = 0; k < 4; k++) CHECK(near(corner[k], QUARTERS[k]), "orientation %u corner %d is %u, not %u", o, k, corner[k], QUARTERS[k]);
    ink_image_free(&im);
    CHECK(ink_image_probe(data, size, &info) == INK_IMAGE_OK && info.format == INK_IMAGE_JPEG && info.orientation == o, "probe reads orientation %u", o);
    CHECK(info.width == 32 && info.height == 16, "probe gives the upright size for orientation %u", o);
}
static void broken(const uint8_t *data, size_t size, const char *what) {
    ink_image_t im; ink_image_info_t info;
    CHECK(ink_image_probe(data, size, &info) == INK_IMAGE_OK && info.orientation == 1 && !info.taken[0], "%s counts as no orientation", what);
    int r = ink_image_decode(data, size, &im);
    CHECK(r == INK_IMAGE_ERROR || (r == INK_IMAGE_OK && im.width == 16 && im.height == 32), "%s decodes as stored or not at all", what);
    ink_image_free(&im);
}
static void cuts(const uint8_t *data, size_t size) { /* every prefix of the head: never past the end (ASan) */
    ink_image_info_t info; ink_image_t im;
    for (size_t n = 0; n < 1200 && n < size; n++) {
        ink_image_probe(data, n, &info);
        CHECK(info.orientation >= 1 && info.orientation <= 8, "a head cut at %zu bytes stays sane", n);
    }
    uint8_t *copy = osal_malloc(size, OSAL_MEM_EXTERNAL); CHECK(copy != NULL, "copy for byte flips");
    uint32_t seed = 12345;
    for (int round = 0; round < 3000; round++) { /* flip bytes in the EXIF block only, deterministically */
        memcpy(copy, data, size);
        for (int k = 0; k < 4; k++) { seed = seed * 1103515245u + 12345u; copy[20 + (seed >> 8) % 200] ^= (uint8_t)(seed >> 3); }
        ink_image_probe(copy, size, &info);
        CHECK(info.orientation >= 1 && info.orientation <= 8, "flipped EXIF round %d stays sane", round);
        if (round % 300 == 0 && ink_image_decode(copy, size, &im) == INK_IMAGE_OK) ink_image_free(&im);
    }
    osal_free(copy);
}
static osal_sem_t *done;
static void run(void *arg) {
    (void)arg;
    const uint8_t *all[8] = {orient_1, orient_2, orient_3, orient_4, orient_5, orient_6, orient_7, orient_8};
    const size_t sizes[8] = {sizeof orient_1, sizeof orient_2, sizeof orient_3, sizeof orient_4, sizeof orient_5, sizeof orient_6, sizeof orient_7, sizeof orient_8};
    for (unsigned o = 1; o <= 8; o++) upright(all[o - 1], sizes[o - 1], o);
    ink_image_info_t info;
    CHECK(ink_image_probe(orient_6, sizeof orient_6, &info) == 0 && !strcmp(info.taken, "2026:11:02 10:20:00"), "probe reads DateTimeOriginal from the Exif IFD");
    broken(broken_ifd_out_of_range, sizeof broken_ifd_out_of_range, "an IFD offset past the block");
    broken(broken_count_huge, sizeof broken_count_huge, "an entry count past the block");
    broken(broken_bad_magic, sizeof broken_bad_magic, "a TIFF header without 42");
    broken(broken_orientation_9, sizeof broken_orientation_9, "orientation 9");
    broken(broken_length_past_end, sizeof broken_length_past_end, "an APP1 length past the file");
    cuts(orient_6, sizeof orient_6);
    ink_image_t im;
    CHECK(ink_image_decode(big_jpeg, sizeof big_jpeg, &im) == INK_IMAGE_LIMIT, "a 1600 x 1200 JPEG is over the decoder's limit");
    CHECK(ink_image_probe(big_jpeg, sizeof big_jpeg, &info) == 0 && info.width == 1600 && info.height == 1200, "probe still reads its size");
    CHECK(ink_image_probe(webp_head, sizeof webp_head, &info) == 0 && info.format == INK_IMAGE_WEBP && info.width == 64 && info.height == 48, "probe knows a WebP head");
    CHECK(ink_image_probe("hello, not a picture", 20, &info) == INK_IMAGE_ERROR && info.format == INK_IMAGE_UNKNOWN, "unknown bytes are no format");
    ink_probe_count("test.finished");
    osal_sem_give(done);
}
void ink_main(void) {
    static ink_exec_job_t job;
    CHECK(hal_audio_play_start() == 0, "platform audio boot completed");
    hal_audio_play_stop(false);
    CHECK(ink_exec_init() == 0 && ink_exec_ready(INK_EXEC_MEDIA) == 0, "media executor ready");
    done = osal_sem_create(0, 1);
    CHECK(done != NULL && ink_exec_post(INK_EXEC_MEDIA, &job, run, NULL) == 0, "post the test");
    CHECK(osal_sem_take(done, 9000000) == OSAL_OK, "exif test completed");
    osal_sem_delete(done);
}
