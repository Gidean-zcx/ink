/* F9: a synthetic caller over the real file service and UI runtime. */
#include "ink/sys/file_picker.h"
#include "ink/ui_shell.h"
#define ink_main product_main
#include "../system/boot/main.c"
#undef ink_main
#include "integration/whole_product.h"
#include <stdlib.h>

static unsigned callbacks, creates;
static uint32_t selected, expected;
static unsigned caller_page = 9;
static ink_fs_type_t wanted_type = INK_FS_IMAGE;
static char before[3072], expected_path[INK_FS_PATH_MAX];
static lv_obj_t *caller_screen;
static void caller_create(lv_obj_t *screen, void *arg) {
    (void)arg; creates++; caller_screen = screen;
    ink_shell_topbar(screen, "合成调用页");
    ink_ui_label(screen, "合成位置：第十页，第二项", INK_FONT_BODY, INK_WEIGHT_REGULAR,
                 INK_MARGIN, INK_STATUS_BAR + INK_PRIMARY, INK_CANVAS_W - 2 * INK_MARGIN);
}
static int caller_snapshot(char *out, size_t cap) { return snprintf(out, cap, "合成调用页，page=%u，selected=example:item-2", caller_page); }
static const ink_activity_t CALLER = {.name = "test.picker-caller", .create = caller_create, .snapshot = caller_snapshot};
static void push_caller(void) { ink_ui_push(&CALLER, NULL); }
static void chosen(void *ctx, uint32_t id) {
    CHECK(ctx == &caller_page && ink_ui_is_current(), "the original caller receives its result on UI");
    selected = id; callbacks++;
}
static void open_picker(void) { CHECK(ink_file_picker_open(wanted_type, chosen, &caller_page) == 0, "open the system picker"); }
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void quiet(void) { CHECK(WAIT_FOR(settled(), 10000), "the display settles"); }
static bool ready;
static void poll_ready(void) {
    ready = ink_ui_snapshot(g_snap, sizeof g_snap, NULL) > 0 && strstr(g_snap, "\"status\":\"\"");
}
static void wait_ready(void) {
    for (unsigned i = 0; i < 1000; i++) { on_ui(poll_ready); if (ready) break; delay_ms(20); }
    CHECK(ready, "the BG index reaches a readable UI model"); quiet();
}
static void remember_caller(void) {
    CHECK(ink_ui_snapshot(before, sizeof before, NULL) > 0, "capture caller position before opening");
}
static void restored(void) {
    CHECK(!strcmp(ink_ui_top_name(), CALLER.name) && lv_screen_active() == caller_screen && caller_page == 9,
          "closing the overlay restores the same screen and position");
    CHECK(ink_ui_snapshot(g_snap, sizeof g_snap, NULL) > 0 && !strcmp(before, g_snap), "caller snapshot is unchanged");
}
static void page_one(void) {
    CHECK(ink_ui_snapshot(g_snap, sizeof g_snap, NULL) > 0 && strstr(g_snap, "\"page\":\"FilePicker\""), "F1 supports the real overlay snapshot");
    CHECK(strstr(g_snap, "\"page\":1,\"pages\":2,\"total\":7") && strstr(g_snap, "\"visible\":4"), "only seven image entries are paged by F4");
    CHECK(!strstr(g_snap, "SYNTH_AUDIO") && !strstr(g_snap, "SYNTH_TEXT") && !strstr(g_snap, ".png") && !strstr(g_snap, "/family"), "no wrong type, extension or raw path is exposed");
    CHECK(strstr(g_snap, "file_picker.select") && strstr(g_snap, "file_picker.cancel") && strstr(g_snap, "file_picker.next"), "AI gets the exact registered selection and navigation actions");
    CHECK(ink_ui_context_snapshot(g_snap, sizeof g_snap, NULL) > 0, "the complete AI context fits the production buffer");
}
static void page_two(void) {
    CHECK(ink_ui_snapshot(g_snap, sizeof g_snap, NULL) > 0 && strstr(g_snap, "\"page\":2,\"pages\":2") && strstr(g_snap, "\"visible\":3"), "physical next key turns the real F4 page");
    CHECK(!strstr(g_snap, "file_picker.next"), "last page has no next action");
    /* The private fixture is the final image, after the six family fixtures. */
    char id[40]; snprintf(id, sizeof id, "file:%u", (unsigned)expected);
    CHECK(strstr(g_snap, id) && strstr(g_snap, "SYNTH_PRIVATE"), "adult can see the private image's stable id");
}
static const char *label;
static bool found;
static void find_label(lv_obj_t *root) {
    if (lv_obj_has_flag(root, LV_OBJ_FLAG_HIDDEN)) return;
    if (lv_obj_check_type(root, &lv_label_class) && !strcmp(lv_label_get_text(root), label)) {
        lv_area_t a; lv_obj_update_layout(root); lv_obj_get_coords(root, &a);
        g_btn_x[0] = (a.x1 + a.x2) / 2; g_btn_y[0] = (a.y1 + a.y2) / 2; found = true;
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); i++) find_label(lv_obj_get_child(root, i));
}
static void locate(void) { found = false; find_label(lv_layer_sys()); CHECK(found, "the target is visible: %s", label); }
static void capture(void *arg) {
    CHECK(settled(), "capture a settled picker/caller frame");
    if (!strcmp(arg, "scene")) { ink_probe_count("test.snapshot.settled"); ink_probe_count("test.snapshot.FilePicker"); }
    sim_display_snapshot(arg);
}
static void snap(const char *name) { quiet(); vtk_event_at(vtk_now() + SIM_MS(1), capture, (void *)name); delay_ms(5); }
static void child_page(void) {
    CHECK(ink_ui_snapshot(g_snap, sizeof g_snap, NULL) > 0 && !strstr(g_snap, "SYNTH_PRIVATE") && !strstr(g_snap, "SYNTH_ADULT_SECRET"), "child cannot see the adult's private image on any picker page");
}
static void run(void *arg) {
    (void)arg;
    CHECK(WAIT_FOR(ink_member_space_open(), 10000), "the real owner space opens"); quiet();
    int removed = ink_fs_remove("/members/kid/SYNTH_CHILD.png");
    CHECK(removed == 0 || removed == INK_FS_ERR_NOT_FOUND, "reset only the previous synthetic child fixture");
    for (int i = 0; i < 6; i++) {
        char path[INK_FS_PATH_MAX]; snprintf(path, sizeof path, "/family/picker/SYNTH_IMAGE_%d.png", i);
        CHECK(ink_fs_write_atomic(path, "SYNTHETIC_IMAGE", 15) == 0, "create synthetic image %d", i);
    }
    CHECK(ink_fs_write_atomic("/family/picker/SYNTH_AUDIO.mp3", "SYNTHETIC", 9) == 0, "create wrong-type audio");
    CHECK(ink_fs_write_atomic("/family/picker/SYNTH_TEXT.txt", "SYNTHETIC", 9) == 0, "create wrong-type text");
    CHECK(ink_fs_write_atomic("/me/SYNTH_PRIVATE.png", "SYNTH_ADULT_SECRET", 18) == 0, "create the adult private fixture");
    uint32_t cursor = 0; ink_fs_entry_t e;
    CHECK(ink_fs_index("/me", INK_FS_IMAGE, &cursor, &e) == 1, "prime the private file index as an adult");
    expected = e.id; snprintf(expected_path, sizeof expected_path, "%s", e.path);
    on_ui(push_caller); quiet(); on_ui(remember_caller); snap("caller-before");
    unsigned original_creates = creates;
    on_ui(open_picker); wait_ready(); on_ui(page_one); snap("scene");
    key(INK_KEY_NEXT); quiet(); on_ui(page_two);
    CHECK(AI("file_picker.prev", NULL) && g_rc.status == INK_RC_DONE, "AI uses the registered previous capability"); quiet();
    CHECK(AI("file_picker.next", NULL) && g_rc.status == INK_RC_DONE, "AI uses the same next capability as the key"); quiet();
    CHECK(BY_HAND("file_picker.cancel", NULL) && g_rc.status == INK_RC_DONE, "cancel returns through its registered capability"); quiet(); on_ui(restored);
    CHECK(callbacks == 1 && !selected && creates == original_creates, "cancel returns zero exactly once without rebuilding the caller"); snap("caller-after");
    on_ui(open_picker); wait_ready(); key(INK_KEY_NEXT); quiet();
    label = "SYNTH_PRIVATE"; on_ui(locate); if (found) tap(g_btn_x[0], g_btn_y[0]);
    CHECK(WAIT_FOR(callbacks == 2, 10000) && selected == expected, "touch returns the stable file-index-1 id"); quiet(); on_ui(restored);
    CHECK(ink_fs_lookup(selected, &e) == 0 && e.id == expected && !strcmp(e.path, expected_path), "the caller resolves the returned id on BG");
    on_ui(open_picker); wait_ready(); key(INK_KEY_NEXT); quiet();
    char args[64]; snprintf(args, sizeof args, "{\"id\":%u}", (unsigned)expected);
    CHECK(AI("file_picker.select", args) && g_rc.status == INK_RC_DONE && strstr(g_result, "\"id\":"), "AI gets the same selected id in its final receipt");
    CHECK(callbacks == 3 && selected == expected, "AI and touch share the same selection callback");
    CHECK(ink_member_switch("kid") == 0, "switch to the real child");
    on_ui(ink_ui_home); quiet(); on_ui(push_caller); quiet(); on_ui(remember_caller);
    CHECK(ink_fs_write_atomic("/me/SYNTH_CHILD.png", "SYNTHETIC", 9) == 0, "create the child's own image");
    on_ui(open_picker); wait_ready(); on_ui(child_page);
    CHECK(AI("file_picker.select", args) && g_rc.status == INK_RC_INVALID && callbacks == 3, "a forged adult id is not selectable by the child");
    key(INK_KEY_NEXT); quiet(); on_ui(child_page);
    label = "SYNTH_CHILD"; on_ui(locate); if (found) tap(g_btn_x[0], g_btn_y[0]);
    CHECK(WAIT_FOR(callbacks == 4, 10000) && selected && selected != expected, "child can select their own image");
    CHECK(ink_fs_lookup(selected, &e) == 0 && !strcmp(e.path, "/me/SYNTH_CHILD.png"), "the child receives their own service object");
    CHECK(ink_fs_lookup(expected, &e) == INK_FS_ERR_DENIED, "file lookup also refuses the adult's private id");
    quiet(); on_ui(restored);
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name = "file-picker-test", .fn = run, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start the synthetic caller test");
}
