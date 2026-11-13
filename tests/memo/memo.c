/* Exercise the real memo app; only the storage rejection boundary is injected. */
#include "ink/sys/data.h"
static bool refuse_write;
static int test_put(ink_db_t *db, const ink_db_write_t *w, uint32_t *id) {
    return refuse_write ? INK_DB_ERR_FULL : ink_db_put(db, w, id);
}
#define ink_db_put test_put
#include "../../system/apps/memo/memo.c"
#undef ink_db_put
#define ink_main product_main
#include "../../system/boot/main.c"
#undef ink_main
#include "../integration/whole_product.h"
#include <stdlib.h>

static char phase[16], snapshot_json[12000], call_args[256], scene[32], capture_scene[32];
static uint32_t first_id, removed_id;
static const char *wanted;
static lv_obj_t *found;
static char long_body[1801];
static void make_long(void) {
    for (size_t i = 0; i < sizeof long_body - 1; i++) long_body[i] = (char)('a' + i % 26);
    long_body[sizeof long_body - 1] = 0; ink_text_commit_utf8(long_body);
}
static void command(const char *cap, const char *args) {
    CHECK(AI(cap, args) && g_rc.status == INK_RC_DONE, "%s completes (%d: %s)", cap, g_rc.status, g_rc.say);
}
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void quiet(void) { CHECK(WAIT_FOR(settled(), 15000), "memo display settles"); }
static void host_phase(void *unused) {
    (void)unused; snprintf(phase, sizeof phase, "%s", getenv("INK_MEMO_PHASE"));
    snprintf(capture_scene, sizeof capture_scene, "%s", getenv("INK_MEMO_SCENE"));
}
static void host_snapshot(void *unused) {
    (void)unused; char path[768];
    int n = snprintf(path, sizeof path, "%s/snapshots.ndjson", g_sim.out_dir);
    if (n < 0 || (size_t)n >= sizeof path) abort();
    FILE *f = fopen(path, "ab");
    if (!f || fprintf(f, "{\"scene\":\"%s\",\"snapshot\":%s}\n", scene, snapshot_json) < 0 || fclose(f)) abort();
}
static void snapshot_page(void) {
    CHECK(ink_ui_snapshot(snapshot_json, sizeof snapshot_json, NULL) > 0, "page %s has an AI snapshot", scene);
    CHECK(ink_ui_context_snapshot(g_snap, sizeof g_snap, NULL) > 0, "page %s fits production AI context", scene);
    vtk_host_call(host_snapshot, NULL);
}
static void frame(void *unused) {
    (void)unused; CHECK(settled(), "capture a settled memo frame");
    char key_name[80]; snprintf(key_name, sizeof key_name, "test.snapshot.%s", scene);
    ink_probe_count("test.snapshot.settled"); ink_probe_count(key_name); sim_display_snapshot("scene");
}
static void snap(const char *name) {
    snprintf(scene, sizeof scene, "%s", name); quiet(); on_ui(snapshot_page);
    if (!strcmp(name, capture_scene)) { vtk_event_at(vtk_now() + SIM_MS(1), frame, NULL); delay_ms(5); }
}
static void find(lv_obj_t *obj) {
    if (lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN)) return;
    if (lv_obj_check_type(obj, &lv_label_class) && !strcmp(lv_label_get_text(obj), wanted)) found = obj;
    for (uint32_t i = 0; i < lv_obj_get_child_count(obj); i++) find(lv_obj_get_child(obj, i));
}
static void locate(void) {
    found = NULL; find(lv_screen_active()); CHECK(found, "control %s is visible", wanted);
    if (found) { lv_area_t r; lv_obj_t *button = lv_obj_get_parent(found); lv_obj_update_layout(button); lv_obj_get_coords(button, &r);
        CHECK(lv_area_get_height(&r) >= ink_ui_px(INK_CONTROL), "control has a full touch target");
        g_btn_x[0] = (r.x1 + r.x2) / 2; g_btn_y[0] = (r.y1 + r.y2) / 2; }
}
static void touch(const char *label) { wanted = label; on_ui(locate); if (found) tap(g_btn_x[0], g_btn_y[0]); quiet(); }
static void open_list(void) {
    command("app.open", "{\"id\":\"memo\"}");
    CHECK(WAIT_FOR(M.list != NULL, 15000), "F4 list loads through the data service"); quiet();
}
static void record_args(uint32_t id) { snprintf(call_args, sizeof call_args, "{\"id\":%u}", (unsigned)id); }
static void new_note(const char *body) {
    command("memo.edit", NULL); quiet();
    CHECK(M.field && ink_text_focused(), "new memo uses the live long editor");
    snprintf(call_args, sizeof call_args, "{\"text\":\"%s\"}", body); command("text.insert", call_args);
    CHECK(WAIT_FOR(!M.saving && ink_db_idle(), 15000), "automatic memo save becomes durable"); quiet();
    command("text.submit", NULL); quiet();
}
static void inspect_saved(void) {
    memo_t *n = ink_app_alloc(sizeof *n, true);
    CHECK(n && read_note(M.db, first_id, n) && !strcmp(n->body, "编辑后的测试正文"), "edit persisted exactly");
    CHECK(n && read_note(M.db, removed_id, n) && n->deleted, "unrestored deletion persists");
    CHECK(n && read_note(M.db, 3, n) && n->checklist && n->checked == 1, "checklist state persists");
    CHECK(n && read_note(M.db, 4, n) && !n->deleted, "undo restores the original record");
    ink_app_free(n);
}
static void select_all(void) { ink_text_select(0, ink_text_char_offset(ink_text_value(), strlen(ink_text_value()))); }
static void write_phase(void) {
    open_list(); CHECK(ink_db_count(M.db) == 0, "fresh real data store is empty");
    touch("写一条"); quiet();
    command("text.insert", "{\"text\":\"成人私有测试正文\"}");
    first_id = M.id; CHECK(WAIT_FOR(!M.saving && ink_db_idle(), 15000), "touch-created memo is durable"); snap("MemoEdit");
    command("text.submit", NULL); quiet();
    record_args(first_id); command("memo.open", call_args); quiet();
    CHECK(!strcmp(ink_ui_page_name(), "MemoEdit") && M.field && ink_text_focused(), "a text memo opens straight into its editor (Fossify)"); snap("MemoEdit");
    on_ui(select_all); command("text.insert", "{\"text\":\"编辑后的测试正文\"}");
    CHECK(WAIT_FOR(!M.saving && ink_db_idle(), 15000), "updated memo is durable");
    command("text.submit", NULL); quiet();
    command("system.home", NULL); open_list();
    new_note("合成删除记录"); removed_id = 2;
    new_note("牛奶\\n电池\\n测试笔记本");
    record_args(3); command("memo.convert", call_args); quiet(); command("memo.open", call_args); quiet();
    CHECK(!strcmp(ink_ui_page_name(), "MemoChecklist"), "checklist has its actual activity");
    touch("牛奶");
    CHECK(WAIT_FOR(ink_db_idle(), 15000), "touch checkbox persists");
    command("memo.check", "{\"id\":3,\"index\":1}"); command("memo.check", "{\"id\":3,\"index\":1}"); quiet(); snap("MemoChecklist");
    command("system.home", NULL); open_list(); new_note("合成撤销记录");
    record_args(4); command("memo.open", call_args); quiet(); touch("删除");
    CHECK(WAIT_FOR(!strcmp(ink_ui_page_name(), "MemoUndo"), 15000), "deletion opens its undo page"); snap("MemoUndo");
    record_args(removed_id); command("memo.delete", call_args); quiet();
    CHECK(M.model.action_count == 5 && !strcmp(M.actions[0].capability, "memo.undo") && strstr(M.actions[0].args_json, "\"id\":2"), "undo targets the latest deletion, including a lower record id");
    touch("已删除一条备忘 · 撤销");
    command("memo.undo", "{\"id\":4}"); quiet();
    record_args(removed_id); command("memo.delete", call_args); quiet();
    command("system.home", NULL); open_list(); snap("Memo"); on_ui(inspect_saved);
    command("memo.edit", NULL); quiet(); on_ui(make_long);
    CHECK(WAIT_FOR(!M.saving && ink_db_idle(), 15000), "long body saves through its draft reference");
    command("text.submit", NULL); quiet();
    memo_t *long_note = ink_app_alloc(sizeof *long_note, true);
    CHECK(long_note && read_note(M.db, 5, long_note) && !strcmp(long_note->body, long_body), "all 1800 bytes survive the 384-byte argument limit"); ink_app_free(long_note);
    CHECK(ink_member_switch("kid") == 0, "switch to the synthetic child"); quiet(); open_list();
    snprintf(scene, sizeof scene, "kid-empty"); on_ui(snapshot_page);
    CHECK(ink_db_count(M.db) == 0 && !strstr(snapshot_json, "编辑后的测试正文"), "child has an independent empty collection");
    record_args(first_id); CHECK(AI("memo.delete", call_args) && g_rc.status == INK_RC_INVALID, "child cannot delete adult id");
    CHECK(AI("memo.open", call_args) && g_rc.status == INK_RC_INVALID, "child cannot open adult id");
    if (ink_db_count(M.db) != 0) return;
    new_note("孩子自己的测试备忘");
    CHECK(ink_db_count(M.db) == 1, "child writes only the child collection");
    CHECK(ink_member_switch("owner") == 0, "return to adult"); quiet(); open_list();
    CHECK(ink_db_count(M.db) == 5, "adult has only the adult records, including undo data");
}
static void verify_phase(void) {
    first_id = 1; removed_id = 2; open_list(); on_ui(inspect_saved);
    CHECK(ink_db_count(M.db) == 5, "adult records survive a new process");
    memo_t *long_note = ink_app_alloc(sizeof *long_note, true);
    for (size_t i = 0; i < sizeof long_body - 1; i++) long_body[i] = (char)('a' + i % 26);
    long_body[sizeof long_body - 1] = 0;
    CHECK(long_note && read_note(M.db, 5, long_note) && !strcmp(long_note->body, long_body), "the complete long body survives reboot"); ink_app_free(long_note);
    CHECK(ink_member_switch("kid") == 0, "rebooted child space opens"); quiet(); open_list();
    memo_t *n = ink_app_alloc(sizeof *n, true);
    CHECK(ink_db_count(M.db) == 1 && n && read_note(M.db, 1, n) && !strcmp(n->body, "孩子自己的测试备忘"), "isolated child memo survives reboot");
    ink_app_free(n);
}
static void visible_failure(void) {
    lv_area_t notice, field, keyboard;
    lv_obj_t *caption = lv_obj_get_child(M.field, 0), *area = lv_obj_get_child(M.field, 1);
    lv_obj_update_layout(M.field); lv_obj_get_coords(caption, &notice); lv_obj_get_coords(area, &field); ink_shell_keyboard_area(&keyboard);
    CHECK(lv_obj_is_visible(caption) && strstr(lv_label_get_text(caption), "没存上") && notice.y2 < field.y1 &&
          notice.y1 > ink_ui_px(INK_STATUS_BAR + INK_PRIMARY) && notice.y2 < keyboard.y1,
          "failure notice is fully above both editor and keyboard");
}
static void fail_phase(void) {
    open_list(); command("memo.edit", NULL); quiet(); refuse_write = true;
    command("text.insert", "{\"text\":\"保存失败也不能丢的测试正文\"}");
    CHECK(WAIT_FOR(M.mode == FAIL, 15000), "storage rejection shows the failure view in the live editor"); quiet();
    CHECK(M.field && !strcmp(ink_textfield_text(M.field), "保存失败也不能丢的测试正文"), "failure preserves exact editor content");
    CHECK(M.notice && strstr(lv_label_get_text(M.notice), "没存上"), "failure is explained in Chinese"); on_ui(visible_failure); snap("MemoSaveFail");
    CHECK(strstr(snapshot_json, "MemoSaveFail") && strstr(snapshot_json, "没存上") && strstr(snapshot_json, "memo.retry"), "AI receives the same failure view, notice and retry capability");
    CHECK(ink_text_draft_saved() == 1 || WAIT_FOR(ink_text_draft_saved() == 1, 15000), "the same content also has a durable long-text draft");
    refuse_write = false; touch("重试");
    CHECK(WAIT_FOR(!M.saving && ink_db_idle(), 15000), "explicit retry becomes durable");
    memo_t *n = ink_app_alloc(sizeof *n, true);
    CHECK(n && read_note(M.db, M.id, n) && !strcmp(n->body, "保存失败也不能丢的测试正文"), "retry stores the unchanged content"); ink_app_free(n);
    uint32_t id = M.id, draft = ink_text_draft_id();
    snprintf(call_args, sizeof call_args, "{\"id\":%u,\"draft\":%u,\"page\":\"%s\"}", (unsigned)id, (unsigned)draft, M.note.page);
    command("memo.save", call_args);
    CHECK(ink_db_durable_ver(M.db) == ink_db_last_ver(M.db), "AI and editor save use the same durable provider");
}
static const char *review_text;
static void review_type(void) { ink_text_commit_utf8(review_text); }
static void review_fill(const char *text) { review_text = text; on_ui(select_all); on_ui(review_type); quiet(); CHECK(WAIT_FOR(!M.saving && ink_db_idle(), 15000), "review write settles"); }
static void review_saved(void) { CHECK(WAIT_FOR(ink_text_draft_saved() == 1, 15000), "review draft durable before navigation"); quiet(); }
static void review_reopen(uint32_t id) { command("system.home", NULL); open_list(); record_args(id); command("memo.edit", call_args); quiet(); }
static void review_draft_loss(void) {
    open_list(); new_note("ORIGINAL"); command("memo.edit", "{\"id\":1}"); quiet();
    uint32_t draft = ink_text_draft_id(); lv_obj_t *field = M.field;
    refuse_write = true; review_fill("UNSAVED");
    CHECK(WAIT_FOR(M.mode == FAIL, 15000), "failure reached");
    CHECK(M.field == field && ink_text_focused() && ink_text_draft_id() == draft, "failure reuses the original focused editor and draft");
    command("text.insert", "{\"text\":\"_CONTINUED\"}"); review_saved();
    refuse_write = false; review_reopen(1);
    CHECK(M.field && !strcmp(ink_textfield_text(M.field), "UNSAVED_CONTINUED"), "failure draft survives leave and reopen");
}
static void review_retry_edit(void) {
    open_list(); new_note("ORIGINAL"); command("memo.edit", "{\"id\":1}"); quiet();
    refuse_write = true; review_fill("RETRY");
    CHECK(WAIT_FOR(M.mode == FAIL, 15000), "failure reached"); review_saved();
    refuse_write = false; touch("重试"); quiet();
    command("text.insert", "{\"text\":\"_AFTER_SUCCESS\"}"); review_saved();
    CHECK(WAIT_FOR(!M.saving && ink_db_idle(), 15000), "subsequent edit autosave finishes before leaving");
    memo_t *n = ink_app_alloc(sizeof *n, true);
    CHECK(n && read_note(M.db, 1, n) && !strcmp(n->body, "RETRY_AFTER_SUCCESS"), "retry restores autosave without an exit or second retry"); ink_app_free(n);
    review_reopen(1);
    CHECK(M.field && !strcmp(ink_textfield_text(M.field), "RETRY_AFTER_SUCCESS"), "successful retry restores subsequent autosave");
}
static void review_check_edit(void) {
    open_list(); new_note("alpha\\nbeta\\ngamma"); command("memo.convert", "{\"id\":1}"); command("memo.check", "{\"id\":1,\"index\":1}"); quiet();
    command("memo.edit", "{\"id\":1}"); quiet(); review_fill("beta\ngamma"); command("text.submit", NULL); quiet();
    command("memo.open", "{\"id\":1}"); quiet();
    CHECK(M.note.checked == 1, "deleting an earlier checklist line preserves completion on beta");
    command("memo.edit", "{\"id\":1}"); quiet(); review_fill("new\nbeta\ngamma"); command("text.submit", NULL); quiet();
    command("memo.open", "{\"id\":1}"); quiet();
    CHECK(M.note.checked == 2, "inserting an unchecked first item moves beta completion with its text");
}
static void review_full_snapshot(void) {
    open_list(); command("memo.edit", NULL); quiet();
    review_fill("一二三四五六七八九十一二三四五六七八九十一二三四\n一二三四五六七八九十一二三四五六七八九十一二三四\n一二三四五六七八九十一二三四五六七八九十一二三四\n一二三四五六七八九十一二三四五六七八九十一二三四");
    command("text.submit", NULL); quiet(); command("memo.convert", "{\"id\":1}"); command("memo.open", "{\"id\":1}"); quiet();
    snprintf(scene, sizeof scene, "MemoChecklist-full"); on_ui(snapshot_page);
}
static void review_long_line(void) {
    static const char LONG_ITEM[] = "123456789012345678901234567890123456789012345678901234567890TAIL_MUST_READ";
    open_list(); new_note(LONG_ITEM); command("memo.convert", "{\"id\":1}"); command("memo.open", "{\"id\":1}"); quiet();
    snprintf(scene, sizeof scene, "MemoChecklist-long"); on_ui(snapshot_page);
    CHECK(strstr(snapshot_json, "TAIL_MUST_READ") && strstr(snapshot_json, "{\\\"id\\\":1,\\\"index\\\":0}"),
          "a long checklist item reaches the AI whole, with the MemoItem page that shows it all");
    touch("改");
    CHECK(!strcmp(ink_ui_page_name(), "MemoItem"), "改 opens the item page (%s)", ink_ui_page_name());
    wanted = LONG_ITEM; on_ui(locate);
    CHECK(found != NULL, "the item page shows the entire checklist item");
}
static void review_burst_ui(void) { ink_text_commit_utf8("BURST_FINAL"); ink_text_enter(); }
static void review_burst(void) {
    open_list(); new_note("ORIGINAL"); command("memo.edit", "{\"id\":1}"); quiet(); on_ui(select_all); on_ui(review_burst_ui); quiet();
    CHECK(WAIT_FOR(ink_db_idle(), 15000), "final save completes after the editor has lost focus");
    memo_t *n = ink_app_alloc(sizeof *n, true);
    CHECK(n && read_note(M.db, 1, n) && !strcmp(n->body, "BURST_FINAL"), "last edit immediately followed by done reaches memo record"); ink_app_free(n);
    command("memo.open", "{\"id\":1}"); quiet();
}

static void run(void *arg) {
    (void)arg; vtk_host_call(host_phase, NULL);
    CHECK(WAIT_FOR(ink_member_space_open(), 10000), "member space opens"); test_start_home(); quiet();
    if (!strcmp(phase, "draft_loss")) review_draft_loss();
    else if (!strcmp(phase, "retry_edit")) review_retry_edit();
    else if (!strcmp(phase, "check_edit")) review_check_edit();
    else if (!strcmp(phase, "long_line")) review_long_line();
    else if (!strcmp(phase, "burst")) review_burst();
    else if (!strcmp(phase, "full_snapshot")) review_full_snapshot();
    else if (!strcmp(phase, "write")) write_phase(); else if (!strcmp(phase, "verify")) verify_phase(); else fail_phase();
    CHECK(WAIT_FOR(ink_db_idle(), 15000), "all memo and draft writes finish"); ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name = "memo-test", .fn = run, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start memo acceptance driver");
}
