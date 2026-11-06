/* Build-time SVG rasterizer. Output: "INKI", u16 width, u16 height, then packed ink coverage, one
 * byte per pixel in 16 levels (what the panel can show). What an icon leaves white is left out, so
 * one file draws in black on paper and in white on black.
 * Packing: a byte n < 128 is followed by n + 1 literal bytes; n >= 128 by one byte to repeat n - 125 times. */
#define NANOSVG_IMPLEMENTATION
#define NANOSVGRAST_IMPLEMENTATION
#include "vendor/nanosvg.h"
#include "vendor/nanosvgrast.h"
#include <stdint.h>
#include <stdbool.h>

static size_t pack(const uint8_t *in, size_t n, uint8_t *out) {
    size_t used = 0;
    for (size_t i = 0; i < n;) {
        size_t same = 1;
        while (i + same < n && in[i + same] == in[i] && same < 130) same++;
        if (same >= 3) {
            out[used++] = (uint8_t)(same + 125);
            out[used++] = in[i];
            i += same;
            continue;
        }
        size_t count = 0;
        while (i + count < n && count < 128 && !(i + count + 2 < n && in[i + count] == in[i + count + 1] && in[i + count] == in[i + count + 2])) count++;
        out[used++] = (uint8_t)(count - 1);
        memcpy(out + used, in + i, count);
        used += count;
        i += count;
    }
    return used;
}

int main(int argc, char **argv) {
    if (argc != 4) { fprintf(stderr, "usage: ink-icon-raster source.svg size output.inki\n"); return 1; }
    int size = atoi(argv[2]);
    if (size <= 0) return 1;
    NSVGimage *im = nsvgParseFromFile(argv[1], "px", 96);
    NSVGrasterizer *rast = nsvgCreateRasterizer();
    size_t count = (size_t)size * size;
    uint8_t *pixels = calloc(count, 4), *cover = malloc(count), *packed = malloc(2 * count + 8);
    if (!im || !im->shapes || !rast || !pixels || !cover || !packed) { fprintf(stderr, "invalid/missing icon: %s\n", argv[1]); return 1; }
    nsvgRasterize(rast, im, 0, 0, size / im->width, pixels, size, size, size * 4);
    for (size_t i = 0; i < count; i++) {
        const uint8_t *p = pixels + 4 * i;
        int ink = p[3] * (255 - (p[0] + 2 * p[1] + p[2]) / 4) / 255;
        cover[i] = (uint8_t)((ink + 8) / 17 * 17);
    }
    size_t used = pack(cover, count, packed);
    FILE *out = fopen(argv[3], "wb");
    const uint8_t hdr[] = {'I', 'N', 'K', 'I', (uint8_t)size, 0, (uint8_t)size, 0};
    bool ok = out && fwrite(hdr, 1, sizeof hdr, out) == sizeof hdr && fwrite(packed, 1, used, out) == used;
    if (out && fclose(out)) ok = false;
    free(pixels); free(cover); free(packed); nsvgDelete(im); nsvgDeleteRasterizer(rast);
    return ok ? 0 : 1;
}
