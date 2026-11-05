/* Build-time SVG rasterizer. Output: INKI, u16 width/height, BGRA8888 pixels. */
#define NANOSVG_IMPLEMENTATION
#define NANOSVGRAST_IMPLEMENTATION
#include "vendor/nanosvg.h"
#include "vendor/nanosvgrast.h"
#include <stdint.h>
#include <stdbool.h>

int main(int argc, char **argv) {
    if (argc != 4 && argc != 5) { fprintf(stderr, "usage: ink-icon-raster source.svg size output.inki\n"); return 1; }
    int size = atoi(argv[2]);
    if (size != 28 && size != 36 && size != 44 && size != 64 && size != 88) return 1;
    NSVGimage *im = nsvgParseFromFile(argv[1], "px", 96);
    NSVGrasterizer *rast = nsvgCreateRasterizer();
    uint8_t *pixels = calloc((size_t)size * size, 4);
    if (!im || !im->shapes || !rast || !pixels) { fprintf(stderr, "invalid/missing icon: %s\n", argv[1]); return 1; }
    nsvgRasterize(rast, im, 0, 0, size / im->width, pixels, size, size, size * 4);
    for (int i = 0; i < size * size; i++) { uint8_t r = pixels[4 * i]; pixels[4 * i] = pixels[4 * i + 2]; pixels[4 * i + 2] = r; }
    if (argc == 5) for (int i = 0; i < size * size; i++) for (int c = 0; c < 3; c++) pixels[4 * i + c] = (uint8_t)(255 - pixels[4 * i + c]);
    FILE *out = fopen(argv[3], "wb");
    const uint8_t hdr[] = {'I', 'N', 'K', 'I', (uint8_t)size, 0, (uint8_t)size, 0};
    bool ok = out && fwrite(hdr, 1, sizeof hdr, out) == sizeof hdr && fwrite(pixels, 4, (size_t)size * size, out) == (size_t)size * size;
    if (out && fclose(out)) ok = false;
    free(pixels); nsvgDelete(im); nsvgDeleteRasterizer(rast);
    return ok ? 0 : 1;
}
