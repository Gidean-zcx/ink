/* Icons are packed on flash (tools/icongen/raster.c) and unpacked once, on first use, into PSRAM. */
/* ui-scale-runtime: dimension x,y,px Forwarded icon geometry; ink_ui_icon callers are checked. */
#include <stdio.h>
#include <string.h>

#include "ink/hal_assets.h"
#include "ink/osal.h"
#include "ink/ui_theme.h"

typedef struct { char key[80]; lv_image_dsc_t dsc; } icon_asset_t;
static icon_asset_t cache[128] INK_PSRAM_BSS;
static int used;

static bool unpack(const uint8_t *in, size_t n, uint8_t *out, size_t want) {
    size_t at = 0;
    for (size_t i = 0; i < n && at < want;) {
        uint8_t code = in[i++];
        size_t run = code < 128 ? code + 1u : code - 125u;
        if (at + run > want || i + (code < 128 ? run : 1) > n) return false;
        if (code < 128) memcpy(out + at, in + i, run);
        else memset(out + at, in[i], run);
        i += code < 128 ? run : 1;
        at += run;
    }
    return at == want;
}

lv_obj_t *ink_ui_icon(lv_obj_t *parent, const char *name, int px, ink_ui_icon_look_t look, int x, int y, bool white) {
    static const char *const LOOK[] = {"line", "solid"};
    char key[80];
    snprintf(key, sizeof key, "icons/%s-%s-%d.inki", name, LOOK[look], px);
    icon_asset_t *asset = NULL;
    for (int i = 0; i < used; i++) if (!strcmp(cache[i].key, key)) { asset = &cache[i]; break; }
    if (!asset && used < (int)(sizeof cache / sizeof *cache)) {
        size_t size = 0, want = (size_t)px * px;
        const uint8_t *data = hal_asset_map(key, &size);
        const char *prev = osal_mem_tag("icons");
        uint8_t *pixels = osal_malloc(want, OSAL_MEM_EXTERNAL);
        osal_mem_tag(prev);
        if (!data || !pixels || size < 8 || memcmp(data, "INKI", 4) || data[4] != px || data[6] != px)
            osal_panic("missing or invalid required icon: %s", key);
        hal_asset_access(data, size);
        if (!unpack(data + 8, size - 8, pixels, want)) osal_panic("damaged icon: %s", key);
        asset = &cache[used++];
        snprintf(asset->key, sizeof asset->key, "%s", key);
        asset->dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
        asset->dsc.header.cf = LV_COLOR_FORMAT_A8;
        asset->dsc.header.w = (uint16_t)px;
        asset->dsc.header.h = (uint16_t)px;
        asset->dsc.header.stride = (uint16_t)px;
        asset->dsc.data_size = (uint32_t)want;
        asset->dsc.data = pixels;
    }
    if (!asset) osal_panic("icon descriptor cache full");
    lv_obj_t *im = lv_image_create(parent);
    lv_image_set_src(im, &asset->dsc);
    lv_obj_set_pos(im, ink_ui_px(x), ink_ui_px(y));
    if (ink_ui_px(px) != px) {
        lv_obj_set_size(im, ink_ui_px(px), ink_ui_px(px));
        lv_image_set_inner_align(im, LV_IMAGE_ALIGN_STRETCH);
    }
    lv_obj_set_style_image_recolor(im, white ? ink_ui_paper() : ink_ui_ink(), 0);
    lv_obj_set_style_image_recolor_opa(im, LV_OPA_COVER, 0);
    return im;
}
