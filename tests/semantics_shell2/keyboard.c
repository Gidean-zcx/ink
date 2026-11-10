#include "common.h"
#ifndef INK_TEST_SHELL_SOURCE
#define INK_TEST_SHELL_SOURCE "../../system/ui/shell/keyboard.c"
#endif
#define ink_ui_semantics_summary inspect_compact_model
#define key shell2_source_key
#include INK_TEST_SHELL_SOURCE
#undef key
#undef ink_ui_semantics_summary

void ink_test_shell2_keyboard(void) {
    ink_text_cfg_t cfg = {.type = INK_TEXT_PLAIN, .max = 128, .initial = "合成文字"};
    CHECK(ink_text_focus(&cfg, NULL, NULL) > 0, "focus the real input service");
    ink_text_key('x'); ink_text_key('i'); ink_text_key('a'); ink_text_key('n');
    view("Keyboard");
    CHECK(strstr(test_snapshot, "composing") && strstr(test_snapshot, "xian") && strstr(test_semantic, "candidate:") && strstr(test_semantic, "模式="), "pinyin, candidates and mode remain available");
    lv_obj_t *b = find_button(K.root, "删除");
    event(b, LV_EVENT_PRESSED);
    stale("home", "keyboard.key");
    event(b, LV_EVENT_CLICKED);
    CHECK(!strcmp(ink_text_composing(), "xian"), "stale physical key does not edit the input");
    ink_receipt_t receipt;
    CHECK(ink_cap_invoke_json("keyboard.key", "{\"code\":8}", INK_SRC_AI, version, &receipt) == INK_RC_STALE,
          "the actual keyboard capability rejects an old AI page version");
    event(b, LV_EVENT_PRESSED); event(b, LV_EVENT_CLICKED);
    CHECK(!strcmp(ink_text_composing(), "xia"), "fresh physical key still works");
    ink_text_blur(0);
    cfg.type = INK_TEXT_PASSWORD; cfg.initial = "";
    CHECK(ink_text_focus(&cfg, NULL, NULL) > 0, "focus a real secret input");
    view("Keyboard"); CHECK(strstr(test_semantic, "无值"), "empty password exports absence");
    ink_text_key('1'); view("Keyboard");
    static char one[sizeof test_semantic]; snprintf(one, sizeof one, "%s", test_semantic);
    ink_text_key('2'); ink_text_key('3'); ink_text_key('4'); view("Keyboard");
    CHECK(!strcmp(one, test_semantic) && strstr(test_semantic, "有值") && strstr(test_semantic, "\"actions\":[]") && !strstr(test_semantic, "selection"), "password exposes presence only, without length or actions");
    ink_text_blur(0);
    cfg.type = INK_TEXT_PHONE; cfg.ai_hidden = true; cfg.initial = "FAKE_SHELL2_SECRET";
    CHECK(ink_text_focus(&cfg, NULL, NULL) > 0, "focus hidden call keypad input");
    view("Keyboard"); CHECK(strstr(test_semantic, "有值") && strstr(test_semantic, "\"actions\":[]"), "hidden call input exposes presence only");
    ink_text_blur(0);
    ink_test_shell2_native_output = true;
    cfg.type = INK_TEXT_PLAIN; cfg.ai_hidden = false; cfg.initial = "合成文字";
    CHECK(ink_text_focus(&cfg, NULL, NULL) > 0, "focus production compound input");
    ink_text_key('x'); ink_text_key('i'); ink_text_key('a'); ink_text_key('n');
    static char actual[3072];
    CHECK(ink_ui_context_snapshot(actual, sizeof actual, NULL) > 0 && strstr(actual, "\"keyboard\":{") &&
          strstr(actual, "\"keyboard_ui\":\"") && strstr(actual, "候选[0]=") && strstr(actual, "composing") && strstr(actual, "模式="),
          "unchanged compound contract fits the actual 3072-byte AI buffer");
    ink_text_blur(0); ink_test_shell2_native_output = false;
}
