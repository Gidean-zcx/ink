#ifndef UI_FIX_PRODUCT
#define UI_FIX_PRODUCT "../../system/ui/shell/off_picture.c"
#endif
#include UI_FIX_PRODUCT
#include "common.h"
static int bookmark(int index, ink_card_t *out) {
    if (index) return 0;
    *out = (ink_card_t){.bookmark = true};
    snprintf(out->title, sizeof out->title, "合成阅读记录测试标题");
    snprintf(out->text, sizeof out->text, "这是一段用于检验大字号换行后仍可完整看见的合成书名和阅读位置");
    return 1;
}
static const ink_app_def_t book = {.id = "synthetic-book", .title = "合成书", .system_app = true, .card = bookmark};
static void show_off(void) { O.why = "shutdown"; draw(NULL); }
static void inspect_off(void) {
    fx_fits(O.card.body, O.card.panel);
    CHECK(strstr(O.body, "合成"), "S28 retained picture includes supplied bookmark");
    lv_area_t a, b; lv_obj_get_coords(O.card.title, &a); lv_obj_get_coords(O.card.body, &b);
    CHECK(a.y2 < b.y1 && b.y2 < ink_ui_px(INK_CANVAS_H - INK_MARGIN), "S28 shutdown heading and multiline reading position fit");
    CHECK(!lv_obj_has_flag(O.card.body, LV_OBJ_FLAG_CLICKABLE), "S28 powered off picture is static");
}
static void run(void *ctx) {
    (void)ctx; fx_start(); CHECK(!ink_app_register(&book), "register synthetic reading bookmark");
    on_ui(show_off); fx_capture("power-off");
    if (!fx_before) on_ui(inspect_off);
    ink_probe_count("test.finished");
}
FX_MAIN("ui-fix-g-off")
