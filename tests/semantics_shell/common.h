#ifndef INK_TEST_SEMANTICS_SHELL_H
#define INK_TEST_SEMANTICS_SHELL_H
#include "ink/sys/ui_semantics.h"
#include "ink/ui_shell.h"
#include "../integration/whole_product.h"

extern int ink_shell_test_effects;
static uint32_t saved_version, saved_revision;
static char saved_object[64];
static void semantic_view(const char *name) {
    const char *actual = NULL;
    CHECK(ink_ui_snapshot(g_snap, sizeof g_snap, &actual) > 0 && actual && !strcmp(actual, name), "runtime exports %s", name);
    static char context[sizeof g_snap];
    CHECK(ink_ui_context_snapshot(context, sizeof context, NULL) > 0 && !strcmp(context, g_snap), "AI reads the same semantic model on %s", name);
    ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL);
    int64_t version = 0, revision = 0;
    CHECK(j && ink_json_parse(j, g_snap, strlen(g_snap)) == 0 &&
          ink_json_find(j, 0, "fields") >= 0 && ink_json_find(j, 0, "actions") >= 0 &&
          ink_json_int(j, ink_json_find(j, 0, "page_version"), &version) && version == ink_ui_content_version(),
          "%s uses the semantic JSON contract and current version: %.100s", name, g_snap);
    saved_version = (uint32_t)version;
    CHECK(j && ink_json_int(j, ink_json_find(j, 0, "object.revision"), &revision) &&
          ink_json_str(j, ink_json_find(j, 0, "object.id"), saved_object, sizeof saved_object) > 0,
          "%s exports a versioned object", name);
    saved_revision = (uint32_t)revision;
    osal_free(j);
    CHECK(!strstr(g_snap, "FAKE_SHELL_PASSWORD") && !strstr(g_snap, "1111") && !strstr(g_snap, "text.insert"), "no secret material or AI password action on %s", name);
}
static void stale(const char *page, const char *action) {
    ink_ui_page_spec_t model = {.id = page, .object = {saved_object, saved_revision}};
    ink_ui_action_target_t target = {saved_version, model.object};
    ink_receipt_t rc;
    CHECK(ink_ui_content_version() != saved_version, "the real %s content advanced", page);
    CHECK(ink_ui_semantics_action(&model, action, &target, INK_SRC_TOUCH, &rc) == INK_RC_STALE &&
          ink_ui_semantics_action(&model, action, &target, INK_SRC_AI, &rc) == INK_RC_STALE,
          "old touch and AI targets are refused on %s", page);
}
static lv_obj_t *label_button(lv_obj_t *root, const char *text) {
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); i++) {
        lv_obj_t *child = lv_obj_get_child(root, (int)i);
        if (lv_obj_check_type(child, &lv_label_class) && !strcmp(lv_label_get_text(child), text)) return lv_obj_get_parent(child);
        lv_obj_t *found = label_button(child, text);
        if (found) return found;
    }
    return NULL;
}
static void click(lv_obj_t *root, const char *label) {
    lv_obj_t *button = label_button(root, label);
    CHECK(button != NULL, "real %s control exists", label);
    if (button) lv_obj_send_event(button, LV_EVENT_CLICKED, NULL);
}
#endif
