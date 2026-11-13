/* Review regressions exercise the real card and settings through safe fixtures. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ink/sys/confirm.h"
#include "ink/sys/interrupt.h"
#include "ink/sys/settings.h"
#include "ink/sys/textfield.h"
#include "ink/ui_shell.h"
#define ink_main product_main
#include "../system/boot/main.c"
#undef ink_main
#include "integration/whole_product.h"
#include "keyboard_matrix.h"

static const char *mode, *detail, *wanted;
static int effects, page, pages, accept_x, accept_y, next_x, next_y, label_x, label_y;
static bool disabled, fits, pager_fits;
static char visible[160];
static lv_obj_t *accept;
static lv_area_t slider;
static bool have_slider;
static int slider_icons;
static int screen_w, screen_h, interrupt_id;
static int pager_bottom, accept_top;
static void effect(ink_call_t *c) { (void)c; effects++; }
static const ink_cap_t CAPS[] = {
    {.id = "test.long_confirm", .title = "删除测试文件", .ask = "{detail}", .effect = INK_FX_LOCAL_DESTRUCTIVE, .child_ok = true,
     .input = "{\"type\":\"object\",\"properties\":{\"detail\":{\"type\":\"string\",\"maxLength\":512}},\"required\":[\"detail\"]}", .run = effect}
};
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void quiet(void) { CHECK(WAIT_FOR(settled(), 10000), "the panel settles"); }
static int count_icons(lv_obj_t *root) {
    int count = lv_obj_check_type(root, &lv_image_class) ? 1 : 0;
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); i++) count += count_icons(lv_obj_get_child(root, (int32_t)i));
    return count;
}
static lv_obj_t *find_label(lv_obj_t *root, const char *text) {
    if (lv_obj_check_type(root, &lv_label_class) && !strcmp(lv_label_get_text(root), text)) return root;
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); i++) {
        lv_obj_t *found = find_label(lv_obj_get_child(root, (int32_t)i), text);
        if (found) return found;
    }
    return NULL;
}
static void check_segments(void) {
    const char *const labels[] = {"小", "中", "大", "特大"};
    lv_obj_t *root = ink_ui_box(lv_layer_sys(), 0, 0, INK_CANVAS_W, INK_CANVAS_H, 0, INK_SURFACE_WHITE, false);
    for (int count = 2; count <= 4; count++) for (int selected = 0; selected < count; selected += count - 1) {
        lv_obj_t *row = ink_ui_segments(root, labels, count, selected, INK_MARGIN, INK_MARGIN,
                                         INK_CANVAS_W - 2 * INK_MARGIN, NULL, NULL);
        lv_obj_update_layout(root);
        lv_area_t outer; lv_obj_get_coords(row, &outer);
        int gap = 0;
        for (int i = 0; i < count; i++) {
            lv_area_t inner; lv_obj_get_coords(lv_obj_get_child(row, i), &inner);
            CHECK(abs(inner.y1 - outer.y1 - gap) <= 1 && abs(outer.y2 - inner.y2 - gap) <= 1,
                  "%d-way segment %d fills the full 108 px segment height", count, i);
            CHECK(inner.x1 >= outer.x1 + gap - 1 && inner.x2 <= outer.x2 - gap + 1,
                  "%d-way segment %d stays within the clipped rounded group", count, i);
        }
        lv_obj_delete(row);
    }
    lv_obj_delete(root);
}
static void theme_clicked(lv_event_t *event) {
    int *clicks = lv_event_get_user_data(event);
    (*clicks)++;
}
static void check_group_row(lv_obj_t *row, bool separated) {
    int circles = 0, dividers = 0;
    lv_area_t bounds; lv_obj_get_coords(row, &bounds);
    for (uint32_t i = 0; i < lv_obj_get_child_count(row); i++) {
        lv_obj_t *o = lv_obj_get_child(row, (int32_t)i);
        lv_area_t a; lv_obj_get_coords(o, &a);
        if (lv_obj_get_child_count(o) == 1 && lv_obj_check_type(lv_obj_get_child(o, 0), &lv_image_class)) {
            circles++;
            CHECK(lv_obj_get_width(o) == ink_ui_px(INK_CIRCLE_SMALL) &&
                  lv_obj_get_height(o) == ink_ui_px(INK_CIRCLE_SMALL) &&
                  a.x1 - bounds.x1 == ink_ui_px(INK_GAP) && abs(a.y1 + a.y2 - bounds.y1 - bounds.y2) <= 1,
                  "shared list icon uses the scaled tokens and is centered in either row height");
            CHECK(!lv_obj_get_style_border_width(o, 0) && !lv_obj_has_flag(o, LV_OBJ_FLAG_CLICKABLE) && count_icons(o) == 1,
                  "shared list icon has one icon and no button outline or hit target");
            CHECK(lv_obj_get_style_bg_opa(o, 0) == LV_OPA_TRANSP && !lv_obj_get_style_radius(o, 0), "decorative icon has no circle fill");
        } else if (lv_obj_get_height(o) <= ink_ui_px(INK_DIVIDER)) {
            dividers++;
            CHECK(lv_color_eq(lv_obj_get_style_bg_color(o, 0), ink_ui_separator()), "a group divider is medium gray");
            CHECK(a.x1 - bounds.x1 == ink_ui_px(INK_CIRCLE_SMALL + INK_GAP_XL) &&
                  a.y1 == bounds.y1 && a.x2 == bounds.x2 && lv_obj_get_height(o) == ink_ui_px(INK_DIVIDER),
                  "shared group divider starts after the icon and uses the scaled stroke");
        }
    }
    CHECK(circles == 1 && dividers == (separated ? 1 : 0), "shared group has one icon circle and a divider only between rows");
}
static void check_theme(void) {
    CHECK(ink_ui_font_px(INK_FONT_CLOCK, INK_WEIGHT_REGULAR) == ink_ui_font_px(INK_FONT_CLOCK, INK_WEIGHT_MEDIUM) &&
          ink_ui_font_px(INK_FONT_NUMBER, INK_WEIGHT_REGULAR) == ink_ui_font_px(INK_FONT_NUMBER, INK_WEIGHT_MEDIUM),
          "large digits use the existing 400-weight face rather than the 300-weight face");
    CHECK(ink_ui_px(0) == 0 && ink_ui_px(0 - INK_STROKE) == 0 - ink_ui_px(INK_STROKE) &&
          ink_ui_px(0 - INK_CONTROL) == 0 - ink_ui_px(INK_CONTROL), "negative layout coordinates round symmetrically");
    CHECK(ink_ui_font_px(INK_FONT_LIST, INK_WEIGHT_MEDIUM) == ink_ui_font_px(INK_FONT_BODY, INK_WEIGHT_MEDIUM),
          "the legacy reading option and the UI body resolve to the same 38 px face");
    CHECK(INK_CONTROL == 108 && INK_PRIMARY == 108 && INK_FIELD_HEIGHT == 108 && INK_SEGMENT == 108 && INK_SLIDER == 108,
          "all shared single-row controls use the requested 108 px token");
    CHECK(INK_ROW_DOUBLE == 136 && INK_ROW_TRIPLE == 164 && INK_GROUP_TITLE == 64,
          "multi-line rows and group headings keep the requested heights");
    int64_t original = ink_setting_get("display.font_size");
    for (int level = 1; level <= 3; level += 2) {
        ink_receipt_t rc; ink_setting_set("display.font_size", level, INK_SRC_TOUCH, &rc);
        int w = INK_CANVAS_W - 2 * INK_MARGIN, clicks = 0;
        lv_obj_t *root = ink_ui_box(lv_layer_sys(), 0, 0, INK_CANVAS_W, INK_CANVAS_H, 0, INK_SURFACE_WHITE, false);
        for (int lines = 0; lines < 2; lines++) {
            int h = lines ? ink_ui_row_double() : INK_CONTROL;
            lv_obj_t *group = ink_ui_card(root, INK_MARGIN, INK_MARGIN, w, 2 * h, INK_SURFACE_STATIC);
            lv_obj_t *first = ink_ui_list_row(group, "wifi", "合成网络", "未连接", 0, 0, w, lines, NULL, NULL);
            lv_obj_t *second = ink_ui_list_row(group, "volume", "合成声音", "60%", 0, h, w, lines, theme_clicked, &clicks);
            lv_obj_update_layout(root);
            lv_obj_t *row_title = find_label(first, "合成网络"), *row_value = find_label(first, "未连接");
            lv_area_t title_rect, value_rect; lv_obj_get_coords(row_title, &title_rect); lv_obj_get_coords(row_value, &value_rect);
            CHECK(!lines || title_rect.y2 < value_rect.y1, "two-line rows place the value below the title");
            CHECK(lv_color_eq(lv_obj_get_style_text_color(row_value, 0), lines ? ink_ui_secondary() : ink_ui_ink()),
                  "single-line row text stays black; only the second line uses gray");
            CHECK(ink_ui_surface_kind(group) == INK_SURFACE_STATIC &&
                  lv_color_eq(lv_obj_get_style_bg_color(group, 0), ink_ui_paper()) &&
                  !lv_obj_get_style_border_width(group, 0), "static groups have no outer frame");
            CHECK(!lv_obj_get_style_border_width(first, 0) && !lv_obj_get_style_border_width(second, 0), "nested rows do not repeat the group frame");
            check_group_row(first, false); check_group_row(second, true);
            CHECK(!lv_obj_has_flag(first, LV_OBJ_FLAG_CLICKABLE) && lv_obj_has_flag(second, LV_OBJ_FLAG_CLICKABLE),
                  "row decoration preserves the existing row hit targets");
            lv_obj_send_event(second, LV_EVENT_CLICKED, NULL);
            lv_obj_delete(group);
        }
        lv_obj_t *single = ink_ui_list_row(root, "book", "合成单行", "", INK_MARGIN, INK_CONTROL, w, false, NULL, NULL);
        lv_obj_update_layout(root); check_group_row(single, false);
        CHECK(!lv_obj_get_style_border_width(single, 0), "a standalone list row has no frame");
        lv_obj_t *black = ink_ui_card(root, 0, 0, w, INK_CONTROL, INK_SURFACE_BLACK);
        CHECK(ink_ui_surface_kind(black) == INK_SURFACE_BLACK &&
              lv_color_eq(lv_obj_get_style_bg_color(black, 0), ink_ui_ink()) && !lv_obj_get_style_border_width(black, 0), "black surfaces retain their fill and border");
        lv_obj_t *outlined = ink_ui_box(root, 0, 0, w, INK_CONTROL, INK_RADIUS_CARD, INK_SURFACE_WHITE, true);
        lv_obj_t *text = ink_ui_line(outlined, "合成短信", INK_FONT_LIST, INK_WEIGHT_REGULAR, INK_GAP, INK_GAP, w / 2);
        lv_area_t before, after; lv_obj_update_layout(root); lv_obj_get_coords(text, &before);
        ink_ui_surface_style(outlined, INK_SURFACE_STATIC, true);
        lv_obj_update_layout(root); lv_obj_get_coords(text, &after);
        CHECK(before.x1 == after.x1 && before.y1 == after.y1, "an existing frame keeps its content position when its gray fill becomes white");
        ink_ui_surface_style(outlined, INK_SURFACE_BLACK, true);
        lv_obj_update_layout(root); lv_obj_get_coords(text, &after);
        CHECK(before.x1 == after.x1 && before.y1 == after.y1 && lv_color_eq(lv_obj_get_style_bg_color(outlined, 0), ink_ui_ink()), "reused black message cards retain their fill and content position");
        lv_obj_t *decor = ink_ui_circle(root, "info", 0, 0, INK_CIRCLE_SMALL, INK_ICON_SMALL, INK_SURFACE_WHITE, NULL, NULL);
        CHECK(!lv_obj_get_style_border_width(decor, 0) && !lv_obj_has_flag(decor, LV_OBJ_FLAG_CLICKABLE),
              "a shared decorative circle has no button outline");
        lv_obj_t *button = ink_ui_circle(root, "back", 0, 0, INK_CIRCLE, INK_ICON_LIST, INK_SURFACE_WHITE, theme_clicked, &clicks);
        CHECK(!lv_obj_get_style_border_width(button, 0) && lv_obj_has_flag(button, LV_OBJ_FLAG_CLICKABLE),
              "a directly bound icon button has no circle outline");
        lv_obj_send_event(button, LV_EVENT_CLICKED, NULL);
        ink_shell_bind(decor, "shell.back", NULL);
        lv_area_t hit; lv_obj_update_layout(decor); lv_obj_get_click_area(decor, &hit);
        CHECK(lv_area_get_width(&hit) >= ink_ui_px(INK_CONTROL) && lv_area_get_height(&hit) >= ink_ui_px(INK_CONTROL),
              "an icon reserves the minimum target even when its capability is bound later");
        CHECK(!lv_obj_get_style_border_width(decor, 0) &&
              lv_obj_has_flag(decor, LV_OBJ_FLAG_CLICKABLE) && !strcmp(ink_shell_bound_cap(decor), "shell.back"),
              "an icon bound after construction keeps its capability without a circle");
        const char *const labels[] = {"合成甲", "合成乙", "合成丙"};
        lv_obj_t *segments = ink_ui_segments(root, labels, 3, 0, 0, 0, w, NULL, NULL);
        for (int i = 0; i < 3; i++) {
            lv_obj_t *part = lv_obj_get_child(segments, i);
            ink_shell_bind(part, "shell.back", NULL);
            CHECK(!lv_obj_get_style_border_width(part, 0), "binding segmented pills does not add circle outlines");
        }
        for (int on = 0; on < 2; on++) {
            lv_obj_t *toggle = ink_ui_toggle(root, "合成开关", on, 0, 0, w, theme_clicked, &clicks);
            lv_obj_t *track = lv_obj_get_child(toggle, 1), *knob = lv_obj_get_child(track, 0);
            lv_obj_update_layout(track);
            CHECK(lv_obj_get_width(track) == ink_ui_px(96) && lv_obj_get_height(track) == ink_ui_px(56) &&
                  lv_obj_get_width(knob) == ink_ui_px(on ? 40 : 24), "switch state has the requested track and distinct knob sizes");
            CHECK(lv_obj_get_style_bg_opa(toggle, 0) == LV_OPA_TRANSP &&
                  lv_obj_get_style_bg_opa(track, 0) == LV_OPA_COVER && lv_obj_get_style_bg_opa(knob, 0) == LV_OPA_COVER &&
                  lv_color_eq(lv_obj_get_style_bg_color(track, 0), on ? ink_ui_ink() : ink_ui_paper()) &&
                  lv_color_eq(lv_obj_get_style_bg_color(knob, 0), on ? ink_ui_paper() : ink_ui_ink()),
                  "shared toggle removes only the rectangular tile and retains its black/white state");
            lv_obj_send_event(toggle, LV_EVENT_CLICKED, NULL);
        }
        CHECK(clicks == 5, "shared row, circle and both toggle states preserve their callbacks and context");
        lv_obj_t *primary = ink_ui_pill(root, "主要", 0, 0, 2 * INK_CONTROL, INK_PRIMARY, true, NULL, NULL);
        lv_obj_t *secondary = ink_ui_pill(root, "次要", 0, 0, 2 * INK_CONTROL, INK_CONTROL, false, NULL, NULL);
        lv_obj_t *plain = ink_ui_text_button(root, "文字", 0, 0, INK_CONTROL, NULL, NULL);
        CHECK(lv_obj_get_style_radius(primary, 0) == ink_ui_px(16) &&
              lv_color_eq(lv_obj_get_style_bg_color(primary, 0), ink_ui_ink()) &&
              lv_color_eq(lv_obj_get_style_text_color(primary, 0), ink_ui_paper()), "primary button has black fill and white text");
        CHECK(lv_obj_get_style_border_width(secondary, 0) == ink_ui_px(2) && !lv_obj_get_style_border_width(plain, 0) &&
              lv_obj_get_style_bg_opa(plain, 0) == LV_OPA_TRANSP, "secondary and text buttons have distinct styles");
        lv_obj_t *tile = ink_ui_switch_tile(root, "wifi", "网络", "已开启", true, true, 0, 0, w / 2 - INK_GAP_M / 2, NULL, NULL);
        CHECK(lv_color_eq(lv_obj_get_style_bg_color(tile, 0), ink_ui_ink()) &&
              lv_color_eq(lv_obj_get_style_text_color(tile, 0), ink_ui_paper()), "enabled switch tile uses black and white");
        lv_obj_t *off = ink_ui_switch_tile(root, "wifi", "网络", "未开启", false, false, 0, 0, w / 2 - INK_GAP_M / 2, NULL, NULL);
        CHECK(lv_obj_get_style_border_width(off, 0) == ink_ui_px(2) && lv_obj_has_state(off, LV_STATE_DISABLED) &&
              lv_color_eq(lv_obj_get_style_text_color(off, 0), ink_ui_secondary()), "unavailable off tile retains its outline and gray text");
        lv_obj_t *empty = ink_ui_empty(root, "没有内容", "添加", theme_clicked, &clicks);
        CHECK(lv_obj_get_child_count(empty) == 2 && !lv_obj_get_style_border_width(empty, 0), "empty state has one line and at most one action");
        lv_obj_t *dots = ink_ui_pagebar(root, 2, 5, 0, NULL);
        CHECK(lv_obj_get_child_count(dots) == 3 && lv_obj_get_child_count(lv_obj_get_child(dots, 1)) == 5,
              "short pagination has five dots and preserves the arrow child contract");
        lv_obj_t *numbers = ink_ui_pagebar(root, 2, 7, 0, NULL);
        CHECK(!strcmp(lv_label_get_text(lv_obj_get_child(numbers, 1)), "2/7"), "long pagination uses compact numbers");
        ink_text_cfg_t cfg = {.type = INK_TEXT_PLAIN, .enter = INK_ENTER_SEARCH, .initial = "合成的很长的搜索字串", .hint = "查找"};
        lv_obj_t *field = ink_textfield_create(root, &cfg, NULL, NULL, false);
        lv_obj_set_width(field, ink_ui_px(3 * INK_CONTROL));
        lv_obj_update_layout(root);
        lv_obj_t *title = lv_obj_get_child(field, 0), *area = lv_obj_get_child(field, 1), *clear = lv_obj_get_child(field, 3);
        lv_area_t field_rect, input_rect, clear_rect, title_rect;
        lv_obj_get_coords(field, &field_rect); lv_obj_get_coords(area, &input_rect);
        lv_obj_get_coords(clear, &clear_rect); lv_obj_get_coords(title, &title_rect);
        CHECK(lv_obj_is_visible(title) && title_rect.y2 < input_rect.y1, "input label stays above the shared field");
        CHECK(input_rect.x2 <= field_rect.x2 && clear_rect.x2 <= input_rect.x2 &&
              lv_area_get_width(&clear_rect) == ink_ui_px(108) && lv_area_get_height(&clear_rect) == ink_ui_px(108),
              "a narrowed search field keeps its right border and full clear target inside");
        CHECK(lv_obj_get_style_bg_opa(clear, 0) == LV_OPA_TRANSP,
              "the clear button does not paint over the input's right border");
        CHECK(lv_obj_get_style_radius(area, 0) == ink_ui_px(16) && lv_obj_get_style_border_width(area, 0) == ink_ui_px(2),
              "search uses the same rectangular input style");
        CHECK(lv_color_eq(lv_obj_get_style_text_color(area, LV_PART_TEXTAREA_PLACEHOLDER), ink_ui_ink()), "input placeholder text stays black");
        lv_obj_add_state(area, LV_STATE_FOCUSED);
        CHECK(lv_obj_get_style_border_width(area, 0) == ink_ui_px(3) && !lv_obj_get_style_outline_width(area, 0),
              "focus thickens one border without adding an outer frame");
        lv_obj_delete(root);
    }
    ink_receipt_t rc; ink_setting_set("display.font_size", original, INK_SRC_TOUCH, &rc);
}
static void check_search_layout(void) {
    lv_obj_update_layout(lv_screen_active());
    lv_obj_t *label = find_label(lv_screen_active(), "搜索");
    CHECK(label, "the real Apps search field is visible");
    if (!label) return;
    lv_obj_t *field = lv_obj_get_parent(label);
    int icons = 0;
    for (uint32_t i = 0; i < lv_obj_get_child_count(field); i++) {
        lv_obj_t *child = lv_obj_get_child(field, (int32_t)i);
        if (lv_obj_is_visible(child) && lv_obj_check_type(child, &lv_image_class)) icons++;
    }
    CHECK(icons == 1, "search includes exactly one visible search icon");
    label = lv_textarea_get_label(lv_obj_get_child(field, 1));
    lv_area_t text, bounds; lv_obj_get_coords(label, &text); lv_obj_get_coords(field, &bounds);
    for (uint32_t i = 0; i < lv_obj_get_child_count(field); i++) {
        lv_obj_t *icon = lv_obj_get_child(field, (int32_t)i);
        if (!lv_obj_check_type(icon, &lv_image_class)) continue;
        lv_area_t a; lv_obj_get_coords(icon, &a);
        CHECK(a.x1 >= bounds.x1 && a.x2 < text.x1 && a.y1 >= bounds.y1 && a.y2 <= bounds.y2,
              "the search icon fits the field without overlapping editable text");
    }
}
static void inspect(lv_obj_t *root) {
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); i++) {
        lv_obj_t *o = lv_obj_get_child(root, (int32_t)i);
        if (lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN)) continue;
        if (lv_obj_get_child_count(o) == 3) {
            lv_obj_t *indicator = lv_obj_get_child(o, 1);
            unsigned dots = lv_obj_get_child_count(indicator);
            if (lv_obj_check_type(indicator, &lv_label_class) && dots && dots <= 5 &&
                lv_obj_get_width(lv_obj_get_child(indicator, 0)) == ink_ui_px(INK_PAGE_DOT)) {
                pages = (int)dots; pager_fits = true;
                lv_area_t bounds; lv_obj_get_coords(o, &bounds); pager_bottom = bounds.y2;
                for (unsigned dot = 0; dot < dots; dot++) {
                    lv_obj_t *mark = lv_obj_get_child(indicator, (int32_t)dot);
                    lv_area_t a; lv_obj_get_coords(mark, &a);
                    pager_fits = pager_fits && a.x1 >= bounds.x1 && a.x2 <= bounds.x2 && a.y1 >= bounds.y1 && a.y2 <= bounds.y2;
                    if (lv_color_eq(lv_obj_get_style_bg_color(mark, 0), ink_ui_ink())) page = (int)dot + 1;
                }
                lv_area_t next; lv_obj_get_coords(lv_obj_get_child(o, 2), &next);
                next_x = (next.x1 + next.x2) / 2; next_y = (next.y1 + next.y2) / 2;
            }
        }
        if (lv_obj_check_type(o, &lv_label_class)) {
            const char *s = lv_label_get_text(o);
            lv_obj_t *parent = lv_obj_get_parent(o);
            lv_area_t area; lv_obj_get_coords(o, &area);
            if (!strcmp(s, "删除")) {
                accept = parent; disabled = lv_obj_has_state(parent, LV_STATE_DISABLED);
                accept_x = (area.x1 + area.x2) / 2; accept_y = (area.y1 + area.y2) / 2;
                lv_area_t bounds; lv_obj_get_coords(parent, &bounds); accept_top = bounds.y1;
            }
            int a, b;
            if (sscanf(s, "%d / %d", &a, &b) == 2) {
                page = a; pages = b;
                lv_point_t size;
                lv_text_get_size(&size, s, lv_obj_get_style_text_font(o, 0), lv_obj_get_style_text_letter_space(o, 0),
                                 lv_obj_get_style_text_line_space(o, 0), lv_obj_get_width(o), LV_TEXT_FLAG_NONE);
                pager_fits = size.y <= ink_ui_px(INK_LINE_LIST) && size.x <= lv_obj_get_width(o);
                lv_area_t bounds; lv_obj_get_coords(parent, &bounds); pager_bottom = bounds.y2;
                lv_obj_get_coords(lv_obj_get_child(parent, 2), &area);
                next_x = (area.x1 + area.x2) / 2; next_y = (area.y1 + area.y2) / 2;
            }
            if (detail && s[0] && strstr(detail, s) && !lv_obj_check_type(parent, &lv_button_class)) {
                snprintf(visible, sizeof visible, "%s", s);
                lv_point_t size;
                lv_text_get_size(&size, s, lv_obj_get_style_text_font(o, LV_PART_MAIN),
                    lv_obj_get_style_text_letter_space(o, LV_PART_MAIN), lv_obj_get_style_text_line_space(o, LV_PART_MAIN),
                    lv_obj_get_width(o), LV_TEXT_FLAG_NONE);
                fits = size.y <= lv_obj_get_height(o);
            }
            if (wanted && !strcmp(s, wanted)) {
                label_x = (area.x1 + area.x2) / 2; label_y = (area.y1 + area.y2) / 2;
                if (i > 0) {
                    lv_obj_t *icon = lv_obj_get_child(root, (int32_t)i - 1);
                    const char *cap = ink_shell_bound_cap(icon);
                    if (cap && !strcmp(cap, "shell.launch")) {
                        lv_area_t tile; lv_obj_get_coords(icon, &tile);
                        label_x = (tile.x1 + tile.x2) / 2; label_y = (tile.y1 + tile.y2) / 2;
                    }
                }
                while (parent && !lv_obj_has_flag(parent, LV_OBJ_FLAG_CLICKABLE)) parent = lv_obj_get_parent(parent);
                if (parent) { lv_obj_get_coords(parent, &slider); have_slider = true; slider_icons = count_icons(parent); }
            }
        }
        inspect(o);
    }
}
static void read_view(void) {
    lv_obj_update_layout(lv_layer_sys()); lv_obj_update_layout(lv_screen_active());
    page = pages = 1; accept = NULL; visible[0] = 0; fits = disabled = have_slider = false; label_x = label_y = -1;
    inspect(lv_screen_active()); inspect(lv_layer_sys());
    screen_w = ink_ui_width(); screen_h = ink_ui_height();
}
static void swipe(int x0, int y0, int x1, int y1) {
    hw(HW_PRESS, x0, y0); delay_ms(100); hw(HW_PRESS, x1, y1); delay_ms(100); hw(HW_RELEASE, x1, y1); quiet();
}
static uint32_t open_confirm(const char *text) {
    detail = text;
    ink_args_t *args = osal_malloc(sizeof *args, OSAL_MEM_EXTERNAL);
    ink_args_init(args); ink_args_str(args, "detail", text);
    g_done = 0;
    ink_call_opts_t opts = {.src = INK_SRC_AI, .reply_exec = INK_EXEC_DIRECT, .done = rc_done};
    CHECK(ink_cap_call("test.long_confirm", args, &opts) == 0, "submit a safe confirmation fixture");
    osal_free(args);
    CHECK(WAIT_FOR(g_done, 3000) && g_rc.status == INK_RC_PENDING_CONFIRMATION, "a real pending confirmation");
    quiet(); on_ui(read_view);
    ink_confirm_view_t view;
    CHECK(ink_confirm_list(&view, 1) == 1 && !strcmp(view.detail, text), "the service preserves the complete detail");
    return view.token;
}
static void forced_accept(void) { if (accept) lv_obj_send_event(accept, LV_EVENT_CLICKED, NULL); }
static void raise_call(void) {
    ink_interrupt_t it = {.kind = INK_INT_CALL};
    snprintf(it.title, sizeof it.title, "测试来电");
    interrupt_id = ink_interrupt_raise(&it);
}
static void read_all(const char *text) {
    char seen[512] = "";
    CHECK(page == 1 && pages > 1 && disabled, "a long transaction starts unread with acceptance disabled");
    CHECK(pager_bottom < accept_top, "the paging controls do not overlap confirmation buttons");
    on_ui(forced_accept); quiet();
    CHECK(effects == 0, "even a synthetic click cannot bypass unread pages");
    for (int i = 1; i <= pages && i < 80; i++) {
        CHECK(page == i && fits && visible[0], "page %d fits the actual label without clipping", i);
        if (strlen(seen) + strlen(visible) < sizeof seen) strcat(seen, visible);
        if (i < pages) { tap(next_x, next_y); quiet(); on_ui(read_view); }
    }
    CHECK(!strcmp(seen, text), "all objects and the irreversible consequence appear, without lost bytes");
    CHECK(WAIT_FOR(({ on_ui(read_view); !disabled; }), 1000), "the presented last page enables acceptance");
    CHECK(!disabled, "acceptance is enabled after presenting the last page");
}
static void confirmation_tests(void) {
    const char *long_text = "测试文件甲\n测试文件乙\n测试文件丙\n这三个文件都找不回来";
    uint32_t first = open_confirm(long_text);
    char page_args[96]; snprintf(page_args, sizeof page_args, "{\"token\":%u,\"delta\":1}", (unsigned)first);
    CHECK(AI("confirm.page", page_args) && g_rc.status == INK_RC_INVALID, "AI cannot mark mandatory pages read");
    on_ui(read_view); CHECK(page == 1 && disabled, "an AI page request cannot change reading progress");
    CHECK(ink_confirm_decide(first, true, INK_SRC_AI) < 0 && effects == 0, "AI cannot accept even with a real token");
    read_all(long_text);
    tap(accept_x, accept_y); quiet();
    CHECK(effects == 1 && strcmp(ink_ui_top_name(), "confirm"), "one human acceptance executes the fixture exactly once");
    effects = 0;
    uint32_t second = open_confirm(long_text);
    CHECK(second != first && disabled && page == 1, "a new token cannot inherit the prior reading state");
    CHECK(BY_HAND("confirm.page", page_args) && g_rc.status == INK_RC_STALE, "a stale token cannot turn the next transaction's pages");
    on_ui(read_view); CHECK(page == 1, "stale pagination leaves the new card on its first page");
    int w = screen_w, h = screen_h;
    swipe(w / 2, h - 4, w / 2, h * 3 / 4);
    CHECK(strcmp(ink_ui_top_name(), "confirm") && effects == 0, "bottom edge cancels without executing");
    open_confirm(long_text);
    swipe(4, h * 2 / 3, w / 2, h * 2 / 3);
    CHECK(strcmp(ink_ui_top_name(), "confirm") && effects == 0, "left edge cancels without executing");
    open_confirm("文件名称很长很长很长很长很长很长很长很长很长很长很长很长.txt\n删除后找不回来");
    CHECK(disabled && pages > 1 && fits, "a wrapped filename is paginated, not clipped");
    key(g_profile.keys_mask & (1u << HAL_KEY_BACK) ? HAL_KEY_BACK : HAL_KEY_HOME); quiet();
    CHECK(strcmp(ink_ui_top_name(), "confirm") && effects == 0, "a supported physical cancel key also cancels");
    open_confirm("另一份测试文件\n另一个对象\n内容已经更新\n删除后找不回来");
    CHECK(disabled && page == 1, "updated transaction contents must be read again");
    key(HAL_KEY_HOME); quiet(); CHECK(strcmp(ink_ui_top_name(), "confirm") && effects == 0, "HOME also cancels");
    open_confirm(long_text); on_ui(raise_call); quiet();
    swipe(w / 2, h - 4, w / 2, h * 3 / 4);
    swipe(4, h * 2 / 3, w / 2, h * 2 / 3); key(HAL_KEY_HOME);
    CHECK(!strcmp(ink_ui_top_name(), "interrupt") && effects == 0, "call gestures and HOME never fall through to confirmation");
    ink_interrupt_end(interrupt_id); quiet(); on_ui(read_view);
    CHECK(!strcmp(ink_ui_top_name(), "confirm") && page == 1 && disabled, "returning from interruption requires reading again");
    key(HAL_KEY_HOME); quiet();
    char oversized[221]; memset(oversized, 'x', sizeof oversized - 1); oversized[sizeof oversized - 1] = 0;
    ink_args_t *args = osal_malloc(sizeof *args, OSAL_MEM_EXTERNAL);
    ink_args_init(args); ink_args_str(args, "detail", oversized);
    ink_call_opts_t opts = {.src = INK_SRC_AI, .reply_exec = INK_EXEC_DIRECT, .done = rc_done};
    g_done = 0;
    CHECK(ink_cap_call("test.long_confirm", args, &opts) == 0, "submit oversized description"); osal_free(args);
    CHECK(WAIT_FOR(g_done, 3000) && g_rc.status == INK_RC_REJECTED, "an unrepresentable description is rejected before confirmation");
    ink_confirm_view_t view; CHECK(ink_confirm_list(&view, 1) == 0 && effects == 0, "no truncated transaction can be accepted");
}
static void slider_tests(void) {
    on_ui(check_segments); quiet();
    const char *ids[] = {"display.light", "display.warmth", "audio.volume"};
    const char *titles[] = {"亮度", "暖光", "音量"};
    for (int i = 0; i < 3; i++) {
        CHECK(BY_HAND(i < 2 ? "shell.display" : "shell.sound", NULL) && g_rc.status == INK_RC_DONE, "open real slider page"); quiet();
        wanted = titles[i]; on_ui(read_view);
        CHECK(have_slider, "find slider %s in the rendered tree", titles[i]);
        CHECK(slider_icons >= 2, "%s includes unframed minus and plus icons", titles[i]);
        for (int drag = 0; drag < 2; drag++) for (int value = 0; value <= 10; value++) {
            int span = slider.x2 - slider.x1, y = (slider.y1 + slider.y2) / 2;
            int x = slider.x1 + (span * value + 5) / 10;
            if (value == 0) x++;
            if (value == 10) x--;
            if (drag) {
                hw(HW_PRESS, (slider.x1 + slider.x2) / 2, y); delay_ms(80);
                hw(HW_PRESS, x, y); delay_ms(80); hw(HW_RELEASE, x, y);
            } else tap(x, y);
            quiet();
            CHECK(ink_setting_get(ids[i]) == value * 10, "%s %s level %d maps to %d, got %lld", ids[i], drag ? "drag" : "tap", value, value * 10, (long long)ink_setting_get(ids[i]));
            on_ui(read_view);
        }
        key(HAL_KEY_HOME); quiet();
    }
}
static char context[4096];
static void read_context(void) {
    const char *name;
    ink_ui_snapshot(context, sizeof context, &name);
}
static void check_child_actions(void) {
    char *ids = osal_malloc(8192, OSAL_MEM_EXTERNAL);
    CHECK(ids && ink_cap_ids_json(ids, 8192) > 0, "read the current child's callable ids");
    if (ids) {
        CHECK(!strstr(ids, "\"shell.settings\"") && !strstr(ids, "\"member.set_pin\""), "the compact action list excludes disallowed and owner-only actions");
        osal_free(ids);
    }
}
static void add_test_networks(void *unused) {
    (void)unused;
    sim_wifi_ap_add("FAKE_UI_TEST_AP_1", -45, "SYNTH_PASSWORD");
    sim_wifi_ap_add("FAKE_UI_TEST_AP_2", -60, "SYNTH_PASSWORD");
}
static void context_tests(void) {
    vtk_event_at(vtk_now() + SIM_MS(1), add_test_networks, NULL); delay_ms(5);
    CHECK(AI("shell.control", NULL) && g_rc.status == INK_RC_DONE, "open control through the bus"); quiet(); on_ui(read_context);
    CHECK(strstr(context, "叫名字=") && strstr(context, "更多=0"), "control describes its current wake and expansion states");
    CHECK(AI("shell.more", NULL) && g_rc.status == INK_RC_DONE, "expand through the same capability"); quiet(); on_ui(read_context);
    CHECK(strstr(context, "更多=1") && strstr(context, "暖光="), "expanded control includes visible warmth");
    CHECK(AI("shell.network", NULL) && g_rc.status == INK_RC_DONE, "open real network settings"); quiet();
    wanted = "找附近的 Wi-Fi"; on_ui(read_view);
    CHECK(label_x >= 0, "the scan control exists"); tap(label_x, label_y);
    bool found = false;
    for (int i = 0; i < 500 && !found; i++) { on_ui(read_context); found = strstr(context, "网络[0]=") != NULL; if (!found) delay_ms(20); }
    CHECK(found && strstr(context, "网络组=1") && strstr(context, "SIM="), "visible networks have stable selection indices and connection context");
    CHECK(AI("shell.wifi_pick", "{\"index\":0}") && g_rc.status == INK_RC_DONE, "the listed index selects the real network"); quiet();
    CHECK(ink_text_focused() && ink_text_type() == INK_TEXT_PASSWORD, "selection opens a human password field");
    CHECK(AI("text.insert", "{\"text\":\"FORBIDDEN\"}") && g_rc.status != INK_RC_DONE, "AI cannot type the network password");
    key(HAL_KEY_HOME); quiet();
    CHECK(AI("settings.set", "{\"id\":\"voice.handsfree\",\"value\":2}") && g_rc.status == INK_RC_DONE, "change the actual wake setting");
    CHECK(AI("settings.set", "{\"id\":\"display.lock_style\",\"value\":1}") && g_rc.status == INK_RC_DONE, "change the actual lock style");
    CHECK(AI("settings.set", "{\"id\":\"power.lock_after_s\",\"value\":600}") && g_rc.status == INK_RC_DONE, "set the actual automatic lock time");
    CHECK(AI("shell.settings", NULL) && g_rc.status == INK_RC_DONE, "return to the settings overview"); quiet();
    CHECK(AI("shell.page_next", NULL) && g_rc.status == INK_RC_DONE, "read the next settings page at the larger row size"); quiet();
    wanted = "10 分钟"; on_ui(read_view); CHECK(label_x >= 0, "the overview reflects the automatic lock time");
    CHECK(AI("settings.open", "{\"id\":\"system\"}") && g_rc.status == INK_RC_DONE, "D-137: the AI group is on the home's 系统 page"); quiet();
    wanted = "叫名字已关闭"; on_ui(read_view); CHECK(label_x >= 0, "the overview reflects the wake setting");
    CHECK(AI("shell.settings", NULL) && g_rc.status == INK_RC_DONE, "return to the first settings page"); quiet();
    /* A UI-only connection-state fixture; no pairing or real radio is used. */
    ink_state_set_bool(ink_state_find("bt.connected"), true); quiet();
    wanted = "已连接"; on_ui(read_view); CHECK(label_x >= 0, "a connection-state event updates the visible Bluetooth status");
    ink_state_set_bool(ink_state_find("bt.connected"), false); quiet();
    CHECK(ink_member_switch("kid") == 0, "switch to the real child space"); quiet();
    CHECK(AI("shell.control", NULL) && g_rc.status == INK_RC_DONE, "child opens their own controls"); quiet(); on_ui(read_context);
    CHECK(!strstr(context, "shell.settings") && !strstr(context, "shell.members"), "the child snapshot does not advertise adult controls");
    on_ui(check_child_actions);
    CHECK(AI("shell.settings", NULL) && g_rc.status == INK_RC_REJECTED, "a globally registered setting entry is actually denied to the child");
    CHECK(!strcmp(ink_ui_top_name(), "KidControl"), "the denied operation stays in the child control center");
}
static ink_exec_job_t blocked_job, drained_job;
static volatile bool bg_started, bg_release, bg_drained;
static void block_background(void *arg) {
    (void)arg;
    __atomic_store_n(&bg_started, true, __ATOMIC_RELEASE);
    while (!__atomic_load_n(&bg_release, __ATOMIC_ACQUIRE)) osal_task_delay_us(1000);
}
static void background_drained(void *arg) { (void)arg; __atomic_store_n(&bg_drained, true, __ATOMIC_RELEASE); }
static void member_privacy_tests(void) {
    CHECK(BY_HAND("shell.search", "{\"query\":\"PRIVATE_OWNER_TEST_QUERY\"}") && g_rc.status == INK_RC_DONE, "the adult searches through the real capability"); quiet();
    CHECK(BY_HAND("shell.display", NULL) && g_rc.status == INK_RC_DONE, "leave all apps in the same member's session"); quiet();
    CHECK(BY_HAND("shell.back", NULL) && g_rc.status == INK_RC_DONE, "come back to all apps"); quiet(); on_ui(read_context);
    CHECK(strstr(context, "PRIVATE_OWNER_TEST_QUERY"), "ordinary navigation preserves the current member's query");
    CHECK(BY_HAND("shell.network", NULL) && g_rc.status == INK_RC_DONE, "open real network settings"); quiet();
    CHECK(ink_exec_post(INK_EXEC_BG, &blocked_job, block_background, NULL) == 0 && WAIT_FOR(__atomic_load_n(&bg_started, __ATOMIC_ACQUIRE), 2000), "hold the background executor before the scan starts");
    wanted = "找附近的 Wi-Fi"; on_ui(read_view);
    CHECK(label_x >= 0, "find the real scan button"); if (label_x >= 0) tap(label_x, label_y); quiet();
    CHECK(BY_HAND("shell.members", NULL) && g_rc.status == INK_RC_DONE, "open the actual member picker"); quiet();
    CHECK(BY_HAND("shell.member_choose", NULL) && g_rc.status == INK_RC_DONE, "continue from the member handoff preview"); quiet();
    wanted = "小"; on_ui(read_view);
    CHECK(label_x >= 0, "find the child's avatar"); if (label_x >= 0) tap(label_x, label_y); quiet();
    CHECK(!strcmp(ink_member_current()->id, "kid"), "touch really switches to the child");
    on_ui(read_context);
    CHECK(!strstr(context, "PRIVATE_OWNER_TEST_QUERY"), "the child snapshot has no adult query");
    CHECK(!strcmp(ink_shell_board(), "KidHome"), "a different member starts on their child home page");
    wanted = "PRIVATE_OWNER_TEST_QUERY"; on_ui(read_view);
    CHECK(label_x < 0, "the child screen has no adult query");
    __atomic_store_n(&bg_release, true, __ATOMIC_RELEASE);
    CHECK(ink_exec_post(INK_EXEC_BG, &drained_job, background_drained, NULL) == 0 && WAIT_FOR(__atomic_load_n(&bg_drained, __ATOMIC_ACQUIRE), 5000), "the old member's queued scan has returned");
    quiet(); on_ui(read_context);
    CHECK(!strstr(context, "没能完成，可以再试一次") && !strstr(context, "已经找过附近的网络"), "a late scan receipt cannot change the new member's result");
    CHECK(BY_HAND("shell.apps", NULL) && g_rc.status == INK_RC_DONE, "inspect the child's own apps page"); quiet(); on_ui(read_context);
    CHECK(strstr(context, "查找=；") && !strstr(context, "PRIVATE_OWNER_TEST_QUERY"), "the child's search field is empty on Apps too");
    wanted = "PRIVATE_OWNER_TEST_QUERY"; on_ui(read_view); CHECK(label_x < 0, "Apps does not reveal the old query");
    CHECK(ink_member_switch("owner") == 0, "return to the adult without reusing the old session"); quiet();
    CHECK(BY_HAND("shell.network", NULL) && g_rc.status == INK_RC_DONE, "reopen networks after switching back"); quiet(); on_ui(read_context);
    CHECK(!strstr(context, "网络[0]=") && strstr(context, "选中网络=；"), "network results and selection do not cross sessions");
}
static lv_obj_t *answer_root, *answer_status, *answer_question, *answer_label;
static const char *answer_words;
static char answer_text[4096] INK_PSRAM_BSS, answer_seen[4096] INK_PSRAM_BSS;
static bool answer_fits, answer_next_disabled, answer_prev_disabled;
static lv_obj_t *bound(lv_obj_t *root, const char *id) {
    const char *cap = ink_shell_bound_cap(root);
    if (cap && !strcmp(cap, id)) return root;
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); i++) {
        lv_obj_t *found = bound(lv_obj_get_child(root, (int32_t)i), id);
        if (found) return found;
    }
    return NULL;
}
static void open_answer(void) { ink_shell_ai_open(&answer_root, &answer_status, &answer_question, &answer_label); }
static void update_answer(void) { ink_shell_ai_words(answer_words); ink_shell_ai_status("回答完成", INK_AI_ERROR_NONE); }
static void close_answer(void) { ink_shell_ai_closed(); lv_obj_delete(answer_root); answer_root = NULL; }
static void read_answer(void) {
    read_view();
    const char *text = lv_label_get_text(answer_label);
    snprintf(answer_text, sizeof answer_text, "%s", text);
    lv_point_t size;
    lv_text_get_size(&size, text, lv_obj_get_style_text_font(answer_label, 0), lv_obj_get_style_text_letter_space(answer_label, 0),
                     lv_obj_get_style_text_line_space(answer_label, 0), lv_obj_get_width(answer_label), LV_TEXT_FLAG_NONE);
    answer_fits = size.y <= lv_obj_get_height(answer_label);
    lv_obj_t *next = bound(answer_root, "ai.panel.next"), *prev = bound(answer_root, "ai.panel.prev");
    answer_next_disabled = !next || lv_obj_has_state(next, LV_STATE_DISABLED);
    answer_prev_disabled = !prev || lv_obj_has_state(prev, LV_STATE_DISABLED);
    if (next) { lv_area_t a; lv_obj_get_coords(next, &a); next_x = (a.x1 + a.x2) / 2; next_y = (a.y1 + a.y2) / 2; }
    if (prev) { lv_area_t a; lv_obj_get_coords(prev, &a); label_x = (a.x1 + a.x2) / 2; label_y = (a.y1 + a.y2) / 2; }
}
static void check_long_layout(void) {
    lv_obj_update_layout(answer_root);
    int height = ink_ui_height() - lv_obj_get_y(answer_root);
    int expected = 2 * INK_GAP_L + 2 * INK_CONTROL + 3 * INK_GAP_S +
        5 * ink_ui_line_height(INK_FONT_BODY) + ink_ui_line_height(INK_FONT_NOTE) + INK_GAP_M;
    CHECK(height == ink_ui_px(expected), "a long answer uses the exact shared spacing and five body lines");
    CHECK(lv_obj_get_height(answer_label) >= ink_ui_px(5 * ink_ui_line_height(INK_FONT_BODY)), "the expanded answer has five body lines");
    lv_obj_t *source = find_label(answer_root, "出自这一页");
    lv_obj_t *next = bound(answer_root, "ai.panel.next");
    CHECK(source && next && lv_obj_get_y(answer_label) + lv_obj_get_height(answer_label) <= lv_obj_get_y(source) &&
          lv_obj_get_y(source) + lv_obj_get_height(source) <= lv_obj_get_y(lv_obj_get_parent(next)) &&
          lv_obj_get_y(lv_obj_get_parent(next)) + lv_obj_get_height(lv_obj_get_parent(next)) <= height,
          "expanded text, source and paging controls fit without overlap");
}
static void check_short_layout(void) {
    lv_obj_update_layout(answer_root);
    int height = ink_ui_height() - lv_obj_get_y(answer_root);
    CHECK(height * 100 >= ink_ui_height() * 42 && height * 100 <= ink_ui_height() * 44 &&
          lv_obj_get_height(answer_label) == ink_ui_px(3 * ink_ui_line_height(INK_FONT_BODY)), "a short answer returns to the compact panel");
}
static void traverse_answer(const char *text) {
    answer_words = text; on_ui(open_answer); on_ui(update_answer); quiet(); on_ui(read_answer);
    on_ui(check_long_layout);
    CHECK(page == 1 && pages > 1 && answer_prev_disabled, "a long answer has a visible page count and starts on its first page");
    answer_seen[0] = 0;
    int total = pages;
    for (int i = 1; i <= total && i < 200; i++) {
        CHECK(page == i && answer_fits && answer_text[0], "answer page %d fits the actual font and label", i);
        size_t used = strlen(answer_seen), size = strlen(answer_text);
        CHECK(used + size < sizeof answer_seen, "the traversed fixture fits its complete-text buffer");
        if (used + size < sizeof answer_seen) memcpy(answer_seen + used, answer_text, size + 1);
        on_ui(read_context);
        static char escaped[8192]; ink_jw_t writer; ink_jw_init(&writer, escaped, sizeof escaped); ink_jw_str(&writer, answer_text);
        CHECK(!writer.overflow && strstr(context, escaped) && strstr(context, "页码="), "AI sees the displayed page and page count in F1 JSON");
        if (i < total) {
            CHECK(!answer_next_disabled, "a further page has an enabled next button");
            if (i % 2) tap(next_x, next_y);
            else CHECK(AI("ai.panel.next", NULL) && g_rc.status == INK_RC_DONE, "AI uses the same next-page capability");
            quiet(); on_ui(read_answer);
        }
    }
    CHECK(!strcmp(answer_seen, text), "complete traversal preserves every byte, without missing or repeated text");
    CHECK(answer_next_disabled, "the last page disables forward navigation");
    CHECK(BY_HAND("ai.panel.next", NULL) && g_rc.status == INK_RC_DONE, "an extra next-page request is bounded");
    quiet(); on_ui(read_answer); CHECK(page == total, "forward navigation stays on the last page");
    if (total > 1) { tap(label_x, label_y); quiet(); on_ui(read_answer); CHECK(page == total - 1, "the visible previous button goes back"); }
    for (int i = total - 1; i > 1; i--) {
        CHECK(AI("ai.panel.prev", NULL) && g_rc.status == INK_RC_DONE, "AI can return through previous pages");
        quiet(); on_ui(read_answer); CHECK(page == i - 1 && answer_fits, "previous page is reachable without clipping");
    }
    on_ui(close_answer); quiet();
}
static void answer_tests(void) {
    traverse_answer("第一行\n第二行\n第三行\n第四行\n第五行\n第六行\n第七行\n第八行");
    static char long_text[1800] INK_PSRAM_BSS;
    for (int i = 0; i < 30; i++) strcat(long_text, "中文答案 mixed words 123，阅读这一页。 ");
    traverse_answer(long_text);
    answer_words = "第一行\n第二行\n第三行\n第四行\n第五行\n第六行\n第七行\n第八行"; on_ui(open_answer); on_ui(update_answer); quiet();
    CHECK(AI("ai.panel.next", NULL) && g_rc.status == INK_RC_DONE, "advance before replacing streamed content");
    answer_words = "回答"; on_ui(update_answer); quiet(); on_ui(read_answer);
    on_ui(check_short_layout);
    CHECK(page == 1 && pages == 1 && !strcmp(answer_text, "回答") && answer_next_disabled && answer_prev_disabled, "shorter streamed content clamps the page and updates both buttons");
    static char many_lines[4096] INK_PSRAM_BSS;
    memset(many_lines, '\n', sizeof many_lines - 1);
    answer_words = many_lines; on_ui(update_answer); quiet(); on_ui(read_answer);
    CHECK(pages >= 1000 && pager_fits, "four-digit page counts fit the visible pager on one line");
    on_ui(close_answer); quiet();
    CHECK(AI("ai.panel.next", NULL) && g_rc.status == INK_RC_STALE, "a closed answer cannot be paginated");
}
static void tool_page(lv_obj_t *screen, void *arg) { (void)screen; (void)arg; }
static const ink_activity_t TOOL_PAGE_A = {.name = "fake-tool-a", .app = "tool_a", .create = tool_page};
static const ink_activity_t TOOL_PAGE_B = {.name = "fake-tool-b", .app = "tool_b", .create = tool_page};
static const ink_app_def_t TOOL_A = {.id = "tool_a", .title = "工具甲", .entry = &TOOL_PAGE_A, .child_ok = true};
static const ink_app_def_t TOOL_B = {.id = "tool_b", .title = "工具乙", .entry = &TOOL_PAGE_B, .child_ok = true};
static const ink_text_client_t TEXT_CLIENT = {0};
static ink_text_type_t keyboard_type;
static char first_candidate[40], picked_text[128];
static lv_area_t keyboard_bounds;
static lv_obj_t *keyboard_panel(void) {
    lv_obj_t *root = ink_test_matrix(lv_layer_sys());
    for (int i = 0; i < 2 && root; i++) root = lv_obj_get_parent(root);
    CHECK(root && !strcmp(ink_ui_top_name(), "Keyboard"), "the real keyboard panel exists");
    return root;
}
static void remember_keyboard(void) {
    lv_obj_update_layout(lv_layer_sys());
    lv_obj_t *root = keyboard_panel();
    if (root) lv_obj_get_coords(root, &keyboard_bounds);
}
static void check_keyboard_bounds(void) {
    lv_obj_update_layout(lv_layer_sys());
    lv_obj_t *root = keyboard_panel();
    if (!root) return;
    lv_area_t current; lv_obj_get_coords(root, &current);
    if (keyboard_type == INK_TEXT_PASSWORD)
        CHECK(current.x1 == keyboard_bounds.x1 && current.x2 == keyboard_bounds.x2 &&
              current.y1 > keyboard_bounds.y1 && current.y2 == keyboard_bounds.y2,
              "the password keyboard drops the candidate band: same width and bottom, shorter");
    else
        CHECK(current.x1 == keyboard_bounds.x1 && current.x2 == keyboard_bounds.x2 &&
              current.y1 == keyboard_bounds.y1 && current.y2 == keyboard_bounds.y2,
              "candidate and edit states preserve the same keyboard area");
}
static void focus_keyboard(void) {
    ink_text_cfg_t cfg = {.type = keyboard_type, .initial = keyboard_type == INK_TEXT_PASSWORD ? "FAKE_UI_SECRET" : "测试文字", .max = 128};
    CHECK(ink_text_focus(&cfg, &TEXT_CLIENT, NULL) > 0, "focus the real text service");
}
static void blur_keyboard(void) { ink_text_blur(0); }
static void read_candidate(void) {
    ink_text_cand_t candidates[INK_TEXT_PAGE];
    int n = ink_text_candidates(0, candidates, INK_TEXT_PAGE);
    CHECK(n > 0, "the synthetic pinyin input has real candidates");
    snprintf(first_candidate, sizeof first_candidate, "%s", n ? candidates[0].word : "");
}
static void compose_xian(void) {
    for (const char *p = "xian"; *p; p++) {
        char args[32]; snprintf(args, sizeof args, "{\"code\":%d}", (int)*p);
        CHECK(AI("keyboard.key", args) && g_rc.status == INK_RC_DONE, "type through the keyboard capability");
    }
    quiet();
}
static void snapshot_tests(void) {
    on_ui(ink_ui_home); quiet(); /* D-135: today's cards are on the one page, and only those with content */
    wanted = "继续听"; on_ui(read_view); CHECK(label_x < 0, "the home draws no listening card without content");
    wanted = "今日报纸"; on_ui(read_view); CHECK(label_x < 0, "the home draws no newspaper card without content");
    CHECK(BY_HAND("shell.search", "{\"query\":\"\"}") && g_rc.status == INK_RC_DONE, "open Apps with a two-page tool fixture"); quiet(); on_ui(read_context);
    on_ui(check_search_layout);
    const char *names[] = {"时钟", "短信", "设置", "对话记录", "工具甲", "工具乙"};
    const char *entries[] = {"时钟（clock）", "短信（message）", "设置（settings）", "对话记录（ai）", "工具甲（tool_a）", "工具乙（tool_b）"};
    unsigned seen = 0; bool tool_touched = false;
    for (int turn = 0; turn < 4; turn++) {
        on_ui(read_context);
        int current = 0, total = 0; const char *at = strstr(context, "工具页=");
        CHECK(at && sscanf(at, "工具页=%d/%d", &current, &total) == 2 && current == turn + 1 && total <= 4,
              "Apps exposes the actual page after the input label reflows it");
        for (unsigned i = 0; i < sizeof names / sizeof *names; i++) {
            wanted = names[i]; on_ui(read_view);
            bool advertised = strstr(context, entries[i]) != NULL;
            CHECK(advertised == (label_x >= 0), "only visible entries occur in the page snapshot: %s", names[i]);
            if (advertised) seen |= 1u << i;
        }
        wanted = "工具甲"; on_ui(read_view);
        if (label_x >= 0 && !tool_touched) {
            tap(label_x, label_y); quiet();
            CHECK(!strcmp(ink_ui_page_name(), "fake-tool-a"), "touch launches the advertised stable id");
            tool_touched = true; CHECK(BY_HAND("shell.back", NULL), "back to the same page of all apps"); quiet();
        }
        if (current >= total) break;
        CHECK(AI("shell.apps_next", NULL) && g_rc.status == INK_RC_DONE, "turn to the next actual tool page"); quiet();
    }
    CHECK(seen == 63 && tool_touched, "all registered, built-in and fixed-group entries remain visible and callable across pages");
    CHECK(AI("shell.launch", "{\"id\":\"settings\"}") && g_rc.status == INK_RC_DONE, "AI can use the visible settings id"); quiet(); CHECK(!strcmp(ink_ui_page_name(), "Settings"), "the advertised id opens actual settings");
    on_ui(ink_ui_home); quiet();
    CHECK(AI("shell.search", "{\"query\":\"工具甲\"}") && g_rc.status == INK_RC_DONE, "filter Apps through its capability"); quiet(); on_ui(read_context);
    CHECK(strstr(context, "工具页=1/1") && strstr(context, "工具甲（tool_a）") && !strstr(context, "工具乙（tool_b）") && !strstr(context, "短信（message）"), "search lays out only the matches, from the first page, in the snapshot too");
    wanted = "工具甲"; on_ui(read_view); CHECK(label_x >= 0, "the match is drawn on the first page");
    CHECK(AI("shell.launch", "{\"id\":\"tool_a\"}") && g_rc.status == INK_RC_DONE, "AI launches the same tool as touch"); quiet(); CHECK(!strcmp(ink_ui_page_name(), "fake-tool-a"), "touch and AI reach the same real activity");
    on_ui(ink_ui_home); quiet();
    const char *titles[] = {"测试通知甲", "测试通知乙", "测试通知丙"}; uint32_t ids[3] = {0};
    for (int i = 0; i < 3; i++) {
        ink_notify_t n = {.title = titles[i], .body = "测试通知内容", .prio = INK_NOTIFY_LOW, .src = INK_SRC_TOUCH};
        CHECK(ink_notify_post(&n, &ids[i]) == 0 && ids[i], "post a synthetic notification to the real service");
    }
    CHECK(AI("shell.control", NULL) && g_rc.status == INK_RC_DONE, "open the populated control center"); quiet(); on_ui(read_context);
    for (int i = 0; i < 3; i++) { /* control-1: the first stage has room for two rows; the newest is always one of them */
        wanted = titles[i]; on_ui(read_view);
        CHECK((label_x >= 0) == (strstr(context, titles[i]) != NULL) && (i != 2 || label_x >= 0), "control notification %d agrees between screen and snapshot", i);
    }
    char args[48]; snprintf(args, sizeof args, "{\"id\":%u}", (unsigned)ids[2]);
    CHECK(AI("notify.read", args) && g_rc.status == INK_RC_DONE, "AI marks the displayed notification read"); quiet(); on_ui(read_context);
    char token[64]; snprintf(token, sizeof token, "通知[%u]=", (unsigned)ids[2]);
    CHECK(strstr(context, token) && strstr(context, "已读=1"), "control preserves the stable notification id and its read state");
    CHECK(AI("shell.notifications", NULL) && g_rc.status == INK_RC_DONE, "open the full notification list"); quiet(); on_ui(read_context);
    for (int i = 0; i < 3; i++) CHECK(strstr(context, titles[i]), "the full visible list exposes notification %d", i);
    on_ui(ink_ui_home); quiet(); keyboard_type = INK_TEXT_PLAIN; on_ui(focus_keyboard); quiet(); on_ui(remember_keyboard);
    compose_xian(); on_ui(check_keyboard_bounds);
    CHECK(AI("keyboard.candidates", "{}") && g_rc.status == INK_RC_DONE, "expand the real candidates through the bus"); quiet(); on_ui(check_keyboard_bounds);
    CHECK(AI("keyboard.candidates", "{\"fold\":true}") && g_rc.status == INK_RC_DONE, "fold candidates through the same capability"); quiet(); on_ui(check_keyboard_bounds);
    on_ui(read_candidate); on_ui(read_context);
    snprintf(token, sizeof token, "候选[0]=%s", first_candidate);
    CHECK(first_candidate[0] && strstr(context, token) && strstr(context, "光标=") && strstr(context, "选区="), "keyboard describes its actual candidate id, cursor and selection");
    wanted = first_candidate; on_ui(read_view); CHECK(label_x >= 0, "the described candidate is visible"); if (label_x >= 0) tap(label_x, label_y); quiet();
    snprintf(picked_text, sizeof picked_text, "%s", ink_text_value());
    on_ui(focus_keyboard); compose_xian();
    CHECK(AI("keyboard.pick", "{\"index\":0}") && g_rc.status == INK_RC_DONE, "AI selects the candidate id from the snapshot"); quiet();
    CHECK(!strcmp(picked_text, ink_text_value()), "touch and AI commit the same candidate");
    CHECK(AI("text.select", "{\"start\":1,\"end\":3}") && g_rc.status == INK_RC_DONE, "select real text through the bus"); quiet(); on_ui(read_context);
    on_ui(check_keyboard_bounds);
    CHECK(strstr(context, "选区=1..3") && !strstr(context, "候选["), "the editing snapshot reports selection and omits hidden candidates");
    keyboard_type = INK_TEXT_PASSWORD; on_ui(focus_keyboard); quiet(); on_ui(read_context);
    on_ui(check_keyboard_bounds);
    CHECK(strstr(context, "******") && !strstr(context, "FAKE_UI_SECRET") && !strstr(context, "候选[") && !strstr(context, "选区="), "password snapshots remain masked without candidates or selection");
    on_ui(blur_keyboard); quiet();
}
static bool alarm_title_font, alarm_time_font;
static void read_alarm_fields(void) {
    lv_obj_t *root = lv_layer_sys();
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); i++) {
        lv_obj_t *panel = lv_obj_get_child(root, (int32_t)i);
        for (uint32_t j = 0; j < lv_obj_get_child_count(panel); j++) {
            lv_obj_t *body = lv_obj_get_child(panel, (int32_t)j);
            for (uint32_t k = 0; k < lv_obj_get_child_count(body); k++) {
                lv_obj_t *o = lv_obj_get_child(body, (int32_t)k);
                if (!lv_obj_check_type(o, &lv_label_class)) continue;
                const char *s = lv_label_get_text(o);
                if (!strcmp(s, "起床")) alarm_title_font = lv_obj_get_style_text_font(o, 0) == ink_ui_font_px(INK_FONT_TITLE, INK_WEIGHT_BOLD);
                if (!strcmp(s, "07:30")) alarm_time_font = lv_obj_get_style_text_font(o, 0) == ink_ui_font_px(INK_FONT_NUMBER, INK_WEIGHT_REGULAR);
            }
        }
    }
}
static void alarm_tests(void) {
    ink_tm_t tm = {.year = 2026, .month = 11, .day = 5, .hour = 7, .min = 29, .sec = 58};
    ink_time_set(ink_time_from_local(&tm), false);
    ink_alarm_t a = {.kind = INK_ALARM, .at_utc = ink_time_now_s() + 2, .src = INK_SRC_TOUCH};
    snprintf(a.title, sizeof a.title, "起床");
    CHECK(ink_alarm_add(&a) > 0 && WAIT_FOR(!strcmp(active_state(), "alarm"), 5000), "a real synthetic alarm rings at 07:30"); quiet(); on_ui(read_context); on_ui(read_alarm_fields);
    CHECK(strstr(context, "起床") && strstr(context, "时间=07:30") && strstr(context, "日期=") && strstr(context, "11月5日"), "alarm snapshot separates its title, actual time and date");
    CHECK(alarm_title_font && alarm_time_font, "Chinese alarm title uses title type while time uses the numeric font");
    wanted = "再睡 10 分钟"; on_ui(read_view); CHECK(label_x >= 0, "the snooze button states its actual duration"); if (label_x >= 0) tap(label_x, label_y); quiet();
    CHECK(!strcmp(active_state(), "none"), "touch snoozes the real alarm");
    int cursor = 0; ink_alarm_t saved;
    CHECK(ink_alarm_next(&cursor, INK_ALARM, &saved) == 1 && saved.id == a.id && saved.snooze_until - ink_time_now_s() >= 598 && saved.snooze_until - ink_time_now_s() <= 600, "the same service schedules ten minutes, not its nine-minute default");
    CHECK(ink_alarm_cancel(a.id) == 0, "remove the snoozed fixture");
    a.id = 0; a.at_utc = ink_time_now_s() + 2;
    CHECK(ink_alarm_add(&a) > 0 && WAIT_FOR(!strcmp(active_state(), "alarm"), 5000), "ring another real alarm to check dismiss"); quiet();
    wanted = "这次不再响"; on_ui(read_view); CHECK(label_x >= 0, "dismiss states that it affects this occurrence"); if (label_x >= 0) tap(label_x, label_y); quiet();
    CHECK(!strcmp(active_state(), "none"), "touch dismisses the actual alarm");
}
static void test_task(void *unused) {
    (void)unused; delay_ms(1800); quiet();
    if (!strcmp(mode, "slider")) slider_tests();
    else if (!strcmp(mode, "context")) context_tests();
    else if (!strcmp(mode, "privacy")) member_privacy_tests();
    else if (!strcmp(mode, "answer")) answer_tests();
    else if (!strcmp(mode, "snapshot")) snapshot_tests();
    else if (!strcmp(mode, "alarm")) alarm_tests();
    else if (!strcmp(mode, "theme")) on_ui(check_theme);
    else confirmation_tests();
    ink_probe_count("test.finished");
}
void ink_main(void) {
    mode = getenv("INK_UI_REGRESSION"); if (!mode) mode = "confirm";
    product_main(); test_start_home(); ink_cap_register(CAPS, sizeof CAPS / sizeof *CAPS);
    if (!strcmp(mode, "snapshot")) { ink_app_register(&TOOL_A); ink_app_register(&TOOL_B); }
    osal_task_cfg_t cfg = {.name = "ui-review", .fn = test_task, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    if (osal_task_create(&cfg, NULL) != OSAL_OK) osal_panic("UI regression task");
}
