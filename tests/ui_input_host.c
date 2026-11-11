/* F2 exercises real controls, the shared input service and the capability bus. */
#include "ink/sys/textfield.h"
#include "ink/ui_shell.h"
#define ink_main product_main
#include "../system/boot/main.c"
#undef ink_main
#include "integration/whole_product.h"

static ink_ui_field_spec_t spec = {.id = "query", .role = "textbox", .label = "搜索", .object = {"example:query", 1}};
static const ink_ui_action_spec_t action = {
    .id = "focus", .label = "输入", .capability = "test.short_focus", .args_json = "{}", .enabled = true
};
static const ink_ui_page_spec_t page = {
    .id = "test.short_input", .title = "短输入测试", .object = {"example:query", 1},
    .fields = &spec, .field_count = 1, .actions = &action, .action_count = 1
};
static lv_obj_t *field, *heading;
static const char URL[] = "example.com/a_b?x=1%20#c";
static ink_text_type_t kind = INK_TEXT_PLAIN;
static lv_area_t original, heading_original, fixed_keyboard;
static char snapshot[3072], page_json[2048], events[768], hand_events[768], submitted[128], stale_args[256];
static size_t events_len;
static int edits, enters, hand_edits, hand_enters;
static ink_source_t focus_source;

static void record(const char *event, const char *value) {
    int n = snprintf(events + events_len, sizeof events - events_len, "%s:%s\n", event, value);
    CHECK(n >= 0 && (size_t)n < sizeof events - events_len, "the synthetic event record fits");
    if (n >= 0 && (size_t)n < sizeof events - events_len) events_len += (size_t)n;
}
static void changed(lv_event_t *event) {
    (void)event;
    edits++;
    CHECK(spec.value && !strcmp(spec.value, ink_textfield_text(field)), "the F1 field value follows every edit");
    record("changed", spec.value);
}
static void entered(void *ctx, const char *value) {
    (void)ctx;
    enters++;
    snprintf(submitted, sizeof submitted, "%s", value);
    record("entered", value);
    CHECK(!ink_text_focused(), "completion ends local editing before notifying the page");
}
static void focus_cap(ink_call_t *call) {
    focus_source = call->src;
    if (ink_textfield_short_focus(field) != 0) call->rc.status = INK_RC_STALE;
}
static const ink_cap_t CAPS[] = {
    {.id = "test.short_focus", .title = "输入搜索或网址", .activity = "test.short_input",
     .effect = INK_FX_NAVIGATE, .child_ok = true, .run = focus_cap}
};
static void create(lv_obj_t *screen, void *arg) {
    (void)arg;
    ink_shell_topbar(screen, page.title);
    heading = ink_ui_line(screen, "原来的位置", INK_FONT_LIST, INK_WEIGHT_REGULAR,
                          INK_MARGIN, INK_STATUS_BAR + INK_PRIMARY, INK_CANVAS_W - 2 * INK_MARGIN);
    ink_text_cfg_t cfg = {.type = kind, .enter = kind == INK_TEXT_URL ? INK_ENTER_DONE : INK_ENTER_SEARCH,
                          .initial = "", .hint = kind == INK_TEXT_URL ? "输入网址" : "输入搜索词", .max = 128};
    field = ink_textfield_create(screen, &cfg, entered, NULL, false);
    CHECK(field != NULL, "create the production text field");
    if (!field) return;
    lv_obj_set_pos(field, ink_ui_px(INK_MARGIN), ink_ui_px(INK_CANVAS_H - INK_MARGIN - INK_PRIMARY));
    lv_obj_set_width(field, ink_ui_px(INK_CANVAS_W - 2 * INK_MARGIN));
    CHECK(ink_textfield_bind_short(field, &page, &spec, &action) == 0, "bind the F1 declaration to the short host");
    lv_obj_add_event_cb(field, changed, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_update_layout(screen);
    lv_obj_get_coords(field, &original); lv_obj_get_coords(heading, &heading_original);
    events_len = 0; events[0] = submitted[0] = 0; edits = enters = 0;
}
static int page_snapshot(char *out, size_t cap) { return ink_textfield_short_snapshot(field, out, cap); }
static const ink_activity_t PAGE = {.name = "test.short_input", .create = create, .snapshot = page_snapshot};
static void open_page(void) { ink_ui_home(); ink_ui_push(&PAGE, NULL); }
static void ai_focus(void) {
    ink_receipt_t rc;
    ink_ui_action_target_t target = {ink_ui_content_version(), page.object};
    CHECK(ink_ui_semantics_action(&page, action.id, &target, INK_SRC_AI, &rc) == INK_RC_DONE,
          "AI focuses the same field through the declared capability");
}
static bool same_area(const lv_area_t *a, const lv_area_t *b) {
    return a->x1 == b->x1 && a->x2 == b->x2 && a->y1 == b->y1 && a->y2 == b->y2;
}
static lv_obj_t *find_label(lv_obj_t *root, const char *text) {
    if (lv_obj_check_type(root, &lv_label_class) && !strcmp(lv_label_get_text(root), text)) return root;
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); i++) {
        lv_obj_t *found = find_label(lv_obj_get_child(root, (int32_t)i), text);
        if (found) return found;
    }
    return NULL;
}
static const char *key_label;
static lv_point_t key_point;
static bool key_visible;
static void locate_key(void) {
    lv_obj_update_layout(lv_layer_sys());
    lv_obj_t *label = find_label(lv_layer_sys(), key_label);
    key_visible = label && lv_obj_is_visible(label);
    CHECK(key_visible, "the visible keyboard has the requested key: %s", key_label);
    if (!key_visible) return;
    lv_area_t button, keyboard;
    lv_obj_get_coords(lv_obj_get_parent(label), &button); ink_shell_keyboard_area(&keyboard);
    CHECK(button.x1 >= keyboard.x1 && button.x2 <= keyboard.x2 && button.y1 >= keyboard.y1 && button.y2 <= keyboard.y2,
          "the actual key is inside the fixed keyboard: %s", key_label);
    key_point = (lv_point_t){(button.x1 + button.x2) / 2, (button.y1 + button.y2) / 2};
}
static void tap_keyboard(const char *label) {
    key_label = label; on_ui(locate_key);
    if (key_visible) tap(key_point.x, key_point.y);
}
static void inspect_focus(void) {
    const char *name = NULL;
    CHECK(ink_ui_snapshot(snapshot, sizeof snapshot, &name) > 0 && name && !strcmp(name, "Keyboard"),
          "snapshot includes the page and visible keyboard");
    CHECK(ink_ui_context_snapshot(g_snap, sizeof g_snap, NULL) > 0 && !strcmp(snapshot, g_snap),
          "AI reads the same input and page snapshot");
    ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL);
    CHECK(j && ink_json_parse(j, snapshot, strlen(snapshot)) == 0, "combined snapshot is valid JSON");
    if (!j) return;
    int64_t session = 0, revision = 0;
    CHECK(ink_json_int(j, ink_json_find(j, 0, "keyboard.session"), &session) &&
          ink_json_int(j, ink_json_find(j, 0, "keyboard.revision"), &revision), "snapshot identifies the exact input session");
    snprintf(stale_args, sizeof stale_args, "{\"session\":%lld,\"revision\":%lld,\"text\":\"OLD_MEMBER_INPUT\"}",
             (long long)session, (long long)revision);
    CHECK(ink_json_str(j, ink_json_find(j, 0, "page"), page_json, sizeof page_json) > 0 &&
          ink_json_parse(j, page_json, strlen(page_json)) == 0, "the F1 page model survives the keyboard wrapper");
    int64_t y = -1, height = -1, keyboard_y = -1;
    char id[32];
    CHECK(ink_json_str(j, ink_json_find(j, 0, "input.field"), id, sizeof id) > 0 && !strcmp(id, spec.id) &&
          ink_json_eq(j, ink_json_find(j, 0, "input.focused"), "true"), "snapshot names the focused field");
    CHECK(ink_json_int(j, ink_json_find(j, 0, "input.rect.y"), &y) &&
          ink_json_int(j, ink_json_find(j, 0, "input.rect.height"), &height) &&
          ink_json_int(j, ink_json_find(j, 0, "input.keyboard_rect.y"), &keyboard_y) && y >= 0 && height > 0 && y + height <= keyboard_y,
          "snapshot proves that the keyboard does not cover the input field");
    osal_free(j);
    lv_area_t actual, other;
    lv_obj_get_coords(field, &actual); lv_obj_get_coords(heading, &other);
    CHECK(actual.y1 == y && lv_area_get_height(&actual) == height && same_area(&other, &heading_original),
          "snapshot geometry matches LVGL and surrounding content stays in place");
    CHECK(lv_obj_has_state(field, LV_STATE_FOCUSED) && lv_obj_get_style_outline_width(lv_obj_get_child(field, 1), 0) == ink_ui_px(INK_STROKE),
          "the real focused field has a visible black outline using the standard stroke");
    lv_obj_t *space = find_label(lv_layer_sys(), "空格");
    CHECK(space != NULL, "the real system keyboard has a space key");
    if (space) {
        lv_obj_t *root = space;
        for (int i = 0; i < 4; i++) root = lv_obj_get_parent(root);
        lv_obj_get_coords(root, &actual); ink_shell_keyboard_area(&other);
        CHECK(same_area(&actual, &other) && actual.y1 == keyboard_y,
              "keyboard actual=%d,%d,%d,%d declared=%d,%d,%d,%d snapshot-y=%lld", actual.x1, actual.y1, actual.x2, actual.y2,
              other.x1, other.y1, other.x2, other.y2, (long long)keyboard_y);
        if (!fixed_keyboard.y2) fixed_keyboard = actual;
        CHECK(same_area(&actual, &fixed_keyboard), "keyboard position stays fixed across input and mode changes");
    }
}
static void inspect_restored(void) {
    lv_area_t actual, other;
    lv_obj_update_layout(field); lv_obj_get_coords(field, &actual); lv_obj_get_coords(heading, &other);
    CHECK(!ink_text_focused() && !strcmp(ink_ui_page_name(), page.id) && !strcmp(ink_ui_top_name(), page.id),
          "completion closes the keyboard on the original page");
    CHECK(same_area(&actual, &original) && same_area(&other, &heading_original) && !lv_obj_has_state(field, LV_STATE_FOCUSED),
          "completion restores the exact field and surrounding positions without focus styling");
    CHECK(ink_ui_snapshot(snapshot, sizeof snapshot, NULL) > 0 && strstr(snapshot, "\"focused\":false") &&
          !strstr(snapshot, "\"keyboard\":"), "restored snapshot clears focus and the active session");
}
static void inspect_done(void) {
    inspect_restored();
    CHECK(!strcmp(ink_textfield_text(field), submitted) && !strcmp(spec.value, submitted), "completion keeps the submitted value in the F1 field");
}
static void compare_events(void) {
    CHECK(edits == hand_edits && enters == hand_enters && !strcmp(events, hand_events) && !strcmp(submitted, "合成查询"),
          "human paste and AI fill produce identical field values and change/submit events");
}
static void inspect_url(void) {
    CHECK(!strcmp(ink_textfield_text(field), URL) && !ink_text_composing()[0],
          "real URL key taps insert exact characters without pinyin composition");
}
static void inspect_long_url(void) {
    const char *value = "https://example.com/abcdefghijklmnop?x=1";
    ink_text_select(0, 128); ink_text_commit_utf8(value);
    lv_obj_update_layout(field);
    lv_obj_t *area = lv_obj_get_child(field, 1), *label = lv_textarea_get_label(area);
    const char *line = lv_label_get_text(label);
    CHECK(!strcmp(ink_textfield_text(field), value) && !strcmp(line, value) &&
          lv_textarea_get_cursor_pos(area) == (uint32_t)strlen(value),
          "native URL retains the full text and positions its cursor at the tail");
    lv_point_t point;
    lv_label_get_letter_pos(label, (uint32_t)strlen(value), &point);
    lv_area_t content, bounds;
    lv_obj_get_content_coords(area, &content); lv_obj_get_coords(label, &bounds);
    CHECK(bounds.x1 + point.x >= content.x1 && bounds.x1 + point.x <= content.x2 + 1,
          "the native cursor fits the visible field");
}
static void handoff(void) {
    char before[128]; snprintf(before, sizeof before, "%s", ink_textfield_text(field));
    int before_edits = edits, before_enters = enters;
    CHECK(ink_member_switch("kid") == 0, "switch member with an existing short input session");
    ink_receipt_t rc;
    CHECK(ink_cap_invoke_json("text.insert", stale_args, INK_SRC_AI, 0, &rc) == INK_RC_STALE,
          "the new member cannot use the old session even before UI notifications drain");
    CHECK(ink_cap_invoke_json("text.submit", stale_args, INK_SRC_AI, 0, &rc) == INK_RC_STALE,
          "the new member cannot submit the old session");
    CHECK(edits == before_edits && enters == before_enters && !strcmp(before, ink_textfield_text(field)),
          "refused old-session writes have no field or event effects");
    CHECK(ink_textfield_short_focus(field) < 0 && ink_textfield_short_snapshot(field, snapshot, sizeof snapshot) < 0 && !snapshot[0],
          "the old host cannot be focused or read by the new member");
    ink_probe_count("test.finished");
}
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void take_snapshot(void *name) { sim_display_snapshot(name); }
static void capture(const char *name) {
    CHECK(WAIT_FOR(settled(), 10000), "screen settles before capture");
    vtk_event_at(vtk_now() + SIM_MS(1), take_snapshot, (void *)name); delay_ms(5);
}
static void check_specs(void) {
    int old_size = (int)ink_setting_get("display.font_size"), normal_label = 0, normal_height = 0;
    for (int level = 1; level <= 3; level += 2) {
        ink_receipt_t rc; ink_setting_set("display.font_size", level, INK_SRC_TOUCH, &rc);
        lv_obj_t *parent = lv_obj_create(lv_screen_active());
        lv_obj_remove_style_all(parent); lv_obj_set_size(parent, ink_ui_px(INK_CANVAS_W), ink_ui_px(INK_CANVAS_H));
        for (int style = INK_FIELD_SINGLE; style <= INK_FIELD_PASSWORD; style++) {
            ink_text_cfg_t cfg = {.type = style == INK_FIELD_PASSWORD ? INK_TEXT_PASSWORD : INK_TEXT_PLAIN,
                                   .hint = "合成占位", .initial = style == INK_FIELD_PASSWORD ? "秘密😀" : "合成输入", .max = 128};
            lv_obj_t *a = ink_textfield_create(parent, &cfg, NULL, NULL, false);
            lv_obj_t *b = ink_textfield_create(parent, &cfg, NULL, NULL, false);
            ink_textfield_style(a, (ink_textfield_kind_t)style, "持续可见的字段标签");
            ink_textfield_style(b, (ink_textfield_kind_t)style, "持续可见的字段标签");
            lv_obj_set_size(b, 123, 57); /* Legacy callers cannot change component geometry. */
            lv_obj_update_layout(parent);
            lv_obj_t *area = lv_obj_get_child(a, 1), *label = lv_obj_get_child(a, 0);
            CHECK(lv_obj_get_height(a) == ink_ui_px(ink_textfield_height((ink_textfield_kind_t)style)) &&
                  lv_obj_get_height(a) == lv_obj_get_height(b), "same style ignores legacy dimensions at %d%%", level == 1 ? 100 : 120);
            CHECK(lv_obj_get_width(a) == ink_ui_px(INK_CANVAS_W - 2 * INK_MARGIN) && lv_obj_get_width(a) == lv_obj_get_width(b),
                  "all input widths follow the container and theme margins");
            CHECK(lv_obj_get_style_pad_left(area, 0) == ink_ui_px(style == INK_FIELD_SEARCH ? INK_GAP_L + INK_FIELD_SEARCH_ICON + INK_GAP_M : INK_FIELD_PAD) &&
                  lv_obj_get_style_border_width(area, 0) == ink_ui_px(INK_FIELD_STROKE), "native inputs use theme padding and stroke");
            CHECK(lv_obj_get_height(label) >= ink_ui_px(ink_ui_line_height(INK_FONT_FIELD_LABEL)) &&
                  lv_obj_get_y(area) >= lv_obj_get_height(label) + ink_ui_px(INK_FIELD_LABEL_GAP), "label fits and stays above its input");
            if (style == INK_FIELD_SINGLE) {
                if (level == 1) { normal_label = lv_obj_get_height(label); normal_height = lv_obj_get_height(area); }
                else CHECK(lv_obj_get_height(label) > normal_label && lv_obj_get_height(area) > normal_height, "120%% enlarges both label and input");
            }
            if (style == INK_FIELD_PASSWORD) CHECK(!strcmp(lv_label_get_text(lv_textarea_get_label(area)), "***"), "native password masks Unicode codepoints immediately");
            int before = lv_obj_get_height(a);
            ink_textfield_error(a, "合成错误：请修改这个字段后再试"); lv_obj_update_layout(a);
            CHECK(lv_obj_get_height(a) > before, "inline error expands the field instead of overlaying it");
            ink_textfield_error(a, NULL); lv_obj_update_layout(a);
            CHECK(lv_obj_get_height(a) == before, "removing an error restores the standard height");
            lv_obj_delete(a); lv_obj_delete(b);
        }
        lv_obj_delete(parent);
    }
    ink_receipt_t rc; ink_setting_set("display.font_size", old_size, INK_SRC_TOUCH, &rc);
}
static void run(void *arg) {
    (void)arg; delay_ms(1800);
    CHECK(WAIT_FOR(settled(), 10000), "home settles");
    on_ui(check_specs);
    on_ui(open_page); capture("search-before");
    tap((original.x1 + original.x2) / 2, (original.y1 + original.y2) / 2);
    capture("search-focused"); on_ui(inspect_focus);
    CHECK(focus_source == INK_SRC_TOUCH, "a real field tap uses the F1 focus capability");
    CHECK(BY_HAND("clipboard.write", "{\"text\":\"合成查询\"}") && g_rc.status == INK_RC_DONE &&
          BY_HAND("keyboard.edit", "{\"action\":4}") && g_rc.status == INK_RC_DONE, "human pastes through the production keyboard capability");
    CHECK(BY_HAND("keyboard.key", "{\"code\":10}") && g_rc.status == INK_RC_DONE, "human completes with the real enter action");
    capture("search-restored"); on_ui(inspect_done);
    snprintf(hand_events, sizeof hand_events, "%s", events); hand_edits = edits; hand_enters = enters;
    CHECK(hand_edits == 1 && hand_enters == 1, "human edit emits one change and one submit");
    on_ui(open_page); on_ui(ai_focus);
    CHECK(focus_source == INK_SRC_AI, "AI uses the same focus capability");
    CHECK(AI("text.insert", "{\"text\":\"合成查询\"}") && g_rc.status == INK_RC_DONE &&
          AI("text.submit", "{}") && g_rc.status == INK_RC_DONE, "AI fills and completes through the shared text capabilities");
    on_ui(inspect_done); on_ui(compare_events);
    kind = INK_TEXT_URL; on_ui(open_page); on_ui(ai_focus); on_ui(inspect_focus);
    bool symbols = false;
    for (const char *p = URL; *p; p++) {
        bool next_symbols = !(*p >= 'a' && *p <= 'z');
        if (symbols != next_symbols) { tap_keyboard(symbols ? "字母" : "符号"); symbols = next_symbols; }
        char label[2] = {*p, 0}; tap_keyboard(label);
    }
    if (symbols) tap_keyboard("字母");
    on_ui(inspect_url); capture("url-focused"); on_ui(inspect_focus);
    tap_keyboard("符号"); capture("url-symbols"); on_ui(inspect_focus);
    tap_keyboard("字母");
    on_ui(inspect_focus);
    key(HAL_KEY_HOME); capture("url-restored"); on_ui(inspect_restored);
    CHECK(!ink_text_focused(), "the Plus HOME key first closes the short input session");
    on_ui(ai_focus); on_ui(inspect_long_url); capture("url-long-cursor"); on_ui(inspect_focus); on_ui(handoff);
}
void ink_main(void) {
    product_main();
    CHECK(ink_cap_register(CAPS, sizeof CAPS / sizeof *CAPS) == 0, "register the synthetic page's focus capability");
    osal_task_cfg_t cfg = {.name = "ui-input-host-test", .fn = run, .stack_bytes = 16384,
                           .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start the test driver");
}
