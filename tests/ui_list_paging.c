/* F4: real LVGL views, F1 bindings, file ids and hardware input paths. */
#include "ink/sys/ui_list.h"
#include "ink/ui_shell.h"
#define ink_main product_main
#include "../system/boot/main.c"
#undef ink_main
#include "integration/whole_product.h"
#include <stdlib.h>

static ink_ui_list_t *list;
static ink_ui_list_position_t position;
static lv_obj_t *body, *slots[3], *first_slots[3];
static char ids[3][64], labels[3][64], args[3][128], snapshot[4096], opened[64];
static size_t total = 1000;
static unsigned columns = 1, creates, binds, next_calls, prev_calls;
static ink_source_t nav_source, open_source;
static uint32_t file_id;
static const char *file_label = "合成文件.txt";
static ink_ui_action_target_t old_target;
static const ink_activity_t PAGE, DETAIL;

static void nav(ink_call_t *c) {
    nav_source = c->src;
    if (!strcmp(c->cap->id, "test.list_next")) { next_calls++; ink_ui_list_turn(list, 1); }
    else { prev_calls++; ink_ui_list_turn(list, -1); }
}
static void open_item(ink_call_t *c) {
    ink_args_get_str(&c->args, "id", opened, sizeof opened); open_source = c->src;
    ink_ui_push(&DETAIL, NULL);
}
static const ink_cap_t CAPS[] = {
    {.id = "test.list_next", .title = "合成列表下一页", .activity = "test.list", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = nav},
    {.id = "test.list_prev", .title = "合成列表上一页", .activity = "test.list", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = nav},
    {.id = "test.list_open", .title = "打开合成项", .activity = "test.list", .effect = INK_FX_NAVIGATE, .child_ok = true,
     .input = "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\"}},\"required\":[\"id\"]}", .run = open_item}
};
static const ink_ui_action_spec_t actions[] = {
    {.id = "prev", .label = "上一页", .capability = "test.list_prev"},
    {.id = "next", .label = "下一页", .capability = "test.list_next"}
};
static const ink_ui_page_spec_t model = {
    .id = "test.list", .title = "合成列表", .object = {"example:list", 7}, .actions = actions, .action_count = 2
};
static void bind_item(lv_obj_t *slot, size_t index, ink_ui_field_spec_t *field, ink_ui_action_spec_t *action, void *ctx) {
    (void)ctx;
    unsigned at = index % 3;
    CHECK(index >= position.page * 3 && index < position.page * 3 + 3 && index < total,
          "only visible indices reach the binder: %zu", index);
    if (!lv_obj_get_child_count(slot)) {
        creates++;
        ink_ui_line(slot, "", INK_FONT_LIST, INK_WEIGHT_REGULAR, INK_GAP, INK_GAP,
                    (INK_CANVAS_W - 2 * INK_MARGIN) / (int)columns - 2 * INK_GAP);
    }
    binds++; slots[at] = slot;
    if (index == 4 && file_id) snprintf(ids[at], sizeof ids[at], "file:%u", (unsigned)file_id);
    else snprintf(ids[at], sizeof ids[at], "example:item-%zu", index + 1);
    if (index == 4 && file_id) snprintf(labels[at], sizeof labels[at], "%s", file_label);
    else snprintf(labels[at], sizeof labels[at], "合成项 %zu", index + 1);
    snprintf(args[at], sizeof args[at], "{\"id\":\"%s\"}", ids[at]);
    *field = (ink_ui_field_spec_t){.id = ids[at], .role = "listitem", .label = labels[at], .value = "合成内容", .object = {ids[at], 7}};
    *action = (ink_ui_action_spec_t){.id = ids[at], .label = "打开", .capability = "test.list_open", .args_json = args[at], .object = field->object, .enabled = true};
    lv_label_set_text(lv_obj_get_child(slot, 0), labels[at]);
}
static void create(lv_obj_t *screen, void *arg) {
    (void)arg;
    ink_shell_topbar(screen, model.title);
    body = ink_ui_box(screen, 0, INK_STATUS_BAR + INK_PRIMARY + INK_GAP_XS, INK_CANVAS_W,
        INK_CANVAS_H - INK_STATUS_BAR - INK_PRIMARY - INK_GAP_XS, 0, INK_SURFACE_WHITE, false);
    list = ink_ui_list_create(body, &model, &position, 3, columns, INK_CONTROL, "这里还没有内容", bind_item, NULL);
    CHECK(list != NULL, "create the bounded view");
    if (list) ink_ui_list_refresh(list, total);
}
static void destroy(void) { list = NULL; body = NULL; }
static int page_snapshot(char *out, size_t cap) { return ink_ui_list_snapshot(list, out, cap); }
static bool page_key(ink_key_t k, bool lng) { return ink_ui_list_key(list, k, lng); }
static const ink_activity_t PAGE = {.name = "test.list", .create = create, .destroy = destroy, .snapshot = page_snapshot, .on_key = page_key};
static void detail_create(lv_obj_t *screen, void *arg) { (void)arg; ink_shell_topbar(screen, "合成详情"); }
static int detail_snapshot(char *out, size_t cap) { return snprintf(out, cap, "合成详情：%s", opened); }
static const ink_activity_t DETAIL = {.name = "test.listdetail", .create = detail_create, .snapshot = detail_snapshot};
static void open_page(void) { ink_ui_push(&PAGE, NULL); }
static void save_snapshot(void *arg) {
    (void)arg;
    static unsigned saved;
    char path[600];
    int n = snprintf(path, sizeof path, "%s/list-snapshots.ndjson", g_sim.out_dir);
    if (n < 0 || (size_t)n >= sizeof path) abort();
    FILE *f = fopen(path, saved++ ? "ab" : "wb");
    if (!f || fputs(snapshot, f) == EOF || fputc('\n', f) == EOF || fclose(f)) abort();
}
static void read_snapshot(void) {
    CHECK(ink_ui_snapshot(snapshot, sizeof snapshot, NULL) > 0, "a complete list snapshot is available");
    CHECK(ink_ui_context_snapshot(g_snap, sizeof g_snap, NULL) > 0, "the AI context fits its production buffer");
    vtk_host_call(save_snapshot, NULL);
}
static bool has_label(lv_obj_t *o, const char *text) {
    if (lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN)) return false;
    if (lv_obj_check_type(o, &lv_label_class) && !strcmp(lv_label_get_text(o), text)) return true;
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) if (has_label(lv_obj_get_child(o, i), text)) return true;
    return false;
}
static unsigned item_count(void) {
    unsigned n = 0;
    for (uint32_t i = 0; i < lv_obj_get_child_count(body); i++) {
        lv_obj_t *o = lv_obj_get_child(body, i);
        if (lv_obj_get_child_count(o) == 1 && lv_obj_check_type(lv_obj_get_child(o, 0), &lv_label_class)) n++;
    }
    return n;
}
static unsigned object_count(lv_obj_t *o) {
    unsigned n = 1;
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) n += object_count(lv_obj_get_child(o, i));
    return n;
}
static void first_page(void) {
    read_snapshot();
    CHECK(strstr(snapshot, "\"page\":1,\"pages\":334,\"total\":1000,\"offset\":0,\"visible\":3,\"truncated\":true"), "1000 items have explicit pagination and truncation");
    CHECK(strstr(snapshot, "仅含当前页，其他项请翻页读取") && !strstr(snapshot, "example:item-4\""), "the snapshot tells AI how to read omitted items");
    CHECK(creates == 3 && item_count() == 3 && binds == 3, "1000 items create exactly 3 item roots and 3 labels");
    ink_probe_value("test.list.item_objects", item_count());
    ink_probe_value("test.list.item_descendants", item_count() * 2);
    ink_probe_value("test.list.total_objects", object_count(body));
    memcpy(first_slots, slots, sizeof slots);
    old_target = (ink_ui_action_target_t){ink_ui_content_version(), model.object};
}
static void second_page(void) {
    read_snapshot();
    CHECK(position.page == 1 && nav_source == INK_SRC_KEY && next_calls == 1, "a physical next key uses the registered next capability");
    CHECK(!memcmp(first_slots, slots, sizeof slots) && creates == 3 && item_count() == 3, "paging rebinds the same LVGL objects");
    ink_receipt_t rc;
    CHECK(ink_ui_list_action(list, "next", &old_target, INK_SRC_AI, &rc) == INK_RC_STALE && next_calls == 1, "an old snapshot cannot turn the new page");
    lv_obj_update_layout(slots[1]); lv_area_t a; lv_obj_get_coords(slots[1], &a);
    g_btn_x[0] = (a.x1 + a.x2) / 2; g_btn_y[0] = (a.y1 + a.y2) / 2;
}
static void restored(void) {
    char expected[64]; snprintf(expected, sizeof expected, "file:%u", (unsigned)file_id);
    read_snapshot();
    CHECK(position.page == 1 && !strcmp(position.selected, expected) && !strcmp(opened, expected), "return restores the original page and stable selected file id");
    CHECK(strstr(snapshot, expected) && strstr(snapshot, file_label), "renaming the real file updates its label without changing its identity");
    CHECK(lv_obj_get_style_outline_width(slots[1], 0) == INK_STROKE, "the restored selection is visible");
}
static void ai_open(void) {
    ink_receipt_t rc;
    ink_ui_action_target_t target = {ink_ui_content_version(), {ids[1], 7}};
    CHECK(ink_ui_list_action(list, ids[1], &target, INK_SRC_AI, &rc) == INK_RC_DONE, "AI opens the same bound file action");
}
static void pager_position(void) {
    for (uint32_t i = 0; i < lv_obj_get_child_count(body); i++) {
        lv_obj_t *o = lv_obj_get_child(body, i);
        if (lv_obj_get_child_count(o) != 3) continue;
        lv_area_t a; lv_obj_update_layout(o); lv_obj_get_coords(lv_obj_get_child(o, 2), &a);
        g_btn_x[0] = (a.x1 + a.x2) / 2; g_btn_y[0] = (a.y1 + a.y2) / 2;
    }
}
static void boundaries(void) {
    CHECK(position.page == 0 && nav_source == INK_SRC_AI && prev_calls >= 1, "AI previous uses the same page capability as input");
    CHECK(!ink_ui_list_turn(list, -1), "the first page cannot underflow");
    for (int i = 0; i < 333; i++) CHECK(ink_ui_list_turn(list, 1), "turn to page %d", i + 2);
    read_snapshot();
    CHECK(item_count() == 1 && has_label(body, "334 / 334") && strstr(snapshot, "\"offset\":999,\"visible\":1,\"truncated\":true"), "the last page contains just its last item");
    CHECK(!strstr(snapshot, "test.list_next") && !ink_ui_list_turn(list, 1), "last-page next is absent and cannot overflow");
    total = 0; ink_ui_list_refresh(list, total); read_snapshot();
    CHECK(item_count() == 0 && position.page == 0 && has_label(body, "这里还没有内容") && has_label(body, "1 / 1"), "empty pages have no item objects and a visible empty state");
    CHECK(strstr(snapshot, "\"fields\":[],\"actions\":[]") && strstr(snapshot, "\"total\":0,\"offset\":0,\"visible\":0,\"truncated\":false"), "empty snapshot has neither fabricated items nor page actions");
    total = 2; ink_ui_list_refresh(list, total); read_snapshot();
    CHECK(item_count() == 2 && has_label(body, "1 / 1") && strstr(snapshot, "\"total\":2,\"offset\":0,\"visible\":2,\"truncated\":false") &&
          !strstr(snapshot, "test.list_prev") && !strstr(snapshot, "test.list_next"), "a single page shows both items and disables both endpoints");
    char small[32] = "filled";
    CHECK(ink_ui_list_snapshot(list, small, sizeof small) < 0 && !small[0], "small output is an explicit failure without partial JSON");
    int size = ink_ui_list_snapshot(list, snapshot, sizeof snapshot);
    CHECK(size > 0 && ink_ui_list_snapshot(list, snapshot, (size_t)size) < 0 && !snapshot[0], "pagination overflow also clears the whole output");
}
static void grid(void) {
    lv_obj_update_layout(body);
    CHECK(item_count() == 3 && lv_obj_get_x(slots[1]) > lv_obj_get_x(slots[0]) &&
          lv_obj_get_y(slots[1]) == lv_obj_get_y(slots[0]) && lv_obj_get_y(slots[2]) > lv_obj_get_y(slots[0]), "the same view binds a two-column grid");
    CHECK(ink_ui_list_turn(list, 1) && creates > 0 && item_count() == 3, "grid paging keeps one visible page");
}
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void quiet(void) { CHECK(WAIT_FOR(settled(), 10000), "the display settles"); }
static void run(void *arg) {
    (void)arg; CHECK(WAIT_FOR(ink_member_space_open(), 10000), "the real member space opens"); quiet();
    int removed = ink_fs_remove("/me/paging/改名后.txt");
    CHECK(removed == 0 || removed == INK_FS_ERR_NOT_FOUND, "reset only the previous synthetic rename fixture");
    CHECK(ink_fs_write_atomic("/me/paging/合成文件.txt", "SYNTHETIC", 9) == 0, "create the synthetic file");
    CHECK(AI("file.index", "{\"path\":\"/me/paging\",\"type\":\"text\",\"limit\":1}") && g_rc.status == INK_RC_DONE, "obtain a stable id from file-index-1");
    ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL); int64_t id = 0;
    CHECK(j && ink_json_parse(j, g_result, strlen(g_result)) == 0 &&
        ink_json_int(j, ink_json_find(j, ink_json_find(j, 0, "entries") + 1, "id"), &id) && id > 0, "read the real stable file id");
    osal_free(j); file_id = (uint32_t)id;
    on_ui(open_page); quiet(); on_ui(first_page);
    key(INK_KEY_NEXT); quiet(); on_ui(second_page);
    tap(g_btn_x[0], g_btn_y[0]); quiet();
    CHECK(!strcmp(ink_ui_page_name(), DETAIL.name) && open_source == INK_SRC_TOUCH, "a physical touch opens the original item through its capability");
    CHECK(ink_fs_move("/me/paging/合成文件.txt", "/me/paging/改名后.txt") == 0, "rename the file while its detail is open"); file_label = "改名后.txt";
    char lookup[48]; snprintf(lookup, sizeof lookup, "{\"id\":%u}", (unsigned)file_id);
    CHECK(AI("file.lookup", lookup) && g_rc.status == INK_RC_DONE && strstr(g_result, "改名后.txt"), "the original service id resolves after rename");
    CHECK(BY_HAND("shell.back", NULL) && g_rc.status == INK_RC_DONE, "return through the existing back-button capability"); quiet(); on_ui(restored);
    on_ui(ai_open); quiet(); CHECK(open_source == INK_SRC_AI, "AI used the same open capability");
    CHECK(BY_HAND("shell.back", NULL) && g_rc.status == INK_RC_DONE, "return from the AI-opened item through the same capability"); quiet(); on_ui(restored);
    CHECK(AI("test.list_prev", NULL) && g_rc.status == INK_RC_DONE, "AI turns through the registered previous capability"); quiet();
    CHECK(AI("test.list_next", NULL) && g_rc.status == INK_RC_DONE && nav_source == INK_SRC_AI && next_calls == 2,
          "AI next invokes the exact capability previously invoked by the physical next key"); quiet();
    CHECK(AI("test.list_prev", NULL) && g_rc.status == INK_RC_DONE, "return to the first page for endpoint checks"); quiet();
    on_ui(pager_position); tap(g_btn_x[0], g_btn_y[0]); quiet();
    CHECK(position.page == 1 && next_calls == 3 && nav_source == INK_SRC_TOUCH, "the pagebar button invokes the same next capability as the key and AI");
    CHECK(AI("test.list_prev", NULL) && g_rc.status == INK_RC_DONE, "return after testing the pagebar button"); quiet();
    on_ui(boundaries); quiet(); on_ui(ink_ui_pop); quiet();
    total = 1000; columns = 2; memset(&position, 0, sizeof position);
    on_ui(open_page); quiet(); on_ui(grid); on_ui(ink_ui_pop); quiet();
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    CHECK(ink_cap_register(CAPS, sizeof CAPS / sizeof *CAPS) == 0, "register the synthetic navigation capabilities");
    osal_task_cfg_t cfg = {.name = "ui-list-test", .fn = run, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start the test driver");
}
