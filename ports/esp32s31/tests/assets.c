#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "storage_idf.h"
#include "ink/hal_assets.h"
#include "ink/osal.h"
void ink_s31_assets_init(void);
static uint8_t first[4608 * 1024];
static const esp_partition_t fonts = {0xae4000, 5242880};
static unsigned mappings;
static size_t live, peak;
static unsigned allocations;
static unsigned fail_at;
void *osal_malloc(size_t n, uint32_t caps) {
    assert(caps == OSAL_MEM_EXTERNAL);
    if (++allocations == fail_at) return NULL;
    size_t *p = malloc(n + sizeof *p); assert(p);
    *p = n; live += n; if (live > peak) peak = live;
    return p + 1;
}
void osal_free(void *p) { if (p) { size_t *v = (size_t *)p - 1; live -= *v; free(v); } }
const char *osal_mem_tag(const char *tag) { (void)tag; return "test"; }
const esp_partition_t *esp_partition_find_first(int type, int sub, const char *name) {
    assert(type == ESP_PARTITION_TYPE_DATA && sub == ESP_PARTITION_SUBTYPE_ANY && !strcmp(name, "font_data"));
    return &fonts;
}
int esp_partition_mmap(const esp_partition_t *p, size_t off, size_t n, int mode,
                       const void **out, esp_partition_mmap_handle_t *handle) {
    assert(p == &fonts && !off && n == sizeof first && mode == ESP_PARTITION_MMAP_DATA);
    mappings++; *out = first; *handle = 1; return 0;
}
int main(int argc, char **argv) {
    assert(argc == 2);
    FILE *f = fopen(argv[1], "rb"); assert(f);
    size_t n = fread(first, 1, sizeof first, f); assert(n > 12 && !fclose(f));
    ink_s31_assets_init();
    size_t size = 999;
    const void *a = hal_asset_map("fonts/a.inkf", &size);
    assert(a && size == 4 && !memcmp(a, "font", 4));
    a = hal_asset_map("icons/z.inki", &size);
    assert(a && size == 4 && !memcmp(a, "icon", 4));
    hal_asset_unmap(a);
    for (unsigned i = 0; i < 2; i++) {
        a = hal_asset_map("icons/compressed.inki", &size);
        assert(a && size == 4096 && live == size && peak > live);
        for (size_t j = 0; j < size; j++) assert(((const char *)a)[j] == "icon"[j % 4]);
        hal_asset_unmap(a);
        assert(live == 0);
    }
    assert(!hal_asset_map("icons/corrupt.inki", &size) && !size && !live);
    for (unsigned i = 1; i <= 2; i++) {
        fail_at = allocations + i;
        assert(!hal_asset_map("icons/compressed.inki", &size) && !size && !live);
    }
    fail_at = 0;
    assert(!hal_asset_map("absent", &size) && size == 0);
    assert(!hal_asset_map(NULL, &size) && size == 0);
    assert(!hal_asset_map("fonts/a.inkf/extra", &size));
    assert(mappings == 1);
    first[8] = 0xff; first[9] = 0xff; first[10] = 0xff; first[11] = 0xff;
    assert(!hal_asset_map("fonts/a.inkf", &size) && size == 0);
    memset(first, 255, sizeof first);
    assert(!hal_asset_map("fonts/a.inkf", &size) && hal_asset_map("icons/z.inki", &size));
    a = hal_asset_map("icons/z.inki", &size);
    hal_asset_access(a, 4); hal_asset_unmap(a);
    assert(!live);
    puts("two asset packs OK"); return 0;
}
