/* Reuse the existing synthetic shell fixtures; exercise real app capabilities. */
#define INK_DESIGN_LANG
#include "ui_shell_scenes.c"
#include "pagebar.h"

static int requested_size;
static char *tree_json;
static ink_jw_t tree_writer;
static bool first_node;
static void read_environment(void *arg) {
    (void)arg;
    const char *name = getenv("INK_UI_SCENE"), *size = getenv("INK_UI_SCENE_TEXT");
    sim_copy_name(scene, sizeof scene, name ? name : "Main");
    requested_size = size ? atoi(size) : 1;
}
static const char *node_text(lv_obj_t *obj) {
    if (lv_obj_check_type(obj, &lv_label_class)) return lv_label_get_text(obj);
    for (uint32_t i = 0; i < lv_obj_get_child_count(obj); i++) {
        const char *text = node_text(lv_obj_get_child(obj, (int32_t)i));
        if (text[0]) return text;
    }
    return "";
}
static void tree_target(const char *text, const char *cap, lv_area_t hit, unsigned depth) {
    if (!first_node) ink_jw_raw(&tree_writer, ",");
    first_node = false;
    ink_jw_raw(&tree_writer, "{\"text\":"); ink_jw_str(&tree_writer, text);
    ink_jw_raw(&tree_writer, ",\"cap\":"); ink_jw_str(&tree_writer, cap ? cap : "");
    ink_jw_raw(&tree_writer, ",\"depth\":"); ink_jw_int(&tree_writer, depth);
    ink_jw_raw(&tree_writer, ",\"x\":"); ink_jw_int(&tree_writer, hit.x1);
    ink_jw_raw(&tree_writer, ",\"y\":"); ink_jw_int(&tree_writer, hit.y1);
    ink_jw_raw(&tree_writer, ",\"width\":"); ink_jw_int(&tree_writer, LV_MAX(0, lv_area_get_width(&hit)));
    ink_jw_raw(&tree_writer, ",\"height\":"); ink_jw_int(&tree_writer, LV_MAX(0, lv_area_get_height(&hit)));
    ink_jw_raw(&tree_writer, "}");
}
static void tree_node(lv_obj_t *obj, lv_area_t clip, unsigned depth) {
    if (!lv_obj_is_visible(obj)) return;
    lv_area_t own, hit;
    lv_obj_get_coords(obj, &own);
    lv_obj_get_click_area(obj, &hit);
    hit.x1 = LV_MAX(hit.x1, clip.x1); hit.y1 = LV_MAX(hit.y1, clip.y1);
    hit.x2 = LV_MIN(hit.x2, clip.x2); hit.y2 = LV_MIN(hit.y2, clip.y2);
    bool matrix = lv_obj_check_type(obj, &lv_keyboard_class) || lv_obj_check_type(obj, &lv_buttonmatrix_class);
    if (matrix && !lv_obj_has_state(obj, LV_STATE_DISABLED)) {
        lv_buttonmatrix_t *keys = (lv_buttonmatrix_t *)obj;
        for (uint32_t i = 0; i < keys->btn_cnt; i++) {
            if (lv_buttonmatrix_has_button_ctrl(obj, i, LV_BUTTONMATRIX_CTRL_HIDDEN) ||
                lv_buttonmatrix_has_button_ctrl(obj, i, LV_BUTTONMATRIX_CTRL_DISABLED)) continue;
            lv_area_t cell = keys->button_areas[i];
            cell.x1 += own.x1; cell.x2 += own.x1; cell.y1 += own.y1; cell.y2 += own.y1;
            cell.x1 = LV_MAX(cell.x1, clip.x1); cell.x2 = LV_MIN(cell.x2, clip.x2);
            cell.y1 = LV_MAX(cell.y1, clip.y1); cell.y2 = LV_MIN(cell.y2, clip.y2);
            tree_target(lv_buttonmatrix_get_button_text(obj, i), "keyboard.key", cell, depth + 1);
        }
    } else if (lv_obj_has_flag(obj, LV_OBJ_FLAG_CLICKABLE) && !lv_obj_has_state(obj, LV_STATE_DISABLED) &&
               (ink_shell_bound_cap(obj) || lv_obj_get_event_count(obj))) {
        tree_target(node_text(obj), ink_shell_bound_cap(obj), hit, depth);
    }
    if (!lv_obj_has_flag(obj, LV_OBJ_FLAG_OVERFLOW_VISIBLE)) {
        clip.x1 = LV_MAX(clip.x1, own.x1); clip.y1 = LV_MAX(clip.y1, own.y1);
        clip.x2 = LV_MIN(clip.x2, own.x2); clip.y2 = LV_MIN(clip.y2, own.y2);
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(obj); i++) tree_node(lv_obj_get_child(obj, (int32_t)i), clip, depth + 1);
}
static void check_about_frames(lv_obj_t *obj) {
    if (!lv_obj_is_visible(obj)) return;
    CHECK(!lv_obj_get_style_border_width(obj, 0) ||
        lv_obj_get_style_border_side(obj, 0) != (LV_BORDER_SIDE_LEFT | LV_BORDER_SIDE_RIGHT), "about has no orphan side frame");
    for (uint32_t i = 0; i < lv_obj_get_child_count(obj); i++) check_about_frames(lv_obj_get_child(obj, (int32_t)i));
}
static lv_obj_t *field_named(const char *title) {
    wanted = title; find_label();
    CHECK(found_label, "field label is visible: %s", title);
    return found_label ? lv_obj_get_parent(found_label) : NULL;
}
static void check_page_repairs(void) {
    if (!strcmp(scene, "Sms") || !strcmp(scene, "SmsThread"))
        CHECK(!ink_test_pagebar(lv_screen_active(), 1, 1), "empty SMS page has no pager");
    if (!strcmp(scene, "SetAbout")) check_about_frames(lv_screen_active());
    if (!strcmp(scene, "Main")) {
        unsigned strips = 0;
        for (uint32_t i = 0; i < lv_obj_get_child_count(lv_screen_active()); i++) {
            lv_obj_t *o = lv_obj_get_child(lv_screen_active(), (int32_t)i);
            if (lv_obj_get_height(o) != ink_ui_px(INK_GAP_XL) || lv_obj_get_child_count(o) != 3) continue;
            strips++;
            for (int dot = 0; dot < 3; dot++) {
                lv_obj_t *d = lv_obj_get_child(o, dot);
                CHECK(lv_obj_get_width(d) == ink_ui_px(INK_PAGE_DOT) && lv_obj_get_height(d) == ink_ui_px(INK_PAGE_DOT), "home indicators are equal dots");
                if (dot) CHECK(lv_obj_get_x(d) - lv_obj_get_x(lv_obj_get_child(o, dot - 1)) == ink_ui_px(INK_GAP_L), "home dots remain separated");
            }
        }
        CHECK(strips == 1, "one home indicator strip");
    }
    const char *title = !strcmp(scene, "Contacts") ? "搜索联系人" : !strcmp(scene, "SmsThread") ? "短信内容" : !strcmp(scene, "Phone") ? "号码" : NULL;
    if (!title) return;
    lv_obj_t *f = field_named(title); lv_area_t a, b;
    if (!f) return;
    lv_obj_get_coords(f, &a);
    CHECK(a.y2 < ink_ui_height() - ink_ui_px(INK_MARGIN), "the complete field clears the footer");
    if (!strcmp(scene, "Contacts")) {
        wanted = "没有匹配的联系人"; find_label(); CHECK(found_label, "contact empty state is visible");
        if (found_label) { lv_obj_get_coords(lv_obj_get_parent(lv_obj_get_parent(found_label)), &b); CHECK(a.y2 < b.y1, "contact list begins below the whole search field"); }
    } else if (!strcmp(scene, "Phone")) {
        wanted = "1"; find_label(); CHECK(found_label, "dial pad is visible");
        if (found_label) { lv_obj_get_coords(lv_obj_get_parent(found_label), &b); CHECK(a.y2 < b.y1, "dial pad begins below the whole number field"); }
    }
}
static void inspect_design(void) {
    tree_json = osal_malloc(65536, OSAL_MEM_EXTERNAL);
    CHECK(tree_json, "tree buffer is available");
    if (!tree_json) return;
    ink_jw_init(&tree_writer, tree_json, 65536);
    ink_jw_raw(&tree_writer, "{\"scene\":"); ink_jw_str(&tree_writer, scene);
    ink_jw_raw(&tree_writer, ",\"page\":"); ink_jw_str(&tree_writer, ink_ui_page_name());
    ink_jw_raw(&tree_writer, ",\"overlay\":"); ink_jw_str(&tree_writer, ink_ui_top_name());
    ink_jw_raw(&tree_writer, ",\"targets\":["); first_node = true;
    lv_area_t canvas = {0, 0, ink_ui_width() - 1, ink_ui_height() - 1};
    lv_obj_update_layout(lv_screen_active()); lv_obj_update_layout(lv_layer_sys());
    tree_node(lv_screen_active(), canvas, 0); tree_node(lv_layer_sys(), canvas, 0);
    ink_jw_raw(&tree_writer, "]}");
    CHECK(!tree_writer.overflow, "complete control tree fits");
    assert_text_fits();
    check_page_repairs();
    read_screen(); CHECK(g_snap[0], "page provides its actual AI snapshot");
}
static void save_tree(void *arg) {
    (void)arg;
    const char *path = getenv("INK_DESIGN_TREE");
    if (!path || !tree_json) return;
    FILE *file = fopen(path, "w");
    if (!file) { perror("control tree"); exit(2); }
    bool written = fwrite(tree_json, 1, tree_writer.len, file) == tree_writer.len;
    int closed = fclose(file);
    if (!written || closed) { perror("control tree write"); exit(2); }
}
static void design_command(const char *id, const char *args) {
    CHECK(BY_HAND(id, args) && g_rc.status == INK_RC_DONE, "%.60s: %d %.100s", id, g_rc.status, g_rc.say);
    quiet();
}
static void open_design_app(void) {
    static const struct { const char *name, *app, *cap, *args; } cases[] = {
        {"Sms", "message", NULL, NULL}, {"SmsThread", "message", "sms.thread", "{\"peer\":\"00000000000\"}"},
        {"Phone", "phone", NULL, NULL}, {"PhoneRecents", "phone", "phone.recents", NULL},
        {"PhoneCall", "phone", "call.dial", "{\"number\":\"00000000000\"}"},
        {"Contacts", "phone", "contacts.open", NULL}, {"MemoEdit", "memo", "memo.edit", NULL},
        {"Clock", "clock", NULL, NULL}, {"ClockTimer", "clock", "clock.timer", NULL},
        {"ClockStopwatch", "clock", "clock.stopwatch", NULL}, {"Calculator", "calculator", NULL, NULL},
        {"Calendar", "upcoming", "calendar.open", NULL}, {"Weather", "weather", NULL, NULL},
        {"Gallery", "gallery", NULL, NULL}, {"Camera", "camera", NULL, NULL}, {"Files", "files", NULL, NULL}
    };
    for (unsigned i = 0; i < sizeof cases / sizeof *cases; i++) {
        if (strcmp(scene, cases[i].name)) continue;
        char args[80]; snprintf(args, sizeof args, "{\"id\":\"%s\"}", cases[i].app);
        design_command("app.open", args);
        if (cases[i].cap) design_command(cases[i].cap, cases[i].args);
        return;
    }
}
static void design_task(void *arg) {
    (void)arg;
    vtk_host_call(read_environment, NULL);
    CHECK(WAIT_FOR(ink_member_space_open(), 10000), "member space opens");
    test_start_home(); quiet(); ink_power_hold(INK_NEED_AWAKE);
    set_level("display.font_size", requested_size); quiet();
    if (!strcmp(scene, "Ai")) design_command("reader.open_book", "{\"title\":\"星星邮差\"}");
    on_ui(prepare); open_design_app();
    if (!strcmp(scene, "Lock")) { set_level("display.lock_style", 1); ink_power_lock(); }
    if (!strcmp(scene, "PowerMenu")) { hw(HW_KEY_DOWN, HAL_KEY_POWER, 0); delay_ms(1600); hw(HW_KEY_UP, HAL_KEY_POWER, 0); }
    if (!strcmp(scene, "Ai")) CHECK(WAIT_FOR(sim_metric_value("ai.answers") >= 1, 20000), "fixture AI answer arrives");
    quiet(); on_ui(inspect_design); vtk_host_call(save_tree, NULL); osal_free(tree_json); tree_json = NULL;
    snapshot_now(); ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    ink_cap_register(FIXTURES, sizeof FIXTURES / sizeof *FIXTURES);
    osal_task_cfg_t cfg = {.name = "design-lang", .fn = design_task, .stack_bytes = 16384,
        .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    if (osal_task_create(&cfg, NULL) != OSAL_OK) osal_panic("design language task");
}
