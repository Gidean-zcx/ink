/* F1 uses a real C activity and the production snapshot and capability paths. */
#include "ink/sys/ui_semantics.h"
#include "ink/ui_shell.h"
#define ink_main product_main
#include "../system/boot/main.c"
#undef ink_main
#include "integration/whole_product.h"

static ink_ui_field_spec_t fields[] = {
    {.id = "body", .role = "textbox", .label = "备忘", .value = "合成\"备忘\"\n第二行", .object = {"example:note-a", 7}},
    {.id = "password", .role = "textbox", .label = "密码", .value = "FAKE_PASSWORD_123", .object = {"example:secret-password", 99}, .secret = true},
    {.id = "code", .role = "textbox", .label = "验证码", .value = "", .object = {"example:secret-code", 88}, .secret = true}
};
static const ink_ui_action_spec_t actions[] = {
    {.id = "save", .label = "保存", .capability = "test.semantic_save", .args_json = "{\"id\":\"example:note-a\",\"revision\":7}", .enabled = true},
    {.id = "disabled", .label = "不可用", .capability = "test.semantic_save"},
    {.id = "missing", .label = "不存在", .capability = "test.semantic_missing", .enabled = true}
};
static const ink_ui_page_spec_t model = {
    .id = "test.semantics", .title = "合成备忘", .object = {"example:note-a", 7},
    .fields = fields, .field_count = sizeof fields / sizeof *fields,
    .actions = actions, .action_count = sizeof actions / sizeof *actions
};
static int effects;
static ink_source_t last_source;
static ink_ui_action_target_t target, drawn_target;
static char target_id[64], snapshot[3072], previous[3072];
static lv_obj_t *button;

static void save(ink_call_t *call) {
    char id[64];
    int64_t revision = 0;
    CHECK(ink_args_get_str(&call->args, "id", id, sizeof id) > 0 && !strcmp(id, model.object.id) &&
          ink_args_get_int(&call->args, "revision", &revision) && revision == model.object.revision, "the binding supplies the declared object and revision");
    effects++;
    last_source = call->src;
    snprintf(call->rc.object, sizeof call->rc.object, "%s", model.object.id);
    call->rc.revision = model.object.revision;
}
/* A global capability makes the F1 version check decisive, independently of the bus's activity gate. */
static const ink_cap_t CAPS[] = {
    {.id = "test.semantic_save", .title = "保存合成备忘", .child_ok = true, .effect = INK_FX_LOCAL_WRITE,
     .input = "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\"},\"revision\":{\"type\":\"integer\"}},\"required\":[\"id\",\"revision\"]}", .run = save},
    {.id = "test.semantic_adult", .title = "合成成人操作", .run = save},
    {.id = "test.semantic_system", .title = "合成系统操作", .effect = INK_FX_SYSTEM, .child_ok = true, .run = save}
};
static void clicked(lv_event_t *event) {
    ink_receipt_t receipt;
    const ink_ui_action_spec_t *action = lv_event_get_user_data(event);
    CHECK(ink_ui_semantics_action(&model, action->id, &drawn_target, INK_SRC_TOUCH, &receipt) == INK_RC_DONE,
          "the themed button invokes its declared capability");
}
static void create(lv_obj_t *screen, void *arg) {
    (void)arg;
    ink_shell_topbar(screen, model.title);
    int y = INK_STATUS_BAR + INK_CONTROL;
    for (size_t i = 0; i < model.field_count; i++) {
        const ink_ui_field_spec_t *f = &model.fields[i];
        ink_ui_list_row(screen, "memo", f->label, f->secret ? "******" : f->value, INK_MARGIN, y,
                        INK_CANVAS_W - 2 * INK_MARGIN, true, NULL, NULL);
        y += ink_ui_row_double() + INK_GAP;
    }
    drawn_target = (ink_ui_action_target_t){ink_ui_content_version(), model.object};
    button = ink_ui_pill(screen, actions[0].label, INK_MARGIN, INK_CANVAS_H - INK_MARGIN - INK_PRIMARY,
                         INK_CANVAS_W - 2 * INK_MARGIN, INK_PRIMARY, true, clicked, (void *)&actions[0]);
}
static int page_snapshot(char *out, size_t cap) { return ink_ui_semantics_snapshot(&model, out, cap); }
static const ink_activity_t PAGE = {.name = "test.semantics", .create = create, .snapshot = page_snapshot};

static bool json_text(const ink_json_t *j, int obj, const char *key, const char *expected) {
    char text[128];
    return ink_json_str(j, ink_json_find(j, obj, key), text, sizeof text) >= 0 && !strcmp(text, expected);
}
static void open_page(void) { ink_ui_push(&PAGE, NULL); }
static void policy_snapshot(void) {
    static const ink_ui_action_spec_t bindings[] = {
        {.id = "owner", .label = "本人确认", .capability = "confirm.accept", .args_json = "{\"token\":42}", .enabled = true},
        {.id = "event", .label = "确认事件", .capability = "confirm.result", .enabled = true},
        {.id = "adult", .label = "成人操作", .capability = "test.semantic_adult", .enabled = true},
        {.id = "system", .label = "系统操作", .capability = "test.semantic_system", .enabled = true},
        {.id = "allowed", .label = "允许操作", .capability = "test.semantic_save", .enabled = true}
    };
    bool adult = ink_member_current()->role == INK_ROLE_ADULT;
    bool expected[] = {false, false, adult, adult, true};
    ink_ui_page_spec_t page = model;
    page.actions = bindings; page.action_count = sizeof bindings / sizeof *bindings;
    char *directory = osal_malloc(16384, OSAL_MEM_EXTERNAL);
    CHECK(directory && ink_cap_ids_json(directory, 16384) > 0 &&
          ink_ui_semantics_snapshot(&page, previous, sizeof previous) > 0, "policy directory and semantic snapshot are complete");
    if (!directory) return;
    for (size_t i = 0; i < page.action_count; i++) {
        CHECK((strstr(directory, bindings[i].capability) != NULL) == expected[i],
              "the AI directory applies member policy to %s", bindings[i].capability);
        CHECK((strstr(previous, bindings[i].capability) != NULL) == expected[i] &&
              (strstr(previous, bindings[i].label) != NULL) == expected[i],
              "the semantic snapshot applies the same policy to %s", bindings[i].capability);
    }
    CHECK(!strstr(previous, "token"), "a hidden owner-only action does not leak its bound arguments");
    ink_receipt_t receipt;
    ink_ui_action_target_t now = {ink_ui_content_version(), model.object};
    int before = effects;
    CHECK(ink_ui_semantics_action(&page, "owner", &now, INK_SRC_AI, &receipt) == INK_RC_INVALID &&
          ink_ui_semantics_action(&page, "event", &now, INK_SRC_AI, &receipt) == INK_RC_INVALID,
          "AI still cannot invoke owner-only actions or events");
    if (!adult) CHECK(ink_ui_semantics_action(&page, "adult", &now, INK_SRC_AI, &receipt) == INK_RC_REJECTED &&
                      ink_ui_semantics_action(&page, "system", &now, INK_SRC_AI, &receipt) == INK_RC_REJECTED,
                      "the child's AI cannot invoke adult or system actions");
    CHECK(effects == before, "hidden actions have no side effects");
    osal_free(directory);
}
static void inspect(void) {
    const char *name = NULL;
    CHECK(ink_ui_snapshot(snapshot, sizeof snapshot, &name) > 0 && name && !strcmp(name, model.id), "the real activity exports the semantic snapshot");
    CHECK(ink_ui_context_snapshot(previous, sizeof previous, NULL) > 0 && !strcmp(snapshot, previous), "AI reads the same page model");
    ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL);
    CHECK(j && ink_json_parse(j, snapshot, strlen(snapshot)) == 0, "the complete snapshot is valid JSON");
    if (!j) return;
    CHECK(json_text(j, 0, "page", model.id) && json_text(j, 0, "title", model.title) &&
          json_text(j, 0, "object.id", model.object.id), "page identity, title and object are exported");
    int64_t revision = 0, version = 0;
    CHECK(ink_json_int(j, ink_json_find(j, 0, "object.revision"), &revision) && revision == 7 &&
          ink_json_int(j, ink_json_find(j, 0, "page_version"), &version) && version == ink_ui_content_version(), "object revision and runtime page version are distinct");
    int field_array = ink_json_find(j, 0, "fields"), action_array = ink_json_find(j, 0, "actions");
    CHECK(field_array >= 0 && j->tok[field_array].size == 3 && action_array >= 0 && j->tok[action_array].size == 1, "all fields and only enabled available actions are exported");
    int field = field_array + 1, action = action_array + 1;
    CHECK(json_text(j, field, "id", fields[0].id) && json_text(j, field, "role", fields[0].role) &&
          json_text(j, field, "label", fields[0].label) && json_text(j, field, "value", fields[0].value) &&
          json_text(j, field, "object.id", fields[0].object.id), "the declared field and escaped value survive the snapshot");
    CHECK(json_text(j, action, "id", actions[0].id) && json_text(j, action, "label", actions[0].label) &&
          json_text(j, action, "capability", actions[0].capability) && json_text(j, action, "args_json", actions[0].args_json), "action identity, label, capability and bound parameters are exported");
    CHECK(ink_json_str(j, ink_json_find(j, action, "object.id"), target_id, sizeof target_id) > 0 &&
          ink_json_int(j, ink_json_find(j, action, "object.revision"), &revision), "the action's target can be read from the snapshot");
    target = (ink_ui_action_target_t){(uint32_t)version, {target_id, (uint32_t)revision}};
    osal_free(j);
    CHECK(strstr(snapshot, "\"id\":\"password\",\"role\":\"textbox\",\"label\":\"密码\",\"value\":\"有值\",\"secret\":true}") &&
          strstr(snapshot, "\"id\":\"code\",\"role\":\"textbox\",\"label\":\"验证码\",\"value\":\"无值\",\"secret\":true}") &&
          !strstr(snapshot, "FAKE_PASSWORD_123") && !strstr(snapshot, "example:secret-"), "password and empty code reveal only presence, with no secret object or value");
    fields[1].value = "FAKE_PASSWORD_MUCH_LONGER_456";
    CHECK(ink_ui_snapshot(previous, sizeof previous, NULL) > 0 && !strcmp(snapshot, previous), "secret length changes do not change the snapshot");
    fields[1].value = NULL; fields[2].value = "FAKE_CODE_987654";
    CHECK(ink_ui_snapshot(previous, sizeof previous, NULL) > 0 && strstr(previous, "\"label\":\"密码\",\"value\":\"无值\"") &&
          strstr(previous, "\"label\":\"验证码\",\"value\":\"有值\"") && !strstr(previous, "FAKE_CODE_987654"), "filled code and absent password also stay private");
    fields[1].value = "FAKE_PASSWORD_123"; fields[2].value = "";
    char small[16] = "filled";
    CHECK(ink_ui_semantics_snapshot(&model, small, sizeof small) < 0 && !small[0], "overflow is an explicit failure with no partial snapshot");
}
static void actions_test(void) {
    ink_receipt_t receipt;
    CHECK(effects == 1 && last_source == INK_SRC_TOUCH, "a real touch used the capability bus once");
    CHECK(ink_ui_semantics_action(&model, "save", &target, INK_SRC_AI, &receipt) == INK_RC_DONE &&
          effects == 2 && last_source == INK_SRC_AI && !strcmp(receipt.object, model.object.id) && receipt.revision == 7, "AI uses the same binding and structured receipt");
    CHECK(ink_ui_semantics_action(&model, "disabled", &target, INK_SRC_AI, &receipt) == INK_RC_INVALID &&
          ink_ui_semantics_action(&model, "unknown", &target, INK_SRC_AI, &receipt) == INK_RC_INVALID && effects == 2, "unavailable and unknown bindings have no effects");
    ink_ui_action_target_t wrong_object = target;
    wrong_object.object.revision++;
    CHECK(ink_ui_semantics_action(&model, "save", &wrong_object, INK_SRC_AI, &receipt) == INK_RC_STALE && effects == 2, "a stale object revision is refused");
    ink_ui_content_changed();
    CHECK(ink_ui_semantics_action(&model, "save", &target, INK_SRC_AI, &receipt) == INK_RC_STALE && effects == 2 &&
          receipt.page_gen == ink_ui_content_version(), "an old page version is refused before even a global capability runs");
    target.page_version = ink_ui_content_version();
    CHECK(ink_ui_semantics_action(&model, "save", &target, INK_SRC_AI, &receipt) == INK_RC_DONE && effects == 3, "a current target succeeds after the version changes");
    ink_ui_home();
    CHECK(ink_ui_semantics_action(&model, "save", &target, INK_SRC_AI, &receipt) == INK_RC_STALE && effects == 3, "a closed page cannot act through its old binding");
}
static void button_position(void) {
    lv_obj_update_layout(button);
    lv_area_t area; lv_obj_get_coords(button, &area);
    g_btn_x[0] = (area.x1 + area.x2) / 2; g_btn_y[0] = (area.y1 + area.y2) / 2;
    CHECK(lv_obj_check_type(button, &lv_button_class), "the page reuses the themed pill button");
}
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void run(void *arg) {
    (void)arg;
    delay_ms(1800);
    CHECK(WAIT_FOR(settled(), 10000), "home settles");
    on_ui(open_page);
    CHECK(WAIT_FOR(settled(), 10000), "the C page settles");
    on_ui(inspect); on_ui(policy_snapshot); on_ui(button_position);
    tap(g_btn_x[0], g_btn_y[0]);
    on_ui(actions_test);
    CHECK(ink_member_switch("kid") == 0, "switch to the real child space for snapshot policy");
    on_ui(open_page);
    CHECK(WAIT_FOR(settled(), 10000), "the child's C page settles");
    on_ui(policy_snapshot);
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    CHECK(ink_cap_register(CAPS, sizeof CAPS / sizeof *CAPS) == 0, "register the synthetic capability");
    osal_task_cfg_t cfg = {.name = "ui-semantics-test", .fn = run, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start the test driver");
}
