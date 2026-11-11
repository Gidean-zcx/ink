/* Real input service, keyboard controls, capabilities and rendered frames. */
#include <stdlib.h>
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

static lv_obj_t *button(lv_obj_t *root, int code) {
    for (uint32_t i = 0; i < lv_obj_get_event_count(root); i++) {
        lv_event_dsc_t *e = lv_obj_get_event_dsc(root, i);
        if (lv_event_dsc_get_cb(e) == on_key && (int)(intptr_t)lv_event_dsc_get_user_data(e) == code) return root;
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); i++) {
        lv_obj_t *found = button(lv_obj_get_child(root, (int32_t)i), code);
        if (found) return found;
    }
    return NULL;
}
static bool labeled(int code, const char *text) {
    lv_obj_t *b = K.root ? button(K.root, code) : NULL;
    if (!b) return false;
    for (uint32_t i = 0; i < lv_obj_get_child_count(b); i++) {
        lv_obj_t *l = lv_obj_get_child(b, (int32_t)i);
        if (lv_obj_check_type(l, &lv_label_class) && !strcmp(lv_label_get_text(l), text)) return true;
    }
    return false;
}
static void touch(int code) {
    lv_obj_t *b = K.root ? button(K.root, code) : NULL;
    CHECK(b && lv_obj_is_visible(b), "visible keyboard control: %d", code);
    if (b) { lv_obj_send_event(b, LV_EVENT_PRESSED, NULL); lv_obj_send_event(b, LV_EVENT_CLICKED, NULL); }
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
    lv_obj_t *b = button(K.root, code);
    bool image = false, hidden_label = false;
    if (b) for (uint32_t i = 0; i < lv_obj_get_child_count(b); i++) {
        lv_obj_t *c = lv_obj_get_child(b, (int32_t)i);
        image |= lv_obj_check_type(c, &lv_image_class) && lv_obj_is_visible(c);
        hidden_label |= lv_obj_check_type(c, &lv_label_class) && lv_obj_has_flag(c, LV_OBJ_FLAG_HIDDEN) && !strcmp(lv_label_get_text(c), label);
    }
    CHECK(image && hidden_label, "icon replaces visual text and retains accessibility label: %d", code);
}
static void functional(void) {
    ink_text_cfg_t cfg = {.type = INK_TEXT_PLAIN, .max = 128};
    CHECK(!ink_text_english(), "a fresh process starts in Chinese");
    CHECK(ink_text_focus(&cfg, NULL, NULL) > 0, "focus plain input");
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
            CHECK(ai_key(INK_TEXT_KEY_LANGUAGE) == INK_RC_REJECTED, "AI cannot operate a password field");
            touch('n'); touch('i');
            CHECK(!strcmp(ink_text_value(), "ni") && !ink_text_composing()[0], "password letters stay literal");
            CHECK(ink_ui_context_snapshot(snapshot, sizeof snapshot, NULL) > 0 && !strstr(snapshot, "\"ni\""), "password value stays masked");
        } else if (cfg.type == INK_TEXT_URL) {
            touch('n'); touch('i'); touch(KEY_SWITCH); touch('.');
            CHECK(!strcmp(ink_text_value(), "ni.") && !ink_text_composing()[0], "URL keeps literal letters and punctuation");
        } else {
            touch(cfg.type == INK_TEXT_PHONE ? '+' : '.'); touch('1');
            CHECK(!strcmp(ink_text_value(), cfg.type == INK_TEXT_PHONE ? "+1" : ".1"), "numeric/phone behavior unchanged");
        }
    }
    cfg.type = INK_TEXT_PLAIN;
    CHECK(ink_text_focus(&cfg, NULL, NULL) > 0, "return to plain input");
    expect_mode(true);
    ink_text_blur(0);
}

static bool contains(const lv_area_t *outer, const lv_area_t *inner) {
    return inner->x1 >= outer->x1 && inner->y1 >= outer->y1 && inner->x2 <= outer->x2 && inner->y2 <= outer->y2;
}
static void inspect(void) {
    CHECK(ink_ui_context_snapshot(snapshot, sizeof snapshot, NULL) > 0, "capture real semantic snapshot");
    lv_obj_update_layout(K.root);
    lv_area_t root; lv_obj_get_coords(K.root, &root);
    for (uint32_t i = 0; i < lv_obj_get_child_count(K.keys); i++) {
        lv_obj_t *row = lv_obj_get_child(K.keys, (int32_t)i);
        if (row == K.edit || row == K.cands || row == K.composing) continue;
        lv_area_t previous = {0};
        for (uint32_t j = 0; j < lv_obj_get_child_count(row); j++) {
            lv_obj_t *b = lv_obj_get_child(row, (int32_t)j);
            lv_area_t bounds; lv_obj_get_coords(b, &bounds);
            CHECK(contains(&root, &bounds) && (!j || bounds.x1 > previous.x2), "keys fit without overlap: %s", picture);
            previous = bounds;
            for (uint32_t k = 0; k < lv_obj_get_child_count(b); k++) {
                lv_obj_t *child = lv_obj_get_child(b, (int32_t)k);
                if (!lv_obj_is_visible(child)) continue;
                lv_area_t a; lv_obj_get_coords(child, &a);
                CHECK(contains(&bounds, &a), "key label/icon fits: %s", picture);
                if (lv_obj_check_type(child, &lv_image_class))
                    CHECK(lv_obj_get_width(child) == ink_ui_px(INK_ICON_MEDIUM), "icon follows ui-scale size");
            }
        }
    }
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
static void chinese_scene(void) {
    if (ink_text_english()) ink_text_key(INK_TEXT_KEY_LANGUAGE);
    ink_text_commit_utf8("明天带作业本，");
    ink_text_key('n'); ink_text_key('i');
}
static void run(void *arg) {
    (void)arg;
    CHECK(WAIT_FOR(ink_member_space_open() && settled(), 15000), "boot settles");
    test_start_home(); ink_power_hold(INK_NEED_AWAKE);
    on_ui(functional);
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
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name = "ime-test", .fn = run, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start keyboard test driver");
}
