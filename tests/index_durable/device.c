/* File latency during repeated UI redraws, on synthetic card data. */
#include "../services_finish/product.h"
#include "ink/ui_shell.h"

unsigned ink_test_index_path_id(const char *path);
static bool ready(void) {
    char state[32] = "";
    return ink_state_get_str(ink_state_find("file.index_state"), state, sizeof state) && !strcmp(state, "ready");
}
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static unsigned page;
static void create(lv_obj_t *screen, void *arg) {
    (void)arg;
    lv_obj_set_style_bg_color(screen, page++ % 2 ? lv_color_black() : lv_color_white(), 0);
    lv_obj_t *label = lv_label_create(screen);
    lv_label_set_text(label, "合成翻页测试");
    lv_obj_center(label);
}
static const ink_activity_t ACTIVITY = {.name = "SYNTHETIC redraw", .create = create};
static void open_page(void) { ink_ui_push(&ACTIVITY, NULL); }
static void redraw(void) { ink_ui_redraw(); }
static void active(void) {
    on_ui(redraw);
    CHECK(WAIT_FOR(ink_display_busy(), 2000), "file operation overlaps a real display refresh");
}
static void run(void *arg) {
    (void)arg;
    CHECK(WAIT_FOR(ink_member_space_open(), 10000), "member space opens");
    test_start_home();
    CHECK(WAIT_FOR(ready() && !ink_storage_pending() && settled(), 30000), "completed index and UI settle");
    const char *mode = getenv("INK_DURABLE_MODE");
    if (mode) {
        unsigned id = ink_test_index_path_id("/content/SYNTHETIC_OLD.txt");
        CHECK(id != 0, "legacy path retains a stable id");
        ink_probe_value("test.durable.old_id", id);
        if (!strcmp(mode, "upgrade")) {
            ink_fs_entry_t e;
            unsigned expected = (unsigned)strtoul(getenv("INK_DURABLE_OLD_ID"), NULL, 10);
            CHECK(id == expected && !ink_fs_lookup(id, &e), "v1 migration retains the original id");
            ink_db_t *db = ink_db_open(ink_db_find_def("file.index"), NULL);
            ink_rec_t rec;
            CHECK(db && !ink_db_get(db, id, &rec, NULL, 0) && rec.schema == 2 && rec.len == 216,
                "v1 record becomes v2 with size and modification time");
            ink_db_close(db);
            CHECK(hal_fs_size("/content/.file-index-dirty") < 0, "completed migration has no dirty marker");
        }
        ink_probe_count("test.finished"); vtk_stop(); return;
    }
    on_ui(open_page);
    for (int i = 0; i < 5; i++) {
        char path[96], renamed[96];
        snprintf(path, sizeof path, "/family/SYNTHETIC_NEW_%d.txt", i);
        snprintf(renamed, sizeof renamed, "/family/SYNTHETIC_RENAMED_%d.txt", i);
        CHECK(WAIT_FOR(settled(), 12000), "previous frame settles");
        active();
        uint64_t start = osal_now_us();
        CHECK(!ink_fs_write_atomic(path, "XXXXX", 5), "create while display refreshes");
        ink_probe_value("test.durable.create_ms", (osal_now_us() - start) / 1000.0);
        CHECK(WAIT_FOR(ready() && !ink_storage_pending() && settled(), 30000), "new file gets indexed before rename");
        char physical[96];
        snprintf(physical, sizeof physical, "/content/SYNTHETIC_NEW_%d.txt", i);
        unsigned id = ink_test_index_path_id(physical);
        CHECK(id != 0, "renamed file has an existing stable id");
        active();
        start = osal_now_us();
        CHECK(!ink_fs_move(path, renamed), "rename while display refreshes");
        ink_probe_value("test.durable.rename_ms", (osal_now_us() - start) / 1000.0);
        CHECK(WAIT_FOR(settled(), 12000), "renamed frame settles");
        active();
        start = osal_now_us();
        CHECK(!ink_fs_remove(renamed), "delete while display refreshes");
        ink_probe_value("test.durable.delete_ms", (osal_now_us() - start) / 1000.0);
        ink_fs_entry_t e;
        CHECK(ink_fs_lookup(id, &e) == INK_FS_ERR_NOT_FOUND, "deleted id no longer resolves");
        CHECK(WAIT_FOR(ready() && !ink_storage_pending() && settled(), 30000), "next sample starts from a completed index");
    }
    ink_probe_count("test.finished");
    vtk_stop();
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name = "durable-test", .fn = run, .stack_bytes = 16384,
        .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start durability latency test");
}
