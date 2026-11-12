/* Exercise the unmodified file-backed Mogo font engine, outside the simulator. */
#include "ttf_font.h"
#include <assert.h>
#include <stdalign.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const uint8_t ink_test_builtin[32] asm("_binary_builtin_ttf_start") = {0};
asm(".globl _binary_builtin_ttf_end\n.set _binary_builtin_ttf_end, _binary_builtin_ttf_start + 32");
typedef union { max_align_t alignment; size_t bytes; } allocation_t;
static size_t live, peak;
static uint8_t canvas[256 * 160];

void *heap_caps_malloc(size_t bytes, int caps) {
    (void)caps;
    allocation_t *p = malloc(sizeof(*p) + bytes);
    assert(p);
    p->bytes = bytes;
    live += bytes;
    if (live > peak) peak = live;
    return p + 1;
}
void heap_caps_free(void *pointer) {
    if (!pointer) return;
    allocation_t *p = (allocation_t *)pointer - 1;
    assert(live >= p->bytes);
    live -= p->bytes;
    free(p);
}
void *heap_caps_calloc(size_t count, size_t bytes, int caps) {
    void *p = heap_caps_malloc(count * bytes, caps);
    memset(p, 0, count * bytes);
    return p;
}
void *heap_caps_realloc(void *pointer, size_t bytes, int caps) {
    void *p = heap_caps_malloc(bytes, caps);
    if (pointer) {
        size_t old = ((allocation_t *)pointer - 1)->bytes;
        memcpy(p, pointer, old < bytes ? old : bytes);
        heap_caps_free(pointer);
    }
    return p;
}
size_t heap_caps_get_largest_free_block(int caps) {
    (void)caps;
    return 16u * 1024u * 1024u;
}
const char *app_settings_fonts_dir(void) { return "/absent"; }
const char *app_settings_font_path(void) { return "builtin"; }
void epd_draw_pixel(int x, int y, uint8_t gray, uint8_t *pixels) {
    assert(x >= 0 && x < 256 && y >= 0 && y < 160);
    pixels[y * 256 + x] = gray;
}
static size_t painted(void) {
    size_t count = 0;
    for (size_t i = 0; i < sizeof canvas; ++i) count += canvas[i] != 255;
    return count;
}
int main(int argc, char **argv) {
    if (argc != 2) return 2;
    if (ttf_font_open(argv[1]) != ESP_OK) {
        ink_mogo_font_shutdown();
        puts("native file font rejected");
        return 3;
    }
    assert(ttf_font_ready() && !ttf_font_is_builtin());
    assert(ttf_font_has_text("中文阅读，Aa09"));
    assert(!ttf_font_has_text("\xF0\xA0\x80\x80"));
    for (int px = 36; px <= 90; px += 18) {
        memset(canvas, 255, sizeof canvas);
        ttf_draw_text_px(canvas, 8, 100, px, "中文，A", EPD_DRAW_ALIGN_LEFT, 0, 15);
        assert(painted() > 100);
    }
    memset(canvas, 255, sizeof canvas);
    ttf_draw_text_px(canvas, 8, 100, 48, "\xF0\xA0\x80\x80", EPD_DRAW_ALIGN_LEFT, 0, 15);
    assert(painted() > 20 && ttf_text_width_px(48, "\xF0\xA0\x80\x80") > 0);
    uint8_t missing[sizeof canvas];
    memcpy(missing, canvas, sizeof missing);
    memset(canvas, 255, sizeof canvas);
    ttf_draw_text_px(canvas, 8, 100, 48, "\xF4\x8F\xBF\xBF", EPD_DRAW_ALIGN_LEFT, 0, 15);
    assert(memcmp(missing, canvas, sizeof missing) == 0);
    FILE *file = fopen(argv[1], "rb");
    assert(file && fseek(file, 0, SEEK_END) == 0);
    long bytes = ftell(file);
    assert(bytes > 0 && fseek(file, 0, SEEK_SET) == 0);
    uint8_t *embedded = heap_caps_malloc((size_t)bytes, 3);
    assert(fread(embedded, 1, (size_t)bytes, file) == (size_t)bytes && fclose(file) == 0);
    int slot = ttf_font_open_mem(embedded, (size_t)bytes, "test-embedded");
    assert(slot > 0 && ttf_font_slot_ready(slot));
    (void)ttf_font_select(slot);
    ink_mogo_font_shutdown();
    assert(live == 0);
    ink_mogo_font_shutdown();
    assert(live == 0 && !ttf_font_ready() && !ttf_font_slot_ready(slot));
    assert(ttf_font_selected() == TTF_FONT_SLOT_SYSTEM);
    printf("native file font: Chinese/Latin/size/missing-glyph/unload passed; host heap peak=%zu\n", peak);
    return 0;
}
