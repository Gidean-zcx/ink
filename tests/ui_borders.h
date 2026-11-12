/* Check styles rather than anti-aliased text or image pixels. */
static void check_border_surfaces(lv_obj_t *obj) {
    if (!lv_obj_is_visible(obj)) return;
    if (lv_obj_get_style_bg_opa(obj, 0) == LV_OPA_COVER) {
        CHECK(!lv_color_eq(lv_obj_get_style_bg_color(obj, 0), lv_color_hex(0x888888)) ||
              lv_obj_get_height(obj) == ink_ui_px(INK_DIVIDER), "medium gray fill is reserved for thin separators");
        if (ink_ui_surface_kind(obj) == INK_SURFACE_STATIC) {
            CHECK(lv_color_eq(lv_obj_get_style_bg_color(obj, 0), ink_ui_paper()), "static surfaces are white");
            CHECK(lv_obj_get_style_border_width(obj, 0) <= ink_ui_px(INK_STROKE), "explicit static borders use at most the default stroke");
            CHECK(lv_color_eq(lv_obj_get_style_border_color(obj, 0), ink_ui_ink()), "static frames are black");
        }
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(obj); i++) check_border_surfaces(lv_obj_get_child(obj, (int32_t)i));
}
