/* F3: real long-input host, power-cut recovery and synthetic member boundaries. */
#include "ink/sys/textfield.h"
#include "ink/ui_shell.h"
#include <stdlib.h>
#define ink_main product_main
#include "../../system/boot/main.c"
#undef ink_main
#include "../integration/whole_product.h"

static const char MIX[] = "中A😀e\xCC\x81👩‍👧🇨🇳Z";
static const char UNIT[] = "中A😀e\xCC\x81👩‍👧🇨🇳Z ";
static char initial[2048], snapshot[16384], stale[256], remembered[4096];
static lv_obj_t *field;
static uint32_t reference;
static const char *page_name = "ImeLong";
static size_t first, last, page_number, page_count;

static void indexes(void) {
    const size_t bytes[] = {0, 3, 4, 8, 9, 11, 15, 18, 22, 26, 30, 31};
    for (size_t c = 0; c < sizeof bytes / sizeof *bytes; c++) {
        CHECK(ink_text_byte_offset(MIX, c) == bytes[c], "codepoint %zu maps to byte %zu", c, bytes[c]);
        CHECK(ink_text_char_offset(MIX, bytes[c]) == c, "byte %zu maps to codepoint %zu", bytes[c], c);
        if (c + 1 < sizeof bytes / sizeof *bytes)
            for (size_t b = bytes[c]; b < bytes[c + 1]; b++)
                CHECK(ink_text_char_offset(MIX, b) == c, "interior byte %zu floors to %zu", b, c);
    }
    CHECK(ink_text_byte_offset(MIX, 999) == 31 && ink_text_char_offset(MIX, 999) == 11, "past-end positions clamp");
    const size_t clusters[] = {0, 3, 4, 8, 11, 22, 30, 31};
    for (size_t i = 0; i + 1 < sizeof clusters / sizeof *clusters; i++)
        CHECK(ink_text_cluster_end(MIX, clusters[i]) == clusters[i + 1], "whole mixed cluster at %zu", clusters[i]);
    CHECK(ink_text_cluster_end("👍🏽X", 0) == 8 && ink_text_cluster_end("✈️X", 0) == 6, "emoji modifiers and variation selectors stay attached");
    CHECK(ink_text_cluster_end("e\xCC\x81", 0) == 3 && ink_text_byte_offset("e\xCC\x81", 1) == 1,
          "codepoint indices and grapheme page boundaries are distinct");
    CHECK(ink_text_cluster_end("", 0) == 0, "empty cluster has no bytes");
}
static void settle(void) { if (field) ink_textfield_long_refresh(field); }
static int page_snapshot(char *out, size_t cap) { return field ? ink_textfield_long_snapshot(field, out, cap) : -1; }
static void create(lv_obj_t *screen, void *arg) {
    (void)arg;
    ink_shell_topbar(screen, "写备忘");
    lv_obj_t *done = ink_ui_pill(screen, "完成", INK_CANVAS_W - INK_MARGIN - INK_PRIMARY, INK_STATUS_BAR,
                                INK_PRIMARY, INK_CONTROL, false, NULL, NULL);
    ink_shell_bind(done, "text.submit", "{}");
    ink_text_cfg_t cfg = {.type = INK_TEXT_PLAIN, .max = 4095, .initial = initial, .draft_page = page_name, .draft_id = reference};
    field = ink_textfield_long_create(screen, &cfg);
    CHECK(field != NULL, "dedicated long editor opens the owned draft");
    if (field) reference = ink_text_draft_id();
}
static const ink_activity_t PAGE = {.name = "ImeLong", .create = create, .snapshot = page_snapshot, .settle = settle};
static void open_page(void) { ink_ui_home(); ink_ui_push(&PAGE, NULL); }
static bool boundary(size_t byte) {
    static const size_t starts[] = {0, 3, 4, 8, 11, 22, 30, 31};
    for (size_t i = 0; i < sizeof starts / sizeof *starts; i++) if (byte % strlen(UNIT) == starts[i]) return true;
    return false;
}
static void inspect_page(void) {
    CHECK(field && ink_textfield_long_snapshot(field, snapshot, sizeof snapshot) > 0, "long viewport snapshot is available");
    ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL);
    CHECK(j && ink_json_parse(j, snapshot, strlen(snapshot)) == 0, "viewport snapshot is JSON");
    if (!j) return;
    int64_t a = -1, b = -1, p = 0, n = 0;
    CHECK(ink_json_int(j, ink_json_find(j, 0, "start"), &a) && ink_json_int(j, ink_json_find(j, 0, "end"), &b) &&
          ink_json_int(j, ink_json_find(j, 0, "page_number"), &p) && ink_json_int(j, ink_json_find(j, 0, "pages"), &n), "snapshot contains actual page bounds");
    first = (size_t)a; last = (size_t)b; page_number = (size_t)p; page_count = (size_t)n;
    size_t start_byte = ink_text_byte_offset(ink_text_value(), first), end_byte = ink_text_byte_offset(ink_text_value(), last);
    CHECK(boundary(start_byte) && boundary(end_byte), "rendered page avoids splitting UTF-8, accents, ZWJ and flags");
    CHECK(p > 0 && n > 1 && b > a, "long fixture actually spans multiple nonempty pages");
    lv_obj_t *label = lv_obj_get_child(field, 0);
    const char *visible = lv_label_get_text(label);
    CHECK(strlen(visible) >= end_byte - start_byte && strstr(visible, "|") != NULL, "actual label renders the page and fixed caret");
    lv_area_t area, keyboard; lv_obj_get_coords(field, &area); ink_shell_keyboard_area(&keyboard);
    CHECK(area.y2 < keyboard.y1 && !lv_obj_has_flag(field, LV_OBJ_FLAG_SCROLLABLE), "viewport stays above the F2 keyboard without scrolling");
    osal_free(j);
}
static void select_first(void) { ink_text_select(1, 2); }
static void start_text(void) { ink_text_set_cursor(0); }
static void save_position(void) {
    ink_text_set_cursor(12);
    ink_text_key('n'); ink_text_key('i');
    CHECK(!strcmp(ink_text_composing(), "ni"), "unconfirmed pinyin remains separate from body");
    snprintf(remembered, sizeof remembered, "%s", ink_text_value());
}
static void recover(void) {
    CHECK(field && !strcmp(ink_text_value(), initial) && !strcmp(ink_text_composing(), "ni") && ink_text_cursor() == 12,
          "power-cut boot restores exact body, composition and cursor");
    CHECK(reference == 1 && ink_text_draft_saved() == 1, "same page/member draft reference was durable");
}
static void wrong_page(void) {
    ink_text_blur(0);
    CHECK(ink_textfield_long_snapshot(field, snapshot, sizeof snapshot) > 0 && strstr(snapshot, "\"actions\":[]"),
          "closed keyboard keeps the page readable without advertising unfocused editing actions");
    lv_obj_t *label = lv_obj_get_child(field, 0);
    CHECK(strlen(lv_label_get_text(label)) < strlen(initial) && !strncmp(lv_label_get_text(label), initial, strlen(lv_label_get_text(label))),
          "closing input retains the actual first page instead of displaying the whole document in its viewport");
    ink_text_cfg_t cfg = {.type = INK_TEXT_PLAIN, .max = 4095, .draft_page = "OtherPage", .draft_id = 1};
    CHECK(ink_text_focus(&cfg, NULL, NULL) < 0, "another page cannot load this draft reference");
}
static void another_page(void) {
    ink_text_cfg_t cfg = {.type = INK_TEXT_PLAIN, .initial = "OTHER_PAGE_SYNTHETIC", .draft_page = "OtherPage"};
    CHECK(ink_text_focus(&cfg, NULL, NULL) > 0, "another page gets its own draft");
    CHECK(ink_text_draft_id() != 1, "page references are distinct");
    ink_text_key('x'); ink_text_blur(0);
}
static void secrets(void) {
    ink_text_cfg_t cfg = {.type = INK_TEXT_PASSWORD, .draft_page = "ImeLong"};
    CHECK(ink_text_focus(&cfg, NULL, NULL) < 0, "passwords cannot enter the draft collection");
    cfg.type = INK_TEXT_PLAIN; cfg.ai_hidden = true;
    CHECK(ink_text_focus(&cfg, NULL, NULL) < 0, "hidden fields cannot enter the draft collection");
}
static void stale_target(void) {
    CHECK(ink_text_snapshot(snapshot, sizeof snapshot) > 0, "read current text target");
    ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL);
    int64_t s = 0, r = 0;
    if (j && ink_json_parse(j, snapshot, strlen(snapshot)) == 0) {
        ink_json_int(j, ink_json_find(j, 0, "session"), &s); ink_json_int(j, ink_json_find(j, 0, "revision"), &r);
    }
    osal_free(j);
    snprintf(stale, sizeof stale, "{\"session\":%lld,\"revision\":%lld,\"text\":\"NEW_MEMBER_MUST_NOT_WRITE\"}", (long long)s, (long long)r);
    snprintf(remembered, sizeof remembered, "%s", ink_text_value());
    CHECK(ink_member_switch("kid") == 0, "switch with old input callbacks still queued");
    ink_receipt_t rc;
    CHECK(ink_cap_invoke_json("text.insert", stale, INK_SRC_AI, 0, &rc) == INK_RC_STALE, "new member cannot edit the previous session before UI handoff drains");
    CHECK(!strcmp(remembered, ink_text_value()), "member rejection has no edit side effect");
    CHECK(ink_text_snapshot(snapshot, sizeof snapshot) < 0 && !snapshot[0], "old body is unreadable immediately after member switch");
    CHECK(ink_textfield_long_snapshot(field, snapshot, sizeof snapshot) < 0 && !snapshot[0], "old page viewport is unreadable immediately after member switch");
}
static void kid_draft(void) {
    ink_text_cfg_t cfg = {.type = INK_TEXT_PLAIN, .max = 4095, .draft_page = "ImeLong", .draft_id = 1};
    CHECK(ink_text_focus(&cfg, NULL, NULL) < 0, "child cannot reopen adult draft reference");
    cfg.draft_id = 0; cfg.initial = "CHILD_ONLY_SYNTHETIC";
    CHECK(ink_text_focus(&cfg, NULL, NULL) > 0 && !strcmp(ink_text_value(), cfg.initial), "child creates an independent same-page draft");
    ink_text_key('x');
}
static lv_obj_t *find_label(lv_obj_t *root, const char *text) {
    if (lv_obj_check_type(root, &lv_label_class) && !strcmp(lv_label_get_text(root), text)) return root;
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); i++) {
        lv_obj_t *found = find_label(lv_obj_get_child(root, (int32_t)i), text); if (found) return found;
    }
    return NULL;
}
static lv_point_t point;
static const char *button_text;
static void locate_button(void) {
    lv_obj_t *l = find_label(lv_layer_sys(), button_text);
    CHECK(l && lv_obj_is_visible(l), "selection action button is visible: %s", button_text);
    point = (lv_point_t){0, 0}; if (!l) return;
    lv_area_t a; lv_obj_update_layout(l); lv_obj_get_coords(lv_obj_get_parent(l), &a);
    CHECK(a.x1 >= 0 && a.x2 < INK_CANVAS_W && a.y2 < INK_CANVAS_H, "selection action fits the screen");
    point = (lv_point_t){(a.x1 + a.x2) / 2, (a.y1 + a.y2) / 2};
}
static void locate_letter(void) {
    lv_obj_t *label = lv_obj_get_child(field, 0); lv_area_t a;
    lv_label_get_letter_pos(label, 2, &point); lv_obj_get_coords(label, &a);
    point.x += a.x1 + ink_ui_px(INK_GAP_XS); point.y += a.y1 + ink_ui_px(INK_GAP_S);
}
static void menu_action(const char *text) {
    on_ui(start_text); on_ui(locate_letter);
    hw(HW_PRESS, point.x, point.y); delay_ms(800); hw(HW_RELEASE, point.x, point.y); delay_ms(180);
    button_text = text; on_ui(locate_button); tap(point.x, point.y);
}
static void check_words(void) {
    ink_db_t *db = ink_db_open_member(ink_db_find_def("text.words"), "owner");
    CHECK(db && ink_db_count(db) == 2, "human and AI each add exactly one word");
    ink_db_cursor_t at = {0}; ink_rec_t rec; char value[64]; unsigned sources = 0;
    while (db && ink_db_next(db, &at, &rec, value, sizeof value) == 1) {
        CHECK(!strcmp(value, "A"), "both actions store only the selected word"); sources |= 1u << rec.src;
    }
    CHECK((sources & (1u << INK_SRC_TOUCH)) && (sources & (1u << INK_SRC_AI)), "word receipts preserve the trusted action sources");
    ink_db_close(db);
}
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void take_snapshot(void *arg) { (void)arg; sim_display_snapshot("scene"); }
static void capture(void) {
    CHECK(WAIT_FOR(settled(), 10000), "long editor settles before capture");
    vtk_event_at(vtk_now() + SIM_MS(1), take_snapshot, NULL); delay_ms(5);
    ink_probe_count("test.snapshot.settled"); ink_probe_count("test.snapshot.ImeLong");
}
static void inspect_question(void) {
    const char *request = sim_hub_last_ask();
    ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL); char text[64] = "";
    CHECK(request && j && ink_json_parse(j, request, strlen(request)) == 0, "real AI request reaches only the synthetic hub");
    if (request && j) ink_json_str(j, ink_json_find(j, 0, "params.question"), text, sizeof text);
    CHECK(!strcmp(text, "A"), "AI question uses the selection rather than the whole draft"); osal_free(j);
}
static void run(void *arg) {
    (void)arg; delay_ms(1800);
    const char *phase = getenv("INK_TEXT_PHASE");
    on_ui(indexes);
    if (phase && !strcmp(phase, "indexes")) { ink_probe_count("test.finished"); return; }
    initial[0] = 0;
    for (int i = 0; i < 36; i++) strcat(initial, UNIT);
    bool recovering = phase && !strcmp(phase, "recover"); reference = recovering ? 1 : 0;
    on_ui(open_page);
    if (!field) { ink_probe_count("test.finished"); return; }
    if (recovering) {
        on_ui(recover); on_ui(check_words); on_ui(wrong_page); on_ui(another_page); on_ui(secrets);
        reference = 1; on_ui(open_page); on_ui(stale_target);
        delay_ms(500); on_ui(kid_draft);
        CHECK(WAIT_FOR(ink_text_draft_saved() == 1, 10000), "child draft reaches durable storage separately");
        ink_probe_count("test.finished"); return;
    }
    on_ui(start_text); on_ui(inspect_page);
    size_t covered = last, pages = page_count;
    for (size_t i = 1; i < pages; i++) {
        CHECK(AI("text.page", "{\"direction\":1}") && g_rc.status == INK_RC_DONE, "AI uses the page capability");
        on_ui(inspect_page); CHECK(first == covered, "page ranges are contiguous, without lost or duplicated characters"); covered = last;
    }
    CHECK(ink_text_byte_offset(initial, covered) == strlen(initial), "all bytes are covered by real rendered pages");
    key(HAL_KEY_PREV); on_ui(inspect_page); CHECK(page_number + 1 == page_count, "physical previous key uses the same page capability");
    on_ui(start_text); capture();
    menu_action("加单词");
    CHECK(AI("text.select", "{\"start\":1,\"end\":2}") && g_rc.status == INK_RC_DONE &&
          AI("text.add_word", "{}") && g_rc.status == INK_RC_DONE, "AI selection action uses the registered word capability");
    on_ui(check_words);
    ink_hub_demand(); CHECK(WAIT_FOR(ink_hub_online(), 30000), "synthetic hub online");
    menu_action("问 AI"); CHECK(WAIT_FOR(sim_hub_last_ask() != NULL, 5000), "human selection question is sent"); on_ui(inspect_question);
    key(HAL_KEY_HOME);
    double asks = sim_metric_value("hub.requests.ai.ask");
    CHECK(AI("text.ask_ai", "{}") && g_rc.status == INK_RC_PENDING_CONFIRMATION, "unsolicited AI external action follows existing confirmation policy");
    unsigned token = 0; sscanf(g_rc.object, "confirm:%u", &token);
    CHECK(token && ink_confirm_decide(token, true, INK_SRC_TOUCH) == 0, "person confirms the same selection action");
    CHECK(WAIT_FOR(!strcmp(ink_ui_top_name(), "Ai"), 5000), "confirmed AI action opens the real answer panel");
    CHECK(WAIT_FOR(sim_metric_value("hub.requests.ai.ask") > asks, 5000), "confirmed AI capability sends a second real question"); on_ui(inspect_question);
    key(HAL_KEY_HOME); on_ui(save_position);
    ink_db_flush_all();
    CHECK(WAIT_FOR(ink_text_draft_saved() == 1 && ink_db_idle() && !ink_storage_pending(), 10000), "all accepted text and composition are durable before the abrupt cut");
    ink_probe_count("test.cut"); ink_probe_count("test.finished"); hal_power_reboot();
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name = "text-long-test", .fn = run, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL,
                           .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start bounded long editor test");
}
