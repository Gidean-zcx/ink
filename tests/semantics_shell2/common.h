#ifndef INK_TEST_SHELL2_H
#define INK_TEST_SHELL2_H
#include "ink/sys/ui_semantics.h"
#include "ink/ui_shell.h"
#include "../integration/whole_product.h"
extern bool ink_test_shell2_native_output;
static char test_snapshot[16384], test_semantic[12288], object_id[64];
static uint32_t test_page_version, test_object_revision;
static void view(const char *name) {
    const char *top = NULL;
    CHECK(ink_ui_snapshot(test_snapshot, sizeof test_snapshot, &top) > 0 && top && !strcmp(top, name), "real %s test_snapshot exports", name);
    ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL);
    if (j) ink_json_parse(j, test_snapshot, strlen(test_snapshot));
    if (!j) return;
    int keyboard = ink_json_find(j, 0, "keyboard_ui");
    if (keyboard >= 0) {
        CHECK(ink_json_str(j, keyboard, test_semantic, sizeof test_semantic) > 0,
              "keyboard_ui remains a string containing F1 semantics");
    } else {
        size_t len = strlen(test_snapshot);
        CHECK(len < sizeof test_semantic, "test model fits");
        if (len < sizeof test_semantic) memcpy(test_semantic, test_snapshot, len + 1);
        else test_semantic[0] = 0;
    }
    unsigned pv = 0, rev = 0;
    const char *v = strstr(test_semantic, "\"page_version\":");
    const char *o = strstr(test_semantic, "\"object\":{\"id\":\"");
    const char *r = o ? strstr(o, "\"revision\":") : NULL;
    CHECK(strstr(test_semantic, "\"fields\":[") && strstr(test_semantic, "\"actions\":[") && v && r &&
          sscanf(v, "\"page_version\":%u", &pv) == 1 && pv == ink_ui_content_version() &&
          sscanf(r, "\"revision\":%u", &rev) == 1 &&
          sscanf(o, "\"object\":{\"id\":\"%63[^\"]", object_id) == 1,
          "%s uses the semantic JSON contract", name);
    test_page_version = pv; test_object_revision = rev;
    osal_free(j);
    CHECK(!strstr(test_snapshot, "FAKE_SHELL2_SECRET"), "%s exports no secret text", name);
}
static int inspect_compact_model(const ink_ui_page_spec_t *page, char *out, size_t cap) {
    static char compact[3072];
    CHECK(ink_ui_semantics_summary(page, compact, sizeof compact) > 0 && !strstr(compact, "FAKE_SHELL2_SECRET"), "production compact model fits and masks secrets");
    return ink_test_shell2_native_output ? ink_ui_semantics_summary(page, out, cap) : ink_ui_semantics_snapshot(page, out, cap);
}
static void stale(const char *page, const char *cap) {
    ink_ui_action_spec_t action = {.id = cap, .label = "合成动作", .capability = cap, .enabled = true};
    ink_ui_page_spec_t model = {.id = page, .object = {object_id, test_object_revision}, .actions = &action, .action_count = 1};
    ink_ui_action_target_t target = {test_page_version, model.object};
    ink_ui_content_changed();
    ink_receipt_t rc;
    CHECK(ink_ui_semantics_action(&model, cap, &target, INK_SRC_AI, &rc) == INK_RC_STALE &&
          ink_ui_semantics_action(&model, cap, &target, INK_SRC_TOUCH, &rc) == INK_RC_STALE,
          "stale AI and touch targets are rejected on %s", page);
}
static lv_obj_t *find_button(lv_obj_t *root, const char *label) {
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); i++) {
        lv_obj_t *child = lv_obj_get_child(root, (int)i);
        if (lv_obj_check_type(child, &lv_label_class) && !strcmp(lv_label_get_text(child), label)) return lv_obj_get_parent(child);
        lv_obj_t *found = find_button(child, label);
        if (found) return found;
    }
    return NULL;
}
static void event(lv_obj_t *b, lv_event_code_t code) { CHECK(b, "real control exists"); if (b) lv_obj_send_event(b, code, NULL); }
#endif
