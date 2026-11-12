/* Real input service, keyboard controls, capabilities and rendered frames. */
#include <stdlib.h>
#include <time.h>
#include "ink/sys/textfield.h"
#include "ink/sys/storage.h"
#include "src/widgets/buttonmatrix/lv_buttonmatrix_private.h"
#include "../system/services/text/text_internal.h"
#define ink_main product_main
#include "configured_boot.c"
#undef ink_main
#include "integration/whole_product.h"
#define key ink_test_keyboard_key
#include "../system/ui/shell/keyboard.c"
#undef key

static char snapshot[8192];
static const char *picture;
static volatile bool captured;
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }

static int key_index(int code) {
    for (int i = 0; i < K.buttons; i++) if (K.codes[i] == code) return i;
    return -1;
}
static lv_obj_t *button(lv_obj_t *root, int code) { (void)root; return key_index(code) < 0 ? NULL : K.board; }
static bool labeled(int code, const char *text) {
    int i = key_index(code);
    return i >= 0 && !strcmp(K.legends[i], text);
}
static void touch(int code) {
    int i = key_index(code);
    CHECK(i >= 0 && lv_obj_is_visible(K.board), "visible keyboard control: %d", code);
    if (i < 0) return;
    lv_obj_t *board = K.footer && i >= 9 ? K.footer : K.board;
    lv_buttonmatrix_set_selected_button(board, (uint32_t)(K.footer && i >= 9 ? i - 9 : i));
    lv_obj_send_event(board, LV_EVENT_PRESSED, NULL);
    lv_obj_send_event(board, LV_EVENT_VALUE_CHANGED, NULL);
}
static ink_rc_t ai_key(int code) {
    char args[40]; snprintf(args, sizeof args, "{\"code\":%d}", code);
    ink_receipt_t rc;
    return ink_cap_invoke_json("keyboard.key", args, INK_SRC_AI, ink_ui_content_version(), &rc);
}
static void expect_mode(bool english) {
    CHECK(ink_text_english() == english && labeled(INK_TEXT_KEY_LANGUAGE, english ? "英" : "中"), "current mode key matches service");
    CHECK(ink_ui_context_snapshot(snapshot, sizeof snapshot, NULL) > 0 && strstr(snapshot, english ? "输入=英文" : "输入=中文"), "AI snapshot follows language mode");
}
static void expect_icon(int code, const char *label) {
    lv_obj_t *icon = K.icons[code == KEY_BACKSPACE];
    CHECK(icon && lv_obj_check_type(icon, &lv_image_class) &&
          lv_obj_has_flag(K.board, LV_OBJ_FLAG_SEND_DRAW_TASK_EVENTS) && labeled(code, label),
          "matrix draws the theme icon and retains the accessible label: %d", code);
}
static void functional(void) {
    ink_text_cfg_t cfg = {.type = INK_TEXT_PLAIN, .max = 128};
    CHECK(!ink_text_english(), "a fresh process starts in Chinese");
    CHECK(ink_text_focus(&cfg, NULL, NULL) > 0, "focus plain input");
    lv_area_t normal; ink_shell_keyboard_area(&normal);
    expect_mode(false);
    expect_icon(KEY_SHIFT, TR("大写")); expect_icon(KEY_BACKSPACE, TR("删除"));
    CHECK(strstr(snapshot, "大写=keyboard.key(2)") && strstr(snapshot, "删除=keyboard.key(8)"), "icon labels are in semantic snapshot");
    CHECK(labeled(',', "，") && labeled('.', "。"), "Chinese bottom punctuation labels");
    CHECK(strstr(ink_cap_find("keyboard.key")->input, "3 中英切换"), "AI discovers switch code in existing capability schema");
    ink_text_key('n'); ink_text_key('i');
    ink_text_cand_t c;
    CHECK(ink_text_candidates(0, &c, 1) == 1, "Chinese pinyin has a real candidate");
    touch(INK_TEXT_KEY_LANGUAGE);
    CHECK(!strcmp(ink_text_value(), "ni") && !ink_text_composing()[0], "switch commits pending pinyin verbatim");
    expect_mode(true);
    touch('a'); touch('b'); touch(' '); touch('c');
    CHECK(!strcmp(ink_text_value(), "niab c") && !ink_text_composing()[0], "English letters commit directly and space inserts a space");
    CHECK(ai_key(INK_TEXT_KEY_LANGUAGE) == INK_RC_DONE, "AI switches through keyboard.key");
    expect_mode(false);
    ink_text_key('n'); ink_text_key('i');
    CHECK(!strcmp(ink_text_composing(), "ni") && ink_text_candidates(0, &c, 1) == 1, "switching back restores pinyin candidates");
    CHECK(ink_text_focus(&cfg, NULL, NULL) > 0, "new field for punctuation");
    touch(','); touch('.');
    CHECK(!strcmp(ink_text_value(), "，。"), "Chinese punctuation is full width");
    touch(KEY_SWITCH);
    CHECK(labeled('?', "？") && labeled(';', "；"), "Chinese symbols match insertion");
    touch('?');
    CHECK(!strcmp(ink_text_value(), "，。？"), "Chinese symbol key inserts full width");
    CHECK(ai_key(INK_TEXT_KEY_LANGUAGE) == INK_RC_DONE, "switch while symbols are visible");
    expect_mode(true);
    CHECK(labeled(',', ",") && labeled('.', ".") && labeled('?', "?"), "English symbols use ASCII labels");
    touch(','); touch('.'); touch('?');
    CHECK(!strcmp(ink_text_value(), "，。？,.?"), "English punctuation is half width");
    CHECK(ink_text_focus(&cfg, NULL, NULL) > 0, "move to a different plain field");
    expect_mode(true);
    touch('x'); touch(' ');
    CHECK(!strcmp(ink_text_value(), "x ") && !ink_text_composing()[0], "new field retains English input behavior");
    static const ink_text_type_t TYPES[] = {INK_TEXT_PASSWORD, INK_TEXT_URL, INK_TEXT_NUMBER, INK_TEXT_PHONE};
    for (size_t i = 0; i < sizeof TYPES / sizeof *TYPES; i++) {
        cfg.type = TYPES[i];
        CHECK(ink_text_focus(&cfg, NULL, NULL) > 0, "focus specialized input: %d", cfg.type);
        CHECK(!button(K.root, INK_TEXT_KEY_LANGUAGE), "specialized input has no language switch: %d", cfg.type);
        ink_text_key(INK_TEXT_KEY_LANGUAGE);
        CHECK(ink_text_english() && !ink_text_value()[0], "specialized input ignores language code: %d", cfg.type);
        if (cfg.type == INK_TEXT_PASSWORD) {
            lv_area_t compact, actual; ink_shell_keyboard_area(&compact); lv_obj_update_layout(K.root); lv_obj_get_coords(K.root, &actual);
            int saved = ink_ui_px(ink_ui_line_height(INK_FONT_LIST) + candidates_height() + 2 * INK_KB_GAP);
            CHECK(compact.y1 - normal.y1 == saved && actual.y1 == compact.y1, "password keyboard removes the entire candidate band from its reported and actual height");
            CHECK(!lv_obj_is_visible(K.cands) && !lv_obj_is_visible(K.composing), "password hides candidates and composing");
            touch(KEY_SWITCH); ink_shell_keyboard_area(&actual);
            CHECK(actual.y1 == compact.y1 && !lv_obj_is_visible(K.cands), "password symbols retain the compact height");
            touch(KEY_SWITCH);
            CHECK(ai_key(INK_TEXT_KEY_LANGUAGE) == INK_RC_REJECTED, "AI cannot operate a password field");
            touch('n'); touch('i');
            CHECK(!strcmp(ink_text_value(), "ni") && !ink_text_composing()[0], "password letters stay literal");
            CHECK(ink_ui_context_snapshot(snapshot, sizeof snapshot, NULL) > 0 && !strstr(snapshot, "\"ni\""), "password value stays masked");
        } else if (cfg.type == INK_TEXT_URL) {
            lv_area_t restored; ink_shell_keyboard_area(&restored); lv_obj_update_layout(K.root);
            CHECK(restored.y1 == normal.y1 && lv_obj_is_visible(K.composing) && lv_obj_is_visible(K.cands), "leaving password symbols restores ordinary input geometry");
            touch('n'); touch('i'); touch(KEY_SWITCH); touch('.');
            CHECK(!strcmp(ink_text_value(), "ni.") && !ink_text_composing()[0], "URL keeps literal letters and punctuation");
        } else {
            touch(cfg.type == INK_TEXT_PHONE ? '+' : '.'); touch('1');
            CHECK(!strcmp(ink_text_value(), cfg.type == INK_TEXT_PHONE ? "+1" : ".1"), "numeric/phone behavior unchanged");
        }
    }
    cfg.type = INK_TEXT_PLAIN;
    CHECK(ink_text_focus(&cfg, NULL, NULL) > 0, "return to plain input");
    lv_area_t restored; ink_shell_keyboard_area(&restored);
    CHECK(restored.y1 == normal.y1 && lv_obj_is_visible(K.cands), "returning to plain input restores its candidate band");
    expect_mode(true);
    ink_text_blur(0);
    /* A short field focused right after password input measures the plain keyboard it will get (polish-1 probe). */
    ink_text_cfg_t secret = {.type = INK_TEXT_PASSWORD, .max = 32};
    ink_text_focus(&secret, NULL, NULL); ink_text_blur(0);
    ink_text_cfg_t ordinary = {.type = INK_TEXT_PLAIN, .max = 32};
    lv_obj_t *field = ink_textfield_create(lv_screen_active(), &ordinary, NULL, NULL, false);
    ink_textfield_style(field, INK_FIELD_SINGLE, "合成短输入");
    lv_obj_set_pos(field, ink_ui_px(INK_MARGIN), ink_ui_height() - ink_ui_px(INK_MARGIN + ink_textfield_height(INK_FIELD_SINGLE)));
    CHECK(ink_textfield_short_focus(field) == 0, "focus ordinary short field after password");
    lv_area_t bounds, keyboard; lv_obj_update_layout(field); lv_obj_get_coords(field, &bounds); ink_shell_keyboard_area(&keyboard);
    CHECK(bounds.y2 < keyboard.y1, "ordinary short field must fit after password: bottom=%d keyboard=%d", (int)bounds.y2, (int)keyboard.y1);
    lv_obj_delete(field);
}

static void language_over_ai_panel(void) {
    ink_text_cfg_t cfg = {.type = INK_TEXT_PLAIN, .max = 32};
    CHECK(ink_text_focus(&cfg, NULL, NULL) > 0, "focus above AI panel");
    lv_obj_t *panel, *status, *question, *answer;
    ink_shell_ai_open(&panel, &status, &question, &answer);
    CHECK(lv_obj_get_index(K.root) < lv_obj_get_index(panel), "AI panel needs keyboard ordering");
    bool english = ink_text_english();
    touch(INK_TEXT_KEY_LANGUAGE);
    expect_mode(!english);
    ink_shell_ai_closed();
    ink_text_blur(0);
}

static bool contains(const lv_area_t *outer, const lv_area_t *inner) {
    return inner->x1 >= outer->x1 && inner->y1 >= outer->y1 && inner->x2 <= outer->x2 && inner->y2 <= outer->y2;
}
static void inspect(void) {
    CHECK(ink_ui_context_snapshot(snapshot, sizeof snapshot, NULL) > 0, "capture real semantic snapshot");
    lv_obj_update_layout(K.root);
    lv_area_t root; lv_obj_get_coords(K.root, &root);
    CHECK(lv_obj_check_type(K.board, &lv_keyboard_class), "keys use lv_keyboard");
    CHECK(lv_keyboard_get_textarea(K.board) == NULL, "no textarea input path");
    for (uint32_t i = 0; i < lv_obj_get_event_count(K.board); i++)
        CHECK(lv_event_dsc_get_cb(lv_obj_get_event_dsc(K.board, i)) != lv_keyboard_def_event_cb, "default textarea writer removed");
    lv_buttonmatrix_t *matrix = (lv_buttonmatrix_t *)K.board;
    lv_area_t bounds; lv_obj_get_coords(K.board, &bounds);
    int row_y = -1, min_width = 0, max_width = 0, letter_width = 0;
    for (int i = 0; i < K.buttons; i++) {
        if (K.footer && i == 9) {
            matrix = (lv_buttonmatrix_t *)K.footer; lv_obj_get_coords(K.footer, &bounds);
        }
        lv_area_t a = matrix->button_areas[K.footer && i >= 9 ? i - 9 : i];
        int width = lv_area_get_width(&a);
        if (K.codes[i] >= 'a' && K.codes[i] <= 'z') {
            if (!letter_width) letter_width = width;
            CHECK(abs(width - letter_width) <= 2, "letters share one width across rows (pixel rounding)");
        }
        if (a.y1 != row_y) { row_y = a.y1; min_width = max_width = 0; }
        if ((K.control[i] & 0x0f) == INK_KB_UNIT && K.codes[i]) {
            if (!min_width || width < min_width) min_width = width;
            if (width > max_width) max_width = width;
            CHECK(max_width - min_width <= 1, "equal keys in each row differ by at most one pixel");
        }
        a.x1 += bounds.x1; a.x2 += bounds.x1; a.y1 += bounds.y1; a.y2 += bounds.y1;
        CHECK(contains(&root, &a), "native matrix key fits in keyboard: %s", picture);
        if (!K.codes[i]) continue;
        lv_point_t text;
        lv_text_get_size(&text, K.legends[i], font(), 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
        if (K.codes[i] == KEY_BACKSPACE || (K.codes[i] == KEY_SHIFT && !K.symbols) ||
            (K.codes[i] == KEY_ENTER && ink_ui_text_width(enter_label(), INK_FONT_LIST, INK_WEIGHT_REGULAR) > INK_KB_FUNCTION_WIDTH))
            text.x = text.y = ink_ui_px(INK_ICON_MEDIUM * key_height(false) / INK_KB_KEY);
        CHECK(text.x <= width && text.y <= lv_area_get_height(&a), "label fits native key: %s / %.23s", picture, K.legends[i]);
    }
    /* The number pad keeps tall digits; nine-key pinyin is as tall as the full keyboard (letter-height rows). */
    CHECK(lv_obj_get_height(K.board) == ink_ui_px((K.footer ? 3 : 4) * key_height(K.built_for == 3) + (K.footer ? 2 : 3) * INK_KB_GAP), "height comes from keyboard tokens and font scale");
    CHECK(lv_obj_get_width(K.mic) == ink_ui_px(INK_KB_FUNCTION_WIDTH) && lv_obj_get_height(K.mic) == ink_ui_px(key_height(false)), "voice uses function-key dimensions");

}
static void save(void *arg) {
    (void)arg;
    char path[1024]; int n = snprintf(path, sizeof path, "%s/%s.json", g_sim.out_dir, picture);
    if (n < 0 || (size_t)n >= sizeof path) abort();
    FILE *f = fopen(path, "wb");
    if (!f || fputs(snapshot, f) == EOF || fclose(f)) abort();
}
static void frame(void *arg) { sim_display_snapshot(arg); captured = true; }
static void capture(const char *name) {
    picture = name;
    CHECK(WAIT_FOR(settled(), 15000), "frame settled: %s", name);
    on_ui(inspect); vtk_host_call(save, NULL);
    captured = false; vtk_event_at(vtk_now() + SIM_US(1), frame, (void *)name);
    CHECK(WAIT_FOR(captured, 2000), "frame saved: %s", name);
    ink_probe_count("test.frames");
}
/* Exercise the pointer pipeline, including overlay changes during a language switch. */
static void end_scene(void);
static int pointer_code, pointer_x, pointer_y;
static bool switch_nine, switch_english;
static uint32_t root_events;
static void switch_callbacks(void) { CHECK(lv_obj_get_event_count(K.root) == root_events, "mode switches keep root callback count stable"); }
static void pointer_target(void) {
    int i = key_index(pointer_code);
    CHECK(i >= 0, "switch test key exists: %d", pointer_code);
    if (i < 0) return;
    lv_obj_t *board = K.footer && i >= 9 ? K.footer : K.board;
    lv_obj_update_layout(K.root);
    lv_area_t bounds; lv_obj_get_coords(board, &bounds);
    lv_area_t a = ((lv_buttonmatrix_t *)board)->button_areas[K.footer && i >= 9 ? i - 9 : i];
    pointer_x = bounds.x1 + (a.x1 + a.x2) / 2;
    pointer_y = bounds.y1 + (a.y1 + a.y2) / 2;
}
static void pointer_key(int code) {
    pointer_code = code; on_ui(pointer_target);
    hw(HW_PRESS, pointer_x, pointer_y); delay_ms(80);
    hw(HW_RELEASE, pointer_x, pointer_y);
}
static void switch_setup(void) {
    ink_receipt_t rc;
    ink_setting_set("text.nine_key", switch_nine, INK_SRC_TOUCH, &rc);
    if (ink_text_english() != switch_english) ink_text_key(INK_TEXT_KEY_LANGUAGE);
    root_events = lv_obj_get_event_count(K.root);
}
static void switch_once(void) { touch(INK_TEXT_KEY_LANGUAGE); }
static void switch_keys(void) {
    static const int offsets[] = {300, 1000, 2000};
    for (int nine = 0; nine < 2; nine++) for (int english = 0; english < 2; english++) {
        CHECK(BY_HAND("memo.edit", NULL) && WAIT_FOR(ink_text_focused(), 10000), "open real memo for switching");
        switch_nine = nine; switch_english = english; on_ui(switch_setup);
        CHECK(WAIT_FOR(settled() && !ink_storage_pending(), 15000), "switch setup settles");
        uint32_t commits = ink_display_commits();
        on_ui(switch_once);
        CHECK(WAIT_FOR(settled(), 15000), "language refresh settles");
        CHECK(ink_display_commits() - commits == 1, "language switch draws one frame: %u", (unsigned)(ink_display_commits() - commits));
        on_ui(switch_setup);
        CHECK(WAIT_FOR(settled(), 15000), "pointer switch setup settles");
        double dropped = sim_metric_value("ui.touch_under_overlay_dropped");
        double clean = sim_metric_value("display.dispatch.FLASH_UI") + sim_metric_value("display.dispatch.FLASH_UI+clean");
        pointer_key(INK_TEXT_KEY_LANGUAGE);
        uint64_t switched = osal_now_us();
        for (int i = 0; i < 3; i++) {
            uint64_t target = switched + (uint64_t)offsets[i] * 1000;
            if (target > osal_now_us()) osal_task_delay_us(target - osal_now_us());
            pointer_key(english && nine ? '2' : 'a');
            const char *value = english ? ink_text_composing() : ink_text_value();
            CHECK(WAIT_FOR(strlen(value) == (size_t)i + 1, 200), "switch %s %s at %d ms enters text: %.40s", nine ? "nine" : "full", english ? "en-zh" : "zh-en", offsets[i], value);
        }
        CHECK(sim_metric_value("ui.touch_under_overlay_dropped") == dropped, "switch keeps queued pointer input");
        CHECK(sim_metric_value("display.dispatch.FLASH_UI") + sim_metric_value("display.dispatch.FLASH_UI+clean") == clean, "switch does not request a flashing keyboard rebuild");
        CHECK(WAIT_FOR(settled(), 15000), "typing settles");
        on_ui(switch_callbacks);
        CHECK(BY_HAND("system.home", NULL) && WAIT_FOR(!ink_text_focused(), 10000), "leave switch memo");
    }
    ink_receipt_t rc; ink_setting_set("text.nine_key", false, INK_SRC_TOUCH, &rc);
}
static ink_text_cand_t nine_candidates[60];
static char learned_word[40], unlearned_word[40];
static void nine_query(const char *digits) {
    ink_text_cfg_t cfg = {.type = INK_TEXT_PLAIN, .max = 128};
    ink_text_focus(&cfg, NULL, NULL);
    if (ink_text_english()) touch(INK_TEXT_KEY_LANGUAGE);
    for (const char *p = digits; *p; p++) touch(*p);
}
static int nine_find(const char *word) {
    int count = ink_text_candidates(0, nine_candidates, 60);
    for (int i = 0; i < count; i++) if (!strcmp(nine_candidates[i].word, word)) return i;
    return -1;
}
static void nine_owner(void) {
    ink_text_cfg_t cfg = {.type = INK_TEXT_PLAIN}; ink_text_focus(&cfg, NULL, NULL);
    if (ink_text_english()) touch(INK_TEXT_KEY_LANGUAGE);
    touch(KEY_LAYOUT);
    CHECK(ink_text_nine() && K.built_for == 4, "layout toggle selects nine keys for this member");
    nine_query("94664");
    int china = nine_find("中国");
    snprintf(unlearned_word, sizeof unlearned_word, "%s", nine_candidates[0].word);
    CHECK(china >= 0 && nine_candidates[china].consumed == 5, "94664 offers the phrase 中国");
    CHECK(ink_ui_context_snapshot(snapshot, sizeof snapshot, NULL) > 0 && strstr(snapshot, "94664") && strstr(snapshot, "九宫格"), "nine-key composition and layout are visible to AI");
    nine_query("946644862"); china = nine_find("中国");
    CHECK(china >= 0 && nine_candidates[china].consumed == 8, "phrase selection consumes its full numeric prefix");
    if (china >= 0) {
        ink_receipt_t rc; char args[48]; snprintf(args, sizeof args, "{\"index\":%d}", china);
        CHECK(ink_cap_invoke_json("keyboard.pick", args, INK_SRC_TOUCH, ink_ui_content_version(), &rc) == INK_RC_DONE, "nine-key selection uses keyboard.pick");
        CHECK(!strcmp(ink_text_value(), "中国") && !strcmp(ink_text_composing(), "2"), "picking a phrase preserves remaining digits");
    }
    nine_query("94664"); nine_find("中国");
    int target = 1;
    while (target < 60 && !strcmp(nine_candidates[target].word, unlearned_word)) target++;
    CHECK(target < 60, "ambiguous nine-key query has another candidate");
    if (target < 60) {
        snprintf(learned_word, sizeof learned_word, "%s", nine_candidates[target].word);
        ink_text_pick(target); nine_query("94664"); nine_find("中国");
        CHECK(!strcmp(nine_candidates[0].word, learned_word), "nine-key selection retains member learning");
    }
    ink_text_blur(0);
}
static void nine_child(void) {
    CHECK(!ink_text_nine(), "new member retains their own full-key layout");
    ink_text_cfg_t cfg = {.type = INK_TEXT_PLAIN}; ink_text_focus(&cfg, NULL, NULL);
    touch(KEY_LAYOUT); nine_query("94664"); nine_find("中国");
    CHECK(!strcmp(nine_candidates[0].word, unlearned_word), "owner learning is absent from the child's candidates");
    touch(KEY_LAYOUT); ink_text_blur(0);
}
static void nine_return(void) {
    CHECK(ink_text_nine(), "returning member retains nine-key preference");
    nine_query("94664"); nine_find("中国");
    CHECK(!strcmp(nine_candidates[0].word, learned_word), "returning member retains their learned candidates");
    touch(KEY_LAYOUT); ink_text_blur(0);
}
static void benchmark(void *arg) {
    (void)arg;
    py_cand_t candidates[60];
    for (int length = 1; length <= 6; length++) {
        char digits[8], letters[8];
        snprintf(digits, sizeof digits, "%.*s", length, "946644");
        snprintf(letters, sizeof letters, "%.*s", length, "zhongg");
        double times[2] = {0}, maximum[2] = {0};
        for (int mode = 0; mode < 2; mode++) for (int repeat = 0; repeat < 100; repeat++) {
            struct timespec start, end;
            if (clock_gettime(CLOCK_MONOTONIC, &start)) abort();
            if (mode) py_lookup_nine(digits, candidates, 60);
            else py_lookup(letters, candidates, 60);
            if (clock_gettime(CLOCK_MONOTONIC, &end)) abort();
            double us = (end.tv_sec - start.tv_sec) * 1e6 + (end.tv_nsec - start.tv_nsec) / 1e3;
            times[mode] += us;
            if (us > maximum[mode]) maximum[mode] = us;
        }
        printf("NINE_BENCH %d %.3f %.3f %.3f %.3f\n", length, times[0] / 100, maximum[0], times[1] / 100, maximum[1]);
    }
}
static void chinese_scene(void) {
    if (ink_text_english()) ink_text_key(INK_TEXT_KEY_LANGUAGE);
    ink_text_commit_utf8("明天带作业本，");
    ink_text_key('n'); ink_text_key('i');
}
static void end_scene(void) { ink_text_blur(0); }
static void wifi_environment(void *arg) { (void)arg; sim_wifi_ap_add("SYNTH_POLISH_WIFI", -42, "SYNTHETIC_PASSWORD"); }
static void read_wifi_page(void) { CHECK(ink_ui_snapshot(snapshot, sizeof snapshot, NULL) > 0, "read real Wi-Fi page"); }
static bool wifi_scanned(void) { on_ui(read_wifi_page); return strstr(snapshot, "SYNTH_POLISH_WIFI") != NULL; }
static lv_obj_t *scan_button;
static void find_scan(lv_obj_t *obj) {
    const char *cap = ink_shell_bound_cap(obj);
    if (cap && !strcmp(cap, "wifi.scan")) scan_button = obj;
    for (uint32_t i = 0; i < lv_obj_get_child_count(obj); i++) find_scan(lv_obj_get_child(obj, i));
}
static void scan_wifi(void) {
    scan_button = NULL; find_scan(lv_screen_active());
    if (scan_button) lv_obj_send_event(scan_button, LV_EVENT_CLICKED, NULL);
}
static int password_fields;
static void inspect_password_field(lv_obj_t *obj) {
    if (!lv_obj_is_visible(obj)) return;
    if (lv_obj_check_type(obj, &lv_textarea_class) && lv_textarea_get_password_mode(obj)) {
        lv_area_t field, keyboard; ink_shell_keyboard_area(&keyboard); lv_obj_update_layout(obj); lv_obj_get_coords(lv_obj_get_parent(obj), &field);
        CHECK(field.y1 >= 0 && field.y2 < keyboard.y1 && field.x1 >= 0 && field.x2 <= keyboard.x2, "real Wi-Fi password label and input fit above the compact keyboard");
        password_fields++;
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(obj); i++) inspect_password_field(lv_obj_get_child(obj, i));
}
static void wifi_layout(void) {
    password_fields = 0; inspect_password_field(lv_screen_active());
    CHECK(password_fields == 1 && ink_text_type() == INK_TEXT_PASSWORD, "real Wi-Fi page focuses exactly one password field");
    CHECK(!lv_obj_is_visible(K.cands) && !lv_obj_is_visible(K.composing), "real password page has no candidate or composing band");
}
static bool verify_layout, switch_only;
static void host_mode(void *arg) { (void)arg; verify_layout = getenv("INK_IME_LAYOUT_VERIFY") != NULL; switch_only = getenv("INK_IME_SWITCH_ONLY") != NULL; }
static void remember_layout(void) {
    ink_receipt_t rc;
    CHECK(ink_setting_set("text.nine_key", 1, INK_SRC_TOUCH, &rc) == INK_RC_DONE, "save owner layout for restart");
}
static int screenshot_kind;
static void screenshot_scene(void) {
    K.symbols = false;
    ink_receipt_t rc;
    ink_setting_set("text.nine_key", screenshot_kind == 4, INK_SRC_TOUCH, &rc);
    ink_text_cfg_t cfg = {.type = screenshot_kind == 3 ? INK_TEXT_NUMBER : screenshot_kind == 5 ? INK_TEXT_PASSWORD : INK_TEXT_PLAIN,
                          .initial = screenshot_kind == 5 ? "SYNTHETIC_SECRET" : "", .hint = "合成输入"};
    ink_text_focus(&cfg, NULL, NULL);
    if (ink_text_english() != (screenshot_kind == 1)) ink_text_key(INK_TEXT_KEY_LANGUAGE);
    if (screenshot_kind == 2) ai_key(KEY_SWITCH);
    if (screenshot_kind == 0) chinese_scene();
    if (screenshot_kind == 4) for (const char *p = "94664"; *p; p++) ink_text_key(*p);
}
static void run(void *arg) {
    (void)arg;
    CHECK(WAIT_FOR(ink_member_space_open() && settled(), 15000), "boot settles");
    test_start_home(); ink_power_hold(INK_NEED_AWAKE);
    if (verify_layout) {
        CHECK(ink_text_nine(), "owner layout survives simulator restart");
        CHECK(ink_member_switch("kid") == 0 && WAIT_FOR(!ink_text_focused(), 5000), "verify child after restart");
        CHECK(!ink_text_nine(), "child layout survives independently across restart");
        CHECK(ink_member_switch("owner") == 0 && WAIT_FOR(!ink_text_focused(), 5000), "verify owner after restart");
        CHECK(ink_text_nine(), "owner layout is restored after member handoff");
        ink_probe_count("test.finished");
        return;
    }
    if (switch_only) { switch_keys(); ink_probe_count("test.finished"); return; }
    on_ui(functional);
    on_ui(language_over_ai_panel);
    switch_keys();
    vtk_event_at(vtk_now() + SIM_US(1), wifi_environment, NULL);
    py_want_dict(NULL);
    CHECK(WAIT_FOR(py_dict_loaded(), 5000), "shipping IKD1 loads before nine-key acceptance");
    on_ui(nine_owner);
    CHECK(ink_member_switch("kid") == 0 && WAIT_FOR(!ink_text_focused(), 5000), "switch to child"); on_ui(nine_child);
    CHECK(ink_member_switch("owner") == 0 && WAIT_FOR(!ink_text_focused(), 5000), "switch back to owner"); on_ui(nine_return);
    ink_svc_lock(); vtk_host_call(benchmark, NULL); ink_svc_unlock();
    for (int language = 0; language < 2; language++) {
        CHECK(BY_HAND("settings.set", language ? "{\"id\":\"system.language\",\"value\":1}" : "{\"id\":\"system.language\",\"value\":0}") && g_rc.status == INK_RC_DONE, "set interface language");
        CHECK(BY_HAND("memo.edit", NULL) && g_rc.status == INK_RC_DONE && WAIT_FOR(ink_text_focused(), 10000), "open real memo editor");
        on_ui(chinese_scene); capture(language ? "ImePinyin-en" : "ImePinyin-zh");
        CHECK(AI("keyboard.key", "{\"code\":3}") && g_rc.status == INK_RC_DONE, "AI changes the rendered keyboard");
        capture(language ? "ImeEnglish-en" : "ImeEnglish-zh");
        CHECK(BY_HAND("keyboard.key", "{\"code\":1}") && g_rc.status == INK_RC_DONE, "open symbol page");
        capture(language ? "ImeSymbols-en" : "ImeSymbols-zh");
        CHECK(BY_HAND("system.home", NULL) && g_rc.status == INK_RC_DONE && WAIT_FOR(!ink_text_focused(), 10000), "close editor");
    }
    CHECK(BY_HAND("settings.set", "{\"id\":\"system.language\",\"value\":0}") && g_rc.status == INK_RC_DONE, "use Chinese for acceptance screenshots");
    for (int large = 0; large < 2; large++) {
        CHECK(BY_HAND("settings.set", large ? "{\"id\":\"display.font_size\",\"value\":3}" : "{\"id\":\"display.font_size\",\"value\":1}") && g_rc.status == INK_RC_DONE, "set screenshot font size");
        for (int kind = 0; kind < 6; kind++) {
            static const char *names[] = {"Pinyin", "English", "Symbols", "Number", "Nine", "Password"};
            CHECK(BY_HAND("memo.edit", NULL) && WAIT_FOR(ink_text_focused(), 10000), "open screenshot host");
            screenshot_kind = kind; on_ui(screenshot_scene);
            char name[64]; snprintf(name, sizeof name, "%s-%s", names[kind], large ? "large" : "normal");
            capture(name);
            on_ui(end_scene);
            CHECK(BY_HAND("system.home", NULL) && WAIT_FOR(!ink_text_focused(), 10000), "close screenshot host");
        }
        CHECK(BY_HAND("shell.network", NULL), "open real network settings");
        for (int i = 0; i < 4; i++) {
            on_ui(scan_wifi);
            if (scan_button) break;
            CHECK(BY_HAND("shell.page_next", NULL), "find network scan control");
        }
        CHECK(WAIT_FOR(wifi_scanned(), 10000), "synthetic Wi-Fi appears in the real scan results");
        CHECK(BY_HAND("shell.wifi_pick", "{\"index\":0}") && WAIT_FOR(ink_text_focused(), 10000), "open real Wi-Fi password field");
        capture(large ? "WifiPassword-large" : "WifiPassword-normal"); on_ui(wifi_layout);
        CHECK(BY_HAND("system.home", NULL) && WAIT_FOR(!ink_text_focused(), 10000), "leave real password page");
    }
    double saved = sim_metric_value("settings.saved");
    on_ui(remember_layout); ink_db_flush_all();
    CHECK(WAIT_FOR(sim_metric_value("settings.saved") > saved && !ink_storage_pending() && ink_db_idle(), 10000), "layout and learning are durable before restart");
    ink_probe_count("test.finished");
}
void ink_main(void) {
    vtk_host_call(host_mode, NULL);
    product_main();
    osal_task_cfg_t cfg = {.name = "ime-test", .fn = run, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start keyboard test driver");
}
