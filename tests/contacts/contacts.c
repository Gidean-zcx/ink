/* The real application, instrumenting only the result boundary. */
#include "ink/sys/cap.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static ink_source_t selected_source;
static char selected_result[128];
static void observed_result(ink_call_t *c, const char *json) {
    if (!strcmp(c->cap->id, "contacts.select")) {
        selected_source = c->src;
        snprintf(selected_result, sizeof selected_result, "%.*s", (int)sizeof selected_result - 1, json);
    }
    ink_cap_result(c, json);
}
#define ink_cap_result observed_result
#include "../../system/apps/contacts/contacts.c"
#undef ink_cap_result
#define ink_main product_main
#include "../../system/boot/main.c"
#undef ink_main
#include "../integration/whole_product.h"

static char phase[16], snap[4096], case_name[24], call_args[256], contact_name[97], contact_number[25];
static uint32_t first_id, deleted_id;
static const char *wanted_label;
static lv_obj_t *located;
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void quiet(void) { CHECK(WAIT_FOR(settled(), 10000), "the contacts display settles"); }
static void read_phase(void *arg) { (void)arg; snprintf(phase, sizeof phase, "%s", getenv("INK_CONTACTS_PHASE")); }
static void export_snapshot(void *arg) {
    (void)arg;
    char path[768];
    int n = snprintf(path, sizeof path, "%s/snapshots.ndjson", g_sim.out_dir);
    if (n < 0 || (size_t)n >= sizeof path) abort();
    FILE *f = fopen(path, "ab");
    if (!f || fprintf(f, "{\"case\":\"%s\",\"snapshot\":%s}\n", case_name, snap) < 0 || fclose(f)) abort();
}
static void read_page(void) {
    CHECK(ink_ui_snapshot(snap, sizeof snap, NULL) > 0, "read the real page snapshot");
    CHECK(ink_ui_context_snapshot(g_snap, sizeof g_snap, NULL) > 0, "snapshot fits the production AI context buffer");
    vtk_host_call(export_snapshot, NULL);
}
static void page(const char *label) { snprintf(case_name, sizeof case_name, "%s", label); on_ui(read_page); }
static uint32_t result_id(void) {
    ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL);
    int64_t id = 0;
    CHECK(j && ink_json_parse(j, g_result, strlen(g_result)) == 0 && ink_json_int(j, ink_json_find(j, 0, "id"), &id) && id > 0, "result has a stable positive contact id");
    osal_free(j); return (uint32_t)id;
}
static bool put(const char *name, const char *number, uint32_t id) {
    if (id) snprintf(call_args, sizeof call_args, "{\"id\":%u,\"name\":\"%s\",\"number\":\"%s\"}", (unsigned)id, name, number);
    else snprintf(call_args, sizeof call_args, "{\"name\":\"%s\",\"number\":\"%s\"}", name, number);
    return AI("contacts.put", call_args) && g_rc.status == INK_RC_DONE;
}
static void open_list(void) {
    CHECK(BY_HAND("shell.launch", "{\"id\":\"phone\"}") && g_rc.status == INK_RC_DONE, "the existing phone launcher opens the phone");
    CHECK(BY_HAND("contacts.open", NULL) && g_rc.status == INK_RC_DONE, "the contact entry opens contacts");
    CHECK(WAIT_FOR(P.list != NULL, 10000), "background loading produces the real list"); quiet();
}
static void find_label(lv_obj_t *o) {
    if (lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN)) return;
    if (lv_obj_check_type(o, &lv_label_class) && !strcmp(lv_label_get_text(o), wanted_label)) located = o;
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) find_label(lv_obj_get_child(o, i));
}
static void locate_button(void) {
    located = NULL; find_label(lv_screen_active());
    CHECK(located != NULL, "visible contacts control: %s", wanted_label);
    if (!located) return;
    lv_obj_t *button = lv_obj_get_parent(located);
    lv_area_t a; lv_obj_update_layout(button); lv_obj_get_coords(button, &a);
    CHECK(lv_area_get_height(&a) >= ink_ui_px(INK_CONTROL), "the control has a full touch target");
    g_btn_x[0] = (a.x1 + a.x2) / 2; g_btn_y[0] = (a.y1 + a.y2) / 2;
}
static void tap_label(const char *label) {
    wanted_label = label; on_ui(locate_button);
    if (located) tap(g_btn_x[0], g_btn_y[0]);
    quiet();
}
static unsigned field_at;
static void locate_field(void) {
    lv_area_t a; lv_obj_update_layout(P.fields[field_at]); lv_obj_get_coords(P.fields[field_at], &a);
    g_btn_x[0] = (a.x1 + a.x2) / 2; g_btn_y[0] = (a.y1 + a.y2) / 2;
}
static void inspect_short_focus(void) {
    char page_json[3072], field[24];
    ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL);
    bool parsed = j && ink_ui_snapshot(snap, sizeof snap, NULL) > 0 &&
        ink_json_parse(j, snap, strlen(snap)) == 0 &&
        ink_json_str(j, ink_json_find(j, 0, "page"), page_json, sizeof page_json) > 0 &&
        ink_json_parse(j, page_json, strlen(page_json)) == 0;
    int input = parsed ? ink_json_find(j, 0, "input") : -1;
    CHECK(parsed && ink_json_str(j, ink_json_find(j, input, "field"), field, sizeof field) > 0 &&
          !strcmp(field, field_at ? "number" : "name") && ink_json_eq(j, ink_json_find(j, input, "focused"), "true"),
          "AI snapshot identifies the actual focused short field");
    osal_free(j);
}
static void type_field(unsigned index, const char *text) {
    field_at = index; on_ui(locate_field); tap(g_btn_x[0], g_btn_y[0]); quiet();
    CHECK(ink_text_focused(), "physical touch focuses the F2 short input");
    on_ui(inspect_short_focus);
    snprintf(call_args, sizeof call_args, "{\"text\":\"%s\"}", text);
    CHECK(AI("text.insert", call_args) && g_rc.status == INK_RC_DONE, "shared text service edits the field");
    CHECK(AI("text.submit", NULL) && g_rc.status == INK_RC_DONE, "finish the same input session"); quiet();
}
static void inspect_records(void) {
    CHECK(P.db && read_contact(P.db, first_id, contact_name, contact_number) && !strcmp(contact_name, "改过的测试家人") && !strcmp(contact_number, "00000000003"), "updated contact has its stable id and exact number");
    CHECK(ink_db_get(P.db, deleted_id, NULL, NULL, 0) == INK_DB_ERR_NOT_FOUND, "deleted contact is absent");
    CHECK(ink_db_durable_ver(P.db) == ink_db_last_ver(P.db), "success was reported only after persistence");
}
static void capture_frame(void *arg) {
    (void)arg;
    CHECK(settled(), "screenshot is fully presented");
    ink_probe_count("test.snapshot.settled"); ink_probe_count("test.snapshot.PhoneContacts");
    sim_display_snapshot("scene");
}
static void inspect_touch_record(void) {
    ink_db_cursor_t cursor = {0}; ink_rec_t r;
    CHECK(ink_db_next(P.db, &cursor, &r, NULL, 0) == 1 && r.src == INK_SRC_AI, "AI creation reaches the contact store");
    CHECK(ink_db_next(P.db, &cursor, &r, NULL, 0) == 1 && r.src == INK_SRC_TOUCH, "save button reaches the same contact provider with touch provenance");
    deleted_id = r.id;
}
static void switch_to(const char *member) {
    CHECK(ink_member_switch(member) == 0, "switch to synthetic member %s", member);
    quiet(); open_list();
}
static void isolation(void) {
    switch_to("kid"); page("kid-empty");
    CHECK(strstr(snap, "\"total\":0") && !strstr(snap, "测试家人"), "child cannot see the adult contacts");
    snprintf(call_args, sizeof call_args, "{\"id\":%u}", (unsigned)first_id);
    CHECK(AI("contacts.select", call_args) && g_rc.status == INK_RC_INVALID, "adult-only id cannot be selected by child");
    CHECK(!put("越界修改", "00000000009", first_id), "child cannot update absent adult id");
    CHECK(ink_member_allows("contacts.put", true) && put("孩子测试", "00000000009", 0), "child CRUD follows the existing member permission");
    uint32_t child_id = result_id();
    snprintf(call_args, sizeof call_args, "{\"id\":%u}", (unsigned)child_id);
    CHECK(AI("contacts.select", call_args) && g_rc.status == INK_RC_DONE && strstr(g_result, "00000000009"), "same numeric id selects only the child's own number");
    switch_to("owner"); page("owner-return");
    CHECK(strstr(snap, "\"total\":5") && !strstr(snap, "孩子测试"), "adult retains only the adult collection");
    on_ui(inspect_records);
}
static void write_phase(void) {
    open_list(); page("empty");
    CHECK(strstr(snap, "\"total\":0"), "fresh contacts page is empty");
    const char *invalid[] = {"", "+", "12+3", "123a", " 123", "1234567890123456789012345"};
    for (size_t i = 0; i < sizeof invalid / sizeof *invalid; i++) CHECK(!put("错误测试", invalid[i], 0), "reject basic invalid number %zu", i);
    CHECK(!put("", "00000000001", 0), "reject an empty name");
    CHECK(put("测试家人", "+00000000001", 0), "AI adds a contact"); first_id = result_id(); quiet();
    page("one"); CHECK(strstr(snap, "\"total\":1"), "one record is visible without fictional rows");
    CHECK(AI("data.list", "{\"collection\":\"phone.contacts\"}") && g_rc.status == INK_RC_DONE && strstr(g_result, "测试家人"), "AI queries the member collection through the existing read capability");
    CHECK(AI("data.put", "{\"collection\":\"phone.contacts\",\"value\":{\"number\":\"bad\"}}") && g_rc.status == INK_RC_REJECTED, "generic writes cannot bypass contact validation");
    tap_label("添加联系人");
    type_field(0, "触摸测试"); type_field(1, "00000000002");
    page("form"); CHECK(strstr(snap, "contacts.put") && strstr(snap, "触摸测试"), "AI sees live form values and the save capability");
    tap_label("保存联系人");
    CHECK(!strcmp(ink_ui_page_name(), "PhoneContacts"), "saving returns to contacts"); on_ui(inspect_touch_record);
    CHECK(put("改过的测试家人", "00000000003", first_id), "AI updates by id"); quiet();
    CHECK(!put("不存在测试", "00000000004", 999), "updating a missing id does not create a contact");
    snprintf(call_args, sizeof call_args, "{\"id\":%u}", (unsigned)first_id);
    CHECK(AI("contacts.edit", call_args) && g_rc.status == INK_RC_DONE, "open the selected contact"); quiet();
    tap_label("选择这个号码");
    char touched[128]; snprintf(touched, sizeof touched, "%s", selected_result);
    CHECK(selected_source == INK_SRC_TOUCH && strstr(touched, "00000000003"), "the number selection button returns the stored number");
    CHECK(AI("contacts.select", call_args) && g_rc.status == INK_RC_DONE && selected_source == INK_SRC_AI && !strcmp(touched, g_result), "AI and physical touch use the same selection provider and result");
    CHECK(result_id() == first_id, "selection returns the stable contact id");
    CHECK(BY_HAND("shell.back", NULL), "return from the editor"); quiet();
    snprintf(call_args, sizeof call_args, "{\"id\":%u}", (unsigned)deleted_id);
    CHECK(AI("contacts.edit", call_args) && g_rc.status == INK_RC_DONE, "open the synthetic contact to delete"); quiet();
    tap_label("删除联系人");
    ink_confirm_view_t pending[1];
    CHECK(ink_confirm_list(pending, 1) == 1, "irrecoverable deletion asks for device confirmation");
    CHECK(ink_confirm_decide(pending[0].token, true, INK_SRC_TOUCH) == 0, "human confirms deletion"); quiet();
    CHECK(WAIT_FOR(!strcmp(ink_ui_page_name(), "PhoneContacts"), 10000), "confirmed deletion returns to the list");
    for (int i = 0; i < 3; i++) CHECK(put("合成联系人", "00000000004", 0), "add fixture %d for a full page", i);
    quiet();
    page("four"); CHECK(strstr(snap, "\"pages\":1,\"total\":4"), "four contacts fit one complete page");
    CHECK(put("末页测试", "00000000005", 0), "add a second-page fixture"); quiet(); page("multi");
    vtk_event_at(vtk_now() + SIM_MS(1), capture_frame, NULL); delay_ms(5);
    key(INK_KEY_NEXT); quiet(); page("last");
    CHECK(strstr(snap, "\"page\":2,\"pages\":2,\"total\":5,\"offset\":4,\"visible\":1"), "physical paging exposes only the final row");
    CHECK(AI("contacts.prev", NULL) && g_rc.status == INK_RC_DONE, "AI paging uses the registered page operation"); quiet();
    on_ui(inspect_records); isolation();
}
static void verify_phase(void) {
    first_id = 1; deleted_id = 2; open_list(); page("reboot");
    CHECK(strstr(snap, "\"total\":5") && strstr(snap, "改过的测试家人"), "contacts survive a new simulator process");
    on_ui(inspect_records);
    switch_to("kid"); page("kid-reboot");
    CHECK(strstr(snap, "\"total\":1") && strstr(snap, "孩子测试") && !strstr(snap, "测试家人"), "child's isolated contacts survive reboot too");
}
static uint32_t editor_revision, confirmation_token;
static char saved_args[768], confirmed_status[24];
static volatile bool confirmation_finished;
static void confirmed(void *ctx, const char *id, const ink_args_t *args) {
    (void)ctx; (void)id;
    int64_t token = 0;
    if (ink_args_get_int(args, "token", &token) && (uint32_t)token == confirmation_token) {
        ink_args_get_str(args, "status", confirmed_status, sizeof confirmed_status);
        confirmation_finished = true;
    }
}
static void inspect_editor(void) {
    ink_rec_t record;
    CHECK(ink_db_get(P.db, first_id, &record, NULL, 0) == INK_DB_OK, "editor fixture exists");
    editor_revision = record.ver;
    CHECK(P.form.object.revision == editor_revision, "editor snapshot targets the loaded record revision");
    CHECK(ink_ui_context_snapshot(g_snap, sizeof g_snap, NULL) > 0, "versioned editor fits the production AI context");
    snprintf(saved_args, sizeof saved_args, "%s", P.save_args);
}
static void inspect_stale_editor(void) {
    CHECK(!strcmp(ink_ui_page_name(), "ContactEdit") && P.fields[0] &&
          strstr(ink_textfield_text(P.fields[0]), "草稿"), "stale save retains the editor and unsaved draft");
    CHECK(P.notice && strstr(lv_label_get_text(P.notice), "重新打开"), "stale save explains how to reload");
}
static void answer(const ink_confirm_view_t *pending, const char *status) {
    confirmation_token = pending->token; confirmation_finished = false;
    CHECK(ink_confirm_decide(pending->token, true, INK_SRC_TOUCH) == 0, "human accepts the held deletion");
    CHECK(WAIT_FOR(confirmation_finished, 10000) && !strcmp(confirmed_status, status), "deletion receipt is %s", status);
    quiet();
}
static void review_phase(void) {
    open_list();
    CHECK(put("复查联系人", "00000000001", 0), "create the review fixture"); first_id = result_id(); quiet();
    snprintf(call_args, sizeof call_args, "{\"id\":%u}", (unsigned)first_id);
    CHECK(AI("contacts.edit", call_args) && g_rc.status == INK_RC_DONE, "open the review editor"); quiet();
    on_ui(inspect_editor);
    if (!strcmp(phase, "stale")) {
        CHECK(AI("contacts.name", NULL) && g_rc.status == INK_RC_DONE, "focus the existing name field"); quiet();
        CHECK(AI("text.insert", "{\"text\":\"草稿\"}") && g_rc.status == INK_RC_DONE, "add an unsaved draft through the shared text service");
        CHECK(AI("text.submit", NULL) && g_rc.status == INK_RC_DONE, "finish editing the draft"); quiet(); on_ui(inspect_editor);
        CHECK(put("AI修改后的联系人", "00000000002", first_id), "AI updates while a draft is open"); quiet();
        tap_label("保存联系人");
        CHECK(read_contact(P.db, first_id, contact_name, contact_number) && !strcmp(contact_number, "00000000002"), "stale touch save preserves the AI update");
        on_ui(inspect_stale_editor);
        CHECK(AI("contacts.put", saved_args) && g_rc.status == INK_RC_STALE, "AI replay of the same old save also returns stale");
        CHECK(BY_HAND("shell.back", NULL), "leave the stale draft"); quiet();
        snprintf(call_args, sizeof call_args, "{\"id\":%u}", (unsigned)first_id);
        CHECK(AI("contacts.edit", call_args) && g_rc.status == INK_RC_DONE, "explicitly reopen the current record"); quiet();
        on_ui(inspect_editor);
        CHECK(strstr(saved_args, "00000000002"), "reopened editor reads the latest number");
        tap_label("保存联系人");
        CHECK(!strcmp(ink_ui_page_name(), "PhoneContacts"), "a current-version save succeeds");
    } else {
        CHECK(ink_cap_subscribe("confirm.result", confirmed, NULL, INK_EXEC_DIRECT) == 0, "observe the real confirmation receipt");
        tap_label("删除联系人");
        ink_confirm_view_t pending[1];
        CHECK(ink_confirm_list(pending, 1) == 1, "touch deletion creates a confirmation");
        CHECK(strstr(pending[0].detail, "复查联系人") && strstr(pending[0].detail, "00000000001"), "confirmation identifies the persisted name and number");
        CHECK(strstr(pending[0].detail, "删除联系人") && !strstr(pending[0].detail, "id：") && !strstr(pending[0].detail, "if_revision"), "confirmation uses plain Chinese instead of internal argument names");
        CHECK(put("AI修改后的联系人", "00000000002", first_id), "AI updates while the old deletion waits"); quiet();
        answer(pending, "stale");
        CHECK(read_contact(P.db, first_id, contact_name, contact_number) && !strcmp(contact_number, "00000000002"), "old confirmation cannot delete the updated record");
        CHECK(BY_HAND("shell.back", NULL), "leave the old deletion target"); quiet();
        CHECK(AI("contacts.edit", call_args) && g_rc.status == INK_RC_DONE, "reload the updated deletion target"); quiet();
        tap_label("删除联系人");
        CHECK(ink_confirm_list(pending, 1) == 1, "hold a card for the current revision");
        CHECK(put("AI修改后的联系人", "00000000002", first_id), "same-value rewrite still advances the record revision"); quiet();
        answer(pending, "stale");
        CHECK(ink_db_get(P.db, first_id, NULL, NULL, 0) == INK_DB_OK, "version binding also rejects an equal-content rewrite");
        CHECK(BY_HAND("shell.back", NULL), "return to the list before AI deletion"); quiet();
        ink_rec_t record;
        CHECK(ink_db_get(P.db, first_id, &record, NULL, 0) == INK_DB_OK, "read the current deletion target");
        snprintf(saved_args, sizeof saved_args, "{\"id\":%u,\"if_revision\":%u,\"name\":\"AI修改后的联系人\",\"number\":\"00000000002\"}", (unsigned)first_id, (unsigned)record.ver);
        CHECK(AI("contacts.delete", call_args) && g_rc.status == INK_RC_INVALID && !ink_confirm_list(pending, 1), "id alone cannot create an unidentifiable deletion card");
        snprintf(call_args, sizeof call_args, "{\"id\":%u,\"if_revision\":%u,\"name\":\"不匹配的名字\",\"number\":\"00000000002\"}", (unsigned)first_id, (unsigned)record.ver);
        CHECK(AI("contacts.delete", call_args) && g_rc.status == INK_RC_PENDING_CONFIRMATION && ink_confirm_list(pending, 1) == 1, "hold a request with a mismatched display name");
        answer(pending, "stale");
        CHECK(ink_db_get(P.db, first_id, NULL, NULL, 0) == INK_DB_OK, "a misleading description cannot authorize deletion");
        CHECK(AI("contacts.delete", saved_args) && g_rc.status == INK_RC_PENDING_CONFIRMATION, "AI can request a bound deletion from the list");
        CHECK(ink_confirm_list(pending, 1) == 1 && strstr(pending[0].detail, "AI修改后的联系人") && strstr(pending[0].detail, "00000000002"), "AI deletion card identifies the same current record");
        answer(pending, "done");
        CHECK(ink_db_get(P.db, first_id, NULL, NULL, 0) == INK_DB_ERR_NOT_FOUND, "a fresh human confirmation deletes its exact target");
    }
}
static void run(void *arg) {
    (void)arg; vtk_host_call(read_phase, NULL);
    CHECK(WAIT_FOR(ink_member_space_open(), 10000), "the member space opens"); quiet();
    if (!strcmp(phase, "write")) write_phase(); else if (!strcmp(phase, "verify")) verify_phase(); else review_phase();
    CHECK(WAIT_FOR(ink_db_idle(), 10000), "no contact writes remain outstanding");
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name = "contacts-test", .fn = run, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start contacts acceptance driver");
}
