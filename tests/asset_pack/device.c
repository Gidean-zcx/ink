/* Exercise the shipping HAL and a real page with an excluded state icon. */
#include <stdlib.h>
#include "ink/hal_assets.h"
#include "ink/ui_theme.h"
#define ink_main product_main
#include "../configured_boot.c"
#undef ink_main
#include "../integration/whole_product.h"

static bool missing;
static void mode(void *arg) { (void)arg; missing = getenv("INK_ASSET_MISSING") != NULL; }
static void icon_page(lv_obj_t *root, void *arg) {
    (void)arg;
    ink_ui_icon(root, "plus", INK_ICON_STATE, INK_ICON_LINE, 0, 0, false);
}
static const ink_activity_t PAGE = {.name = "test.asset.missing", .create = icon_page};
static void show_missing(void) { ink_ui_push(&PAGE, NULL); }
static void run(void *arg) {
    (void)arg;
    CHECK(WAIT_FOR(ink_member_space_open(), 10000), "member opens");
    test_start_home();
    if (missing) {
        on_ui(show_missing);
    } else {
        osal_mem_stats_t before, during, after;
        osal_mem_stats(OSAL_MEM_EXTERNAL, &before);
        size_t size = 0;
        const void *data = hal_asset_map("icons/mic-line-106.inki", &size);
        CHECK(data && size > 8 && !memcmp(data, "INKI", 4), "state icon inflates to INKI");
        osal_mem_stats(OSAL_MEM_EXTERNAL, &during);
        CHECK(during.free + size <= before.free, "inflated entry lives in PSRAM");
        hal_asset_unmap(data);
        osal_mem_stats(OSAL_MEM_EXTERNAL, &after);
        CHECK(after.free == before.free, "unmap reclaims the inflated entry and workspace");
        data = hal_asset_map("icons/mic-line-106.inki", &size);
        CHECK(data && !memcmp(data, "INKI", 4), "reclaimed entry can be inflated again");
        hal_asset_unmap(data);
    }
    ink_probe_count("test.finished");
    vtk_stop();
}
void ink_main(void) {
    vtk_host_call(mode, NULL);
    product_main();
    osal_task_cfg_t cfg = {.name = "asset-test", .fn = run, .stack_bytes = 16384,
                           .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start asset test");
}
