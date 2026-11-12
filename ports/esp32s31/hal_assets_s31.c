/* INKA: little-endian header (magic, count, bytes), entries (name[96], offset, bytes). */
#include <stdint.h>
#include <string.h>
#include "esp_partition.h"
#include "ink/hal_assets.h"
#include "ink/osal.h"
#include "reader_miniz/miniz.h"
extern const uint8_t ink_embedded_start[] asm("_binary_ink_embedded_bin_start");
extern const uint8_t ink_embedded_end[] asm("_binary_ink_embedded_bin_end");
static const void *g_fonts;
static esp_partition_mmap_handle_t g_mapping;
void ink_s31_assets_init(void) {
    const esp_partition_t *p = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, "font_data");
    if (p && p->address == 0xae4000 && p->size == 5 * 1024 * 1024)
        esp_partition_mmap(p, 0, 4608 * 1024, ESP_PARTITION_MMAP_DATA, &g_fonts, &g_mapping);
}
static uint32_t word(const uint8_t *p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static const void *find(const uint8_t *p, size_t cap, const char *name, size_t *size) {
    if (!p || cap < 12 || memcmp(p, "INKA", 4)) return NULL;
    uint32_t count = word(p + 4), bytes = word(p + 8);
    if (bytes > cap || bytes < 12 || count > (bytes - 12) / 104) return NULL;
    for (uint32_t i = 0; i < count; i++) {
        const uint8_t *e = p + 12 + i * 104;
        uint32_t off = word(e + 96), stored = word(e + 100), len = stored & 0x7fffffffu;
        if (!memchr(e, 0, 96) || off < 12 + count * 104 || off > bytes || len > bytes - off) return NULL;
        if (!strcmp((const char *)e, name)) {
            if (stored & 0x80000000u) {
                if (len < 4 || !word(p + off) || word(p + off) > 5 * 1024 * 1024) return NULL;
                size_t want = word(p + off), in = len - 4, out = want;
                const char *prev = osal_mem_tag("assets");
                uint8_t *buf = osal_malloc(want, OSAL_MEM_EXTERNAL);
                tinfl_decompressor *d = osal_malloc(sizeof *d, OSAL_MEM_EXTERNAL);
                osal_mem_tag(prev);
                tinfl_status status = TINFL_STATUS_FAILED;
                if (d && buf) {
                    tinfl_init(d);
                    status = tinfl_decompress(d, p + off + 4, &in, buf, buf, &out,
                        TINFL_FLAG_PARSE_ZLIB_HEADER | TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF);
                }
                osal_free(d);
                if (status != TINFL_STATUS_DONE || out != want || in != len - 4) { osal_free(buf); return NULL; }
                if (size) *size = want;
                return buf;
            }
            if (size) *size = len;
            return p + off;
        }
    }
    return NULL;
}
const void *hal_asset_map(const char *name, size_t *size) {
    if (size) *size = 0;
    if (!name) return NULL;
    const void *p = find(g_fonts, 4608 * 1024, name, size);
    return p ? p : find(ink_embedded_start, (size_t)(ink_embedded_end - ink_embedded_start), name, size);
}
void hal_asset_unmap(const void *data) {
    uintptr_t p = (uintptr_t)data, fonts = (uintptr_t)g_fonts;
    if (data && !(p >= fonts && p - fonts < 4608 * 1024) &&
        !(p >= (uintptr_t)ink_embedded_start && p < (uintptr_t)ink_embedded_end)) osal_free((void *)data);
}
void hal_asset_access(const void *p, size_t len) { (void)p; (void)len; }
