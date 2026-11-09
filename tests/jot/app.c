/* F13 acceptance uses only public app interfaces and real UI input. */
#include "../../examples/jot/jot.h"
#include "ink/sys/app.h"
#include "ink/sys/ui.h"
#include "ink/ui_shell.h"
#include <stdlib.h>
static int ink_jot_start_ui(const ink_activity_t *home);
#define ink_main product_main
#define ink_ui_start ink_jot_start_ui
#include "../../system/boot/main.c"
#undef ink_ui_start
#undef ink_main
#include "../integration/whole_product.h"

static char phase[16], snapshot[3072], context[3072], save_cap[48];
static uint32_t save_version;
static ink_receipt_t ai_receipt;
static const char *wanted;
static lv_point_t point;
static bool found;
static int forbidden_runs;
static ink_card_t adult_card;
static const char SAME[] = "合成记录😀";

static void forbidden(ink_call_t *call) { (void)call; forbidden_runs++; }
static const ink_cap_t ADULT = {
    .id = "test.jot_adult", .title = "合成大人操作", .effect = INK_FX_LOCAL_WRITE, .run = forbidden
};
static int ink_jot_start_ui(const ink_activity_t *home) {
    if (ink_jot_init() != 0 || ink_cap_register(&ADULT, 1) != 0) return -1;
    return ink_ui_start(home);
}
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void quiet(void) { CHECK(WAIT_FOR(settled(), 10000), "the real screen settles"); }
static lv_obj_t *label(lv_obj_t *root, const char *text) {
    if (!lv_obj_is_visible(root)) return NULL;
    if (lv_obj_check_type(root, &lv_label_class) && strstr(lv_label_get_text(root), text)) return root;
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); i++) {
        lv_obj_t *hit = label(lv_obj_get_child(root, (int32_t)i), text);
        if (hit) return hit;
    }
    return NULL;
}
static void locate(void) {
    lv_obj_update_layout(lv_screen_active());
    lv_obj_t *hit = label(lv_screen_active(), wanted);
    found = hit != NULL;
    CHECK(found, "visible control exists: %s", wanted);
    if (hit) {
        lv_area_t area;
        lv_obj_get_coords(hit, &area);
        point = (lv_point_t){(area.x1 + area.x2) / 2, (area.y1 + area.y2) / 2};
    }
}
static void press(const char *text) {
    wanted = text; on_ui(locate);
    if (found) tap(point.x, point.y);
    quiet();
}
static void inspect(void) {
    CHECK(ink_ui_snapshot(snapshot, sizeof snapshot, NULL) > 0 &&
          ink_ui_context_snapshot(context, sizeof context, NULL) > 0 && !strcmp(snapshot, context),
          "AI context equals the actual foreground snapshot");
    if (strstr(snapshot, "\"total\":0"))
        CHECK(label(lv_screen_active(), "还没有记录"), "the empty-state text is on screen");
    ink_json_t *json = ink_app_alloc(sizeof *json, true);
    CHECK(json && ink_json_parse(json, snapshot, strlen(snapshot)) == 0, "snapshot is complete JSON");
    if (json && ink_json_parse(json, snapshot, strlen(snapshot)) == 0) {
        for (int t = 1; t < json->ntok; t++) {
            if (json->tok[t].type != 1) continue;
            char value[128] = "", id[32] = "";
            if (ink_json_str(json, ink_json_find(json, t, "value"), value, sizeof value) > 0)
                CHECK(label(lv_screen_active(), value) != NULL, "every exported value is visible: %s", value);
            if (ink_json_str(json, ink_json_find(json, t, "id"), id, sizeof id) > 0 && !strcmp(id, "save")) {
                int64_t version = 0;
                CHECK(ink_json_str(json, ink_json_find(json, t, "capability"), save_cap, sizeof save_cap) > 0 &&
                      !strcmp(save_cap, "jot.save") && label(lv_screen_active(), "保存"), "the button and AI share the declared save capability");
                CHECK(ink_json_int(json, ink_json_find(json, 0, "page_version"), &version), "AI reads the page version");
                save_version = (uint32_t)version;
            }
        }
    }
    ink_app_free(json);
}
static void ai_save(void) {
    CHECK(ink_cap_invoke_json(save_cap, "{}", INK_SRC_AI, save_version, &ai_receipt) == INK_RC_DONE &&
          ai_receipt.revision > 0 && !strncmp(ai_receipt.object, "jot:", 4), "AI calls the snapshot's capability and receives a structured receipt");
}
static void note(const char *text, bool touch) {
    CHECK(AI("jot.new", NULL) && g_rc.status == INK_RC_DONE, "open the input page through its capability"); quiet();
    CHECK(AI("jot.focus", NULL) && g_rc.status == INK_RC_DONE, "focus through F2");
    char args[200];
    ink_jw_t w;
    ink_jw_init(&w, args, sizeof args); ink_jw_raw(&w, "{\"text\":"); ink_jw_str(&w, text); ink_jw_raw(&w, "}");
    CHECK(!w.overflow && AI("text.insert", args) && g_rc.status == INK_RC_DONE, "fill the real session with synthetic text");
    CHECK(AI("text.submit", NULL) && g_rc.status == INK_RC_DONE, "finish input before saving"); quiet();
    on_ui(inspect);
    CHECK(strstr(snapshot, "合成") != NULL, "the input snapshot contains the screen text");
    if (touch) press("保存");
    else { on_ui(ai_save); quiet(); }
    CHECK(!strcmp(ink_ui_page_name(), "jot.list"), "saving returns to the list");
    on_ui(inspect);
}
static ink_db_t *db(void) { return ink_db_open(ink_db_find_def("jot.entries"), NULL); }
static void verify_records(void) {
    ink_db_t *records = db();
    ink_db_cursor_t cursor = {0};
    ink_rec_t a = {0}, b = {0};
    char first[600], second[600];
    CHECK(records && ink_db_count(records) == 4, "four adult records exist");
    CHECK(ink_db_next(records, &cursor, &a, first, sizeof first) == 1 &&
          ink_db_next(records, &cursor, &b, second, sizeof second) == 1 &&
          a.len == b.len && !memcmp(first, second, a.len) && a.id != b.id &&
          a.src == INK_SRC_TOUCH && b.src == INK_SRC_AI, "touch and AI persist identical payloads through the same handler, retaining provenance");
    ink_db_close(records);
}
static void open_app(void) {
    CHECK(AI("jot.open", NULL) && g_rc.status == INK_RC_DONE, "the card's capability opens the app"); quiet();
    CHECK(WAIT_FOR((on_ui(inspect), strstr(snapshot, "\"total\":")), 10000), "the app loads its own collection asynchronously");
    on_ui(inspect);
}
static void close_app(void) {
    CHECK(BY_HAND("app.close", "{\"id\":\"jot\"}") && g_rc.status == INK_RC_DONE, "stop the app and release member data"); quiet();
}
static void verify_restored(void) {
    on_ui(inspect);
    CHECK(strstr(snapshot, "\"page\":2,\"pages\":2,\"total\":4") && strstr(snapshot, "合成四") &&
          !strstr(snapshot, SAME) && !strstr(snapshot, "孩子的合成记录"), "the saved list position and only the adult's visible record are restored");
    verify_records();
}
static void card_before_switch(void) {
    for (int i = 0; i < ink_app_count(); i++) {
        const ink_app_def_t *app = ink_app_at(i);
        if (!strcmp(app->id, "jot")) CHECK(app->card(0, &adult_card) == 1 &&
            !strcmp(adult_card.action, "jot.open") && app->card(1, &adult_card) == 0, "one card declares the same opening capability");
    }
}
static void child_permissions(void) {
    CHECK(!ink_app_card_visible(&adult_card), "a retained adult card is invisible to the child");
    CHECK(ink_cap_find("jot.save")->child_ok && !ink_cap_ai_callable(ink_cap_find("jot.save")) &&
          ink_cap_ai_callable(ink_cap_find("jot.new")), "the child declaration is true and AI availability follows the current page");
    CHECK(!ink_cap_find("test.jot_adult")->child_ok && AI("test.jot_adult", NULL) &&
          g_rc.status == INK_RC_REJECTED && forbidden_runs == 0, "an adult-only declaration is rejected before the handler runs");
}
static void durable(void) {
    ink_db_t *records = db();
    uint32_t ver = ink_db_last_ver(records);
    ink_db_flush_all();
    CHECK(ink_db_wait(records, ver, 10000000) == 0, "records are durable, beyond acceptance in RAM");
    CHECK(WAIT_FOR(ink_db_idle() && !ink_storage_pending(), 10000), "app state and all writes reach simulated flash");
    ink_db_close(records);
}
static void host_phase(void *arg) {
    (void)arg;
    const char *value = getenv("INK_JOT_PHASE");
    sim_copy_name(phase, sizeof phase, value ? value : "write");
}
static void run(void *arg) {
    (void)arg;
    CHECK(WAIT_FOR(ink_member_space_open(), 10000), "the member space opens"); quiet();
    test_start_home(); quiet(); open_app();
    if (!strcmp(phase, "write")) {
        CHECK(strstr(snapshot, "\"total\":0"), "the initial list is empty");
        note(SAME, true); note(SAME, false); note("合成三", false); note("合成四", false);
        CHECK(AI("jot.next", NULL) && g_rc.status == INK_RC_DONE, "turn and save the list position"); quiet();
    }
    verify_restored();
    CHECK(AI("app.cards", NULL) && g_rc.status == INK_RC_DONE && strstr(g_result, "jot.open") &&
          strstr(g_result, "记下新的事情"), "the real shared card capability includes the example");
    on_ui(card_before_switch); durable(); close_app();
    CHECK(ink_member_switch("kid") == 0, "switch to the real child member"); quiet(); open_app();
    if (!strcmp(phase, "write")) {
        CHECK(strstr(snapshot, "\"total\":0") && !strstr(snapshot, SAME), "the child starts with a separate empty collection");
        note("孩子的合成记录", false);
    }
    on_ui(child_permissions); on_ui(inspect);
    CHECK(strstr(snapshot, "\"total\":1") && strstr(snapshot, "孩子的合成记录") && !strstr(snapshot, SAME), "the child sees only their record, also after reboot");
    durable(); close_app(); ink_member_test_reset(); quiet(); open_app(); verify_restored(); durable();
    ink_probe_count("test.finished");
}
void ink_main(void) {
    vtk_host_call(host_phase, NULL); product_main();
    osal_task_cfg_t cfg = {.name = "jot-test", .fn = run, .stack_bytes = 16384,
        .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start the existing-style test driver");
}
