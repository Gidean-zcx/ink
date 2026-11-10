/* INKA: little-endian header (magic, count, bytes), entries (name[96], offset, bytes). */
#include <stdint.h>
#include <string.h>
#include "esp_partition.h"
#include "ink/hal_assets.h"
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
        uint32_t off = word(e + 96), len = word(e + 100);
        if (!memchr(e, 0, 96) || off < 12 + count * 104 || off > bytes || len > bytes - off) return NULL;
        if (!strcmp((const char *)e, name)) {
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
void hal_asset_unmap(const void *data) { (void)data; } /* Both packs live for the whole boot. */
void hal_asset_access(const void *p, size_t len) { (void)p; (void)len; }
